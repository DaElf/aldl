#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>

/* local objects */
#include "loadconfig.h"
#include "config.h"
#include "error.h"
#include "dfiler.h"
#include "aldl-io/config.h"
#include "aldl-io/aldl-io.h"

/* ------- GLOBAL----------------------- */

extern aldl_conf_t *aldl; /* aldl data structure */
extern aldl_commdef_t *comm; /* comm specs */
dfile_t *config; /* configuration */

/* ------- LOCAL FUNCTIONS ------------- */

/* get various config options by name */
int configopt_int_fatal(char *str, int min, int max);
int configopt_int(char *str, int min, int max, int def);
byte configopt_byte(char *str, byte def);
byte configopt_byte_fatal(char *str);
float configopt_float(char *str, float def);
float configopt_float_fatal(char *str);
char *configopt_fatal(char *str);
char *configopt(char *str,char *def);

/* get a packet config string */
char *pktconfig(char *buf, char *parameter, int n);
char *dconfig(char *buf, char *parameter, int n);

/* convert a 0xFF format string to a 'byte'... */
byte hextobyte(char *str);

/* initial memory allocation routines */
void aldl_alloc_a(); /* fixed structures */
void aldl_alloc_b(); /* definition arrays */
void aldl_alloc_c(); /* more data space */

/* config file loading */
void load_config_a(); /* load data to alloc_a structures */
void load_config_b(); /* load data to alloc_b structures */
void load_config_c();

void loadconfig(char *configfile) {
  /* parse config file ... never free this structure */
  config = dfile_load(configfile);
  if(config == NULL) fatalerror(ERROR_CONFIG,"cant load config file");
  #ifdef VERBLOSITY
  printf("configuration, stage A...\n");
  #endif
  aldl_alloc_a();
  load_config_a();
  #ifdef VERBLOSITY
  printf("configuration, stage B...\n");
  #endif
  aldl_alloc_b();
  load_config_b();
  #ifdef VERBLOSITY
  printf("configuration, stage C...\n");
  #endif
  aldl_alloc_c();
  load_config_c();
  #ifdef VERBLOSITY
  printf("configuration complete.\n");
  #endif
}

void aldl_alloc_a() {
  /* primary aldl configuration structure */
  aldl = malloc(sizeof(aldl_conf_t));
  if(aldl == NULL) fatalerror(ERROR_MEMORY,"conf_t alloc");
  memset(aldl,0,sizeof(aldl_conf_t));

  /* communication definition */
  comm = malloc(sizeof(aldl_commdef_t));
  if(comm == NULL) fatalerror(ERROR_MEMORY,"commdef alloc");
  memset(comm,0,sizeof(aldl_commdef_t));
  aldl->comm = comm; /* link to conf */

  /* stats tracking structure */
  aldl->stats = malloc(sizeof(aldl_stats_t));
  if(aldl->stats == NULL) fatalerror(ERROR_MEMORY,"stats alloc");
  memset(aldl->stats,0,sizeof(aldl_stats_t));
}

void load_config_a() {
  comm->checksum_enable = configopt_int("CHECKSUM_ENABLE",0,1,1);;
  comm->pcm_address = configopt_byte_fatal("PCM_ADDRESS");
  comm->idledelay = configopt_int("IDLE_DELAY",0,5000,10);
  comm->chatterwait = configopt_int("IDLE_ENABLE",0,1,1);
  comm->shutupcommand = generate_mode(configopt_byte_fatal("SHUTUP_MODE"),comm);
  comm->returncommand = generate_mode(configopt_byte_fatal("RETURN_MODE"),comm);
  comm->shutuprepeat = configopt_int("SHUTUP_REPEAT",0,5000,1);
  comm->shutuprepeatdelay = configopt_int("SHUTUP_DELAY",0,5000,75);
  comm->n_packets = configopt_int("N_PACKETS",1,99,1);
  aldl->n_defs = configopt_int_fatal("N_DEFS",1,512);
}

void aldl_alloc_b() {
  /* allocate space to store packet definitions */
  comm->packet = malloc(sizeof(aldl_packetdef_t) * comm->n_packets);
  if(comm->packet == NULL) fatalerror(ERROR_MEMORY,"packet mem");
}

void load_config_b() {
  int x;
  char *pktname = malloc(50);
  for(x=0;x<comm->n_packets;x++) {
    comm->packet[x].id = configopt_byte_fatal(pktconfig(pktname,"ID",x));
    comm->packet[x].length = configopt_int_fatal(pktconfig(pktname,
                                                "SIZE",x),1,255);
    comm->packet[x].offset = configopt_int(pktconfig(pktname,
                                                 "OFFSET",x),0,254,3);
    comm->packet[x].frequency = configopt_int(pktconfig(pktname,
                                                 "FREQUENCY",x),0,1000,1);
    generate_pktcommand(&comm->packet[x],comm);
    #ifdef VERBLOSITY
    printf("loaded packet %i\n",x);
    #endif
  };
  free(pktname);

  /* sanity checks for single packet mode */
  #ifndef ALDL_MULTIPACKET
  if(comm->packet[0].frequency == 0) {
    fatalerror(ERROR_CONFIG,"the only packet is disabled");
  };
  if(comm->n_packets != 1) {
    fatalerror(ERROR_CONFIG,"this config requires multipacket capabilities");
  };
  #endif
}

