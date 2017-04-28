#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
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
  unixfd = open(filename, O_RDWR | O_CREAT | O_EXCL,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (unixfd == -1) {
    return PFE_UNIX;
  } else if (errno == EEXIST) {
    return PFE_UNIX;
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
    return PFE_UNIX;
  }

  /* make sure the file isn't open */
  for (i = 0; i < PF_FTAB_SIZE; ++i) {
    file = &file_table[i];
    if (file->valid && (strcmp(file->filename, filename) == 0)) {
      return PFE_FILEOPEN;
    }
  }

  if (unlink(filename) == -1) {
    return PFE_UNIX;
  }

  return PFE_OK;
}

int PF_OpenFile(char *filename) {
  int i, fd, unixfd;
  PFftab_ele *file;
  struct stat file_status;

  if (!file_exists(filename)) {
    return PFE_UNIX;
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
    return PFE_FTABFULL;
  }

  /* Open file and read value */
  if ((unixfd = open(filename, O_RDWR)) == -1) {
    return PFE_HDRREAD;
  }
  if (read(unixfd, &file->hdr, sizeof(PFhdr_str)) != sizeof(PFhdr_str)) {
    return PFE_HDRREAD;
  }
  if (fstat(unixfd, &file_status) == -1) {
    return PFE_HDRREAD;
  }

  file->valid = TRUE;
  file->inode = file_status.st_ino;

  file->filename = malloc(strlen(filename));
  strcpy(file->filename, filename);

  file->unixfd = unixfd;
  file->hdrchanged = FALSE;

  return fd;
}

/*
 * 0. Check if valid fd
 * 1. Flush BF
 * 2. If updated file header => write
 * 3. Close
 * 4. file_table[fd].valid = FALSE
 */
int PF_CloseFile(int fd) {
  int bfe_return_value;
  PFftab_ele *file;

  /* Ensure that the fd is valid and open */
  if (fd < 0 || fd >= PF_FTAB_SIZE) {
    return PFE_FD;
  }
  if (!file_table[fd].valid) {
    return PFE_FILENOTOPEN;
  }

  bfe_return_value = BF_FlushBuf(fd);
  if (bfe_return_value == BFE_PAGEFIXED) {
    return PFE_FILEOPEN;
  } else if (bfe_return_value == BFE_INCOMPLETEWRITE) {
    return PFE_HDRWRITE;
  }

  /* Write header back to file if it changed */
  file = &file_table[fd];
  if (file->hdrchanged) {
    lseek(file->unixfd, 0, SEEK_SET);
    if (write(file->unixfd, &file->hdr, sizeof(PFhdr_str)) == -1) {
      return PFE_HDRWRITE;
    }
  }

  /* Close file and mark the fd as unused */
  if (close(file->unixfd) == -1) {
    return PFE_UNIX;
  }

  free(file_table[fd].filename);
  file_table[fd].valid = FALSE;

  return PFE_OK;
}

/*
 * 0. Check validity of fd
 * 1. Allocate buffer with BF_AllocBuf
 * 2. Mark dirty (it's pinned by default)
 * 3. Increment pagenum of file & set hdrchanged = true
 */
int PF_AllocPage(int fd, int *pagenum, char **pagebuf) {
  PFftab_ele *file;
  BFreq bq;

  if (fd < 0 || fd >= PF_FTAB_SIZE) {
    return PFE_FD;
  }
  if (!file_table[fd].valid) {
    return PFE_FILENOTOPEN;
  }

  file = &file_table[fd];
  bq.fd = fd;
  bq.unixfd = file->unixfd;
  bq.pagenum = file->hdr.numpages;
  if (BF_AllocBuf(bq, (PFpage **)pagebuf) != BFE_OK) {
    return PFE_HDRWRITE;
  }

  if (BF_TouchBuf(bq) != BFE_OK) {
    return PFE_UNIX;
  }

/*pagenum is zero indexed*/
  *pagenum = file->hdr.numpages;

  ++(file->hdr.numpages);
  file->hdrchanged = TRUE;

  return PFE_OK;
}

/*
 * Get the (pagenum + 1) page and update pagenum
 * Return PF_EOF if there are no more pages
 */
int PF_GetNextPage(int fd, int *pagenum, char **pagebuf) {
  PFftab_ele *file;
  BFreq bq;

<<<<<<< HEAD
  /* Allow -1 because we'll increment the pagenum */
  if (fd >= PF_FTAB_SIZE) {
=======
  
  if (fd < 0 || fd >= PF_FTAB_SIZE) {
>>>>>>> Fixed statement that allowed fd to be -1 in PF_GetNextPage function
    return PFE_FD;
  }
  if (!file_table[fd].valid) {
    return PFE_FILENOTOPEN;
  }

  file = &file_table[fd];
  ++(*pagenum);

  /* Equality because pagenum is 0-indexed */
  if (*pagenum >= file->hdr.numpages) {
    return PFE_EOF;
  }

  bq.fd = fd;
  bq.unixfd = file->unixfd;
  bq.pagenum = *pagenum;
  if (BF_GetBuf(bq, (PFpage **)pagebuf) != BFE_OK) {
    return PFE_HDRREAD;
  }

  return PFE_OK;
}

int PF_GetFirstPage(int fd, int *pagenum, char **pagebuf) {
  *pagenum = -1;
  return PF_GetNextPage(fd, pagenum, pagebuf);
}

int PF_GetThisPage(int fd, int pagenum, char **pagebuf) {
  int return_value;
  /* Get next page will increment the page num, so decrement it first */
  --pagenum;

  return_value = PF_GetNextPage(fd, &pagenum, pagebuf);

  if (return_value == PFE_OK) {
    return PFE_OK;
  } else if (return_value == PFE_EOF || return_value == PFE_FD) {
    return PFE_INVALIDPAGE;
  } else {
    return return_value;
  }
}

int PF_DirtyPage(int fd, int pagenum) {
  PFftab_ele *file;
  BFreq bq;

  if (fd < 0 || fd >= PF_FTAB_SIZE) {
    return PFE_FD;
  }

  file = &file_table[fd];
  if (!file->valid) {
    return PFE_FILENOTOPEN;
  }
  if (pagenum >= file->hdr.numpages) {
    return PFE_INVALIDPAGE;
  }

  bq.fd = fd;
  bq.unixfd = file->unixfd;
  bq.pagenum = pagenum;
  if (BF_TouchBuf(bq) != BFE_OK) {
    return PFE_UNIX;
  }

  return PFE_OK;
}

int PF_UnpinPage(int fd, int pagenum, int dirty) {
  PFftab_ele *file;
  BFreq bq;

  if (fd < 0 || fd >= PF_FTAB_SIZE) {
    return PFE_FD;
  }
  file = &file_table[fd];
  if (!file->valid) {
    return PFE_FILENOTOPEN;
  }
  if (pagenum >= file->hdr.numpages) {
    return PFE_INVALIDPAGE;
  }

  bq.fd = fd;
  bq.unixfd = file->unixfd;
  bq.pagenum = pagenum;
  bq.dirty = dirty;

  if (dirty) {
    if (BF_TouchBuf(bq) != BFE_OK) {
      return PFE_UNIX;
    }
  }

  if (BF_UnpinBuf(bq) != BFE_OK) {
    return PFE_PAGEFREE;
  }

  return PFE_OK;
}
