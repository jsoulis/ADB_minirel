#include "minirel.h"

#include "am.h"
#include "data_structures.h"
#include "hf.h"

index_table_entry index_table[AM_ITAB_SIZE];

void AM_Init() { HF_Init(); }