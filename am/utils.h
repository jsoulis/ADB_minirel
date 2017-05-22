#ifndef __AM_UTILS_H__
#define __AM_UTILS_H__

#include "string.h"

#include "minirel.h"
#include "data_structures.h"

int sizeof_filename_with_index(char *filename, int indexNo);

void set_filename_with_index(char *filename, int indexNo, char *updatedName);

int max_node_count(uint8_t key_length);

/* Find the pointer index where to input the new key-value pair */
int find_ptr_index(const char *key, uint8_t key_length, uint8_t key_type, uint8_t ptr_length, char *pairs, int key_count);

bool_t is_operation_true(const char *a, const char *b, uint8_t key_length, uint8_t key_type, int operation);

char* get_key_address(char *pairs, uint8_t key_length, uint8_t ptr_length, int index);
char* get_ptr_address(char *pairs, uint8_t key_length, uint8_t ptr_length, int index);

/*
 Should be called when the first key is inserted 
Returns AME_OK on success, error otherwise
Caller should set AMerrno
 */
int initialize_root_node(index_table_entry *entry, char *key);
/* Returns the leaf node and sets the pagenum on which it's at */
leaf_node *find_leaf(int fd, internal_node *root, const char *key, int *pagenum);
/* returns AME_OK on success */
int update_scan_entry_key_index(scan_table_entry *scan_entry, index_table_entry *index_entry);

#endif