// Separately approved init installation/removal only. Never reboots or opens HCI.
#ifdef __ANDROID__
#include <sys/ioctl.h>
#include <sys/xattr.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <linux/fs.h>
#include <sys/system_properties.h>
#include <sched.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <string>

static bool remove_mode=false;
static const char* kFinal="/system/etc/init/ngd-native-bt.rc";
static const char* kText=R"NGD16(# Candidate only. Not installed. Target: the verified BLINK_6125 Android 10 build.
# Root init.rc loads persistent properties in its earlier action for this event.
# exec_start completes before the queued zygote-start and boot/class_start actions.
on load_persist_props_action
    exec_start ngd-bt-prepare

# Keep OEM Bluetooth stopped even if the native payload fails validation.
# This is intentionally disruptive to the whole BLINK service. Installation
# requires explicit approval; MCU is a separate Android application.
on boot
    stop blink

on property:init.svc.blink=running
    stop blink

service ngd-bt-prepare /data/local/ngd-native-bt/ngd16_native_bt --boot-prepare
    user root
    group root
    seclabel u:r:su:s0
    disabled
    oneshot
    timeout_period 20

on property:sys.boot_completed=1 && property:ngd.native.boot_ready=1
    start ngd-native-bt

service ngd-native-bt /data/local/ngd-native-bt/ngd16_native_bt --persistent
    user root
    group root
    seclabel u:r:su:s0
    disabled
    oneshot
    # No restart loop, no laptop heartbeat, no session time limit.
)NGD16";
static const char* kReserve="/sys/fs/ext4/dm-3/reserved_clusters";
static std::string Read(const char* p) {
 int fd=open(p,O_RDONLY|O_CLOEXEC);if(fd<0)return {};char b[256]={};ssize_t n=read(fd,b,sizeof(b)-1);close(fd);
 std::string s=n>0?std::string(b,n):std::string();while(!s.empty()&&s.back()=='\n')s.pop_back();return s;
}
static uint64_t Now(){timespec t={};if(clock_gettime(CLOCK_BOOTTIME,&t))return 0;return uint64_t(t.tv_sec)*1000+t.tv_nsec/1000000;}
static bool Same(const char* a,const char* b){struct stat x={},y={};return !stat(a,&x)&&!stat(b,&y)&&x.st_dev==y.st_dev&&x.st_ino==y.st_ino;}
static bool Reserve(const char* value) {
 int fd=open(kReserve,O_WRONLY|O_CLOEXEC);if(fd<0)return false;
 bool ok=write(fd,value,strlen(value))==static_cast<ssize_t>(strlen(value));close(fd);
 return ok&&Read(kReserve)==value;
}

