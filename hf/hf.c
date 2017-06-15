#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>

#include "minirel.h"

#include "bf.h"
#include "pf.h"
#include "hf.h"


#define OP_EQ   1
#define OP_LT   2
#define OP_GT   3
#define OP_LTEQ 4
#define OP_GTEQ 5
#define OP_NEQ  6

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
  int pageMap[100];
} HFftab_ele;

typedef struct HFScanTab_ele {
  bool_t valid;
  int HFfd;
  int offset;
  int op;
  char type;
  char *value;
  RECID lastScannedRec;
} HFScanTab_ele;

/*table keeps track of OPEN HF files */
HFftab_ele HFfile_table[HF_FTAB_SIZE];
/*table keeps track of OPEN scans*/
HFScanTab_ele scan_table[MAXSCANS];

void HF_Init(void) {
  /*initialize other data strctures needed for this layer*/

  int i;

  PF_Init();

  for (i = 0; i < HF_FTAB_SIZE; ++i) {
    HFfile_table[i].valid = FALSE;
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

  fd = PF_OpenFile(filename);
  if(fd < 0) {
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

  if (PF_UnpinPage(fd, pagenum, TRUE) != PFE_OK) {
    return HFE_PF;
  }

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
    file = &HFfile_table[i];
    if (file->valid && (strcmp(file->filename, filename)==0)) {
      return HFE_FILEOPEN;
    }
    if (!file->valid && (strcmp(file->filename, filename)==0)) {
      break;
    }
  }

  if (PF_DestroyFile(filename) != 0) {
    return HFE_PF;
  }

  return HFE_OK;
}

int HF_OpenFile(char *filename) {
 /*header data should be copied to the file table.*/
 int HFfd, i, fd, k;
 HFftab_ele *file;
 int pagenum;
 char *pagebuf;
 HFhdr_str *hdr;

 file = NULL;
 for (i = 0; i < HF_FTAB_SIZE; ++i) {
   if (!HFfile_table[i].valid) {
     file = &HFfile_table[i];
     HFfd = i;
     break;
   }
 }
 if (file == NULL) {
   return HFE_FTABFULL;
 }

 if ((fd = PF_OpenFile(filename)) < 0) {
   return HFE_PF;
 }

 file->valid = TRUE;
 file->filename = malloc(strlen(filename));
 strcpy(file->filename, filename);

 file->PFfd = fd;

 /*TO DO - Is GetFirstPage really returning the page the header
   is written on..? */
 /*update - I verified by stepping through code that GetFirstpage
  does return the same page as allocated in HF_CreateFile */
 /*get header page for hf file */
 if (PF_GetFirstPage(fd, &pagenum, &pagebuf) !=0) {
   return HFE_PF;
 }

 /*we now are pointing to header info we already wrote
 when we created the file */
 hdr = (HFhdr_str *) pagebuf;

 /*initialize pageMap to bad dummy values*/

 for (k = hdr->numpages; k < 100; k++) {
 file->pageMap[k] = -1;
}

 file->hdr.numrecs = hdr->numrecs;
 file->hdr.numpages = hdr->numpages;
 file->hdr.recsize = hdr->recsize;
 file->hdr.maxNumRecords = hdr->maxNumRecords;

 file->hdrchanged = FALSE;
 file->scanActive = FALSE;

 if (PF_UnpinPage(fd, pagenum, FALSE) != PFE_OK) {
   return HFE_PF;
 }

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

  if (!HFfile_table[HFfd].valid) {
    return HFE_FILENOTOPEN;
  }

  if(HFfile_table[HFfd].scanActive) {
    return HFE_SCANOPEN;
  }

  /*write back header of file if it was changed in table*/
  file = &HFfile_table[HFfd];
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

    if (PF_UnpinPage(file->PFfd, pagenum, TRUE) != PFE_OK) {
      return HFE_PF;
    }



  }

  if (PF_CloseFile(file->PFfd) != 0) {
    return HFE_PF;
  }

  free(HFfile_table[HFfd].filename);
  HFfile_table[HFfd].valid = FALSE;

  return HFE_OK;

}

