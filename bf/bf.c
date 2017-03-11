#include <stdlib.h>
#include "minirel.h"

#include "bf.h"
#include "hash_table.h"

/* Local structures */


/* FreeListNode */
typedef struct FL_Node {
  struct FL_Node *next; /* should be NULL if no next element */
  BFpage *page;
} FL_Node;

typedef struct LRU_Node {
  struct LRU_Node *prev, *next; /* should be NULL if no next/prev element*/
  BFpage *page;
} LRU_Node;

/* Local functions */

/* FreeList functions */

/* Will initialize the list and the buffer pages */
FL_Node *FL_Init(unsigned int size) {
  unsigned int i;
  FL_Node *head, *node;

  head = node = malloc(sizeof(FL_Node));

  for (i = 1; i < size; ++i) {
    node->next = malloc(sizeof(FL_Node));
    node->page = malloc(sizeof(BFpage));
    node = node->next;
  }

  node->next = NULL;

  return head;
}

/* Cleans up any remaining buffer pages and the list itself */
void FL_Clean(FL_Node *node) {
  FL_Node *next;

  while (node) {
    next = node->next;

    free(node->page);
    free(node);

    node = next;
  }
}

/* Variables */
FL_Node *free_list;
BFhash_entry *hash_table;

/* API implementations */

/*
 * Should initialize the structures, FreeList and HashTable
 */
void BF_Init(void) {
  free_list = FL_Init(BF_MAX_BUFS);
  hash_table = HT_Init(BF_HASH_TBL_SIZE);
}
