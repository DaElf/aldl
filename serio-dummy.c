#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "serio.h"
#include "aldl-io.h"
#include "error.h"
#include "config.h"

/****************GLOBALSn'STRUCTURES*****************************/

unsigned char *databuff;
char txmode;

/****************FUNCTIONS**************************************/

void serial_close() {
  return;
}

int serial_init(char *port) {
  printf("serial dummy driver active\n");
  txmode=0;
  databuff=malloc(64);
  int x;
  databuff[0]=0xF4;
  databuff[1]=0x92;
  databuff[2]=0x01;
  for(x=3;x<63;x++) databuff[x] = 0xF3;
  databuff[63] = checksum_generate(databuff,63);
  return 1;
};

void serial_purge() {
  return;
}

void serial_purge_rx() {
  return;
}

void serial_purge_tx() {
  return;
}

int serial_write(byte *str, int len) {
  txmode++;
  return 0;
}

inline int serial_read(byte *str, int len) {
  if(txmode == 0) { /* idle traffic req */
    str[0] = 0x33;
    return 1;
  } if(txmode == 1) { /* shutup req */
    str[0] = 0xF4;
    str[1] = 0x56;
    str[2] = 0x08;
    str[3] = 0xAE;
    return 4;
  } if(txmode > 1) { /* data request */
    int x;
    for(x=0;x<len;x++) {
      str[x] = databuff[x]; 
    };
    return len;
  };
  return 0;
}
