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
  char *filename;
  HFhdr_str hdr;
  bool_t hdrchanged;
  bool_t scanActive; /*TRUE if scan is active*/
}

HFftab_ele file_table[HF_FTAB_SIZE];

void HF_Init(void) {
  //initialize other data strctures needed for this layer
  PF_Init();
}

int HF_CreateFile(char *fileName, int recSize) {
  HFhdr_str *hdr;

  if(PF_CreateFile(filename)!=0) {
    return HFE_PF;
  }

  if(PF_OpenFile(filename) < 0) {
    return HFE_PF;
  }

}

int HF_DestroyFile(char *fileName) {

}

int HF_OpenFile(char *fileName) {

}

int HF_CloseFile(int HFfd) {

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
