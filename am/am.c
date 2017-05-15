
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "minirel.h"
#include "pf.h"
#include "bf.h"
#include "am.h"
//#include "hf.h""
#include "amheader.h"

#define INNER 'I'
#define LEAF 'L'

#define AM_ErrorCheck if (err != PFE_OK) {AMerrno = AME_PF; return (AME_PF);}
enum { EQ =1, LT, LTQ, GT, GTE, NEQ };// for comparison


typedef struct node_header{
	char pageType;			/* type of node, inner or leaf */
	int  numRecords;		/* how many records in the node */
	int	 pageId;			/* next leaf node or the last pointer of an inner node */

	//int depth;
	//int attrLength;
	//char attrType;
} node_header;

/**typedef struct open_index{
		int depth;
		int total_numpages;
		int max_records_per_page;
		int max_numpages;
		
		char *filename;
}open_index[MAXOPENFILES];**/ 			//is needed??

typedef struct scan_entryindex{
	int 	fd;
	int 	status;				/*status 0 = empty, status 1 = exists  */
	int 	op;				/* to compare the index with the value */
	char 	*value;			/* the value which will be compared */
	char 	attrType;		
	int 	attrLength;		/* the size of key */
	int 	index;
	
	int		pageId;		//next leaf page
	//int 	last_pagenum;
	//int 	next_pagenum;
	//int 	lastBucketNum;
}scan_table[MAXSCANS];








//--------------------------Initialize----------------------------------
void AM_Init(void)
{
	/* Initialize HF layer*/
 	// HF_Init();

 	for ( int i = 0; i < MAXOPENFILES; i++) //this function is needed??
	 {}

	for(int i = 0; i < MAXSCANS; i++)
	{
	// scan_table[i].value = NULL;
	// scan_table[i].attrType = FALSE;???
	//	scan_table[i].attrLength = AM_INVALIDPARA;
	//	scan_table[i].op = AM_INVALIDPARA;
	//	scan_table[i].fd = AM_INVALIDPARA;

	scan_table[i].status = 1; 
	}
	AMerrno = AME_OK;
}
//------------------------------------------------------------------------------

/*Create an index numbered indexNo on the file filename*/
int AM_CreateIndex(char* filename, int indexNo, char attrType, int attrLength, bool_t isUnique)
{
	int 	err;		/* returns the error*/
	int 	AM_fd;			/* AM file descriptor*/	
	int 	pagenum;		/* (root_block) page number of the root page (first page)*/
	char 	*pagebuf;		/* (root_node)  buffer to hold a page*/
	
	//int depth;

	int 	records_per_node;	/*the number of records that are hold in the one page (node)  */
	char 	index_filename[strlen(filename)]; /* String to store the indexed files name with extensions*/
	//or??
	// char *file;
	//file = new char [strlen(filename)]		/**/
	//char index_filename[sizeof("testrel")]

//--- Check Attributes-------------------------------------------------
	/*  Check the parameters */
	if (( attrType != 'c') && (attrType != 'i') && (attrType != 'f'))
		{
			AMerrno = AME_INVALIDATTRTYPE;
			return(AME_INVALIDATTRTYPE);
		}
	if ((attrLength < 1) || (attrLength > 255))
		{
			AMerrno = AME_INVALIDATTRLENGTH;
			return(AME_INVALIDATTRLENGTH);
		}
	if (attrLength != 4 && (attrType != 'i' || attrType != 'f')) /* 4 for 'i' or 'f' */
	{
		AMerrno = AME_INVALIDATTRLENGTH;
		return(AME_INVALIDATTRLENGTH);
	}
	else			
		if (attrType != 'c' &&  (attrLength < 1 || attrLength != 4))	/* 1-255 for 'c' 	*/			
		{
			AMerrno = AME_INVALIDATTRLENGTH; 
			return(AME_INVALIDATTRLENGTH);
		}

	if (index_filename == NULL)		//this check is needed?
	{
		AMerrno = AME_NOMEM;
		return AME_NOMEM;
	}
//--------------------------------------------------------------------

	/* Get the fileName and create a paged file by its name*/
	sprintf(index_filename, "%s.%d", filename, indexNo);

//-----Create, Open, Allocate PF File------------------------------
	err = PF_CreateFile(index_filename);	//check this
	AM_ErrorCheck;

	/* Open the file from PF layer */
	AM_fd = err = PF_OpenFile(index_filename); //delete err?
	if (AM_fd == -1)
	{
		AMerrno = AME_PF;
		return AME_PF;
	}

	/*Allocate pages for the root*/
	err = PF_AllocPage(AM_fd, &pagenum, &pagebuf);
	AM_ErrorCheck;

//----------------------------------------------------------------------------------------------------
	//totalBlocks = 0; need?
	records_per_node = (PAGE_SIZE - sizeof(node_header))/(attrLength + sizeof(int)); 		//check 

	/* Initialize */
	memset(pagebuf , 0 , PAGE_SIZE);
	((/*typedef*/ struct node_header *)pagebuf)->numRecords = records_per_node;
	((/*typedef*/ struct node_header *)pagebuf)->pageType= LEAF;
	((/*typedef*/ struct node_header *)pagebuf)->pageId = -1;	// next leaf page or the last pointer of an inner node

	//((typedef struct node_header *)pagebuf)->depth = 0;
	//((typedef struct node_header *)pagebuf)->attrType = sizeof(char);
	//((typedef struct node_header *)pagebuf)->attrLength = sizeof(int);
	
	//to update the fd and pagebuf are needed??? if yes, I should add codes here

	err = PF_UnpinPage( AM_fd, pagenum, TRUE);
	AM_ErrorCheck;

	/*	Close the file */
	err = PF_CloseFile(AM_fd);
	AM_ErrorCheck;

	/* initialize the root page and the leftmost page numbers */
	//AM_rootPageNum = pagenum;??

	return AME_OK;

}

