#include "ngd_native_session.h"
#include "ngd_hci_session.h"
#ifdef __ANDROID__
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stddef.h>
#include <cstring>
#include <atomic>
#endif
namespace ngd12 {
bool RequestRootRestore() {
#ifdef __ANDROID__
  if (geteuid()!=0) return false;
  int fd=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0);if(fd<0)return false;
  struct timeval timeout={3,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
  struct sockaddr_un addr={};addr.sun_family=AF_UNIX;const size_t n=std::strlen(kSocketName);std::memcpy(addr.sun_path+1,kSocketName,n);
  bool ok=connect(fd,reinterpret_cast<sockaddr*>(&addr),offsetof(sockaddr_un,sun_path)+1+n)==0;
  struct ucred peer={};socklen_t len=sizeof(peer);ok=ok&&getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&len)==0&&peer.uid==0;
  if(ok)ok=send(fd,"RESTORE",7,MSG_NOSIGNAL)==7;
  char reply[16]={};ssize_t count=ok?recv(fd,reply,sizeof(reply),0):-1;close(fd);
  return count==9&&std::memcmp(reply,"RESTORING",9)==0;
#else
  return false;
#endif
}
bool QueryRootSession(bool admit, uint64_t* deadline, bool* renewable) {
  if (renewable) *renewable = false;
#ifdef __ANDROID__
  static std::atomic<int> admitted_server{-1};
#endif
  if (!deadline) return false;
  *deadline = 0;
#ifdef __ANDROID__
  int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  if (fd < 0) return false;
  struct timeval timeout = {3, 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  struct sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;
  const size_t n = std::strlen(kSocketName);
  std::memcpy(addr.sun_path + 1, kSocketName, n);
  bool ok = connect(fd, reinterpret_cast<sockaddr*>(&addr), offsetof(sockaddr_un,sun_path)+1+n) == 0;
  struct ucred peer = {}; socklen_t len = sizeof(peer);
  ok = ok && getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&len)==0 && peer.uid==0;
  const char* request = admit ? "ADMIT" : "CHECK";
  if (ok) ok = send(fd,request,std::strlen(request),MSG_NOSIGNAL)==static_cast<ssize_t>(std::strlen(request));
  char response[96] = {};
  ssize_t count = ok ? recv(fd,response,sizeof(response)-1,0) : -1;
  close(fd);
  if (count <= 8) return false;
  response[count] = 0;
  bool lease = std::strncmp(response,"NGD16_LEASE ",12)==0;
  if (!lease && std::strncmp(response,"NGD12_OK ",9)!=0) return false;
  if (!ngd::ParseDeadline(response+(lease?12:9),deadline) || !*deadline) return false;
  if (lease) {
    // Root credentials alone do not authorize replacement of an admitted broker.
    int expected = -1;
    if (admit) admitted_server.compare_exchange_strong(expected,peer.pid);
    if (admitted_server.load()!=peer.pid) return false;
  }
  if (renewable) *renewable = lease;
  return true;
#else
  (void)admit;
  return false;
#endif
}
}  // namespace ngd12
