// Copyright 2026. NGD controlled test admission; this does NOT release OEM HCI.
#ifndef NGD_HCI_SESSION_H
#define NGD_HCI_SESSION_H
#include <stdint.h>
#include <string.h>
namespace ngd {
constexpr uint64_t kMaxSessionMs = 120000;
inline bool ParseDeadline(const char* text, uint64_t* value) {
  if (!text || !*text || !value) return false;
  uint64_t n = 0;
  for (const char* p = text; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
    const uint64_t digit = static_cast<uint64_t>(*p - '0');
    if (n > (UINT64_MAX - digit) / 10) return false;
    n = n * 10 + digit;
  }
  *value = n;
  return true;
}
// The mode is operator authorization, NOT proof that BLINK released the HAL.
// Never renew an already admitted session by extending the property deadline.
inline uint64_t RemainingMs(const char* mode, const char* text, uint64_t now,
                            uint64_t captured_deadline, uint64_t* effective) {
  uint64_t requested = 0;
  if (effective) *effective = 0;
  if (!mode || strcmp(mode, "manual-after-oem-release") != 0 ||
      !ParseDeadline(text, &requested) || requested <= now ||
      requested - now > kMaxSessionMs) return 0;
  uint64_t deadline = requested;
  if (captured_deadline && captured_deadline < deadline) deadline = captured_deadline;
  if (deadline <= now) return 0;
  if (effective) *effective = deadline;
  return deadline - now;
}
// The root broker renews a short lease only after checking ownership.
// This is separate from, and cannot extend, a bounded maintenance admission.
inline uint64_t LeaseRemainingMs(bool admitted_renewable, bool reply_renewable,
                                 uint64_t deadline, uint64_t now) {
  return admitted_renewable && reply_renewable && deadline > now &&
         deadline-now <= 5000 ? deadline-now : 0;
}
}  // namespace ngd
#endif
