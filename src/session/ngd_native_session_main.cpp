// Maintenance-only controller. No installation, remount, flash or automatic reboot.
#include "ngd_native_session.h"
#include "ngd14_host_heartbeat.h"
#include "ngd_verified_owner_cache.h"
#include "ngd_hci_probe.h"
#include "ngd_btdata_snapshot.h"
#ifdef __ANDROID__
#include "ngd12_artifact_digests.h"
#include <android/hardware/bluetooth/1.0/IBluetoothHci.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/system_properties.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <signal.h>
#include <sched.h>
#include <unistd.h>
#include <stddef.h>
#include <time.h>
#include <errno.h>
#include <atomic>
#include <thread>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
extern char** environ;
namespace {
constexpr const char* kBlink="/system/bin/blink";
constexpr const char* kHold="/data/local/ngd-native-bt/ngd_blink_hold12";
constexpr const char* kJournal="/data/local/ngd-native-bt/session.journal";
constexpr const char* kProcess="com.android.bluetooth.ngd";
constexpr const char* kFactorySha="e2ac7bf449db522d53d8c3c54769802541e92a4d4935bbe46482e93387374a7c";
constexpr const char* kFingerprint="qti/trinket/trinket:10/QKQ1.200816.002/eng.zouqiao.20250217.185927:userdebug/test-keys";
bool persistent=false;
bool ActivationEnabled() {
 struct stat st={};const char* p="/data/local/ngd-native-bt/enabled";
 return lstat(p,&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==0&&(st.st_mode&0777)==0600&&st.st_size==0;
}
volatile sig_atomic_t interrupted=0;
void Interrupt(int) { interrupted=1; }
uint64_t Now() { struct timespec t={}; if(clock_gettime(CLOCK_BOOTTIME,&t)) return 0; return uint64_t(t.tv_sec)*1000+t.tv_nsec/1000000; }
std::string Prop(const char* name) { char v[PROP_VALUE_MAX]={}; __system_property_get(name,v); return v; }
std::string Trim(std::string s) { while(!s.empty() && (s.back()=='\n'||s.back()=='\r'||s.back()==' ')) s.pop_back(); return s; }
std::string ReadFile(const std::string& path) { std::ifstream f(path); std::stringstream s; s<<f.rdbuf(); return s.str(); }
std::string Cmdline(int pid) { std::ifstream f("/proc/"+std::to_string(pid)+"/cmdline"); std::string n; std::getline(f,n,'\0'); return n; }
std::string Hash(const char* path) {
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW); if(fd<0) return {};
 SHA256_CTX ctx; SHA256_Init(&ctx); char buf[16384]; ssize_t n;
 while((n=read(fd,buf,sizeof(buf)))>0) SHA256_Update(&ctx,buf,n);
 close(fd); if(n<0) return {}; unsigned char digest[SHA256_DIGEST_LENGTH]; SHA256_Final(digest,&ctx);
 char hex[65]={}; for(unsigned i=0;i<32;++i) std::snprintf(hex+2*i,3,"%02x",digest[i]); return hex;
}
bool SameInode(const char* a,const char* b) { struct stat x={},y={};return stat(a,&x)==0 && stat(b,&y)==0 && x.st_dev==y.st_dev && x.st_ino==y.st_ino; }
bool InstallReadOnlyBlinkMask() {
 if(!SameInode(kBlink,kHold)&&mount(kHold,kBlink,nullptr,MS_BIND,nullptr))return false;
 // OEM init chmods /system/bin/blink to 0777 after the early bind.
 // Make only this file mount read-only so it cannot change the source inode.
 if(mount(nullptr,kBlink,nullptr,MS_BIND|MS_REMOUNT|MS_RDONLY|MS_NOSUID|MS_NODEV,nullptr))return false;
 struct statvfs flags={};
 return statvfs(kBlink,&flags)==0&&(flags.f_flag&ST_RDONLY)&&SameInode(kBlink,kHold);
}
struct Result { bool ok=false; std::string output; };
Result Run(const std::vector<std::string>& args,uint64_t limit_ms=3000) {
 Result r; int p[2]; if(args.empty()||pipe2(p,O_CLOEXEC)) return r;
 posix_spawn_file_actions_t acts; if(posix_spawn_file_actions_init(&acts)) {close(p[0]);close(p[1]);return r;}
 int e=posix_spawn_file_actions_addclose(&acts,p[0]);
 e|=posix_spawn_file_actions_addopen(&acts,STDIN_FILENO,"/dev/null",O_RDONLY,0);
 e|=posix_spawn_file_actions_adddup2(&acts,p[1],1);e|=posix_spawn_file_actions_adddup2(&acts,p[1],2);e|=posix_spawn_file_actions_addclose(&acts,p[1]);
 std::vector<char*> argv;for(const auto& a:args)argv.push_back(const_cast<char*>(a.c_str()));argv.push_back(nullptr);
 pid_t child=-1;if(!e)e=posix_spawn(&child,argv[0],&acts,nullptr,argv.data(),environ);
 posix_spawn_file_actions_destroy(&acts);close(p[1]);if(e){close(p[0]);return r;}
 fcntl(p[0],F_SETFL,O_NONBLOCK);const uint64_t deadline=Now()+limit_ms;int status=0;bool exited=false,eof=false,bad=false;
 while(!bad) {
  char b[8192];ssize_t n;while((n=read(p[0],b,sizeof(b)))>0){r.output.append(b,n);if(r.output.size()>1024*1024){bad=true;break;}}
  if(n==0)eof=true;if(n<0&&errno!=EAGAIN&&errno!=EINTR)bad=true;
  if(!exited){pid_t w=waitpid(child,&status,WNOHANG);if(w==child)exited=true;else if(w<0&&errno!=EINTR){exited=true;bad=true;}}
  if(exited&&eof)break;if(Now()>=deadline){bad=true;break;}struct pollfd q={p[0],POLLIN,0};poll(&q,1,20);
 }
 close(p[0]);if(!exited){kill(child,SIGKILL);waitpid(child,&status,0);}
 r.ok=!bad&&exited&&WIFEXITED(status)&&WEXITSTATUS(status)==0;return r;
}
std::vector<int> Processes(const std::string& name) {
 std::vector<int> p;DIR* d=opendir("/proc");if(!d)return {-1};struct dirent* entry;
 while((entry=readdir(d))) {char* end=nullptr;long pid=strtol(entry->d_name,&end,10);if(!end||*end||pid<=0)continue;if(Cmdline(pid)==name)p.push_back(pid);}
 closedir(d);return p;
}
bool OriginalGone(dev_t dev,ino_t ino) {
 DIR* d=opendir("/proc");if(!d)return false;bool gone=true;struct dirent* entry;
 while((entry=readdir(d))) {char* end=nullptr;long pid=strtol(entry->d_name,&end,10);if(!end||*end||pid<=0)continue;
  struct stat s={};std::string file="/proc/"+std::to_string(pid)+"/exe";
  if(stat(file.c_str(),&s)==0){if(s.st_dev==dev&&s.st_ino==ino)gone=false;}else if(errno!=ENOENT&&errno!=ESRCH)gone=false;
 }
 closedir(d);return gone;
}
bool MaskVisibleInAllExistingNamespaces() {
 struct stat hold={};if(stat(kHold,&hold))return false;
 DIR* d=opendir("/proc");if(!d)return false;bool ok=true;struct dirent* entry;
 while((entry=readdir(d))) {char* end=nullptr;long pid=strtol(entry->d_name,&end,10);if(!end||*end||pid<=0)continue;
  const std::string path="/proc/"+std::to_string(pid)+"/root/system/bin/blink";struct stat s={};
  // Use the captured stat: the process may exit before a second path lookup.
  if(stat(path.c_str(),&s)==0){
   if(s.st_dev!=hold.st_dev||s.st_ino!=hold.st_ino){
    std::printf("NGD_GUARD_NAMESPACE_MISMATCH pid=%ld dev=%llu ino=%llu expected_dev=%llu expected_ino=%llu\n",pid,
     static_cast<unsigned long long>(s.st_dev),static_cast<unsigned long long>(s.st_ino),
     static_cast<unsigned long long>(hold.st_dev),static_cast<unsigned long long>(hold.st_ino));ok=false;
   }
  }else if(errno!=ENOENT&&errno!=ESRCH){std::printf("NGD_GUARD_NAMESPACE_STAT_ERROR pid=%ld errno=%d\n",pid,errno);ok=false;}
 }
 closedir(d);return ok;
}
template<class F> bool Wait(F predicate,uint64_t ms) {uint64_t until=Now()+ms;do {if(predicate())return true;usleep(100000);}while(Now()<until);return false;}
struct Journal {unsigned long long dev=0,ino=0;int parent=0;std::string old_on,old_ble,data_sha;int hal=-1,mcu=-1;bool armed=false,data_restored=false;};
bool LoadJournal(Journal* j) {
 int fd=open(kJournal,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return false;struct stat s={};char b[1024]={};
 bool ok=fstat(fd,&s)==0&&s.st_uid==0&&(s.st_mode&0777)==0600&&S_ISREG(s.st_mode);ssize_t n=read(fd,b,sizeof(b)-1);close(fd);
 char on[8]={},ble[8]={};const int fields=sscanf(b,"%llu %llu %d %7s %7s",&j->dev,&j->ino,&j->parent,on,ble);j->old_on=on;j->old_ble=ble;
 const std::string text(b,n>0?n:0);size_t hash=text.find("DATA_SHA=");if(hash!=std::string::npos)j->data_sha=text.substr(hash+9,64);j->armed=text.find("ARMED\n")!=std::string::npos;j->data_restored=text.find("DATA_RESTORED\n")!=std::string::npos;
 size_t hp=text.find("HAL_PID=");if(hp!=std::string::npos)j->hal=atoi(text.c_str()+hp+8);
 size_t mp=text.find("MCU_PID=");if(mp!=std::string::npos)j->mcu=atoi(text.c_str()+mp+8);
 return ok&&n>0&&fields>=3;
}
bool AppendJournal(const std::string& text) {int fd=open(kJournal,O_WRONLY|O_APPEND|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return false;bool ok=write(fd,text.data(),text.size())==static_cast<ssize_t>(text.size())&&fsync(fd)==0;close(fd);return ok;}
bool Barrier(const Journal& j) {return SameInode(kBlink,kHold)&&Hash(kHold)==NGD12_HOLD_SHA&&MaskVisibleInAllExistingNamespaces()&&OriginalGone(j.dev,j.ino);}
ngd::HciProbeResult RootProbe(int pid,bool admitted,const Journal& j) {
 // This explicit override is root-only, after the executable maintenance barrier.
 return ngd::ProbeHciForProcess(pid,admitted,Barrier(j));
}
bool CloseInternal() {
 Journal j;if(geteuid()!=0||!LoadJournal(&j)||getppid()!=j.parent||!Barrier(j)||!Processes(kProcess).empty())return false;
 const auto p=RootProbe(-1,false,j);if(p.occupancy!=ngd::HciOccupancy::NoForeignClientObserved)return false;
 using android::hardware::bluetooth::V1_0::IBluetoothHci;
 const auto service=IBluetoothHci::getService("default");if(!service)return false;
 const auto returned=service->close();return returned.isOk(); // Declared HIDL API, no guessed C++ slots.
}
const char* OriginalApk() {
 const char* lower="/data/local/ngd-native-bt/view/oem/BluetoothExt.apk";
 struct stat st={}; return lstat(lower,&st)==0 ? lower : "/system/app/BluetoothExt/BluetoothExt.apk";
}
const char* OriginalBlink() {
 return SameInode(kBlink,kHold)?"/data/local/ngd-native-bt/view/oem-bin/blink":kBlink;
}
bool RecoveryReceipt(const char* path) {
 int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return false;struct stat s={};char b[4096]={};
 bool ok=fstat(fd,&s)==0&&s.st_uid==0&&(s.st_mode&0777)==0600&&S_ISREG(s.st_mode)&&s.st_size<4096;
 ssize_t n=read(fd,b,sizeof(b)-1);close(fd);std::string text(b,n>0?n:0);
 const std::string prefix="SNAPSHOT_SHA=";
 size_t pos=text.find(prefix);if(pos==std::string::npos)return false;
 const std::string sha=text.substr(pos+prefix.size(),64);
 return ok&&sha.size()==64&&Hash("/data/local/ngd-native-bt/oem.snapshot")==sha&&
  text.find(std::string("FINGERPRINT=")+kFingerprint+"\n")!=std::string::npos&&
  text.find("METHOD=RAM_BIND_SYSTEM_VIEW_NO_UNINSTALL\n")!=std::string::npos&&
  text.find("BOOT_ID="+Trim(ReadFile("/proc/sys/kernel/random/boot_id"))+"\n")!=std::string::npos&&
  Hash(OriginalApk())=="554671c357f5c81fbc4deec68dc60801a7b1e840722db328c1d1748e5b38033c";

}
bool UsbConnected() {
 if(persistent)return ActivationEnabled();
 DIR* d=opendir("/sys/class/udc");if(!d)return Trim(ReadFile("/sys/class/android_usb/android0/state"))=="CONFIGURED";bool configured=false;struct dirent* e;
 while((e=readdir(d))){if(e->d_name[0]=='.')continue;std::string state=Trim(ReadFile(std::string("/sys/class/udc/")+e->d_name+"/state"));if(state=="configured")configured=true;}
 closedir(d);return configured||Trim(ReadFile("/sys/class/android_usb/android0/state"))=="CONFIGURED";
}
bool PermissionBaseline(const char* mode) {
 if(Hash("/data/local/ngd-native-bt/permissions.jar")!="2694207165d54f4e84e219bc0e7ffc5ac459eca10bd00e30dbf25b4874c4491f")return false;
 return Run({"/system/bin/env","CLASSPATH=/data/local/ngd-native-bt/permissions.jar","/system/bin/app_process","/system/bin","Ngd15NativePermissions",mode},7000).ok;
}
bool RestoreFactoryUpdate() {
 // This variant never installs or uninstalls a PackageManager package.
 // The outer guardian removes the temporary view and restarts the framework.
 auto p=Run({"/system/bin/pm","path","org.codeaurora.bluetooth"});
 if(!p.ok||Trim(p.output)!="package:/system/app/BluetoothExt/BluetoothExt.apk")return false;
 auto pkg=Run({"/system/bin/dumpsys","package","org.codeaurora.bluetooth"});
 return pkg.ok&&pkg.output.find("userId=1002")!=std::string::npos&&
  Hash(OriginalApk())=="554671c357f5c81fbc4deec68dc60801a7b1e840722db328c1d1748e5b38033c";
}

bool CaptureBaseline(const char* receipt) {
 if(geteuid()!=0||Prop("ro.build.fingerprint")!=kFingerprint||Prop("init.svc.blink")!="running")return false;
 auto p=Run({"/system/bin/pm","path","org.codeaurora.bluetooth"});
 if(!p.ok||Trim(p.output)!="package:/system/app/BluetoothExt/BluetoothExt.apk"||!Processes(kProcess).empty())return false;
 if(Trim(Run({"/system/bin/settings","get","global","bluetooth_on"}).output)!="0")return false;
 auto users=Run({"/system/bin/pm","list","users"});auto up=users.output.find("UserInfo{");
 if(!users.ok||up==std::string::npos||users.output.find("UserInfo{0:")==std::string::npos||users.output.find("UserInfo{",up+1)!=std::string::npos)return false;
 auto pkg=Run({"/system/bin/dumpsys","package","org.codeaurora.bluetooth"});
 if(!pkg.ok||pkg.output.find("userId=1002")==std::string::npos||(pkg.output.find("enabled=0")==std::string::npos&&pkg.output.find("enabled=3")==std::string::npos))return false;

 ngd12::DataSnapshot data({"/data/misc/bluedroid","/data/misc/bluetooth","/data/user/0/org.codeaurora.bluetooth","/data/user_de/0/org.codeaurora.bluetooth"});
 if(!PermissionBaseline("capture")||!data.Capture()||!data.SaveExclusive("/data/local/ngd-native-bt/oem.snapshot"))return false;
 std::string text=std::string("METHOD=RAM_BIND_SYSTEM_VIEW_NO_UNINSTALL\nFINGERPRINT=")+kFingerprint+"\nBOOT_ID="+Trim(ReadFile("/proc/sys/kernel/random/boot_id"))+"\nSNAPSHOT_SHA="+Hash("/data/local/ngd-native-bt/oem.snapshot")+"\n";
 int fd=open(receipt,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return false;
 bool ok=write(fd,text.data(),text.size())==static_cast<ssize_t>(text.size())&&fsync(fd)==0;close(fd);
 printf("NGD14_BASELINE nodes=%zu saved=%d; no HCI/BLINK changes\n",data.Count(),ok);return ok&&RecoveryReceipt(receipt);
}
bool RollbackUpdateOnly(const char* receipt) {
 if(geteuid()!=0||!RecoveryReceipt(receipt)||!Processes(kProcess).empty()||
    Prop("init.svc.blink")!="running"||Hash(kBlink)!=kFactorySha||
    ReadFile("/proc/net/unix").find(ngd12::kSocketName)!=std::string::npos)return false;
 bool factory=RestoreFactoryUpdate();
 ngd12::DataSnapshot data({"/data/misc/bluedroid","/data/misc/bluetooth","/data/user/0/org.codeaurora.bluetooth","/data/user_de/0/org.codeaurora.bluetooth"});
 bool restored=data.LoadVerified("/data/local/ngd-native-bt/oem.snapshot")&&data.RestoreOnly({"/data/user/0/org.codeaurora.bluetooth","/data/user_de/0/org.codeaurora.bluetooth"});
 return factory&&restored;
}
bool RefreshAudioHal() {
 auto pid=[](const char* name){auto p=Run({"/system/bin/pidof",name},1500);auto text=Trim(p.output);
  return p.ok&&!text.empty()&&text.find_first_not_of("0123456789")==std::string::npos?text:std::string();};
 const std::string before=pid("android.hardware.audio@2.0-service");
 if(before.empty()||Prop("init.svc.vendor.audio-hal-2-0")!="running")return false;
 std::printf("NGD_AUDIO_HAL_RESTART begin old=%s\n",before.c_str());std::fflush(stdout);
 if(!Run({"/system/bin/setprop","ctl.restart","vendor.audio-hal-2-0"}).ok)return false;
 std::string stable_hal,stable_server;uint64_t stable_since=0;
 bool ok=Wait([&]{
  auto hal=pid("android.hardware.audio@2.0-service"),server=pid("audioserver");
  if(hal.empty()||server.empty()||hal==before||Prop("init.svc.vendor.audio-hal-2-0")!="running"||Prop("init.svc.audioserver")!="running"){stable_since=0;return false;}
  if(!stable_since||hal!=stable_hal||server!=stable_server){stable_hal=hal;stable_server=server;stable_since=Now();return false;}
  return Now()-stable_since>=2000;
 },25000);
 std::printf("NGD_AUDIO_HAL_RESTART %s hal=%s audioserver=%s\n",ok?"PASS":"FAIL",stable_hal.c_str(),stable_server.c_str());std::fflush(stdout);return ok;
}
class Runtime final : public ngd12::Operations {
 public:
  Runtime(std::string self,std::string receipt):self_(self),receipt_(receipt),deadline_(persistent?UINT64_MAX:Now()+120000){}
  std::atomic<bool> armed{false},abort{false},serving{true};std::atomic<int> admitted_pid{-1};
  Journal journal;int initial_hal=-1;
  uint64_t deadline()const{return deadline_;}
  bool Do(ngd12::Action action) override {
   if(action==ngd12::Action::Disarm)restoring_=true;
   bool ok=(restoring_||(!abort.load()&&!interrupted))&&Execute(action);std::printf("NGD14 %llu %s %s\n",static_cast<unsigned long long>(Now()),ngd12::ActionName(action),ok?"PASS":"FAIL");std::fflush(stdout);return ok;
  }
  bool Execute(ngd12::Action a) {
   using A=ngd12::Action;
   switch(a) {
    case A::Preflight: {
     if(preflight_done_)return true;
     if(Prop("persist.bluetooth.a2dp_offload.disabled")!=(persistent?"false":"true"))return false;
     if(persistent?Prop("persist.vendor.bt.a2dp.hal.implementation")!="true":!Prop("persist.vendor.bt.a2dp.hal.implementation").empty())return false;
     if(geteuid()!=0||!RecoveryReceipt(receipt_.c_str())||Prop("ro.build.fingerprint")!=kFingerprint||Prop("ro.build.version.sdk")!="29"||Trim(ReadFile("/sys/fs/selinux/enforce"))!="0"||!UsbConnected()){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}
     if(!SameInode("/proc/self/ns/mnt","/proc/1/ns/mnt")){int ns=open("/proc/1/ns/mnt",O_RDONLY|O_CLOEXEC);if(ns<0){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}int ret=setns(ns,CLONE_NEWNS);close(ns);if(ret){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}}
     if(Hash("/system/lib64/libbluetooth_qti.so")!="bde378e8fd1cde351a340dc4e75200512726bdce9a02dd2d1218bf17c75a9be6"||Hash(OriginalBlink())!=kFactorySha||Hash(kHold)!=NGD12_HOLD_SHA){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}
     auto pkg=Run({"/system/bin/dumpsys","package","org.codeaurora.bluetooth"});if(!pkg.ok||pkg.output.find("userId=1002")==std::string::npos||pkg.output.find("UPDATED_SYSTEM_APP")!=std::string::npos||pkg.output.find("SYSTEM")==std::string::npos||pkg.output.find("codePath=/system/app/BluetoothExt")==std::string::npos){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}
     auto path=Run({"/system/bin/pm","path","org.codeaurora.bluetooth"});auto apk=Trim(path.output);
     if(!path.ok||apk!="package:/system/app/BluetoothExt/BluetoothExt.apk"||apk.find('\n')!=std::string::npos||Hash(apk.substr(8).c_str())!=NGD12_APK_SHA){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}
     auto mcu=Processes("com.carocean.mcuserver");if(mcu.size()!=1){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}initial_mcu_=mcu[0];
     if(ReadFile("/proc/"+std::to_string(initial_mcu_)+"/cgroup").find("/uid_1000/pid_")==std::string::npos){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}
     if(Hash("/data/local/ngd-native-bt/permissions.jar")!="2694207165d54f4e84e219bc0e7ffc5ac459eca10bd00e30dbf25b4874c4491f"||!data_.LoadVerified("/data/local/ngd-native-bt/oem.snapshot")){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}data_saved_=true;
     if(Prop("init.svc.blink")!="running"&&!(Prop("init.svc.blink")=="stopped"&&SameInode(kBlink,kHold))){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}
     old_on_=Trim(Run({"/system/bin/settings","get","global","bluetooth_on"}).output);
     old_ble_=Trim(Run({"/system/bin/settings","get","global","ble_scan_always_enabled"}).output);
     if(old_on_!="0"||(old_ble_!="0"&&old_ble_!="1"&&old_ble_!="null")){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}
     auto p=ngd::ProbeHci(false);if(!p.snapshot.valid){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}initial_hal=p.snapshot.server_pid;
     struct stat s={};if(stat(OriginalBlink(),&s)){std::printf("NGD_PREFLIGHT_REJECT line=%d errno=%d\n",__LINE__,errno);std::fflush(stdout);return false;}journal.dev=s.st_dev;journal.ino=s.st_ino;journal.parent=getpid();journal.old_on=old_on_;journal.old_ble=old_ble_;
     preflight_done_=Now()<deadline_;return preflight_done_;
    }
    case A::MaskBlink: {
     int fd=open(kJournal,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return false;
     char text[128];int n=snprintf(text,sizeof(text),"%llu %llu %d %s %s\n",journal.dev,journal.ino,journal.parent,old_on_.c_str(),old_ble_.c_str());
     bool ok=write(fd,text,n)==n&&fsync(fd)==0;close(fd);journal_owned_=true;if(!ok||!AppendJournal("HAL_PID="+std::to_string(initial_hal)+"\nMCU_PID="+std::to_string(initial_mcu_)+"\n"))return false;
     if(!InstallReadOnlyBlinkMask())return false;masked_=true;
     // Verify that init observes the overmount; never change init.rc on disk.
     return SameInode("/proc/1/root/system/bin/blink",kHold)&&MaskVisibleInAllExistingNamespaces();
    }
    case A::DisableNgd: {
     Run({"/system/bin/svc","bluetooth","disable"});
     return Run({"/system/bin/am","force-stop","--user","0","org.codeaurora.bluetooth"}).ok&&Run({"/system/bin/pm","disable-user","--user","0","org.codeaurora.bluetooth"}).ok;
    }
    case A::BleScanOff:return Run({"/system/bin/settings","put","global","ble_scan_always_enabled","0"}).ok;
    case A::StopBlink:return Run({"/system/bin/setprop","ctl.stop","blink"}).ok&&Wait([&]{return Prop("init.svc.blink")=="stopped";},3000);
    case A::OriginalBlinkGone:return Wait([&]{return Barrier(journal);},3000);
    case A::CloseHci:return Wait([&]{
     auto result=Run({self_,"--internal-close-hci"},3000);
     if(!result.ok){std::printf("NGD16_CLOSE_REJECT child=%s\n",result.output.c_str());std::fflush(stdout);}
     return result.ok;
    },persistent?10000:0);
    case A::HciFree:return Wait([&]{auto p=RootProbe(-1,false,journal);
     bool free=p.snapshot.valid&&p.snapshot.server_pid==initial_hal&&p.occupancy==ngd::HciOccupancy::NoForeignClientObserved;
     if(!free){std::printf("NGD16_FREE_REJECT valid=%d server=%d expected=%d reason=%s\n",p.snapshot.valid,p.snapshot.server_pid,initial_hal,p.reason);std::fflush(stdout);}
     return free;
    },persistent?10000:0);
    case A::SnapshotData:{journal.data_sha=Hash("/data/local/ngd-native-bt/oem.snapshot");return data_saved_&&journal.data_sha.size()==64&&AppendJournal("DATA_SHA="+journal.data_sha+"\n");}
    case A::Arm:if(Now()+5000>=deadline_||abort.load()||interrupted||!AppendJournal("AUDIO_OFFLOAD_OLD=true\nAUDIO_HAL_IMPL_OLD=EMPTY\nAUDIO_HAL_REINIT=1\nARMED\n"))return false;
     if(!Run({"/system/bin/setprop","persist.bluetooth.a2dp_offload.disabled","false"}).ok||Prop("persist.bluetooth.a2dp_offload.disabled")!="false")return false;
     if(!Run({"/system/bin/setprop","persist.vendor.bt.a2dp.hal.implementation","true"}).ok||Prop("persist.vendor.bt.a2dp.hal.implementation")!="true")return false;
     audio_reinit_needed_=!persistent;
     if((!persistent&&!RefreshAudioHal())||Cmdline(initial_mcu_)!="com.carocean.mcuserver")return false;
     journal.armed=true;armed=true;return true;
    case A::EnableNgd: return Run({"/system/bin/pm","enable","--user","0","org.codeaurora.bluetooth"}).ok&&Run({"/system/bin/svc","bluetooth","enable"}).ok;
    case A::WaitAndroidOn:return Wait([&]{auto s=Run({"/system/bin/dumpsys","bluetooth_manager"},2000);return s.ok&&s.output.find("state: ON")!=std::string::npos&&admitted_pid.load()>0&&!abort.load();},30000);
    case A::Disarm:armed=false;return true;
    case A::NgdGone:return Wait([&]{return Processes(kProcess).empty();},4000);
    case A::RestoreData:return AppendJournal("NATIVE_CONFIG_KEPT\n");
    case A::UnmaskBlink:return SameInode(kBlink,kHold)&&Prop("init.svc.blink")=="stopped";
    case A::RestoreSettings:{
     if(persistent)return Run({"/system/bin/settings","put","global","bluetooth_on","0"}).ok;
     bool impl_restored=Run({"/system/bin/setprop","persist.vendor.bt.a2dp.hal.implementation",""}).ok&&Prop("persist.vendor.bt.a2dp.hal.implementation").empty();
     bool audio_restored=Run({"/system/bin/setprop","persist.bluetooth.a2dp_offload.disabled","true"}).ok&&Prop("persist.bluetooth.a2dp_offload.disabled")=="true";
     if(!impl_restored||!audio_restored)return false;
     if(audio_reinit_needed_){if(!RefreshAudioHal())return false;audio_reinit_needed_=false;}
     bool ok=Run({"/system/bin/settings","put","global","bluetooth_on",old_on_}).ok;
     if(old_ble_=="null")ok=Run({"/system/bin/settings","delete","global","ble_scan_always_enabled"}).ok&&ok;
     else ok=Run({"/system/bin/settings","put","global","ble_scan_always_enabled",old_ble_}).ok&&ok;
     return ok;
    }
    case A::StartBlink:return Prop("init.svc.blink")=="stopped";
    case A::OemOwner:return Cmdline(initial_mcu_)=="com.carocean.mcuserver"&&Processes(kProcess).empty()&&SameInode(kBlink,kHold);

   }
   return false;
  }
  void VerifyHciContinuously() {
   while(serving.load()) {
    int peer=admitted_pid.load();
    if(armed.load()&&peer>0) {
     if(persistent && Cmdline(peer)!=kProcess && Processes(kProcess).empty()) {
      auto idle=RootProbe(-1,false,journal);
      if(idle.snapshot.valid&&idle.snapshot.server_pid==initial_hal&&idle.occupancy==ngd::HciOccupancy::NoForeignClientObserved){admitted_pid=-1;owner_cache_.Update(-1,false,Now());usleep(100000);continue;}
     }
     auto p=RootProbe(peer,true,journal);
     bool ok=armed.load()&&p.snapshot.valid&&p.snapshot.server_pid==initial_hal&&p.occupancy==ngd::HciOccupancy::NoForeignClientObserved;
     owner_cache_.Update(peer,ok,Now());
     if(!ok&&armed.load()){std::printf("NGD15_HCI_VERIFIER_DENIED reason=%s valid=%d hal=%d expected=%d occupancy=%s\n",p.reason,p.snapshot.valid,p.snapshot.server_pid,initial_hal,ngd::OccupancyName(p.occupancy));std::fflush(stdout);abort=true;armed=false;}
    }
    for(int i=0;i<5&&serving.load();++i)usleep(20000);
   }
  }
  void Serve(int listener) {
   while(serving.load()) {
    if(armed.load()&&!UsbConnected()){abort=true;armed=false;}
    struct pollfd q={listener,POLLIN,0};if(poll(&q,1,100)<=0)continue;
    int fd=accept4(listener,nullptr,nullptr,SOCK_CLOEXEC);if(fd<0)continue;
    struct timeval tv={3,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
    struct ucred peer={};socklen_t size=sizeof(peer);char request[32]={};
    bool credentials=getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&size)==0;
    if(!credentials||(peer.uid!=0&&peer.uid!=1002)){send(fd,"DENY",4,MSG_NOSIGNAL);close(fd);continue;}
    ssize_t n=credentials?recv(fd,request,sizeof(request)-1,0):-1;
    if(n>0&&peer.uid==0&&std::strcmp(request,"RESTORE")==0){abort=true;send(fd,"RESTORING",9,MSG_NOSIGNAL);close(fd);continue;}
    const bool peer_process=Cmdline(peer.pid)==kProcess;
    const bool activation=UsbConnected();const bool mcu_same=Cmdline(initial_mcu_)=="com.carocean.mcuserver";
    const bool barrier=Barrier(journal);
    bool valid=n>0&&peer.uid==1002&&peer_process&&armed.load()&&Now()<deadline_&&activation&&mcu_same&&barrier;
    bool admitting=std::strcmp(request,"ADMIT")==0;bool checking=std::strcmp(request,"CHECK")==0;
    int admitted=admitted_pid.load();valid=valid&&(admitting||checking)&&(admitted<0?admitting:admitted==peer.pid);
    if(valid&&admitted<0) {
     auto p=RootProbe(peer.pid,false,journal);
     valid=p.snapshot.valid&&p.snapshot.server_pid==initial_hal&&p.occupancy==ngd::HciOccupancy::NoForeignClientObserved;
     owner_cache_.Update(peer.pid,valid,Now()); // Full live verification before first admission.
    } else if(valid&&!owner_cache_.Allows(peer.pid,Now())) {
     // A fresh full check may replace an expired cache, e.g. after suspend.
     // Never allow an old observation merely because the adapter used to work.
     auto p=RootProbe(peer.pid,true,journal);
     valid=p.snapshot.valid&&p.snapshot.server_pid==initial_hal&&p.occupancy==ngd::HciOccupancy::NoForeignClientObserved;
     owner_cache_.Update(peer.pid,valid,Now());
     std::printf("NGD16_CACHE_RECHECK result=%d reason=%s\n",valid,p.reason);std::fflush(stdout);
    }
    valid=valid&&armed.load()&&Now()<deadline_;
    if(valid){admitted_pid=peer.pid;char response[80];snprintf(response,sizeof(response),persistent?"NGD16_LEASE %llu":"NGD12_OK %llu",static_cast<unsigned long long>(persistent?Now()+5000:deadline_));send(fd,response,strlen(response),MSG_NOSIGNAL);}
    else {std::printf("NGD16_DENY pid=%d uid=%d admitted_pid=%d credentials=%d request=%s recv=%ld peer_process=%d barrier=%d armed=%d activation=%d mcu_same=%d cached=%d now=%llu deadline=%llu\n",peer.pid,peer.uid,admitted,credentials,request,static_cast<long>(n),peer_process,barrier,armed.load(),activation,mcu_same,owner_cache_.Allows(peer.pid,Now()),static_cast<unsigned long long>(Now()),static_cast<unsigned long long>(deadline_));std::fflush(stdout);send(fd,"DENY",4,MSG_NOSIGNAL);if(armed.load()&&credentials&&peer.uid==1002)abort=true;}
    close(fd);
   }
  }
 private:
  ngd15::VerifiedOwnerCache owner_cache_;
  bool audio_reinit_needed_=false;
  std::string self_,receipt_,old_on_,old_ble_;uint64_t deadline_;int initial_mcu_=-1;bool restoring_=false,masked_=false,journal_owned_=false,preflight_done_=false,data_saved_=false;
  ngd12::DataSnapshot data_{std::vector<std::string>{"/data/misc/bluedroid","/data/misc/bluetooth","/data/user/0/org.codeaurora.bluetooth","/data/user_de/0/org.codeaurora.bluetooth"}};
};
int Listener() {
 int fd=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0);if(fd<0)return -1;struct sockaddr_un a={};a.sun_family=AF_UNIX;
 memcpy(a.sun_path+1,ngd12::kSocketName,strlen(ngd12::kSocketName));
 if(bind(fd,reinterpret_cast<sockaddr*>(&a),offsetof(sockaddr_un,sun_path)+1+strlen(ngd12::kSocketName))||listen(fd,8)){close(fd);return -1;}return fd;
}
bool RecoveryCloseInternal() {
 Journal j;if(geteuid()!=0||!LoadJournal(&j)||getppid()!=j.parent||
  !SameInode(("/proc/"+std::to_string(getppid())+"/exe").c_str(),"/proc/self/exe")||
  Prop("init.svc.blink")!="stopped"||!OriginalGone(j.dev,j.ino)||!Processes(kProcess).empty())return false;
 auto p=ngd::ProbeHciForProcess(-1,false,true);
 if(!p.snapshot.valid||p.snapshot.server_pid!=j.hal||p.occupancy!=ngd::HciOccupancy::NoForeignClientObserved)return false;
 auto hci=android::hardware::bluetooth::V1_0::IBluetoothHci::getService("default");return hci&&hci->close().isOk();
}
bool RecoverFromJournal(const char* receipt,const std::string& self) {
 if(geteuid()!=0||!RecoveryReceipt(receipt)||Prop("ro.build.fingerprint")!=kFingerprint)return false;
 bool needs_audio_reinit=ReadFile(kJournal).find("AUDIO_HAL_REINIT=1\n")!=std::string::npos;
 Journal j;if(!LoadJournal(&j)||j.hal<=0||j.mcu<=0||j.old_on!="0"||
  (j.old_ble!="0"&&j.old_ble!="1"&&j.old_ble!="null"))return false;
 if(kill(j.parent,0)==0||errno!=ESRCH)return false;
 if(!SameInode("/proc/self/ns/mnt","/proc/1/ns/mnt")){int fd=open("/proc/1/ns/mnt",O_RDONLY|O_CLOEXEC);if(fd<0)return false;int e=setns(fd,CLONE_NEWNS);close(fd);if(e)return false;}
 bool masked=SameInode(kBlink,kHold);
 if((!masked&&Hash(kBlink)!=kFactorySha)||Hash(kHold)!=NGD12_HOLD_SHA)return false;
 // Disarm is implicit: the old broker is dead; no other broker may be present.
 if(ReadFile("/proc/net/unix").find(ngd12::kSocketName)!=std::string::npos)return false;
 Run({"/system/bin/svc","bluetooth","disable"});
 Run({"/system/bin/am","force-stop","--user","0","org.codeaurora.bluetooth"});
 Run({"/system/bin/pm","disable-user","--user","0","org.codeaurora.bluetooth"});
 if(!Wait([&]{return Processes(kProcess).empty();},4000))return false;
 bool impl_restored=persistent||(Run({"/system/bin/setprop","persist.vendor.bt.a2dp.hal.implementation",""}).ok&&Prop("persist.vendor.bt.a2dp.hal.implementation").empty());
 bool offload_restored=persistent||(Run({"/system/bin/setprop","persist.bluetooth.a2dp_offload.disabled","true"}).ok&&Prop("persist.bluetooth.a2dp_offload.disabled")=="true");
 if(!impl_restored||!offload_restored)return false;
 if(!persistent&&needs_audio_reinit&&!RefreshAudioHal())return false;
 const bool original_gone=OriginalGone(j.dev,j.ino);
 if(!original_gone) {
  // Do not close an HCI which is already owned by a live OEM process.
  auto p=ngd::ProbeHci(false);if(!p.snapshot.valid||p.snapshot.server_pid!=j.hal)return false;
  bool oem=false;
  for(int pid:p.snapshot.clients){
   if(Cmdline(pid)=="/system/bin/hwservicemanager")continue;
   struct stat st={};if(stat(("/proc/"+std::to_string(pid)+"/exe").c_str(),&st)||
    static_cast<unsigned long long>(st.st_dev)!=j.dev||static_cast<unsigned long long>(st.st_ino)!=j.ino)return false;
   oem=true;
  }
  if(!oem)return false;
 }
 std::string record=std::to_string(j.dev)+" "+std::to_string(j.ino)+" "+std::to_string(getpid())+" "+j.old_on+" "+j.old_ble+"\nHAL_PID="+std::to_string(j.hal)+"\nMCU_PID="+std::to_string(j.mcu)+"\n";
 if(j.armed)record+="ARMED\n";if(j.data_restored)record+="DATA_RESTORED\n";
 std::string next=std::string(kJournal)+".recover."+std::to_string(getpid());
 int fd=open(next.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return false;
 bool ok=write(fd,record.data(),record.size())==static_cast<ssize_t>(record.size())&&fsync(fd)==0;close(fd);if(!ok||rename(next.c_str(),kJournal))return false;
 if(original_gone) {
  if(!Run({"/system/bin/setprop","ctl.stop","blink"}).ok||!Wait([&]{return Prop("init.svc.blink")=="stopped";},3000))return false;
  if(!Wait([&]{return Run({self,"--recovery-close-hci"},3000).ok;},persistent?10000:0))return false;
  if(!Wait([&]{auto p=ngd::ProbeHciForProcess(-1,false,true);
   return p.snapshot.valid&&p.snapshot.server_pid==j.hal&&p.occupancy==ngd::HciOccupancy::NoForeignClientObserved;
  },persistent?10000:0))return false;
 }
 // Deliberately keep OEM BLINK stopped and its executable masked.
 // A failed/expired native trial must not silently switch back to OEM Bluetooth.
 if(!masked||!SameInode(kBlink,kHold)||Prop("init.svc.blink")!="stopped")return false;
 bool settings=Run({"/system/bin/settings","put","global","bluetooth_on","0"}).ok;
 bool mcu=Cmdline(j.mcu)=="com.carocean.mcuserver";
 if(settings&&mcu)unlink(kJournal);
 printf("NGD15_NATIVE_RECOVERY own_hci_closed=1 OEM_kept_stopped=1 MCU_same=%d\n",mcu);
 return settings&&mcu;
}

bool SafeRecover(const char* receipt,const std::string& self) {
 struct stat st={};
 if(lstat(kJournal,&st)==0)return RecoverFromJournal(receipt,self);
 if(errno!=ENOENT)return false;
 // No journal means no BLINK mask/stop/admission was reached.
 auto path=Run({"/system/bin/pm","path","org.codeaurora.bluetooth"});
 if(path.ok&&Trim(path.output)=="package:/system/app/BluetoothExt/BluetoothExt.apk"&&Hash(Trim(path.output).substr(8).c_str())==NGD12_APK_SHA)
  Run({"/system/bin/am","force-stop","--user","0","org.codeaurora.bluetooth"});
 return RecoveryReceipt(receipt); // No journal means no native HCI admission; do not restore OEM data.
}

// Detached staging only. QTI admission is not available in this code path.
int StageNativeSystem() {
 const char* dir="/data/local/ngd-native-bt/view";
 const char* lower="/data/local/ngd-native-bt/view/oem";
 const char* target="/system/app/BluetoothExt";
 const char* candidate="/data/local/ngd-native-bt/view/candidate";
 if(geteuid()!=0||Prop("ro.build.fingerprint")!=kFingerprint||
  Trim(ReadFile("/sys/fs/selinux/enforce"))!="0")return 77;
 if(!SameInode("/proc/self/ns/mnt","/proc/1/ns/mnt")) {
  int fd=open("/proc/1/ns/mnt",O_RDONLY|O_CLOEXEC);if(fd<0)return 77;
  int e=setns(fd,CLONE_NEWNS);close(fd);if(e)return 77;
 }
 struct stat st={};
 if(lstat(dir,&st)||st.st_uid!=0||(st.st_mode&0777)!=0700||!S_ISDIR(st.st_mode))return 77;
 if(Hash("/system/app/BluetoothExt/BluetoothExt.apk")!=
  "554671c357f5c81fbc4deec68dc60801a7b1e840722db328c1d1748e5b38033c"||
  Hash("/data/local/ngd-native-bt/view/candidate/BluetoothExt.apk")!=NGD12_APK_SHA||
  Hash("/data/local/ngd-native-bt/view/candidate/lib/arm64/libbluetooth_jni_ngd.so")!=NGD12_JNI_SHA)return 78;
 if(mount(target,lower,nullptr,MS_BIND,nullptr))return 78;
 // The OEM alias MUST NOT remain a peer of the system shared mount.
 // Otherwise the replacement propagates over the supposed OEM alias as well.
 if(mount(nullptr,lower,nullptr,MS_PRIVATE,nullptr))return 78;
 // Preserve an unmasked OEM executable alias for subsequent held sessions.
 if(mount("/system/bin","/data/local/ngd-native-bt/view/oem-bin",nullptr,MS_BIND,nullptr)||
    mount(nullptr,"/data/local/ngd-native-bt/view/oem-bin",nullptr,MS_PRIVATE,nullptr))return 79;
 if(Hash("/data/local/ngd-native-bt/view/oem-bin/blink")!=kFactorySha)return 79;
 if(!CaptureBaseline("/data/local/ngd-native-bt/boot.receipt"))return 79;
 if(!Run({"/system/bin/setprop","pm.boot.disable_package_cache","true"}).ok)return 79;
 if(mount(candidate,target,nullptr,MS_BIND,nullptr))return 79;
 if(Hash(OriginalApk())!="554671c357f5c81fbc4deec68dc60801a7b1e840722db328c1d1748e5b38033c"||
  Hash("/system/app/BluetoothExt/BluetoothExt.apk")!=NGD12_APK_SHA)return 79;
 if(!Run({"/system/bin/setprop","ctl.restart","zygote"}).ok)return 79;
 bool ready=Wait([&]{
  auto p=Run({"/system/bin/dumpsys","package","org.codeaurora.bluetooth"},2000);
  return p.ok&&p.output.find("versionCode=140014 ")!=std::string::npos&&
   p.output.find("UPDATED_SYSTEM_APP")==std::string::npos&&
   p.output.find("codePath=/system/app/BluetoothExt")!=std::string::npos&&
   Processes("com.carocean.mcuserver").size()==1;
 },70000);
 printf("NGD15_NATIVE_VIEW_%s; QTI gate remains closed; no OEM restoration requested\n",ready?"READY":"FAILED");fflush(stdout);
 return ready?0:79;
}
int StartDetachedNativeStage() {
 int sync[2];if(pipe2(sync,O_CLOEXEC))return 71;
 pid_t child=fork();if(child<0){close(sync[0]);close(sync[1]);return 71;}
 if(child>0) {
  close(sync[1]);char ok=0;ssize_t n=read(sync[0],&ok,1);close(sync[0]);
  if(n!=1||ok!='1')return 71;
  printf("NGD15_NATIVE_STAGE_DETACHED pid=%d\n",child);fflush(stdout);return 0;
 }
 close(sync[0]);signal(SIGHUP,SIG_IGN);signal(SIGPIPE,SIG_IGN);
 if(setsid()<0)_exit(71);
 int log=open("/data/local/ngd-native-bt/view/stage.log",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
 int nullfd=open("/dev/null",O_RDONLY);
 if(log<0||nullfd<0)_exit(71);
 if(dup2(log,1)<0||dup2(log,2)<0||dup2(nullfd,0)<0)_exit(71);
 close(log);close(nullfd);char ok='1';write(sync[1],&ok,1);close(sync[1]);
 int result=StageNativeSystem();printf("NGD15_STAGE_EXIT=%d\n",result);fflush(stdout);_exit(result);
}


bool TrustedTree(const std::string& path) {
 struct stat st={};if(lstat(path.c_str(),&st)||st.st_uid!=0||(st.st_mode&0022))return false;
 if(S_ISREG(st.st_mode))return st.st_nlink==1;
 if(!S_ISDIR(st.st_mode))return false;
 DIR* dir=opendir(path.c_str());if(!dir)return false;bool ok=true;struct dirent* e;
 while((e=readdir(dir))) {
  if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
  if(!TrustedTree(path+"/"+e->d_name)){ok=false;break;}
 }
 closedir(dir);return ok;
}
// NGD16_SETTINGS_INTEGRATION: one-byte OEM Settings visibility fix, original preserved.
bool TrustedPayload() {
 struct stat st={};const char* root="/data/local/ngd-native-bt";
 if(lstat(root,&st)||!S_ISDIR(st.st_mode)||st.st_uid!=0||(st.st_mode&0777)!=0700)return false;
 return TrustedTree(std::string(root)+"/Settings.bluetooth-visible.apk")&&TrustedTree(std::string(root)+"/view/candidate")&&
  TrustedTree(std::string(root)+"/ngd_blink_hold12")&&
  TrustedTree(std::string(root)+"/ngd16_native_bt")&&
  TrustedTree(std::string(root)+"/permissions.jar")&&
  TrustedTree(std::string(root)+"/oem.snapshot");
}
bool ArchivePreviousBootJournal() {
 if(access(kJournal,F_OK)!=0)return errno==ENOENT;
 const char* receipt="/data/local/ngd-native-bt/boot.receipt";
 struct stat a={},b={};
 if(lstat(kJournal,&a)||lstat(receipt,&b)||!S_ISREG(a.st_mode)||!S_ISREG(b.st_mode)||
    a.st_uid!=0||b.st_uid!=0||(a.st_mode&0777)!=0600||(b.st_mode&0777)!=0600||b.st_size>4096)return false;
 std::string text=ReadFile(receipt);auto pos=text.find("BOOT_ID=");if(pos==std::string::npos)return false;
 std::string boot=text.substr(pos+8,36);
 if(boot.size()!=36||boot.find_first_not_of("0123456789abcdef-")!=std::string::npos||
    boot==Trim(ReadFile("/proc/sys/kernel/random/boot_id")))return false;
 std::string archive=std::string(kJournal)+".previous-"+boot;
 if(access(archive.c_str(),F_OK)==0)return false;
 return rename(kJournal,archive.c_str())==0;
}
int BootPrepare() {
 fprintf(stderr,"BOOT_PREPARE phase=preflight\n");
 if(geteuid()!=0||!ActivationEnabled()||Prop("ro.build.fingerprint")!=kFingerprint||
    Prop("ro.build.version.sdk")!="29"||Trim(ReadFile("/sys/fs/selinux/enforce"))!="0")return 77;
 if(!SameInode("/proc/self/ns/mnt","/proc/1/ns/mnt")||!TrustedPayload())return 77;
 fprintf(stderr,"BOOT_PREPARE phase=payload_verified\n");
 const char* target="/system/app/BluetoothExt";
 const char* lower="/data/local/ngd-native-bt/view/oem";
 const char* bin="/data/local/ngd-native-bt/view/oem-bin";
 const char* candidate="/data/local/ngd-native-bt/view/candidate";
 // Idempotence only for the fully prepared current boot, never arbitrary mounts.
 if(SameInode(kBlink,kHold)&&Hash("/system/app/BluetoothExt/BluetoothExt.apk")==NGD12_APK_SHA&&Hash("/product/priv-app/Settings/Settings.apk")=="fed58fe187827f26ee54b618f5f0dbc88f28d5cfc1fa1986bf07208a3569b927")
  return Prop("ngd.native.boot_ready")=="1"?0:79;
 if(Hash(kBlink)!=kFactorySha||Hash(kHold)!=NGD12_HOLD_SHA||
    Hash("/system/app/BluetoothExt/BluetoothExt.apk")!="554671c357f5c81fbc4deec68dc60801a7b1e840722db328c1d1748e5b38033c"||
    Hash("/data/local/ngd-native-bt/view/candidate/BluetoothExt.apk")!=NGD12_APK_SHA||
    Hash("/data/local/ngd-native-bt/view/candidate/lib/arm64/libbluetooth_jni_ngd.so")!=NGD12_JNI_SHA||
    Hash("/system/lib64/libbluetooth_qti.so")!="bde378e8fd1cde351a340dc4e75200512726bdce9a02dd2d1218bf17c75a9be6")return 78;
 // Audio properties must already be loaded from /data/property before the HAL starts.
 // Refuse a half-configured installation instead of restarting audio during boot.
 if(Prop("persist.bluetooth.a2dp_offload.disabled")!="false"||Prop("persist.vendor.bt.a2dp.hal.implementation")!="true")return 78;
 if(Hash("/product/priv-app/Settings/Settings.apk")!="46ea77f8832d248d9c0ed695016d2ecd56caf994fd31946f845d3cc94aec95e6"||
    Hash("/data/local/ngd-native-bt/Settings.bluetooth-visible.apk")!="fed58fe187827f26ee54b618f5f0dbc88f28d5cfc1fa1986bf07208a3569b927")return 78;
 if(!ArchivePreviousBootJournal())return 79;
 fprintf(stderr,"BOOT_PREPARE phase=factory_verified\n");
 // Recreate private mount points on every boot; empty staging directories
 // are not a durable prerequisite. Reject symlinks and writable foreign paths.
 for(const char* path:{lower,bin,"/data/local/ngd-native-bt/view/oem-settings"}) {
  if(mkdir(path,0700)&&errno!=EEXIST)return 79;
  struct stat st={};
  if(lstat(path,&st)||!S_ISDIR(st.st_mode)||st.st_uid!=0||(st.st_mode&0022))return 79;
 }
 fprintf(stderr,"BOOT_PREPARE phase=mountpoints_ready\n");
 if(mount("/product/priv-app/Settings","/data/local/ngd-native-bt/view/oem-settings",nullptr,MS_BIND,nullptr)||
    mount(nullptr,"/data/local/ngd-native-bt/view/oem-settings",nullptr,MS_PRIVATE,nullptr))return 79;
 if(Hash("/data/local/ngd-native-bt/view/oem-settings/Settings.apk")!="46ea77f8832d248d9c0ed695016d2ecd56caf994fd31946f845d3cc94aec95e6")return 79;
 if(mount(target,lower,nullptr,MS_BIND,nullptr)||mount(nullptr,lower,nullptr,MS_PRIVATE,nullptr)||
    mount("/system/bin",bin,nullptr,MS_BIND,nullptr)||mount(nullptr,bin,nullptr,MS_PRIVATE,nullptr))return 79;
 if(Hash(OriginalApk())!="554671c357f5c81fbc4deec68dc60801a7b1e840722db328c1d1748e5b38033c"||Hash(OriginalBlink())!=kFactorySha)return 79;
 fprintf(stderr,"BOOT_PREPARE phase=original_aliases_verified\n");
 // First establish the barrier; this does not open HCI.
 if(!InstallReadOnlyBlinkMask()||!SameInode("/proc/1/root/system/bin/blink",kHold))return 79;
 if(mount(candidate,target,nullptr,MS_BIND,nullptr)||Hash("/system/app/BluetoothExt/BluetoothExt.apk")!=NGD12_APK_SHA)return 79;
 if(mount("/data/local/ngd-native-bt/Settings.bluetooth-visible.apk","/product/priv-app/Settings/Settings.apk",nullptr,MS_BIND,nullptr)||
    Hash("/product/priv-app/Settings/Settings.apk")!="fed58fe187827f26ee54b618f5f0dbc88f28d5cfc1fa1986bf07208a3569b927"||
    Hash("/data/local/ngd-native-bt/view/oem-settings/Settings.apk")!="46ea77f8832d248d9c0ed695016d2ecd56caf994fd31946f845d3cc94aec95e6")return 79;
 if(!Run({"/system/bin/setprop","pm.boot.disable_package_cache","true"}).ok)return 79;
 fprintf(stderr,"BOOT_PREPARE phase=views_verified\n");
 return Run({"/system/bin/setprop","ngd.native.boot_ready","1"}).ok?0:79;
}
int BootPrepareLogged() {
 struct stat st={};
 if(geteuid()!=0||lstat("/data/local/ngd-native-bt",&st)||!S_ISDIR(st.st_mode)||
    st.st_uid!=0||(st.st_mode&0777)!=0700)return 77;
 int fd=open("/data/local/ngd-native-bt/boot-prepare.log",O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW|O_CLOEXEC,0600);
 if(fd<0)return 77;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=0||(st.st_mode&0777)!=0600||st.st_nlink!=1){close(fd);return 77;}
 if(dup2(fd,STDERR_FILENO)<0){close(fd);return 77;}
 fprintf(stderr,"BOOT_PREPARE boot=%s\n",Trim(ReadFile("/proc/sys/kernel/random/boot_id")).c_str());
 errno=0;int result=BootPrepare();int saved_errno=errno;
 fprintf(stderr,"BOOT_PREPARE result=%d last_errno=%d (errno may be stale; phase is authoritative)\n",result,saved_errno);
 fflush(stderr);fsync(fd);close(fd);return result;
}
bool PersistentReceipt() {
 const char* path="/data/local/ngd-native-bt/boot.receipt";
 // Stale journals require explicit recovery; never silently delete them.
 if(access(kJournal,F_OK)==0)return false;
 if(!Run({"/system/bin/svc","bluetooth","disable"}).ok||
    !Run({"/system/bin/am","force-stop","--user","0","org.codeaurora.bluetooth"}).ok||
    !Run({"/system/bin/pm","disable-user","--user","0","org.codeaurora.bluetooth"}).ok||
    !Run({"/system/bin/settings","put","global","bluetooth_on","0"}).ok||!Wait([&]{return Processes(kProcess).empty();},4000))return false;
 const std::string sha=Hash("/data/local/ngd-native-bt/oem.snapshot");
 if(sha.size()!=64||Hash(OriginalApk())!="554671c357f5c81fbc4deec68dc60801a7b1e840722db328c1d1748e5b38033c")return false;
 const std::string text=std::string("METHOD=RAM_BIND_SYSTEM_VIEW_NO_UNINSTALL\nFINGERPRINT=")+kFingerprint+"\nBOOT_ID="+Trim(ReadFile("/proc/sys/kernel/random/boot_id"))+"\nSNAPSHOT_SHA="+sha+"\n";
 // Only this root-owned receipt is regenerated for the new boot. OEM data is never restored here.
 int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return false;
 struct stat st={};bool ok=fstat(fd,&st)==0&&st.st_uid==0&&(st.st_mode&0777)==0600&&S_ISREG(st.st_mode);
 ok=ok&&write(fd,text.data(),text.size())==static_cast<ssize_t>(text.size())&&fsync(fd)==0;close(fd);return ok&&RecoveryReceipt(path);
}

} // namespace
int main(int argc,char** argv) {
 if(argc==2&&!strcmp(argv[1],"--print-target")){std::puts("NGD_TARGET_FINGERPRINT=qti/trinket/trinket:10/QKQ1.200816.002/eng.zouqiao.20250217.185927:userdebug/test-keys");return 0;}
 signal(SIGPIPE,SIG_IGN);
 persistent=getenv("NGD16_PERSISTENT") && !strcmp(getenv("NGD16_PERSISTENT"),"1");
 if(argc==2&&!strcmp(argv[1],"--hci-snapshot"))return ngd::PrintDirectHciSnapshot();
 if(argc==2&&!strcmp(argv[1],"--verify-payload"))return geteuid()==0&&TrustedPayload()?0:77;
 if(argc==2&&!strcmp(argv[1],"--boot-prepare"))return BootPrepareLogged();
 if(argc==2&&!strcmp(argv[1],"--persistent")) {
  if(geteuid()!=0||!ActivationEnabled()||Prop("sys.boot_completed")!="1"||Prop("ngd.native.boot_ready")!="1")return 77;
  int log=open("/data/local/ngd-native-bt/runtime.log",O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW|O_CLOEXEC,0600);
  struct stat st={};
  if(log<0||fstat(log,&st)||st.st_uid!=0||(st.st_mode&0777)!=0600||!S_ISREG(st.st_mode)||st.st_nlink!=1)return 77;
  if(dup2(log,STDOUT_FILENO)<0||dup2(log,STDERR_FILENO)<0){close(log);return 77;}close(log);
  persistent=true;setenv("NGD16_PERSISTENT","1",1);
  if(!Wait([&]{return Processes("com.carocean.mcuserver").size()==1&&
    Prop("init.svc.vendor.audio-hal-2-0")=="running"&&Prop("init.svc.audioserver")=="running";},30000))return 79;
  if(!PersistentReceipt())return 79;
  static char a0[]="ngd16";static char a1[]="--maintenance-accept-whole-blink-pause";
  static char a2[]="--recovery-receipt";static char a3[]="/data/local/ngd-native-bt/boot.receipt";
  static char* args[]={a0,a1,a2,a3,nullptr};argc=4;argv=args;
 }
 if(argc==2&&!strcmp(argv[1],"--prepare-held-resume")) {
  if(geteuid()!=0||Prop("init.svc.blink")!="stopped"||!SameInode(kBlink,kHold)||Hash(kHold)!=NGD12_HOLD_SHA||!Processes(kProcess).empty()||ReadFile("/proc/net/unix").find(ngd12::kSocketName)!=std::string::npos)return 77;
  if(!SameInode("/proc/self/ns/mnt","/proc/1/ns/mnt")) {int fd=open("/proc/1/ns/mnt",O_RDONLY|O_CLOEXEC);if(fd<0)return 77;int e=setns(fd,CLONE_NEWNS);close(fd);if(e)return 77;}
  const char* path="/data/local/ngd-native-bt/view/oem-bin";
  if(mkdir(path,0700))return 78;
  if(mount("/system/bin",path,nullptr,MS_BIND,nullptr)||mount(nullptr,path,nullptr,MS_PRIVATE,nullptr))return 78;
  if(Hash(OriginalBlink())!=kFactorySha)return 78;
  std::puts("HELD_RESUME_PREPARED: OEM executable remains masked and service stopped; no HCI initialization");return 0;
 }
 if(argc==3&&!strcmp(argv[1],"--stage-native-system")&&!strcmp(argv[2],"--accept-framework-mcu-restart"))return StartDetachedNativeStage();
 if(argc==3&&strcmp(argv[1],"--capture-oem")==0)return CaptureBaseline(argv[2])?0:79;
 if(argc==4&&!strcmp(argv[1],"--rollback-system-view")&&!strcmp(argv[2],"--recovery-receipt"))return RollbackUpdateOnly(argv[3])?0:90;
 if(argc==2&&strcmp(argv[1],"--restore-now")==0)return ngd12::RequestRootRestore()?0:20;
 if(argc==2&&!strcmp(argv[1],"--recovery-close-hci"))return RecoveryCloseInternal()?0:30;
 if(argc==2&&strcmp(argv[1],"--internal-close-hci")==0)return CloseInternal()?0:30;
 if(argc==4&&strcmp(argv[1],"--recover-from-journal")==0&&strcmp(argv[2],"--recovery-receipt")==0) {
  char self[4096]={};ssize_t n=readlink("/proc/self/exe",self,sizeof(self)-1);if(n<=0)return 70;self[n]=0;
  return RecoverFromJournal(argv[3],self)?0:90;
 }
 if(argc!=4||strcmp(argv[1],"--maintenance-accept-whole-blink-pause")||strcmp(argv[2],"--recovery-receipt")) {
  fprintf(stderr,"No default actions. Usage: ngd_native_session --maintenance-accept-whole-blink-pause --recovery-receipt ROOT_0600_RECEIPT\n");return 64;
 }
 if(geteuid()!=0)return 77;
 signal(SIGHUP,SIG_IGN);signal(SIGINT,Interrupt);signal(SIGTERM,Interrupt);
 char self[4096]={};ssize_t n=readlink("/proc/self/exe",self,sizeof(self)-1);if(n<=0)return 70;self[n]=0;
 if(getenv("NGD14_SESSION_WORKER")==nullptr) {
  pid_t worker=fork();if(worker<0)return 71;
  if(worker==0){if(setpgid(0,0))_exit(71);setenv("NGD14_SESSION_WORKER","1",1);execv(self,argv);_exit(72);}
  if(setpgid(worker,worker)&&getpgid(worker)!=worker){kill(worker,SIGKILL);waitpid(worker,nullptr,0);return 71;}
  uint64_t until=persistent?UINT64_MAX:Now()+180000;int status=0;ngd14::HostHeartbeat heartbeat(Now());
  fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK);
  while(Now()<until){
   if(!persistent) {char input[128];ssize_t count=read(STDIN_FILENO,input,sizeof(input));
   if(count==0||(count<0&&errno!=EAGAIN&&errno!=EINTR)||
    (count>0&&!heartbeat.Feed(input,static_cast<size_t>(count),Now()))||!heartbeat.Alive(Now())) {
     printf("NGD14_STOP host heartbeat lost; closing native Bluetooth; OEM remains stopped\n");fflush(stdout);break;
   }
   }
   if(interrupted|| (persistent&&!ActivationEnabled()))break;
   pid_t done=waitpid(worker,&status,WNOHANG);if(done==worker){
    kill(-worker,SIGKILL); // Reap every remaining utility child before any restoration command.
    if(WIFEXITED(status)&&(WEXITSTATUS(status)==0||WEXITSTATUS(status)==10))return WEXITSTATUS(status);
    return SafeRecover(argv[3],self)?10:90;
   }if(done<0)return 90;usleep(100000);}
  kill(-worker,SIGKILL);waitpid(worker,&status,0);
  return SafeRecover(argv[3],self)?10:90;
 }
 if(!SameInode(("/proc/"+std::to_string(getppid())+"/exe").c_str(),"/proc/self/exe"))return 77;
 Runtime ops(self,argv[3]);
 // Entering init's mount namespace must happen before creating threads.
 // During boot PackageManager/lshal may still be settling. Retry only the
 // read-only preflight, with all original checks, before any broker/HCI action.
 bool preflight=false;
 const uint64_t readiness_deadline=Now()+(persistent?30000:0);
 do {
  if(interrupted||(persistent&&!ActivationEnabled()))break;
  if(ops.Do(ngd12::Action::Preflight)){preflight=true;break;}
  if(!persistent||Now()>=readiness_deadline)break;
  usleep(500000);
 }while(Now()<readiness_deadline);
 if(!preflight)return 78;
 int listener=Listener();if(listener<0)return 73;
 std::thread broker([&]{ops.Serve(listener);});
 std::thread verifier([&]{ops.VerifyHciContinuously();});
 ngd12::Transaction transaction(ops);
 bool prepared=transaction.Prepare();
 if(prepared)while(Now()<ops.deadline()&&!ops.abort.load()&&!interrupted)usleep(100000);
 ops.armed=false;
 bool restored=transaction.Restore();
 if(restored){Journal completed;if(LoadJournal(&completed)&&completed.parent==getpid()&&unlink(kJournal))restored=false;}
 ops.serving=false;broker.join();verifier.join();close(listener);
 printf("NGD14_RESULT prepared=%s native_closed_oem_kept_stopped=%s hardware_audio_test=UNVERIFIED\n",prepared?"true":"false",restored?"true":"false");
 return restored?(prepared?0:10):90;
}
#else
#include <cstdio>
int main(){std::puts("Android maintenance actions unavailable on this host");return 77;}
#endif