void aldl_alloc_c() {
  /* storage for raw packet data */
  int x = 0;
  for(x=0;x<comm->n_packets;x++) {
    comm->packet[x].data = malloc(comm->packet[x].length);
    if(comm->packet[x].data == NULL) fatalerror(ERROR_MEMORY,"pkt data");
  };

  /* storage for data definitions */
  aldl->def = malloc(sizeof(aldl_define_t) * aldl->n_defs);
  if(aldl->def == NULL) fatalerror(ERROR_MEMORY,"definition");
};

void load_config_c() {
  int x=0;
  char *configstr = malloc(50);
  char *tmp;
  aldl_define_t *d;
  int z;

  for(x=0;x<aldl->n_defs;x++) {
    d = &aldl->def[x]; /* shortcut to def */
    tmp=configopt_fatal(dconfig(configstr,"TYPE",x));
    if(faststrcmp(tmp,"BINARY") == 1) {
      d->type=ALDL_BOOL;
      d->binary=configopt_int_fatal(dconfig(configstr,"BINARY",x),0,7);
      d->invert=configopt_int(dconfig(configstr,"INVERT",x),0,1,0);
    } else {
      if(faststrcmp(tmp,"FLOAT") == 1) {
        d->type=ALDL_FLOAT;
        d->precision=configopt_int(dconfig(configstr,"PRECISION",x),0,1000,0);
        d->min.f=configopt_float(dconfig(configstr,"MIN",x),0);
        d->max.f=configopt_float(dconfig(configstr,"MAX",x),9999999);
        d->adder.f=configopt_float(dconfig(configstr,"ADDER",x),0);
        d->multiplier.f=configopt_float(dconfig(configstr,"MULTIPLIER",x),1);
      } else if(faststrcmp(tmp,"INT") == 1) {
        d->type=ALDL_INT; 
        d->min.i=configopt_int(dconfig(configstr,"MIN",x),-32678,32767,0);
        d->max.i=configopt_int(dconfig(configstr,"MAX",x),-32678,32767,65535);
        d->adder.i=configopt_int(dconfig(configstr,"ADDER",x),-32678,32767,0);
        d->multiplier.i=configopt_int(dconfig(configstr,"MULTIPLIER",x),
                                         -32678,32767,1);
      } else if(faststrcmp(tmp,"UINT") == 1) {
        d->type=ALDL_UINT;
        d->min.u=(unsigned int)configopt_int(dconfig(configstr,"MIN",x),
                                                0,65535,0);
        d->max.u=(unsigned int)configopt_int(dconfig(configstr,"MAX",x),
                                                0,65535,65535);
        d->adder.u=(unsigned int)configopt_int(dconfig(configstr,"ADDER",x),
                                               0,65535,0);
        d->multiplier.u=(unsigned int)configopt_int(dconfig(configstr,
                                           "MULTIPLIER",x),0,65535,1);
      } else {
        fatalerror(ERROR_CONFIG,"invalid data type in def");
      };
      d->uom=configopt(dconfig(configstr,"UOM",x),NULL);
      d->size=configopt_byte(dconfig(configstr,"OFFSET",x),8);     
      /* FIXME no support for signed input type */
    };
    d->id=configopt_int(dconfig(configstr,"ID",x),0,32767,x);
    for(z=x-1;z>=0;z--) { /* check for duplicate unique id */
      if(aldl->def[z].id == d->id) fatalerror(ERROR_CONFIG,"duplicate id");
    };
    d->offset=configopt_byte_fatal(dconfig(configstr,"OFFSET",x));
    d->packet=configopt_byte_fatal(dconfig(configstr,"PACKET",x));
    if(d->packet > comm->n_packets - 1) fatalerror(ERROR_CONFIG,"pkt range");
    d->name=configopt_fatal(dconfig(configstr,"NAME",x));
    d->description=configopt_fatal(dconfig(configstr,"DESC",x));
    #ifdef VERBLOSITY
    printf("loaded definition %i\n",x);
    #endif
  };
  free(configstr);
}

char *configopt_fatal(char *str) {
  char *val = configopt(str,NULL);
  if(val == NULL) fatalerror(ERROR_CONFIG_MISSING,str);
  return val;
};

char *configopt(char *str,char *def) {
  char *val = value_by_parameter(str, config);
  if(val == NULL) return def;
  return val;
};

float configopt_float(char *str, float def) {
  char *in = configopt(str,NULL);
  if(in == NULL) return def;
  int x = atof(in);
  return x;
};

float configopt_float_fatal(char *str) {
  int x = atof(configopt_fatal(str));
  return x;
};

int configopt_int(char *str, int min, int max, int def) {
  char *in = configopt(str,NULL);
  if(in == NULL) return def;
  int x = atoi(in);
  if(x < min || x > max) fatalerror(ERROR_RANGE,str);
  return x;
};

int configopt_int_fatal(char *str, int min, int max) {
  int x = atoi(configopt_fatal(str));
  if(x < min || x > max) fatalerror(ERROR_RANGE,str);
  return x;
};

byte configopt_byte(char *str, byte def) {
  char *in = configopt(str,NULL);
  if(in == NULL) return def;
  return hextobyte(in);
};

byte configopt_byte_fatal(char *str) {
  char *in = configopt_fatal(str);
  return hextobyte(in);
};

byte hextobyte(char *str) {
  /* FIXME this kinda sucks */
  return (int)strtol(str,NULL,16);
};

char *pktconfig(char *buf, char *parameter, int n) {
  sprintf(buf,"P%i.%s",n,parameter);
  return buf;
};

char *dconfig(char *buf, char *parameter, int n) {
  sprintf(buf,"D%i.%s",n,parameter);
  return buf;
};
