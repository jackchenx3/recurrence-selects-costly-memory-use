#pragma once
#include "science.hpp"
namespace brc {
inline std::string chunkname(unsigned c){return "chunk_"+([](unsigned i){std::ostringstream o;o<<std::setw(3)<<std::setfill('0')<<i;return o.str();})(c)+".bin";
}
inline J manifest(const Tier& t){J j;
auto n=[&](const char* k,uint64_t v){j.o[k]=J::number(v);
};
auto s=[&](const char* k,const std::string& v){j.o[k]=J::text(v);
};
n("schema",1);
s("study",STUDY);
s("tier",t.name);
s("protocol_sha256",PROTOCOL);
s("contract_sha256",CONTRACT);
s("keys_sha256",KEYHASH);
n("block_count",t.n);
n("history_horizon",256);
n("followup_horizon",256);
n("record_bytes",RECORD);
n("chunk_record_limit",100);
n("all_f_blocks",t.n/2);
n("all_m_blocks",t.n/2);
J f;
f.kind=J::ARR;
j.o["files"]=f;
return j;
}
inline void validate_manifest(const J& j,const Tier& t){J m=manifest(t);
ck(j.kind==J::OBJ&&j.o.size()==m.o.size(),2,"FORMAT","manifest keys");
for(auto& x:m.o){ck(j.o.count(x.first),2,"FORMAT","manifest keys");
if(x.first!="files")ck(dump(j.at(x.first))==dump(x.second),2,"FORMAT","manifest "+x.first);
}auto& f=j.at("files");
ck(f.kind==J::ARR&&f.a.size()==t.chunks(),2,"FORMAT","manifest files");
for(unsigned c=0;c<t.chunks();++c){auto& row=f.a[c];
keys(row,{"name","bytes","sha256"});
ck(row.at("name").text()==chunkname(c)&&row.at("bytes").u()==64+uint64_t(t.count(c))*RECORD&&ishex(row.at("sha256").text(),64),2,"FORMAT","manifest file row");
}}
inline Bytes chunkheader(const Tier& t,unsigned c){Bytes h(64,0);
std::copy_n("BRS3CH01",8,h.begin());
put(h,8,1,2);
put(h,10,t.code,1);
put(h,12,c,4);
put(h,16,100*c,4);
put(h,20,t.count(c),4);
put(h,24,RECORD,4);
put(h,28,256,2);
put(h,30,256,2);
auto d=unhex(PROTOCOL);
std::copy(d.begin(),d.end(),h.begin()+32);
return h;
}
inline void record_header(const Bytes& r,const Tier& t,unsigned b){ck(r.size()==RECORD,2,"FORMAT","record length");
ck(std::equal(r.begin(),r.begin()+8,"BRS3R001")&&get(r,8,2)==1&&get(r,10,1)==t.code&&get(r,11,1)==(b&1)&&get(r,12,4)==b&&get(r,16,2)==256&&get(r,18,2)==256&&get(r,20,4)==RECORD&&get(r,24,8)==0,2,"FORMAT","record metadata b="+std::to_string(b));
}
inline void record_states(const Bytes& r,unsigned b){for(unsigned k=0;k<769;++k){auto s=decode(r,STATE_OFFSET+264*k);
bool ok=legal(s);
if(k==0)ok=ok&&s.valid==0&&s.policy==((b&1)?UINT32_MAX:0);
else ok=ok&&s.valid==s.policy;
ck(ok,3,"STATE","b="+std::to_string(b)+" state="+std::to_string(k));
}}
inline std::array<uint64_t,4> endpoints(const Bytes& r,unsigned b){std::array<uint64_t,4> sum{};
for(unsigned u=257;u<=512;++u){uint32_t target=get(r,32+4*(u-1),4);
for(unsigned arm=0;arm<2;++arm){auto s=decode(r,STATE_OFFSET+264*(u+256*arm));
sum[arm]+=pop(s.policy);
for(auto g:s.x)sum[2+arm]+=32-pop(g^target);
}}for(unsigned a=0;a<4;++a)ck(sum[a]==get(r,SUM_OFFSET+4*a,4),3,"MISMATCH","b="+std::to_string(b)+" endpoint="+std::to_string(a));
return sum;
}
inline void compare(const Bytes& actual,const Bytes& own,unsigned b){if(actual==own)return;
size_t off=0;
while(off<RECORD&&actual[off]==own[off])++off;
std::string loc="b="+std::to_string(b)+" byte="+std::to_string(off);
if(off>=32&&off<STATE_OFFSET)loc+=" target="+std::to_string((off-32)/4+1);
else if(off>=STATE_OFFSET&&off<RETRY_OFFSET)loc+=" state="+std::to_string((off-STATE_OFFSET)/264);
else if(off>=RETRY_OFFSET&&off<SUM_OFFSET)loc+=" retry="+std::to_string((off-RETRY_OFFSET)/4);
throw Fail(3,"MISMATCH",loc);
}
inline Bytes datafile(const fs::path& p){try{check_regular(p);
return readfile(p,64+100*RECORD);
}catch(const Fail&){throw Fail(2,"FORMAT","data file type");
}catch(...){throw Fail(5,"IO","data read");
}}
struct AuditTotals{int64_t m=0,a=0;
};
inline AuditTotals audit_data(const fs::path& data,const Tier& t,unsigned nt){
 ck(fs::is_directory(data),2,"FORMAT","data directory");
std::set<std::string> want={"MANIFEST.json","MANIFEST.sha256"},got;
for(unsigned c=0;c<t.chunks();++c)want.insert(chunkname(c));
for(auto& e:fs::directory_iterator(data)){got.insert(e.path().filename().string());
struct stat st{};
ck(::lstat(e.path().c_str(),&st)==0&&S_ISREG(st.st_mode),2,"FORMAT","data file type");
}ck(got==want,2,"FORMAT","data inventory");
for(unsigned c=0;c<t.chunks();++c)ck(fs::file_size(data/chunkname(c))==64+uint64_t(t.count(c))*RECORD,2,"FORMAT","chunk length");
 J j;
Bytes mb,ms;
try{mb=readfile(data/"MANIFEST.json",1000000);
ms=readfile(data/"MANIFEST.sha256",256);
j=parse(mb);
validate_manifest(j,t);
}catch(const Fail&){throw;
}catch(...){throw Fail(2,"FORMAT","manifest schema");
}
 std::exception_ptr firsthash;
 try{ck(str(ms)==hex(sha(mb))+"  MANIFEST.json\n",3,"HASH","manifest checksum");
}catch(...){firsthash=std::current_exception();
}
 // Complete schema/metadata pass precedes any hash failure; bounded one chunk RAM.
 for(unsigned c=0;c<t.chunks();++c){auto blob=datafile(data/chunkname(c));
auto h=chunkheader(t,c);
ck(std::equal(h.begin(),h.end(),blob.begin()),2,"FORMAT","chunk header");
for(unsigned i=0;i<t.count(c);++i)record_header(slice(blob,64+i*RECORD,RECORD),t,100*c+i);
try{ck(hex(sha(blob))==j.at("files").a[c].at("sha256").text(),3,"HASH","chunk checksum");
for(unsigned i=0;i<t.count(c);++i){size_t o=64+i*RECORD;
SHA256 sh;
sh.update(blob.data()+o,HASH_OFFSET);
ck(eq(blob,o+HASH_OFFSET,sh.finish()),3,"HASH","record checksum b="+std::to_string(100*c+i));
}}catch(...){if(!firsthash)firsthash=std::current_exception();
}}
 if(firsthash)std::rethrow_exception(firsthash);
 RNG rng(t);
AuditTotals total;
std::exception_ptr stateerror,checkerror,replayerror;
 for(unsigned c=0;c<t.chunks();++c){auto blob=datafile(data/chunkname(c));
ck(hex(sha(blob))==j.at("files").a[c].at("sha256").text(),3,"HASH","chunk changed during audit");
std::vector<std::exception_ptr> se(t.count(c)),ce(t.count(c)),re(t.count(c));
std::vector<std::array<uint64_t,4>> sums(t.count(c));
parallel(t.count(c),nt,[&](unsigned i){unsigned b=100*c+i;auto r=slice(blob,64+i*RECORD,RECORD);try{record_states(r,b);}catch(...){se[i]=std::current_exception();return;}try{auto tg=targets(rng,b);for(unsigned u=1;u<=512;++u)ck(get(r,32+4*(u-1),4)==tg[u],3,"MISMATCH","b="+std::to_string(b)+" target="+std::to_string(u));for(unsigned k=0;k<768;++k)ck(get(r,RETRY_OFFSET+4*k,4)<=8160,3,"MISMATCH","b="+std::to_string(b)+" retry range="+std::to_string(k));sums[i]=endpoints(r,b);}catch(...){ce[i]=std::current_exception();return;}try{auto own=generate(t,rng,b);compare(r,own,b); // includes every target/state/retry/sum and record SHA
 // Aggregate from independently generated records, after exact byte agreement.
 for(unsigned k=0;k<4;++k)sums[i][k]=get(own,SUM_OFFSET+4*k,4);
 }catch(...){re[i]=std::current_exception();}});
 for(unsigned i=0;i<t.count(c);++i){if(!stateerror&&se[i])stateerror=se[i];
if(!checkerror&&ce[i])checkerror=ce[i];
if(!replayerror&&re[i])replayerror=re[i];
total.m+=int64_t(sums[i][0])-int64_t(sums[i][1]);
total.a+=int64_t(sums[i][2])-int64_t(sums[i][3]);
}}
 if(stateerror)std::rethrow_exception(stateerror);
if(checkerror)std::rethrow_exception(checkerror);
if(replayerror)std::rethrow_exception(replayerror);
return total;
}
} // namespace brc
