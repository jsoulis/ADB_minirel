#ifndef __FREE_LIST_H__
#define __FREE_LIST_H__

#include "bf_page.h"

/* Will initialize the list and the buffer pages */
BFpage *FL_Init(unsigned int size);

/* Cleans up any remaining buffer pages and the list itself */
void FL_Clean(BFpage *node);

/* Pops the first element and updates the head */
BFpage *FL_Pop(BFpage **head);

/* Adds page to the list and makes it the current head */
void FL_Push(BFpage **head, BFpage *page);

#endif
