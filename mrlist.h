/*
  MrList. 

  MrList returns a pointer that you can access as a normal array ( p[0], p[1], ) while taking care of the allocating and resizing for you.

  USAGE:

  uint64_t *lst = mrlist_new();
  for( int x=0; x < 20; x++ ) {
    uint64_t num = x;
    mrlist_add(&lst, num); // Must pass a reference as we may grow the array
    printf("  %d\n", lst[x]);
  }
  mrlist_free(lst);

*/
#ifndef _MRLIST_H
#define _MRLIST_H

#include <stdint.h>

#define mrlist_free( lst ) ({ char *p = ((char*)lst)-8; free(p); })

uint64_t *mrlist_new(int len);
uint64_t *mrlist_init(void *lst, int len);
void mrlist_copy(void *lst, void *dst);
int mrlist_at_max( uint64_t *lst );
void mrlist_grow( uint64_t *lst );
void mrlist_add( uint64_t *lst, uint64_t item);
void _mrlist_grow(uint64_t **lst);
int mrlist_len(uint64_t *lst);
int mrlist_num_bytes(void *lst);


#endif 

