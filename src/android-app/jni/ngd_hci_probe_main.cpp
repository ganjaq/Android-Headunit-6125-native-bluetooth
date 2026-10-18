#include "ngd_hci_probe.h"
#include <cstdio>
#include <cstring>
int main(int argc, char** argv) {
  if (argc != 2 || std::strcmp(argv[1], "--check") != 0) {
    std::fprintf(stderr, "Usage: ngd_hci_probe --check (read-only; never loads QTI)\n");
    return 64;
  }
  const ngd::HciProbeResult r = ngd::ProbeHci(false);
  std::printf("{\"hci\":\"%s\",\"reason\":\"%s\",\"snapshot_valid\":%s,\"server_pid\":%d,\"client_pids\":[",
              ngd::OccupancyName(r.occupancy), r.reason, r.snapshot.valid ? "true" : "false", r.snapshot.server_pid);
  for (size_t i = 0; i < r.snapshot.clients.size(); ++i)
    std::printf("%s%d", i ? "," : "", r.snapshot.clients[i]);
  std::puts("],\"qti_loaded\":false,\"oem_changed\":false,\"hci_switch_implemented\":false}");
  if (r.occupancy == ngd::HciOccupancy::Busy) return 10;
  if (r.occupancy == ngd::HciOccupancy::Unknown) return 20;
  return 0; // Observation only; never permission to initialize a controller.
}
