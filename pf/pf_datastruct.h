#ifndef __PF_DATA_STRUCT_H__

#define __PF_DATA_STRUCT_H__

#include "minirel.h"
/* file header and data page structure */
 typedef struct PFhdr_str {
	int 	firstfree;	/* first free page in the linked list 					of free pages*/
	int 	numpages;	/* number of pages in the file */
 } PFhdr_str;
/**************************************************************************
because of it says originally its from minirel.h and doesn't approve this here
***************************************************************************
PFpage {
	char pagebuf [PAGE_SIZE];
	int nextfree;	// page number of the next free page in the linked list 			of the free pages 
  } PFpage; 
**************************************************************************/
#define PF_HDR_SIZE sizeof(PFhdr_str)	/* size of file header */
#define PF_PAGE_LIST_END	-1	/* end of the list of free pages*/
#define PF_PAGE_USED		-2	/* page is being used */	
 
/* PF file table structure */
typedef struct PFftab_ele {
    bool_t    valid;       /* set to TRUE when a file is open. */
    char      *fname;      /* file name                        */
    int       unixfd;      /* Unix file descriptor             */
    PFhdr_str hdr;         /* file header                      */
    short     hdrchanged;  /* TRUE if file header has changed  */
} PFftab_ele;

/* Interface functions from Buffer Manager */
extern BF_GetBuf();
extern BF_AllocBuf();

#endif
