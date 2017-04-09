#include "minirel.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>

#include "pf.h"
#include "pf_datastruct.h"


int PFerrno = PFE_OK;	

/* assing open file entry to the table */
static PFftab_ele PFftab[PF_FTAB_SIZE]; 

/* find the index to PF file table entry whose "filename" field is same as "filename" */

static PFtabFindFilename(char *filename)
{ 
int i;

	for (i=0; i < PF_FTAB_SIZE; i++){
        	if(PFftab[i].fname != NULL&& strcmp(PFftab[i].fname,filename) == 0)
                /* found it */
                return(i);
        }
        return(-1);
}
/* read function*/
PFreadfunc(int fd, int pagenum, PFpage *buf)
{
int error_value;

/* seek to the appropriate place */
        if ((error_value=lseek(PFftab[fd].unixfd,pagenum*sizeof(PFpage)+PF_HDR_SIZE)) == -1){
                PFerrno = PFE_UNIX;
                return(PFerrno);
        }

/* read the data */
        if((error_value=read(PFftab[fd].unixfd,(char *)buf,sizeof(PFpage)))
                !=sizeof(PFpage)){
                if (error_value <0)
                        PFerrno = PFE_UNIX;
                else    PFerrno = PFE_HDRREAD;
                return(PFerrno);
        }

        return(PFE_OK);
}
PFwritefunc(int fd, int pagenum, PFpage *buf)
{
int error_value;

/* seek to the right place */
        if ((error_value=lseek(PFftab[fd].unixfd,pagenum*sizeof(PFpage)+PF_HDR_SIZE)) == -1){
                PFerrno = PFE_UNIX;
                return(PFerrno);
        }

/* write out the page */
        if((error_value=write(PFftab[fd].unixfd,(char *)buf,sizeof(PFpage)))
                        !=sizeof(PFpage)){
                if (error_value <0)
                        PFerrno = PFE_UNIX;
                else    PFerrno = PFE_HDRWRITE;
                return(PFerrno);
        }

        return(PFE_OK);

}

/* true if file descript r fd is invaild */
#define PFinvalidFd(fd) ((fd) < 0 || (fd) >= PF_FTAB_SIZE \
				|| PFftab[fd].fname == NULL)

/* true if page number of file is invalid in case of -> 
 0 or >= number of pages in the file */
#define PFinvalidPagenum(fd,pagenum) ((pagenum)<0 || (pagenum) >= \
				PFftab[fd].hdr.numpages)

void PF_Init(void) {
int i;
/* init the hash table */
	BF_Init();
/* init the file table to be not used*/
	for (i=0; i < PF_FTAB_SIZE; i++){
		PFftab[i].fname = NULL;
	}
}
/* create a file named filename. the file should not have already existed */
int PF_CreateFile(char *filename) {
int fd;	/* unix file descripotr */
PFhdr_str hdr;	/* file header */
int error_value;

/* create file for use */
	if ((fd=open(filename,O_CREAT|O_EXCL|O_WRONLY,0664))<0){
/* unix error on open */
		PFerrno = PFE_UNIX;
		return(PFE_UNIX);
	}

/* write out the file header */
	hdr.firstfree = PF_PAGE_LIST_END;	/* no free page yet */
	hdr.numpages = 0;
	if ((error_value=write(fd,(char *)&hdr,sizeof(hdr))) != sizeof(hdr)){
/* error while writing. Abort everything. */
		if (error_value < 0)
			PFerrno = PFE_UNIX;
		else PFerrno = PFE_HDRWRITE;
		close(fd);
		unlink(filename);
		return(PFerrno);
	}

	if ((error_value=close(fd)) == -1){
		PFerrno = PFE_UNIX;
		return(PFerrno);
	}

	return(PFE_OK);
}
 /* destroy the file filename. the file shoudl have existed and should not be opened */
int PF_DestroyFile(char *filename) {
int error_value;

        if (PFtabFindFilename(filename)!= -1){
                /* file is open */
                PFerrno = PFE_FILEOPEN;
                return(PFerrno);
        }

        if ((error_value =unlink(filename))!= 0){
                /* unix error */
                PFerrno = PFE_UNIX;
                return(PFerrno);
           return(PFerrno);
        }

        /* success */
        return(PFE_OK);
}

/* open the file named filename */

