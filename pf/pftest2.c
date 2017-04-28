/* pftest2 -- for grading the PF layer, Bongki Moon, Apr/11/2017 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "minirel.h"

#include "bf.h"
#include "pf.h"

#define NUM_PAGES 100
#define STR_SIZE 128

char *dbFileName;
char command[STR_SIZE];

int create_file_counter = 0, destroy_file_counter = 0, open_file_counter = 0,
    close_file_counter = 0, get_next_counter = 0, get_first_counter = 0,
    get_this_counter = 0, alloc_page_counter = 0, dirty_page_counter = 0,
    unpin_page_counter = 0, print_error_counter = 0, set_err_stream_counter = 0;

/* Test the functions for file management */
void manage_single_file() {
  int fd_single;
  int error;

  /* Making sure file doesn't exist */
  unlink(dbFileName);

  /* create, open, close a single file */
  printf("\nTesting create, open, close a single file \n");
  if ((error = PF_CreateFile(dbFileName)) != PFE_OK)
    printf("\t File create failed \n");
  else {
    create_file_counter++;
    if ((fd_single = PF_OpenFile(dbFileName)) < 0)
      printf("\t File open failed \n");
    else {
      open_file_counter++;
      if ((error = PF_CloseFile(fd_single)) == PFE_OK)
        close_file_counter++;
      else
        printf("\t File close failed \n");
    }
  }

  sprintf(command, "ls -al %s", dbFileName);
  system(command);
  fflush(NULL);
}

int manage_many_files() {
  int fd[PF_FTAB_SIZE];
  int fd_single;
  char files[PF_FTAB_SIZE][STR_SIZE];
  int error, i;
  bool_t error_flag = FALSE;

  /* giving names to tmp files */
  for (i = 0; i < PF_FTAB_SIZE; i++) sprintf(files[i], "tmpfile.ifvega.%d", i);

  printf("\n Creating %d files \n", PF_FTAB_SIZE);

  for (i = 0; i < PF_FTAB_SIZE; i++) {
    if ((error = PF_CreateFile(files[i])) != PFE_OK) {
      error_flag = TRUE;
      printf("\t PF_CreateFile failed for %s\n", files[i]);
    }
  }

  if (!error_flag) create_file_counter++;

  error_flag = FALSE;

  printf("\n Opening and closing %d files\n", PF_FTAB_SIZE);

  for (i = 0; i < PF_FTAB_SIZE; i++) {
    if ((fd[i] = PF_OpenFile(files[i])) < 0) {
      error_flag = TRUE;
      printf("\t PF_OpenFile failed for %s\n", files[i]);
    }
  }
  if (!error_flag) open_file_counter++;

  error_flag = FALSE;
  for (i = 0; i < PF_FTAB_SIZE; i++) {
    if (PF_CloseFile(fd[i]) != PFE_OK) {
      error_flag = TRUE;
      printf("\t PF_CloseFile failed for %s\n", files[i]);
    }
  }
  if (!error_flag) close_file_counter++;

  sprintf(command, "ls -al %s*", "tmpfile");
  system(command);
  fflush(NULL);

  error_flag = FALSE;

  printf("\n Destroying %d files\n", PF_FTAB_SIZE);

  for (i = 0; i < PF_FTAB_SIZE; i++) {
    if (PF_DestroyFile(files[i]) != PFE_OK) {
      error_flag = TRUE;
      printf("\t PF_DestroyFile failed for %s\n", files[i]);
    }
  }
  if (!error_flag) destroy_file_counter++;

  sprintf(command, "ls -al %s*", "tmpfile");
  system(command);
  fflush(NULL);

  /* erasing the file */
  unlink(dbFileName);

  printf("\n Opening a non-existing file\n");

  if ((fd_single = PF_OpenFile(dbFileName)) < 0)
    open_file_counter++;
  else {
    printf("\t file open failed, a non-existing file was open\n");
    PF_CloseFile(fd_single);
  }
  return 0;
}

/*
 * Open the file, allocate as many pages in the file as the buffer manager
 * would allow, and write the page number into the data, then close the file.
 */
