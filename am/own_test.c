#include "assert.h"
#include "utils.h"
#include "stdio.h"

void test_filename_size() {
  char *filename = "sweet"; /* strlen("sweet") = 5 */

  /* It includes null character */
  assert(sizeof_filename_with_index(filename, 1) == 8);
  assert(sizeof_filename_with_index(filename, 15) == 9);
  assert(sizeof_filename_with_index(filename, 100) == 10);
}

void test_filename_with_index() {
  char *filename = "test"; /* len 4 */
  char updated_name_1[7];
  char updated_name_2[9];

  set_filename_with_index(filename, 1, updated_name_1);
  set_filename_with_index(filename, 537, updated_name_2);

  assert(strcmp("test.1", updated_name_1) == 0);
  assert(strcmp("test.537", updated_name_2) == 0);
}

int main() {
  test_filename_size();
  test_filename_with_index();

  printf("Passed all tests\n");
  return 0;
}