#ifndef NGD_VERIFIED_OWNER_CACHE_H
#define NGD_VERIFIED_OWNER_CACHE_H
#include <cstdint>
#include <mutex>
namespace ngd15 {
// Only the root verifier may refresh this. Each authenticated request still checks
// the maintenance barrier, UID, exact admitted process, MCU, USB and session deadline.
class VerifiedOwnerCache {
 public:
  void Update(int pid,bool verified,uint64_t now) {
   std::lock_guard<std::mutex> lock(m_);pid_=pid;valid_=verified&&pid>0;observed_=now;
  }
  bool Allows(int pid,uint64_t now) {
   std::lock_guard<std::mutex> lock(m_);
   return valid_&&pid==pid_&&now>=observed_&&now-observed_<3000;
  }
 private:
  std::mutex m_;int pid_=-1;bool valid_=false;uint64_t observed_=0;
};
}
#endif
