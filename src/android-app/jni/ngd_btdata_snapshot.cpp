#include "ngd_btdata_snapshot.h"
#include <sys/xattr.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cstdint>
namespace ngd12 {
bool DataSnapshot::Excluded(const std::string& path) const {
 return path=="/data/misc/bluetooth/logs"||path.find("/data/misc/bluetooth/logs/")==0;
}
bool DataSnapshot::Walk(const std::string& path,std::map<std::string,Node>* nodes,bool capture) const {
 if(Excluded(path))return true;
 struct stat s{};if(lstat(path.c_str(),&s))return errno==ENOENT;
 if(S_ISLNK(s.st_mode)||(!S_ISDIR(s.st_mode)&&!S_ISREG(s.st_mode)&&!(S_ISSOCK(s.st_mode)&&!capture)))return false;
 if(nodes->size()>=4096)return false;
 Node n;n.meta=s;
 if(capture){
  if(S_ISREG(s.st_mode)){
   size_t total=0;for(const auto& e:*nodes)total+=e.second.data.size();
   if(s.st_size<0||static_cast<uint64_t>(s.st_size)>32*1024*1024||total+s.st_size>32*1024*1024)return false;
   int fd=open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return false;
   n.data.resize(s.st_size);size_t pos=0;while(pos<n.data.size()){ssize_t k=read(fd,&n.data[pos],n.data.size()-pos);if(k<=0){close(fd);return false;}pos+=k;}
   struct stat after{};bool ok=fstat(fd,&after)==0&&after.st_ino==s.st_ino&&after.st_size==s.st_size&&after.st_mtim.tv_sec==s.st_mtim.tv_sec&&after.st_mtim.tv_nsec==s.st_mtim.tv_nsec;close(fd);if(!ok)return false;
  }
  ssize_t size=llistxattr(path.c_str(),nullptr,0);if(size<0)return false;std::vector<char> names(size);
  if(size&&llistxattr(path.c_str(),names.data(),names.size())!=size)return false;
  for(size_t pos=0;pos<names.size();){std::string key(names.data()+pos);pos+=key.size()+1;
   ssize_t count=lgetxattr(path.c_str(),key.c_str(),nullptr,0);if(count<0||count>65536)return false;std::string value(count,'\0');
   if(count&&lgetxattr(path.c_str(),key.c_str(),&value[0],value.size())!=count)return false;n.attrs[key]=value;
  }
 }
 (*nodes)[path]=n;
 if(S_ISDIR(s.st_mode)){DIR* d=opendir(path.c_str());if(!d)return false;bool ok=true;struct dirent* e;
  while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;if(!Walk(path+"/"+e->d_name,nodes,capture)){ok=false;break;}}
  closedir(d);if(!ok)return false;
 }
 return true;
}
bool DataSnapshot::Capture(){nodes_.clear();captured_=false;for(const auto& path:roots_)if(!Walk(path,&nodes_,true))return false;captured_=true;return true;}
bool DataSnapshot::SaveExclusive(const char* path) const {
 if(!captured_)return false;int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return false;
 FILE* f=fdopen(fd,"wb");if(!f){close(fd);return false;}
 bool ok=fwrite("NGD12DATA1",1,10,f)==10;
 auto scalar=[&](uint64_t v){if(fwrite(&v,sizeof(v),1,f)!=1)ok=false;};
 auto bytes=[&](const std::string& v){scalar(v.size());if(fwrite(v.data(),1,v.size(),f)!=v.size())ok=false;};
 scalar(nodes_.size());
 for(const auto& row:nodes_){const Node& n=row.second;bytes(row.first);scalar(n.meta.st_mode);scalar(n.meta.st_uid);scalar(n.meta.st_gid);scalar(n.meta.st_atim.tv_sec);scalar(n.meta.st_atim.tv_nsec);scalar(n.meta.st_mtim.tv_sec);scalar(n.meta.st_mtim.tv_nsec);bytes(n.data);scalar(n.attrs.size());for(const auto& a:n.attrs){bytes(a.first);bytes(a.second);}}
 if(fflush(f)||fsync(fd))ok=false;if(fclose(f))ok=false;return ok;
}
bool DataSnapshot::RestoreOnly(const std::vector<std::string>& selected) const {
 if(!captured_||selected.empty())return false;
 for(const auto& root:selected)if(std::find(roots_.begin(),roots_.end(),root)==roots_.end())return false;
 DataSnapshot subset(selected);subset.captured_=true;
 for(const auto& row:nodes_)for(const auto& root:selected)
  if(row.first==root||row.first.find(root+"/")==0){subset.nodes_.insert(row);break;}
 return subset.Restore();
}
bool DataSnapshot::Restore() const {
 if(!captured_)return false;
 std::map<std::string,Node> current;for(const auto& path:roots_)if(!Walk(path,&current,false))return false;
 // Remove only entries created after this snapshot, under the explicit roots.
 for(auto it=current.rbegin();it!=current.rend();++it)if(nodes_.count(it->first)==0){
  int result=S_ISDIR(it->second.meta.st_mode)?rmdir(it->first.c_str()):unlink(it->first.c_str());
  if(result&&errno==ENOTEMPTY&&it->first=="/data/misc/bluetooth") {
   DIR* d=opendir(it->first.c_str());if(!d)return false;bool only_logs=true;struct dirent* e;
   while((e=readdir(d)))if(strcmp(e->d_name,".")&&strcmp(e->d_name,"..")&&strcmp(e->d_name,"logs"))only_logs=false;
   closedir(d);if(only_logs)result=0;
  }
  if(result)return false;
 }
 for(const auto& row:nodes_){const std::string& path=row.first;const Node& n=row.second;
  struct stat now{};if(lstat(path.c_str(),&now)==0&&((now.st_mode&S_IFMT)!=(n.meta.st_mode&S_IFMT)))return false;
  if(S_ISDIR(n.meta.st_mode)){if(mkdir(path.c_str(),0700)&&errno!=EEXIST)return false;}
  else {int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return false;
   size_t pos=0;while(pos<n.data.size()){ssize_t k=write(fd,n.data.data()+pos,n.data.size()-pos);if(k<=0){close(fd);return false;}pos+=k;}if(fsync(fd)){close(fd);return false;}close(fd);
  }
 }
 // Restore directories last so their timestamps survive child restoration.
 for(auto it=nodes_.rbegin();it!=nodes_.rend();++it){const auto& path=it->first;const Node& n=it->second;
  if(lchown(path.c_str(),n.meta.st_uid,n.meta.st_gid)||chmod(path.c_str(),n.meta.st_mode&07777))return false;
  ssize_t size=llistxattr(path.c_str(),nullptr,0);if(size<0)return false;std::vector<char> names(size);if(size&&llistxattr(path.c_str(),names.data(),size)!=size)return false;
  for(size_t p=0;p<names.size();){std::string key(names.data()+p);p+=key.size()+1;if(n.attrs.count(key)==0&&lremovexattr(path.c_str(),key.c_str()))return false;}
  for(const auto& a:n.attrs)if(lsetxattr(path.c_str(),a.first.c_str(),a.second.data(),a.second.size(),0))return false;
  struct timespec times[2]={n.meta.st_atim,n.meta.st_mtim};if(utimensat(AT_FDCWD,path.c_str(),times,AT_SYMLINK_NOFOLLOW))return false;
 }
 return true;
}
bool DataSnapshot::LoadVerified(const char* path) {
 captured_=false;nodes_.clear();int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return false;
 struct stat meta{};if(fstat(fd,&meta)||!S_ISREG(meta.st_mode)||meta.st_uid!=geteuid()||(meta.st_mode&0777)!=0600||meta.st_size>40*1024*1024){close(fd);return false;}
 FILE* f=fdopen(fd,"rb");if(!f){close(fd);return false;}char magic[10]={};bool ok=fread(magic,1,10,f)==10&&!memcmp(magic,"NGD12DATA1",10);
 auto scalar=[&](){uint64_t n=0;if(fread(&n,sizeof(n),1,f)!=1)ok=false;return n;};
 size_t total=0;auto bytes=[&](size_t max){std::string v;uint64_t n=scalar();if(!ok||n>max||total+n>40*1024*1024){ok=false;return v;}v.resize(n);if(n&&fread(&v[0],1,n,f)!=n)ok=false;total+=n;return v;};
 uint64_t count=scalar();if(count>4096)ok=false;
 for(uint64_t i=0;i<count&&ok;++i){std::string name=bytes(4096);bool allowed=false;for(const auto& root:roots_)if(name==root||name.find(root+"/")==0)allowed=true;
  const bool bad_end=name.size()>=3&&(name.compare(name.size()-3,3,"/..")==0||name.compare(name.size()-2,2,"/.")==0);
  if(!allowed||bad_end||name.find("//")!=std::string::npos||name.find("/../")!=std::string::npos||name.find("/./")!=std::string::npos||name.size()<1||name.back()=='/'||name.find('\0')!=std::string::npos||Excluded(name)||nodes_.count(name)){ok=false;break;}
  Node n;n.meta.st_mode=scalar();n.meta.st_uid=scalar();n.meta.st_gid=scalar();n.meta.st_atim.tv_sec=scalar();n.meta.st_atim.tv_nsec=scalar();n.meta.st_mtim.tv_sec=scalar();n.meta.st_mtim.tv_nsec=scalar();
  if((!S_ISDIR(n.meta.st_mode)&&!S_ISREG(n.meta.st_mode))||n.meta.st_atim.tv_nsec>=1000000000||n.meta.st_mtim.tv_nsec>=1000000000){ok=false;break;}
  n.data=bytes(32*1024*1024);uint64_t attrs=scalar();if(attrs>256)ok=false;
  for(uint64_t k=0;k<attrs&&ok;++k){std::string key=bytes(1024),value=bytes(65536);if(key.empty()||key.find('\0')!=std::string::npos||n.attrs.count(key)){ok=false;break;}n.attrs[key]=value;}
  nodes_[name]=n;
 }
 if(ok&&fgetc(f)!=EOF)ok=false;fclose(f);captured_=ok;if(!ok)nodes_.clear();return ok;
}
}
