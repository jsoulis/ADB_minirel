#ifndef __AM_UTILS_H__
#define __AM_UTILS_H__

#include "string.h"

#include "minirel.h"
#include "data_structures.h"

int sizeof_filename_with_index(char *filename, int indexNo);

void set_filename_with_index(char *filename, int indexNo, char *updatedName);

int max_node_count(uint8_t key_length);


bool_t is_operation_true(const char *a, const char *b, uint8_t key_length, uint8_t key_type, int operation);

int find_ptr_index_internal(const char *key, uint8_t key_length, uint8_t key_type, char *pairs, int key_count);
char* get_key_address_internal(char *pairs, uint8_t key_length, int index);
int* get_ptr_address_internal(char *pairs, uint8_t key_length, int index);

int find_ptr_index_leaf(const char *key, uint8_t key_length, uint8_t key_type, char *pairs, int key_count);
char* get_key_address_leaf(char *pairs, uint8_t key_length, int index);
RECID* get_ptr_address_leaf(char *paris, uint8_t key_length, int index);

/*
 Should be called when the first key is inserted 
Returns AME_OK on success, error otherwise
Caller should set AMerrno
 */
int initialize_root_node(index_table_entry *entry, char *key);
leaf_node *find_leaf(int fd, internal_node *root, const char *key);
/* returns AME_OK on success */
int update_scan_entry_key_index(scan_table_entry *scan_entry, index_table_entry *index_entry);
/* returns error if couldn't merge, AME_OK otherwise */
int merge(index_table_entry *entry, char *key, leaf_node *le_node);
int find_parent(int fd, internal_node *root, const char *key, internal_node **parent);

#endif