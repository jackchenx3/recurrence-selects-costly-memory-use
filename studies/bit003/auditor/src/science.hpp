#pragma once
#include "runtime.hpp"
namespace brc {
enum Purpose {INITIAL_GENOTYPE,TARGET_INNOVATION,TARGET_COPY,FRESH_MASK,SCOUT_MASK,LOCAL_BIT,DONOR_KEY,SURVIVAL_UNIFORM,POLICY_MUTATION,PURPOSES};
inline const std::array<std::string,PURPOSES>& purposes(){static const std::array<std::string,PURPOSES> p{{"INITIAL_GENOTYPE","TARGET_INNOVATION","TARGET_COPY","FRESH_MASK","SCOUT_MASK","LOCAL_BIT","DONOR_KEY","SURVIVAL_UNIFORM","POLICY_MUTATION"}};
return p;
}
using Words=std::array<uint32_t,4>;
using Key=std::array<uint32_t,2>;
// Unchanged Philox round/extraction from own independent auditor baseline.
inline Words philox(Words c,Key k){for(unsigned r=0;r<10;++r){uint64_t a=uint64_t(0xd2511f53U)*c[0],b=uint64_t(0xcd9e8d57U)*c[2];
c={{uint32_t(b>>32)^c[1]^k[0],uint32_t(b),uint32_t(a>>32)^c[3]^k[1],uint32_t(a)}};
if(r!=9){k[0]+=0x9e3779b9U;
k[1]+=0xbb67ae85U;
}}return c;
}
inline Key derive(const Tier& t,Purpose p){auto d=sha(std::string(STUDY)+"|"+t.name+"|"+purposes()[p]);
Bytes b(d.begin(),d.end());
return {{uint32_t(get(b,0,4)),uint32_t(get(b,4,4))}};
}
inline void keytable(const fs::path& inputs){std::map<std::string,std::string> rows;
std::set<uint64_t> unique;
for(auto tn:{"production-r1","fixture-r1","benchmark-r1"})for(unsigned p=0;p<PURPOSES;++p){auto t=tier(tn);
std::string name=std::string(STUDY)+"|"+tn+"|"+purposes()[p];
auto d=sha(name);
auto k=derive(t,Purpose(p));
uint64_t w=k[0]|(uint64_t(k[1])<<32);
ck(unique.insert(w).second,2,"INPUT","key collision");
rows[name]=name+"\t"+hex(d)+"\t"+hx(k[0],8)+"\t"+hx(k[1],8)+"\t"+hx(w,16)+"\n";
}std::string s="key_string\tsha256\tk0\tk1\tkey64\n";
for(auto& x:rows)s+=x.second;
ck(s==str(inputfile(inputs/"KEYS.tsv")),2,"INPUT","derived key table");
}
class RNG {
 Tier tr;
std::array<Key,PURPOSES> k;
public:explicit RNG(Tier t):tr(t){for(unsigned p=0;p<PURPOSES;++p)k[p]=derive(t,Purpose(p));
}
 Words at(Purpose p,uint32_t b,uint32_t u,uint32_t e,uint32_t s=0)const{
 require(p<PURPOSES&&b<tr.n,"random block/purpose");
if(p==INITIAL_GENOTYPE)require(u==0&&e<32&&s==0,"initial address");
else{require(u>=1&&u<=512,"update address");
if(p==TARGET_COPY||p==TARGET_INNOVATION)require(e==0&&s==0,"target address");
else if(p==LOCAL_BIT)require(e<32&&s<32,"local address");
else if(p==DONOR_KEY)require(e<96&&s==0,"donor address");
else if(p==SURVIVAL_UNIFORM)require(e<32&&s<256,"survival address");
else require(e<32&&s==0,"slot address");
}return philox({{b,u,e,s}},k[p]);
}
 uint32_t word(Purpose p,uint32_t b,unsigned u,unsigned e,unsigned s=0)const{return at(p,b,u,e,s)[0];
}
 uint64_t wide(Purpose p,uint32_t b,unsigned u,unsigned e,unsigned s=0)const{auto w=at(p,b,u,e,s);
return w[0]|(uint64_t(w[1])<<32);
}
};
struct State{std::array<uint32_t,32>x{},cache{};
uint32_t policy=0,valid=0;
};
inline void encode_into(Bytes& b,size_t off,const State& s){for(unsigned i=0;i<32;++i){put(b,off+4*i,s.x[i],4);
put(b,off+128+4*i,s.cache[i],4);
}put(b,off+256,s.policy,4);
put(b,off+260,s.valid,4);
}
inline State decode(const Bytes& b,size_t off){State s;
for(unsigned i=0;i<32;++i){s.x[i]=get(b,off+4*i,4);
s.cache[i]=get(b,off+128+4*i,4);
}s.policy=get(b,off+256,4);
s.valid=get(b,off+260,4);
return s;
}
inline bool legal(const State& s){if(s.valid&~s.policy)return false;
for(unsigned i=0;i<32;++i)if(!((s.valid>>i)&1)&&s.cache[i])return false;
return true;
}
struct Tape{std::array<uint32_t,32>fresh{},scout{},local{};
std::array<uint64_t,96>donor{};
uint32_t flip=0;
const RNG* rng=nullptr;
unsigned b=0,u=0;
};
inline Tape tape(const RNG& rng,unsigned b,unsigned u){Tape t;
t.rng=&rng;
t.b=b;
t.u=u;
for(unsigned i=0;i<32;++i){t.fresh[i]=rng.word(FRESH_MASK,b,u,i);
t.scout[i]=rng.word(SCOUT_MASK,b,u,i);
if((rng.word(POLICY_MUTATION,b,u,i)&31)==0)t.flip|=uint32_t(1)<<i;
for(unsigned bit=0;bit<32;++bit)if((rng.word(LOCAL_BIT,b,u,i,bit)&31)==0)t.local[i]|=uint32_t(1)<<bit;
}for(unsigned i=0;i<96;++i)t.donor[i]=rng.wide(DONOR_KEY,b,u,i);
return t;
}
struct Turn{State state;
unsigned retries=0,accuracy=0;
};
inline Turn turnover(const State& s,uint32_t target,bool replace,const Tape& t){
 std::array<uint32_t,128> g;
std::array<unsigned,128> h;
std::array<uint64_t,128>w;
 uint32_t eligible=s.policy&s.valid;
 for(unsigned i=0;i<32;++i){g[i]=s.x[i];
g[32+i]=!replace&&((eligible>>i)&1)?s.cache[i]:s.x[i]^t.fresh[i];
g[64+i]=s.x[i]^t.scout[i];
unsigned best=0;
for(unsigned f=0;f<3;++f){h[32*f+i]=pop(g[32*f+i]^target);
if(f&&(h[32*f+i]<h[32*best+i]||(h[32*f+i]==h[32*best+i]&&t.donor[3*i+f]<t.donor[3*i+best])))best=f;
}g[96+i]=g[32*best+i]^t.local[i];
h[96+i]=pop(g[96+i]^target);
}
 uint64_t W=0;
for(unsigned j=0;j<128;++j){w[j]=uint64_t(1)<<(32-h[j]);
W+=w[j];
}
 Turn out;
 for(unsigned d=0;d<32;++d){uint64_t threshold=(uint64_t(0)-W)%W,z=0;
unsigned retry=0;
for(;retry<256;++retry){uint64_t x=t.rng->wide(SURVIVAL_UNIFORM,t.b,t.u,d,retry);
__uint128_t m=__uint128_t(x)*W;
if(uint64_t(m)>=threshold){z=m>>64;
break;
}}ck(retry<256,4,"RETRY_CAP","b="+std::to_string(t.b)+" u="+std::to_string(t.u)+" draw="+std::to_string(d));
out.retries+=retry;
 uint64_t sum=0;
unsigned j=0;
for(;j<128;++j){sum+=w[j];
if(sum>z)break;
}require(j<128&&w[j],"PL selection");
W-=w[j];
w[j]=0;
unsigned p=j%32;
bool pol=((s.policy>>p)^(t.flip>>d))&1;
out.state.x[d]=g[j];
if(pol){out.state.policy|=uint32_t(1)<<d;
out.state.valid|=uint32_t(1)<<d;
out.state.cache[d]=s.x[p];
}out.accuracy+=32-h[j];
}
 return out;
}
inline State initial(const RNG& r,unsigned b){State s;
s.policy=(b&1)?UINT32_MAX:0;
for(unsigned i=0;i<32;++i)s.x[i]=r.word(INITIAL_GENOTYPE,b,0,i);
return s;
}
inline std::array<uint32_t,513> targets(const RNG& r,unsigned b){std::array<uint32_t,513> t{};
for(unsigned u=1;u<=512;++u){uint32_t in=r.word(TARGET_INNOVATION,b,u,0);
unsigned copy=r.word(TARGET_COPY,b,u,0)&1;
t[u]=u>=3&&copy?t[u-2]:in;
}return t;
}
constexpr size_t RECORD=208216,STATE_OFFSET=2080,RETRY_OFFSET=205096,SUM_OFFSET=208168,HASH_OFFSET=208184;
inline void rehash(Bytes& r){SHA256 h;
h.update(r.data(),HASH_OFFSET);
auto d=h.finish();
std::copy(d.begin(),d.end(),r.begin()+HASH_OFFSET);
}
inline Bytes generate(const Tier& tr,const RNG& rng,unsigned b,bool reverse_arms=false,bool forged=false){Bytes out(RECORD,0);
std::copy_n("BRS3R001",8,out.begin());
put(out,8,1,2);
put(out,10,tr.code,1);
put(out,11,b&1,1);
put(out,12,b,4);
put(out,16,256,2);
put(out,18,256,2);
put(out,20,RECORD,4);
auto tg=targets(rng,b);
for(unsigned u=1;u<=512;++u)put(out,32+4*(u-1),tg[u],4);
State s=initial(rng,b);
encode_into(out,STATE_OFFSET,s);
 for(unsigned u=1;u<=256;++u){auto t=tape(rng,b,u);
auto v=turnover(s,tg[u],false,t);
s=v.state;
encode_into(out,STATE_OFFSET+264*u,s);
put(out,RETRY_OFFSET+4*(u-1),v.retries,4);
}
 if(forged){s.x[0]^=1;
encode_into(out,STATE_OFFSET+264*256,s);
}
 State c=s,a=s;
uint64_t mc=0,ma=0,ac=0,aa=0;
 for(unsigned u=257;u<=512;++u){auto t=tape(rng,b,u);
Turn C,A;
if(reverse_arms){A=turnover(a,tg[u],true,t);
C=turnover(c,tg[u],false,t);
}else{C=turnover(c,tg[u],false,t);
A=turnover(a,tg[u],true,t);
}c=C.state;
a=A.state;
encode_into(out,STATE_OFFSET+264*u,c);
encode_into(out,STATE_OFFSET+264*(u+256),a);
put(out,RETRY_OFFSET+4*(u-1),C.retries,4);
put(out,RETRY_OFFSET+4*(u+255),A.retries,4);
mc+=pop(c.policy);
ma+=pop(a.policy);
ac+=C.accuracy;
aa+=A.accuracy;
}
 put(out,SUM_OFFSET,mc,4);
put(out,SUM_OFFSET+4,ma,4);
put(out,SUM_OFFSET+8,ac,4);
put(out,SUM_OFFSET+12,aa,4);
rehash(out);
return out;
}
inline std::string results(const Tier& t,int64_t m,int64_t a){return "BIT003_RESULTS_V1\nstatus=VALID\ntier="+t.name+"\nn_blocks="+std::to_string(t.n)+"\nn_all_f="+std::to_string(t.n/2)+"\nn_all_m="+std::to_string(t.n/2)+"\npolicy_signed_sum="+std::to_string(m)+"\npolicy_denominator="+std::to_string(uint64_t(t.n)*8192)+"\naccuracy_signed_sum="+std::to_string(a)+"\naccuracy_denominator="+std::to_string(uint64_t(t.n)*262144)+"\ninference=PRODUCTION_ONLY\n";
}
} // namespace brc