int PF_OpenFile(char *filename) {
int count;      /* number of bytes in read */
int fd;

/* find a free entry in the file table */
        if ((fd=PFftabFindFree())< 0){
                /* file table full */
                PFerrno = PFE_FTABFULL;
                return(PFerrno);
        }

/* open the file */
        if ((PFftab[fd].unixfd = open(filename,O_RDWR))< 0){
                /* can't open the file */
                PFerrno = PFE_UNIX;
                return(PFerrno);
        }
 /* Read the file header */
        if ((count=read(PFftab[fd].unixfd,(char *)&PFftab[fd].hdr,PF_HDR_SIZE))
                                != PF_HDR_SIZE){
                if (count < 0)
                        /* unix error */
                        PFerrno = PFE_UNIX;
                else    /* not enough bytes in file */
                        PFerrno = PFE_HDRREAD;
                close(PFftab[fd].unixfd);
                return(PFerrno);
        }
/* set file header to be not changed */
        PFftab[fd].hdrchanged = FALSE;

/* save the file name */
        if (filename == NULL){

close(PFftab[fd].unixfd);
/* no space */
                PFerrno = PFE_FTABFULL;
                return(PFerrno);
        }
        strcpy(PFftab[fd].fname, filename);

        return(fd);
}

/* close the file associated with PF file descriptor fd*/
int PF_CloseFile(int fd) {
int error_value;

	if (PFinvalidFd(fd)){

/* invalid file descriptor */
		PFerrno = PFE_FD;
		return(PFerrno);
	}

	 if (PFftab[fd].hdrchanged){

/* write the header back to the file */
/* seek to the appropriate place */
		if ((error_value=lseek(PFftab[fd].unixfd,(unsigned)0)) == -1){
/* seek error */
			PFerrno = PFE_UNIX;
			return(PFerrno);
		}

/* write header*/
		if((error_value=write(PFftab[fd].unixfd, (char *)&PFftab[fd].hdr,
				PF_HDR_SIZE))!=PF_HDR_SIZE){
			if (error_value <0)
				PFerrno = PFE_UNIX;
			else	PFerrno = PFE_HDRWRITE;
			return(PFerrno);
		}
		PFftab[fd].hdrchanged = FALSE;
	}

/* close the file */
	if ((error_value=close(PFftab[fd].unixfd))== -1){
		PFerrno = PFE_UNIX;
		return(PFerrno);
	}

/* free the filename space */
	free((char *)PFftab[fd].fname);
	PFftab[fd].fname = NULL;

	return(PFE_OK);
}
/* allocate a new page for fd  */
int PF_AllocPage(int fd, int *pagenum, char **pagebuf) {
 PFpage *fpage; /* pointer to file page */
int error_value;
 
        if (PFinvalidFd(fd)) {
                 PFerrno= PFE_FD;
                 return(PFerrno);
         }
 
        if (PFftab[fd].hdr.firstfree != PF_PAGE_LIST_END) {

/* get a page from the free list */
                 *pagenum = PFftab[fd].hdr.firstfree;
                if ((error_value=BF_GetBuf(fd,*pagenum,&fpage,PFreadfunc,
                                         PFwritefunc))!= PFE_OK)
/* can't get the page */
                        return(error_value);
                 PFftab[fd].hdr.firstfree = fpage->nextfree;
                 PFftab[fd].hdrchanged = TRUE;
         }
         else {

/* free list empty, allocate one more page from t    he file */
                 *pagenum = PFftab[fd].hdr.numpages;
                if ((error_value=BF_AllocBuf(fd,*pagenum,&fpage,PFwritefunc ) ) != PFE_OK)          

/* can't allocate a page */
                 return(error_value);
 
/* increment number of pages for this file */
                 PFftab[fd].hdr.numpages++;
                 PFftab[fd].hdrchanged = TRUE;
 
/* mark this page dirty */
                if ((error_value=BF_TouchBuf(fd,*pagenum))!= PFE_OK){
                        printf("internal error: PFalloc()\n");
                        exit(1); 
                }
	}

/* Mark the new page used */
        fpage -> nextfree = PF_PAGE_USED;

/* return value */
        *pagebuf = fpage->pagebuf;
        	return(PFE_OK);
}

