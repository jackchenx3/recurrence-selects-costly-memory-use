#pragma once
#include "dataset.hpp"
#include "rational.hpp"
namespace brs {
  inline u32 h32(const J&v){
    auto s=v.str();
    need(ishex(s,8));
    return u32(std::stoul(s,nullptr,16));
  }
  inline u64 h64(const J&v){
    auto s=v.str();
    need(ishex(s,16));
    return std::stoull(s,nullptr,16);
  }
  inline State state_json(const J&j){
    try{
      keys(j,{
        "x","c","policy_mask","valid_mask"
      }
      );
      need(j.at("x").type==J::A&&j.at("c").type==J::A&&j.at("x").a.size()==32&&j.at("c").a.size()==32);
      State s;
      for(u32 i=0;i<32;++i){
        s.x[i]=h32(j.at("x").a[i]);
        s.c[i]=h32(j.at("c").a[i]);
      }
      s.policy=h32(j.at("policy_mask"));
      s.valid=h32(j.at("valid_mask"));
      need(legal(s),2,"STATE","state");
      return s;
    }
    catch(Error&e){
      e.field="state";
      throw;
    }
  }
  inline J state_value(const State&s){
    return J::obj({
      {
        "x",jhex(s.x)
      }
      ,{
        "c",jhex(s.c)
      }
      ,{
        "policy_mask",hx(s.policy,8)
      }
      ,{
        "valid_mask",hx(s.valid,8)
      }
    }
    );
  }
  inline Tape injected(const J&j){
    keys(j,{
      "fresh","scout","local","donor","survival","policy_flip"
    }
    );
    Tape d;
    auto array=[&](const char*n,size_t len){
      auto&v=j.at(n);
      need(v.type==J::A&&v.a.size()==len);
    }
    ;
    array("fresh",32);
    array("scout",32);
    array("local",32);
    array("donor",96);
    array("survival",32);
    for(u32 i=0;i<32;++i){
      d.fresh[i]=h32(j.at("fresh").a[i]);
      d.scout[i]=h32(j.at("scout").a[i]);
      d.local[i]=h32(j.at("local").a[i]);
    }
    for(u32 i=0;i<96;++i)d.donor[i]=h64(j.at("donor").a[i]);
    d.flip=h32(j.at("policy_flip"));
    std::array<std::vector<u64>,32>v;
    for(u32 i=0;i<32;++i){
      auto&a=j.at("survival").a[i];
      need(a.type==J::A&&!a.a.empty()&&a.a.size()<=256);
      for(auto&w:a.a)v[i].push_back(h64(w));
    }
    d.survival=[v=std::move(v)](u32 draw,u32 retry){
      if(retry>=v[draw].size()){
        Error e(3,"TAPE_EXHAUSTED");
        e.retry=retry;
        throw e;
      }
      return v[draw][retry];
    }
    ;
    return d;
  }
  inline J turn_value(const Turn&t,u32 target,u32 q){
    J probe=J::arr(),local=J::arr(),donor=J::arr(),selected=J::arr(),retry=J::arr();
    for(u32 i=0;i<32;++i){
      probe.push(hx(t.g[32+i],8));
      local.push(hx(t.g[96+i],8));
      donor.push(t.donor[i]);
      selected.push(t.selected[i]);
      retry.push(t.retries[i]);
    }
    return J::obj({
      {
        "switched_mask",hx(q,8)
      }
      ,{
        "probe",probe
      }
      ,{
        "donor_family",donor
      }
      ,{
        "local_child",local
      }
      ,{
        "selected_ids",selected
      }
      ,{
        "draw_rejections",retry
      }
      ,{
        "state",state_value(t.post)
      }
      ,{
        "queries",128
      }
      ,{
        "M",pop(t.post.policy)
      }
      ,{
        "A",accuracy(t.post,target)
      }
    }
    );
  }
  inline void scheduling_fields(const J&p){
    auto&a=p.at("arm_order");
    need(a.type==J::A&&a.a.size()==2);
    auto x=a.a[0].str(),y=a.a[1].str();
    need((x=="C"&&y=="A")||(x=="A"&&y=="C"));
    need(p.at("threads").num(32)>=1);
  }
  inline J pair_step(State&c,State&a,u32 target,const Tape&d,bool reverse){
    Turn tc,ta;
    std::exception_ptr ec,ea;
    u32 q=a.policy&a.valid;
    auto C=[&]{
      try{
        tc=turnover(c,target,0,d);
      }
      catch(Error&e){
        e.arm="C";
        ec=std::current_exception();
      }
    }
    ;
    auto A=[&]{
      try{
        ta=turnover(a,target,q,d);
      }
      catch(Error&e){
        e.arm="A";
        ea=std::current_exception();
      }
    }
    ;
    if(reverse){
      A();
      C();
    }
    else{
      C();
      A();
    }
    if(ec)std::rethrow_exception(ec);
    if(ea)std::rethrow_exception(ea);
    c=tc.post;
    a=ta.post;
    return J::obj({
      {
        "C",turn_value(tc,target,0)
      }
      ,{
        "A",turn_value(ta,target,q)
      }
    }
    );
  }
  inline J kat_value(const Inputs&in){
    J vectors=J::arr();
    std::istringstream stream(in.txt("KAT.txt"));
    std::string line;
    u32 row=0;
    while(std::getline(stream,line)){
      std::istringstream s(line);
      std::string name,round;
      s>>name>>round;
      if(name!="philox4x32"||round!="10")continue;
      std::array<u32,4>c{
      }
      ,expected{
      }
      ;
      mm::PhiloxKey k{
      }
      ;
      J cj=J::arr(),kj=J::arr(),wj=J::arr();
      std::string x;
      for(auto&v:c){
        s>>x;
        need(ishex(x,8));
        v=std::stoul(x,nullptr,16);
        cj.push(x);
      }
      s>>x;
      k.k0=std::stoul(x,nullptr,16);
      kj.push(x);
      s>>x;
      k.k1=std::stoul(x,nullptr,16);
      kj.push(x);
      for(auto&v:expected){
        s>>x;
        need(ishex(x,8));
        v=std::stoul(x,nullptr,16);
      }
      auto actual=mm::philox4x32_10(c,k);
      if(actual!=expected){
        Error e(3,"MISMATCH","kat");
        e.index=row;
        throw e;
      }
      for(auto v:actual)wj.push(hx(v,8));
      vectors.push(J::obj({
        {
          "counter",cj
        }
        ,{
          "key",kj
        }
        ,{
          "words",wj
        }
      }
      ));
      ++row;
    }
    need(row==3,2,"INPUT","kat");
    return J::obj({
      {
        "vectors",vectors
      }
    }
    );
  }
  inline J key_value(const Inputs&in){
    std::set<std::string>strings;
    for(auto tau:{
      "production-r1","benchmark-r1","fixture-r1"
    }
    )for(auto&p:purposes())strings.insert(std::string(STUDY)+"|"+tau+"|"+p);
    std::string table="key_string\tsha256\tk0\tk1\tkey64\n";
    std::set<u64>newkeys,oldkeys;
    for(auto&s:strings){
      auto d=mm::sha256_bytes(s.data(),s.size());
      u64 k=get(d.data(),8);
      need(newkeys.insert(k).second,2,"INPUT","keys");
      table+=s+"\t"+mm::to_hex(d)+"\t"+hx(get(d.data(),4),8)+"\t"+hx(get(d.data()+4,4),8)+"\t"+hx(k,16)+"\n";
    }
    need(table==in.txt("KEYS.tsv"),2,"INPUT","keys");
    auto lines=split(in.txt("LEGACY_KEYS.tsv"),'\n');
    need(lines.back().empty(),2,"INPUT","keys");
    lines.pop_back();
    need(lines.front()=="purpose_string\tsha256\tk0_le32\tk1_le32\tfirst64_le",2,"INPUT","keys");
    std::string prev;
    for(size_t i=1;i<lines.size();++i){
      auto row=split(lines[i],'\t');
      need(row.size()==5&&row[0]>prev,2,"INPUT","keys");
      prev=row[0];
      auto d=mm::sha256_bytes(row[0].data(),row[0].size());
      u64 k=get(d.data(),8);
      need(row[1]==mm::to_hex(d)&&row[2]==hx(get(d.data(),4),8)&&row[3]==hx(get(d.data()+4,4),8)&&row[4]==hx(k,16)&&oldkeys.insert(k).second&&!newkeys.count(k),2,"INPUT","keys");
    }
    need(oldkeys.size()==126,2,"INPUT","keys");
    return J::obj({
      {
        "keys_sha256",sha(table)
      }
      ,{
        "legacy_sha256",sha(in.txt("LEGACY_KEYS.tsv"))
      }
      ,{
        "new_count",27
      }
      ,{
        "legacy_count",126
      }
      ,{
        "distinct",true
      }
      ,{
        "disjoint",true
      }
    }
    );
  }
  inline J address(const J&p){
    keys(p,{
      "tier","purpose","b","u","entity","subindex","words"
    }
    );
    Tier t;
    try{
      t=tier(p.at("tier").str());
    }
    catch(...){
      fail(2,"RANGE","tier");
    }
    auto name=p.at("purpose").str();
    auto it=std::find(purposes().begin(),purposes().end(),name);
    field(it!=purposes().end(),"purpose",2,"RANGE");
    u32 purpose=u32(it-purposes().begin());
    auto num=[&](const char*n,u64 max){
      try{
        return p.at(n).num(max);
      }
      catch(...){
        fail(2,"RANGE",n);
      }
    }
    ;
    u32 b=num("b",t.n-1),u=num("u",purpose==INITIAL?0:512);
    field(purpose==INITIAL||u>=1,"u",2,"RANGE");
    u32 e=num("entity",purpose==INNOV||purpose==COPY?0:purpose==DONOR?95:31),s=num("subindex",purpose==LOCAL?31:purpose==SURVIVAL?255:0);
    auto&w=p.at("words");
    std::array<u32,4>v{
    }
    ;
    try{
      need(w.type==J::A&&w.a.size()==4);
      for(u32 i=0;i<4;++i)v[i]=h32(w.a[i]);
    }
    catch(...){
      fail(2,"RANGE","words");
    }
    J extracted;
    if(purpose==COPY)extracted=J(v[0]&1);
    else if(purpose==LOCAL||purpose==MUTATION)extracted=J((v[0]&31)==0);
    else if(purpose==DONOR||purpose==SURVIVAL)extracted=J(hx(u64(v[0])|(u64(v[1])<<32),16));
    else extracted=J(hx(v[0],8));
    J counter=J::arr();
    counter.push(b);
    counter.push(u);
    counter.push(e);
    counter.push(s);
    return J::obj({
      {
        "counter",counter
      }
      ,{
        "extracted",extracted
      }
    }
    );
  }
  inline J compute_fixture(const std::string&kind,const J&p,const Inputs&in){
    if(kind=="KEYS"){
      keys(p,{
      }
      );
      return key_value(in);
    }
    if(kind=="KAT"){
      keys(p,{
        "algorithm","rounds"
      }
      );
      need(p.at("algorithm").str()=="philox4x32"&&p.at("rounds").num()==10);
      return kat_value(in);
    }
    if(kind=="ADDRESS")return address(p);
    if(kind=="STATE"){
      keys(p,{
        "state"
      }
      );
      auto s=state_json(p.at("state"));
      Bytes b(264);
      encode(b.data(),s);
      return J::obj({
        {
          "state",state_value(decode(b.data()))
        }
        ,{
          "bytes_hex",hex(b)
        }
        ,{
          "bytes",264
        }
      }
      );
    }
    if(kind=="LEMIRE"){
      keys(p,{
        "w","x"
      }
      );
      u64 w;
      try{
        w=h64(p.at("w"));
        field(w>=1&&w<=(u64(1)<<39),"w",2,"RANGE");
      }
      catch(...){
        fail(2,"RANGE","w");
      }
      auto&x=p.at("x");
      need(x.type==J::A&&!x.a.empty()&&x.a.size()<=256);
      std::vector<u64>v;
      for(auto&z:x.a)v.push_back(h64(z));
      auto pick=lemire(w,[&](u32 r){
        if(r>=v.size()){
          Error e(3,"TAPE_EXHAUSTED");e.retry=r;throw e;
        }
        return v[r];
      }
      );
      return J::obj({
        {
          "threshold",hx((u64(0)-w)%w,16)
        }
        ,{
          "z",hx(pick.z,16)
        }
        ,{
          "rejections",pick.retries
        }
      }
      );
    }
    if(kind=="OPERATOR"){
      keys(p,{
        "state","u","target","tape","arm_order","threads"
      }
      );
      State c=state_json(p.at("state")),a=c;
      need(p.at("u").num(512)>=1);
      u32 target=h32(p.at("target"));
      auto tape=injected(p.at("tape"));
      scheduling_fields(p);
      return pair_step(c,a,target,tape,p.at("arm_order").a[0].str()=="A");
    }
    if(kind=="CHAIN"){
      keys(p,{
        "state","first_u","steps","arm_order","threads"
      }
      );
      State c=state_json(p.at("state")),a=c;
      u32 u=p.at("first_u").num(512);
      auto&steps=p.at("steps");
      need(u>=1&&steps.type==J::A&&steps.a.size()>=1&&steps.a.size()<=4&&u+steps.a.size()-1<=512);
      scheduling_fields(p);
      std::vector<std::pair<u32,Tape>>st;
      for(auto&s:steps.a){
        keys(s,{
          "target","tape"
        }
        );
        st.emplace_back(h32(s.at("target")),injected(s.at("tape")));
      }
      J values=J::arr();
      for(auto&s:st){
        try{
          auto v=pair_step(c,a,s.first,s.second,p.at("arm_order").a[0].str()=="A");
          v["u"]=J(u);
          values.push(v);
        }
        catch(Error&e){
          e.u=u;
          throw;
        }
        ++u;
      }
      return J::obj({
        {
          "steps",values
        }
      }
      );
    }
    if(kind=="TARGET"){
      keys(p,{
        "first_u","previous","innovations","copy"
      }
      );
      auto range=[&](bool ok,const char*n){
        field(ok,n,2,"RANGE");
      }
      ;
      u32 first;
      try{
        first=p.at("first_u").num(512);
        range(first==1||first>=3,"first_u");
      }
      catch(...){
        fail(2,"RANGE","first_u");
      }
      auto&previous=p.at("previous");
      std::vector<u32>ts;
      try{
        need(previous.type==J::A&&previous.a.size()==(first==1?0:2));
        for(auto&x:previous.a)ts.push_back(h32(x));
      }
      catch(...){
        fail(2,"RANGE","previous");
      }
      auto&inn=p.at("innovations");
      std::vector<u32>iv;
      try{
        need(inn.type==J::A&&inn.a.size()>=1&&first+inn.a.size()-1<=512);
        for(auto&x:inn.a)iv.push_back(h32(x));
      }
      catch(...){
        fail(2,"RANGE","innovations");
      }
      auto&copy=p.at("copy");
      std::vector<u32>cv;
      try{
        need(copy.type==J::A&&copy.a.size()==iv.size());
        for(auto&x:copy.a)cv.push_back(x.num(1));
      }
      catch(...){
        fail(2,"RANGE","copy");
      }
      J out=J::arr();
      for(u32 i=0;i<iv.size();++i){
        u32 v=(first+i>=3&&cv[i])?ts[ts.size()-2]:iv[i];
        ts.push_back(v);
        out.push(hx(v,8));
      }
      return J::obj({
        {
          "targets",out
        }
      }
      );
    }
    if(kind=="TOTALS"){
      keys(p,{
        "tier","rows"
      }
      );
      auto t=tier(p.at("tier").str());
      i64 m=0,a=0;
      try{
        auto&rows=p.at("rows");
        need(rows.type==J::A&&rows.a.size()==t.n);
        for(u32 i=0;i<t.n;++i){
          auto&r=rows.a[i];
          need(r.type==J::A&&r.a.size()==5&&r.a[0].num()==i);
          u32 mc=r.a[1].num(8192),ma=r.a[2].num(8192),ac=r.a[3].num(262144),aa=r.a[4].num(262144);
          m+=i64(mc)-ma;
          a+=i64(ac)-aa;
        }
      }
      catch(...){
        fail(2,"RANGE","rows");
      }
      return J::obj({
        {
          "results_text",results(t,m,a)
        }
      }
      );
    }
    if(kind=="INFERENCE"){
      keys(p,{
        "policy_signed_sum","accuracy_signed_sum"
      }
      );
      auto number=[&](const char*n,i64 bound){
        i64 v;
        try{
          v=p.at(n).sint();
          field(v>=-bound&&v<=bound,n,2,"RANGE");
        }
        catch(...){
          fail(2,"RANGE",n);
        }
        return v;
      }
      ;
      i64 m=number("policy_signed_sum",163840000),a=number("accuracy_signed_sum",5242880000LL);
      return J::obj({
        {
          "policy",interval(m,163840000,19778834661ULL)
        }
        ,{
          "accuracy",interval(a,5242880000ULL,23018074131ULL)
        }
      }
      );
    }
    if(kind=="CLASSIFY"){
      keys(p,{
        "lower","upper"
      }
      );
      Rat lo=fraction(p.at("lower").str()),hi=fraction(p.at("upper").str());
      field(Rat(-1)<=lo&&lo<=hi&&hi<=Rat(1),"interval",2,"RANGE");
      return J::obj({
        {
          "classification",classify(lo,hi)
        }
      }
      );
    }
    if(kind=="CERTIFICATE"){
      keys(p,{
      }
      );
      return certificate(in.txt("INTERVAL_CERTIFICATE.json"));
    }
    if(kind=="RECORD"){
      keys(p,{
        "base","patches","truncate","append_hex","rehash"
      }
      );
      need(p.at("base").str()=="FIXIN_BUNDLE/literals/SYNTHETIC_RECORD.bin");
      Bytes r=in.at(p.at("base").str());
      auto&patches=p.at("patches");
      need(patches.type==J::A);
      std::vector<bool>used(r.size());
      for(auto&patch:patches.a){
        keys(patch,{
          "offset","hex"
        }
        );
        u32 off=patch.at("offset").num(RECORD_BYTES);
        auto b=unhex(patch.at("hex").str());
        need(!b.empty()&&off+b.size()<=r.size());
        for(u32 j=0;j<b.size();++j){
          need(!used[off+j]);
          used[off+j]=true;
          r[off+j]=b[j];
        }
      }
      r.resize(p.at("truncate").num(RECORD_BYTES));
      auto app=unhex(p.at("append_hex").str());
      r.insert(r.end(),app.begin(),app.end());
      if(p.at("rehash").boolean()&&r.size()==RECORD_BYTES)rehash(r);
      return record_value(r);
    }
    if(kind=="CHUNK_HEADER"){
      keys(p,{
        "bytes_hex","tier","chunk_index"
      }
      );
      auto t=tier(p.at("tier").str());
      u32 k=p.at("chunk_index").num(UINT32_MAX);
      return validate_chunk(unhex(p.at("bytes_hex").str()),t,k);
    }
    if(kind=="MANIFEST"){
      keys(p,{
        "bytes_hex","tier"
      }
      );
      auto t=tier(p.at("tier").str());
      auto s=text(unhex(p.at("bytes_hex").str()));
      auto m=validate_manifest(s,t);
      return J::obj({
        {
          "block_count",t.n
        }
        ,{
          "chunk_count",t.chunks()
        }
        ,{
          "manifest_sha256",sha(s)
        }
      }
      );
    }
    if(kind=="CLI"){
      keys(p,{
        "argv"
      }
      );
      try{
        auto&a=p.at("argv");
        need(a.type==J::A);
        std::vector<std::string>v;
        for(auto&x:a.a)v.push_back(x.str());
        auto m=drycli(v);
        J options;
        for(auto&x:m)options[x.first]=J(x.second);
        return J::obj({
          {
            "command",v[0]
          }
          ,{
            "options",options
          }
        }
        );
      }
      catch(...){
        fail(2,"CLI","argv");
      }
    }
    fail(2,"FORMAT","kind");
  }
  inline bool known_kind(const std::string&s){
    static const std::set<std::string>k={
      "KEYS","KAT","ADDRESS","STATE","LEMIRE","OPERATOR","CHAIN","TARGET","TOTALS","INFERENCE","CLASSIFY","CERTIFICATE","RECORD","CHUNK_HEADER","MANIFEST","CLI"
    }
    ;
    return k.count(s);
  }
  inline std::pair<J,int> fixture(const J&c,const Inputs&in){
    keys(c,{
      "version","id","kind","payload"
    }
    );
    need(c.at("version").num()==1);
    auto id=c.at("id").str(),kind=c.at("kind").str();
    need(!id.empty()&&id.size()<=64&&id[0]>='A'&&id[0]<='Z'&&id.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")==id.npos&&known_kind(kind));
    J out=J::obj({
      {
        "version",1
      }
      ,{
        "id",id
      }
      ,{
        "kind",kind
      }
    }
    );
    try{
      auto value=compute_fixture(kind,c.at("payload"),in);
      out["status"]=J("OK");
      out["value"]=value;
      return{
        out,0
      }
      ;
    }
    catch(const Error&e){
      J where;
      if(!e.arm.empty())where["arm"]=J(e.arm);
      if(e.draw>=0)where["draw"]=J(e.draw);
      if(e.retry>=0)where["retry"]=J(e.retry);
      if(e.u>=0)where["u"]=J(e.u);
      if(!e.field.empty())where["field"]=J(e.field);
      if(e.index>=0)where["index"]=J(e.index);
      out["status"]=J("ERROR");
      out["code"]=J(e.code);
      out["where"]=where;
      return{
        out,e.exit
      }
      ;
    }
  }
}
