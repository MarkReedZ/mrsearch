
#include <signal.h>
#include <getopt.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#include "mrloop.h"

#include "common.h"
#include "mrsearch.h"
#include "hashtable.h"
#include "blocks.h"
#include "city.h"
#include "mrlist.h"

//Debug
static uint64_t bytes = 0;
static char *toks[256];
static int num_toks = 0;

#define BUFFER_SIZE 32*1024

// debug
unsigned long nw = 0;
unsigned long wsz = 0;
unsigned long nr = 0;
unsigned long rsz = 0;
unsigned long tot = 0;

static mr_loop_t *loop = NULL;
struct settings settings;

uint64_t mrq_disk_blocks[3];
int      mrq_disk_reads;

hashtable_t *ht, *htnew;

#define NUM_IOVEC 256
typedef struct _conn
{
  char *buf, *recv_buf, *out_buf, *out_p;
  int out_max_sz, out_cur_sz, out_written;
  int max_sz;
  int cur_sz;
  int needs;
  int fd;

  getq_item_t *getq_head, *getq_tail;
  bool stalled;

  struct iovec iovs[NUM_IOVEC];
  int iov_index;

  int write_in_progress;
} my_conn_t;

struct sockaddr_in addr;

//static int total_clients = 0;  // Total number of connected clients
//static int total_mem = 0;
static int num_writes = 0;
//static int num_items = 0;

//static char item_buf[1024] = {0,1};
//static int item_len = 0;
static char resp_get[2] = {0,1};
static char resp_get_not_found[6] = {0,1,0,0,0,0};
static char resp_get_not_found_len = 6;

uint64_t num_bits64 (uint64_t x) { return 64 - __builtin_clzll(x); }

static void setup() {
  ht = malloc( sizeof(hashtable_t) );
  ht_init(ht, 32 * 1024 * 1024);
  blocks_init();

  //srand(1); // TODO
  srand((int)time(NULL));

}

static void tear_down() {
}

static void print_buffer( char* b, int len ) {
  for ( int z = 0; z < len; z++ ) {
    printf( "%02x ",(int)b[z]);
    //printf( "%c",b[z]);
  }
  printf("\n");
}

void *setup_conn(int fd, char **buf, int *buflen ) {
  my_conn_t *c = calloc( 1, sizeof(my_conn_t));
  c->fd = fd;
  c->buf = malloc( BUFFER_SIZE*2 );
  c->recv_buf = malloc( BUFFER_SIZE );
  c->max_sz = BUFFER_SIZE*2;
  c->out_buf = c->out_p = malloc( BUFFER_SIZE );
  c->out_max_sz = BUFFER_SIZE;
  *buf = c->recv_buf;
  *buflen = BUFFER_SIZE;
  return c;
}

void free_conn( my_conn_t *c ) {
  mr_close( loop, c->fd );
  free(c->buf);
  free(c->recv_buf);
  free(c->out_buf);
  // TODO free the output queue items?
  free(c);
}

// TODO delete?
static void conn_append( my_conn_t* c, char *data, int len ) {
  DBG printf(" append cur %d \n", c->cur_sz);

  if ( (c->cur_sz + len) > c->max_sz ) {
    while ( (c->cur_sz + len) > c->max_sz ) c->max_sz <<= 1;
    c->buf = realloc( c->buf, c->max_sz );
  }

  memcpy( c->buf + c->cur_sz, data, len ); 
  c->cur_sz += len;

  DBG printf(" append cur now %d %p \n", c->cur_sz, c->buf);

}

