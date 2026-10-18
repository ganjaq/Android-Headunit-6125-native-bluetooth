#include "ngd_hci_probe.h"
#include <algorithm>
#include <climits>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <cerrno>
#ifdef __ANDROID__
#include <android/hardware/bluetooth/1.0/IBluetoothHci.h>
#include <sys/system_properties.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <cstdio>
extern char** environ;
#endif
namespace ngd {
static const char kHci[] = "android.hardware.bluetooth@1.0::IBluetoothHci/default";
static bool PositivePid(const std::string& text, int* pid) {
  if (text.empty()) return false;
  unsigned n = 0;
  for (char c : text) {
    if (c < '0' || c > '9' || n > (static_cast<unsigned>(INT_MAX) - (c-'0')) / 10)
      return false;
    n = n * 10 + c - '0';
  }
  if (!n) return false;
  *pid = static_cast<int>(n);
  return true;
}
HciSnapshot ParseHciSnapshot(const std::string& text, bool completed) {
  HciSnapshot result;
  if (!completed || text.empty()) return result;
  bool binderized = false, columns = false, found = false;
  std::istringstream input(text);
  std::string line;
  while (std::getline(input, line)) {
    if (line.find("| All ") != std::string::npos) {
      binderized = line.find("All binderized services") != std::string::npos;
      continue;
    }
    if (!binderized) continue;
    if (line.find("Server") != std::string::npos && line.find("Clients") != std::string::npos) {
      columns = true;
      continue;
    }
    if (line.find(kHci) == std::string::npos) continue;
    std::istringstream row(line);
    std::vector<std::string> values;
    std::string value;
    while (row >> value) values.push_back(value);
    // Match the captured Android10 lshal schema, never infer a truncated column.
    if (found || !columns || values.size() < 5 || values[2] != kHci ||
        values[1] != "Y" || values[3].find('/') == std::string::npos ||
        !PositivePid(values[4], &result.server_pid)) return HciSnapshot{};
    for (size_t i = 5; i < values.size(); ++i) {
      int pid = -1;
      if (!PositivePid(values[i], &pid)) return HciSnapshot{};
      result.clients.push_back(pid);
    }
    found = true;
  }
  result.valid = found;
  return result;
}
HciSnapshot ParseHwbinderNode(const std::string& text,int server,uint64_t cookie) {
 HciSnapshot out;if(server<=0||!cookie||text.empty())return out;
 bool process=false,context=false,found=false;std::istringstream input(text);std::string line;
 while(std::getline(input,line)) {
  if(line.compare(0,5,"proc ")==0){int pid=-1;if(!PositivePid(line.substr(5),&pid)||pid!=server)return {};process=true;continue;}
  if(line.compare(0,8,"context ")==0){context=line=="context hwbinder";continue;}
  if(!context)continue;
  std::istringstream row(line);std::string label,id,u,c;
  if(!(row>>label)||label!="node")continue;
  if(!(row>>id>>u>>c)||c.size()<2||c[0]!='c')return {};
  auto hex=c.substr(1);if(hex.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)return {};
  errno=0;char* end=nullptr;unsigned long long parsed=strtoull(hex.c_str(),&end,16);
  if(errno||!end||*end)return {};
  if(parsed!=cookie)continue;
  if(found)return {};
  found=true;
  auto pos=line.rfind(" proc");if(pos==std::string::npos)return {};
  std::istringstream clients(line.substr(pos+5));std::string pidstr;
  while(clients>>pidstr){int pid=-1;if(!PositivePid(pidstr,&pid)||std::find(out.clients.begin(),out.clients.end(),pid)!=out.clients.end())return {};out.clients.push_back(pid);}
 }
 if(!process||!found)return {};
 out.valid=true;out.server_pid=server;return out;
}
HciProbeResult EvaluateHci(const HciSnapshot& s, const std::string& blink,
                          int manager, int self, bool admitted) {
  HciProbeResult r;
  r.snapshot = s;
  if (blink == "running" || blink == "restarting") {
    r.occupancy = HciOccupancy::Busy;
    r.reason = "blink_active_or_reacquire_risk";
    return r;
  }
  if (blink != "stopped" || !s.valid) return r;
  for (int pid : s.clients) {
    if (pid == manager && manager > 0 && manager != s.server_pid) continue;
    if (admitted && pid == self && self > 0) continue;
    r.occupancy = HciOccupancy::Busy;
    r.reason = "foreign_or_unidentified_hci_client";
    return r;
  }
  r.occupancy = HciOccupancy::NoForeignClientObserved;
  r.reason = "no_foreign_reference_observed_not_release_proof";
  return r;
}
const char* OccupancyName(HciOccupancy s) {
  switch (s) {
    case HciOccupancy::Busy: return "BUSY";
    case HciOccupancy::NoForeignClientObserved: return "NO_FOREIGN_CLIENT_OBSERVED";
    default: return "UNKNOWN";
  }
}
#ifdef __ANDROID__
static uint64_t NowMs() {
  struct timespec t = {};
  if (clock_gettime(CLOCK_BOOTTIME, &t) != 0) return 0;
  return static_cast<uint64_t>(t.tv_sec) * 1000 + t.tv_nsec / 1000000;
}
static std::string Cmdline(int pid) {
  std::ifstream f("/proc/" + std::to_string(pid) + "/cmdline");
  std::string name;
  std::getline(f, name, '\0');
  return name;
}
int PrintDirectHciSnapshot() {
 if(geteuid()!=0)return 77;
 using android::hardware::bluetooth::V1_0::IBluetoothHci;
 auto service=IBluetoothHci::tryGetService("default");if(!service)return 78;
 int pid=-1;uint64_t cookie=0;bool callback=false;
 auto ret=service->getDebugInfo([&](const auto& info){pid=info.pid;cookie=info.ptr;callback=true;});
 if(!ret.isOk()||!callback||pid<=0||!cookie)return 78;
 std::ifstream file("/sys/kernel/debug/binder/proc/"+std::to_string(pid));if(!file)return 78;
 std::string text;char chunk[4096];
 while(file.read(chunk,sizeof(chunk))||file.gcount()){text.append(chunk,static_cast<size_t>(file.gcount()));if(text.size()>1024*1024)return 78;}
 if(!file.eof())return 78;
 auto s=ParseHwbinderNode(text,pid,cookie);if(!s.valid)return 78;
 // Like lshal, exclude only this root-owned inspection process, which never initializes HCI.
 auto own=std::find(s.clients.begin(),s.clients.end(),getpid());
 if(own==s.clients.end())return 78; // Exact live node must show our temporary reference.
 s.clients.erase(own);
 std::printf("NGD_HCI_SNAPSHOT %d",pid);for(int client:s.clients)std::printf(" %d",client);std::printf("\n");
 return 0;
}
static std::string SnapshotCommand(bool* completed) {
  *completed = false;
  int fd[2];
  if (pipe2(fd, O_CLOEXEC) != 0) return {};
  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) { close(fd[0]); close(fd[1]); return {}; }
  int err = posix_spawn_file_actions_addclose(&actions, fd[0]);
  err |= posix_spawn_file_actions_adddup2(&actions, fd[1], STDOUT_FILENO);
  err |= posix_spawn_file_actions_adddup2(&actions, fd[1], STDERR_FILENO);
  err |= posix_spawn_file_actions_addclose(&actions, fd[1]);
  char arg0[] = "/proc/self/exe";
  char arg1[] = "--hci-snapshot";
  char* argv[] = {arg0, arg1, nullptr};
  pid_t child = -1;
  if (!err) err = posix_spawn(&child, arg0, &actions, nullptr, argv, environ);
  posix_spawn_file_actions_destroy(&actions);
  close(fd[1]);
  if (err) { close(fd[0]); return {}; }
  if (fcntl(fd[0], F_SETFL, O_NONBLOCK) == -1) {
    close(fd[0]); kill(child, SIGKILL); waitpid(child, nullptr, 0); return {};
  }
  const uint64_t start = NowMs();
  const uint64_t limit = start + 2000;
  std::string text;
  bool eof = false, exited = false, failed = !start;
  int status = 0;
  while (!failed) {
    char buf[8192];
    ssize_t n;
    while ((n = read(fd[0], buf, sizeof(buf))) > 0) {
      text.append(buf, static_cast<size_t>(n));
      if (text.size() > 1024 * 1024) { failed = true; break; }
    }
    if (n == 0) eof = true;
    if (n < 0 && errno != EAGAIN && errno != EINTR) failed = true;
    if (!exited) {
      const pid_t waited = waitpid(child, &status, WNOHANG);
      if (waited == child) exited = true;
      else if (waited < 0 && errno != EINTR) failed = true;
    }
    if (eof && exited) break;
    const uint64_t now = NowMs();
    if (!now || now >= limit) { failed = true; break; }
    struct pollfd poll_fd = {fd[0], POLLIN, 0};
    poll(&poll_fd, 1, static_cast<int>(std::min<uint64_t>(limit-now, 20)));
  }
  close(fd[0]);
  // Only our own bounded, read-only snapshot child may be reaped; never BLINK/HAL.
  if (!exited) { kill(child, SIGKILL); waitpid(child, &status, 0); }
  *completed = !failed && eof && exited && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  if(!*completed)std::fprintf(stderr,"NGD_HCI_READ_INCOMPLETE elapsed_ms=%llu failed=%d eof=%d exited=%d status=%d bytes=%zu\n",
    static_cast<unsigned long long>(NowMs()-start),failed,eof,exited,status,text.size());
  return text;
}
HciProbeResult ProbeHciForProcess(int expected_pid, bool admitted, bool barrier) {
  if (barrier && geteuid()!=0) return HciProbeResult{};
  char state[PROP_VALUE_MAX] = {};
  __system_property_get("init.svc.blink", state);
  bool completed = false;
  const std::string text = SnapshotCommand(&completed);
  HciSnapshot s;
  if(completed){
   std::istringstream in(text);std::string tag,value;
   if((in>>tag>>value)&&tag=="NGD_HCI_SNAPSHOT"&&PositivePid(value,&s.server_pid)){
    s.valid=true;
    while(in>>value){int pid=-1;if(!PositivePid(value,&pid)||std::find(s.clients.begin(),s.clients.end(),pid)!=s.clients.end()){s=HciSnapshot{};break;}s.clients.push_back(pid);}
   }
  }
  int manager = -1;
  for (int pid : s.clients) {
    if (Cmdline(pid) == "/system/bin/hwservicemanager") {
      if (manager > 0 && manager != pid) return HciProbeResult{};
      manager = pid;
    }
  }
  return EvaluateHci(s, barrier ? "stopped" : state, manager, expected_pid, admitted);
}
HciProbeResult ProbeHci(bool admitted) { return ProbeHciForProcess(getpid(),admitted,false); }
#else
int PrintDirectHciSnapshot() {return 77;}
HciProbeResult ProbeHciForProcess(int, bool, bool) { return HciProbeResult{}; }
HciProbeResult ProbeHci(bool) { return HciProbeResult{}; }
#endif
}  // namespace ngd