/* Destroys the index fileName, indexNo */ 
int AM_DestroyIndex(char *filename, int indexNo)
{
	int err;
	//char index_filename[sizeof("testrel")];		/* Indexed file */
	char index_filename[strlen(filename)];		//Pwe need max index name..strlen of file name is suitable??

	// check filename == NULL??   is needed???
	//check IndexNo < 0 ?
	if (index_filename == NULL)		//this check is needed?
	{
		AMerrno = AME_NOMEM;
		return AME_NOMEM;
	}
	//..check smth else..?

	sprintf(index_filename, "%s.%d", filename, indexNo);  //needed?
	
	err = PF_DestroyFile(index_filename);
	AM_ErrorCheck;
	
	//free(index_filename);
	return AME_OK;
}

int AM_OpenIndex(char *filename, int indexNo)
{
	int AM_fd;
	char index_filename[strlen(filename)];
	//char index_filename[sizeof("testrel")]; /* String to store the indexed files name with extensions*/

	if (index_filename == NULL)		//this check is needed?
	{
		return AMerrno;
	}
	sprintf(index_filename, /*sizeof(index_filename),/* %s.%.4i.index or */"%s.%d", filename, indexNo); //

	if ((AM_fd = PF_OpenFile(index_filename)) < 0)
	{
		AMerrno = AM_fd;
		return AME_FD;	//or AM_fd
	}

	
	return AM_fd;	/* returns the value of file descriptior of B+ tree index - index into the index table */
}

int AM_CloseIndex(int AM_fd)
{
	return PF_CloseFile(AM_fd);
	//close the index file with the indicated file descriptor by calling PF_CloseFile().
	//It deletes the entry for this index from the index table
}

