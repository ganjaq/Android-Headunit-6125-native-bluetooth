#ifndef NGD14_HOST_HEARTBEAT_H
#define NGD14_HOST_HEARTBEAT_H
#include <cstddef>
#include <cstdint>
namespace ngd14 {
class HostHeartbeat {
 public:
  explicit HostHeartbeat(uint64_t now):deadline_(now+3000){}
  bool Feed(const char* bytes,size_t size,uint64_t now) {
    if(!Alive(now))return false;
    static constexpr char token[]="NGD14_KEEPALIVE\n";
    for(size_t i=0;i<size;++i){
      if(bytes[i]!=token[partial_]){valid_=false;return false;}
      if(++partial_==sizeof(token)-1){partial_=0;deadline_=now+3000;}
    }
    return true;
  }
  bool Alive(uint64_t now) const {return valid_&&now<deadline_;}
 private:
  uint64_t deadline_;size_t partial_=0;bool valid_=true;
};
}
#endif
