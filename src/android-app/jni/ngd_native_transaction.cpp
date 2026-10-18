#include "ngd_native_session.h"
namespace ngd12 {
const char* ActionName(Action a) {
  switch(a) {
#define NGD_NAME(x) case Action::x: return #x
    NGD_NAME(Preflight); NGD_NAME(MaskBlink); NGD_NAME(DisableNgd);
    NGD_NAME(BleScanOff); NGD_NAME(StopBlink); NGD_NAME(OriginalBlinkGone);
    NGD_NAME(CloseHci); NGD_NAME(HciFree); NGD_NAME(SnapshotData); NGD_NAME(Arm); NGD_NAME(EnableNgd);
    NGD_NAME(WaitAndroidOn); NGD_NAME(Disarm); NGD_NAME(NgdGone);
    NGD_NAME(RestoreData); NGD_NAME(UnmaskBlink); NGD_NAME(RestoreSettings); NGD_NAME(StartBlink); NGD_NAME(OemOwner);
#undef NGD_NAME
  }
  return "Unknown";
}
bool Transaction::Prepare() {
  if (!ops_.Do(Action::Preflight)) return false;
  touched_ = true; // Even a failed mount may have partially changed runtime state.
  if (!ops_.Do(Action::MaskBlink)) return false;
  if (!ops_.Do(Action::DisableNgd) || !ops_.Do(Action::NgdGone) || !ops_.Do(Action::BleScanOff)) return false;
  oem_stopped_ = true; // Stop request can succeed even if its verification times out.
  if (!ops_.Do(Action::StopBlink) || !ops_.Do(Action::OriginalBlinkGone) ||
      !ops_.Do(Action::CloseHci) || !ops_.Do(Action::HciFree) || !ops_.Do(Action::SnapshotData)) return false;
  ngd_may_own_ = true;
  if (!ops_.Do(Action::Arm) || !ops_.Do(Action::EnableNgd) ||
      !ops_.Do(Action::WaitAndroidOn)) return false;
  return true;
}
bool Transaction::Restore() {
  if (restored_) return true;
  if (!touched_) { restored_ = true; return true; }
  // Never expose the OEM executable/start OEM while an NGD owner may remain.
  if (!ops_.Do(Action::Disarm)) return false;
  const bool disable_ok = ops_.Do(Action::DisableNgd);
  if (!disable_ok || !ops_.Do(Action::NgdGone)) return false;
  if (oem_stopped_ && (!ops_.Do(Action::CloseHci) || !ops_.Do(Action::HciFree))) return false;
  const bool data_ok = ops_.Do(Action::RestoreData);
  if (!ops_.Do(Action::UnmaskBlink)) return false;
  const bool settings_ok = ops_.Do(Action::RestoreSettings);
  const bool oem_ok = ops_.Do(Action::StartBlink) && ops_.Do(Action::OemOwner);
  restored_ = data_ok && settings_ok && oem_ok;
  return restored_;
}
}  // namespace ngd12
