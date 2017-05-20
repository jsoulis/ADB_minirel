#include "string.h"
#include "stdlib.h"

#include "minirel.h"

#include "am.h"
#include "data_structures.h"
#include "hf.h"
#include "utils.h"
#include "pf.h"

index_table_entry index_table[AM_ITAB_SIZE];

void AM_Init() {
  int i;
  HF_Init();

  for (i = 0; i < AM_ITAB_SIZE; ++i) {
    index_table[i].in_use = FALSE;
  }
}

int AM_CreateIndex(char *filename, int index_no, char attr_type, int attr_length,
                   bool_t is_unique) {
  internal_node *root;
  int fd, pagenum;
  char *filename_with_index;

  if (!(attr_type == 'c' || attr_type == 'i' || attr_type == 'f') || (attr_length < 1 || attr_length > 255)) {
    AMerrno = AME_INVALIDATTRTYPE;
    return AMerrno;
  }

  /* add 1 to size to fit NULL character */
  filename_with_index = malloc(sizeof_filename_with_index(filename, index_no));
  set_filename_with_index(filename, index_no, filename_with_index);

  if (PF_CreateFile(filename_with_index) != PFE_OK) {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  fd = PF_OpenFile(filename_with_index);
  if (fd < 0) {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  if (PF_AllocPage(fd, &pagenum, ((char**)&root)) != PFE_OK) {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  root->valid_entries = 0;
  root->key_type = attr_type;
  /* max length is 255 */
  root->key_length = (uint8_t)attr_length;

  /* Unpin and mark dirty */
  if (PF_UnpinPage(fd, pagenum, TRUE) != PFE_OK) {
    free(filename_with_index);

    AMerrno = AME_PF;
    return AMerrno;
  }

  free(filename_with_index);
  /* stop complaining about unused variable */
  (void)is_unique;
  return AME_OK;
}

int AM_DestroyIndex(char *filename, int index_no) {
  int return_code = AME_OK;
  char *filename_with_index = malloc(sizeof_filename_with_index(filename, index_no));
  set_filename_with_index(filename, index_no, filename_with_index);
  /* PF won't destroy a pinned file. So if we have the root pinned we can just
   * check like this */
  if (PF_DestroyFile(filename_with_index) != PFE_OK) {
    AMerrno = AME_PF;
    return_code = AMerrno;
  }

  free(filename_with_index);
  return return_code;
}