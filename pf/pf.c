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

void create_bf_request(int fd, PFftab_ele *file, BFreq *bq) {
  bq->fd = fd;
  bq->unixfd = file->unixfd;
  bq->pagenum = file->hdr.numpages;
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
    return PFE_FTABFULL;
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
    return PFE_FILE_IN_USE;
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
    return PFE_CLOSE;
  }
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
  create_bf_request(fd, file, &bq);
  if (BF_AllocBuf(bq, (PFpage **)pagebuf) != BFE_OK) {
    return PFE_ALLOC_PAGE;
  }

  if (BF_TouchBuf(bq) != BFE_OK) {
    return PFE_ALLOC_PAGE;
  }

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

  /* Allow -1 because we'll increment the pagenum */
  if (fd < -1 || fd >= PF_FTAB_SIZE) {
    return PFE_FD;
  }
  if (!file_table[fd].valid) {
    return PFE_FILENOTOPEN;
  }

  file = &file_table[fd];
  ++(*pagenum);

  /* Equality because pagenum is 0-indexed */
  if (*pagenum == file->hdr.numpages) {
    return PFE_EOF;
  }

  create_bf_request(fd, file, &bq);
  if (BF_GetBuf(bq, (PFpage **)pagebuf) != BFE_OK) {
    return PFE_HDRREAD;
  }

  return PFE_OK;
}
