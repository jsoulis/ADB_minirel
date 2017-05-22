#include "assert.h"
#include "stdint.h"
#include "stdio.h"
#include "utils.h"
#include "unistd.h"
#include "stdlib.h"
#include "am.h"

int AMerrno;

void test_filename_size()
{
  char *filename = "sweet"; /* strlen("sweet") = 5 */

  /* It includes null character */
  assert(sizeof_filename_with_index(filename, 0) == 8);
  assert(sizeof_filename_with_index(filename, 1) == 8);
  assert(sizeof_filename_with_index(filename, 15) == 9);
  assert(sizeof_filename_with_index(filename, 100) == 10);
}

void test_filename_with_index()
{
  char *filename = "test"; /* len 4 */
  char updated_name_1[7];
  char updated_name_2[9];

  set_filename_with_index(filename, 0, updated_name_1);
  assert(strcmp("test.0", updated_name_1) == 0);

  set_filename_with_index(filename, 1, updated_name_1);
  assert(strcmp("test.1", updated_name_1) == 0);

  set_filename_with_index(filename, 537, updated_name_2);
  assert(strcmp("test.537", updated_name_2) == 0);
}

void test_max_node_count()
{
  /* Header is size 24 without value pointers (because of aligning most probably)
   * One extra RECID at the start, because not all are pairs
   * RECID is size 8
   * Should result in (4096 - 24 - 8) / pair_size
   */
  assert(max_node_count(4) == 338);
  assert(max_node_count(15) == 176);
}

void test_create_destroy_index()
{
  unlink("success1.1");
  unlink("success1.5");
  unlink("success2.987");
  assert(AM_CreateIndex("fail1", 1, 'a', 1, FALSE) == AME_INVALIDATTRTYPE);
  assert(AM_CreateIndex("fail2", 1, 'c', 256, FALSE) == AME_INVALIDATTRLENGTH);
  assert(AM_CreateIndex("success1", 1, 'c', 1, FALSE) == AME_OK);
  assert(AM_CreateIndex("success1", 5, 'c', 1, FALSE) == AME_OK);
  assert(AM_CreateIndex("success2", 987, 'c', 4, FALSE) == AME_OK);

  assert(AM_DestroyIndex("success1", 1) == AME_OK);
  assert(AM_DestroyIndex("success1", 5) == AME_OK);
  assert(AM_DestroyIndex("success2", 987) == AME_OK);

  assert(AM_DestroyIndex("wut", 1) == AME_PF);
  assert(AM_DestroyIndex("success1", 2) == AME_PF);
}

void test_open_close_index()
{
  int i;
  /* No such file */
  assert(AM_OpenIndex("fail", 1) == AME_PF);

  assert(AM_CreateIndex("success", 0, 'c', 1, FALSE) == AME_OK);
  assert(AM_OpenIndex("success", 0) == 0);
  assert(AM_OpenIndex("success", 0) == AME_DUPLICATEOPEN);
  /* No such index */
  assert(AM_OpenIndex("success", 1) == AME_PF);

  /* Fill the table */
  for (i = 1; i < AM_ITAB_SIZE; ++i)
  {
    assert(AM_CreateIndex("success", i, 'c', 1, FALSE) == AME_OK);
    /* Equals i because we're opening in order */
    assert(AM_OpenIndex("success", i) == i);
  }

  assert(AM_OpenIndex("success", 0) == AME_FULLTABLE);

  /* Still open */
  assert(AM_DestroyIndex("success", 0) == AME_PF);
  assert(AM_CloseIndex(0) == AME_OK);
  assert(AM_DestroyIndex("success", 0) == AME_OK);

  /* Close and destroy all indices */
  for (i = 1; i < AM_ITAB_SIZE; ++i)
  {
    assert(AM_DestroyIndex("success", i) == AME_PF);
    assert(AM_CloseIndex(i) == AME_OK);
    assert(AM_DestroyIndex("success", i) == AME_OK);
  }

  assert(AM_CloseIndex(-1) == AME_FD);
  assert(AM_CloseIndex(AM_ITAB_SIZE) == AME_FD);
  /* Already closed */
  assert(AM_CloseIndex(5) == AME_FD);
}

