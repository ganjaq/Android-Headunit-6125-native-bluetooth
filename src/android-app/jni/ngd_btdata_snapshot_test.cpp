#include "ngd_btdata_snapshot.h"
#include <cassert>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <fstream>
#include <sstream>
#include <cstdio>
static void Write(const std::string& p,const std::string& value){std::ofstream f(p);f<<value;assert(f.good());}
static std::string Read(const std::string& p){std::ifstream f(p);std::stringstream s;s<<f.rdbuf();return s.str();}
int main(){
 char pattern[]="/tmp/ngd-snapshotXXXXXX";char* d=mkdtemp(pattern);assert(d);std::string dir=d,root=dir+"/config",file=root+"/bt_config.conf",backup=dir+"/backup";
 assert(mkdir(root.c_str(),0700)==0);Write(file,"OEM_PAIRING_DATA");assert(chmod(file.c_str(),0640)==0);assert(setxattr(file.c_str(),"user.ngd-test","old",3,0)==0);
 ngd12::DataSnapshot original({root});assert(original.Capture());assert(original.Count()==2);assert(original.SaveExclusive(backup.c_str()));assert(!original.SaveExclusive(backup.c_str()));
 Write(file,"NEW_NGD_PAIRING_DATA");Write(root+"/new-device","TEMP");assert(mkdir((root+"/new-dir").c_str(),0700)==0);Write(root+"/new-dir/config","TEMP");assert(setxattr(file.c_str(),"user.ngd-test","new",3,0)==0);
 ngd12::DataSnapshot loaded({root});assert(loaded.LoadVerified(backup.c_str()));assert(loaded.Restore());assert(Read(file)=="OEM_PAIRING_DATA");assert(access((root+"/new-device").c_str(),F_OK)!=0);assert(access((root+"/new-dir").c_str(),F_OK)!=0);
 struct stat s{};assert(stat(file.c_str(),&s)==0&&(s.st_mode&0777)==0640);char value[8]={};assert(getxattr(file.c_str(),"user.ngd-test",value,sizeof(value))==3&&std::string(value,3)=="old");
 assert(symlink("/etc/passwd",(root+"/unexpected-link").c_str())==0);ngd12::DataSnapshot reject({root});assert(!reject.Capture());assert(!loaded.Restore());assert(unlink((root+"/unexpected-link").c_str())==0);
 ngd12::DataSnapshot wrong_roots({dir+"/different"});assert(!wrong_roots.LoadVerified(backup.c_str()));
 Write(dir+"/corrupt","bad");assert(!reject.LoadVerified((dir+"/corrupt").c_str()));
 // Package-only rollback must leave a still-running OEM's Bluetooth data untouched.
 std::string other=dir+"/package";assert(mkdir(other.c_str(),0700)==0);Write(other+"/prefs","OEM_APP");
 ngd12::DataSnapshot combined({root,other});assert(combined.Capture());Write(file,"OEM_CHANGED_WHILE_RUNNING");Write(other+"/prefs","TEST_APP");
 assert(!combined.RestoreOnly({dir}));assert(combined.RestoreOnly({other}));
 assert(Read(file)=="OEM_CHANGED_WHILE_RUNNING"&&Read(other+"/prefs")=="OEM_APP");
 unlink((other+"/prefs").c_str());rmdir(other.c_str());
 // Remove only test-owned entries and the exact generated temporary directory.
 unlink((dir+"/corrupt").c_str());unlink(file.c_str());rmdir(root.c_str());unlink(backup.c_str());rmdir(dir.c_str());
 puts("PASS: persisted BT data roundtrip, old pairing data/metadata/xattrs restored, new files removed, symlinks and wrong roots rejected; selected package-only restore preserves live OEM data");
}