/* */
int PF_GetNextPage(int fd, int *pagenum, char **pagebuf) {
int tmppage;	/* page number to scan for next valid page */
int error_value;	/* error code */
PFpage *fpage;	/* pointer to file page */

	if (PFinvalidFd(fd)){
		PFerrno = PFE_FD;
		return(PFerrno);
	}
	if (*pagenum < -1 || *pagenum >= PFftab[fd].hdr.numpages){
		PFerrno = PFE_INVALIDPAGE;
		return(PFerrno);
	}

/* scan the file until a valid used page is found */
	for (tmppage= *pagenum+1;tmppage<PFftab[fd].hdr.numpages;tmppage++){
		if ( (error_value=BF_GetBuf(fd,tmppage,&fpage,PFreadfunc,
					PFwritefunc))!= PFE_OK)
			return(error_value);
		else if (fpage -> nextfree == PF_PAGE_USED){
/* found a used page */
			*pagenum = tmppage;
			*pagebuf = (char *)fpage->pagebuf;
			return(PFE_OK);
		}

/* page is free, unfix it */
		if ((error_value=PF_UnfinPage(fd,tmppage,FALSE))!= PFE_OK)
			return(error_value);
	}

/* No valid used page found */
	PFerrno = PFE_EOF;
	return(PFerrno);
}

 /* read the first page in the file with fd */
int PF_GetFirstPage(int fd, int *pagenum, char **pagebuf) {
         *pagenum = -1;
         return(PF_GetNextPage(fd,pagenum,pagebuf));
 }

/* read a valid page specified by pagenum from fd */ 
int PF_GetThisPage(int fd, int pagenum, char **pagebuf) {
int error_value;
PFpage *fpage;

	if (PFinvalidFd(fd)){
		PFerrno = PFE_FD;
		return(PFerrno);
	}

	if (PFinvalidPagenum(fd,pagenum)){
		PFerrno = PFE_INVALIDPAGE;
		return(PFerrno);
	}

	if ( (error_value=BF_GetBuf(fd,pagenum,&fpage,PFreadfunc,PFwritefunc))!= PFE_OK) 
	
	if (fpage->nextfree == PF_PAGE_USED){
/* page is used*/
		*pagebuf = (char *)fpage -> pagebuf;
		return(PFE_OK);
	}
	else {
/* invalid page */
		if (PF_UnpinPage(fd,pagenum,FALSE)!= PFE_OK){
			printf("internal error:PFgetThis()\n");
			exit(1);
		}
		PFerrno = PFE_INVALIDPAGE;
		return(PFerrno);
	}
}

/*after checking the validity of the fd and pagenum values, this function marks the page associated with fd and pagenum */
int PF_DirtyPage(int fd, int pagenum) {
PFpage *fpage; /* pointer to file page */
int error_value;

        if (PFinvalidFd(fd)){
                PFerrno = PFE_FD;
                return(PFerrno);
        }

        if (PFinvalidPagenum(fd,pagenum)){
                PFerrno = PFE_INVALIDPAGE;
                return(PFerrno);
        }

        if ((error_value=BF_GetBuf(fd,pagenum,&fpage,PFreadfunc,PFwritefunc))!= PFE_OK)

/* can't get this page */
                return(error_value);

	if (fpage -> nextfree != PF_PAGE_USED){
/* this page already freed */
		if (PF_UnpinPage(fd,pagenum,FALSE)!= PFE_OK){
			printf("internal error: PFdirty()\n");
			exit(1);
		}
		PFerrno = PFE_PAGEFREE;
		return(PFerrno);
	}

/* put this page into the free list */
	fpage -> nextfree = PFftab[fd].hdr.firstfree;
	PFftab[fd].hdr.firstfree = pagenum;
	PFftab[fd].hdrchanged = TRUE;

/* unpin this page */
	return(PF_UnpinPage(fd,pagenum,TRUE));
}

/*after checking the validity of the fd and pagenum values, this function marks the dirty associated with fd and pagenum  */
int PF_UnpinPage(int fd, int pagenum, int dirty) {
	if (PFinvalidFd(fd)){
		PFerrno = PFE_FD;
		return(PFerrno);
	}

	if (PFinvalidPagenum(fd,pagenum)){
		PFerrno = PFE_INVALIDPAGE;
		return(PFerrno);
	}

	return(PF_UnpinPage(fd,pagenum,dirty));
}

/* error messages */
static char *PFerrormsg[]={
"No error",
"No memory",
"No buffer space",
"unix error",
"incomplete read of page from file",
"incomplete write of page to file",
"incomplete read of header from file",
"incomplete write of header from file",
"invalid page number",
"file already open",
"file table full",
"invalid file descriptor",
"end of file",
"page already free",
"page already unfixed",
"new page to be allocated already in buffer",
"hash table entry not found",
"page already in hash table"

};
