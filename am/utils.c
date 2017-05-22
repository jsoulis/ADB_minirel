#include "utils.h"

#include "string.h"

#include "am.h"
#include "data_structures.h"
#include "minirel.h"
#include "pf.h"

int sizeof_filename_with_index(char *filename, int indexNo) {
  /* +1 because a dot is added */
  int length = strlen(filename) + 1;
  do {
    ++length;
    indexNo /= 10;
  } while (indexNo);

  /* Fit null character too */
  return length + 1;
}

void set_filename_with_index(char *filename, int indexNo, char *updatedName) {
  int backwardsIndex;
  /* Don't count NULL character */
  const int newLength = sizeof_filename_with_index(filename, indexNo) - 1;

  strcpy(updatedName, filename);
  backwardsIndex = newLength;

  updatedName[backwardsIndex] = '\0';
  do {
    /* turn into ascii */
    updatedName[--backwardsIndex] = (indexNo % 10) + 48;
    indexNo /= 10;
  } while (indexNo);

  updatedName[backwardsIndex - 1] = '.';
}

int max_node_count(uint8_t key_length) {
  /* leaf nodes requires more memory, so let's just use that as an upper bound
   */
  int header_size, key_value_size, node_count;
  header_size = sizeof(leaf_node);
  key_value_size = key_length + sizeof(RECID);

  node_count = (PAGE_SIZE - header_size) / key_value_size;

  return node_count;
}

bool_t is_operation_true(const char *a, const char *b, uint8_t key_length,
                         uint8_t key_type, int operation) {
  int a_i, b_i;
  float a_f, b_f;

  int memcmp_code;
  switch (key_type) {
  case 'i':
    a_i = *(int *)a;
    b_i = *(int *)b;
    switch (operation) {
    case EQ_OP:
      return a_i == b_i;
    case LT_OP:
      return a_i < b_i;
    case GT_OP:
      return a_i > b_i;
    case LE_OP:
      return a_i <= b_i;
    case GE_OP:
      return a_i >= b_i;
    case NE_OP:
      return a_i != b_i;
    }
    break;
  case 'f':
    a_f = *(float *)a;
    b_f = *(float *)b;
    switch (operation) {
    case EQ_OP:
      return a_f == b_f;
    case LT_OP:
      return a_f < b_f;
    case GT_OP:
      return a_f > b_f;
    case LE_OP:
      return a_f <= b_f;
    case GE_OP:
      return a_f >= b_f;
    case NE_OP:
      return a_f != b_f;
    }
    break;
  case 'c':
    memcmp_code = memcmp(a, b, key_length);
    switch (operation) {
    case EQ_OP:
      return memcmp_code == 0;
    case LT_OP:
      return memcmp_code < 0;
    case GT_OP:
      return memcmp_code > 0;
    case LE_OP:
      return memcmp_code == 0 || memcmp_code < 0;
    case GE_OP:
      return memcmp_code == 0 || memcmp_code > 0;
    case NE_OP:
      return memcmp_code != 0;
    }
  }

  return FALSE;
}

int find_ptr_index_internal(const char *key, uint8_t key_length,
                            uint8_t key_type, char *pairs, int key_count) {
  int i;

  for (i = 0; i < key_count; ++i) {
    if (is_operation_true(key, get_key_address_internal(pairs, key_length, i),
                          key_length, key_type, LT_OP)) {
      break;
    }
  }
  /* If it's never less than, the last spot is the index we want to insert into
   */
  return i;
}

char *get_key_address_internal(char *pairs, uint8_t key_length, int index) {
  return (pairs + INTERNAL_NODE_PTR_SIZE) +
         (key_length + INTERNAL_NODE_PTR_SIZE) * index;
}
int *get_ptr_address_internal(char *pairs, uint8_t key_length, int index) {
  return (int *)(pairs + (key_length + INTERNAL_NODE_PTR_SIZE) * index);
}

int find_ptr_index_leaf(const char *key, uint8_t key_length, uint8_t key_type,
                        char *pairs, int key_count) {
  int i;

  for (i = 0; i < key_count; ++i) {
    if (is_operation_true(key, get_key_address_leaf(pairs, key_length, i),
                          key_length, key_type, LE_OP)) {
      break;
    }
  }
  /* If it's never less than, the last spot is the index we want to insert into
   */
  return i;
}

char *get_key_address_leaf(char *pairs, uint8_t key_length, int index) {
  return pairs + (key_length + LEAF_NODE_PTR_SIZE) * index;
}

