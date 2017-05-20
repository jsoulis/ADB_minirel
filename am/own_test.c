#include "assert.h"
#include "utils.h"
#include "stdio.h"

void test_filename_size() {
  char *filename = "sweet"; /* strlen("sweet") = 5 */

  assert(sizeofFilenameWithIndex(filename, 1) == 7);
  assert(sizeofFilenameWithIndex(filename, 15) == 8);
}

void test_filename_with_index() {
  char *filename = "test"; /* len 4 */
  char updatedName1[7];
  char updatedName2[9];

  setFilenameWithIndex(filename, 1, updatedName1);
  setFilenameWithIndex(filename, 537, updatedName2);

  assert(strcmp("test.1", updatedName1) == 0);
  assert(strcmp("test.537", updatedName2) == 0);
}

int main() {
  test_filename_size();
  test_filename_with_index();

  printf("Passed all tests\n");
  return 0;
}