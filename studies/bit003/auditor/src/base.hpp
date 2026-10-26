#pragma once
// Independently authored from the three public contracts; no producer components.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace brc {
using Bytes=std::vector<uint8_t>;
using Digest=std::array<uint8_t,32>;
namespace fs=std::filesystem;
constexpr const char* STUDY="PHASE3-BIT-SUSTAINED-RETRIEVAL-003";
constexpr const char* PROTOCOL="30754cf9de941f09b8086e8ffb27a2b4af42882c6bfcca484928efa926ae6b98";
struct Error:std::runtime_error { using std::runtime_error::runtime_error; };
inline void require(bool b,const std::string& s="invalid input") { if(!b) throw Error(s); }
inline void require(bool b,const char* s) { if(!b) throw Error(s); }
inline uint64_t get(const Bytes& b,size_t off,unsigned n) {
 require(n<=8 && off<=b.size() && n<=b.size()-off,"byte range");
 uint64_t v=0; for(unsigned i=0;i<n;++i) v|=uint64_t(b[off+i])<<(8*i); return v;
}
inline void put(Bytes& b,size_t off,uint64_t v,unsigned n) {
 require(n<=8 && off<=b.size() && n<=b.size()-off,"byte range");
 for(unsigned i=0;i<n;++i) b[off+i]=uint8_t(v>>(8*i));
}
inline void append(Bytes& b,uint64_t v,unsigned n) { size_t p=b.size(); b.resize(p+n); put(b,p,v,n); }
inline void append(Bytes& b,const Bytes& v) { b.insert(b.end(),v.begin(),v.end()); }
inline void append(Bytes& b,const Digest& v) { b.insert(b.end(),v.begin(),v.end()); }
inline Bytes bytes(const std::string& s) { return Bytes(s.begin(),s.end()); }
inline std::string str(const Bytes& b) { return std::string(b.begin(),b.end()); }
inline Bytes slice(const Bytes& b,size_t p,size_t n) { require(p<=b.size()&&n<=b.size()-p); return Bytes(b.begin()+p,b.begin()+p+n); }
inline bool eq(const Bytes& b,size_t p,const Digest& h) { return p<=b.size()&&32<=b.size()-p&&std::equal(h.begin(),h.end(),b.begin()+p); }
inline std::string hex(const uint8_t* p,size_t n) {
 static const char* h="0123456789abcdef"; std::string s; s.reserve(n*2);
 for(size_t i=0;i<n;++i) {s+=h[p[i]>>4];s+=h[p[i]&15];} return s;
}
inline std::string hex(const Bytes& b) { return hex(b.data(),b.size()); }
inline std::string hex(const Digest& b) { return hex(b.data(),b.size()); }
inline bool ishex(const std::string& s,size_t n) {return s.size()==n&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
inline Bytes unhex(const std::string& s) {
 require(s.size()%2==0&&ishex(s,s.size()),"hex syntax"); Bytes b;
 auto v=[](char c){return c<='9'?c-'0':c-'a'+10;};
 for(size_t i=0;i<s.size();i+=2) b.push_back(uint8_t(16*v(s[i])+v(s[i+1]))); return b;
}
inline std::string hx(uint64_t v,unsigned n) {std::ostringstream o;o<<std::hex<<std::setfill('0')<<std::setw(n)<<v;return o.str();}
// SHA-256: big-endian message schedule, 64 rounds, 64-bit bit length.
class SHA256 {
 std::array<uint32_t,8> h{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
 std::array<uint8_t,64> buf{}; size_t used=0; uint64_t count=0;
 static uint32_t rr(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
 void block(const uint8_t* p) {
  static constexpr uint32_t k[64]={
   0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
   0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
   0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
   0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
   0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
   0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
   0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
   0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  uint32_t w[64]; for(unsigned i=0;i<16;++i) w[i]=(uint32_t(p[4*i])<<24)|(uint32_t(p[4*i+1])<<16)|(uint32_t(p[4*i+2])<<8)|p[4*i+3];
  for(unsigned i=16;i<64;++i) {uint32_t a=w[i-15],b=w[i-2];w[i]=w[i-16]+(rr(a,7)^rr(a,18)^(a>>3))+w[i-7]+(rr(b,17)^rr(b,19)^(b>>10));}
  auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
  for(unsigned i=0;i<64;++i) {uint32_t t=z+(rr(e,6)^rr(e,11)^rr(e,25))+((e&f)^(~e&g))+k[i]+w[i];uint32_t v=(rr(a,2)^rr(a,13)^rr(a,22))+((a&b)^(a&c)^(b&c));z=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+v;}
  h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
 }
public:
 void update(const uint8_t* p,size_t n) {
  require(n<=(std::numeric_limits<uint64_t>::max()/8)-count,"hash length");count+=n;
  while(n){size_t v=std::min(n,64-used);std::memcpy(buf.data()+used,p,v);used+=v;p+=v;n-=v;if(used==64){block(buf.data());used=0;}}
 }
 void update(const Bytes& b){update(b.data(),b.size());}
 Digest finish() {
  uint64_t bits=count*8;buf[used++]=0x80;
  if(used>56){std::fill(buf.begin()+used,buf.end(),0);block(buf.data());used=0;}
  std::fill(buf.begin()+used,buf.begin()+56,0);
  for(unsigned i=0;i<8;++i)buf[63-i]=uint8_t(bits>>(8*i));block(buf.data());Digest out{};
  for(unsigned i=0;i<8;++i)for(unsigned j=0;j<4;++j)out[4*i+j]=uint8_t(h[i]>>(24-8*j));return out;
 }
};
inline Digest sha(const Bytes& b){SHA256 s;s.update(b);return s.finish();}
inline Digest sha(const std::string& s){return sha(bytes(s));}
inline uint64_t decimal(const std::string& s,uint64_t max=std::numeric_limits<uint64_t>::max()) {
 require(!s.empty()&&(s=="0"||s[0]!='0'),"decimal syntax");uint64_t v=0;
 for(char c:s){require(c>='0'&&c<='9',"decimal syntax");unsigned d=c-'0';require(v<=max/10&&(v<max/10||d<=max%10),"decimal range");v=v*10+d;}return v;
}
inline int64_t integer(const std::string& s,int64_t bound) {
 bool neg=!s.empty()&&s[0]=='-';std::string a=neg?s.substr(1):s;require(!(neg&&a=="0"),"integer syntax");uint64_t v=decimal(a,uint64_t(bound));return neg?-int64_t(v):int64_t(v);
}
inline std::vector<std::string> split(const std::string& s,char sep) {
 std::vector<std::string> v;size_t p=0;
 for(;;){size_t n=s.find(sep,p);v.push_back(s.substr(p,n==std::string::npos?n:n-p));if(n==std::string::npos)break;p=n+1;}return v;
}
inline std::vector<std::string> lines(const Bytes& b) {
 require(!b.empty()&&b.back()==10,"final LF");
 for(uint8_t c:b)require(c==10||(c>=32&&c<=126),"ASCII");
 auto l=split(str(b),'\n');l.pop_back();for(auto& s:l)require(!s.empty()&&s.back()!=' ',"blank/trailing space");return l;
}
inline Bytes readfile(const fs::path& p,uint64_t cap=1073741824) {
 int fd=::open(p.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)throw Error("file unreadable");
 struct stat st{};if(::fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<0||uint64_t(st.st_size)>cap){::close(fd);throw Error("file type/size");}
 Bytes b(size_t(st.st_size),0);size_t p0=0;
 while(p0<b.size()){ssize_t n=::read(fd,b.data()+p0,b.size()-p0);if(n<0&&errno==EINTR)continue;if(n<=0){::close(fd);throw Error("file read");}p0+=size_t(n);}
 uint8_t extra;ssize_t n=::read(fd,&extra,1);int rc=::close(fd);require(n==0&&rc==0,"file changed/read");return b;
}
inline unsigned pop(uint32_t x){return unsigned(__builtin_popcount(x));}
} // namespace brc
