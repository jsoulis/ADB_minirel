#ifndef __LRU_H__
#define __LRU_H__

#include "bf_page.h"
#include "hash_table.h"

/* Update LRU. The most recently used element is at the front */
void LRU_Push(BFpage **head, BFpage *new_node);

void LRU_Remove(BFpage **head, BFpage *page);

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
                  BFpage **ret_bpage);
#endif