RECID HF_InsertRec(int HFfd, char *record) {
  HFftab_ele *file;
  HFhdr_str *hdr;
  RECID recid, invalid;
  char *pagebuf;
  int pagenum, a, i, j, k, HFerrno;
  bool_t* bitmapArray;
  char** recordArray;
  invalid.recnum = -1;
  invalid.pagenum = -1;

  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    HFerrno = HFE_FD;
    return invalid;
  }

  if (!HFfile_table[HFfd].valid) {
    HFerrno = HFE_FILENOTOPEN;
    return invalid;
  }

  file = &HFfile_table[HFfd];
  hdr = &file->hdr;

  /*case in which there are no data pages*/
  if(hdr->numpages == 0) {
    if (PF_AllocPage(file->PFfd, &pagenum, &pagebuf) != 0) {
      HFerrno = HFE_PF;
      return invalid;
    }
    ++(hdr->numpages);
    file->pageMap[0] = pagenum;
    bitmapArray = (bool_t *)pagebuf;
    recordArray = (char **)(bitmapArray + hdr->maxNumRecords);
    /*initialize bitmap to show empty page */
    for (i = 0; i < hdr->maxNumRecords; i++) {
      bitmapArray[i] = TRUE;
    }
    bitmapArray[0] = FALSE;
    recordArray[0] = malloc((hdr->recsize + 1) * sizeof(char));
    strcpy(recordArray[0], record);
    ++(hdr->numrecs);
    recid.recnum = 0;
    recid.pagenum = 0;
    file->hdrchanged = TRUE;
    if (PF_UnpinPage(file->PFfd, pagenum, TRUE) != PFE_OK) {
      return invalid;
    }
    return recid;
  }
  /*there are data pages and not all are full*/
  else {
    for (i = 0; i < hdr->numpages; i++) {
      if (PF_GetThisPage(file->PFfd, file->pageMap[i], &pagebuf) != 0) {
        HFerrno = HFE_PF;
        return invalid;
      }
      bitmapArray = (bool_t *)pagebuf;
      recordArray = (char **)(bitmapArray + hdr->maxNumRecords);
      for (j = 0; j < hdr->maxNumRecords; j++) {
        if(bitmapArray[j]) {
          recid.recnum = j;
          recid.pagenum = i;
          bitmapArray[j] = FALSE;
          recordArray[j] = malloc((hdr->recsize + 1) * sizeof(char));
          strcpy(recordArray[j], record);
          ++(hdr->numrecs);
          file->hdrchanged = TRUE;
          if (PF_UnpinPage(file->PFfd, file->pageMap[i], TRUE) != PFE_OK) {
            return invalid;
          }
          return recid;
        }
      }
      if (PF_UnpinPage(file->PFfd, file->pageMap[i], TRUE) != PFE_OK) {
        return invalid;
      }
    }
    /*There are data pages but are ALL full */
    if (PF_AllocPage(file->PFfd, &pagenum, &pagebuf) != 0) {
      HFerrno = HFE_PF;
      return invalid;
    }
    ++(hdr->numpages);
    for (k = 0; k < 100; k++) {
	     if (file->pageMap[k] == -1) {
	        file->pageMap[k] = pagenum;
          break;
	       }
    }
    bitmapArray = (bool_t *)pagebuf;
    recordArray = (char **)(bitmapArray + hdr->maxNumRecords);
    /*initialize bitmap to show empty page */
    for (i = 0; i < hdr->maxNumRecords; i++) {
      bitmapArray[i] = TRUE;
    }
    bitmapArray[0] = FALSE;
    recordArray[0] = malloc((hdr->recsize + 1) * sizeof(char));
    strcpy(recordArray[0], record);
    ++(hdr->numrecs);
    recid.recnum = 0;
    recid.pagenum = k;
    file->hdrchanged = TRUE;
    if (PF_UnpinPage(file->PFfd, pagenum, TRUE) != PFE_OK) {
      return invalid;
    }
    return recid;
  }
}
int HF_DeleteRec(int HFfd, RECID recid) {
  HFftab_ele *file;
  HFhdr_str *hdr;
  char *pagebuf;
  bool_t* bitmapArray;
  char** recordArray;

  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    return HFE_FD;
  }

  if (!HFfile_table[HFfd].valid) {
    return HFE_FILENOTOPEN;
  }

  file = &HFfile_table[HFfd];
  hdr = &file->hdr;

  if (recid.pagenum >= hdr->numpages || recid.recnum >= hdr->maxNumRecords ) {
    return HFE_INVALIDRECORD;
  }

  if (PF_GetThisPage(file->PFfd, file->pageMap[recid.pagenum], &pagebuf) != 0) {
    return HFE_PF;
  }

  bitmapArray = (bool_t *)pagebuf;
  recordArray = (char **)(bitmapArray + hdr->maxNumRecords);

  if (bitmapArray[recid.recnum]) {
    return HFE_INVALIDRECORD;
  }
  else {
    bitmapArray[recid.recnum] = TRUE;
  }

  --(hdr->numrecs);
  file->hdrchanged = TRUE;
  if (PF_UnpinPage(file->PFfd, file->pageMap[recid.pagenum], TRUE) != PFE_OK) {
    return HFE_PF;
  }

  return HFE_OK;

}