int AM_InsertEntry(int AM_fd, char *value, RECID recId)
{
	//inserts (value, recId) pair into the index represented by the open file with AM_fd
	//value parameter points to value to be inserted into the index (key)
	//recId parameter idenfities a record with that value to be added to the index
	//AME_OK
	//Otherwise AM error code
	int 	err;
	int 	pagenum;	/* the page number in the buffer */
	char 	*pagebuf;	/* buffer to hold a page */
	void 	*val;		/* the size of key value from attrLength*/

	int 	index;	 	/* the index where key can be inserted or found */	
	int 	status;  /* key is in tree (old) ornot (new) */

	//int inserted_key;	/* if the key was inserted into the leaf or it is needed to split */
	//int add_to_parent;			/* whether the key has to be added to parent ??*/

	
	//val = malloc(attrLength);
	//if(!val ) return AME_NOMEM;
	//memcpy(val, value, attrLength);
	
//------Check AM_Fd. value---------------------------------------------------------------
	if (value == NULL)
	{
		AMerrno =  AME_INVALIDVALUE;
		return AME_INVALIDVALUE;
	}

	/**if (AM_fd == -1)
	{
		AMerrno = AME_FD;
	return AME_FD;
	}*/
	
//--- Check Attributes-------------------------------------------------
	/*  Check the parameters */ //extract the attrType and length from where? AM_fd??
	if (( attrType != 'c') && (attrType != 'i') && (attrType != 'f'))
		{
			AMerrno = AME_INVALIDATTRTYPE;
			return(AME_INVALIDATTRTYPE);
		}
	if ((attrLength < 1) || (attrLength > 255))
		{
			AMerrno = AME_INVALIDATTRLENGTH;
			return(AME_INVALIDATTRLENGTH);
		}
	if (attrLength != 4 && (attrType != 'i' || attrType != 'f')) /* 4 for 'i' or 'f' */
	{
		AMerrno = AME_INVALIDATTRLENGTH;
		return(AME_INVALIDATTRLENGTH);
	}
	else			
		if (attrType != 'c' &&  (attrLength < 1 || attrLength != 4))	/* 1-255 for 'c' 	*/			
		{
			AMerrno = AME_INVALIDATTRLENGTH; 
			return(AME_INVALIDATTRLENGTH);
		}

//-----------FIND LEAF-------------------------------------
	/* To find the leaf node for the key */
	err = find_leaf(AM_fd,value, attrType, attLength /*, &pagenum, &pagebuf, &index*/); 
	
	/* Check error */
	if (err != AME_OK )
	{
		AMerrno = err;
		return err;
	}

//------------INSERT-----------------------------------------------------
	/* To insert a pair (key value and recID) into the leaf*/
//	inserted_key = AM_insert_leaf(pagebuf,value, recId, header->attrLength, index, keystatus);

	err = insert_value_to_node(AM_fe, value, attrType, attrLength,recId );

	/**If key is inserted, return AME_OK */
	if ( err != AME_OK)
	{
		Amerrno = err;
		return err;
	}
	
	//check if inserted (TRUE/FALSE)
	//..TODO


//------------SPLIT----------------------------------
	/* To allocate a new page (block) to the new root */
	err = PF_AllocPage(AM_fd,&pagenum, &pagebuf);
	if( err != PFE_OK)
	{
		AMerrno = err;
		return err;
	}

	//..
	//((struct node_header *)pagebuf)->pageId = left;
	//((struct node_header *)pagebuf)->type = INNER;

	err = PF_UnpinPage(AM_fd, pagenum, TRUE);
	if( err != PFE_OK)
	{
		AMerrno = err;
		return err;
	}

	
	free(val);
	return AME_OK;

}


int AM_DeleteEntry(int AM_fd, char*value, RECID recId)
{
	//removes (value, recId) pair from the index represented by the open file associated with AM_fd
	//AME_OK
	//Otherwise AM code error
	char 	*pagebuf;		/* a page( node) */
	int 	pagenum;		/* the page number of the page (block)*/
	int 	index;			/* the index where key is present/ where is the current key*/
	int 	status;

	//int 	keysize;		/* the size of the record/key  / the length of key, ptr pair for a leaf */
	int 	err;		/* holds the return value of functions called within this function */
	
//------------Chech parameters, fd, value---------------------
	/** To check the parameters */
	if ( (attrType != 'i') && (attrType != 'f' ) && (attrType != 'c') )
	{
		AMerrno = AME_INVALIDVALUE;
		return (AME_INVALIDVALUE);
	}

	if (AM_fd == -1)
	{
		AMerrno = AME_FD;
		return (AME_FD);
	}

	if( value == NULL )
	{
		AMerrno = AME_INVALIDATTRTYPE;
		return (AME_INVALIDATTRTYPE);
	}

//-------------------------------

	/* To find the pagenum and the indexNo of the key to be deleted if there is*/
	err = find_leaf(AM_fd, value, attrType, attrLength /*, &pagenum, &pagebuf, &index???*/);
	if( err != AME_OK) 
	{
		Amerrno = err;
		return err;
	}
	/* To check if it returns error value */
	if( err= PF_GetThisPage(AM_fd, /*pointer to get pageId*/ &pagebuf) != PFE_OK)
		{
		AMerrno = err;
		return err;
	}
	//TODO
		//if P == recId ,ccompare,set pointer recId etc...
		/* the key is not in the B+ tree */
		//if(err = NO_KEY)
		//{
		//	AMerrno = AME_KEYNOTFOUND;
		//	return(AME_KEYNOTFOUND);
		//}

	//-----------Modification is needed-----------------------------------------------------------------------------------------------------------------------	

	//TODO
	//------------------------------------------------------------------------------
	
	err = PF_UnpinPage(AM_fd, pagenum, TRUE);

	return err;


}

