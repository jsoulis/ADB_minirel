#ifndef __AM_DATA_STRUCTURES__
#define __AM_DATA_STRUCTURES__

#include "stdint.h"
#include "minirel.h"

typedef enum { INTERNAL, LEAF } node_type;

typedef struct
{
  char *key;
  int page_num;
} internal_node_key_value_pair;

typedef struct
{
  node_type type;
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
  node_type type;
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
  char *filename;
  internal_node *root;
} index_table_entry;

typedef struct
{
  bool_t in_use;
  int am_fd;
  int operation;
  char *key;
  int index_in_node;
  leaf_node *node;
} scan_table_entry;

#endif