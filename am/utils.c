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

/*
int binary_search(const char *key, uint8_t key_length, uint8_t ptr_length, const char *pairs, int key_count) {
  int middle_index;

  middle_index = key_length + key_count / 2;
}
*/