#include "string.h"
#include "stdlib.h"

#include "minirel.h"

#include "am.h"
#include "data_structures.h"
#include "hf.h"
#include "utils.h"
#include "pf.h"

index_table_entry index_table[AM_ITAB_SIZE];
scan_table_entry scan_table[MAXISCANS];

void AM_Init()
{
  int i;
  HF_Init();

  for (i = 0; i < AM_ITAB_SIZE; ++i)
  {
    index_table[i].in_use = FALSE;
    index_table[i].filename = 0;
  }

  for (i = 0; i < MAXISCANS; ++i)
  {
    scan_table[i].in_use = FALSE;
    scan_table[i].index_in_node = 0;
  }
}

int AM_CreateIndex(char *filename, int index_no, char attr_type, int attr_length,
                   bool_t is_unique)
{
  internal_node *root;
  int fd, pagenum;
  char *filename_with_index;

  if (!(attr_type == 'c' || attr_type == 'i' || attr_type == 'f'))
  {
    AMerrno = AME_INVALIDATTRTYPE;
    return AMerrno;
  }

  if (attr_length < 1 || attr_length > 255)
  {
    AMerrno = AME_INVALIDATTRLENGTH;
    return AMerrno;
  }

  /* add 1 to size to fit NULL character */
  filename_with_index = malloc(sizeof_filename_with_index(filename, index_no));
  set_filename_with_index(filename, index_no, filename_with_index);

  if (PF_CreateFile(filename_with_index) != PFE_OK)
  {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  fd = PF_OpenFile(filename_with_index);
  if (fd < 0)
  {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  if (PF_AllocPage(fd, &pagenum, ((char **)&root)) != PFE_OK)
  {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  if (attr_type == 'f' || attr_type == 'i')
  {
    attr_length = 4;
  }

  root->valid_entries = 0;
  root->key_type = attr_type;
  /* max length is 255 */
  root->key_length = (uint8_t)attr_length;

  /* Unpin and mark dirty */
  if (PF_UnpinPage(fd, pagenum, TRUE) != PFE_OK)
  {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  if (PF_CloseFile(fd) != PFE_OK)
  {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  free(filename_with_index);
  /* stop complaining about unused variable */
  (void)is_unique;
  return AME_OK;
}

int AM_DestroyIndex(char *filename, int index_no)
{
  int return_code = AME_OK;
  char *filename_with_index = malloc(sizeof_filename_with_index(filename, index_no));
  set_filename_with_index(filename, index_no, filename_with_index);
  /* PF won't destroy a pinned file. So if we have the root pinned we can just
   * check like this */
  if (PF_DestroyFile(filename_with_index) != PFE_OK)
  {
    AMerrno = AME_PF;
    return_code = AMerrno;
  }

  free(filename_with_index);
  return return_code;
}

int AM_OpenIndex(char *filename, int index_no)
{
  char *filename_with_index;
  int i, fd, pagenum, am_fd = -1;

  /* Check if there is space in table */
  for (i = 0; i < AM_ITAB_SIZE; ++i)
  {
    if (!index_table[i].in_use)
    {
      am_fd = i;
      break;
    }
  }
  if (am_fd == -1)
  {
    AMerrno = AME_FULLTABLE;
    return AMerrno;
  }

  filename_with_index = malloc(sizeof_filename_with_index(filename, index_no));
  set_filename_with_index(filename, index_no, filename_with_index);

  for (i = 0; i < AM_ITAB_SIZE; ++i)
  {
    if (index_table[i].filename && strcmp(index_table[i].filename, filename_with_index) == 0)
    {
      free(filename_with_index);

      AMerrno = AME_DUPLICATEOPEN;
      return AMerrno;
    }
  }

  fd = PF_OpenFile(filename_with_index);
  if (fd < 0)
  {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  if (PF_GetFirstPage(fd, &pagenum, (char **)&index_table[am_fd].root) != PFE_OK)
  {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  index_table[am_fd].in_use = TRUE;
  index_table[am_fd].fd = fd;
  index_table[am_fd].filename = filename_with_index;
  return am_fd;
}

int AM_CloseIndex(int am_fd)
{
  int i;
  index_table_entry *entry;

  if (am_fd < 0 || am_fd >= AM_ITAB_SIZE)
  {
    AMerrno = AME_FD;
    return AMerrno;
  }

  for (i = 0; i < MAXISCANS; ++i) {
    if (scan_table[i].in_use && scan_table[i].am_fd == am_fd) {
      AMerrno = AME_SCANOPEN;
      return AMerrno;
    }
  }

  entry = &index_table[am_fd];
  if (!entry->in_use)
  {
    AMerrno = AME_FD;
    return AMerrno;
  }

  /* pagenum 0 should always be the root */
  if (PF_UnpinPage(entry->fd, 0, FALSE) != PFE_OK)
  {
    AMerrno = AME_FD;
    return AMerrno;
  }

  if (PF_CloseFile(entry->fd) != PFE_OK)
  {
    AMerrno = AME_FD;

    return AMerrno;
  }

  entry->in_use = FALSE;
  free(entry->filename);
  entry->filename = 0;
  return AME_OK;
}

int AM_OpenIndexScan(int am_fd, int operation, char *key)
{
  scan_table_entry *entry;
  int i;
  int scan_id = -1;

  if (am_fd < 0 || am_fd >= AM_ITAB_SIZE || !index_table[am_fd].in_use)
  {
    AMerrno = AME_FD;

    return AMerrno;
  }

  /* If the key is null, allow any operation */
  if (key != 0 && !(operation == EQ_OP || operation == LT_OP || operation == GT_OP || operation == LE_OP || operation == GE_OP || operation == NE_OP))
  {
    AMerrno = AME_INVALIDOP;

    return AMerrno;
  }

  for (i = 0; i < MAXISCANS; ++i)
  {
    if (!scan_table[i].in_use)
    {
      scan_id = i;
      break;
    }
  }
  if (scan_id == -1)
  {
    AMerrno = AME_SCANTABLEFULL;

    return AMerrno;
  }

  entry = &scan_table[scan_id];
  entry->in_use = TRUE;
  entry->am_fd = am_fd;
  entry->operation = operation;
  entry->key = key;
  entry->index_in_node = 0;
  entry->node = 0;

  return scan_id;
}

int AM_CloseIndexScan(int scan_id)
{
  scan_table_entry *entry;
  if (scan_id < 0 || scan_id >= MAXISCANS || !scan_table[scan_id].in_use) {
    AMerrno = AME_INVALIDSCANDESC;
    return AMerrno;
  }

  entry = &scan_table[scan_id];
  entry->in_use = FALSE;
  entry->am_fd = -1;
  entry->operation = -1;
  entry->key = 0;
  entry->index_in_node = 0;
  entry->node = 0;

  return AME_OK;
}

/*
Find the correct index with binary search
Possible cases.
Either simple case where we can just insert without trouble
Push up and split
Split recursively
Leaf with duplicate keys are trying to split
 * Duplicate keys are already a key in parent => 
          push lower / higher depending on new key
 * Always keep duplicate values on the same leaf
   Move whichever side of the dups. that have fewer elements
*/

int AM_InsertEntry(int am_fd, char *key, RECID value) {
  (void)am_fd;
  (void)key;
  (void)value;
  return 0;
}

RECID AM_FindNextEntry(int scan_id) {
  RECID value;
  (void) scan_id;
  return value;
}