void test_operation()
{
  int i1 = 3;
  int i2 = 7;

  float f1 = 1.3;
  float f2 = 10.3;

  char *c1 = "cool";
  char *c2 = "dada";

  assert(is_operation_true((char *)&i1, (char *)&i1, sizeof(int), 'i', EQ_OP) == TRUE);
  assert(is_operation_true((char *)&i1, (char *)&i2, sizeof(int), 'i', EQ_OP) == FALSE);
  assert(is_operation_true((char *)&i1, (char *)&i2, sizeof(int), 'i', LT_OP) == TRUE);
  assert(is_operation_true((char *)&i1, (char *)&i2, sizeof(int), 'i', GT_OP) == FALSE);
  assert(is_operation_true((char *)&i1, (char *)&i1, sizeof(int), 'i', LE_OP) == TRUE);
  assert(is_operation_true((char *)&i1, (char *)&i2, sizeof(int), 'i', LE_OP) == TRUE);
  assert(is_operation_true((char *)&i2, (char *)&i2, sizeof(int), 'i', GE_OP) == TRUE);
  assert(is_operation_true((char *)&i1, (char *)&i2, sizeof(int), 'i', GE_OP) == FALSE);
  assert(is_operation_true((char *)&i1, (char *)&i2, sizeof(int), 'i', NE_OP) == TRUE);
  assert(is_operation_true((char *)&i1, (char *)&i1, sizeof(int), 'i', NE_OP) == FALSE);

  assert(is_operation_true((char *)&f1, (char *)&f1, sizeof(float), 'f', EQ_OP) == TRUE);
  assert(is_operation_true((char *)&f1, (char *)&f2, sizeof(float), 'f', EQ_OP) == FALSE);
  assert(is_operation_true((char *)&f1, (char *)&f2, sizeof(float), 'f', LT_OP) == TRUE);
  assert(is_operation_true((char *)&f1, (char *)&f2, sizeof(float), 'f', GT_OP) == FALSE);
  assert(is_operation_true((char *)&f1, (char *)&f1, sizeof(float), 'f', LE_OP) == TRUE);
  assert(is_operation_true((char *)&f1, (char *)&f2, sizeof(float), 'f', LE_OP) == TRUE);
  assert(is_operation_true((char *)&f2, (char *)&f2, sizeof(float), 'f', GE_OP) == TRUE);
  assert(is_operation_true((char *)&f1, (char *)&f2, sizeof(float), 'f', GE_OP) == FALSE);
  assert(is_operation_true((char *)&f1, (char *)&f2, sizeof(float), 'f', NE_OP) == TRUE);
  assert(is_operation_true((char *)&f1, (char *)&f1, sizeof(float), 'f', NE_OP) == FALSE);

  assert(is_operation_true(c1, c1, strlen(c1), 'c', EQ_OP) == TRUE);
  assert(is_operation_true(c1, c2, strlen(c1), 'c', EQ_OP) == FALSE);
  assert(is_operation_true(c1, c2, strlen(c1), 'c', LT_OP) == TRUE);
  assert(is_operation_true(c1, c2, strlen(c1), 'c', GT_OP) == FALSE);
  assert(is_operation_true(c1, c1, strlen(c1), 'c', LE_OP) == TRUE);
  assert(is_operation_true(c1, c2, strlen(c1), 'c', LE_OP) == TRUE);
  assert(is_operation_true(c2, c2, strlen(c1), 'c', GE_OP) == TRUE);
  assert(is_operation_true(c1, c2, strlen(c1), 'c', GE_OP) == FALSE);
  assert(is_operation_true(c1, c2, strlen(c1), 'c', NE_OP) == TRUE);
  assert(is_operation_true(c1, c1, strlen(c1), 'c', NE_OP) == FALSE);
}
void test_find_ptr_index_internal()
{
  int keys[] = {1, 0, 3, 11};
  uint8_t key_length = sizeof(int);
  int key_count = 3;
  int i_pairs[] = {0x5, 0x1, 0x6, 0x3, 0x7, 0xA, 0x8};

  char *pairs = (char*)i_pairs;

  int index = find_ptr_index_internal((char*)&keys[0], key_length, 'c', pairs, key_count);
  assert(index == 1);
  assert(*get_ptr_address_internal(pairs, key_length, index) == 6);

  index = find_ptr_index_internal((char*)&keys[1], key_length, 'c', pairs, key_count);
  assert(index == 0);
  assert(*get_ptr_address_internal(pairs, key_length, index) == 5);

  index = find_ptr_index_internal((char*)&keys[2], key_length, 'c', pairs, key_count);
  assert(index == 2);
  assert(*get_ptr_address_internal(pairs, key_length, index) == 7);

  index = find_ptr_index_internal((char*)&keys[3], key_length, 'c', pairs, key_count);
  assert(index == 3);
  assert(*get_ptr_address_internal(pairs, key_length, index) == 8);
}

