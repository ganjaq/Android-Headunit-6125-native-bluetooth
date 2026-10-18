// Temporary maintenance entrypoint: no Bluetooth, MCU, sockets or device I/O.
// Original /system/bin/blink bytes are never replaced on disk.
#include <unistd.h>
int main(void) {
  for (unsigned i=0;i<120;++i) sleep(1);
  return 0;
}
