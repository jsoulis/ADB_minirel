#include <stdlib.h>
#include <unistd.h>
#include "minirel.h"

#include "bf.h"
#include "hash_table.h"

/* FreeList functions */

/* Will initialize the list and the buffer pages */
BFpage *FL_Init(unsigned int size) {
  unsigned int i;
  BFpage *head, *node;

  head = node = malloc(sizeof(BFpage));

  for (i = 1; i < size; ++i) {
    node->nextpage = malloc(sizeof(BFpage));

    node = node->nextpage;
  }

  node->nextpage = NULL;

  return head;
}

/* Cleans up any remaining buffer pages and the list itself */
void FL_Clean(BFpage *node) {
  BFpage *next;

  while (node) {
    next = node->nextpage;

    free(node->nextpage);
    free(node);

    node = next;
  }
}

/* Variables */
BFpage *free_list_head;
BFpage *lru_head;
BFhash_entry **hash_table;

/* API implementations */

/*
 * Should initialize the structures, FreeList and HashTable
 */
void BF_Init(void) {
  free_list_head = FL_Init(BF_MAX_BUFS);
  hash_table = HT_Init(BF_HASH_TBL_SIZE);
}

int initialize_bpage(BFreq bq, BFpage *page) {
  page->dirty = FALSE;
  page->count = 1;
  page->fd = bq.fd;
  page->pagenum = bq.pagenum;

  /* Read page data from an offset. It assumes that pagenum is zero-indexed */
  lseek(bq.unixfd, (PAGE_SIZE * bq.pagenum), SEEK_SET);
  return read(bq.unixfd, page->fpage.pagebuf, PAGE_SIZE);
}

int BF_GetBuf(BFreq bq, PFpage **fpage) {
  BFpage *bpage;

  /* First look in the hash table */
  bpage = HT_Find(hash_table, bq.fd, bq.pagenum);
  if (bpage) {
    ++bpage->count;
    *fpage = &bpage->fpage;

    return BFE_OK;
  }

  /* if there is available space in FreeList just use that */
  if (free_list_head) {
    /* Pop an element from the free_list */
    bpage = free_list_head;
    free_list_head = bpage->nextpage;

    initialize_bpage(bq, bpage);
    /* Update LRU. The least recently used element is at the front */
    bpage->nextpage = lru_head;
    lru_head->prevpage = bpage;

    HT_Add(hash_table, bpage);

    return BFE_OK;
  }

  return BFE_NOBUF;
}
