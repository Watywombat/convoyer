#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace conveyor {
enum class State { Boot, ConfigFault, Recovery, Idle, Feed, Read, Lookup, Capacity, Commit, Gate, Verify, Report, Hold };
enum class Event { Ready, BadConfig, Pending, Cleared, Arrival, AtReader, Tag, InvalidTag, Found, Unknown, Space, Full, Recorded, GateReady, CorrectExit, WrongExit, Timeout, Failed, Reported, Stop, Count };
enum class Error { None, Config, Interrupted, Nfc, UnknownProduct, Full, Wms, UncertainMovement, Jam, Misroute, Manual };
struct Result { State next; Error error; bool accepted; };
struct Rule { State from; Event event; State to; Error error; };
constexpr Rule rules[] = {
 {State::Boot, Event::Ready, State::Idle, Error::None},
 {State::Boot, Event::BadConfig, State::ConfigFault, Error::Config},
 {State::Boot, Event::Pending, State::Recovery, Error::Interrupted},
 {State::ConfigFault, Event::Cleared, State::Boot, Error::None},
 {State::Recovery, Event::Cleared, State::Boot, Error::None},
 {State::Hold, Event::Cleared, State::Boot, Error::None},
 {State::Idle, Event::Arrival, State::Feed, Error::None},
 {State::Feed, Event::AtReader, State::Read, Error::None},
 {State::Read, Event::Tag, State::Lookup, Error::None},
 {State::Read, Event::InvalidTag, State::Hold, Error::Nfc},
 {State::Lookup, Event::Found, State::Capacity, Error::None},
 {State::Lookup, Event::Unknown, State::Hold, Error::UnknownProduct},
 {State::Capacity, Event::Space, State::Commit, Error::None},
 {State::Capacity, Event::Full, State::Hold, Error::Full},
 {State::Commit, Event::Recorded, State::Gate, Error::None},
 {State::Gate, Event::GateReady, State::Verify, Error::None},
 {State::Verify, Event::CorrectExit, State::Idle, Error::None},
 {State::Verify, Event::WrongExit, State::Report, Error::Misroute},
 {State::Report, Event::Reported, State::Hold, Error::Misroute},
 {State::Feed, Event::Failed, State::Hold, Error::Jam},
 {State::Feed, Event::Timeout, State::Hold, Error::Jam},
 {State::Read, Event::Failed, State::Hold, Error::Nfc},
 {State::Read, Event::Timeout, State::Hold, Error::Nfc},
 {State::Lookup, Event::Failed, State::Hold, Error::Wms},
 {State::Lookup, Event::Timeout, State::Hold, Error::Wms},
 {State::Capacity, Event::Failed, State::Hold, Error::Wms},
 {State::Capacity, Event::Timeout, State::Hold, Error::Wms},
 {State::Commit, Event::Failed, State::Recovery, Error::UncertainMovement},
 {State::Commit, Event::Timeout, State::Recovery, Error::UncertainMovement},
 {State::Gate, Event::Failed, State::Hold, Error::Jam},
 {State::Gate, Event::Timeout, State::Hold, Error::Jam},
 {State::Verify, Event::Failed, State::Report, Error::Jam},
 {State::Verify, Event::Timeout, State::Report, Error::Jam},
 {State::Report, Event::Failed, State::Recovery, Error::Wms},
 {State::Report, Event::Timeout, State::Recovery, Error::Wms},
 {State::Idle, Event::Stop, State::Recovery, Error::Manual},
 {State::Feed, Event::Stop, State::Recovery, Error::Manual},
 {State::Read, Event::Stop, State::Recovery, Error::Manual},
 {State::Lookup, Event::Stop, State::Recovery, Error::Manual},
 {State::Capacity, Event::Stop, State::Recovery, Error::Manual},
 {State::Commit, Event::Stop, State::Recovery, Error::Manual},
 {State::Gate, Event::Stop, State::Recovery, Error::Manual},
 {State::Verify, Event::Stop, State::Recovery, Error::Manual},
 {State::Report, Event::Stop, State::Recovery, Error::Manual},
};
inline Result step(State state, Event event) {
 for (const auto &r : rules)
  if (r.from == state && r.event == event) return {r.to,r.error,true};
 return {state,Error::None,false};
}
// Payload after tag decoding: ASCII product reference, without padding.
constexpr size_t maxRef = 63;
inline bool parseRef(const uint8_t *p,size_t n,char (&out)[maxRef+1]) {
 out[0]=0;
 if (!p || !n || n>maxRef) return false;
 for(size_t i=0;i<n;++i) {
  auto c=p[i];
  if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.')) return false;
 }
 std::memcpy(out,p,n); out[n]=0; return true;
}
inline int exitFor(int warehouse,const int (&mapping)[3]) {
 if (warehouse<=0 || mapping[0]<=0 || mapping[1]<=0 || mapping[2]<=0 ||
 mapping[0]==mapping[1] || mapping[1]==mapping[2] || mapping[0]==mapping[2]) return 0;
 for(int i=0;i<3;++i) if(mapping[i]==warehouse) return i+1;
 return 0;
}
inline Event httpResult(State state,int status) {
 if(status==0 || status==408 || status==504) return Event::Timeout;
 if(state==State::Lookup && status==404) return Event::Unknown;
 if(status<200 || status>=300) return Event::Failed;
 // Emit only after validating the response body in the adapter.
 switch(state) {
 case State::Lookup:return Event::Found;
 case State::Commit:return Event::Recorded;
 case State::Report:return Event::Reported;
 default:return Event::Failed;
 }
}
inline Event exitEdge(int expected,int observed) {
 return expected>=1 && expected<=3 && observed==expected ? Event::CorrectExit : Event::WrongExit;
}
}
