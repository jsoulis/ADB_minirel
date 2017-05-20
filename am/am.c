#include "stdint.h"
#include "minirel.h"

#include "am.h"

typedef struct {
  char *key;
  int pageNum;
} internal_node_key_value_pair;

typedef struct {
  int validEntries;
  char keyType;
  /* Maximum size is 255 */
  uint8_t keyLength;
  internal_node_key_value_pair *pairs;
} internal_node;

typedef struct {
  char *key;
  RECID value;
} leaf_key_value_pair;

typedef struct {
  int validEntries;
  char keyType;
  /* Maximum size is 255 */
  uint8_t keyLength;
  int nextLeaf, prevLeaf;
  internal_node_key_value_pair *pairs;
} leaf_node;