static int conn_flush( my_conn_t *c ) {
  ssize_t nwritten = 0;

  while ( c->out_cur_sz > 0 ) {
    nwritten = write(c->fd, c->out_p, c->out_cur_sz);
    if ( nwritten <= 0 ) {
      if ( nwritten == -1 && errno == EAGAIN ) {
        mr_add_write_callback( loop, can_write, c, c->fd );
      }
      c->stalled = true;
      return 0; // TODO -1 and not EAGAIN? Close connection? Make socket blocking and kill client
    }
    //test_data( c, c->out_p, nwritten );
    c->out_p += nwritten;
    c->out_cur_sz -= nwritten;
  }
  c->out_p = c->out_buf;
  return 1;
}
static int conn_append_out( my_conn_t* c, char *p, int sz ) {
  //DBG printf(" append out %d \n", c->out_cur_sz);
  if ( c->out_p != c->out_buf ) return sz;
  int rlen = BUFFER_SIZE - c->out_cur_sz;
  int len = sz < rlen ? sz : rlen;
  memcpy( c->out_buf + c->out_cur_sz, p, len ); 
  c->out_cur_sz += len;

  //if ( sz != len ) printf("YAY partial out buffer append sz %d len %d\n", sz, len);
  
  return sz - len;
}
static int conn_write_command( my_conn_t *conn ) {
  if ( conn->out_cur_sz < 2 && !conn_flush(conn) ) return 0; 
  conn_append_out( conn, resp_get, 2 );
  return 1;
}
// Returns the number of bytes written if partial otherwise 0 for success
//  TODO this can probably return 0 if we're already stalled!
static int conn_write_buffer( my_conn_t* conn, char *buf, int bsz ) {
  // If we didn't append all of our data to the output buffer
  // flush it and continue until done or stalled
  int sz = conn_append_out( conn, buf, bsz );

  char *p = buf + bsz-sz;
  while ( sz ) {

    // If the output buffer is full and cannot flush return the remaining bytes
    if ( !conn_flush(conn) ) {
      if ( bsz-sz == 0 ) {
        printf("TODO Yeah we can't return 0 here!\n");
        exit(1);
      }
      return bsz-sz; 
    }

    int rem = conn_append_out( conn, p, sz );
    p += sz-rem;
    sz = rem;
  }
  return 0;
}

void conn_process_queue( my_conn_t *conn ) {
  while ( conn->getq_head ) {
    getq_item_t *qi = conn->getq_head;

    if ( qi->item ) {

      char *it = qi->item;

      if ( !conn_write_command( conn ) ) return; //if ( conn->out_cur_sz < 2 && !conn_flush(conn) ) return;

      {

        int n = conn_write_buffer( conn, it, qi->sz );
        if ( n ) { 
          qi->item = NULL;
          qi->buf = it;
          qi->cur = n;
          qi->sz  = qi->sz-n;
          return;
        } 

      }

    } else if ( qi->buf ) {
      
      int n = conn_write_buffer( conn, qi->buf + qi->cur, qi->sz );
      if ( n ) { 
        qi->cur += n;
        qi->sz -= n;
        return;
      }
      if ( qi->type == 3 ) {
        for ( int i=0; i < qi->reads_done; i++ ) { 
          free(qi->disk_items[i].iov.iov_base); 
        }
      }

    } else {

  // TODO disk reads
/*
      // Disk reads still outstanding - TODO do we need to wait for them all to return?
      if ( qi->num_reads > qi->reads_done ) return;
 
      if ( qi->num_reads > 1 ) {
        printf("DELME more than 1 read!\n" );
        exit(1);
      }

      bool found = false; 
      for ( int i=0; i < qi->reads_done; i++ ) { 
        item *it = qi->disk_items[i].iov.iov_base;
        char *itkey = it->data+it->size;
        //printf(" item from disk key: %.*s\n", it->keysize, itkey);

        if ( it->keysize == qi->keylen && (memcmp(qi->key, itkey, qi->keylen) == 0)) {

        	conn_append_out( conn, resp_get, 2 ); // TODO use the new write_command to bail out if buffer full

          {
          	char *p = ((char*)it)+2;
          	int n = conn_write_buffer( conn, p, it->size+4 );
          	if ( n ) { 
            	qi->buf = p;
            	qi->cur = n;
            	qi->sz  = it->size+4-n;
            	return;
          	} 
          }
          found = true;
        }
        free(qi->disk_items[i].iov.iov_base); 
        qi->disk_items[i].iov.iov_base = NULL;

 
      }
      free(qi->key);

      if ( !found ) { 
        int n =  conn_write_buffer( conn, resp_get_not_found,   resp_get_not_found_len );
        if ( n ) { 
          //conn_queue_buffer( conn, resp_get_not_found+n, resp_get_not_found_len-n );
          printf("DELME TODO disk reads missed and can't write the full miss response\n");
          exit(1);
        } 
      }
*/

    }


    conn->getq_head = qi->next;
    if ( conn->getq_head == NULL ) conn->getq_tail = NULL;// TODO do we need this
    // TODO free? We need a flag to free or not a buffer?
    free(qi);

  }
  // Flush the buffer when done
  conn_flush(conn);
}

