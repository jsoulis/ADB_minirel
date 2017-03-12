#include <stdio.h>
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

/* Pops the first element and updates the head */
BFpage *FL_Pop(BFpage **head) {
  BFpage *page = *head;
  *(head) = page->nextpage;

  return page;
}

void FL_Push(BFpage **head, BFpage *page) {
  page->nextpage = *head;
  *head = page;
}

/* Local functions */

void initialize_bpage_no_read(BFreq bq, BFpage *page) {
  page->dirty = FALSE;
  page->count = 1;
  page->fd = bq.fd;
  page->unixfd = bq.unixfd;
  page->pagenum = bq.pagenum;
  page->nextpage = NULL;
  page->prevpage = NULL;
}

/* Returns -1 on read error */
int initialize_bpage(BFreq bq, BFpage *page) {
  initialize_bpage_no_read(bq, page);

  /* Read page data from an offset. It assumes that pagenum is zero-indexed */
  lseek(bq.unixfd, (PAGE_SIZE * bq.pagenum), SEEK_SET);
  return read(bq.unixfd, page->fpage.pagebuf, PAGE_SIZE);
}

/* Update LRU. The most recently used element is at the front */
void LRU_Push(BFpage **head, BFpage *new_node) {
  /* If the most recently used element is already at the front, do nothing */
  if (*head == new_node) {
    return;
  }

  new_node->nextpage = *head;
  if (*head) {
    (*head)->prevpage = new_node;
  }
  *head = new_node;
}

void LRU_Remove(BFpage **head, BFpage *page) {
  if (!page) {
    return;
  }

  if (*head == page) {
    *head = page->nextpage;
  }

  if (page->prevpage) {
    page->prevpage->nextpage = page->nextpage;
  }
  if (page->nextpage) {
    page->nextpage->prevpage = page->prevpage;
  }
}

/*
 * This function does a bit more than the name suggests
 * It handles everything regarding removing an element from the LRU,
 * writing to disk, and removes it from the hash table
 *
 * Returns BFE_INCOMPLETEWRITE on write error,
 * BFE_PAGEFIXED if no pages can be replaced,
 * BFE_OK otherwise
 * Writes the cleared BFpage to *ret_bpage
 */
int LRU_ClearLast(BFpage *lru_head, BFhash_entry **hash_table,
                  BFpage **ret_bpage) {
  BFpage *bpage = lru_head;
  /*
   * Find the least recently used object (the tail)
   * TODO: Keep a pointer to the tail to not have to go through the whole list
   */
  while (bpage->nextpage) {
    bpage = bpage->nextpage;
  }

  /* We need to find a page that's not pinned */
  while (bpage->count > 0 && bpage) {
    bpage = bpage->prevpage;
  }

  if (!bpage) {
    return BFE_PAGEFIXED;
  }

  /* Only have to write if the page is dirty */
  if (bpage->dirty) {
    lseek(bpage->unixfd, (PAGE_SIZE * bpage->pagenum), SEEK_SET);
    if (write(bpage->unixfd, bpage->fpage.pagebuf, PAGE_SIZE) == -1) {
      return BFE_INCOMPLETEWRITE;
    }
  }

  HT_Remove(hash_table, bpage->fd, bpage->pagenum);

  LRU_Remove(&lru_head, bpage);

  *ret_bpage = bpage;
  return 0;
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

int BF_GetBuf(BFreq bq, PFpage **fpage) {
  BFpage *bpage;
  int error_value;

  /* First look in the hash table */
  bpage = HT_Find(hash_table, bq.fd, bq.pagenum);
  if (bpage) {
    ++bpage->count;
  } else {
    if (free_list_head) {
      /* if there is available space in FreeList just use that */
      bpage = FL_Pop(&free_list_head);
    } else {
      /* Will set bpage to the now cleared page */
      error_value = LRU_ClearLast(lru_head, hash_table, &bpage);
      if (error_value != BFE_OK) {
        return error_value;
      }
    }

    if (initialize_bpage(bq, bpage) == -1) {
      return BFE_INCOMPLETEREAD;
    }

    LRU_Push(&lru_head, bpage);
    HT_Add(hash_table, bpage);
  }

  *fpage = &bpage->fpage;
  return BFE_OK;
}

int BF_AllocBuf(BFreq bq, PFpage **fpage) {
  BFpage *bpage;
  int error_value;

  bpage = HT_Find(hash_table, bq.fd, bq.pagenum);
  /* Since we're allocating a new block, there shouldn't exist one currently */
  if (bpage) {
    return BFE_PAGEINBUF;
  }

  if (free_list_head) {
    bpage = FL_Pop(&free_list_head);
  } else {
    error_value = LRU_ClearLast(lru_head, hash_table, &bpage);
    if (error_value != BFE_OK) {
      return error_value;
    }
  }

  /* Because it's a new page we shouldn't read it */
  initialize_bpage_no_read(bq, bpage);

  LRU_Push(&lru_head, bpage);
  HT_Add(hash_table, bpage);

  *fpage = &bpage->fpage;
  return BFE_OK;
}

int BF_UnpinBuf(BFreq bq) {
  BFpage *page = HT_Find(hash_table, bq.fd, bq.pagenum);

  if (!page) {
    return BFE_PAGENOTINBUF;
  }

  if (page->count <= 0) {
    return BFE_PAGEUNFIXED;
  }

  --(page->count);
  return BFE_OK;
}

int BF_TouchBuf(BFreq bq) {
  BFpage *page = HT_Find(hash_table, bq.fd, bq.pagenum);

  if (!page) {
    return BFE_PAGENOTINBUF;
  }

  if (page->count <= 0) {
    return BFE_PAGEUNFIXED;
  }

  page->dirty = TRUE;

  /* Add it to the front, because it's the most recently used element */
  LRU_Remove(&lru_head, page);
  LRU_Push(&lru_head, page);

  return BFE_OK;
}

int BF_FlushBuf(int fd) {
  BFpage *bpage, *next;

  bpage = lru_head;

  while (bpage) {
    next = bpage->nextpage;

    if (bpage->fd == fd) {
      if (bpage->count > 0) {
        return BFE_PAGEFIXED;
      }

      if (bpage->dirty) {
        lseek(bpage->unixfd, (PAGE_SIZE * bpage->pagenum), SEEK_SET);
        if (write(bpage->unixfd, bpage->fpage.pagebuf, PAGE_SIZE) == -1) {
          return BFE_INCOMPLETEWRITE;
        }
      }

      HT_Remove(hash_table, bpage->fd, bpage->pagenum);
      LRU_Remove(&lru_head, bpage);

      FL_Push(&free_list_head, bpage);
    }

    bpage = next;
  }

  return BFE_OK;
}

void BF_ShowBuf(void) {
  unsigned int i;
  BFpage *bpage;

  i = 0;
  bpage = lru_head;

  printf("\n\tPrinting information about the LRU\n\n");
  while (bpage) {
    printf("%u [ fd: %i, pagenum: %i, unixfd: %i, dirty: %i ]\n", i++,
           bpage->fd, bpage->pagenum, bpage->unixfd, bpage->dirty);
    bpage = bpage->nextpage;
  }
  printf("\n\n\tFinished printing information about the LRU\n");
}
