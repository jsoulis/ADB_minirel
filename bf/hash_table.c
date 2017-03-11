#include "hash_table.h"

#include "bf.h"
#include <stdlib.h>

BFhash_entry **HT_Init(unsigned int size) {
  unsigned int i;
  BFhash_entry **table;

  table = malloc(sizeof(BFhash_entry*) * size);

  for (i = 0; i < size; ++i) {
    table[i] = NULL;
  }

  return table;
}

/* Will only clean up hash entries, not buffer pages */
void HT_Clean(BFhash_entry **table, unsigned int size) {
  unsigned int i;
  BFhash_entry *this, *next;

  for (i = 0; i < size; ++i) {
    if (table[i] != NULL) {
      this = table[i];

      while (this) {
        next = this->nextentry;
        free(this);
        this = next;
      }
    }
  }

  free(table);
}

/*
 * Returns the index in the hash table of the given fd and pagenum
 * Uses a very simple hash algorithm that may be improved
 */
unsigned int HT_Index(int fd, int pagenum) {
  return (BF_HASH_TBL_SIZE * fd + pagenum) % BF_HASH_TBL_SIZE;
}

/* Will return the given object, NULL otherwise */
BFpage *HT_Find(BFhash_entry **table, int fd, int pagenum) {
  unsigned int index;
  BFhash_entry *entry;

  index = HT_Index(fd, pagenum);
  entry = table[index];

  while (entry) {
    if (entry->fd == fd && entry->pagenum == pagenum) return entry->bpage;

    entry = entry->nextentry;
  }

  return NULL;
}

void initialize_entry(BFhash_entry *entry, BFpage *page) {
  entry->bpage = page;
  entry->fd = page->fd;
  entry->pagenum = page->pagenum;
  entry->preventry = NULL;
  entry->nextentry = NULL;
}

void HT_Add(BFhash_entry **table, BFpage *page) {
  BFhash_entry *entry, *prev;
  const unsigned int index = HT_Index(page->fd, page->pagenum);

  entry = table[index];
  prev = NULL;

  while (entry) {
    prev = entry;
    entry = entry->nextentry;
  }

  entry = malloc(sizeof(BFhash_entry));
  initialize_entry(entry, page);

  if (prev) {
    prev->nextentry = entry;
    entry->preventry = prev;
  }

  /* We have to write back the entry if it's the first element */
  if (table[index] == NULL) {
    table[index] = entry;
  }
}

void HT_Remove(BFhash_entry **table, int fd, int pagenum) {
  BFhash_entry *entry;
  const unsigned int index = HT_Index(fd, pagenum);

  entry = table[index];

  while (entry) {
    if (entry->fd == fd && entry->pagenum == pagenum) {
      if (entry->preventry) {
        entry->preventry->nextentry = entry->nextentry;
      }
      if (entry->nextentry) {
        entry->nextentry->preventry = entry->preventry;
      }
      
      /* If it's the first element we have to update the table head */
      if (entry == table[index]) {
        table[index] = entry->nextentry;
      }

      free(entry);
      break;
    }
  }
}
