#ifndef __BF_HASH_TABLE_H__
#define __BF_HASH_TABLE_H__
#include "bf_page.h"

typedef struct BFhash_entry {
  struct BFhash_entry *nextentry; /* next hash table element or NULL */
  struct BFhash_entry *preventry; /* prev hash table element or NULL */
  int fd;                         /* file descriptor                 */
  int pagenum;                    /* page number                     */
  struct BFpage *bpage;           /* ptr to buffer holding this page */
} BFhash_entry;

/* HashTable functions */

BFhash_entry **HT_Init(unsigned int size);

/* Will only clean up hash entries, not buffer pages */
void HT_Clean(BFhash_entry **table, unsigned int size);

/*
 * Returns the index in the hash table of the given fd and pagenum
 * Uses a very simple hash algorithm that may be improved
 */
unsigned int HT_Index(int fd, int pagenum);

/* Will return the given object, NULL otherwise */
BFpage *HT_Find(BFhash_entry **table, int fd, int pagenum);

void HT_Add(BFhash_entry **table, BFpage *page);
void HT_Remove(BFhash_entry **table, int fd, int pagenum);

#endif
