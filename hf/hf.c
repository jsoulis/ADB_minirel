#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "minirel.h"

#include "bf.h"
#include "pf.h"
#include "hf.h"

typedef struct HFhdr_str {
  int numrecs;
  int numpages;
  int maxNumRecords;
  int recsize;
} HFhdr_str;

typedef struct HFftab_ele {
  bool_t valid;
  int PFfd;
  char *filename;
  HFhdr_str hdr;
  bool_t hdrchanged;
  bool_t scanActive; /*TRUE if scan is active*/
} HFftab_ele;

typedef struct HFScanTab_ele {
  bool_t valid;
  int HFfd;
  RECID lastScannedRec;
} HFScanTab_ele;

/*table keeps track of OPEN HF files */
HFftab_ele file_table[HF_FTAB_SIZE];
/*table keeps track of OPEN scans*/
HFScanTab_ele scan_table[MAXSCANS];

void HF_Init(void) {
  /*initialize other data strctures needed for this layer*/

  int i;

  PF_Init();

  for (i = 0; i < HF_FTAB_SIZE; ++i) {
    file_table[i].valid = FALSE;
  }

}

int HF_CreateFile(char *filename, int recSize) {
  HFhdr_str *hdr;
  int fd, maxNumRecords;
  int pagenum;
  char *pagebuf;

  if(recSize >= PAGE_SIZE) {
    return HFE_RECSIZE;
  }

  if(PF_CreateFile(filename)!=0) {
    return HFE_PF;
  }

  if(fd = PF_OpenFile(filename) < 0) {
    return HFE_PF;
  }

  if (PF_AllocPage(fd, &pagenum, &pagebuf) != 0)
  {
    return HFE_PF;
  }

  /*calculate the max number of records for data page*/
  /*may need to be recSize + 2 in order to account for '/0'*/
  maxNumRecords = floor(PAGE_SIZE/(recSize + 1));

  hdr = (HFhdr_str *) pagebuf;

  hdr->numrecs = 0;
  hdr->numpages = 0;
  hdr->recsize = recSize;
  hdr->maxNumRecords = maxNumRecords;

  if (PF_CloseFile(fd) != 0)
  {
    return HFE_PF;
  }

  return HFE_OK;
}

int HF_DestroyFile(char *filename) {
  HFftab_ele *file;
  int i;

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
 /*header data should be copied to the file table.*/
 int HFfd, i, fd;
 HFftab_ele *file;
 int pagenum;
 char *pagebuf;
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
 if (PF_GetFirstPage(fd, &pagenum, &pagebuf) !=0) {
   return HFE_PF;
 }

 /*we now are pointing to header info we already wrote
 when we created the file */
 hdr = (HFhdr_str *) pagebuf;

 file->hdr.numrecs = hdr->numrecs;
 file->hdr.numpages = hdr->numpages;
 file->hdr.recsize = hdr->recsize;

 file->hdrchanged = FALSE;
 file->scanActive = FALSE;

 return HFfd;

}

int HF_CloseFile(int HFfd) {
  HFhdr_str *hdr;
  HFftab_ele *file;
  int pagenum;
  char *pagebuf;

  /* ensure that the HFfd is valid and open*/
  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    return HFE_FD;
  }

  if (!file_table[HFfd].valid) {
    return HFE_FILENOTOPEN;
  }

  if(file_table[HFfd].scanActive) {
    return HFE_SCANOPEN;
  }

  /*write back header of file if it was changed in table*/
  file = &file_table[HFfd];
  if(file->hdrchanged) {
    if(PF_GetFirstPage(file->PFfd, &pagenum, &pagebuf)!=0)
    {
      return HFE_PF;
    }
    hdr = (HFhdr_str *) pagebuf;

    /*update file based on whats in open HF table entry*/
    hdr->numrecs = file->hdr.numrecs;
    hdr->numpages = file->hdr.numpages;
    hdr->recsize = file->hdr.recsize;

  }

  if (PF_CloseFile(file->PFfd) != 0) {
    return HFE_PF;
  }

  free(file_table[HFfd].filename);
  file_table[HFfd].valid = FALSE;

  return HFE_OK;

}

