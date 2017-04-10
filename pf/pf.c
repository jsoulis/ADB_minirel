#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "minirel.h"

#include "bf.h"
#include "pf.h"

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

int PF_CreateFile(char *filename) {
  char header[PAGE_SIZE];
  int unixfd;

  /* By adding O_CREAT and O_EXCL we can get error if file exists */
  unixfd = open(filename, O_RDWR | O_CREAT | O_EXCL);
  if (unixfd == -1) {
    return PFE_UNIX;
  } else if (errno == EEXIST) {
    return PFE_FILE_EXISTS;
  }

  memset(&header, 0, PAGE_SIZE);
  if (write(unixfd, &header, PAGE_SIZE) == -1) {
    return PFE_HDRWRITE;
  }

  if (close(unixfd) == -1) {
    return PFE_UNIX;
  }

  return PFE_OK;
}