void writefile() {
  int i;
  int fd, pagenum;
  char *buf;

  bool_t error_flag = FALSE;
  int error;
  int pages_allocated, page_alloc_failed;

  unlink(dbFileName);
  if ((error = PF_CreateFile(dbFileName)) != PFE_OK) {
    printf("Could not test writing to file, fail to create a file\n");
    return;
  }

  if ((fd = PF_OpenFile(dbFileName)) < 0) {
    printf("Could not test writing to file, fail to open a file \n");
    return;
  }

  /* open file1, and allocate a few pages in there */

  pages_allocated = 0;
  page_alloc_failed = 0;

  printf("\n Testing page allocation, allocating %d pages \n", NUM_PAGES);

  for (i = 0; i < NUM_PAGES; i++) {
    if ((error = PF_AllocPage(fd, &pagenum, &buf)) == PFE_OK) {
      memcpy(buf, (char *)&i, sizeof(int));
      pages_allocated++;
    } else {
      error_flag = TRUE;
      page_alloc_failed++;
      printf("PF_AllocPage failed with fd=%d, pagenum=%d\n", fd, i);
    }
  }

  if (pages_allocated == BF_MAX_BUFS) alloc_page_counter++;
  if (page_alloc_failed == NUM_PAGES - BF_MAX_BUFS) alloc_page_counter++;

  error_flag = FALSE;
  printf("\n Marking allocated %d pages as dirty \n", pages_allocated);

  for (i = 0; i < pages_allocated; ++i) {
    if (PF_DirtyPage(fd, i) != PFE_OK) {
      error_flag = TRUE;
      printf("PF_DirtyPage failed with fd=%d, pagenum=%d\n", fd, i);
    }
  }
  if (!error_flag) dirty_page_counter++;

  printf("\n Unpinning %d pages \n", pages_allocated);

  for (i = 0; i < pages_allocated; i++) {
    if ((error = PF_UnpinPage(fd, i, FALSE)) != PFE_OK) {
      error_flag = TRUE;
      printf("PF_DirtyPage failed with fd=%d, pagenum=%d\n", fd, i);
    }
  }
  if (!error_flag) unpin_page_counter++;

  sprintf(command, "ls -al %s*", dbFileName);
  system(command);
  fflush(NULL);

  /* since pages have been unfixed, we should be able to allocate some more */
  printf("\n Allocating another page after unpining\n");

  if ((error = PF_AllocPage(fd, &pagenum, &buf)) == PFE_OK) {
    i = 9999;
    memcpy(buf, (char *)&i, sizeof(int));
    alloc_page_counter++;
  } else
    printf("\t Fail to allocate a page\n");

  if (PF_DirtyPage(fd, pagenum) == PFE_OK) {
    dirty_page_counter++;
  } else
    printf("\t PF_DirtyPage failed\n");

  if ((error = PF_UnpinPage(fd, pagenum, FALSE)) == PFE_OK) {
    unpin_page_counter++;
  } else
    printf("\t Fail to unfix a page\n");

  /* close the file */
  if ((error = PF_CloseFile(fd)) != PFE_OK)
    printf("\t Fail to close a file \n");

  sprintf(command, "ls -al %s*", dbFileName);
  system(command);
  fflush(NULL);
}

/*
 * Open the file, allocate as many pages in the file as the buffer manager
 * would allow. This time, do it one at a time. Every allocated page is
 * unpinned and write back to disk before the next one is allocated.
 */
void writefileagain() {
  int i;
  int fd, pagenum;
  char *buf;

  bool_t error_alloc = FALSE, error_unpin = FALSE;
  int error;

  if ((fd = PF_OpenFile(dbFileName)) < 0) {
    printf("Could not test writing to file, fail to open a file \n");
    return;
  }

  /* open file1, and allocate a few pages in there */

  printf("\n Testing page allocation, allocating %d pages \n", NUM_PAGES);

  for (i = 0; i < NUM_PAGES; i++) {
    if ((error = PF_AllocPage(fd, &pagenum, &buf)) != PFE_OK) {
      error_alloc = TRUE;
      printf("writefileagain: PF_AllocPage failed with fd=%d, i=%d\n", fd, i);
    } else
      memcpy(buf, (char *)&i, sizeof(int));

    if ((error = PF_UnpinPage(fd, pagenum, TRUE)) != PFE_OK) error_unpin = TRUE;
  }

  if (!error_alloc)
    alloc_page_counter++;
  else
    printf("\t writefileagain: Page allocation failed \n");

  if (!error_unpin)
    unpin_page_counter++;
  else
    printf("\t writefileagain: Unpinning page failed \n");

  /* close the file */
  if ((error = PF_CloseFile(fd)) != PFE_OK) printf("\t Fail to close a file\n");

  sprintf(command, "ls -al %s*", dbFileName);
  system(command);
  fflush(NULL);
}

/*
 * read the specified file and then print its contents
 */