RECID HF_InsertRec(int HFfd, char *record) {
  HFftab_ele *file;
  HFhdr_str *hdr;
  RECID recid, invalid;
  char *pagebuf;
  int pagenum, i, j, HFerrno;
  bool_t* bitmapArray;
  char* recordArray;

  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    HFerrno = HFE_FD;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;
  }

  if (!file_table[HFfd].valid) {
    HFerrno = HFE_FILENOTOPEN;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;
  }

  file = &file_table[HFfd];
  hdr = &file->hdr;

  /*case in which there are no data pages*/
  if(hdr->numpages == 0) {
    if (PF_AllocPage(file->PFfd, &pagenum, &pagebuf) != 0) {
      HFerrno = HFE_PF;
      invalid.recnum = -1;
      invalid.pagenum = -1;
      return invalid;
    }
    ++(hdr->numpages);
    bitmapArray = (bool_t *) pagebuf;
    recordArray = (char*)(bitmapArray) + sizeof(hdr->maxNumRecords);
    /*initialize bitmap to show empty page */
    for (i = 0; i < hdr->maxNumRecords; i++) {
      bitmapArray[i] = TRUE;
    }
    bitmapArray[0] = FALSE;
    strcpy(&recordArray[0], record);
    ++(hdr->numrecs);
    recid.recnum = 0;
    recid.pagenum = 0;
    file->hdrchanged = TRUE;
    return recid;
  }
  /*there are data pages and not all are full*/
  else {
    for (i = 0; i < hdr->numpages; i++) {
      if (PF_GetThisPage(file->PFfd, i, &pagebuf) != 0) {
        HFerrno = HFE_PF;
        invalid.recnum = -1;
        invalid.pagenum = -1;
        return invalid;
      }

      bitmapArray = (bool_t *) pagebuf;
      recordArray = (char*)(bitmapArray) + sizeof(hdr->maxNumRecords);
      for (j = 0; j < hdr->maxNumRecords; j++) {
        if(bitmapArray[j]) {
          recid.recnum = j;
          recid.pagenum = i;
          bitmapArray[j] = FALSE;
          strcpy(&recordArray[j], record);
          ++(hdr->numrecs);
          file->hdrchanged = TRUE;
          return recid;
        }
      }
    }
    /*There are data pages but are ALL full */
    if (PF_AllocPage(file->PFfd, &pagenum, &pagebuf) != 0) {
      HFerrno = HFE_PF;
      invalid.recnum = -1;
      invalid.pagenum = -1;
      return invalid;
    }
    ++(hdr->numpages);
    bitmapArray = (bool_t *) pagebuf;
    recordArray = (char*)(bitmapArray) + sizeof(hdr->maxNumRecords);
    /*initialize bitmap to show empty page */
    for (i = 0; i < hdr->maxNumRecords; i++) {
      bitmapArray[i] = TRUE;
    }
    bitmapArray[0] = FALSE;
    strcpy(&recordArray[0], record);
    ++(hdr->numrecs);
    recid.recnum = 0;
    recid.pagenum = (hdr->numpages) - 1;
    file->hdrchanged = TRUE;
    return recid;
  }
}

int HF_DeleteRec(int HFfd, RECID recid) {
  HFftab_ele *file;
  HFhdr_str *hdr;
  char *pagebuf;
  bool_t* bitmapArray;
  char* recordArray;

  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    return HFE_FD;
  }

  if (!file_table[HFfd].valid) {
    return HFE_FILENOTOPEN;
  }

  file = &file_table[HFfd];
  hdr = &file->hdr;

  if (recid.pagenum >= hdr->numpages || recid.recnum >= hdr->maxNumRecords ) {
    return HFE_INVALIDRECORD;
  }

  if (PF_GetThisPage(file->PFfd, recid.pagenum, &pagebuf) != 0) {
    return HFE_PF;
  }

  bitmapArray = (bool_t *) pagebuf;
  recordArray = (char*)(bitmapArray) + sizeof(hdr->maxNumRecords);

  if (bitmapArray[recid.recnum]) {
    return HFE_INVALIDRECORD;
  }
  else {
    bitmapArray[recid.recnum] = TRUE;
  }

  --(hdr->numrecs);
  file->hdrchanged = TRUE;

  return HFE_OK;

}

RECID HF_GetFirstRec(int HFfd, char *record) {
  HFftab_ele *file;
  HFhdr_str *hdr;
  bool_t* bitmapArray;
  char* recordArray;
  RECID recid, invalid;
  int i, j, HFerrno;
  char *pagebuf;

  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    HFerrno = HFE_FD;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;
  }

  if (!file_table[HFfd].valid) {
    HFerrno = HFE_FILENOTOPEN;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;
  }

  file = &file_table[HFfd];
  hdr = &file->hdr;

  if (hdr->numpages == 0)
  {
    HFerrno = HFE_EOF;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;
  }