RECID HF_GetFirstRec(int HFfd, char *record) {
  HFftab_ele *file;
  HFhdr_str *hdr;
  bool_t* bitmapArray;
  char** recordArray;
  RECID recid, invalid;
  int i, j, HFerrno;
  char *pagebuf;
  invalid.recnum = -1;
  invalid.pagenum = -1;

  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    HFerrno = HFE_FD;
    return invalid;
  }

  if (!HFfile_table[HFfd].valid) {
    HFerrno = HFE_FILENOTOPEN;
    return invalid;
  }

  file = &HFfile_table[HFfd];
  hdr = &file->hdr;

  if (hdr->numpages == 0)
  {
    HFerrno = HFE_EOF;
    return invalid;
  }
/*get first page and then look for first valid record.*/
for (i = 0; i < hdr->numpages; i++) {
  if (PF_GetThisPage(file->PFfd, file->pageMap[i], &pagebuf) != 0) {
    HFerrno = HFE_PF;
    return invalid;
  }
  bitmapArray = (bool_t *)pagebuf;
  recordArray = (char **)(bitmapArray + hdr->maxNumRecords);
  for (j = 0; j < file->hdr.maxNumRecords; j++) {
    if(!bitmapArray[j]) {
      recid.recnum = j;
      recid.pagenum = i;
      strcpy(record, recordArray[j]);
      if (PF_UnpinPage(file->PFfd, file->pageMap[i], FALSE) != PFE_OK) {
        HFerrno = HFE_PF;
        return invalid;
      }
      return recid;
    }
    if (PF_UnpinPage(file->PFfd, file->pageMap[i], FALSE) != PFE_OK) {
      HFerrno = HFE_PF;
      return invalid;
    }
  }
  if (PF_UnpinPage(file->PFfd, file->pageMap[i], FALSE) != PFE_OK) {
    HFerrno = HFE_PF;
    return invalid;
  }
}
HFerrno = HFE_EOF;
return invalid;

}

RECID HF_GetNextRec(int HFfd, RECID recid, char *record) {
  HFftab_ele *file;
  HFhdr_str *hdr;
  char *pagebuf;
  RECID nextRec, invalid;
  bool_t* bitmapArray;
  char** recordArray;
  int HFerrno;
  invalid.recnum = -1;
  invalid.pagenum = -1;

  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    HFerrno = HFE_FD;
    return invalid;
  }

  if (!HFfile_table[HFfd].valid) {
    HFerrno = HFE_FILENOTOPEN;
    return invalid;
  }

  file = &HFfile_table[HFfd];
  hdr = &file->hdr;

  if (hdr->numpages == 0) {
    HFerrno = HFE_EOF;
    return invalid;
  }

  if (recid.pagenum >= hdr->numpages || recid.recnum >= hdr->maxNumRecords ) {
    HFerrno = HFE_INVALIDRECORD;
    return invalid;
  }

  if (recid.recnum == hdr->maxNumRecords - 1) {
    nextRec.recnum = 0;
    nextRec.pagenum = recid.pagenum + 1;
  }
  else {
    nextRec.recnum = recid.recnum+1;
    nextRec.pagenum = recid.pagenum;
  }
  if(file->pageMap[nextRec.pagenum] == -1)
  {
    HFerrno = HFE_EOF;
    return invalid;
  }
  if (PF_GetThisPage(file->PFfd, file->pageMap[nextRec.pagenum], &pagebuf) != 0) {
    HFerrno = HFE_PF;
    return invalid;
  }

  bitmapArray = (bool_t *)pagebuf;
  recordArray = (char **)(bitmapArray + hdr->maxNumRecords);

  while (bitmapArray[nextRec.recnum]) {
    if(nextRec.recnum == hdr->maxNumRecords - 1) {
      if (PF_UnpinPage(file->PFfd, file->pageMap[nextRec.pagenum], FALSE) != PFE_OK) {
        HFerrno = HFE_PF;
        return invalid;
      }
      nextRec.recnum = -1;
      nextRec.pagenum++;
      if(file->pageMap[nextRec.pagenum] == -1)
      {
        HFerrno = HFE_EOF;
        return invalid;
      }
      if (PF_GetThisPage(file->PFfd, file->pageMap[nextRec.pagenum], &pagebuf) != 0) {
        HFerrno = HFE_PF;
        return invalid;
      }
    }
    nextRec.recnum++;
  }

  strcpy(record, recordArray[nextRec.recnum]);

  if (PF_UnpinPage(file->PFfd, file->pageMap[nextRec.pagenum], FALSE) != PFE_OK) {
    HFerrno = HFE_PF;
    return invalid;
  }

  return nextRec;

}

