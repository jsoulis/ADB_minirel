#include "lru.h"

#include "bf.h"
#include "hash_table.h"
#include <unistd.h>

void LRU_Push(BFpage **head, BFpage *new_node) {
  /* If the most recently used element is already at the front, do nothing */
  if (*head == new_node) {
    return;
  }

  new_node->prevpage = 0;
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
  while (bpage && bpage->count > 0) {
    bpage = bpage->prevpage;
  }

  if (!bpage) {
    return BFE_PAGEFIXED;
  }

  /* Only have to write if the page is dirty */
  if (bpage->dirty) {
    lseek(bpage->unixfd, PAGE_SIZE + (PAGE_SIZE * bpage->pagenum), SEEK_SET);
    if (write(bpage->unixfd, bpage->fpage.pagebuf, PAGE_SIZE) == -1) {
      return BFE_INCOMPLETEWRITE;
    }
  }

  HT_Remove(hash_table, bpage->fd, bpage->pagenum);

  LRU_Remove(&lru_head, bpage);

  *ret_bpage = bpage;
  return 0;
}
