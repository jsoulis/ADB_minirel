#ifndef __AM_DATA_STRUCTURES__
#define __AM_DATA_STRUCTURES__

#include "stdint.h"
#include "minirel.h"

typedef struct
{
  char *key;
  int page_num;
} internal_node_key_value_pair;

typedef struct
{
  int valid_entries;
  char key_type;
  /* Maximum size is 255 */
  uint8_t key_length;
  char *pairs;
} internal_node;

typedef struct
{
  char *key;
  RECID value;
} leaf_key_value_pair;

typedef struct
{
  int valid_entries;
  char key_type;
  /* Maximum size is 255 */
  uint8_t key_length;
  int next_leaf, prev_leaf;
  char *pairs;
} leaf_node;

typedef struct
{
  bool_t in_use;
  int fd; /* fd of PF layer */
  internal_node *root;
} index_table_entry;

#endif