int HF_GetThisRec(int HFfd, RECID recid, char *record) {
  HFftab_ele *file;
  HFhdr_str *hdr;
  char *pagebuf;
  bool_t* bitmapArray;
  char** recordArray;

  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    return HFE_FD;
  }

  if (!HFfile_table[HFfd].valid) {
    return HFE_FILENOTOPEN;
  }

  file = &HFfile_table[HFfd];
  hdr = &file->hdr;

  if (hdr->numpages == 0)
  {
    return HFE_EOF;
  }

  if (recid.pagenum >= hdr->numpages || recid.recnum >= hdr->maxNumRecords ) {
    return HFE_INVALIDRECORD;
  }

  if (PF_GetThisPage(file->PFfd, file->pageMap[recid.pagenum], &pagebuf) != 0) {
    return HFE_PF;
  }

  bitmapArray = (bool_t *)pagebuf;
  recordArray = (char **)(bitmapArray + hdr->maxNumRecords);

  if (bitmapArray[recid.recnum]) {
    return HFE_INVALIDRECORD;
  }
  else {
    strcpy(record, recordArray[recid.recnum]);
  }

  if (PF_UnpinPage(file->PFfd, file->pageMap[recid.pagenum], FALSE) != PFE_OK) {
    return HFE_PF;
  }

  return HFE_OK;

}

int HF_OpenFileScan(int fileDesc, char attrType, int attrLength, int attrOffset, int op, char *value) {
HFScanTab_ele *file;
HFftab_ele *f;
int i, j, sd;
RECID fillrec;
fillrec.recnum = 0;
fillrec.pagenum = 0;

file = NULL;
for (i = 0; i < MAXSCANS; ++i) {
  if (!scan_table[i].valid) {
    file = &scan_table[i];
    sd = i;
    break;
  }
}
if (file == NULL) {
  return HFE_STABFULL;
}

if (fileDesc < 0 || fileDesc >= HF_FTAB_SIZE) {
  return HFE_FD;
}

if(!HFfile_table[fileDesc].valid) {
  return HFE_FILENOTOPEN;
}
f = &HFfile_table[fileDesc];
f->scanActive = TRUE;

if(op < 1 || op > 6) {
  HFE_OPERATOR;
}

if (attrOffset < 0) {
  return HFE_ATTROFFSET;
}

file->HFfd = fileDesc;
file->op = op;
file->offset = attrOffset;
file->lastScannedRec.recnum = fillrec.recnum;
file->lastScannedRec.pagenum = fillrec.pagenum;
file->valid = TRUE;
file->value = malloc(strlen(value));
strcpy(file->value, value);

switch(attrType) {
  case 'c':
  case 'C':
    if (attrLength < 1 || attrLength > 255) {
      return HFE_ATTRLENGTH;
    }
    file->type = 'c';
    break;
  case 'i':
  case 'I':
    if (attrLength != 4) {
      return HFE_ATTRLENGTH;
    }
    file->type = 'i';
    break;
  case 'f':
  case 'F':
    if (attrLength != 4) {
      return HFE_ATTRLENGTH;
    }
    file->type = 'f';
    break;
  default:
    return HFE_ATTRTYPE;

}
}

int HF_GetRecordValue(HFScanTab_ele *file, float *val1, float *val2, char *record) {
  if(file->type == 'c') {
    *val1 = *(char *)(record + file->offset);
    *val2 = *(char *)(file->value);
  }
  if(file->type == 'i') {
    *val1 = *(int *)(record + file->offset);
    *val2 = *(int *)(file->value);
  }
  if(file->type == 'f') {
    *val1 = *(float *)(record + file->offset);
    *val2 = *(float *)(file->value);
  }
  return HFE_OK;
}

