#include "ngd_native_session.h"
#include <vector>
#include <cassert>
#include <algorithm>
#include <cstdio>
using ngd12::Action;
struct Fake final:ngd12::Operations {
 std::vector<Action> seen;int fail=-1;int count=0;
 bool Do(Action a)override {seen.push_back(a);return count++!=fail;}
 size_t Pos(Action a)const {auto p=std::find(seen.begin(),seen.end(),a);return p==seen.end()?seen.size():p-seen.begin();}
 bool Has(Action a)const{return Pos(a)<seen.size();}
};
int main() {
 Fake success;ngd12::Transaction t(success);assert(t.Prepare());assert(t.Restore());size_t total=success.seen.size();assert(t.Restore()&&success.seen.size()==total);
 assert(success.Pos(Action::MaskBlink)<success.Pos(Action::StopBlink));assert(success.Pos(Action::CloseHci)<success.Pos(Action::Arm));assert(success.Pos(Action::Arm)<success.Pos(Action::EnableNgd));
 int prep=0;for(Action a:success.seen){++prep;if(a==Action::WaitAndroidOn)break;}
 for(int fail=0;fail<prep;++fail){Fake f;f.fail=fail;ngd12::Transaction x(f);assert(!x.Prepare());assert(x.Restore());
  if(fail==0){assert(!f.Has(Action::MaskBlink));assert(!f.Has(Action::StartBlink));}
  if(f.Has(Action::EnableNgd)){assert(f.Pos(Action::MaskBlink)<f.Pos(Action::EnableNgd));}
 }
 // Inject every restoration failure after successful preparation.
 const int restore_count=static_cast<int>(success.seen.size())-prep;
 for(int stage=0;stage<restore_count;++stage){Fake f;ngd12::Transaction x(f);assert(x.Prepare());const size_t start=f.seen.size();f.fail=f.count+stage;assert(!x.Restore());
  std::vector<Action> restore(f.seen.begin()+start,f.seen.end());
  auto has=[&](Action a){return std::find(restore.begin(),restore.end(),a)!=restore.end();};
  if(stage<=4){assert(!has(Action::UnmaskBlink));assert(!has(Action::StartBlink));}
  if(stage==5){assert(has(Action::UnmaskBlink));assert(has(Action::StartBlink));}
 }
 // A settings restoration failure must not prevent a safely released OEM restart.
 Fake f;ngd12::Transaction x(f);assert(x.Prepare());f.fail=f.count+7;assert(!x.Restore());assert(f.Has(Action::StartBlink));
 uint64_t deadline=9;assert(!ngd12::QueryRootSession(true,&deadline));assert(deadline==0);assert(!ngd12::RequestRootRestore());
 puts("PASS: transaction order, every preparation/restore failure, no concurrent owner restoration, host denial, idempotent success");
}
