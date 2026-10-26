#pragma once
#include "io.hpp"
namespace brs {
  enum Purpose{
    INITIAL,INNOV,COPY,FRESH,SCOUT,LOCAL,DONOR,SURVIVAL,MUTATION
  }
  ;
  inline const std::array<std::string,9>&purposes(){
    static const std::array<std::string,9>p={
      "INITIAL_GENOTYPE","TARGET_INNOVATION","TARGET_COPY","FRESH_MASK","SCOUT_MASK","LOCAL_BIT","DONOR_KEY","SURVIVAL_UNIFORM","POLICY_MUTATION"
    }
    ;
    return p;
  }
  struct State {
    std::array<u32,32>x{
    }
    ,c{
    }
    ;
    u32 policy=0,valid=0;
  }
  ;
  inline bool legal(const State&s){
    if(s.valid&~s.policy)return false;
    for(u32 i=0;i<32;++i)if(!(s.valid&(u32(1)<<i))&&s.c[i])return false;
    return true;
  }
  inline void encode(u8*p,const State&s){
    for(u32 i=0;i<32;++i){
      put(p+4*i,s.x[i],4);
      put(p+128+4*i,s.c[i],4);
    }
    put(p+256,s.policy,4);
    put(p+260,s.valid,4);
  }
  inline State decode(const u8*p){
    State s;
    for(u32 i=0;i<32;++i){
      s.x[i]=get(p+4*i,4);
      s.c[i]=get(p+128+4*i,4);
    }
    s.policy=get(p+256,4);
    s.valid=get(p+260,4);
    return s;
  }
  inline u32 accuracy(const State&s,u32 target){
    u32 a=0;
    for(auto g:s.x)a+=32-pop(g^target);
    return a;
  }
  struct Tape{
    std::array<u32,32>fresh{
    }
    ,scout{
    }
    ,local{
    }
    ;
    std::array<u64,96>donor{
    }
    ;
    u32 flip=0;
    std::function<u64(u32,u32)>survival;
  }
  ;
  struct Rng{
    Tier t;
    u32 b;
    std::array<mm::PhiloxKey,9>keys{
    }
    ;
    Rng(Tier t_,u32 b_):t(std::move(t_)),b(b_){
      need(b<t.n);
      for(u32 p=0;p<9;++p){
        const std::string key_string=std::string(STUDY)+"|"+t.name+"|"+purposes()[p];
        auto d=mm::sha256_bytes(key_string.data(),key_string.size());
        keys[p]={
          u32(get(d.data(),4)),u32(get(d.data()+4,4))
        }
        ;
      }
    }
    mm::PhiloxOutput words(u32 p,u32 u,u32 e=0,u32 s=0)const{
      return mm::philox4x32_10({
        {
          b,u,e,s
        }
      }
      ,keys[p]);
    }
    u32 word(u32 p,u32 u,u32 e=0,u32 s=0)const{
      return words(p,u,e,s)[0];
    }
    u64 wide(u32 p,u32 u,u32 e=0,u32 s=0)const{
      auto w=words(p,u,e,s);
      return u64(w[0])|(u64(w[1])<<32);
    }
    Tape tape(u32 u)const{
      Tape d;
      for(u32 i=0;i<32;++i){
        d.fresh[i]=word(FRESH,u,i);
        d.scout[i]=word(SCOUT,u,i);
        for(u32 bit=0;bit<32;++bit)if((word(LOCAL,u,i,bit)&31)==0)d.local[i]|=u32(1)<<bit;
        for(u32 f=0;f<3;++f)d.donor[3*i+f]=wide(DONOR,u,3*i+f);
        if((word(MUTATION,u,i)&31)==0)d.flip|=u32(1)<<i;
      }
      d.survival=[this,u](u32 draw,u32 retry){
        return wide(SURVIVAL,u,draw,retry);
      }
      ;
      return d;
    }
    std::array<u32,512>targets()const{
      std::array<u32,512>t{
      }
      ;
      for(u32 u=1;u<=512;++u){
        u32 inn=word(INNOV,u),copy=word(COPY,u)&1;
        t[u-1]=(u>=3&&copy)?t[u-3]:inn;
      }
      return t;
    }
    State initial()const{
      State s;
      s.policy=(b&1)?UINT32_MAX:0;
      for(u32 i=0;i<32;++i)s.x[i]=word(INITIAL,0,i);
      return s;
    }
  }
  ;
  struct Pick{
    u32 retries;
    u64 z;
  }
  ;
  template<class F>inline Pick lemire(u64 w,F source){
    need(w>=1&&w<=(u64(1)<<39));
    u64 threshold=(u64(0)-w)%w;
    for(u32 r=0;r<256;++r){
      __uint128_t m=__uint128_t(source(r))*w;
      if(u64(m)>=threshold)return{
        r,u64(m>>64)
      }
      ;
    }
    Error e(4,"RETRY_CAP");
    e.retry=256;
    throw e;
  }
  struct Turn{
    State post;
    std::array<u32,128>g{
    }
    ;
    std::array<u32,32>selected{
    }
    ,retries{
    }
    ,donor{
    }
    ;
    u32 rejections=0;
  }
  ;
  // Integer Fenwick selection equals the reference ascending cumulative scan.
  // Every weight remains separate, even for identical candidate genotypes.
  inline Turn turnover(const State&s,u32 target,u32 q,const Tape&d){
    need(legal(s)&&!(q&~(s.policy&s.valid)),3,"STATE");
    Turn o;
    std::array<u32,128>m{
    }
    ;
    for(u32 i=0;i<32;++i){
      u32 bit=u32(1)<<i;
      o.g[i]=s.x[i];
      o.g[32+i]=((s.policy&s.valid&bit)&&!(q&bit))?s.c[i]:(s.x[i]^d.fresh[i]);
      o.g[64+i]=s.x[i]^d.scout[i];
    }
    for(u32 j=0;j<96;++j)m[j]=pop(o.g[j]^target);
    for(u32 i=0;i<32;++i){
      u32 best=0;
      for(u32 f=1;f<3;++f)if(m[32*f+i]<m[32*best+i]||(m[32*f+i]==m[32*best+i]&&d.donor[3*i+f]<d.donor[3*i+best]))best=f;
      o.donor[i]=best;
      o.g[96+i]=o.g[32*best+i]^d.local[i];
      m[96+i]=pop(o.g[96+i]^target);
    }
    std::array<u64,129>tree{
    }
    ;
    std::array<u64,128>w{
    }
    ;
    u64 total=0;
    for(u32 j=0;j<128;++j){
      w[j]=u64(1)<<(32-m[j]);
      total+=w[j];
      tree[j+1]+=w[j];
      u32 p=(j+1)+((j+1)&(~j));
      if(p<=128)tree[p]+=tree[j+1];
    }
    for(u32 snew=0;snew<32;++snew){
      Pick p;
      try{
        p=lemire(total,[&](u32 r){
          return d.survival(snew,r);
        }
        );
      }
      catch(Error&e){
        e.draw=int(snew);
        throw;
      }
      u32 idx=0;
      u64 z=p.z;
      for(u32 bit=128;bit;bit>>=1){
        u32 k=idx+bit;
        if(k<=128&&tree[k]<=z){
          z-=tree[k];
          idx=k;
        }
      }
      need(idx<128&&w[idx],5,"INTERNAL");
      u32 j=idx;
      o.selected[snew]=j;
      o.retries[snew]=p.retries;
      o.rejections+=p.retries;
      u64 remove=w[j];
      total-=remove;
      for(u32 k=j+1;k<=128;k+=k&(~k+1))tree[k]-=remove;
      w[j]=0;
      u32 parent=j%32,bit=u32(1)<<snew;
      o.post.x[snew]=o.g[j];
      if(((s.policy>>parent)^(d.flip>>snew))&1){
        o.post.policy|=bit;
        o.post.valid|=bit;
        o.post.c[snew]=s.x[parent];
      }
    }
    return o;
  }
  inline void rehash(Bytes&r){
    need(r.size()==RECORD_BYTES);
    auto h=mm::sha256_bytes(r.data(),RECORD_HASH);
    std::copy(h.begin(),h.end(),r.begin()+RECORD_HASH);
  }
  inline void followup(Bytes&r,const Rng&rng,const State&checkpoint,bool reverse=false){
    State c=checkpoint,a=checkpoint;
    std::array<u32,4>sums{
    }
    ;
    for(u32 u=257;u<=512;++u){
      u32 target=get(r.data()+32+4*(u-1),4);
      auto d=rng.tape(u);
      Turn tc,ta;
      if(reverse){
        ta=turnover(a,target,a.policy&a.valid,d);
        tc=turnover(c,target,0,d);
      }
      else{
        tc=turnover(c,target,0,d);
        ta=turnover(a,target,a.policy&a.valid,d);
      }
      c=tc.post;
      a=ta.post;
      encode(r.data()+STATE_BASE+264*u,c);
      encode(r.data()+STATE_BASE+264*(u+256),a);
      put(r.data()+RETRY_BASE+4*(u-1),tc.rejections,4);
      put(r.data()+RETRY_BASE+4*(u+255),ta.rejections,4);
      sums[0]+=pop(c.policy);
      sums[1]+=pop(a.policy);
      sums[2]+=accuracy(c,target);
      sums[3]+=accuracy(a,target);
    }
    for(u32 i=0;i<4;++i)put(r.data()+SUM_BASE+4*i,sums[i],4);
  }
  inline Bytes produce_record(const Tier&t,u32 b,bool reverse=false){
    Rng rng(t,b);
    Bytes r(RECORD_BYTES,0);
    std::memcpy(r.data(),"BRS3R001",8);
    put(r.data()+8,1,2);
    r[10]=t.code;
    r[11]=b&1;
    put(r.data()+12,b,4);
    put(r.data()+16,256,2);
    put(r.data()+18,256,2);
    put(r.data()+20,RECORD_BYTES,4);
    auto targets=rng.targets();
    for(u32 i=0;i<512;++i)put(r.data()+32+4*i,targets[i],4);
    State s=rng.initial();
    encode(r.data()+STATE_BASE,s);
    for(u32 u=1;u<=256;++u){
      auto o=turnover(s,targets[u-1],0,rng.tape(u));
      s=o.post;
      encode(r.data()+STATE_BASE+264*u,s);
      put(r.data()+RETRY_BASE+4*(u-1),o.rejections,4);
    }
    followup(r,rng,s,reverse);
    rehash(r);
    return r;
  }
}
