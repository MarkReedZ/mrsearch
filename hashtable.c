
#include "hashtable.h"
#include "mrsearch.h"
#include "blocks.h"
#include "city.h"

void ht_init(hashtable_t *ht, uint32_t sz) {

  int index_len = sz >> 3;
  ht->mask = index_len - 1;
  ht->max_size = index_len * 0.70;
  ht->size = 0;
  ht->index_size = index_len;

  ht->tbl = calloc( index_len+1, sizeof(uint64_t));
  if (!ht->tbl) {
    fprintf(stderr, "Failed to init hashtable.\n");
    exit(EXIT_FAILURE);
  }
}

uint64_t ht_find(hashtable_t *ht, char *key, uint16_t keylen, uint64_t hv) {

  uint32_t hash = hv & ht->mask;
  char *p = ht->tbl[hash];

  if ( p == NULL ) return 0;

  // ht[hash] ->  [num=3] [totkeylen] len len len key key key - ptr ptr ptr
  uint8_t num = p[0];
  
  uint64_t *itemptr = (uint64_t*)(p + 2 + p[1]);
  p += 2; 
  for (int x = 0; x < num; x++) {
    if ( keylen == p[x] && (memcmp(key, (p+num), keylen) == 0) ) return itemptr[x];
  }
  return 0;

}

  // ht[hash] ->  [num=3] [totkeylen] len len len key key key - ptr ptr ptr
void ht_insert(hashtable_t *ht, uint64_t blockAddr, char *key, uint16_t keylen, uint64_t hv) {

  uint32_t hash = hv & ht->mask;
  char *p = ht->tbl[hash];

  if ( p == NULL ) {
    p = malloc( 3 + keylen + 8 );
    p[0] = 1;
    p[1] = keylen+1;
    p[2] = keylen;
    memcpy( p+3, key, keylen );
  } else {

    // If we're already in the bucket update
    uint8_t num = p[0];
    uint64_t *itemptr = (uint64_t*)(p + 2 + p[1]);

    p += 2; 
    for (int x = 0; x < num; x++) {
      if ( keylen == p[x] && (memcmp(key, (p+num), keylen) == 0) ) {
        *itemptr = blockAddr;
        return;
      }
    }
    p -= 2;

    // Otherwise add
    int bytes = p[1] + 2 + p[0]*8;
    bytes += keylen + 1 + 8;
    p = realloc( p, bytes );
    p[2+p[0]] = keylen;
    p[0] = p[0] + 1;
    memcpy( p+3+p[1], key, keylen );
    p[1] = p[1] + keylen + 1;
  }
  uint64_t *bptr = (uint64_t*)(p+2+p[1]);
  *bptr = blockAddr; 
 
  ht->tbl[hash] = p;

  //ht_print_bucket(p);

}

// Called after we lru a block with the number of items in that block
void ht_decrement(hashtable_t *ht, int n) {
  ht->size -= n;
}

  // ht[hash] ->  [num=3] [totkeylen] len len len key key key - ptr ptr ptr
void ht_print_bucket(char *p) {
  int num = p[0];
  uint64_t *itemptr = (uint64_t*)(p + 2 + p[1]);
  printf("Bucket at %p num items %d\n", p, num );
  p += 2;
  for (int i = 0; i < num; i++ ) {
    printf("  key >%.*s< ptr %lx\n",  p[i], p+num, itemptr[i] );
  }
}

void ht_stat(hashtable_t *ht) {
/*
  int zero = 0;
  int mem  = 0;
  int lru  = 0;
  int disk = 0;
  for(int i = 0; i < ht->index_size; i++) {
    uint64_t a = ht->tbl[i];
    if ( a == 0 ) zero += 1;
    else if ( blocks_is_mem(a) ) mem += 1;
    else if ( blocks_is_disk(a) ) disk += 1;
    else lru += 1;
  }

  printf("Hashtable\n");
  printf("  zero %d\n", zero);
  printf("  mem  %d\n", mem);
  printf("  lru  %d\n", lru);
  printf("  disk %d\n\n", disk);
  printf("  tot  %d\n\n", mem+lru+disk);
*/
}