static std::string ReadAll(const char* path) {
 int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return {};
 char data[8192];ssize_t n=read(fd,data,sizeof(data));close(fd);return n>=0?std::string(data,n):std::string();
}
static bool OwnFile(const char* path,bool partial) {
 struct stat st={};if(lstat(path,&st)||!S_ISREG(st.st_mode)||st.st_uid!=0||st.st_nlink>2||
  ((st.st_mode&0777)!=0600&&(st.st_mode&0777)!=0644)||st.st_size>static_cast<off_t>(strlen(kText)))return false;
 const auto data=ReadAll(path);
 return partial?data.size()==static_cast<size_t>(st.st_size)&&!memcmp(data.data(),kText,data.size()):data==kText;
}
static int WriteProbe(int block,int directory,const char* path) {
 int rw=0;if(ioctl(block,BLKROSET,&rw))return 21;
 if(mount(nullptr,"/",nullptr,MS_REMOUNT|MS_NODEV|MS_RELATIME,nullptr)||!Reserve("1024"))return 22;
 if(remove_mode) {
  if(!OwnFile(kFinal,false)||unlink(kFinal)||fsync(directory))return 29;
  return 0;
 }
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return 23;
 char context[256]={};ssize_t label=getxattr("/system/etc/init/bootstat.rc","security.selinux",context,sizeof(context)-1);
 bool ok=label>0&&!strcmp(context,"u:object_r:system_file:s0")&&
  fsetxattr(fd,"security.selinux",context,label,0)==0&&
  write(fd,kText,strlen(kText))==static_cast<ssize_t>(strlen(kText))&&fchmod(fd,0644)==0&&fsync(fd)==0;
 int error=ok?0:errno;close(fd);
 std::printf("INIT_TEMP_WRITE=%d errno=%d\n",ok,error);std::fflush(stdout);
 if(!ok||!OwnFile(path,false))return 24;
 // link() atomically creates the new name and refuses to overwrite any existing file.
 if(link(path,kFinal)||unlink(path)||fsync(directory))return 26;
 return OwnFile(kFinal,false)?0:25;
}
static bool RemoveOwnProbe(const char* path) {
 struct stat st={};if(lstat(path,&st))return errno==ENOENT;
 return OwnFile(path,true)&&unlink(path)==0;
}
int main(int argc,char** argv) {
 if(argc!=2||geteuid()!=0)return 64;
 remove_mode=!strcmp(argv[1],"--remove-approved");
 if(!remove_mode&&strcmp(argv[1],"--install-approved"))return 64;
 char fingerprint[PROP_VALUE_MAX]={};__system_property_get("ro.build.fingerprint",fingerprint);
 if(strcmp(fingerprint,"qti/trinket/trinket:10/QKQ1.200816.002/eng.zouqiao.20250217.185927:userdebug/test-keys"))return 77;
 if(!Same("/proc/self/ns/mnt","/proc/1/ns/mnt")) {
  int ns=open("/proc/1/ns/mnt",O_RDONLY|O_CLOEXEC);if(ns<0)return 77;int e=setns(ns,CLONE_NEWNS);close(ns);if(e)return 77;
 }
 struct stat root={},device={},parent={};struct statvfs v={};
 int block=open("/dev/block/dm-3",O_RDONLY|O_CLOEXEC);int ro=-1;
 if(block<0||stat("/",&root)||fstat(block,&device)||!S_ISBLK(device.st_mode)||root.st_dev!=device.st_rdev||ioctl(block,BLKROGET,&ro)||ro!=1||statvfs("/",&v)||!(v.f_flag&ST_RDONLY))return 77;
 if(Read(kReserve)!="4096")return 77;
 char slot[PROP_VALUE_MAX]={};__system_property_get("ro.boot.slot_suffix",slot);if(strcmp(slot,"_a"))return 77;
 struct stat existing={};
 if(remove_mode?!OwnFile(kFinal,false):(lstat(kFinal,&existing)==0||errno!=ENOENT))return 77;
 // This checked directory is the only place the test can create a system file.
 if(lstat("/system/etc/init",&parent)||!S_ISDIR(parent.st_mode)||parent.st_uid!=0||(parent.st_mode&0022))return 77;
 int directory=open("/system/etc/init",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(directory<0)return 77;
 const std::string boot=Read("/proc/sys/kernel/random/boot_id");
 std::string log=std::string("/data/local/ngd-native-bt/init-change-")+boot+(remove_mode?"-remove.log":"-install.log");
 int fd=open(log.c_str(),O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return 77;
 struct stat log_stat={};if(fstat(fd,&log_stat)||!S_ISREG(log_stat.st_mode)||log_stat.st_uid!=0||log_stat.st_nlink!=1||(log_stat.st_mode&0777)!=0600)return 77;
 signal(SIGHUP,SIG_IGN);signal(SIGPIPE,SIG_IGN);
 pid_t guardian=fork();if(guardian<0)return 71;
 if(guardian>0){close(fd);close(directory);close(block);int status=0;while(waitpid(guardian,&status,0)<0){if(errno!=EINTR)return 90;}return WIFEXITED(status)?WEXITSTATUS(status):90;}
 if(setsid()<0)_exit(71);
 if(dup2(fd,1)<0||dup2(fd,2)<0)_exit(71);close(fd);
 std::string path="/system/etc/init/.ngd16-init-pending-"+std::to_string(getpid());
 std::printf("GUARD_ACTIVE boot=%s path=%s timeout_ms=15000\n",boot.c_str(),path.c_str());std::fflush(stdout);
 pid_t worker=fork();if(worker<0)_exit(71);
 if(!worker)_exit(WriteProbe(block,directory,path.c_str()));
 int status=0;bool exited=false;uint64_t until=Now()+15000;
 while(Now()<until){if(waitpid(worker,&status,WNOHANG)==worker){exited=true;break;}usleep(20000);}
 if(!exited){kill(worker,SIGKILL);until=Now()+2000;while(Now()<until){if(waitpid(worker,&status,WNOHANG)==worker){exited=true;break;}usleep(20000);}}
 bool write_ok=exited&&WIFEXITED(status)&&WEXITSTATUS(status)==0;
 bool committed=remove_mode?access(kFinal,F_OK)!=0&&errno==ENOENT:OwnFile(kFinal,false);
 write_ok=write_ok&&committed;
 if(!write_ok&&!remove_mode&&OwnFile(kFinal,false)){if(unlink(kFinal))std::puts("INIT_FAILURE_CLEANUP_FAILED");fsync(directory);}
 bool removed=RemoveOwnProbe(path.c_str());if(removed)fsync(directory);
 bool reserve_restored=Reserve("4096");
 // Always restore both independent protections, including when RW was refused.
 int remount=mount(nullptr,"/",nullptr,MS_REMOUNT|MS_RDONLY|MS_NODEV|MS_RELATIME,nullptr);int mount_errno=remount?errno:0;
 int readonly=1;int blocked=ioctl(block,BLKROSET,&readonly);int block_errno=blocked?errno:0;int after=-1;
 bool verified=statvfs("/",&v)==0&&(v.f_flag&ST_RDONLY)&&ioctl(block,BLKROGET,&after)==0&&after==1;
 std::printf("WRITE=%s worker_status=%d exited=%d FILE_REMOVED=%d RESERVE_RESTORED=%d REMOUNT_RO=%d errno=%d BLOCK_RO=%d errno=%d READBACK_RO=%d\n",write_ok?"PASS":"FAIL",status,exited,removed,reserve_restored,remount,mount_errno,blocked,block_errno,verified);std::fflush(stdout);
 close(directory);close(block);_exit(write_ok&&removed&&reserve_restored&&remount==0&&blocked==0&&verified?0:90);
}
#else
int main(){return 77;}
#endif