void can_write( void *conn, int fd ) {

  my_conn_t *c = conn;
  if ( !conn_flush(c) ) return;
  c->stalled = false;

  // Add any queue'd items to the buffer
  conn_process_queue( c );

}

static void conn_queue_item( my_conn_t* conn, char *it, int sz ) {

  getq_item_t *qi = calloc( 1, sizeof(getq_item_t) );
  qi->item = it; 
  qi->sz = sz; 
  qi->block = ht->last_block;
  if ( conn->getq_head == NULL ) {
    conn->getq_head = conn->getq_tail = qi;
    return;
  }
  conn->getq_tail->next = qi;
  conn->getq_tail = qi;
}

static void conn_queue_buffer( my_conn_t* conn, char *buf, int sz ) {
  getq_item_t *qi = calloc( 1, sizeof(getq_item_t) );
  // TODO last block may not be accurate ( processing queue ) so pass in the block id
  qi->block = ht->last_block; // If we manage to LRU the block don't try to access the data later
  qi->buf = buf;
  qi->sz = sz; 
  if ( conn->getq_head == NULL ) {
    conn->getq_head = conn->getq_tail = qi;
    return;
  }
  conn->getq_tail->next = qi;
  conn->getq_tail = qi;
}
static void conn_queue_buffer2( my_conn_t* conn, char *buf, int sz, int nwritten ) {
  getq_item_t *qi = calloc( 1, sizeof(getq_item_t) );
  // TODO last block may not be accurate ( processing queue ) so pass in the block id
  qi->block = ht->last_block; // If we manage to LRU the block don't try to access the data later TODO
  qi->buf = buf;
  qi->cur = nwritten;
  qi->sz = sz-nwritten; 
  if ( conn->getq_head == NULL ) {
    conn->getq_head = conn->getq_tail = qi;
    return;
  }
  conn->getq_tail->next = qi;
  conn->getq_tail = qi;
}

static void conn_write_item( my_conn_t* conn, char *it, int sz ) {

  if ( conn->out_cur_sz < 2 ) {
    if ( !conn_flush(conn) ) {
      conn_queue_item( conn, it, sz );
      return;
    }
  }
  conn_append_out( conn, resp_get, 2 );
  int n =  conn_write_buffer( conn, it, sz );
  if ( n ) conn_queue_buffer( conn, it+n, sz-n );

}

void finishOnData( my_conn_t *conn ) {
  if ( !conn->stalled ) {
    if (conn->getq_head) {
      conn_process_queue( conn );
    }
    if ( conn->out_cur_sz > 0 ) {
      conn_flush(conn);
    }
  }
}

uint64_t *get_list( char *term, int len ) {

  uint64_t *lst;
  unsigned long hv = CityHash64(term, len);      
  DBG printf("get term >%.*s< hv %lx\n",len, term,hv);

  // Get list from hashtable
  uint64_t blockAddr = ht_find( ht, term, len, hv );

  if ( blockAddr == 0 ) {
    blockAddr = blocks_alloc( 8+4*8 ); // mrlist is of size 8 + 8*num
    void *tmp = blocks_translate( blockAddr );
    lst = mrlist_init(tmp, 4);
    ht_insert( ht, blockAddr+8, term, len, hv );
    DBG printf("  get_list not found in ht %p\n",lst);
  } else {
    lst = (uint64_t*)blocks_translate( blockAddr );
    DBG printf("  get_list found in ht %p\n",lst);
  }


  int num_bytes = mrlist_at_max( lst );
  if ( num_bytes ) {
    DBG printf(" get_list list at max size. Bytes %d\n", num_bytes);
    // allocate a new block to accommodate the new item
    uint64_t blockAddr = blocks_alloc( num_bytes ); 
    void *tmp = blocks_translate( blockAddr );
    mrlist_copy( lst, tmp );
    lst = tmp+8;
    mrlist_grow(lst);
    ht_insert( ht, blockAddr+8, term, len, hv );
  }

  return lst;
}