void readfile() {
  int i, fd, error;
  char *buf;
  int pagenum;
  bool_t error_unpin = FALSE;

  printf("\nPrinting file content, %d pages should be read\n",
         2 * NUM_PAGES + 1);

  if ((fd = PF_OpenFile(dbFileName)) < 0) {
    printf("Could not test printing file, fail to open\n");
    return;
  }

  pagenum = -1; /* pagenum = -1 means from the beginning */
  while (((error = PF_GetNextPage(fd, &pagenum, &buf)) == PFE_OK)) {
    memcpy((char *)&i, buf, sizeof(int));
    printf("\t\tPage read %d, value_read %d\n", pagenum, i);
    if (PF_UnpinPage(fd, pagenum, FALSE) != PFE_OK) error_unpin = TRUE;
  }

  if (!error_unpin)
    unpin_page_counter++;
  else
    printf("\t Fail to unpin a page\n");

  if (error == PFE_EOF)
    get_next_counter++;
  else
    printf("\t Fail to read the whole file\n");

  /* close the file */
  if ((error = PF_CloseFile(fd)) != PFE_OK) printf("\t Fail to close a file\n");
}

/*
 * Try to read non-existing pages from the file
 */
void read_invalid_page() {
  int error;
  int fd;
  int invalid_page;
  char *buf;

  if ((fd = PF_OpenFile(dbFileName)) < 0) {
    printf("Could not test reading file, fail to open\n");
    return;
  }

  printf("\n Attempting to read a non-existing page from file \n");

  invalid_page = 3 * NUM_PAGES;

  if ((error = PF_GetThisPage(fd, invalid_page, &buf)) == PFE_OK)
    printf("\t Get this page failed, got a non-existing page\n");
  else
    get_this_counter++;

  /* close the file */
  if ((error = PF_CloseFile(fd)) != PFE_OK)
    printf("\t Fail to close a file \n");
}

/*
 * Try to read unpin a no-pinned page from the file
 */
void close_file_pinned() {
  int error;
  int fd;
  int pagenum;
  int value;
  char *buf;

  if ((fd = PF_OpenFile(dbFileName)) < 0) {
    printf("Could not test pinning/unpinning pages, fail to open\n");
    return;
  }

  printf("\n Now more tests on page management \n");

  if ((error = PF_GetFirstPage(fd, &pagenum, &buf)) != PFE_OK)
    printf("\t, GetFirstPage failed\n");
  else {
    get_first_counter++;
    value = 2017;
    memcpy(buf, (char *)&value, sizeof(int));

    if ((error = PF_UnpinPage(fd, pagenum, TRUE)) != PFE_OK)
      printf("\t Fail to unpin a page\n");
    else {
      unpin_page_counter++;

      /* Get the same page again. The first page gets PINNED again. */

      if ((error = PF_GetThisPage(fd, pagenum, &buf)) == PFE_OK) {
        memcpy((char *)&value, buf, sizeof(int));
        if (value == 2017)
          get_this_counter++;
        else
          printf("\t, GetThisPage failed\n");
      } else
        printf("\t, GetThisPage failed\n");
    }
  }

  /* Closing the file must fail */
  if ((error = PF_CloseFile(fd)) != PFE_OK) {
    printf("\t Fail to close a file \n");
    close_file_counter++;
  }

  printf("\n Closing the file again after unpinning \n");

  if ((error = PF_UnpinPage(fd, pagenum, TRUE)) != PFE_OK)
    printf("\t Fail to unpin a page\n");

  if ((error = PF_CloseFile(fd)) != PFE_OK)
    printf("\t Fail to close a file \n");
  else
    close_file_counter++;
}

void print_results() {
  int total_points;

  total_points = create_file_counter + destroy_file_counter +
                 open_file_counter + close_file_counter + get_next_counter +
                 get_first_counter + get_this_counter + alloc_page_counter +
                 dirty_page_counter + unpin_page_counter;

  printf("\n Summary \n");
  printf("\tFile creation : %d/2\n", create_file_counter);
  printf("\tFile opening  : %d/3\n", open_file_counter);
  printf("\tFile closing  : %d/4\n", close_file_counter);
  printf("\tFile destruct : %d/1\n\n", destroy_file_counter);

  printf("\tPage allocation     : %d/4\n", alloc_page_counter);
  printf("\tMarking dirty pages : %d/2\n", dirty_page_counter);
  printf("\tUnpinning pages     : %d/5\n", unpin_page_counter);

  printf("\tGetting first page : %d/1\n", get_first_counter);
  printf("\tGetting this page  : %d/2\n", get_this_counter);
  printf("\tGetting next page  : %d/1\n\n", get_next_counter);

  printf("Total %d/25\n", total_points);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("usage: pftest db-file\n");
    printf("\t db-file     : database file name\n");
    exit(1);
  }

  dbFileName = argv[1];

  PF_Init();

  /* start test */
  manage_single_file();
  manage_many_files();

  writefile();
  writefileagain();

  readfile();
  read_invalid_page();

  close_file_pinned();

  /* end test */
  sprintf(command, "ls -al %s", dbFileName);
  system(command);
  fflush(NULL);

  print_results();
  return 0;
}
