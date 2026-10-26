// Own fixture-tier generator/forger ONLY. This is deliberately a different
// executable and cannot select benchmark/production keys, blocks or endpoints.
#include "data.hpp"
int main(int argc,char** argv){using namespace brc;
try{auto a=options(argc,argv,{"--inputs","--output","--threads","--order","--mode"});
unsigned nt=threads(a.at("--threads"));
ck(a.at("--order")=="CA"||a.at("--order")=="AC",2,"CLI","fixture arm order");
ck(a.at("--mode")=="forward"||a.at("--mode")=="reverse"||a.at("--mode")=="forged",2,"CLI","fixture mode");
pathcheck(a.at("--inputs"));
pathcheck(a.at("--output"),true);
science_inputs(a.at("--inputs"));
keytable(a.at("--inputs"));
Tier t=tier("fixture-r1");
RNG rng(t);
std::vector<Bytes> records(16);
parallel(16,nt,[&](unsigned b){records[b]=generate(t,rng,b,a.at("--order")=="AC",a.at("--mode")=="forged"&&b==0);},a.at("--mode")=="reverse");
 ck(::mkdir(a.at("--output").c_str(),0700)==0,5,"IO","new fixture directory");
fs::path dir=a.at("--output");
auto blob=chunkheader(t,0);
for(auto& r:records)append(blob,r);
publish(dir/chunkname(0),blob);
J m=manifest(t),entry;
entry.o["name"]=J::text(chunkname(0));
entry.o["bytes"]=J::number(blob.size());
entry.o["sha256"]=J::text(hex(sha(blob)));
m.o["files"].a.push_back(entry);
auto s=dump(m)+"\n";
publish(dir/"MANIFEST.json",s);
publish(dir/"MANIFEST.sha256",hex(sha(s))+"  MANIFEST.json\n");
return 0;
}catch(...){return failure();
}}
