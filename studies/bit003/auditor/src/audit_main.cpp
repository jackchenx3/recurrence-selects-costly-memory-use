#include "data.hpp"
#include "build_metadata.hpp"
int main(int argc,char** argv){using namespace brc;
try{auto a=options(argc,argv,{"--inputs","--tier","--data","--results","--report","--threads"});
auto t=tier(a.at("--tier"));
unsigned nt=threads(a.at("--threads"));
for(auto k:{"--inputs","--data"})pathcheck(a.at(k));
for(auto k:{"--results","--report"})pathcheck(a.at(k),true);
ck(a.at("--results")!=a.at("--report"),2,"CLI","distinct output paths");
ck(fs::path(a.at("--results")).parent_path()!=fs::path(a.at("--data"))&&fs::path(a.at("--report")).parent_path()!=fs::path(a.at("--data")),2,"CLI","outputs outside DATA");
ck(fs::path(a.at("--results")).parent_path()!=fs::path(a.at("--inputs"))&&fs::path(a.at("--report")).parent_path()!=fs::path(a.at("--inputs")),2,"CLI","outputs outside inputs");
science_inputs(a.at("--inputs"));
keytable(a.at("--inputs"));
ck(ishex(BRS3_SOURCE_SHA256,64),5,"INTERNAL","frozen source metadata");
auto binary=hex(filehash("/proc/self/exe"));
auto sums=audit_data(a.at("--data"),t,nt);
std::string report="BIT003_AUDIT_V1\nstatus=VALID\nscope=FULL_INDEPENDENT_REPLAY\ntier="+t.name+"\nblocks_checked="+std::to_string(t.n)+"\nstates_checked="+std::to_string(uint64_t(t.n)*769)+"\nturnovers_checked="+std::to_string(uint64_t(t.n)*768)+"\nprotocol_sha256="+PROTOCOL+"\ncontract_sha256="+CONTRACT+"\npublic_input_index_sha256="+CORE+"\nbundle_inventory_sha256="+INVENTORY+"\nsource_sha256="+BRS3_SOURCE_SHA256+"\nbinary_sha256="+binary+"\nfirst_mismatch=NONE\n";
 // Both complete artifacts are prepared before either final name is committed.
 // Coordinator must capture actual exit0 as well as both final files.
 publish(a.at("--report"),report);
try{publish(a.at("--results"),results(t,sums.m,sums.a));
}catch(...){::unlink(a.at("--report").c_str());
throw;
}return 0;
}catch(...){return failure();
}}
