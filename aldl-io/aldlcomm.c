#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <time.h>

#include "serio.h"
#include "config.h"
#include "aldl-io.h"

#define ALDL_VERBOSE

/* number of attempts to shut up the ecm */
int shutup_attempts;

/* local functions -----*/
int aldl_shutup(); /* repeatedly attempt to make the ecm shut up */
int aldl_send_shutup(); /* send shutup requests, returns 1 all the time */
int aldl_recv_shutup(); /* wait for shutup response, returns 1 on success */
int aldl_waitforchatter(); /* waits forever for a byte, then bails */

/************ FUNCTIONS **********************/

int aldl_reconnect(aldl_commdef_t *c) {
  #ifdef ALDL_VERBOSE
    printf("attempting to place ecm in diagnostic mode.\n");
  #endif
//  while(1) { /* connection attempt loop */
    if(c->chatterwait == 1) {
      /* in chatter wait mode, we simply wait till the ECM says something.
         that way we know it's ready to recieve a shutup request */
      aldl_waitforchatter(c);
    } else {
      /* without chatter wait mode, just wait 10x chatter delay */
      usleep(c->idledelay * 10000);
    };
    if(aldl_shutup(c) == 1) return 1;
//  };
  return 0;
}

int aldl_waitforchatter(aldl_commdef_t *c) {
  #ifdef ALDL_VERBOSE
    printf("waiting for idle chatter to confirm key is on..\n");
  #endif
  serial_chatterwait(c);
  #ifdef ALDL_VERBOSE
    printf("got idle chatter or something.\n");
  #endif
  usleep(c->idledelay * 1000);
  return 1;
}

int aldl_shutup(aldl_commdef_t *c) {
  serial_purge(); /* pre-clear read buffer */
  shutup_attempts = 1;
  while(1) {
    aldl_send_shutup(c);
    shutup_attempts++;
    if(shutup_attempts > c->shutuprepeat) return 0;
    if(aldl_recv_shutup(c) == 1) return 1;
  };
}

int aldl_send_shutup(aldl_commdef_t *c) {
  int x;
  /* cram the shutup string into the stream.  this could be optimized, but
     it does work. */
  for(x=0;x<c->shutuprepeat;x++) {
    #ifdef ALDL_VERBOSE
      printf("sending shutup request %i\n",x + 1);
    #endif
    serial_f_write(c->shutupcommand,c->shutuplength);
    usleep(c->shutuprepeatdelay);
  };
  return 1;
}

int aldl_recv_shutup(aldl_commdef_t *c){
  /* the pcm should echo the string on success, then wait. */
  #ifdef ALDL_VERBOSE
    printf("waiting for response...\n");
  #endif
  int result = serial_listen(c->shutupcommand,c->shutuplength,
               c->shutupcharlimit,
               c->shutupfailwait);
  #ifdef ALDL_VERBOSE
    printf("response success: %i\n",result);
  #endif
  return result;
}

char *aldl_get_packet(aldl_packetdef_t *p) {
  serial_purge();
  msleep(p->delay_recv);
  serial_f_write(p->command, p->commandlength);
  msleep(p->delay_send);
  /* skip header offset */
  if(serial_skip_bytes(p->offset, p->timeout) == 0) {
    /* failed to get header, zero out packet */
    memset(p->data,0,p->length);
    return NULL;
  }
  /* get actual data */
  if(serial_read_bytes(p->data, p->length, p->timeout) == 1) {
    /* success */
  } else {
    /* failed to get data */
    memset(p->data,0,p->length);
    return NULL;
  }
  return p->data;
}