int on_data(void *c, int fd, ssize_t nread, char *buf) {
  my_conn_t *conn = (my_conn_t*)c;
  char *p = NULL;
  int data_left = nread;

  if ( nread == 0 ) { 
    free_conn(c);
    return 1;
  }

  if ( nread < 0 ) { 
    free_conn(c);
    return 1;
  }
  bytes += nread; 

  // If we have partial data for this connection
  if ( conn->cur_sz ) {
    conn_append( conn, buf, nread );
    if ( conn->cur_sz >= conn->needs ) {
      p = conn->buf;
      data_left = conn->cur_sz;
      conn->cur_sz = 0;
    } else {
      return 0;
    }
  } else {
    p = buf;
  }

  while ( data_left > 0 ) {

    if ( data_left < 2 ) {
      conn_append( conn, p, data_left );
      conn->needs = 2;
      finishOnData(conn);
      return 0;
    }

    // TODO check the first byte and bail if its bad
    int cmd   = (unsigned char)p[1];


    DBG printf(" num %d left %d\n", num_writes, data_left );
    DBG print_buffer( p, (data_left>32) ? 32 : data_left  );

    if ( cmd == SEARCH ) {

      if ( data_left < 5 ) {
        conn_append( conn, p, data_left );
        conn->needs = 5;
        finishOnData(conn);
        return 0;
      }

      // magic cmd len search_term
      int len   = (unsigned char)p[2];
      DBG printf("search for >%.*s<\n", len, p+3);

      if ( data_left < 3+len ) {
        conn_append( conn, p, data_left );
        conn->needs = 3+len;
        finishOnData(conn);
        return 0;
      }

                   
      uint64_t *lst = get_list(p+3, len);
      if ( lst == NULL ) {
        //printf("  not found\n");
        if ( conn->getq_head ) conn_queue_buffer( conn, resp_get_not_found, resp_get_not_found_len );
        else {
          int n =  conn_write_buffer( conn, resp_get_not_found,   resp_get_not_found_len );
          if ( n ) conn_queue_buffer( conn, resp_get_not_found+n, resp_get_not_found_len-n );
        }

      } else {
        //printf("  found %p\n",lst);
        //for ( int j = 0; j < mrlist_len(lst); j++ ) {
          //printf(" lst[%d]= 0x%lx\n", j, lst[j]);
        //}

        // If we have items queued up add this one 
        char *ret = (char*)lst; ret -= 4;
        if ( conn->getq_head ) conn_queue_item( conn, ret, mrlist_len(lst)*IDLEN + 4);
        else                   conn_write_item( conn, ret, mrlist_len(lst)*IDLEN + 4);

      }

      p += 3 + len;
      data_left -= 3 + len;
      //num_writes += 1;

    } else if ( cmd == ADD ) {

      //     8B?           8B           
      // [id to store] [text size] [text to index]

      if ( data_left < 16 ) {
        conn_append( conn, p, data_left );
        conn->needs = 8;
        finishOnData(conn);
        return 0;
      }
      uint64_t id = *((uint64_t*)(p+2));
      uint64_t len = *((uint64_t*)(p+10));
      DBG printf("add id %ld len %ld\n", id, len);

      if ( data_left < 16+len ) {
        conn_append( conn, p, data_left );
        conn->needs = 16+len;
        finishOnData(conn);
        return 0;
      }
      DBG printf(" txt to add: >%.*s<\n", (int)len, p+18 );

      p += 18;
      p[len] = 0;
      num_toks = 0;
      char *pch = strtok (p," ,-.\n");
      while (pch != NULL)
      {
        //printf (">%s<\n",pch);
        toks[num_toks++] = pch;
        pch = strtok (NULL, " ,.-");
      }

      data_left -= (18 + len);
      p += len;

      // for each term get the list from the hashtable if null create a new one.  Add the id to the list
      for (int i = 0; i < num_toks; i++ ) {
        //printf(" term %s\n", toks[i]);
        uint64_t *lst = get_list(toks[i], strlen(toks[i]));
        mrlist_add( lst, id );  

        //printf(" List for term %s len %d\n", toks[i], mrlist_len(lst) );
        //for ( int j = 0; j < mrlist_len(lst); j++ ) {
          //printf(" lst[%d]= 0x%lx\n", j, lst[j]);
        //}
      }

      //num_writes += 1;

    } else if ( cmd == STAT ) {

/*
      printf("STAT\n");
      printf("Total reads  %ld\n", settings.tot_reads);
      printf("Total misses %ld\n", settings.misses);
      printf("Total writes %ld\n", settings.tot_writes);
      printf("Avg shift %.2f\n", (double)settings.read_shifts/settings.tot_reads);
      printf("Max shift %d\n", settings.max_shift);
      ht_stat(ht);
      //printf("After full clear\n");
      //ht_stat(ht);
      //exit(1);
*/
      data_left -= 2;
      p += 2;

    } else { 
      // Invalid cmd
      data_left = 0;
      free_conn(conn);
      return 1; // Tell mrloop we closed the connection
    }
  } 

  finishOnData(conn);

  return 0;
}

