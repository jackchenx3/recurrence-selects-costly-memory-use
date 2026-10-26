#pragma once
#include "json.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <exception>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/resource.h>
namespace brc {
constexpr const char* CONTRACT="ec34610b22fdbd1e0a31523995e9ffaa201b06840fb2da4b17f4fc44386e9913";
constexpr const char* CORE="e60b367dbab81dc1388f06f9d7be6da90b927e8302b4d7001be544976abe8889";
constexpr const char* INVENTORY="4cc9b10958a04c458aeba5ab3788dd54153b1ce18fc2cb83fe3847e012599bd6";
constexpr const char* KEYHASH="636c558fdd866cd3d4f60eeee30915d7dce1d2001fbcbd1c5773acb56bcbe522";
struct Fail:Error{int exit;
std::string code;
Fail(int e,std::string c,std::string msg):Error(msg),exit(e),code(std::move(c)){} };
inline void ck(bool ok,int e,const std::string& code,const std::string& loc){if(!ok)throw Fail(e,code,loc);
}
struct Tier {std::string name;
unsigned code,n;
unsigned chunks()const{return (n+99)/100;
}unsigned count(unsigned c)const{return std::min(100U,n-100*c);
}};
inline Tier tier(const std::string& s){if(s=="fixture-r1")return {s,0,16};
if(s=="benchmark-r1")return {s,1,64};
if(s=="production-r1")return {s,2,20000};
throw Fail(2,"CLI","tier");
}
inline void lexical(const std::string& s){ck(s.size()>1&&s[0]=='/'&&s.back()!='/',2,"CLI","absolute path");
for(auto& c:split(s.substr(1),'/'))ck(!c.empty()&&c!="."&&c!=".."&&std::all_of(c.begin(),c.end(),[](unsigned char v){return v>=32&&v<=126;}),2,"CLI","path component");
}
inline void pathcheck(const fs::path& p,bool absent=false){lexical(p.string());
fs::path q="/";
auto parts=split(p.string().substr(1),'/');
for(size_t i=0;i<parts.size();++i){q/=parts[i];
struct stat st{};
if(::lstat(q.c_str(),&st)){ck(absent&&i+1==parts.size()&&errno==ENOENT,2,"CLI","path unavailable");
return;
}ck(!S_ISLNK(st.st_mode),2,"CLI","symlink component");
if(i+1<parts.size())ck(S_ISDIR(st.st_mode),2,"CLI","path parent");
else ck(!absent,2,"CLI","output exists");
}}
inline std::map<std::string,std::string> options(int argc,char** argv,std::initializer_list<const char*> allowed){std::set<std::string> a;
for(auto x:allowed)a.insert(x);
std::map<std::string,std::string> out;
ck(argc==1+2*int(a.size()),2,"CLI","argument count");
for(int i=1;i<argc;i+=2)ck(a.count(argv[i])&&out.emplace(argv[i],argv[i+1]).second,2,"CLI","flag");
return out;
}
inline unsigned threads(const std::string& s){try{auto n=decimal(s,32);
ck(n>=1,2,"CLI","threads");
return unsigned(n);
}catch(const Fail&){throw;
}catch(...){throw Fail(2,"CLI","threads");
}}
inline Bytes inputfile(const fs::path& p,uint64_t cap=40000000){try{return readfile(p,cap);
}catch(...){throw Fail(2,"INPUT","unreadable input");
}}
inline void check_regular(const fs::path& p,bool readonly=false){struct stat st{};
ck(::lstat(p.c_str(),&st)==0&&S_ISREG(st.st_mode)&&(!readonly||(st.st_mode&0222)==0),2,"INPUT","input type/mode");
}
inline void science_inputs(const fs::path& p){
 try{pathcheck(p);
ck(fs::is_directory(p),2,"INPUT","input directory");
std::set<std::string> want={"PROTOCOL.txt","ENGINEERING_CONTRACT.txt","KEYS.tsv","KAT.txt","PUBLIC_INPUT_INDEX.json"},got;
 for(auto& e:fs::directory_iterator(p)){got.insert(e.path().filename().string());
check_regular(e.path(),true);
}ck(got==want,2,"INPUT","SCIENCE_VIEW closure");
auto b=inputfile(p/"PUBLIC_INPUT_INDEX.json");
ck(hex(sha(b))==CORE,2,"INPUT","core digest");
auto j=parse(b);
ck(j.o.size()==4,2,"INPUT","core schema");
for(auto& x:want)if(x!="PUBLIC_INPUT_INDEX.json"){auto& row=j.at(x);
keys(row,{"bytes","sha256"});
auto b2=inputfile(p/x);
ck(row.at("bytes").u()==b2.size()&&row.at("sha256").text()==hex(sha(b2)),2,"INPUT","core bytes");
}
 }catch(const Fail& f){if(f.code=="CLI")throw Fail(2,"INPUT",f.what());
throw;
}catch(...){throw Fail(2,"INPUT","core canonical schema");
}
}
// Linux atomic rename with no replacement, including destination races.
// Failed writes retain explicitly uncommitted partial files.
inline void publish(const fs::path& p,const Bytes& b){auto tmp=p.parent_path()/(".partial."+p.filename().string());
bool committed=false;
int fd=::open(tmp.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW|O_CLOEXEC,0600);
ck(fd>=0,5,"IO","create partial");
try{size_t off=0;
while(off<b.size()){ssize_t n=::write(fd,b.data()+off,b.size()-off);
if(n<0&&errno==EINTR)continue;
ck(n>0,5,"IO","write");
off+=n;
}ck(::fchmod(fd,0400)==0&&::fsync(fd)==0,5,"IO","seal/fsync");
int rc=::close(fd);
fd=-1;
ck(rc==0,5,"IO","close");
ck(::syscall(SYS_renameat2,AT_FDCWD,tmp.c_str(),AT_FDCWD,p.c_str(),1)==0,5,"IO","exclusive atomic rename");
committed=true;
int d=::open(p.parent_path().c_str(),O_DIRECTORY|O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
ck(d>=0,5,"IO","parent");
int z=::fsync(d);
::close(d);
ck(z==0,5,"IO","directory fsync");
}catch(...){if(fd>=0)::close(fd);
if(committed)::syscall(SYS_renameat2,AT_FDCWD,p.c_str(),AT_FDCWD,tmp.c_str(),1);
throw;
}}
inline void publish(const fs::path& p,const std::string& s){publish(p,bytes(s));
}
inline Digest filehash(const fs::path& p){int fd=::open(p.c_str(),O_RDONLY|O_CLOEXEC);
ck(fd>=0,5,"IO","hash file");
SHA256 h;
std::array<uint8_t,65536> buf;
while(true){ssize_t n=::read(fd,buf.data(),buf.size());
if(n<0&&errno==EINTR)continue;
if(n<0){::close(fd);
throw Fail(5,"IO","hash read");
}if(!n)break;
h.update(buf.data(),n);
}ck(::close(fd)==0,5,"IO","hash close");
return h.finish();
}
template<class F> void parallel(unsigned n,unsigned nt,F f,bool reverse=false){std::atomic<unsigned> next{0};
std::vector<std::exception_ptr> errors(n);
std::vector<std::thread> workers;
try{for(unsigned t=0;t<std::min(n,nt);++t)workers.emplace_back([&]{while(true){unsigned k=next.fetch_add(1);if(k>=n)break;unsigned i=reverse?n-1-k:k;try{f(i);}catch(...){errors[i]=std::current_exception();}}});
}catch(...){for(auto& w:workers)w.join();
throw;
}for(auto& w:workers)w.join();
for(auto& e:errors)if(e)std::rethrow_exception(e);
}
inline int failure(){try{throw;
}catch(const Fail& e){std::cerr<<"BRS3_ERROR "<<e.code<<"\n"<<e.what()<<"\n";
return e.exit;
}catch(const std::bad_alloc&){std::cerr<<"BRS3_ERROR RESOURCE\n";
return 5;
}catch(const std::exception& e){std::cerr<<"BRS3_ERROR INTERNAL\n"<<e.what()<<"\n";
return 5;
}catch(...){std::cerr<<"BRS3_ERROR INTERNAL\n";
return 5;
}}
} // namespace brc
