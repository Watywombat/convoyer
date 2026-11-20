#include <initializer_list>
#include "conveyor.hpp"
#include <cassert>
#include <cstdio>
using namespace conveyor;
int main(){
 char ref[maxRef+1]; uint8_t longRef[maxRef+1]; memset(longRef,'A',sizeof longRef);
 assert(parseRef(longRef,maxRef,ref)&&strlen(ref)==maxRef);
 assert(!parseRef(longRef,sizeof longRef,ref)&&!ref[0]);
 assert(!parseRef(nullptr,1,ref)&&!parseRef(longRef,0,ref));
 for(auto c : {0,32,47,255}) {uint8_t b=static_cast<uint8_t>(c);assert(!parseRef(&b,1,ref));}
 const int map[]={14,27,31}, duplicate[]={14,14,31};
 for(int i=0;i<3;i++) assert(exitFor(map[i],map)==i+1);
 assert(!exitFor(2,map)&&!exitFor(14,duplicate));
 State s=State::Boot;
 for(auto e:{Event::Ready,Event::Arrival,Event::AtReader,Event::Tag,Event::Found,Event::Space,Event::Recorded,Event::GateReady,Event::CorrectExit}){
 auto r=step(s,e);assert(r.accepted);s=r.next;}
 assert(s==State::Idle);
 assert(step(State::Capacity,Event::Full).next==State::Hold);
 assert(step(State::Lookup,httpResult(State::Lookup,404)).error==Error::UnknownProduct);
 assert(step(State::Commit,httpResult(State::Commit,504)).next==State::Recovery);
 for(int code:{401,403,429,500})assert(httpResult(State::Lookup,code)==Event::Failed);
 assert(httpResult(State::Commit,201)==Event::Recorded);
 assert(step(State::Verify,exitEdge(2,1)).next==State::Report);
 assert(step(State::Verify,exitEdge(2,2)).next==State::Idle);
 assert(step(State::Boot,Event::Pending).next==State::Recovery);
 for(auto failure:{Event::Failed,Event::Timeout}){
  for(auto state:{State::Feed,State::Read,State::Lookup,State::Capacity,State::Gate})
   assert(step(state,failure).next==State::Hold);
  for(auto state:{State::Commit,State::Report})assert(step(state,failure).next==State::Recovery);
  assert(step(State::Verify,failure).next==State::Report);
 }
 for(auto state:{State::Idle,State::Feed,State::Read,State::Lookup,State::Capacity,State::Commit,State::Gate,State::Verify,State::Report})
  assert(step(state,Event::Stop).next==State::Recovery);
 for(auto state:{State::Hold,State::Recovery,State::ConfigFault})assert(step(state,Event::Cleared).next==State::Boot);
 assert(step(State::Boot,Event::BadConfig).next==State::ConfigFault);
 assert(step(State::Read,Event::InvalidTag).error==Error::Nfc);
 assert(step(State::Report,Event::Reported).next==State::Hold);
 assert(!step(State::Commit,Event::CorrectExit).accepted);
 assert(!step(State::Hold,Event::Arrival).accepted);
 for(int i=0;i<=int(State::Hold);i++)for(int j=0;j<int(Event::Count);j++){
 auto st=State(i);auto ev=Event(j);auto result=step(st,ev);int matches=0;
 for(auto &r:rules)if(r.from==st&&r.event==ev){matches++;assert(result.next==r.to&&result.error==r.error);}
 assert(matches<=1&&result.accepted==(matches==1));if(!matches)assert(result.next==st);}
 puts("OK: NFC, mapping, WMS, nominal cycle, faults and complete state/event matrix");
}
