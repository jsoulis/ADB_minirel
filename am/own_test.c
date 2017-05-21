#include "assert.h"
#include "stdint.h"
#include "stdio.h"
#include "utils.h"
#include "am.h"

int AMerrno;

void test_filename_size()
{
  char *filename = "sweet"; /* strlen("sweet") = 5 */

  /* It includes null character */
  assert(sizeof_filename_with_index(filename, 1) == 8);
  assert(sizeof_filename_with_index(filename, 15) == 9);
  assert(sizeof_filename_with_index(filename, 100) == 10);
}

void test_filename_with_index()
{
  char *filename = "test"; /* len 4 */
  char updated_name_1[7];
  char updated_name_2[9];

  set_filename_with_index(filename, 1, updated_name_1);
  set_filename_with_index(filename, 537, updated_name_2);

  assert(strcmp("test.1", updated_name_1) == 0);
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

void test_create_index()
{
  assert(AM_CreateIndex("fail1", 1, 'a', 1, FALSE) == AME_INVALIDATTRTYPE);
  assert(AM_CreateIndex("fail2", 1, 'c', 256, FALSE) == AME_INVALIDATTRLENGTH);
}

int main()
{
  test_filename_size();
  test_filename_with_index();
  test_max_node_count();

  /* AM functions */
  AM_Init();
  test_create_index();

  printf("Passed all tests\n");
  return 0;
}