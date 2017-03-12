#include "free_list.h"

#include <stdlib.h>

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

void FL_Clean(BFpage *node) {
  BFpage *next;

  while (node) {
    next = node->nextpage;

    free(node->nextpage);
    free(node);

    node = next;
  }
}

BFpage *FL_Pop(BFpage **head) {
  BFpage *page = *head;
  *(head) = page->nextpage;

  return page;
}

void FL_Push(BFpage **head, BFpage *page) {
  page->nextpage = *head;
  *head = page;
}
