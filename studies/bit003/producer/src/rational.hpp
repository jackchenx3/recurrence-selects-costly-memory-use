#pragma once
#include "json.hpp"
#include <boost/multiprecision/cpp_int.hpp>
namespace brs {
  using Big=boost::multiprecision::cpp_int;
  inline Big ab(Big a){
    return a<0?-a:a;
  }
  inline Big gcd(Big a,Big b){
    a=ab(a);
    b=ab(b);
    while(b!=0){
      Big r=a%b;
      a=b;
      b=r;
    }
    return a;
  }
  inline Big big(const std::string&s){
    need(canonical_int(s));
    Big b=0;
    for(size_t i=s[0]=='-'?1:0;i<s.size();++i)b=b*10+(s[i]-'0');
    return s[0]=='-'?-b:b;
  }
  struct Rat{
    Big p,q;
    Rat(Big n=0,Big d=1):p(n),q(d){
      need(q!=0);
      if(q<0){
        p=-p;
        q=-q;
      }
      Big g=gcd(p,q);
      p/=g;
      q/=g;
    }
    std::string str()const{
      return p.convert_to<std::string>()+"/"+q.convert_to<std::string>();
    }
  }
  ;
  inline Rat operator+(const Rat&a,const Rat&b){
    return{
      a.p*b.q+b.p*a.q,a.q*b.q
    }
    ;
  }
  inline Rat operator-(const Rat&a,const Rat&b){
    return{
      a.p*b.q-b.p*a.q,a.q*b.q
    }
    ;
  }
  inline Rat operator*(const Rat&a,const Rat&b){
    return{
      a.p*b.p,a.q*b.q
    }
    ;
  }
  inline Rat operator/(const Rat&a,const Rat&b){
    return{
      a.p*b.q,a.q*b.p
    }
    ;
  }
  inline bool operator<(const Rat&a,const Rat&b){
    return a.p*b.q<b.p*a.q;
  }
  inline bool operator>(const Rat&a,const Rat&b){
    return b<a;
  }
  inline bool operator<=(const Rat&a,const Rat&b){
    return !(b<a);
  }
  inline bool operator==(const Rat&a,const Rat&b){
    return a.p==b.p&&a.q==b.q;
  }
  inline Rat fraction(const std::string&s){
    auto v=split(s,'/');
    need(v.size()==2);
    Rat r(big(v[0]),big(v[1]));
    need(big(v[1])>0&&r.str()==s);
    return r;
  }
  inline std::string classify(const Rat&lo,const Rat&hi){
    if(lo>Rat(1,32))return "MATERIAL_POSITIVE";
    if(hi<Rat(-1,32))return "MATERIAL_NEGATIVE";
    if(Rat(-1,32)<=lo&&hi<=Rat(1,32))return "BOUNDED_WITHIN_MARGIN";
    if(lo>Rat())return "POSITIVE_MAGNITUDE_UNRESOLVED";
    if(hi<Rat())return "NEGATIVE_MAGNITUDE_UNRESOLVED";
    return "UNRESOLVED";
  }
  inline std::string outward(const Rat&r,bool up){
    Big n=r.p*Big("1000000000000"),v=n/r.q,rem=n%r.q;
    if(rem!=0){
      if(up&&n>0)++v;
      if(!up&&n<0)--v;
    }
    bool neg=v<0;
    v=ab(v);
    Big whole=v/Big("1000000000000"),frac=v%Big("1000000000000");
    auto s=frac.convert_to<std::string>();
    return std::string(neg?"-":"")+whole.convert_to<std::string>()+"."+std::string(12-s.size(),'0')+s;
  }
  inline J interval(i64 sum,u64 den,u64 h){
    Rat e(sum,den),half(h,Big("1000000000000"));
    Rat lo=std::max(Rat(-1),e-half),hi=std::min(Rat(1),e+half);
    return J::obj({
      {
        "estimate",e.str()
      }
      ,{
        "lower",lo.str()
      }
      ,{
        "upper",hi.str()
      }
      ,{
        "lower_decimal",outward(lo,false)
      }
      ,{
        "upper_decimal",outward(hi,true)
      }
      ,{
        "classification",classify(lo,hi)
      }
    }
    );
  }
  inline std::pair<Rat,Rat> log_bounds(Rat z){
    Rat sum,power=z,z2=z*z;
    for(u32 k=0;k<48;++k){
      sum=sum+Rat(2)*power/Rat(2*k+1);
      power=power*z2;
    }
    return{
      sum,sum+Rat(2)*power/(Rat(97)*(Rat(1)-z2))
    }
    ;
  }
  inline J certificate(const std::string&bytes){
    auto j=parse(bytes);
    auto l2=log_bounds(Rat(1,3)),l25=log_bounds(Rat(9,41));
    auto check=[&](const J&x,Rat r){
      need(x.str()==r.str(),3,"MISMATCH","certificate");
    }
    ;
    check(j.at("series_bounds").at("ln2").a.at(0),l2.first);
    check(j.at("series_bounds").at("ln2").a.at(1),l2.second);
    check(j.at("series_bounds").at("ln25_over16").a.at(0),l25.first);
    check(j.at("series_bounds").at("ln25_over16").a.at(1),l25.second);
    need(j.at("schema").num()==1&&j.at("n_blocks").num()==20000&&j.at("n_terms").num()==48&&j.at("range_length").num()==2,3,"MISMATCH","certificate");
    std::array<std::string,2> ceilings{
    }
    ;
    for(u32 a=0;a<2;++a){
      auto&v=j.at("endpoints").at(a?"accuracy":"policy");
      Rat alpha=a?Rat(1,100):Rat(1,25),grid(a?23018074131ULL:19778834661ULL,Big("1000000000000")),prev=grid-Rat(1,Big("1000000000000"));
      Rat lo=Rat(a?7:5)*l2.first+l25.first,hi=Rat(a?7:5)*l2.second+l25.second;
      check(v.at("alpha"),alpha);
      check(v.at("h_grid"),grid);
      check(v.at("previous_grid"),prev);
      check(v.at("ln_lower"),lo);
      check(v.at("ln_upper"),hi);
      Rat d1=grid*grid-hi/Rat(10000),d2=lo/Rat(10000)-prev*prev;
      check(v.at("grid_squared_minus_upper"),d1);
      check(v.at("lower_minus_previous_squared"),d2);
      need(d1>Rat()&&d2>Rat()&&v.at("log_argument").num()==u64(a?200:50),3,"MISMATCH","certificate");
      ceilings[a]=grid.str();
    }
    check(j.at("family_alpha"),Rat(1,25)+Rat(1,100));
    return J::obj({
      {
        "certificate_sha256",sha(bytes)
      }
      ,{
        "family_alpha","1/20"
      }
      ,{
        "policy_grid_ceiling",ceilings[0]
      }
      ,{
        "accuracy_grid_ceiling",ceilings[1]
      }
      ,{
        "both_exact_inequalities",true
      }
    }
    );
  }
}
