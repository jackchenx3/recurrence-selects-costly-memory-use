#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <exception>
#include <utility>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <condition_variable>
#include "vendor/sha256.hpp"
#include "vendor/philox.hpp"
namespace brs {
  using u8=uint8_t;
  using u16=uint16_t;
  using u32=uint32_t;
  using u64=uint64_t;
  using i64=int64_t;
  using Bytes=std::vector<u8>;
  namespace fs=std::filesystem;
  constexpr char STUDY[]="PHASE3-BIT-SUSTAINED-RETRIEVAL-003";
  constexpr char CORE_SHA[]="e60b367dbab81dc1388f06f9d7be6da90b927e8302b4d7001be544976abe8889";
  constexpr char INVENTORY_SHA[]="4cc9b10958a04c458aeba5ab3788dd54153b1ce18fc2cb83fe3847e012599bd6";
  constexpr char PROTOCOL_SHA[]="30754cf9de941f09b8086e8ffb27a2b4af42882c6bfcca484928efa926ae6b98";
  constexpr char CONTRACT_SHA[]="ec34610b22fdbd1e0a31523995e9ffaa201b06840fb2da4b17f4fc44386e9913";
  constexpr char KEYS_SHA[]="636c558fdd866cd3d4f60eeee30915d7dce1d2001fbcbd1c5773acb56bcbe522";
  constexpr u32 RECORD_BYTES=208216,RECORD_HASH=208184,STATE_BASE=2080,RETRY_BASE=205096,SUM_BASE=208168;
  struct Error:std::runtime_error {
    int exit;
    std::string code,field,arm;
    int index=-1,draw=-1,retry=-1,u=-1;
    Error(int e,std::string c,std::string f=""):runtime_error(c),exit(e),code(std::move(c)),field(std::move(f)){
    }
  }
  ;
  [[noreturn]] inline void fail(int e,const std::string&c,const std::string&f=""){
    throw Error(e,c,f);
  }
  inline void need(bool b,int e=2,const std::string&c="FORMAT",const std::string&f="payload"){
    if(!b)fail(e,c,f);
  }
  inline u32 pop(u32 v){
    return __builtin_popcount(v);
  }
  inline u64 get(const u8*p,unsigned n){
    u64 v=0;
    for(unsigned i=0;i<n;++i)v|=u64(p[i])<<(8*i);
    return v;
  }
  inline void put(u8*p,u64 v,unsigned n){
    for(unsigned i=0;i<n;++i)p[i]=u8(v>>(8*i));
  }
  inline std::string sha(const void*p,size_t n){
    return mm::to_hex(mm::sha256_bytes(p,n));
  }
  inline std::string sha(const Bytes&b){
    return sha(b.data(),b.size());
  }
  inline std::string sha(const std::string&s){
    return sha(s.data(),s.size());
  }
  inline std::string hx(u64 v,unsigned n){
    std::ostringstream s;
    s<<std::hex<<std::setfill('0')<<std::setw(n)<<v;
    return s.str();
  }
  inline bool ishex(const std::string&s,size_t n){
    return s.size()==n&&s.find_first_not_of("0123456789abcdef")==std::string::npos;
  }
  inline Bytes unhex(const std::string&s){
    need(s.size()%2==0&&ishex(s,s.size()));
    Bytes b(s.size()/2);
    for(size_t i=0;i<b.size();++i)b[i]=u8(std::stoul(s.substr(2*i,2),nullptr,16));
    return b;
  }
  inline std::string hex(const Bytes&b){
    return mm::to_hex(b.data(),b.size());
  }
  inline std::vector<std::string> split(const std::string&s,char c){
    std::vector<std::string>v;
    size_t p=0;
    for(;;){
      auto q=s.find(c,p);
      v.push_back(s.substr(p,q==s.npos?q:q-p));
      if(q==s.npos)return v;
      p=q+1;
    }
  }
  inline bool canonical_int(const std::string&s){
    size_t p=(!s.empty()&&s[0]=='-')?1:0;
    if(p==s.size()||s=="-0")return false;
    if(s[p]=='0'&&p+1!=s.size())return false;
    for(;p<s.size();++p)if(s[p]<'0'||s[p]>'9')return false;
    return true;
  }
  inline u64 decimal(const std::string&s,u64 max=UINT64_MAX){
    need(canonical_int(s)&&s[0]!='-');
    u64 v=0;
    for(char c:s){
      u64 d=c-'0';
      need(v<=max/10&&(v<max/10||d<=max%10));
      v=v*10+d;
    }
    return v;
  }
  struct Tier{
    std::string name;
    u32 code,n;
    u32 chunks()const{
      return(n+99)/100;
    }
    u32 count(u32 k)const{
      return std::min(100u,n-100*k);
    }
  }
  ;
  inline Tier tier(const std::string&s){
    if(s=="fixture-r1")return{
      s,0,16
    }
    ;
    if(s=="benchmark-r1")return{
      s,1,64
    }
    ;
    if(s=="production-r1")return{
      s,2,20000
    }
    ;
    fail(2,"FORMAT","tier");
  }
  inline std::string chunkname(u32 k){
    return "chunk_"+std::string(k<10?"00":k<100?"0":"")+std::to_string(k)+".bin";
  }
}
