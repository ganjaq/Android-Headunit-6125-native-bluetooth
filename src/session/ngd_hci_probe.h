// Read-only admission evidence. This is not an HCI handover protocol.
#ifndef NGD_HCI_PROBE_H
#define NGD_HCI_PROBE_H
#include <cstdint>
#include <string>
#include <vector>
namespace ngd {
enum class HciOccupancy { Unknown, Busy, NoForeignClientObserved };
struct HciSnapshot {
  bool valid = false;
  int server_pid = -1;
  std::vector<int> clients;
};
struct HciProbeResult {
  HciOccupancy occupancy = HciOccupancy::Unknown;
  const char* reason = "incomplete_evidence";
  HciSnapshot snapshot;
};
HciSnapshot ParseHciSnapshot(const std::string& text, bool command_completed);
HciSnapshot ParseHwbinderNode(const std::string& text, int server_pid, uint64_t object_cookie);
int PrintDirectHciSnapshot(); // Root-only read: getDebugInfo + exact hwbinder node; never initialize HCI.
HciProbeResult EvaluateHci(const HciSnapshot& snapshot, const std::string& blink_state,
                          int manager_pid, int self_pid, bool already_admitted);
HciProbeResult ProbeHci(bool already_admitted);
HciProbeResult ProbeHciForProcess(int expected_pid, bool admitted, bool verified_maintenance_barrier);
const char* OccupancyName(HciOccupancy state);
}  // namespace ngd
#endif
