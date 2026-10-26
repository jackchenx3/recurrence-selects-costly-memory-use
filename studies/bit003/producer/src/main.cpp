#include "fixture.hpp"
#include "build_metadata.hpp"
int main(int argc,char**argv){
  using namespace brs;
  try{
    std::vector<std::string>a;
    std::string command=BRS_COMMAND;
    a.push_back(command);
    for(int i=1;i<argc;++i)a.push_back(argv[i]);
    auto opt=drycli(a);
    for(auto&v:opt)if(v.first!="--tier"&&v.first!="--threads"&&v.first!="--case")pathcheck(v.second,v.first=="--output"||v.first=="--results");
    Inputs in(opt.at("--inputs"),command=="brs3_fixture");
    if(command=="brs3_fixture"){
      fs::path p=opt.at("--case");
      auto rel=p.lexically_relative(in.root).generic_string();
      need(in.cases.count(rel),2,"INPUT");
      J c;
      try{
        c=parse(in.txt(rel));
      }
      catch(...){
        fail(2,"FORMAT");
      }
      auto out=fixture(c,in);
      atomictext(opt.at("--output"),json(out.first));
      if(out.second)std::cerr<<"BRS3_ERROR "<<out.first.at("code").str()<<"\n";
      return out.second;
    }
    auto t=tier(opt.at("--tier"));
    u32 threads=decimal(opt.at("--threads"),32);
    if(command=="brs3_produce")produce(opt.at("--output"),t,threads);
    else if(command=="brs3_analyze"){
      auto s=analyze(opt.at("--data"),t,threads);
      atomictext(opt.at("--results"),s);
    }
    else fail(5,"INTERNAL");
    return 0;
  }
  catch(const brs::Error&e){
    std::cerr<<"BRS3_ERROR "<<e.code<<"\n";
    return e.exit;
  }
  catch(const std::bad_alloc&){
    std::cerr<<"BRS3_ERROR RESOURCE\n";
    return 5;
  }
  catch(...){
    std::cerr<<"BRS3_ERROR INTERNAL\n";
    return 5;
  }
}