int AM_OpenIndexScan(int AM_fd, int op, char *value)
{
	int i; 	/* for a loop */
//-----Check FD, attribute, value --------------------------------------
	if (AM_fd < 0 )/*|| AM_fd > MAXOPENFILES ) /*|| !scan_table[AM_fd].status) */ 
	{
		AMerrno = AME_FD;
		return AME_FD;
	}

	if( (scan_table[AM_fd].attrType != 'i') && (scan_table[AM_fd].attrType != 'c') && (scan_table[AM_fd].attrType != 'f' ) )		//enough? or add more?
	{
		AMerrno = AME_INVALIDATTRTYPE;
		return AME_INVALIDATTRTYPE;
	}

//------------------------------------------------------------------


	// to find a free place in the scan table
	for( i = 0; i < MAXSCANS; i++)				
		if(scan_table[i].status) break;	
	
	// if no a free place found, then the scan table is full 
	if ( i == MAXSCANS)
	{
		AMerrno =AME_SCANTABLEFULL;
		return AME_SCANTABLEFULL;					
	} 	

	
	scan_table[i].value = malloc(scan_table[i].attrLength);
	if (!scan_table[i].value)  // if value is NULL??
	{
		AMerrno = AME_NOMEM;
		return AME_NOMEM;
	}

	memcpy(scan_table[i].value, value, scan_table[i].attrLength);
	
	scan_table[i].fd = AM_fd;				
	scan_table[i].op = op;
	scan_table[i].index = -1;		//last record or last bucket number 

	//scan_table[i].attrType = attrType;
	//scan_table[i].attrLength = attrLength;

	//open an index scan over the index
	// The scan descriptor returned is an index into the index scan table (in HF layer similar)
	// If the index scan table is full, AM error code is returned in place of a scan descriptor
	//the value parameter will point to a (binary) value that indexed attribute values are to be compared with
	//The scan will return the record ids of those records whose indexed attribute value matches the value parameter in the desired way

	//case on OP
	switch (op)	//needed?? TODO
	{
		//case EQ:
			break;
		//case LT: 
			break;
		//case GT:
			break;
		//case LTQ:
			break;
		//case GTQ: 
			break;
		//case NQ: 
			break;
		//default: 
			//AMerrno = AME_INVALIDOP; //invalid op to scan
			//return AME_INVALIDOP;
			break;
	}
	//errVal = PF_UnpinPage(AM_fd, find_paagenum, FALSE);
	//AM_ErrorCheck;
	
	scan_table[i].status = 0;
	return i;		
}

RECID AM_FindNextEntry(int scanDesc)
{
	//returns record id of the next record that satisfied the confitions specified for an index scan associated with scanDesc
	//if there is no more records
	int 	AM_fd;
	int 	recId;		/* record ID to be returned*/
	int 	next_leaf_page;
	char 	*pagebuf; //node
	int 	err;
	int 	comp;
	int 	size_record;	/* the size of key, ptr pair for leaf */

	
	//Check if a scan is valid
	if( (scanDesc < 0) || (scanDesc >= MAXSCANS) || (!scan_table->status) ) //check this condition!
	{
		AMerrno = AME_INVALIDSCANDESC;
		return AME_INVALIDSCANDESC;
	}
	
	// Check if scan over??
	//if(scan_entryindex[scNDesc].status == OVER) return AME_EOF;

	/* Get the page (block) to start the scanning */
	if( (err = PF_GetThisPage(scan_table->fd, scan_table->pageId, &pagebuf) != PFE_OK) )
		{
			AMerrno = err;
			return err;
		}

	/* Start the search of the next record of that is needed */
	while(1)
	{
		if( (err = PF_GetThisPage(scan_table->fd, scan_table->pageId, &pagebuf) != PFE_OK) )
			{
				AMerrno = err;
				return err;
			}
	//...TODO
	//find a valid entry
	//compare
	//last node
	// unpin page?
		
		/* to check if op is not equal, then check if we have to skip this value */
		//if( scan_table->op == NEQ)
			//...TODO
		/* if op is equal , then check if you are done */	
		//if( scan_table->op == EQ)
			//..TODO
		/*to check if you are at the l
		ast record if io is < or <= */
		//if( scan_table->op == LT || LTQ)
			//..TODOs
		//return recId;


	//-------------------------------------------------	
	/* find a valid entry */

	/* the end of node (last node) */
		if(scan_table->index == (PAGE_SIZE - sizeof(struct node_header))/((scan_table->attrLength)+sizeof(int)) ) //check!  index == a record is a pair <v, p>
		{
			next_leaf_page = ((struct node_header *)(pagebuf))->pageId;  // node get next block
			if( (err = PF_UnpinPage(scan_table->fd, scan_table->pageId, FALSE) != PFE_OK) )// unpin block
			{
				AMerrno = err;
				return err;
			}
		
			if (next_leaf_page == -1 )	return AME_EOF;

			scan_table->pageId = next_leaf_page;
			scan_table->index -1;
		//continue;	
		}
		//TODO with a pair of <p , v> ???
		recId = (*(int)((char*)(pagebuf) + sizeof(struct node_header) + (index *(attrLength + sizeof(int) ) ) + (attrLength) ) ); //node gets pointer (recId)
		/* unpin the current page*/
		if( (err = PF_UnpinPage(scan_table->fd, scan_table->pageId, FALSE) != PFE_OK) )
		{
			AMerrno = err;
			return err;
		} 

		return recId;
	}	//end while
}

