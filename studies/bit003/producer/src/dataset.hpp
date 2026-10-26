#pragma once
#include "science.hpp"
namespace brs {
  inline void field(bool ok,const std::string&name,int exit=2,const std::string&code="FORMAT"){
    need(ok,exit,code,name);
  }
  inline std::array<u32,4> sums_from_record(const Bytes&r){
    std::array<u32,4>s{
    }
    ;
    for(u32 u=257;u<=512;++u){
      u32 t=get(r.data()+32+4*(u-1),4);
      State c=decode(r.data()+STATE_BASE+264*u),a=decode(r.data()+STATE_BASE+264*(u+256));
      s[0]+=pop(c.policy);
      s[1]+=pop(a.policy);
      s[2]+=accuracy(c,t);
      s[3]+=accuracy(a,t);
    }
    return s;
  }
  inline Tier record_metadata(const Bytes&r,int expected_b=-1,const Tier*expected_t=nullptr){
    field(r.size()==RECORD_BYTES,"length");
    field(std::memcmp(r.data(),"BRS3R001",8)==0,"magic");
    field(get(r.data()+8,2)==1,"schema");
    field(r[10]<=2,"tier");
    auto t=tier(r[10]==0?"fixture-r1":r[10]==1?"benchmark-r1":"production-r1");
    if(expected_t)field(t.code==expected_t->code,"tier");
    u32 b=get(r.data()+12,4);
    field(r[11]==(b&1),"start_code");
    field(b<t.n&&(expected_b<0||b==u32(expected_b)),"block_id");
    field(get(r.data()+16,2)==256,"history_horizon");
    field(get(r.data()+18,2)==256,"followup_horizon");
    field(get(r.data()+20,4)==RECORD_BYTES,"record_bytes");
    for(u32 i=24;i<32;++i)field(r[i]==0,"reserved");
    return t;
  }
  // RECORD primitive checks retries before hashes with RANGE/2. Science checks
  // them only after hashes and state legality, with its own MISMATCH/3 code.
  inline void record_retries(const Bytes&r,bool primitive){
    for(u32 j=0;j<768;++j)if(get(r.data()+RETRY_BASE+4*j,4)>8160){
      Error e(primitive?2:3,primitive?"RANGE":"MISMATCH","retry");
      e.index=j;
      throw e;
    }
  }
  inline void record_hash(const Bytes&r){
    field(sha(r.data(),RECORD_HASH)==mm::to_hex(r.data()+RECORD_HASH,32),"record_sha256",3,"HASH");
  }
  inline void record_states(const Bytes&r){
    u32 b=get(r.data()+12,4);
    for(u32 k=0;k<769;++k){
      State s=decode(r.data()+STATE_BASE+264*k);
      bool ok=legal(s);
      if(k==0)ok=ok&&s.valid==0&&s.policy==((b&1)?UINT32_MAX:0);
      else ok=ok&&s.valid==s.policy;
      if(!ok){
        Error e(3,"STATE","state");
        e.index=k;
        throw e;
      }
    }
  }
  inline std::array<u32,4> record_sums(const Bytes&r){
    auto s=sums_from_record(r);
    const char*names[]={
      "M_C","M_A","A_C","A_A"
    }
    ;
    for(u32 k=0;k<4;++k)field(get(r.data()+SUM_BASE+4*k,4)==s[k],names[k],3,"MISMATCH");
    return s;
  }
  inline J record_value(const Bytes&r){
    auto t=record_metadata(r);
    record_retries(r,true);
    record_hash(r);
    record_states(r);
    auto s=record_sums(r);
    return J::obj({
      {
        "record_bytes",RECORD_BYTES
      }
      ,{
        "block_id",u32(get(r.data()+12,4))
      }
      ,{
        "tier",t.name
      }
      ,{
        "record_sha256",mm::to_hex(r.data()+RECORD_HASH,32)
      }
      ,{
        "M_C",s[0]
      }
      ,{
        "M_A",s[1]
      }
      ,{
        "A_C",s[2]
      }
      ,{
        "A_A",s[3]
      }
    }
    );
  }
  inline Bytes chunk_header(const Tier&t,u32 k){
    Bytes h(64,0);
    std::memcpy(h.data(),"BRS3CH01",8);
    put(h.data()+8,1,2);
    h[10]=t.code;
    put(h.data()+12,k,4);
    put(h.data()+16,k*100,4);
    put(h.data()+20,t.count(k),4);
    put(h.data()+24,RECORD_BYTES,4);
    put(h.data()+28,256,2);
    put(h.data()+30,256,2);
    auto d=unhex(PROTOCOL_SHA);
    std::copy(d.begin(),d.end(),h.begin()+32);
    return h;
  }
  // Shared packing path; opaque fixtures use this exact producer emitter.
  inline Bytes assemble_chunk(const Tier&t,u32 k,const std::vector<Bytes>&records){
    need(k<t.chunks()&&records.size()==t.count(k));
    Bytes chunk=chunk_header(t,k);
    chunk.reserve(64+size_t(t.count(k))*RECORD_BYTES);
    for(auto&r:records){
      need(r.size()==RECORD_BYTES);
      chunk.insert(chunk.end(),r.begin(),r.end());
    }
    return chunk;
  }
  inline J validate_chunk(const Bytes&h,const Tier&t,u32 k){
    field(h.size()==64,"length");
    field(k<t.chunks(),"chunk_index");
    auto want=chunk_header(t,k);
    struct Part{
      u32 off,len;
      const char*name;
    }
    ;
    Part p[]={
      {
        0,8,"magic"
      }
      ,{
        8,2,"schema"
      }
      ,{
        10,1,"tier"
      }
      ,{
        11,1,"reserved"
      }
      ,{
        12,4,"chunk_index"
      }
      ,{
        16,4,"first_block_id"
      }
      ,{
        20,4,"record_count"
      }
      ,{
        24,4,"record_bytes"
      }
      ,{
        28,2,"history_horizon"
      }
      ,{
        30,2,"followup_horizon"
      }
      ,{
        32,32,"protocol_sha256"
      }
    }
    ;
    for(auto&x:p)field(std::equal(h.begin()+x.off,h.begin()+x.off+x.len,want.begin()+x.off),x.name);
    return J::obj({
      {
        "header_bytes",64
      }
      ,{
        "record_count",t.count(k)
      }
      ,{
        "first_block_id",100*k
      }
      ,{
        "expected_file_bytes",u64(64)+u64(t.count(k))*RECORD_BYTES
      }
    }
    );
  }
  inline J manifest_base(const Tier&t){
    return J::obj({
      {
        "schema",1
      }
      ,{
        "study",STUDY
      }
      ,{
        "tier",t.name
      }
      ,{
        "protocol_sha256",PROTOCOL_SHA
      }
      ,{
        "contract_sha256",CONTRACT_SHA
      }
      ,{
        "keys_sha256",KEYS_SHA
      }
      ,{
        "block_count",t.n
      }
      ,{
        "history_horizon",256
      }
      ,{
        "followup_horizon",256
      }
      ,{
        "record_bytes",RECORD_BYTES
      }
      ,{
        "chunk_record_limit",100
      }
      ,{
        "all_f_blocks",t.n/2
      }
      ,{
        "all_m_blocks",t.n/2
      }
      ,{
        "files",J::arr()
      }
    }
    );
  }
  inline J validate_manifest(const std::string&s,const Tier&t){
    J m;
    try{
      m=parse(s,false);
    }
    catch(...){
      fail(2,"FORMAT","json");
    }
    field(json(m)==s,"canonical");
    auto want=manifest_base(t);
    field(m.type==J::O&&m.o.size()==want.o.size(),"keys");
    for(auto&v:want.o)field(m.o.count(v.first),"keys");
    const char*order[]={
      "schema","study","tier","protocol_sha256","contract_sha256","keys_sha256","block_count","history_horizon","followup_horizon","record_bytes","chunk_record_limit","all_f_blocks","all_m_blocks"
    }
    ;
    for(auto n:order)field(dump(m.at(n))==dump(want.at(n)),n);
    auto&f=m.at("files");
    field(f.type==J::A&&f.a.size()==t.chunks(),"files");
    for(u32 k=0;k<t.chunks();++k){
      auto&e=f.a[k];
      try{
        keys(e,{
          "name","bytes","sha256"
        }
        );
        field(e.at("name").str()==chunkname(k)&&e.at("bytes").num()==64+u64(t.count(k))*RECORD_BYTES&&ishex(e.at("sha256").str(),64),"files");
      }
      catch(...){
        fail(2,"FORMAT","files");
      }
    }
    return m;
  }
  inline std::string results(const Tier&t,i64 m,i64 a){
    return "BIT003_RESULTS_V1\nstatus=VALID\ntier="+t.name+"\nn_blocks="+std::to_string(t.n)+"\nn_all_f="+std::to_string(t.n/2)+"\nn_all_m="+std::to_string(t.n/2)+"\npolicy_signed_sum="+std::to_string(m)+"\npolicy_denominator="+std::to_string(u64(t.n)*8192)+"\naccuracy_signed_sum="+std::to_string(a)+"\naccuracy_denominator="+std::to_string(u64(t.n)*262144)+"\ninference=PRODUCTION_ONLY\n";
  }
  // Actual producer publication path. Completed chunks may arrive in any order;
  // the manifest is always assembled by chunk index. No scientific computation
  // occurs here, so fixture tools can supply authenticated opaque records.
  struct ChunkEmission {
    fs::path out;
    Tier t;
    std::vector<J> entries;
    std::vector<bool> seen;
    ChunkEmission(const fs::path&path,const Tier&which):out(path),t(which),
      entries(t.chunks()),seen(t.chunks(),false){
      pathcheck(out,true);
      need(::mkdir(out.c_str(),0700)==0,5,"IO");
      syncdir(out.parent_path());
    }
    void complete(u32 k,const std::vector<Bytes>&records){
      need(k<t.chunks()&&!seen[k]);
      auto chunk=assemble_chunk(t,k,records);
      auto name=chunkname(k);
      atomicfile(out/name,chunk);
      entries[k]=J::obj({
        {"name",name},{"bytes",u64(chunk.size())},{"sha256",sha(chunk)}
      });
      seen[k]=true;
    }
    void finish(){
      J m=manifest_base(t);
      for(u32 k=0;k<t.chunks();++k){
        need(seen[k]);
        m["files"].push(entries[k]);
      }
      auto ms=json(m);
      atomictext(out/"MANIFEST.json",ms);
      atomictext(out/"MANIFEST.sha256",sha(ms)+"  MANIFEST.json\n");
      syncdir(out);
      need(::chmod(out.c_str(),0555)==0,5,"IO");
      syncdir(out.parent_path());
    }
  };
  inline void produce(const fs::path&out,const Tier&t,u32 threads,bool reverse=false,bool descending=false){
    ChunkEmission emission(out,t);
    for(u32 k=0;k<t.chunks();++k){
      u32 n=t.count(k);
      std::vector<Bytes>r(n);
      parallel(n,threads,[&](u32 j){
        u32 i=descending?n-1-j:j;r[i]=produce_record(t,100*k+i,reverse);
      }
      );
      emission.complete(k,r);
    }
    emission.finish();
  }
  // Analyzer does not replay a turnover. It derives all targets and initialization,
  // validates every stored state, then derives endpoint sums from stored follow-up.
  inline std::string analyze(const fs::path&data,const Tier&t,u32 threads){
    std::set<std::string>names={
      "MANIFEST.json","MANIFEST.sha256"
    }
    ;
    for(u32 k=0;k<t.chunks();++k)names.insert(chunkname(k));
    closure(data,names,"FORMAT");
    auto ms=text(readfile(data/"MANIFEST.json",1u<<22,false,"FORMAT"));
    auto seal=text(readfile(data/"MANIFEST.sha256",100,false,"FORMAT"));
    for(u32 k=0;k<t.chunks();++k){
      struct stat st{
      }
      ;
      field(::lstat((data/chunkname(k)).c_str(),&st)==0&&u64(st.st_size)==64+u64(t.count(k))*RECORD_BYTES,"length");
    }
    auto m=validate_manifest(ms,t);
    i64 total_m=0,total_a=0;
    std::exception_ptr best;
    int best_rank=99;
    auto retain=[&](const Error&e){
      int rank=e.code=="FORMAT"?0:e.code=="HASH"?1:e.code=="STATE"?2:3;
      if(rank<best_rank){
        best_rank=rank;
        best=std::make_exception_ptr(e);
      }
    }
    ;
    try{
      field(seal==sha(ms)+"  MANIFEST.json\n","manifest_sha256",3,"HASH");
    }
    catch(const Error&e){
      retain(e);
    }
    for(u32 k=0;k<t.chunks();++k){
      auto chunk=readfile(data/chunkname(k),64+100u*RECORD_BYTES,false,"FORMAT");
      try{
        validate_chunk(Bytes(chunk.begin(),chunk.begin()+64),t,k);
      }
      catch(const Error&e){
        retain(e);
        continue;
      }
      std::vector<Bytes>rs(t.count(k));
      bool schema_ok=true;
      for(u32 i=0;i<t.count(k);++i){
        rs[i]=Bytes(chunk.begin()+64+size_t(i)*RECORD_BYTES,chunk.begin()+64+size_t(i+1)*RECORD_BYTES);
        try{
          record_metadata(rs[i],100*k+i,&t);
        }
        catch(const Error&e){
          retain(e);
          schema_ok=false;
        }
      }
      if(!schema_ok)continue;
      bool hash_ok=true;
      try{
        field(sha(chunk)==m.at("files").a[k].at("sha256").str(),"file_sha256",3,"HASH");
        for(auto&r:rs)record_hash(r);
      }
      catch(const Error&e){
        retain(e);
        hash_ok=false;
      }
      if(!hash_ok)continue;
      std::vector<std::array<u32,4>>sums(rs.size());
      std::vector<std::exception_ptr>errs(rs.size());
      parallel(u32(rs.size()),threads,[&](u32 i){
        try{
          auto&r=rs[i];record_states(r);record_retries(r,false);Rng rng(t,100*k+i);auto targets=rng.targets();for(u32 u=1;u<=512;++u)field(get(r.data()+32+4*(u-1),4)==targets[u-1],"target",3,"MISMATCH");Bytes initial(264);encode(initial.data(),rng.initial());field(std::equal(initial.begin(),initial.end(),r.begin()+STATE_BASE),"initial_state",3,"MISMATCH");sums[i]=record_sums(r);
        }
        catch(...){
          errs[i]=std::current_exception();
        }
      }
      );
      for(u32 i=0;i<rs.size();++i)if(errs[i]){
        try{
          std::rethrow_exception(errs[i]);
        }
        catch(const Error&e){
          retain(e);
        }
      }
      else{
        total_m+=i64(sums[i][0])-sums[i][1];
        total_a+=i64(sums[i][2])-sums[i][3];
      }
    }
    if(best)std::rethrow_exception(best);
    return results(t,total_m,total_a);
  }
}
