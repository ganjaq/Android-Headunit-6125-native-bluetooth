#ifndef NGD_NATIVE_SESSION_H
#define NGD_NATIVE_SESSION_H
#include <stdint.h>
#include <string>
namespace ngd12 {
constexpr uint64_t kSessionMaxMs = 120000;
constexpr const char* kSocketName = "ngd.native.android.session.v16";
// Valid only while a root peer has verified the OEM maintenance barrier.
bool QueryRootSession(bool admit, uint64_t* deadline_ms, bool* renewable = nullptr);
bool RequestRootRestore();
enum class Action { Preflight, MaskBlink, DisableNgd, BleScanOff, StopBlink,
  OriginalBlinkGone, CloseHci, HciFree, SnapshotData, Arm, EnableNgd, WaitAndroidOn,
  Disarm, NgdGone, RestoreData, UnmaskBlink, RestoreSettings, StartBlink, OemOwner };
const char* ActionName(Action action);
class Operations {
 public:
  virtual ~Operations() = default;
  virtual bool Do(Action action) = 0;
};
class Transaction {
 public:
  explicit Transaction(Operations& ops) : ops_(ops) {}
  bool Prepare();
  bool Restore();
  bool touched() const { return touched_; }
 private:
  Operations& ops_;
  bool touched_ = false;
  bool oem_stopped_ = false;
  bool ngd_may_own_ = false;
  bool restored_ = false;
};
}  // namespace ngd12
#endif
