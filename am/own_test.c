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
void test_find_ptr_index()
{
  char keys[] = {(char)1, (char)0, (char)3, (char)11};
  uint8_t key_length = sizeof(char);
  uint8_t ptr_length = sizeof(char);
  int key_count = 3;
  char pairs[] = {0x5, 0x1, 0x6, 0x3, 0x7, 0xA, 0x8};

  int index = find_ptr_index(&keys[0], key_length, 'c', ptr_length, pairs, key_count);
  assert(index == 1);
  assert(*get_ptr_address(pairs, key_length, ptr_length, index) == 6);

  index = find_ptr_index(&keys[1], key_length, 'c', ptr_length, pairs, key_count);
  assert(index == 0);
  assert(*get_ptr_address(pairs, key_length, ptr_length, index) == 5);

  index = find_ptr_index(&keys[2], key_length, 'c', ptr_length, pairs, key_count);
  assert(index == 2);
  assert(*get_ptr_address(pairs, key_length, ptr_length, index) == 7);

  index = find_ptr_index(&keys[3], key_length, 'c', ptr_length, pairs, key_count);
  assert(index == 3);
  assert(*get_ptr_address(pairs, key_length, ptr_length, index) == 8);
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

  assert(AM_OpenIndexScan(i, EQ_OP, (char*)&key) == i);

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

int main()
{
  test_filename_size();
  test_filename_with_index();
  test_max_node_count();
  test_operation();
  test_find_ptr_index();

  /* AM functions */
  AM_Init();
  test_create_destroy_index();
  test_open_close_index();
  test_open_close_scan();

  printf("Passed all tests\n");
  return 0;
}
