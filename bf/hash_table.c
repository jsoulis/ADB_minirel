#include "hash_table.h"

#include "bf.h"
#include <stdlib.h>

BFhash_entry *HT_Init(unsigned int size) {
  unsigned int i;
  BFhash_entry *entry_array;

  entry_array = malloc(sizeof(BFhash_entry) * size);

  for (i = 0; i < size; ++i) {
    entry_array[i].nextentry = NULL;
    entry_array[i].preventry = NULL;
    entry_array[i].bpage = NULL;
  }

  return entry_array;
}

/* Will only clean up hash entries, not buffer pages */
void HT_Clean(BFhash_entry *entry_array, unsigned int size) {
  unsigned int i;
  BFhash_entry *this, *next;

  for (i = 0; i < size; ++i) {
    /* Don't free head, we'll free all heads at the same time at the end */
    this = entry_array[i].nextentry;

    while (this) {
      next = this->nextentry;
      free(this);
      this = next;
    }
  }

  free(entry_array);
}

/*
 * Returns the index in the hash table of the given fd and pagenum
 * Uses a very simple hash algorithm that may be improved
 */
unsigned int HT_Index(int fd, int pagenum) {
  return (BF_HASH_TBL_SIZE * fd + pagenum) % BF_HASH_TBL_SIZE;
}

/* Will return the given object, NULL otherwise */
BFpage *HT_Find(BFhash_entry *table, int fd, int pagenum) {
  unsigned int index;
  BFhash_entry *entry;

  index = HT_Index(fd, pagenum);
  entry = &(table[index]);
  while (entry) {
    if (entry->fd == fd && entry->pagenum == pagenum) return entry->bpage;

    entry = entry->nextentry;
  }

  return NULL;
}
