#ifndef __BUFFER_PAGE_H__
#define __BUFFER_PAGE_H__

#include "minirel.h"

/* Buffer page from documentation */
typedef struct BFpage {
  PFpage fpage;            /* page data from the file                 */
  struct BFpage *nextpage; /* next in the linked list of buffer pages */
  struct BFpage *prevpage; /* prev in the linked list of buffer pages */
  bool_t dirty;            /* TRUE if page is dirty                   */
  short count;             /* pin count associated with the page      */
  int pagenum;             /* page number of this page                */
  int fd;                  /* PF file descriptor of this page         */
} BFpage;

#endif
