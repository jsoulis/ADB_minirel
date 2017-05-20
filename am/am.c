#include "string.h"
#include "stdlib.h"

#include "minirel.h"

#include "am.h"
#include "data_structures.h"
#include "hf.h"
#include "utils.h"
#include "pf.h"

index_table_entry index_table[AM_ITAB_SIZE];

void AM_Init() { HF_Init(); }

int AM_CreateIndex(char *filename, int indexNo, char attrType, int attrLength,
                   bool_t isUnique) {
  internal_node *root;
  int fd, pagenum;
  char *filenameWithIndex;

  if (!(attrType == 'c' || attrType == 'i' || attrType == 'f') || (attrLength < 1 || attrLength > 255)) {
    AMerrno = AME_INVALIDATTRTYPE;
    return AMerrno;
  }

  /* add 1 to size to fit NULL character */
  filenameWithIndex = malloc(sizeofFilenameWithIndex(filename, indexNo));
  setFilenameWithIndex(filename, indexNo, filenameWithIndex);

  if (PF_CreateFile(filenameWithIndex) != PFE_OK) {
    free(filenameWithIndex);

    AMerrno = AME_PF;
    return AMerrno;
  }

  fd = PF_OpenFile(filenameWithIndex);
  if (fd < 0) {
    free(filenameWithIndex);

    AMerrno = AME_PF;
    return AMerrno;
  }

  if (PF_AllocPage(fd, &pagenum, ((char**)&root)) != PFE_OK) {
    free(filenameWithIndex);

    AMerrno = AME_PF;
    return AMerrno;
  }

  root->validEntries = 0;
  root->keyType = attrType;
  /* max length is 255 */
  root->keyLength = (uint8_t)attrLength;

  /* Unpin and mark dirty */
  if (PF_UnpinPage(fd, pagenum, TRUE) != PFE_OK) {
    free(filenameWithIndex);

    AMerrno = AME_PF;
    return AMerrno;
  }

  free(filenameWithIndex);
  /* stop complaining about unused variable */
  (void)isUnique;
  return AME_OK;
}

int AM_DestroyIndex(char *filename, int indexNo) {
  int returnCode = AME_OK;
  char *filenameWithIndex = malloc(sizeofFilenameWithIndex(filename, indexNo));
  setFilenameWithIndex(filename, indexNo, filenameWithIndex);
  /* PF won't destroy a pinned file. So if we have the root pinned we can just
   * check like this */
  if (PF_DestroyFile(filenameWithIndex) != PFE_OK) {
    AMerrno = AME_PF;
    returnCode = AMerrno;
  }

  free(filenameWithIndex);
  return returnCode;
}