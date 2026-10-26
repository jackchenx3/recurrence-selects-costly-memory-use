#pragma once
#include "base.hpp"
namespace brc {
// Small duplicate-aware integer JSON reader. Objects are sorted maps; numbers
// retain decimal text to distinguish strings, booleans and arbitrary integers.
struct J {
 enum Kind {OBJ,ARR,STR,NUM,BOOL} kind=OBJ;
 std::map<std::string,J> o;
 std::vector<J> a;
 std::string s;
 static J text(std::string x){J j;
j.kind=STR;
j.s=std::move(x);
return j;
}
 static J number(uint64_t x){J j;
j.kind=NUM;
j.s=std::to_string(x);
return j;
}
 const J& at(const std::string& k)const{require(kind==OBJ&&o.count(k),"JSON property");
return o.at(k);
}
 uint64_t u(uint64_t bound=UINT64_MAX)const{require(kind==NUM,"JSON integer");
return decimal(s,bound);
}
 std::string text()const{require(kind==STR,"JSON string");
return s;
}
};
inline std::string quote(const std::string& s){std::string out="\"";
for(unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';
out+=char(c);
}else if(c<32||c>=127)out+="\\u00"+hx(c,2);
else out+=char(c);
}return out+'"';
}
inline std::string dump(const J& j){
 if(j.kind==J::STR)return quote(j.s);
if(j.kind==J::NUM||j.kind==J::BOOL)return j.s;
 std::string out=j.kind==J::OBJ?"{":"[";
bool first=true;
 if(j.kind==J::OBJ)for(const auto& p:j.o){if(!first)out+=',';
first=false;
out+=quote(p.first)+":"+dump(p.second);
}
 else for(const auto& x:j.a){if(!first)out+=',';
first=false;
out+=dump(x);
}return out+(j.kind==J::OBJ?"}":"]");
}
class JP {
 const std::string& s;
size_t i=0;
unsigned depth=0;
 void ws(){while(i<s.size()&&(s[i]==' '||s[i]=='\t'||s[i]=='\r'||s[i]=='\n'))++i;
}
 char take(){require(i<s.size(),"JSON EOF");
return s[i++];
}
 std::string string(){require(take()=='"');
std::string x;
while(true){char c=take();
if(c=='"')break;
require(uint8_t(c)>=32&&uint8_t(c)<128,"JSON ASCII");
if(c=='\\'){c=take();
if(c=='u'){require(take()=='0'&&take()=='0');
char h0=take(),h1=take();
auto h=std::string(1,h0)+h1;
auto b=unhex(h);
require(b[0]<128);
x+=char(b[0]);
continue;
}switch(c){case '"':case '\\':case '/':break;
case 'b':c='\b';
break;
case 'f':c='\f';
break;
case 'n':c='\n';
break;
case 'r':c='\r';
break;
case 't':c='\t';
break;
default:throw Error("JSON escape");
}}x+=c;
}return x;
}
 J value(){ws();
require(++depth<=64,"JSON depth");
require(i<s.size());
J j;
char c=s[i];
 if(c=='{'){++i;
ws();
if(i<s.size()&&s[i]=='}')++i;
else while(true){ws();
auto k=string();
ws();
require(take()==':');
J v=value();
require(j.o.emplace(k,std::move(v)).second,"JSON duplicate");
ws();
char q=take();
if(q=='}')break;
require(q==',');
}}
 else if(c=='['){j.kind=J::ARR;
++i;
ws();
if(i<s.size()&&s[i]==']')++i;
else while(true){j.a.push_back(value());
ws();
char q=take();
if(q==']')break;
require(q==',');
}}
 else if(c=='"'){j.kind=J::STR;
j.s=string();
}
 else if(c=='t'||c=='f'){j.kind=J::BOOL;
j.s=c=='t'?"true":"false";
require(s.compare(i,j.s.size(),j.s)==0);
i+=j.s.size();
}
 else {j.kind=J::NUM;
size_t st=i;
if(c=='-')++i;
require(i<s.size()&&s[i]>='0'&&s[i]<='9');
if(s[i]=='0')++i;
else while(i<s.size()&&s[i]>='0'&&s[i]<='9')++i;
j.s=s.substr(st,i-st);
require(j.s!="-0");
}
 --depth;
return j;
 }
public:explicit JP(const std::string& x):s(x){}J parse(){J j=value();
ws();
require(i==s.size(),"JSON trailing");
return j;
}
};
inline J parse(const Bytes& b,bool canonical=true){std::string s=str(b);
J j=JP(s).parse();
if(canonical)require(dump(j)+"\n"==s,"JSON canonical");
return j;
}
inline void keys(const J& j,std::initializer_list<const char*> ks){require(j.kind==J::OBJ&&j.o.size()==ks.size(),"JSON keys");
for(auto k:ks)require(j.o.count(k),"JSON keys");
}
} // namespace brc
