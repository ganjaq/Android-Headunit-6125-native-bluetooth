#include "ngd14_host_heartbeat.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
 const char* t="NGD14_KEEPALIVE\n";ngd14::HostHeartbeat h(100);
 assert(h.Alive(3099)&&!h.Alive(3100));assert(!h.Feed(t,strlen(t),3100));
 ngd14::HostHeartbeat healthy(100);assert(healthy.Feed(t,5,2000));assert(!healthy.Alive(3100));
 ngd14::HostHeartbeat fragmented(100);assert(fragmented.Feed(t,5,1000));assert(fragmented.Feed(t+5,strlen(t)-5,2000));assert(fragmented.Alive(4999)&&!fragmented.Alive(5000));
 ngd14::HostHeartbeat malformed(0);assert(!malformed.Feed("NGD14_WRONG\n",12,100));assert(!malformed.Alive(101));
 ngd14::HostHeartbeat repeated(0);for(unsigned now=1000;now<=60000;now+=1000)assert(repeated.Feed(t,strlen(t),now));assert(!repeated.Alive(63000));
 puts("PASS: host heartbeat expires after 3s; fragmented and repeated frames accepted; malformed, partial-only and expired renewals denied");
}
