#ifndef __AM_UTILS_H__
#define __AM_UTILS_H__

#include "string.h"

#include "minirel.h"
#include "data_structures.h"

int sizeof_filename_with_index(char *filename, int indexNo);

void set_filename_with_index(char *filename, int indexNo, char *updatedName);

int max_node_count(uint8_t key_length);

#endif