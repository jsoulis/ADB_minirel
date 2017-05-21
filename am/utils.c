#include "utils.h"

#include "string.h"

#include "minirel.h"
#include "data_structures.h"

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
  header_size = sizeof(leaf_node) - sizeof(char*);
  key_value_size = key_length + sizeof(RECID);

  /* There's one extra pointer at the start that's not exactly in a pair */
  node_count = (PAGE_SIZE - header_size - sizeof(RECID)) / key_value_size;

  return node_count;
}

bool_t is_operation_true(const char *a, const char *b, uint8_t key_length, uint8_t key_type, int operation)
{
  int a_i, b_i;
  float a_f, b_f;
  
  int memcmp_code;
  switch (key_type)
  {
  case 'i':
    a_i = *(int *)a;
    b_i = *(int *)b;
    switch (operation)
    {
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
    a_f = *(float*) a;
    b_f = *(float*) b; 
    switch( operation) {
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
    switch( operation) {
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


int find_ptr_index(const char *key, uint8_t key_length, uint8_t key_type, uint8_t ptr_length, char *pairs, int key_count)
{
  int i;

  for (i = 0; i <= key_count; ++i) {
    if (is_operation_true(key, get_key_address(pairs, key_length, ptr_length, i), key_length, key_type, LT_OP)) {
      break;
    }
  }
  /* If it's never less than, the last spot is the index we want to insert into */
  return i;
}

char* get_key_address(char *pairs, uint8_t key_length, uint8_t ptr_length, int index) {
  return (pairs + ptr_length) + (key_length + ptr_length) * index;
}
char* get_ptr_address(char *pairs, uint8_t key_length, uint8_t ptr_length, int index) {
  return pairs + (key_length + ptr_length) * index;
}