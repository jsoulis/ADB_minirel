#include <stdlib.h>
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
BFpage *free_list;
BFhash_entry *hash_table;

/* API implementations */

/*
 * Should initialize the structures, FreeList and HashTable
 */
void BF_Init(void) {
  free_list = FL_Init(BF_MAX_BUFS);
  hash_table = HT_Init(BF_HASH_TBL_SIZE);
}

int BF_GetBuf(BFreq bq, PFpage **fpage) {
  BFpage *bpage;

  bpage = HT_Find(hash_table, bq.fd, bq.pagenum);
  if (bpage) {
    *fpage = &bpage->fpage;
    return BFE_OK;
  }

  return BFE_NOBUF;
}
