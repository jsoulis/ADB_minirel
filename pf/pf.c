#include <sys/types.h>

#include "minirel.h"

#include "bf.h"

typedef struct PFhdr_str {
  int numpages; /* number of pages in the file */
} PFhdr_str;

typedef struct PFftab_ele {
  bool_t valid;     /* set to TRUE when a file is open. */
  ino_t inode;      /* inode number of the file         */
  char *fname;      /* file name                        */
  int unixfd;       /* Unix file descriptor             */
  PFhdr_str hdr;    /* file header                      */
  short hdrchanged; /* TRUE if file header has changed  */
} PFftab_ele;

PFftab_ele file_table[PF_FTAB_SIZE];

void PF_Init(void) {
  int i;

  BF_Init();

  for (i = 0; i < PF_FTAB_SIZE; ++i) {
    file_table[i].valid = FALSE;
  }
}
