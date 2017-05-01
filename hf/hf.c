#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "minirel.h"

#include "bf.h"
#include "pf.h"
#include "hf.h"

typedef struct HFhdr_str {
  int numrecs;
  int numpages;
  int recsize;
  int firstPage;
  int lastPage;
} HFhdr_str;

typedef struct HFftab_ele {
  bool_t valid;
  int PFfd;
  char *filename;
  HFhdr_str hdr;
  bool_t hdrchanged;
  bool_t scanActive; /*TRUE if scan is active*/
}

HFftab_ele file_table[HF_FTAB_SIZE];

void HF_Init(void) {
  //initialize other data strctures needed for this layer

  int i;

  PF_Init();

  for (i = 0; i < HF_FTAB_SIZE; ++i) {
    file_table[i].valid = FALSE;
  }

}

int HF_CreateFile(char *filename, int recSize) {
  HFhdr_str *hdr;
  int fd;
  int *pagenum;
  char **pagebuf;

  if(recSize >= PAGE_SIZE) {
    return HFE_RECSIZE;
  }

  if(PF_CreateFile(filename)!=0) {
    return HFE_PF;
  }

  if(fd = PF_OpenFile(filename) < 0) {
    return HFE_PF;
  }

  if (PF_AllocPage(fd, pagenum, pagebuf) != 0)
  {
    return HFE_PF;
  }

  hdr = (HFhdr_str *) pagebuf;

  hdr->numrecs = 0;
  hdr->numpages = 0;
  hdr->recSize = recSize;
  hdr->firstPage = -1;
  hdr->lastPage = -1;

  if (PF_CloseFile(fd) != 0)
  {
    return HFE_PF;
  }

  return HFE_OK;
}

int HF_DestroyFile(char *filename) {
  HFftab_ele *file;
  int i;

  /* Not sure if we need this or not
  if (!file_exists(filename)) {
    return HFE_FILE;
  }
  */

  /*check to see if file is open */
  for (i = 0; i < HF_FTAB_SIZE; ++i) {
    file = &file_table[i];
    if (file->valid && (strcmp(file->filename, filename)==0)) {
      return HFE_FILEOPEN;
    }
  }

  if (PF_DestroyFile(filename) != 0) {
    return HFE_PF;
  }

  return HFE_OK;
}

int HF_OpenFile(char *filename) {
 //header data should be copied to the file table.
 int HFfd, i, fd;
 HFftab_ele *file;
 int *pagenum;
 char **pagebuf;
 HFhdr_str *hdr;

 file = NULL;
 for (i = 0; i < HF_FTAB_SIZE; ++i) {
   if (!file_table[i].valid) {
     file = &file_table[i];
     HFfd = i;
     break;
   }
 }
 if (file == NULL) {
   return HFE_FTABFULL;
 }

 if (fd = PF_OpenFile(filename) != 0) {
   return HFE_PF;
 }

 file->valid = TRUE;
 file->filename = malloc(strlen(filename));
 strcpy(file->filename, filename);

 file->PFfd = fd;

 /*get header page for hf file */
 if (PF_GetFirstPage(fd, pagenum, pagebuf) !=0) {
   return HFE_PF;
 }

 hdr = (HFhdr_str *) pagebuf;

 file->hdr.numrecs = hdr->numrecs;
 file->hdr.numpages = hdr->numpages;
 file->hdr.recsize = hdr->recsize;
 file->hdr.firstPage = hdr->firstPage;
 file->hdr.lastPage = hdr->lastPage;

 file->hdrchanged = TRUE;
 file->scanActive = FALSE;

 return HFfd;

}

int HF_CloseFile(int HFfd) {
  HFhdr_str *hdr;
  HFftab_ele *file;
  int *pagenum;
  char **pagebuf;

  /* ensure that the HFfd is valid and open*/
  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    return HFE_FD;
  }

  if (!file_table[HFfd].valid) {
    return HFE_FILENOTOPEN;
  }

  /*write back header of file if it changed*/
  file = &file_table[HFfd];
  if(file->hdrchanged) {
    if(PF_GetFirstPage(file->PFfd, pagenum, pagebuf)!=0)
    {
      return HFE_PF;
    }
    hdr = (HFhdr_str *) pagebuf;
    file->hdr.numrecs = hdr->numrecs;
    file->hdr.numpages = hdr->numpages;
    file->hdr.recsize = hdr->recsize;
    file->hdr.firstPage = hdr->firstPage;
    file->hdr.lastPage = hdr->lastPage;
  }

  if (PF_CloseFile(file->PFfd) != 0) {
    return HFE_PF;
  }

  free(file_table[HFfd].filename);
  file_table[HFfd].valid = FALSE;

  return HFE_OK;

}

RECID HF_InsertRec(int HFfd, char *record) {
  

}

int HF_DeleteRec(int HFfd, RECID recid) {

}

RECID HF_GetFirstRec(int HFfd, char *record) {

}

RECID HF_GetNextRec(int HFfd, RECID recID, char *record) {

}

int HF_GetThisRec(int HFfd, RECID recID, char *record) {

}

int HF_OpenFileScan(int fileDesc, char attrType, int attrLength, int attrOffset, int op, char *value) {

}

RECID HF_FindNextRec(int scanDesc, char *record) {

}

int HF_CloseFileScan(int scanDesc) {

}

bool_t HF_ValidRecId(int HFfd, RECID recid) {

}