RECID HF_FindNextRec(int scanDesc, char *record) {
  HFScanTab_ele *file;
  HFftab_ele *f;
  RECID recid, invalid;
  int HFerrno, i;
  float val1;
  float val2;
  invalid.recnum = -1;
  invalid.pagenum = -1;


  if (scanDesc < 0 || scanDesc >= MAXSCANS) {
    HFerrno = HFE_SD;
    return invalid;
  }
  if (!scan_table[scanDesc].valid) {
    HFerrno = HFE_SCANNOTOPEN;
    return invalid;
  }

  file = &scan_table[scanDesc];
  f = &HFfile_table[file->HFfd];

  recid = HF_GetFirstRec(file->HFfd, record);

  switch (file->op) {
    case OP_EQ:
      for (i = 0; i < f->hdr.maxNumRecords; i++) {
        HF_GetRecordValue(file, &val1, &val2, record);
        if(val1 == val2) {
          return recid;
        }
        else {
          recid = HF_GetNextRec(file->HFfd, recid, record);
        }
        if(recid.recnum == invalid.recnum && recid.pagenum == invalid.pagenum) {
          HFerrno = HFE_INVALIDRECORD;
          return invalid;
        }
      }
      HFerrno = HFE_EOF;
      break;
    case OP_LT:
      for (i = 0; i < f->hdr.maxNumRecords; i++) {
        HF_GetRecordValue(file, &val1, &val2, record);
        if(val1 < val2) {
          return recid;
        }
        else {
          recid = HF_GetNextRec(file->HFfd, recid, record);
        }
        if(recid.recnum == invalid.recnum && recid.pagenum == invalid.pagenum) {
          HFerrno = HFE_INVALIDRECORD;
          return invalid;
        }
      }
      HFerrno = HFE_EOF;
      break;
    case OP_GT:
      for (i = 0; i < f->hdr.maxNumRecords; i++) {
        HF_GetRecordValue(file, &val1, &val2, record);
        if(val1 > val2) {
          return recid;
        }
        else {
          recid = HF_GetNextRec(file->HFfd, recid, record);
        }
        if(recid.recnum == invalid.recnum && recid.pagenum == invalid.pagenum) {
          HFerrno = HFE_INVALIDRECORD;
          return invalid;
        }
      }
      HFerrno = HFE_EOF;
      break;
    case OP_LTEQ:
      for (i = 0; i < f->hdr.maxNumRecords; i++) {
        HF_GetRecordValue(file, &val1, &val2, record);
        if(val1 <= val2) {
          return recid;
        }
        else {
          recid = HF_GetNextRec(file->HFfd, recid, record);
        }
        if(recid.recnum == invalid.recnum && recid.pagenum == invalid.pagenum) {
          HFerrno = HFE_INVALIDRECORD;
          return invalid;
        }
      }
      HFerrno = HFE_EOF;
      break;
    case OP_GTEQ:
      for (i = 0; i < f->hdr.maxNumRecords; i++) {
        HF_GetRecordValue(file, &val1, &val2, record);
        if(val1 >= val2) {
          return recid;
        }
        else {
          recid = HF_GetNextRec(file->HFfd, recid, record);
        }
        if(recid.recnum == invalid.recnum && recid.pagenum == invalid.pagenum) {
          HFerrno = HFE_INVALIDRECORD;
          return invalid;
        }
      }
      HFerrno = HFE_EOF;
      break;
    case OP_NEQ:
      for (i = 0; i < f->hdr.maxNumRecords; i++) {
        HF_GetRecordValue(file, &val1, &val2, record);
        if(val1 != val2) {
          return recid;
        }
        else {
          recid = HF_GetNextRec(file->HFfd, recid, record);
        }
        if(recid.recnum == invalid.recnum && recid.pagenum == invalid.pagenum) {
          HFerrno = HFE_INVALIDRECORD;
          return invalid;
        }
      }
      HFerrno = HFE_EOF;
      break;
    default:
      HFerrno = HFE_OPERATOR;
      return invalid;
    }

}

int HF_CloseFileScan(int scanDesc) {
  HFftab_ele *f;
  HFScanTab_ele *file;

  if(scanDesc < 0 || scanDesc >= MAXSCANS) {
    return HFE_SD;
  }
  file = &scan_table[scanDesc];
  f = &HFfile_table[file->HFfd];
  file->valid = FALSE;
  f->scanActive = FALSE;
}

bool_t HF_ValidRecId(int HFfd, RECID recid) {
  HFftab_ele *file;
  int HFerrno;

  if(recid.pagenum < 0 || recid.recnum < 0) {
    return FALSE;
  }

  if (HFfd < 0 || HFfd >= HF_FTAB_SIZE) {
    HFerrno = HFE_FD;
    return FALSE;
  }

  if (!HFfile_table[HFfd].valid) {
    HFerrno = HFE_FILENOTOPEN;
    return FALSE;
  }

  file = &HFfile_table[HFfd];
  /* The recid presented should have a pagenum and recnum
  smaller than what is indicated in header for the HF file*/
  if (file->hdr.numpages > recid.pagenum && file->hdr.numrecs > recid.recnum) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}

void HF_PrintError (char *errString) {
  printf("%s\n", errString );
}
