#ifndef __AM_UTILS_H__
#define __AM_UTILS_H__

#include "string.h"

int sizeof_filename_with_index(char* filename, int indexNo) {
  /* +1 because a dot is added */
  int length = strlen(filename) + 1;
  while (indexNo) {
    ++length;
    indexNo /= 10;
  }

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
  while (indexNo) {
    /* turn into ascii */
    updatedName[--backwardsIndex] = (indexNo % 10) + 48;
    indexNo /= 10;
  }
  updatedName[backwardsIndex - 1] = '.';
}

#endif