void test_find_ptr_index_leaf() 
{
  int keys[] = {1, 5, 11, 12};
  uint8_t key_length = sizeof(int);
  int key_count = 3;
  int i;
  RECID values[3];
  RECID value;
  char *pairs;
  values[0].pagenum = 1, values[0].recnum = 2;
  values[1].pagenum = 3, values[1].recnum = 4;
  values[2].pagenum = 5, values[2].recnum = 6;

  pairs = malloc((sizeof(RECID) + key_length) * key_count);
  for (i = 0; i < key_count; ++i) {
    memcpy(pairs + (key_length + sizeof(RECID)) * i, &keys[i], key_length);
    memcpy(pairs + key_length + (key_length + sizeof(RECID)) * i, &values[i], sizeof(RECID));
  }

  i = find_ptr_index_leaf((char*)&keys[0], key_length, 'i', pairs, key_count);
  assert(i == 0);
  value = *get_ptr_address_leaf(pairs, key_length, i);
  assert(value.recnum == values[i].recnum && value.pagenum == values[i].pagenum);

  i = find_ptr_index_leaf((char*)&keys[1], key_length, 'i', pairs, key_count);
  assert(i == 1);
  value = *get_ptr_address_leaf(pairs, key_length, i);
  assert(value.recnum == values[i].recnum && value.pagenum == values[i].pagenum);

  i = find_ptr_index_leaf((char*)&keys[2], key_length, 'i', pairs, key_count);
  assert(i == 2);
  value = *get_ptr_address_leaf(pairs, key_length, i);
  assert(value.recnum == values[i].recnum && value.pagenum == values[i].pagenum);

  /* Point outside of the max index when it's greater */
  i = find_ptr_index_leaf((char*)&keys[3], key_length, 'i', pairs, key_count);
  assert(i == 3);

  free(pairs);
}

void test_open_close_scan() {
  int key = 1;
  int i = 0;

  assert(AM_OpenIndexScan(i, EQ_OP, (char*)&key) == AME_FD);

  assert(AM_CreateIndex("scan", i, 'i', 4, FALSE) == AME_OK);
  assert(AM_OpenIndexScan(i, EQ_OP, (char*)&key) == AME_FD);

  assert(AM_OpenIndex("scan", i) == i);

  assert(AM_OpenIndexScan(i, -1, (char*)&key) == AME_INVALIDOP);
  assert(AM_OpenIndexScan(i, 7, (char*)&key) == AME_INVALIDOP);

  /* NULL pointer shouldn't care about the operation */
  assert(AM_OpenIndexScan(i, 7, 0) == i);

  for (i = 1; i < MAXISCANS; ++i)
  {
    assert(AM_CreateIndex("scan", i, 'i', 4, FALSE) == AME_OK);
    assert(AM_OpenIndex("scan", i) == i);

    assert(AM_OpenIndexScan(i, EQ_OP, (char *)&key) == i);
  }

  assert(AM_OpenIndexScan(0, EQ_OP, (char *)&key) == AME_SCANTABLEFULL);

  assert(AM_CloseIndex(0) == AME_SCANOPEN);
  assert(AM_CloseIndexScan(0) == AME_OK);
  assert(AM_CloseIndex(0) == AME_OK);
  assert(AM_DestroyIndex("scan", 0) == AME_OK);

  for (i = 1; i < MAXISCANS; ++i)
  {
    assert(AM_CloseIndexScan(i) == AME_OK);
    assert(AM_CloseIndex(i) == AME_OK);
    assert(AM_DestroyIndex("scan", i) == AME_OK);
  }

  assert(AM_CloseIndexScan(-1) == AME_INVALIDSCANDESC);
  assert(AM_CloseIndexScan(MAXISCANS) == AME_INVALIDSCANDESC);
  /* Already closed */
  assert(AM_CloseIndexScan(5) == AME_INVALIDSCANDESC);
}

void test_insert_invalid_fd() {
  int key = 1;
  int am_fd = 0;
  RECID value; value.recnum = 0, value.pagenum = 0;

  assert(AM_InsertEntry(0, (char*)&key, value) == AME_FD);
  assert(AM_InsertEntry(-1, (char*)&key, value) == AME_FD);
  assert(AM_InsertEntry(AM_ITAB_SIZE, 0, value) == AME_FD);

  assert(AM_CreateIndex("insert_invalid", 0, 'i', 4, FALSE) == AME_OK);
  assert(AM_OpenIndex("insert_invalid", 0) == am_fd);
  assert(AM_InsertEntry(am_fd, (char*)&key, value) == AME_OK);

  assert(AM_CloseIndex(am_fd) == AME_OK);
  assert(AM_DestroyIndex("insert_invalid", 0) == AME_OK);
}

