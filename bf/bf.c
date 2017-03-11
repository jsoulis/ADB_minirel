#include <stdlib.h>
#include "minirel.h"

#include "bf.h"

/* Local structures */

/* Buffer page from documentation */
typedef struct BFpage {
  PFpage fpage;            /* page data from the file                 */
  struct BFpage *nextpage; /* next in the linked list of buffer pages */
  struct BFpage *prevpage; /* prev in the linked list of buffer pages */
  bool_t dirty;            /* TRUE if page is dirty                   */
  short count;             /* pin count associated with the page      */
  int pageNum;             /* page number of this page                */
  int fd;                  /* PF file descriptor of this page         */
} BFpage;

/* FreeListNode */
typedef struct FL_Node {
  struct FL_Node *next; /* should be NULL if no next element */
  BFpage *page;
} FL_Node;

typedef struct LRU_Node {
  struct LRU_Node *prev, *next; /* should be NULL if no next/prev element*/
  BFpage *page;
} LRU_Node;

typedef struct BFhash_entry {
  struct BFhash_entry *nextentry; /* next hash table element or NULL */
  struct BFhash_entry *preventry; /* prev hash table element or NULL */
  int fd;                         /* file descriptor                 */
  int pageNum;                    /* page number                     */
  struct BFpage *bpage;           /* ptr to buffer holding this page */
} BFhash_entry;

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

/* HashTable functions */

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