/*get first page and then look for first valid record.*/
for (i = 0; i < hdr->numpages; i++) {
  if (PF_GetThisPage(file->PFfd, i, &pagebuf) != 0) {
    HFerrno = HFE_PF;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;
  }
  bitmapArray = (bool_t *) pagebuf;
  recordArray = (char*)(bitmapArray) + sizeof(hdr->maxNumRecords);
  for (j = 0; j < file->hdr.maxNumRecords; j++) {
    if(!bitmapArray[j]) {
      recid.recnum = j;
      recid.pagenum = i;
      return recid;
    }
  }
}
HFerrno = HFE_EOF;
invalid.recnum = -1;
invalid.pagenum = -1;
return invalid;

}

RECID HF_GetNextRec(int HFfd, RECID recid, char *record) {
  HFftab_ele *file;
  HFhdr_str *hdr;
  char *pagebuf;
  RECID nextRec, invalid;
  bool_t* bitmapArray;
  char* recordArray;
  int HFerrno;

  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    HFerrno = HFE_FD;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;
  }

  if (!file_table[HFfd].valid) {
    HFerrno = HFE_FILENOTOPEN;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;
  }

  file = &file_table[HFfd];
  hdr = &file->hdr;

  if (hdr->numpages == 0)
  {
    HFerrno = HFE_EOF;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;
  }

  if (recid.pagenum >= hdr->numpages || recid.recnum >= hdr->maxNumRecords ) {
    HFerrno = HFE_INVALIDRECORD;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;
  }

  if (recid.recnum == hdr->maxNumRecords - 1) {
    nextRec.recnum = 0;
    nextRec.pagenum = recid.pagenum + 1;
  }
  else {
    nextRec.recnum = recid.recnum + 1;
    nextRec.pagenum = recid.pagenum;
  }

  if (nextRec.pagenum >= hdr->numpages) {
    HFerrno = HFE_INVALIDRECORD;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;
  }

  if (PF_GetThisPage(file->PFfd, nextRec.pagenum, &pagebuf) != 0) {
    HFerrno = HFE_PF;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;
  }

  bitmapArray = (bool_t *) pagebuf;
  recordArray = (char*)(bitmapArray) + sizeof(hdr->maxNumRecords);

  if (bitmapArray[nextRec.recnum]) {
    HFerrno = HFE_INVALIDRECORD;
    invalid.recnum = -1;
    invalid.pagenum = -1;
    return invalid;;
  }
  else {
    record = &recordArray[nextRec.recnum];
  }

  return nextRec;


}

int HF_GetThisRec(int HFfd, RECID recid, char *record) {
  HFftab_ele *file;
  HFhdr_str *hdr;
  char *pagebuf;
  bool_t* bitmapArray;
  char* recordArray;

  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    return HFE_FD;
  }

  if (!file_table[HFfd].valid) {
    return HFE_FILENOTOPEN;
  }

  file = &file_table[HFfd];
  hdr = &file->hdr;

  if (hdr->numpages == 0)
  {
    return HFE_EOF;
  }

  if (recid.pagenum >= hdr->numpages || recid.recnum >= hdr->maxNumRecords ) {
    return HFE_INVALIDRECORD;
  }

  if (PF_GetThisPage(file->PFfd, recid.pagenum, &pagebuf) != 0) {
    return HFE_PF;
  }

  bitmapArray = (bool_t *) pagebuf;
  recordArray = (char*)(bitmapArray) + sizeof(hdr->maxNumRecords);

  if (bitmapArray[recid.recnum]) {
    return HFE_INVALIDRECORD;
  }
  else {
    record = &recordArray[recid.recnum];
  }

  return HFE_OK;

}

int HF_OpenFileScan(int fileDesc, char attrType, int attrLength, int attrOffset, int op, char *value) {

}

RECID HF_FindNextRec(int scanDesc, char *record) {

}

int HF_CloseFileScan(int scanDesc) {

}

bool_t HF_ValidRecId(int HFfd, RECID recid) {
  HFftab_ele *file;
  int HFerrno;

  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    HFerrno = HFE_FD;
    return FALSE;
  }

  if (!file_table[HFfd].valid) {
    HFerrno = HFE_FILENOTOPEN;
    return FALSE;
  }

  file = &file_table[HFfd];
  /* The recid presented should have a pagenum and recnum
  smaller than what is indicated in header for the HF file*/
  if (file->hdr.numpages > recid.pagenum) {
    if (file->hdr.maxNumRecords > recid.recnum) {
      return TRUE;
    }
  }
  else {
    return FALSE;
  }
}
