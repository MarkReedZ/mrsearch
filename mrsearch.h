
#pragma once

#include "mrloop.h"
#include "common.h"

#define IDLEN 8

enum cmds {
  ADD = 1,
  SEARCH,
  STAT,
  CMDS_END
};

typedef struct _conn my_conn_t;

typedef struct __attribute__((__packed__)) _item {
  int size;
  int keysize;
  uint32_t len;
  uint32_t max;
  uint64_t data[];
} item;

typedef struct _disk_item_t {
  struct iovec iov;
  void *qi; 
  my_conn_t *conn;
} disk_item_t;

typedef struct _getq_item_t {
  char *item;
  char *buf;
  int cur, sz;
  uint64_t block;
  void *next;

  int type; // TODO use this and share pointers
  // Disk reads
  disk_item_t disk_items[3];
  int num_reads, reads_done;
  char *key;
  int keylen;

} getq_item_t;


void can_write( void *conn, int fd );
void conn_process_queue( my_conn_t *conn );

uint64_t *get_list( char *term, int len );