void test_find_next_entry_invalid_id() {

  int scan_id = 0;
  RECID value;

  value = AM_FindNextEntry(-1);
  assert(value.recnum == -1 && value.pagenum == -1 && AMerrno == AME_INVALIDSCANDESC);
  value = AM_FindNextEntry(MAXISCANS);
  assert(value.recnum== -1 && value.pagenum == -1 && AMerrno == AME_INVALIDSCANDESC);
  /* Not yet open */
  value = AM_FindNextEntry(scan_id);
  assert(value.recnum == -1 && value.pagenum == -1 && AMerrno == AME_INVALIDSCANDESC);
}

void test_insert_scan_simple() {
  int am_fd = 0;
  int scan_id = 0;
  int key = 1;
  RECID value, invalid_value, next_entry;
  value.pagenum = 10, value.recnum = 11;
  invalid_value.pagenum = -1, invalid_value.recnum = -1;

  assert(AM_CreateIndex("insert", 0, 'i', 4, FALSE) == AME_OK);
  assert(AM_OpenIndex("insert", 0) == am_fd);
  assert(AM_InsertEntry(am_fd, (char *)&key, value) == AME_OK);

  assert(AM_OpenIndexScan(am_fd, -1, 0) == scan_id);

  next_entry = AM_FindNextEntry(scan_id);
  assert(next_entry.pagenum == value.pagenum &&
         next_entry.recnum == value.recnum);
  next_entry = AM_FindNextEntry(scan_id);
  assert(next_entry.pagenum == invalid_value.pagenum &&
         next_entry.recnum == invalid_value.recnum);

  assert(AM_CloseIndexScan(scan_id) == AME_OK);
  assert(AM_CloseIndex(am_fd) == AME_OK);
  assert(AM_DestroyIndex("insert", 0) == AME_OK);
}

/* Insert a bigger element before a smaller one.
They should now trade places */
void test_insert_scan_reorder() {
  int am_fd = 0;
  int scan_id = 0;

  int key1 = 100, key2 = 50, key3 = 3;
  RECID value1, value2, value3, invalid_value, next_entry;
  invalid_value.pagenum = -1, invalid_value.recnum = -1;

  value1.pagenum = 10, value1.recnum = 11;
  value2.pagenum = 100, value2.recnum = 110;
  value3.pagenum = 1, value3.recnum = 5;

  assert(AM_CreateIndex("insert", 0, 'i', 4, FALSE) == AME_OK);
  assert(AM_OpenIndex("insert", 0) == am_fd);

  assert(AM_InsertEntry(am_fd, (char *)&key1, value1) == AME_OK);
  assert(AM_InsertEntry(am_fd, (char *)&key2, value2) == AME_OK);
  assert(AM_InsertEntry(am_fd, (char *)&key3, value3) == AME_OK);

  /* Should now be in the order key3, key2, key1 */

  assert(AM_OpenIndexScan(am_fd, -1, 0) == scan_id);

  next_entry = AM_FindNextEntry(scan_id);
  assert(next_entry.pagenum == value3.pagenum &&
         next_entry.recnum == value3.recnum);

  next_entry = AM_FindNextEntry(scan_id);
  assert(next_entry.pagenum == value2.pagenum &&
         next_entry.recnum == value2.recnum);

  next_entry = AM_FindNextEntry(scan_id);
  assert(next_entry.pagenum == value1.pagenum &&
         next_entry.recnum == value1.recnum);

  next_entry = AM_FindNextEntry(scan_id);
  assert(next_entry.pagenum == invalid_value.pagenum &&
         next_entry.recnum == invalid_value.recnum);

  assert(AM_CloseIndexScan(scan_id) == AME_OK);
  assert(AM_CloseIndex(am_fd) == AME_OK);
  assert(AM_DestroyIndex("insert", 0) == AME_OK);
}

int main()
{
  test_filename_size();
  test_filename_with_index();
  test_max_node_count();
  test_operation();
  test_find_ptr_index_internal();
  test_find_ptr_index_leaf();

  /* AM functions */
  AM_Init();
  test_create_destroy_index();
  test_open_close_index();
  test_open_close_scan();
  
  test_insert_invalid_fd();
  test_find_next_entry_invalid_id();
  test_insert_scan_simple();
  test_insert_scan_reorder();

  printf("Passed all tests\n");
  return 0;
}
