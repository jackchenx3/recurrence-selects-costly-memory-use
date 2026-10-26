// Role-owned construction/invariance support. This is not an auditor.
// Source only until SOURCE GO; every runtime use is restricted to fixture-r1.
#include "fixture.hpp"
#include "build_metadata.hpp"
namespace brs {
  inline std::vector<std::string>recipes(){
    return {
      "bitflip","missing_chunk","extra_file","truncated_record","reordered_records","wrong_tier","wrong_keys_hash","wrong_protocol_hash","wrong_contract_hash","duplicate_block","illegal_cache","altered_endpoint_sum_rehashed","altered_retry_rehashed","altered_target_rehashed","altered_state_rehashed","self_consistent_forged_S256_recomputed_branches_sums_and_all_hashes"
    }
    ;
  }
  inline void corrupt(const fs::path&src,const fs::path&out,const std::string&recipe){
    auto names=recipes();
    need(std::find(names.begin(),names.end(),recipe)!=names.end(),2,"CLI");
    auto t=tier("fixture-r1");
    closure(src,{
      "chunk_000.bin","MANIFEST.json","MANIFEST.sha256"
    }
    ,"FORMAT");
    Bytes chunk=readfile(src/"chunk_000.bin");
    auto original=text(readfile(src/"MANIFEST.json"));
    auto original_seal=text(readfile(src/"MANIFEST.sha256"));
    J m=validate_manifest(original,t);
    field(sha(chunk)==m.at("files").a[0].at("sha256").str(),"file_sha256",3,"HASH");
    field(original_seal==sha(original)+"  MANIFEST.json\n","manifest_sha256",3,"HASH");
    field(chunk.size()==64+16u*RECORD_BYTES,"length");
    validate_chunk(Bytes(chunk.begin(),chunk.begin()+64),t,0);
    bool repair=false,missing=false,extra=false;
    u32 rindex=u32(std::find(names.begin(),names.end(),recipe)-names.begin());
    if(rindex==0)chunk[96]^=1;
    else if(rindex==1)missing=true;
    else if(rindex==2)extra=true;
    else if(rindex==3)chunk.pop_back();
    else if(rindex==4){
      std::swap_ranges(chunk.begin()+64,chunk.begin()+64+RECORD_BYTES,chunk.begin()+64+RECORD_BYTES);
      repair=true;
    }
    else if(rindex==5){
      chunk[10]=1;
      repair=true;
    }
    else if(rindex>=6&&rindex<=8){
      m[rindex==6?"keys_sha256":rindex==7?"protocol_sha256":"contract_sha256"]=J(std::string(64,'0'));
      repair=true;
    }
    else if(rindex==9){
      std::copy(chunk.begin()+64,chunk.begin()+64+RECORD_BYTES,chunk.begin()+64+RECORD_BYTES);
      repair=true;
    }
    else{
      Bytes r(chunk.begin()+64,chunk.begin()+64+RECORD_BYTES);
      if(rindex==10)put(r.data()+2208,1,4);
      else if(rindex==11)put(r.data()+SUM_BASE,(get(r.data()+SUM_BASE,4)+1)%8193,4);
      else if(rindex==12)put(r.data()+RETRY_BASE,(get(r.data()+RETRY_BASE,4)+1)%8161,4);
      else if(rindex==13)r[32]^=1;
      else if(rindex==14)r[2344]^=1;
      else if(rindex==15){
        r[69664]^=1;
        Rng rng(t,0);
        State forged=decode(r.data()+69664);
        need(legal(forged),3,"STATE");
        followup(r,rng,forged);
      }
      rehash(r);
      std::copy(r.begin(),r.end(),chunk.begin()+64);
      repair=true;
    }
    if(repair){
      if(rindex<6||rindex>8)m["files"].a[0]["sha256"]=J(sha(chunk));
      original=json(m);
      original_seal=sha(original)+"  MANIFEST.json\n";
    }
    pathcheck(out,true);
    need(::mkdir(out.c_str(),0700)==0,5,"IO");
    syncdir(out.parent_path());
    if(!missing)atomicfile(out/"chunk_000.bin",chunk);
    if(extra)atomicfile(out/"EXTRA.bin",Bytes(1,0));
    atomictext(out/"MANIFEST.json",original);
    atomictext(out/"MANIFEST.sha256",original_seal);
    syncdir(out);
  }
  // Pure opaque-record packer: no RNG and no scientific state interpretation.
  inline Bytes pack_chunk(const Tier&t,u32 k,const std::vector<std::pair<u32,Bytes>>&completed){
    std::vector<Bytes>ordered(t.count(k));
    std::vector<bool>seen(t.count(k));
    for(auto&entry:completed){
      u32 b=entry.first;
      need(b>=100*k&&b<100*k+t.count(k));
      u32 j=b-100*k;
      need(!seen[j]&&entry.second.size()==RECORD_BYTES);
      seen[j]=true;
      ordered[j]=entry.second;
    }
    for(bool b:seen)need(b);
    return assemble_chunk(t,k,ordered);
  }
  // The input is supplied by authenticated FIXTURE_VIEW, never synthesized as
  // a replacement for the producer path. Interpret no state/target/RNG fields.
  inline std::string packing_check(const Bytes&opaque,const fs::path&work){
    need(opaque.size()==RECORD_BYTES,2,"INPUT");
    auto t=tier("production-r1");
    pathcheck(work,true);
    need(::mkdir(work.c_str(),0700)==0,5,"IO");
    syncdir(work.parent_path());
    // Preserve both actual emitted trees as fixture evidence, including failure.
    ChunkEmission forward(work/"forward",t),reverse(work/"reverse",t);
    for(u32 ordinal=0;ordinal<t.chunks();++ordinal){
      // Same supplied unmodified authenticated blob for each opaque slot. Chunk
      // identity resides in the real producer header, not in interpreted records.
      for(u32 pass=0;pass<2;++pass){
        u32 k=pass?t.chunks()-1-ordinal:ordinal;
        std::vector<std::pair<u32,Bytes>>completed;
        for(u32 j=0;j<t.count(k);++j)completed.emplace_back(100*k+j,opaque);
        auto packed=pack_chunk(t,k,completed);
        std::reverse(completed.begin(),completed.end());
        need(packed==pack_chunk(t,k,completed),3,"MISMATCH");
        std::vector<Bytes>records(t.count(k));
        for(auto&v:completed)records[v.first-100*k]=std::move(v.second);
        // These calls perform actual chunk IO, then the shared real manifest IO.
        (pass?reverse:forward).complete(k,records);
      }
    }
    forward.finish();
    reverse.finish();
    std::set<std::string>names={"MANIFEST.json","MANIFEST.sha256"};
    for(u32 k=0;k<t.chunks();++k)names.insert(chunkname(k));
    closure(work/"forward",names,"FORMAT");
    closure(work/"reverse",names,"FORMAT");
    J fi=J::arr(),ri=J::arr();
    for(auto&name:names){
      auto a=readfile(work/"forward"/name),b=readfile(work/"reverse"/name);
      need(a==b,3,"MISMATCH");
      fi.push(J::obj({{"name",name},{"bytes",u64(a.size())},{"sha256",sha(a)}}));
      ri.push(J::obj({{"name",name},{"bytes",u64(b.size())},{"sha256",sha(b)}}));
    }
    need(json(fi)==json(ri),3,"MISMATCH");
    return sha(json(fi));
  }
  inline J rolechecks(u32 threads){
    auto t=tier("fixture-r1");
    std::vector<Bytes>base(16),variant(16);
    parallel(16,threads,[&](u32 b){
      base[b]=produce_record(t,b);
    }
    );
    parallel(16,threads,[&](u32 j){
      u32 b=15-j;variant[b]=produce_record(t,b,true);
    }
    );
    for(u32 b=0;b<16;++b)need(base[b]==variant[b],3,"MISMATCH");
    mm::Sha256 h;
    for(auto&r:base)h.update(r.data(),r.size());
    need(sha(std::string("abc"))=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",3,"MISMATCH");
    // Direct arithmetic invariants, not production observations.
    need(classify(Rat(-1,32),Rat(1,32))=="BOUNDED_WITHIN_MARGIN"&&classify(Rat(1,32),Rat(1,16))=="POSITIVE_MAGNITUDE_UNRESOLVED",3,"MISMATCH");
    return J::obj({
      {
        "schema",1
      }
      ,{
        "scope","OWN_PRODUCER_FIXTURE_INVARIANCE_ONLY"
      }
      ,{
        "threads",threads
      }
      ,{
        "fixture_records_sha256",mm::to_hex(h.finish())
      }
      ,{
        "reverse_arm_and_completion_equal",true
      }
      ,{
        "source_sha256",BRS_SOURCE_SHA
      }
      ,{
        "binary_sha256",binary_sha()
      }
    }
    );
  }
}
int main(int argc,char**argv){
  using namespace brs;
  try{
    std::map<std::string,std::string>opt;
    for(int i=1;i<argc;i+=2){
      need(i+1<argc&&!opt.count(argv[i]),2,"CLI");
      opt[argv[i]]=argv[i+1];
    }
    need(opt.count("--mode")&&opt.count("--inputs")&&opt.count("--output"),2,"CLI");
    std::string mode=opt.at("--mode");
    need(mode=="corrupt"||mode=="invariance"||mode=="packing",2,"CLI");
    std::set<std::string>want=mode=="corrupt"?std::set<std::string>{
      "--mode","--inputs","--output","--data","--recipe"
    }
    :mode=="invariance"?std::set<std::string>{
      "--mode","--inputs","--output","--threads"
    }:std::set<std::string>{"--mode","--inputs","--output"}
    ;
    need(opt.size()==want.size(),2,"CLI");
    for(auto&k:want)need(opt.count(k),2,"CLI");
    pathcheck(opt.at("--inputs"));
    pathcheck(opt.at("--output"),true);
    if(mode=="corrupt")pathcheck(opt.at("--data"));
    Inputs in(opt.at("--inputs"),mode=="packing");
    if(mode=="corrupt")corrupt(opt.at("--data"),opt.at("--output"),opt.at("--recipe"));
    else if(mode=="packing"){
      auto inventory=packing_check(in.at("FIXIN_BUNDLE/literals/SYNTHETIC_RECORD.bin"),
        fs::path(opt.at("--output")+".packing-evidence"));
      atomictext(opt.at("--output"),json(J::obj({
        {"schema",1},{"scope","OWN_OPAQUE_PRODUCER_EMISSION_FIXTURE_ONLY"},
        {"opaque_source_sha256",sha(in.at("FIXIN_BUNDLE/literals/SYNTHETIC_RECORD.bin"))},
        {"emitted_inventory_sha256",inventory},{"forward_reverse_equal",true},
        {"source_sha256",BRS_SOURCE_SHA},{"binary_sha256",binary_sha()}
      })));
    }
    else{
      u32 n=decimal(opt.at("--threads"),32);
      need(n>=1,2,"CLI");
      atomictext(opt.at("--output"),json(rolechecks(n)));
    }
    return 0;
  }
  catch(const Error&e){
    std::cerr<<"BRS3_ERROR "<<e.code<<"\n";
    return e.exit;
  }
  catch(const std::bad_alloc&){
    std::cerr<<"BRS3_ERROR RESOURCE\n";
    return 5;
  }
  catch(...){
    std::cerr<<"BRS3_ERROR INTERNAL\n";
    return 5;
  }
}