RECID *get_ptr_address_leaf(char *pairs, uint8_t key_length, int index) {
  return (RECID *)((pairs + key_length) +
                   (key_length + LEAF_NODE_PTR_SIZE) * index);
}

int initialize_root_node(index_table_entry *entry, char *key) {
  int pagenum, pagenum_2;
  leaf_node *le_node, *le_node_2;
  char *key_address;
  int *ptr_address;

  internal_node *root = entry->root;
  /* Make space for the pointer itself, and then the rest of the memory is for
   * the pairs */
  root->pairs = (char *)&root->pairs + sizeof(char *);

  /* TODO: Make sure to unpin later */
  if (PF_AllocPage(entry->fd, &pagenum, (char **)&le_node) != PFE_OK ||
      PF_AllocPage(entry->fd, &pagenum_2, (char **)&le_node_2) != PFE_OK) {
    return AME_PF;
  }

  le_node->type = LEAF;
  le_node->valid_entries = 0;
  le_node->key_type = root->key_type;
  le_node->key_length = root->key_length;
  le_node->prev_leaf = -1;
  le_node->next_leaf = pagenum_2;

  le_node_2->type = LEAF;
  le_node_2->valid_entries = 0;
  le_node_2->key_type = root->key_type;
  le_node_2->key_length = root->key_length;
  le_node_2->prev_leaf = pagenum;
  le_node_2->next_leaf = -1;

  if (PF_UnpinPage(entry->fd, pagenum, TRUE) != PFE_OK ||
      PF_UnpinPage(entry->fd, pagenum_2, TRUE) != PFE_OK) {
    return AME_PF;
  }

  key_address = get_key_address_internal(root->pairs, root->key_length, 0);
  memcpy(key_address, key, root->key_length);

  ptr_address = get_ptr_address_internal(root->pairs, root->key_length, 0);
  memcpy(ptr_address, &pagenum, INTERNAL_NODE_PTR_SIZE);

  ptr_address = get_ptr_address_internal(root->pairs, root->key_length, 1);
  memcpy(ptr_address, &pagenum_2, INTERNAL_NODE_PTR_SIZE);

  /* valid entries == keys */
  root->valid_entries = 1;

  return AME_OK;
}

/* Will unpin internal pages fetched inside, but not the one given */
leaf_node *find_leaf(int fd, internal_node *root, const char *key,
                     int *pagenum) {
  int ptr_index;
  int prev_pagenum = 0;
  leaf_node *le_node;
  internal_node *in_node = root;
  do {
    in_node->pairs = (char *)&in_node->pairs + sizeof(char *);

    /* If no key, go to first child */
    if (key) {
      ptr_index =
          find_ptr_index_internal(key, in_node->key_length, in_node->key_type,
                                  in_node->pairs, in_node->valid_entries);
    } else {
      ptr_index = 0;
    }
    *pagenum = *get_ptr_address_internal(in_node->pairs, in_node->key_length,
                                         ptr_index);

    PF_GetThisPage(fd, *pagenum, (char **)&in_node);

    /* Don't unpin root */
    if (prev_pagenum != 0) {
      PF_UnpinPage(fd, prev_pagenum, FALSE);
    }

    prev_pagenum = *pagenum;
  } while (in_node->type == INTERNAL);

  le_node = (leaf_node *)in_node;
  le_node->pairs = (char *)&le_node->pairs + sizeof(char *);

  return le_node;
}

int update_scan_entry_key_index(scan_table_entry *scan_entry,
                                index_table_entry *index_entry) {
  int prev_pagenum;
  if (scan_entry->key_index + 1 >= scan_entry->leaf->valid_entries) {
    /* fetch new leaf */
    if (scan_entry->leaf->next_leaf == -1) {
      return AME_EOF;
    }

    scan_entry->key_index = 0;
    prev_pagenum = scan_entry->leaf_pagenum;
    scan_entry->leaf_pagenum = scan_entry->leaf->next_leaf;
    if (PF_GetThisPage(index_entry->fd, scan_entry->leaf_pagenum,
                       (char **)&scan_entry->leaf) != PFE_OK) {
      return AME_PF;
    }
    scan_entry->leaf->pairs =
        (char *)&(scan_entry->leaf->pairs) + sizeof(scan_entry->leaf->pairs);

    if (PF_UnpinPage(index_entry->fd, prev_pagenum, FALSE) != PFE_OK) {
      return AME_PF;
    }
  } else {
    ++scan_entry->key_index;
  }

  return AME_OK;
}