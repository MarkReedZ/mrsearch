
#include "mrlist.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

uint64_t *mrlist_new(int len) {
  char *p = malloc( 8 + len * sizeof(uint64_t) );
  uint32_t *ip = (uint32_t*)p; 
  ip[0] = len; ip[1] = 0; // len, max
  return (uint64_t*)(ip+2);
}

uint64_t *mrlist_init(void *lst, int len) {
  uint32_t *ip = (uint32_t*)lst; 
  ip[0] = len; ip[1] = 0; // len, max
  return (uint64_t*)(ip+2);
}

void mrlist_copy(void *lst, void *dst) {
  char *p = (char*)lst;
  p -= 8;
  memcpy( dst, p, mrlist_num_bytes(lst) );
}
int mrlist_num_bytes(void *lst) {
  uint32_t *ip = (uint32_t*)lst;
  uint32_t max = ip[-2]; 
  return 8 + 8*max;
}
void mrlist_grow(uint64_t *lst) {
  uint32_t *ip = (uint32_t*)lst;
  uint32_t max = ip[-2]; 
  ip[-2] = max + 4;
}

void mrlist_add( uint64_t *lst, uint64_t item) {
  uint32_t *ip = (uint32_t*)lst;
  uint32_t max = ip[-2]; 
  uint32_t cur = ip[-1]; 
  lst[cur++] = item;
  ip[-1] = cur;
}

int mrlist_at_max( uint64_t *lst ) { // returns the mem size if we added one more item

  uint32_t *ip = (uint32_t*)lst;
  uint32_t max = ip[-2]; 
  uint32_t cur = ip[-1]; 
  if ( cur == max ) return 8+max*8+4*8;
  return 0;

}

void _mrlist_grow(uint64_t **lst) {

  uint32_t *ip = (uint32_t*)*lst;
  uint32_t max = ip[-2]; 
  max += 4;
  char *orig = ((char*)(*lst))-8;
  char *p = realloc( orig, 8 + max * sizeof(uint64_t) );
  ip = (uint32_t*)p;
  ip[1] = max;
  *lst = (uint64_t*)(p+8);

}

int mrlist_len( uint64_t *lst ) {
  uint32_t *ip = (uint32_t*)lst;
  return ip[-1]; 
}