static void sig_handler(const int sig) {
  printf("Signal handled: %s.\n", strsignal(sig));
  exit(EXIT_SUCCESS);
}

static void usage(void) {
  printf( "Mrsearch Version 0.1\n"
          "    -h, --help                    This help\n"
          "    -p, --port=<num>              TCP port to listen on (default: 7011)\n"
          "    -m, --max-memory=<mb>         Maximum amount of memory in mb (default: 256)\n"
          "    -d, --max-disk=<gb>           Maximum amount of disk in gb (default: 1)\n"
          "\n"
        );
}

// ./mrcache -m 1500 -d 40 -i 256

int main (int argc, char **argv) {

  signal(SIGPIPE, SIG_IGN);

  char *shortopts =
    "h"
    "m:"
    "d:"
    "p:"
    ;
  const struct option longopts[] = {
    {"help",             no_argument, 0, 'h'},
    {"max-memory", required_argument, 0, 'm'},
    {"max-disk",   required_argument, 0, 'd'},
    {"port",       required_argument, 0, 'p'},
    {0, 0, 0, 0}
  };

  settings.port = 7011;
  settings.max_memory = 512;
  settings.flags = 0;
  settings.disk_size = 0;
  settings.index_size = 0;
  settings.block_size = 2; //mb

  settings.read_shifts  = 0;
  settings.tot_reads    = 0;
  settings.tot_writes   = 0;
  settings.write_shifts = 0;

  int optindex, c;
  while (-1 != (c = getopt_long(argc, argv, shortopts, longopts, &optindex))) {
    switch (c) {
    case 'p':
      settings.port = atoi(optarg);
      break;
    case 'm':
      settings.max_memory = atoi(optarg);
      break;
    case 'd':
      settings.disk_size = atoi(optarg);
      break;
    case 'h':
      usage();
      return(2);
      break;
    default:
      usage();
      return(2);
    }
  }

  setup();

  //if ( settings.disk_size ) {
    //printf("Mrcache starting up on port %d with %dmb of memory and %dgb of disk allocated.  The max number of items is %0.1fm based on the index size of %dm\n", settings.port, settings.max_memory, settings.disk_size, max_items, settings.index_size );
  //} else {
    //printf("Mrcache starting up on port %d with %dmb allocated. Maximum items is %0.1fm\n", settings.port, settings.max_memory+settings.index_size, max_items );
  //}
  printf("Mrsearch starting up on port %d with %dmb allocated.\n", settings.port, settings.max_memory);


  loop = mr_create_loop(sig_handler);
  settings.loop = loop;
  mr_tcp_server( loop, settings.port, setup_conn, on_data );
  //mr_add_timer(loop, 0.1, clear_lru_timer, NULL);
  mr_run(loop);
  mr_free(loop);

  tear_down();

  return 0;
}



