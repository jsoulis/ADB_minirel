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

/* Returns -1 on read error */
int initialize_bpage(BFreq bq, BFpage *page) {
  page->dirty = FALSE;
  page->count = 1;
  page->fd = bq.fd;
  page->pagenum = bq.pagenum;
  page->nextpage = NULL;
  page->prevpage = NULL;

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
  } else if (free_list_head) {
    /* if there is available space in FreeList just use that */

    /* Pop an element from the free_list */
    bpage = free_list_head;
    free_list_head = bpage->nextpage;

    if (initialize_bpage(bq, bpage) == -1) {
      return BFE_INCOMPLETEREAD;
    }
    /* Update LRU. The least recently used element is at the front */
    bpage->nextpage = lru_head;
    lru_head->prevpage = bpage;

    HT_Add(hash_table, bpage);
  } else {
    /*
     * If it's not in the hash table and there's no space in the free list,
     * We have to kick a element from the LRU. Also make sure to remove
     * it from the hash table
     */

    bpage = lru_head;
    /*
     * Find the least recently used object (the tail)
     * TODO: Keep a pointer to the tail to not have to go through the whole list
     */
    while (bpage->nextpage) {
      bpage = bpage->nextpage;
    }

    /* Only have to write if the page is dirty */
    if (bpage->dirty) {
      lseek(bq.unixfd, (PAGE_SIZE * bq.pagenum), SEEK_SET);
      if (write(bq.unixfd, bpage->fpage.pagebuf, PAGE_SIZE) == -1) {
        return BFE_INCOMPLETEWRITE;
      }
    }

    HT_Remove(hash_table, bpage->fd, bpage->pagenum);

    /* Remove from the end of LRU */
    bpage->prevpage->nextpage = NULL;
    bpage->prevpage = NULL;

    /*
     * Now initialize it anew and put it at the front of LRU
     * and put it in the hash table
     */
    if (initialize_bpage(bq, bpage) == -1) {
      return BFE_INCOMPLETEREAD;
    }

    bpage->nextpage = lru_head;
    lru_head->prevpage = bpage;

    HT_Add(hash_table, bpage);
  }

  return BFE_OK;
}
