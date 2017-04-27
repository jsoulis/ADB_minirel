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

void HF_Init(void) {

}

int HF_CreateFile(char *fileName, int recSize) {

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
