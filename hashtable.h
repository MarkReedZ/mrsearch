
#pragma once

#include "common.h"
#include "hashtable.h"

typedef struct _item item;

typedef struct hashtable {
  char **tbl, **newtbl;
  uint32_t mask;
  uint32_t size;
  uint32_t index_size;
  uint32_t max_size;
  //uint64_t last_hash;
  uint64_t last_block;

} hashtable_t;

void   ht_init  (hashtable_t *ht, uint32_t sz);
uint64_t ht_find(hashtable_t *ht, char *key, uint16_t keylen, uint64_t hv);
void   ht_insert(hashtable_t *ht, uint64_t blockAddr, char *key, uint16_t keylen, uint64_t hv);
void   ht_delete(hashtable_t *ht, uint64_t hv);
void   ht_stat  (hashtable_t *ht);
void   ht_decrement(hashtable_t *ht, int num);

// DEBUG
void ht_print_bucket(char *p);
