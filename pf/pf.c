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
  bool_t valid;      /* set to TRUE when a file is open. */
  ino_t inode;       /* inode number of the file         */
  char *filename;    /* file name                        */
  int unixfd;        /* Unix file descriptor             */
  PFhdr_str hdr;     /* file header                      */
  bool_t hdrchanged; /* TRUE if file header has changed  */
} PFftab_ele;

PFftab_ele file_table[PF_FTAB_SIZE];

bool_t file_exists(char *filename) {
  struct stat buf;
  return stat(filename, &buf) != -1;
}

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

int PF_DestroyFile(char *filename) {
  PFftab_ele *file;
  int i;

  if (!file_exists(filename)) {
    return PFE_FILE_NOT_EXIST;
  }

  /* make sure the file isn't open */
  for (i = 0; i < PF_FTAB_SIZE; ++i) {
    file = &file_table[i];
    if (file->valid && (strcmp(file->filename, filename) == 0)) {
      return PFE_FILEOPEN;
    }
  }

  if (unlink(filename) == -1) {
    return PFE_REMOVE;
  }

  return PFE_OK;
}

int PF_OpenFile(char *filename) {
  int i, fd, unixfd;
  PFftab_ele *file;
  struct stat file_status;

  if (!file_exists(filename)) {
    return PFE_FILE_NOT_EXIST;
  }

  /* find the first empy page */
  file = NULL;
  for (i = 0; i < PF_FTAB_SIZE; ++i) {
    if (!file_table[i].valid) {
      file = &file_table[i];
      fd = i;
      break;
    }
  }
  if (file == NULL) {
    return PFE_NO_SPACE;
  }

  /* Open file and read value */
  if ((unixfd = open(filename, O_RDWR)) == -1) {
    return PFE_HDRREAD;
  }
  if (read(unixfd, &file->hdr, sizeof(PFhdr_str)) == -1) {
    return PFE_HDRREAD;
  }
  if (fstat(unixfd, &file_status) == -1) {
    return PFE_HDRREAD;
  }

  file->valid = TRUE;
  file->inode = file_status.st_ino;
  strcpy(file->filename, filename);
  file->unixfd = unixfd;
  file->hdrchanged = FALSE;

  return fd;
}