int AM_CloseIndexScan(int scanDesc)
{
	//Check for valid input
	if( (scanDesc < 0) /*&&*/|| (scanDesc >= MAXSCANS) /*&&*/|| (!scan_table[scanDesc].status)) //check this condition!
	{
		AMerrno = AME_INVALIDSCANDESC;
		return AME_INVALIDSCANDESC;
	}
	scan_table[scanDesc].status = 1; //status = FREE

	free(scan_table[scanDesc].value); //to NULL the value
	return AME_OK;
}

void AM_PrintError(char *errString)
{
// fprintf(stderr, "%s", errString);
// fprintf(stderr, "%s", AMerrno*-1);
//	if (AMerrno == AME_PF)
//	PF_PrintError("");
//else 
// fprintf(stderr, "\n");
//	printf("%s.%s\n", errString, AM_errors[(AMerrno+1)* -1] );
}


static int find_leaf(int AM_fd,char *value, char attrType, int attrLength, int *pagenum , char **pagebuf, int *index )
{
    int     err;       /* returns error value*/
    int     pagenum; //node_block
    char    *pagebuf;    //node
    int     pageId;//next page
    
    //..TODO

    /* To get the root of the B+ tree */
    err = PF_GetFirstPage(AM_fd, pagenum, pagebuf);
    AM_ErrorCheck;

    while(1)
    {
        //..TODO
        if( (err = PF_GetThisPage(fd, pagenum, FALSE)) != PFE_OK ) return err;
                    
        if( ( (struct node_header*)(pagebuf) )->pageType == LEAF ) //if root is a leaf page
        {
            // TODO
        }

    }
}
static int compare(char attrType, void *value1, void *value2, int op)
{
    int ret, res;
    switch (attrType)
    {
        case 'c':
            res = strncmp (value 1, value2, attrLength);
        case 'i':
            if (*(int *)value1 == *(int*)value2)        res = 0;
            else if (*(int *)value1 > *(int *)value2)   res = 1;
            if(*(int *)value1 < *(int *)value2)         res = -1;

        case 'f':
            if (*(float *)value1 == *(float *)value2)       res = 0;
            else if (*(float *)value1 > *(float *)value2)   res = 1;
            if(*(float *)value1 < *(float *)value2)         res = -1;
            
            break;
    }

    switch(op)
    {
        case LT:
            ret = (res < 0)     //low   not foubd
            break:
        case LEQ:
            ret = ( res < 0 || res == 0);
            break;
        case GT:
            ret = ( res > 0 || res == 0 );
            break;
        case GEQ:
            ret = (res>0 || res ==0);
            break;
        case EQ: 
            ret = (res == 0);       //low found
            break;
    }
    return ret;

}

static int insert_value_into_leaf(int fd, int pagenum, void *value, int p, char attrType, int attrLength, char *extra_page, void *value2)
{
    char *pagebuf;
    char attrType;
    int attrLength;
	//TODO

}