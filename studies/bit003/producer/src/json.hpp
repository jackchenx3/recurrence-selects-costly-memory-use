#pragma once
#include "base.hpp"
namespace brs {
  struct J {
    enum Type{
      N,S,B,A,O
    }
    type=O;
    std::string s;
    bool b=false;
    std::vector<J>a;
    std::map<std::string,J>o;
    J()=default;
    J(const char*x):type(S),s(x){
    }
    J(std::string x):type(S),s(std::move(x)){
    }
    J(bool x):type(B),b(x){
    }
    J(int x):type(N),s(std::to_string(x)){
    }
    J(unsigned x):type(N),s(std::to_string(x)){
    }
    J(i64 x):type(N),s(std::to_string(x)){
    }
    J(u64 x):type(N),s(std::to_string(x)){
    }
    static J arr(){
      J v;
      v.type=A;
      return v;
    }
    static J obj(std::initializer_list<std::pair<const std::string,J>>v){
      J j;
      j.o=v;
      return j;
    }
    J&operator[](const std::string&k){
      need(type==O);
      return o[k];
    }
    const J&at(const std::string&k)const{
      need(type==O&&o.count(k));
      return o.at(k);
    }
    std::string str()const{
      need(type==S);
      return s;
    }
    u64 num(u64 max=UINT64_MAX)const{
      need(type==N);
      return decimal(s,max);
    }
    i64 sint()const{
      need(type==N&&canonical_int(s));
      bool neg=s[0]=='-';
      u64 v=decimal(neg?s.substr(1):s,u64(INT64_MAX)+(neg?1:0));
      if(neg&&v==u64(INT64_MAX)+1)return INT64_MIN;
      return neg?-i64(v):i64(v);
    }
    bool boolean()const{
      need(type==B);
      return b;
    }
    void push(J v){
      need(type==A);
      a.push_back(std::move(v));
    }
  }
  ;
  inline void keys(const J&j,std::initializer_list<const char*>ks){
    need(j.type==J::O&&j.o.size()==ks.size());
    for(auto k:ks)need(j.o.count(k));
  }
  inline std::string quote(const std::string&s){
    std::string r="\"";
    for(size_t i=0;i<s.size();++i){
      unsigned c=(unsigned char)s[i];
      if(c=='"'||c=='\\'){
        r+='\\';
        r+=char(c);
      }
      else if(c==8)r+="\\b";
      else if(c==9)r+="\\t";
      else if(c==10)r+="\\n";
      else if(c==12)r+="\\f";
      else if(c==13)r+="\\r";
      else if(c<32)r+="\\u"+hx(c,4);
      else if(c<128)r+=char(c);
      else{
        unsigned v=c&((c<224)?31:(c<240)?15:7),n=c<224?1:c<240?2:3;
        need(i+n<s.size());
        for(unsigned z=0;z<n;++z){
          unsigned t=(unsigned char)s[++i];
          need((t&192)==128);
          v=(v<<6)|(t&63);
        }
        if(v<=65535)r+="\\u"+hx(v,4);
        else{
          v-=65536;
          r+="\\u"+hx(0xd800+(v>>10),4)+"\\u"+hx(0xdc00+(v&1023),4);
        }
      }
    }
    return r+'"';
  }
  inline std::string dump(const J&j){
    if(j.type==J::N)return j.s;
    if(j.type==J::S)return quote(j.s);
    if(j.type==J::B)return j.b?"true":"false";
    std::string r=j.type==J::A?"[":"{";
    bool first=true;
    if(j.type==J::A){
      for(auto&v:j.a){
        if(!first)r+=',';
        first=false;
        r+=dump(v);
      }
    }
    else for(auto&v:j.o){
      if(!first)r+=',';
      first=false;
      r+=quote(v.first)+":"+dump(v.second);
    }
    return r+(j.type==J::A?"]":"}");
  }
  inline std::string json(const J&j){
    return dump(j)+"\n";
  }
  struct Parser {
    const std::string&s;
    size_t p=0;
    unsigned depth=0;
    void ws(){
      while(p<s.size()&&(s[p]==' '||s[p]=='\n'||s[p]=='\r'||s[p]=='\t'))++p;
    }
    char take(){
      need(p<s.size());
      return s[p++];
    }
    void utf(std::string&r,unsigned v){
      if(v<128)r+=char(v);
      else if(v<2048){
        r+=char(192|(v>>6));
        r+=char(128|(v&63));
      }
      else if(v<65536){
        r+=char(224|(v>>12));
        r+=char(128|((v>>6)&63));
        r+=char(128|(v&63));
      }
      else{
        r+=char(240|(v>>18));
        r+=char(128|((v>>12)&63));
        r+=char(128|((v>>6)&63));
        r+=char(128|(v&63));
      }
    }
    unsigned four(){
      unsigned v=0;
      for(int n=0;n<4;++n){
        char c=take();
        need((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'));
        v=16*v+(c<='9'?c-'0':(c|32)-'a'+10);
      }
      return v;
    }
    std::string string(){
      need(take()=='"');
      std::string r;
      for(;;){
        unsigned char c=take();
        if(c=='"')return r;
        need(c>=32&&c<128);
        if(c!='\\'){
          r+=char(c);
          continue;
        }
        char e=take();
        if(e=='"'||e=='\\'||e=='/')r+=e;
        else if(e=='b')r+=char(8);
        else if(e=='f')r+=char(12);
        else if(e=='n')r+='\n';
        else if(e=='r')r+='\r';
        else if(e=='t')r+='\t';
        else{
          need(e=='u');
          unsigned v=four();
          if(v>=0xd800&&v<=0xdbff){
            need(take()=='\\'&&take()=='u');
            unsigned w=four();
            need(w>=0xdc00&&w<=0xdfff);
            v=65536+((v-0xd800)<<10)+w-0xdc00;
          }
          else need(v<0xdc00||v>0xdfff);
          utf(r,v);
        }
      }
    }
    J value(){
      ws();
      need(++depth<=64);
      need(p<s.size());
      J j;
      char c=s[p];
      if(c=='{'){
        ++p;
        ws();
        if(p<s.size()&&s[p]=='}')++p;
        else for(;;){
          ws();
          auto k=string();
          ws();
          need(take()==':');
          auto v=value();
          need(!j.o.count(k));
          j.o.emplace(k,std::move(v));
          ws();
          char d=take();
          if(d=='}')break;
          need(d==',');
        }
      }
      else if(c=='['){
        ++p;
        j=J::arr();
        ws();
        if(p<s.size()&&s[p]==']')++p;
        else for(;;){
          j.push(value());
          ws();
          char d=take();
          if(d==']')break;
          need(d==',');
        }
      }
      else if(c=='"')j=J(string());
      else if(s.compare(p,4,"true")==0){
        j=J(true);
        p+=4;
      }
      else if(s.compare(p,5,"false")==0){
        j=J(false);
        p+=5;
      }
      else{
        size_t begin=p;
        if(s[p]=='-')++p;
        while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;
        j.type=J::N;
        j.s=s.substr(begin,p-begin);
        need(canonical_int(j.s));
      }
      --depth;
      return j;
    }
  }
  ;
  inline J parse(const std::string&s,bool canonical=true){
    Parser p{
      s
    }
    ;
    auto j=p.value();
    p.ws();
    need(p.p==s.size());
    if(canonical)need(json(j)==s);
    return j;
  }
  inline J jhex(const std::array<u32,32>&v){
    J a=J::arr();
    for(auto x:v)a.push(hx(x,8));
    return a;
  }
}
