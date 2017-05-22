#include "stdlib.h"
#include "string.h"

#include "minirel.h"

#include "am.h"
#include "data_structures.h"
#include "hf.h"
#include "pf.h"
#include "utils.h"

index_table_entry index_table[AM_ITAB_SIZE];
scan_table_entry scan_table[MAXISCANS];

void AM_Init() {
  int i;
  HF_Init();

  for (i = 0; i < AM_ITAB_SIZE; ++i) {
    index_table[i].in_use = FALSE;
    index_table[i].filename = 0;
  }

  for (i = 0; i < MAXISCANS; ++i) {
    scan_table[i].in_use = FALSE;
    scan_table[i].key_index = 0;
  }
}

int AM_CreateIndex(char *filename, int index_no, char attr_type,
                   int attr_length, bool_t is_unique) {
  internal_node *root;
  int fd, pagenum;
  char *filename_with_index;

  if (!(attr_type == 'c' || attr_type == 'i' || attr_type == 'f')) {
    AMerrno = AME_INVALIDATTRTYPE;
    return AMerrno;
  }

  if (attr_length < 1 || attr_length > 255) {
    AMerrno = AME_INVALIDATTRLENGTH;
    return AMerrno;
  }

  /* add 1 to size to fit NULL character */
  filename_with_index = malloc(sizeof_filename_with_index(filename, index_no));
  set_filename_with_index(filename, index_no, filename_with_index);

  if (PF_CreateFile(filename_with_index) != PFE_OK) {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  fd = PF_OpenFile(filename_with_index);
  if (fd < 0) {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  if (PF_AllocPage(fd, &pagenum, ((char **)&root)) != PFE_OK) {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  if (attr_type == 'f' || attr_type == 'i') {
    attr_length = 4;
  }

  root->valid_entries = 0;
  root->key_type = attr_type;
  /* max length is 255 */
  root->key_length = (uint8_t)attr_length;

  /* Unpin and mark dirty */
  if (PF_UnpinPage(fd, pagenum, TRUE) != PFE_OK) {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  if (PF_CloseFile(fd) != PFE_OK) {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  free(filename_with_index);
  /* stop complaining about unused variable */
  (void)is_unique;
  return AME_OK;
}

int AM_DestroyIndex(char *filename, int index_no) {
  int return_code = AME_OK;
  char *filename_with_index =
      malloc(sizeof_filename_with_index(filename, index_no));
  set_filename_with_index(filename, index_no, filename_with_index);
  /* PF won't destroy a pinned file. So if we have the root pinned we can just
   * check like this */
  if (PF_DestroyFile(filename_with_index) != PFE_OK) {
    AMerrno = AME_PF;
    return_code = AMerrno;
  }

  free(filename_with_index);
  return return_code;
}

int AM_OpenIndex(char *filename, int index_no) {
  char *filename_with_index;
  int i, fd, pagenum, am_fd = -1;

  /* Check if there is space in table */
  for (i = 0; i < AM_ITAB_SIZE; ++i) {
    if (!index_table[i].in_use) {
      am_fd = i;
      break;
    }
  }
  if (am_fd == -1) {
    AMerrno = AME_FULLTABLE;
    return AMerrno;
  }

  filename_with_index = malloc(sizeof_filename_with_index(filename, index_no));
  set_filename_with_index(filename, index_no, filename_with_index);

  for (i = 0; i < AM_ITAB_SIZE; ++i) {
    if (index_table[i].filename &&
        strcmp(index_table[i].filename, filename_with_index) == 0) {
      free(filename_with_index);

      AMerrno = AME_DUPLICATEOPEN;
      return AMerrno;
    }
  }

  fd = PF_OpenFile(filename_with_index);
  if (fd < 0) {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  if (PF_GetFirstPage(fd, &pagenum, (char **)&index_table[am_fd].root) !=
      PFE_OK) {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  index_table[am_fd].in_use = TRUE;
  index_table[am_fd].fd = fd;
  index_table[am_fd].filename = filename_with_index;
  return am_fd;
}

int AM_CloseIndex(int am_fd) {
  int i;
  index_table_entry *entry;

  if (am_fd < 0 || am_fd >= AM_ITAB_SIZE) {
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
  if (!entry->in_use) {
    AMerrno = AME_FD;
    return AMerrno;
  }

  /* pagenum 0 should always be the root */
  if (PF_UnpinPage(entry->fd, 0, FALSE) != PFE_OK) {
    AMerrno = AME_FD;
    return AMerrno;
  }

  if (PF_CloseFile(entry->fd) != PFE_OK) {
    AMerrno = AME_FD;

    return AMerrno;
  }

  entry->in_use = FALSE;
  free(entry->filename);
  entry->filename = 0;
  return AME_OK;
}

int AM_OpenIndexScan(int am_fd, int operation, char *key) {
  scan_table_entry *entry;
  int i;
  int scan_id = -1;

  if (am_fd < 0 || am_fd >= AM_ITAB_SIZE || !index_table[am_fd].in_use) {
    AMerrno = AME_FD;

    return AMerrno;
  }

  /* If the key is null, allow any operation */
  if (key != 0 &&
      !(operation == EQ_OP || operation == LT_OP || operation == GT_OP ||
        operation == LE_OP || operation == GE_OP || operation == NE_OP)) {
    AMerrno = AME_INVALIDOP;

    return AMerrno;
  }

  for (i = 0; i < MAXISCANS; ++i) {
    if (!scan_table[i].in_use) {
      scan_id = i;
      break;
    }
  }
  if (scan_id == -1) {
    AMerrno = AME_SCANTABLEFULL;

    return AMerrno;
  }

  entry = &scan_table[scan_id];
  entry->in_use = TRUE;
  entry->am_fd = am_fd;
  entry->operation = operation;
  entry->key = key;
  entry->key_index = 0;
  entry->leaf = 0;

  return scan_id;
}

int AM_CloseIndexScan(int scan_id) {
  scan_table_entry *scan_entry;
  index_table_entry *index_entry;
  if (scan_id < 0 || scan_id >= MAXISCANS || !scan_table[scan_id].in_use) {
    AMerrno = AME_INVALIDSCANDESC;
    return AMerrno;
  }

  scan_entry = &scan_table[scan_id];
  index_entry = &index_table[scan_entry->am_fd];

  if (scan_entry->leaf != NULL &&
      PF_UnpinPage(index_entry->fd, scan_entry->leaf_pagenum, FALSE) !=
          PFE_OK) {
    AMerrno = AME_PF;
    return AMerrno;
  }

  scan_entry->in_use = FALSE;
  scan_entry->am_fd = -1;
  scan_entry->operation = -1;
  scan_entry->key = 0;
  scan_entry->key_index = 0;
  scan_entry->leaf = 0;

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

int AM_InsertEntry(int am_fd, char *key, RECID value) 
{
  index_table_entry *entry;
  leaf_node *le_node;
  int ptr_index, pagenum;
  int err;

  if (am_fd < 0 || am_fd >= AM_ITAB_SIZE || !index_table[am_fd].in_use) {
    AMerrno = AME_FD;
    return AMerrno;
  }

  entry = &index_table[am_fd];

  if (entry->root->valid_entries == 0) {
    if ((err = initialize_root_node(entry, key)) != AME_OK) {
      AMerrno = err;
      return AMerrno;
    }
  }

  le_node = find_leaf(entry->fd, entry->root, key, &pagenum);
  ptr_index = find_ptr_index(key, le_node->key_length, le_node->key_type,
                             LEAF_NODE_PTR_SIZE, le_node->pairs,
                             le_node->valid_entries);
  *(RECID*)get_ptr_address(le_node->pairs, le_node->key_length, LEAF_NODE_PTR_SIZE, ptr_index) = value;
  memcpy(get_key_address(le_node->pairs, le_node->key_length,
                         LEAF_NODE_PTR_SIZE, ptr_index),
         key, le_node->key_length);
  ++le_node->valid_entries;
  if (PF_UnpinPage(entry->fd, pagenum, TRUE) != PFE_OK) {
    AMerrno = AME_PF;
    return AMerrno;
  }
  return AME_OK;
}

RECID AM_FindNextEntry(int scan_id) {
  RECID invalid_rec;
  int err;
  scan_table_entry *scan_entry;
  index_table_entry *index_entry;
  char *key_address;
  RECID *ptr_address;

  invalid_rec.recnum = -1, invalid_rec.pagenum = -1;

  if (scan_id < 0 || scan_id >= MAXISCANS || !scan_table[scan_id].in_use) {
    AMerrno = AME_INVALIDSCANDESC;
    return invalid_rec;
  }

  scan_entry = &scan_table[scan_id];
  index_entry = &index_table[scan_entry->am_fd];

  if (!scan_entry->leaf) {
    scan_entry->leaf = find_leaf(index_entry->fd, index_entry->root,
                            scan_entry->key, &scan_entry->leaf_pagenum);
    /* If it's a NE operation, just go from the start and skip the NE elements
     */
    if (scan_entry->key && scan_entry->operation != NE_OP) {
      /* ptr index = key index and it's now GE to to key */
      scan_entry->key_index = find_ptr_index(
          scan_entry->key, scan_entry->leaf->key_length, scan_entry->leaf->key_type,
          LEAF_NODE_PTR_SIZE, scan_entry->leaf->pairs, scan_entry->leaf->valid_entries);
      /* We want to point to the index before */
      switch (scan_entry->operation) {
      case GE_OP:
      case LE_OP:
      case EQ_OP:
        --scan_entry->key_index;
        break;
      }
    } else {
      scan_entry->key_index = -1;
    }
  }

  if ((err = update_scan_entry_key_index(scan_entry, index_entry)) != AME_OK) {
    AMerrno = err;
    return invalid_rec;
  }

  /* Just scan through */
  if (!scan_entry->key) {
    ptr_address = (RECID *)get_ptr_address(scan_entry->leaf->pairs,
                                    scan_entry->leaf->key_length,
                                    LEAF_NODE_PTR_SIZE, scan_entry->key_index);
  } else {
    key_address =
        get_key_address(scan_entry->leaf->pairs, scan_entry->leaf->key_length,
                        LEAF_NODE_PTR_SIZE, scan_entry->key_index);
    switch (scan_entry->operation) {
    case EQ_OP:
    case LT_OP:
    case GT_OP:
    case LE_OP:
    case GE_OP:
      if (is_operation_true(scan_entry->key, key_address, scan_entry->leaf->key_length,
                            scan_entry->leaf->key_type,
                            scan_entry->operation)) {
        ptr_address = (RECID *)get_ptr_address(
            scan_entry->leaf->pairs, scan_entry->leaf->key_length,
            LEAF_NODE_PTR_SIZE, scan_entry->key_index);
      } else {
        AMerrno = AME_EOF;
        ptr_address = &invalid_rec;
      }
      break;
    case NE_OP:
      if (is_operation_true(scan_entry->key, key_address, scan_entry->leaf->key_length,
                            scan_entry->leaf->key_type,
                            scan_entry->operation)) {
        /* if NE we want to skip, update index */
        if ((err = update_scan_entry_key_index(scan_entry, index_entry)) !=
            AME_OK) {
          AMerrno = err;
          ptr_address = &invalid_rec;
        } else {
          ptr_address = (RECID *)get_ptr_address(
              scan_entry->leaf->pairs, scan_entry->leaf->key_length,
              LEAF_NODE_PTR_SIZE, scan_entry->key_index);
        }
      }
    }
  }
  return *ptr_address;
}