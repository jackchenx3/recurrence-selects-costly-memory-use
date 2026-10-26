#pragma once
#include "json.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <cerrno>
namespace brs {
  inline bool lexical(const std::string&s){
    if(s=="/")return true;
    if(s.size()<2||s[0]!='/'||s.back()=='/')return false;
    for(unsigned char c:s)if(c<32||c>126)return false;
    auto v=split(s.substr(1),'/');
    for(auto&x:v)if(x.empty()||x=="."||x=="..")return false;
    return true;
  }
  inline void pathcheck(const fs::path&p,bool absent=false,const std::string&code="CLI"){
    need(lexical(p.string()),2,code);
    if(p=="/"){
      need(!absent,2,code);
      return;
    }
    fs::path cur="/";
    auto v=split(p.string().substr(1),'/');
    for(size_t i=0;i<v.size();++i){
      cur/=v[i];
      struct stat st{
      }
      ;
      int rc=::lstat(cur.c_str(),&st);
      if(absent&&i+1==v.size()){
        need(rc<0&&errno==ENOENT,2,code);
        return;
      }
      need(rc==0&&!S_ISLNK(st.st_mode),2,code);
      if(i+1<v.size())need(S_ISDIR(st.st_mode),2,code);
    }
  }
  struct FD {
    int n=-1;
    FD()=default;
    explicit FD(int f):n(f){
    }
    FD(const FD&)=delete;
    FD&operator=(const FD&)=delete;
    ~FD(){
      if(n>=0)::close(n);
    }
    void close(){
      int f=n;
      n=-1;
      need(::close(f)==0,5,"IO");
    }
  }
  ;
  inline Bytes readfile(const fs::path&p,u64 cap=1u<<26,bool readonly=false,const std::string&code="INPUT"){
    pathcheck(p,false,code);
    struct stat st{
    }
    ;
    need(::lstat(p.c_str(),&st)==0&&S_ISREG(st.st_mode)&&st.st_size>=0&&u64(st.st_size)<=cap,2,code);
    if(readonly)need((st.st_mode&0222)==0,2,code);
    FD fd(::open(p.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
    need(fd.n>=0,2,code);
    struct stat before{
    }
    ;
    need(::fstat(fd.n,&before)==0&&before.st_ino==st.st_ino&&before.st_dev==st.st_dev&&before.st_size==st.st_size,2,code);
    Bytes b(size_t(st.st_size));
    size_t o=0;
    while(o<b.size()){
      ssize_t n=::read(fd.n,b.data()+o,b.size()-o);
      if(n<0&&errno==EINTR)continue;
      need(n>0,2,code);
      o+=size_t(n);
    }
    u8 x;
    ssize_t n;
    do{
      n=::read(fd.n,&x,1);
    }
    while(n<0&&errno==EINTR);
    struct stat after{
    }
    ;
    need(n==0&&::fstat(fd.n,&after)==0&&after.st_size==before.st_size&&after.st_mtim.tv_sec==before.st_mtim.tv_sec&&after.st_mtim.tv_nsec==before.st_mtim.tv_nsec&&after.st_ctim.tv_sec==before.st_ctim.tv_sec&&after.st_ctim.tv_nsec==before.st_ctim.tv_nsec,2,code);
    fd.close();
    return b;
  }
  inline std::string text(const Bytes&b){
    return std::string(b.begin(),b.end());
  }
  inline void syncdir(const fs::path&p){
    FD f(::open(p.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    need(f.n>=0&&::fsync(f.n)==0,5,"IO");
    f.close();
  }
  inline void publish(const fs::path&tmp,const fs::path&dest){
    need(::syscall(SYS_renameat2,AT_FDCWD,tmp.c_str(),AT_FDCWD,dest.c_str(),1)==0,5,"IO");
    try{
      syncdir(dest.parent_path());
    }
    catch(...){
      // Restore the exclusive .partial name after a late directory IO failure.
      // Cleanup must never replace the original exception. If rename fails,
      // preserve an evidence hardlink where possible before unlinking final.
      auto original=std::current_exception();
      if(::syscall(SYS_renameat2,AT_FDCWD,dest.c_str(),AT_FDCWD,tmp.c_str(),1)!=0){
        auto evidence=dest.parent_path()/(".uncommitted."+dest.filename().string()+
          "."+std::to_string(::getpid()));
        // A second exclusive evidence name handles an occupied partial name.
        if(::syscall(SYS_renameat2,AT_FDCWD,dest.c_str(),AT_FDCWD,evidence.c_str(),1)!=0){
          if(::link(dest.c_str(),tmp.c_str())!=0)
            ::link(dest.c_str(),evidence.c_str());
          ::unlink(dest.c_str());
        }
      }
      try{syncdir(dest.parent_path());}catch(...){}
      std::rethrow_exception(original);
    }
  }
  inline void writeall(int f,const void*p,size_t n){
    auto b=static_cast<const u8*>(p);
    while(n){
      ssize_t w=::write(f,b,n);
      if(w<0&&errno==EINTR)continue;
      need(w>0,5,"IO");
      b+=w;
      n-=size_t(w);
    }
  }
  inline void atomicfile(const fs::path&p,const Bytes&b){
    pathcheck(p,true);
    auto tmp=p.parent_path()/(".partial."+p.filename().string());
    FD f(::open(tmp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));
    need(f.n>=0,5,"IO");
    writeall(f.n,b.data(),b.size());
    need(::fchmod(f.n,0444)==0&&::fsync(f.n)==0,5,"IO");
    f.close();
    publish(tmp,p);
  }
  inline void atomictext(const fs::path&p,const std::string&s){
    atomicfile(p,Bytes(s.begin(),s.end()));
  }
  inline void closure(const fs::path&root,const std::set<std::string>&files,const std::string&code){
    std::set<std::string>dirs,seen;
    for(auto&s:files){
      auto p=fs::path(s).parent_path();
      while(!p.empty()){
        dirs.insert(p.generic_string());
        p=p.parent_path();
      }
    }
    struct stat st{
    }
    ;
    need(::lstat(root.c_str(),&st)==0&&S_ISDIR(st.st_mode),2,code);
    for(auto it=fs::recursive_directory_iterator(root);it!=fs::recursive_directory_iterator();++it){
      auto rel=it->path().lexically_relative(root).generic_string();
      need(::lstat(it->path().c_str(),&st)==0&&!S_ISLNK(st.st_mode),2,code);
      if(S_ISDIR(st.st_mode))need(dirs.count(rel),2,code);
      else{
        need(S_ISREG(st.st_mode)&&files.count(rel),2,code);
        seen.insert(rel);
      }
    }
    need(seen==files,2,code);
  }
  inline std::map<std::string,std::string> drycli(const std::vector<std::string>&a){
    need(!a.empty(),2,"CLI");
    std::set<std::string>flags;
    if(a[0]=="brs3_produce")flags={
      "--inputs","--tier","--output","--threads"
    }
    ;
    else if(a[0]=="brs3_analyze")flags={
      "--inputs","--tier","--data","--results","--threads"
    }
    ;
    else if(a[0]=="brs3_audit")flags={
      "--inputs","--tier","--data","--results","--report","--threads"
    }
    ;
    else if(a[0]=="brs3_fixture")flags={
      "--inputs","--case","--output"
    }
    ;
    else fail(2,"CLI");
    std::map<std::string,std::string>o;
    for(size_t i=1;i<a.size();i+=2){
      need(flags.count(a[i])&&i+1<a.size()&&!o.count(a[i]),2,"CLI");
      o[a[i]]=a[i+1];
    }
    need(o.size()==flags.size(),2,"CLI");
    for(auto&v:o){
      if(v.first=="--tier"){
        try{
          tier(v.second);
        }
        catch(...){
          fail(2,"CLI");
        }
      }
      else if(v.first=="--threads"){
        try{
          need(decimal(v.second,32)>=1,2,"CLI");
        }
        catch(...){
          fail(2,"CLI");
        }
      }
      else need(lexical(v.second),2,"CLI");
    }
    return o;
  }
  struct Inputs {
    fs::path root;
    std::map<std::string,Bytes> bytes;
    std::set<std::string>cases;
    explicit Inputs(fs::path r,bool fixture):root(std::move(r)){
      try{
        std::set<std::string>allow={
          "PROTOCOL.txt","ENGINEERING_CONTRACT.txt","KEYS.tsv","KAT.txt","PUBLIC_INPUT_INDEX.json"
        }
        ;
        auto load=[&](const std::string&n)->const Bytes&{
          auto p=bytes.emplace(n,readfile(root/n,1u<<26,true));
          return p.first->second;
        }
        ;
        need(sha(load("PUBLIC_INPUT_INDEX.json"))==CORE_SHA,2,"INPUT");
        J core=parse(text(bytes.at("PUBLIC_INPUT_INDEX.json")));
        keys(core,{
          "PROTOCOL.txt","ENGINEERING_CONTRACT.txt","KEYS.tsv","KAT.txt"
        }
        );
        auto check=[&](const std::string&n,const J&entry){
          keys(entry,{
            "bytes","sha256"
          }
          );
          auto&b=bytes.count(n)?bytes.at(n):load(n);
          need(entry.at("bytes").num()==b.size()&&ishex(entry.at("sha256").str(),64)&&sha(b)==entry.at("sha256").str(),2,"INPUT");
        }
        ;
        for(auto&v:core.o)check(v.first,v.second);
        if(fixture){
          allow.insert({
            "BUNDLE_INVENTORY.json","FIXTURE_SPEC.txt","LEGACY_KEYS.tsv","INTERVAL_CERTIFICATE.json","FIXIN_BUNDLE/INDEX.json","FIXIN_BUNDLE/literals/SYNTHETIC_RECORD.bin"
          }
          );
          need(sha(load("BUNDLE_INVENTORY.json"))==INVENTORY_SHA,2,"INPUT");
          auto inv=parse(text(bytes.at("BUNDLE_INVENTORY.json")));
          need(inv.type==J::O,2,"INPUT");
          check("FIXIN_BUNDLE/INDEX.json",inv.at("FIXIN_BUNDLE/INDEX.json"));
          auto index=parse(text(bytes.at("FIXIN_BUNDLE/INDEX.json")));
          keys(index,{
            "schema","cases"
          }
          );
          need(index.at("schema").num()==1&&index.at("cases").type==J::A,2,"INPUT");
          for(auto&e:index.at("cases").a){
            std::string n=e.at("inputpath").str();
            need(n.rfind("FIXIN_BUNDLE/in/",0)==0&&n.find("..") == n.npos&&n.find('\\')==n.npos&&cases.insert(n).second,2,"INPUT");
            check(n,inv.at(n));
            allow.insert(n);
          }
          need(cases.size()==96,2,"INPUT");
          for(auto&n:allow)if(n!="BUNDLE_INVENTORY.json")check(n,inv.at(n));
        }
        closure(root,allow,"INPUT");
      }
      catch(const Error&){
        fail(2,"INPUT");
      }
      catch(const fs::filesystem_error&){
        fail(2,"INPUT");
      }
    }
    const Bytes&at(const std::string&n)const{
      return bytes.at(n);
    }
    std::string txt(const std::string&n)const{
      return text(at(n));
    }
  }
  ;
  // No answer files, metadata-based answer selection, environment overrides or cwd reads.
  inline std::string binary_sha(){
    char b[4096];
    ssize_t n=::readlink("/proc/self/exe",b,sizeof(b));
    need(n>0&&size_t(n)<sizeof(b),5,"IO");
    return sha(readfile(std::string(b,size_t(n)),1u<<28,false,"IO"));
  }
  template<class F>void parallel(u32 n,u32 threads,F work){
    std::atomic<u32>next{
      0
    }
    ;
    std::vector<std::exception_ptr>errors(n);
    std::vector<std::thread>pool;
    auto run=[&]{
      for(;;){
        u32 k=next.fetch_add(1);
        if(k>=n)return;
        try{
          work(k);
        }
        catch(...){
          errors[k]=std::current_exception();
        }
      }
    }
    ;
    try{
      for(u32 k=0;k<std::min(n,threads);++k)pool.emplace_back(run);
    }
    catch(...){
      for(auto&t:pool)t.join();
      throw;
    }
    for(auto&t:pool)t.join();
    for(auto&e:errors)if(e)std::rethrow_exception(e);
  }
}
