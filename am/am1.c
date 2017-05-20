
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
//#include <stdarg.h>


#define INTERNAL 'I'
#define LEAF 'L'
//#define NULL ((void *)0)
#define AM_ErrorCheck if (err != PFE_OK) {AMerrno = err; return AME_PF;}

#define p 1
#define v  2
#define pp  3
#define vv  4
#define pp_ 5
#define vv_ 6

struct root_node_header 
{
	int root_block;
};

struct node_header{
	char pageType;			/* type of node, inner or leaf */
	int  numRecords;		/* how many records in the node */
	int	 pageId;			/* next leaf node or the last pointer of an inner node */

	//int depth;
	int attrLength;
	char attrType;
};
struct two_nodes_header	//the header of two nodes, this is used for splitting
{
	int num_records;	//
	int pageId;	
};
struct leaf_nodes{		//an array of info about leaf nodes
	int pagenum;
	int index;
	int array[100];  //change it!
};

struct open_index{
	int 	depth;					// the total depth of B+ tree index
	int		is_empty;				// the index exists or not
	//int 	total_numpages;			// total number of blocks
	//int 	max_records;			// max number of records in the index
	//int 	max_numpages;			// total number of pages in the file
	char 	attrType;				// the type of value
	char 	*filename;				// the file name of the index
	int 	attrLength;				// the size of value in bytes
	//char *hash_table;
};
struct open_index AM_index_table[MAXOPENFILES];

struct scan_entryindex{
	int 	fd;
	int 	is_empty;				/* 1 = true = empty  0 = false = not empty (exists) */
	int 	op;				/* to compare the index with the value */
	char 	*value;			/* the value which will be compared */
	char 	attrType;		
	int 	attrLength;		/* the size of key */
	int 	index;
	
	int		pageId;		//next leaf page
	//int 	last_pagenum;
	//int 	next_pagenum;
	//int 	lastBucketNum;
};

struct scan_entryindex AM_scan_table[MAXSCANS];

 int num_records(int attrLength) // the number of entries
{
	return ( (PAGE_SIZE - sizeof(struct node_header))/ ((attrLength + sizeof(int))));
	}

char *filename_size(char *filename, int indexNo)
{
	int fn_size;
	char *index_filename;

	/*Check for a valid input*/
	if( filename == NULL)
	{
		//AMerrno = AME_INVALIDFILENAME;
		return NULL;
	}
	if (indexNo == -1)
	{	
		//AMerrno = AME_INVALIDINDEXNO;
		return NULL;
	}

	int timesCanBeDividedByTen = 0;
	int tempIndexNo = indexNo;
	while (tempIndexNo)
	{
		++timesCanBeDividedByTen;
		tempIndexNo/=10;
	}

	/*The length size */
	fn_size = strlen (filename) + tempIndexNo + 1; // + 1 for \0

	index_filename = malloc (fn_size + sizeof(char));

	if(index_filename == NULL)
	{
		AMerrno = AME_NOMEM;
		return NULL;
	}
	return index_filename;
}

// This function returns the root node
static int root_node(int AM_fd) //GetThisPage
{
	int err;
	int root_block;
	struct root_node_header *root_header;

	if ( (err = PF_GetThisPage(AM_fd, 0, (char**)&root_header) ) != PFE_OK)//GetFirstPage?
	{
		AMerrno = err;
		return err;
	} 

	root_block = root_header->root_block;

	if ( (err = PF_UnpinPage(AM_fd, 0, 0)) != PFE_OK)
	{
		AMerrno = err;
		return err;
	}
	return root_block;
}

static int empty_root_node(int AM_fd, int new_root)//GetFirstPage
{
	int err;
	struct root_node_header *root_header;

	if ( ( err = PF_GetFirstPage(AM_fd,0, (char**)&root_header) ) < 0 )
		return err;
	
	root_header->root_block = new_root;
	if (( err = PF_UnpinPage (AM_fd, 0, TRUE)) < 0 ) return err;
	
	return AME_OK;
	}

static int upd_root_node(int AM_fd, int new_root)
{
	int err;
	struct root_node_header *root_header;

	if ( ( err = PF_GetThisPage(AM_fd,0, (char**)&root_header) ) < 0 )
		return err;
	
	root_header->root_block = new_root;
	if (( err = PF_UnpinPage (AM_fd, 0, TRUE)) < 0 ) return err;
	
	return AME_OK;
	}
//---
static int next_page(char *pagebuf)
{
	return ((struct node_header *)pagebuf)->pageId;
}

#define node_v(pagebuf, i, attrLen) \
	((char *) (pagebuf) + sizeof(struct node_header) \
	 + (i)*((attrLen) + sizeof(int)))


#define node_p(pagebuf, i, attrLen) \
	(*(int *)((char *) (pagebuf) + sizeof(struct node_header) \
	 + (i)*((attrLen) + sizeof(int)) + (attrLen)))


#define set_v(pagebuf, i, val, attrLen) \
	memcpy(node_v((pagebuf), (i), (attrLen)), (val), (attrLen))



#define set_p(pagebuf, i, val, attrLen) \
	(node_p((pagebuf), (i), (attrLen)) = (val))


#define two_nodes_v(pagebuf, i, attrLen) \
	((char *) (pagebuf) + sizeof(struct two_nodes_header) \
	 + (i)*((attrLen) + sizeof(int)))


#define two_nodes_p(pagebuf, i, attrLen) \
	(*(int *)((char *) (pagebuf) + sizeof(struct two_nodes_header) \
	 + (i)*((attrLen) + sizeof(int)) + (attrLen)))


#define set_two_v(pagebuf, i, val, attrLen) \
	memcpy(two_nodes_v((pagebuf), (i), (attrLen)), (val), (attrLen))


#define set_two_p(pagebuf, i, val, attrLen) \
	(two_nodes_p((pagebuf), (i), (attrLen)) = (val))


/**int pointer_value(int pv, char *pagebuf, int i, int attrLength)
{
	int node_p, *pp_nodes, *vv_nodes;
	int node_v;
	switch (pv)
	{
		case p: //one pointer
		
			*node_p = (*(int*)((char *)(pagebuf) + sizeof(struct node_header) + (i)*(attrLength + sizeof(int)) + attrLength ));
			return node_p;
		
		case v: //one value
		{
			node_v = ((char *) (pagebuf) + sizeof(struct node_header)  + (i)*((attrLength) + sizeof(int)));
			return node_v;
		}
		case pp: //two pointers
		{
			pp_nodes = (*(int*)((char*) (pagebuf) + sizeof(struct two_nodes_header) + (i)*((attrLength) + sizeof(int)) + attrLength ));
			return pp_nodes;
		}
		case vv: //two nodes
		{
			vv_nodes = (((char*) (pagebuf) + sizeof(struct two_nodes_header) + (i)*((attrLength) + sizeof(int)) ));
		/*	return vv_nodes;
		}
	}
}	

int set_two_p_v(int ppvv, char /* * /pagebuf, int i, int value, int attrLength)
{
	int two_p;
	int two_v;
	int k =i;
	//k = malloc(sizeof(i));
	int l;
	//l = malloc(sizeof(j));
	int m;
	switch(ppvv)
	{
		case pp_: //To set two pointers
			{
			two_p= (*pointer_value(pp, pagebuf, k, attrLength) = value) ;
			//two_p = *pointer_value(p,pagebuf, k, attrLength);
			//two_p = j;

			return two_p;
			}
			
		case vv_:
		{
			two_v = memcpy( *pointer_value(vv, pagebuf, k, attrLength), value , attrLength);
			return two_v;
		}
	}
}*/

//#define two_p(pagebuf, i, value, attrLength)\
	(two_nodes_p((pagebuf),(i), (attrLength) ) = (value)) 

	static int leaf_node(int AM_fd, int attrLength)
	{
		int err;
		int leaf_node;
		char *pagebuf;
		int tmp_node;

		if ((err=leaf_node = root_node(AM_fd)) < 0)
		{
			AMerrno = err;
			return err;
		}
		
		if((err = PF_GetThisPage(AM_fd, leaf_node, &pagebuf)) < 0 )
		{
			AMerrno = err;
			return err;
		}

		while (((struct node_header *)pagebuf)->pageType == LEAF)
		{
			tmp_node = ((struct node_header *)pagebuf)->pageId;
			if( (err = PF_UnpinPage(AM_fd, leaf_node, FALSE)) < 0)
			{
				AMerrno = err;
				return err;
			}
			if ((err = PF_GetThisPage (AM_fd, leaf_node, &pagebuf) ) < 0 )
			{
				AMerrno = err;
				return err;
			}
			if ( (err= PF_UnpinPage (AM_fd, leaf_node, FALSE))<0)
			{
				AMerrno = err;
				return err;
			}
		}
		return leaf_node;
	}

static int compare(int op, void *value1, void *value2, char attrType, int attrLength)
{
    int res, com;
    switch (attrType)
    {
        case 'c':
            com = strncmp(value1, value2,  attrLength);
        case 'i':
            if (*(int *)value1 == *(int*)value2)        com = 0;
            else if (*(int *)value1 > *(int *)value2)   com = 1;
            if(*(int *)value1 < *(int *)value2)         com = -1;

        case 'f':
            if (*(float *)value1 == *(float *)value2)       com = 0;
            else if (*(float *)value1 > *(float *)value2)   com = 1;
            if(*(float *)value1 < *(float *)value2)         com = -1;
        
            break;
    }

    switch(op)
    {
        case LT_OP:
            res = (com < 0);    //low   not foubd
            break;
        case LE_OP:
            res = ( com < 0 || com == 0);
            break;
        case GT_OP:
            res = ( com > 0 || com == 0 );
            break;
        case GE_OP:
            res = (com > 0 || com ==0);
            break;
        case EQ_OP: 
            res = (com == 0);       //low found
            break;
		case NE_OP:
			res = (com != 0);
    }
    return res;

}

/* Insert a pair in the node pointed by node_block
** If the node was splitted, pagenum_right is set to point the newly created node which is on the right side, and the val will go upward */
static int insert_value_into_node(int AM_fd, int pagenum_left, void *value, int node, char attrType, int attrLength, int *pagenum_right, void *val)
{

    int err;
	int right_recId;
	int left_recId;
	int i, j, k; 		// for loops
	char *two_nodes;		//two nodes
	char *pagebuf;			//node
	char *new_node;

	*new_node = -1;
	int side;

	two_nodes = malloc(PAGE_SIZE * 2);
	if (!two_nodes) return AME_NOMEM;

	((struct two_nodes_header *)pagebuf)->num_records = 0; //set the new two nodes NULL

	if( (err = PF_GetThisPage(AM_fd, pagenum_left, &pagebuf )) != PFE_OK ) 	/* two nodes were created and filled with the pairs from pagebuf. Two nodes are = 0 both*/
	{
		AMerrno = err;
		free(two_nodes);
		return err;
	} 

	/*On each loop j is incremented while i is incremented only when an entry is added to the two nodes, otherwise skip it (in case invalid entry) */
	for( i = 0, j = 0; i <( ((struct node_header *)pagebuf)->numRecords ); j++)
		{
		//if(pointer_value(p,pagebuf, j, attrLength) < 0 ) continue;
		if(node_p(pagebuf, j, attrLength) < 0 ) continue;
		if( compare(LT_OP, value, node_v(pagebuf, i, attrLength), attrType, attrLength)  )  //To ccompare if the value 1 and value 2	going as leftmost */	  break;
		
		//set_two_p_v(pp_,two_nodes, i, pointer_value(v, pagebuf, j, attrLength), attrLength);							//copy a value to two nodes.
		//set_two_p_v(vv_,two_nodes, i++, pointer_value(p,pagebuf, j , attrLength), attrLength) = pointer_value(pagebuf, j , attrLength);						//copy a p to two nodes
		
			set_two_v(two_nodes, i, node_v(pagebuf, j, attrLength), attrLength);
			set_two_p(two_nodes, i++, node_p(pagebuf, j, attrLength), attrLength);


			//ADD COMPARISON FOR DUBLICATE KEYS??
			//if( compare(EQ_OP, value, node_v(pagebuf, i, attrLength), attrType, attrLength)  ) 
			// create pointer to  the record
		}

		/* Insert the new pairs */
		set_two_v(two_nodes, i, value, attrLength);
		set_two_p(two_nodes, i, node, attrLength);
		i++;

		/* insert  the rest of the entries from the primary node (pagebuf) */
		for( ; i < 1 + (((struct node_header *)pagebuf)->numRecords) ; j++ )
		{
			//if( point_value(p,pagebuf, j, attrLength)< 0) continue;
			if(node_p(pagebuf, j, attrLength) <0) continue;
			//memcpy(two_nodes, i, node_v(pagebuf, j, attrLength), attrLength);
			//two_p(two_nodes,  i++, node_p(pagebuf, j, attrLength), attrLength);
		
			set_two_v(two_nodes, i, node_v(pagebuf, j, attrLength), attrLength);
			set_two_p(two_nodes, i++, node_p(pagebuf, j, attrLength), attrLength);
		
		}

		/* if the block of two nodes can fit in a standard node, copy them to the primary node and return The i is a count of records (numRecords) */
		if (i <= num_records(attrLength) )
		{
			(((struct node_header *)pagebuf)->numRecords = i);
			/* Copy the pairs */
			memcpy(pagebuf + sizeof(struct node_header), two_nodes +sizeof( struct two_nodes_header), i * ( attrLength + sizeof( int)));

			free(two_nodes);
			return PF_UnpinPage(AM_fd,pagenum_left, TRUE);
		}

		//*** SPLITTING

		if ( (err = PF_AllocPage(AM_fd, pagenum_right, &new_node) !=PFE_OK))
		{
			AMerrno = err;
			free(two_nodes);
			PF_UnpinPage (AM_fd, pagenum_left, TRUE);
			return err;
		}

		memset( new_node, 0 , PAGE_SIZE);

		/* Split the two_nodes with i records*/
		/* where to split the two_nodes */
		if (i%2)
		{
			left_recId = i/ 2-1;
			right_recId = left_recId + 1;
		}
		else left_recId = right_recId = 5/2;//???

		side = -1; /* the side to which pointer should go. 1 for the right side and -1 for the left side */
		while (compare(EQ_OP, two_nodes_v(two_nodes, left_recId, attrLength), two_nodes_v(two_nodes, right_recId, attrLength), attrType, attrLength) )  //maybe add function - no split, just point to next record??
		{
			if(side == -1) left_recId --;
			else right_recId++;
		
			side*= -1; /* to change the side**/
		}

		/*  */
		side *= -1;
		if(side == -1) side = left_recId + 1;
		else side = right_recId ;

		/*i holds the num of records in the two_nodes*/
		(((struct node_header *)new_node)->numRecords = j);
		memcpy ( pagebuf + sizeof (struct node_header), two_nodes + sizeof(struct two_nodes_header), j*(attrLength + sizeof(int)));
		
			/**/
		for (k = (((struct node_header *)pagebuf)->numRecords);  k < num_records(attrLength);  k++) 
				node_p(pagebuf,k , attrLength) = -1;		

		/* Copy the type of node from previous to new node**/
		((struct node_header *)new_node)->pageType = ((struct node_header *)pagebuf)->pageType;		
		/* nsert  the num of records (entries) into the new node */
		(((struct node_header *)new_node)->numRecords = i - j);
		
		memcpy(new_node + sizeof( struct node_header), two_nodes + sizeof(struct two_nodes_header)+ j*( attrLength + sizeof(int)), (i-j)*(attrLength + sizeof(int)));
		/**/
		for (k = (((struct node_header *)new_node)->numRecords);  k < num_records(attrLength);  k++)
				node_p(new_node,k  , attrLength) = -1;

			
		//node_p(new_node, ((struct node_header *)pagebuf->numRecords), attrLength ) = -1;

		/* the value that should go upwards is I GUESS the first from the new node after the split */
		memcpy(val, node_v(new_node, 0, attrLength), attrLength);
		// value goes upward
		if( ((struct node_header *)new_node)->pageType == LEAF)
		{
			( (struct node_header *)new_node)->pageId = ((struct node_header *)pagebuf)->pageId;
			( (struct node_header *)pagebuf)->pageId = *new_node;
		}
		else	
			{
				( ((struct node_header *)new_node)->pageId) = (node_p(pagebuf, (((struct node_header *)pagebuf)->numRecords)-1, attrLength) );

				 /*set the last pair as invalid*/
				set_p(pagebuf, (((struct node_header *)pagebuf)->numRecords)-1,-1, attrLength);
				((struct node_header *)pagebuf)->numRecords = ( ((struct node_header *)pagebuf)->numRecords)-1 ;

			}

			/**DONE**/
				free(two_nodes);

				if ( err = PF_UnpinPage (AM_fd, pagenum_left, TRUE) != PFE_OK)
				{
					AMerrno = err;
					PF_UnpinPage( AM_fd, *new_node, TRUE);
					return err;

				}

				return PF_UnpinPage(AM_fd, *new_node, TRUE);
}

int find_leaf(int AM_fd,char *value, int attrLength, char attrType, struct leaf_nodes **leafs )
{
    int     err;       /* returns error value*/
    int     pageId;		//next page
	int 	pagenum;	//node block
	char 	*pagebuf;	//block
	int		i,j;			//for a loop
	
	struct leaf_nodes *leaf;
	
	*leafs = leaf = malloc(sizeof(struct leaf_nodes));
	if (!leaf) return AME_NOMEM;


	/*if ( /*???->root_block == NULL) //if root is empty
	/*{
		AMerrno = ROOTNULL;
		return ROOTNULL;
	}*/
	
	leaf->index = 0; //first leaf node in the array of leaf nodes

	if ( (err = pagenum = root_node(AM_fd)) != PFE_OK )
	{
		free(leaf);
		return err;
	}

    /* To get the root of the B+ tree */
    if ( (err = PF_GetThisPage(AM_fd, pagenum, &pagebuf)) != PFE_OK) //GetFirstPage??
	{
		AMerrno = err;
		free(leaf);
		return AME_PF;
	}

    while(1) //or **pagebuf != L
    {
        leaf->array[leaf->index++] = pagenum;

        if( (err = PF_GetThisPage(AM_fd, pagenum, &pagebuf)) != PFE_OK ) 
		{
			AMerrno = err;
			free(leaf);
			return err;
		}
		
		/* Check the page type, leaf or internal node */
		if( ((struct node_header*)pagebuf)->pageType == LEAF )
		{
		//	if ( (err = PF_GetThisPage(AM_fd, pagenum, &pagebuf) ) != PFE_OK )
			if(  (err = PF_UnpinPage(AM_fd, pagenum, FALSE)) !=PFE_OK)
			{
				AMerrno = err;
				free(leaf);
				return err;
			}

			leaf->pagenum = pagenum;
			break;
		}
		int num_records = ((struct node_header*)pagebuf)->numRecords;
		for ( i = 0, j = 0; j < num_records ; i++ )
		{
			if (node_p(pagebuf, i, attrLength) < 0)
			continue;

			if(compare( LT_OP, value, node_v(pagebuf, i, attrLength), attrType, attrLength)) break;	//weird stuff is here *attrType instead of attrType

			pagenum = node_p(pagebuf, i, attrLength);													//weid stuf is here *node_p instead of node_p
			j++;
		}
		if (j == 0) /*value is less than V0)*/ pagenum = next_page(pagebuf);

		if( ( err = PF_UnpinPage(AM_fd, leaf->array[leaf->index-1], FALSE)) != PFE_OK) 
		{
			AMerrno = err;
			free(leaf);
			return err;
		}
    }
	return AME_OK;
}

//--------------------------Initialize----------------------------------
void AM_Init(void)
{
	/* Initialize HF layer*/
 	// HF_Init();

 	for ( int i = 0; i < MAXOPENFILES; i++) //this function is needed??
	 {
		 AM_index_table[i].is_empty = 1;
		 AM_index_table[i].filename = NULL;
	 }

	for(int i = 0; i < MAXSCANS; i++)
	{
	 AM_scan_table[i].value = NULL;
	 AM_scan_table[i].attrType = FALSE;//??
	 AM_scan_table[i].attrLength = 0; //AM_INVALIDPARA;
  	 AM_scan_table[i].op = 0; // AM_INVALIDPARA;
 	 AM_scan_table[i].fd = 0; //AM_INVALIDPARA;

	AM_scan_table[i].is_empty = 1; 
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
	int 	records_in_node;	/*the number of records that are hold in the one page (node)  */
	char 	*index_filename; /* to store the indexed files name with extensions*/
	
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

	if( !(index_filename = filename_size(filename, indexNo)) )	return AMerrno;

//--------------------------------------------------------------------

	/* Get the fileName and create a paged file by its name*/
	sprintf(index_filename, "%s.%d", filename, indexNo);

//-----Create, Open, Allocate PF File------------------------------
	if ( (err = PF_CreateFile(index_filename) ) < 0 )
	{
		AMerrno = err;
		return err;
	}

	/* Open the file from PF layer */
	if ( ( AM_fd = err = PF_OpenFile(index_filename) ) < 0) 
	{
		AMerrno = err;
		return err;
	}

	/*Allocate pages for the root*/
	if ( (err = PF_AllocPage(AM_fd, &pagenum, &pagebuf) < 0 ) )
	{
		//PF_CloseFile(AM_fd);
		AMerrno = err;
		return err;
	}
//----------------------------------------------------------------------------------------------------
	//totalBlocks = 0; need?
	//records_in_node = (PAGE_SIZE - sizeof(struct node_header))/(attrLength + sizeof(RECID)); 		//check 
	records_in_node = num_records(attrLength); 
	/* Initialize */
	memset(pagebuf , 0 , PAGE_SIZE);
	((struct node_header *)pagebuf)->numRecords = records_in_node;
	((struct node_header *)pagebuf)->pageType= LEAF;
	((struct node_header *)pagebuf)->pageId = -1;	// next leaf page or the last pointer of an inner node

	//((typedef struct node_header *)pagebuf)->depth = 0;
	//((typedef struct node_header *)pagebuf)->totalBlock = 0;
	//((typedef struct node_header *)pagebuf)->recIdPtr = PAGE_SIZE;
	//((typedef struct node_header *)pagebuf)->keyPtr = sizeof(struct node_header);
	((struct node_header *)pagebuf)->attrType = sizeof(char);
	((struct node_header *)pagebuf)->attrLength = sizeof(int);
	
	//to update the fd and pagebuf are needed??? if yes, I should add codes here

	if ( (err = PF_UnpinPage( AM_fd, pagenum, TRUE) ) < 0 )
	{
		//PF_CloseFile(AM_fd);
		AMerrno = err;
		return err;
	} 
	
	empty_root_node(AM_fd,pagenum); //create empty root node
	if (empty_root_node == NULL) return AME_ROOTNULL;  //is it proper return parameter??

	/*	Close the file */
	if ( (err = PF_CloseFile(AM_fd) ) != PFE_OK)
	{
		AMerrno = err;
		return err;
	}

	//free(index_filename);
	return AME_OK;

}

/* Destroys the index fileName, indexNo */ 
int AM_DestroyIndex(char *filename, int indexNo)
{
	int err;
	char *index_filename;		//Pwe need max index name..strlen of file name is suitable??

	if( !(index_filename = filename_size(filename, indexNo)) )return AMerrno;
	//..check smth else..?

	sprintf(index_filename, "%s.%d", filename, indexNo);  //needed?
	
	if (( err = PF_DestroyFile(index_filename)) != PFE_OK)
	{
		AMerrno = err;
		return err;
	}

	//free(index_filename);
	return AME_OK;
}

int AM_OpenIndex(char *filename, int indexNo)
{
	int 	err;
	int 	AM_fd;
	int 	pagenum;
	char	*pagebuf;
	char	*index_filename;
	
	if( !(index_filename = filename_size(filename, indexNo)) ) return AMerrno;

	sprintf(index_filename, /*sizeof(index_filename),*/"%s.%d", filename, indexNo); //

	if ((AM_fd = PF_OpenFile(index_filename)) < 0)
	{
		AMerrno = AM_fd;
		return AM_fd; //or AME_FD?
	}


//-------------------CORRECT??------------------------------------
	/* Load the index of the filename*/
	if ( !(AM_index_table[AM_fd].filename == malloc (strlen (index_filename) + 1)) ){
		AMerrno = AME_NOMEM;
		return AME_NOMEM;
	}

	if (( AM_fd = PF_OpenFile(index_filename)) < 0 )
	{
		AMerrno = AM_fd;
		return AM_fd;
	}

	strcpy(AM_index_table[AM_fd].filename, index_filename);
	
	// get the first page into the AM descriptor from PF layer
	if ((err = PF_GetFirstPage(AM_fd, &pagenum, &pagebuf))!= PFE_OK)
	{
		AMerrno = err;
		free(index_filename);
		return err;
	}
	/* store the data in the open index structure */
	//memcpy(&AM_index_table[AM_fd].depth, pagebuf, sizeof(int));
	//TODO

	AM_index_table[AM_fd].is_empty = 0; //non empty

	if((err = PF_UnpinPage(AM_fd, pagenum, FALSE)) != PFE_OK) 
	{
		AMerrno = err;
		return err;
	}
//-----------------------------------------------------
	return AM_fd;	/* returns the value of file descriptior of B+ tree index - index into the index table */
}

int AM_CloseIndex(int AM_fd)
{
	int err;

		//Get first page
		//TODO
		//Unpin..needed???

	//close the index file with the indicated file descriptor by calling PF_CloseFile().
	//It deletes the entry for this index from the index table
	if((err = PF_CloseFile(AM_fd)) != PFE_OK)
	{
		AMerrno = err;
		return err;
	}
	/* release the memory position in which the index was */
	if( !AM_index_table[AM_fd].filename)
	{
		free(AM_index_table[AM_fd].filename);
		AM_index_table[AM_fd].filename = NULL;
	}
	AM_index_table[AM_fd].is_empty = 1; //empty
	return AME_OK;
}

int AM_InsertEntry(int AM_fd, char *value, /*RECID*/ int recId)
{
	//inserts (value, recId) pair into the index repretented by the open file with AM_fd
	//value parameter points to value to be inserted into the index (key)
	//recId parameter idenfities a record with that value to be added to the index
	//AME_OK
	//Otherwise AM error code
	int 	err;
	int 	pagenum;	/* the page number in the buffer */
	char 	*pagebuf;	/* buffer to hold a page */
	void 	*val;		/* the size of key value from attrLength*/

	int 	index;	 	/* the index where key can be inserted or found */	
	int 	is_empty;  /* key is in tree (old) ornot (new) */
	int 	left_recId;
	int 	right_recId;
	int 	i; 			//for a loop

	struct leaf_nodes *leaf;	/* an array of found leaf nodes */

	int attrLength;

	attrLength =( ((struct node_header*)pagebuf)->attrLength );  //check if it is ok (memory size without malloc)
	if (!attrLength) return AME_NOMEM;

//------Check AM_Fd. value---------------------------------------------------------------
	if (value == NULL)
	{
		AMerrno =  AME_INVALIDVALUE;
		return AME_INVALIDVALUE;
	}
	if (AM_fd < 0)
	{
		AMerrno = AME_FD;
		return AME_FD;
	}	
//--- Check Attributes-------------------------------------------------
	/*  Check the parameters */ //extract the attrType and length from where? AM_fd??
	/*if (( attrType != 'c') && (attrType != 'i') && (attrType != 'f'))
		{
			AMerrno = AME_INVALIDATTRTYPE;
			return(AME_INVALIDATTRTYPE);
		}*/
	if ((attrLength < 1) || (attrLength > 255))
		{
			AMerrno = AME_INVALIDATTRLENGTH;
			return(AME_INVALIDATTRLENGTH);
		}
	/*if (attrLength != 4 && (attrType != 'i' || attrType != 'f')) /* 4 for 'i' or 'f' */
	/*{
		AMerrno = AME_INVALIDATTRLENGTH;
		return(AME_INVALIDATTRLENGTH);
	}
	/*else			
		if (attrType != 'c' &&  (attrLength < 1 || attrLength != 4))	/* 1-255 for 'c' 	*/			
		/*{
			AMerrno = AME_INVALIDATTRLENGTH; 
			return(AME_INVALIDATTRLENGTH);
		}*/

//-----------FIND LEAF-------------------------------------
	/* To find the leaf node for the key */
	if ( (err = find_leaf(AM_fd, value,  ((struct node_header*)pagebuf)->attrLength,((struct node_header *)pagebuf)->attrType, &leaf)) != AME_OK) 
	{
		AMerrno = err;
		return err;
	}

//------------INSERT-----------------------------------------------------
	/* To insert a pair (key value and recID) into the leaf*/
//	
	val = malloc(((struct node_header*)pagebuf)->attrLength);
	if (!val) return AME_NOMEM;

	memcpy(val, value, ((struct node_header*)pagebuf)->attrLength);
	/*Copy the recId into the right node of the node*/
	right_recId = recId;

	for ( i = leaf->index-1; i >=0; i--)
	{
		left_recId = right_recId;
		if ((err = insert_value_into_node(AM_fd, leaf->array[i], val, left_recId, ((struct node_header *)pagebuf)->attrType,((struct node_header*)pagebuf)->attrLength, &right_recId, val)) != AME_OK)
		{
			AMerrno = err;
			free(val);
			free(leaf);
			return err;
		}
		
		/* Check if the node was splitted after as the value was inserted into node */
		if (right_recId < 0 ) break; //was not splitted // -1
	}
	/* Since the node was splitted, one node is in left_recId, another node-sibiling is in right_recId */
	left_recId = leaf->array[0];		//After the copy, it now looks like <left_recId, value, right_recId> 
	free(leaf);		

	if (!(right_recId == 0)) // -1
	{
		if(err = PF_AllocPage(AM_fd, &pagenum, &pagebuf) < 0 )	//Allocate the new node
		{
			AMerrno = err;
			free(val);
			return err;
		}
	}

	((struct node_header*)pagebuf)->numRecords = 1;
	memcpy( node_v(pagebuf, 0, attrLength), val, attrLength);
	((struct node_header *)pagebuf)->pageId = left_recId;
	((struct node_header *)pagebuf)->pageType = INTERNAL;

	if ( err = PF_UnpinPage(AM_fd, pagenum, TRUE) < 0)
	{
		AMerrno = err;
		free(val);
		return err;
	}

	if (err = upd_root_node(AM_fd, pagenum) < 0)		//upddate the node for the next execution
	{
		AMerrno = err;
		free(val);
		return err;
	}
	free(val);
	return AME_OK;
}
int AM_DeleteEntry(int AM_fd, char *value, /*RECID*/ int recId)
{
	//removes (value, recId) pair from the index represented by the open file associated with AM_fd
	//AME_OK
	//Otherwise AM code error
	char 	*pagebuf;		/* a page( node) */
	int 	pagenum;		/* the page number of the page (block)*/
	int 	index;			/* the index where key is present/ where is the current key*/
	int 	is_empty;
	int attrLength;
	char attrType;
	//int 	keysize;		/* the size of the record/key  / the length of key, ptr pair for a leaf */
	int 	err;		/* holds the return value of functions called within this function */
	attrLength = ((struct node_header *)pagebuf)->attrLength;
	attrType = (( struct node_header *)pagebuf)->attrType;
	struct leaf_nodes *leaf;
//------------Chech parameters, fd, value---------------------
	/** To check the parameters */
	/**if ( (attrType != 'i') && (attrType != 'f' ) && (attrType != 'c') )
	{
		AMerrno = AME_INVALIDVALUE;
		return (AME_INVALIDVALUE);
	}*/

	if (AM_fd > 0 )
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
	if ((err = find_leaf(AM_fd, value, attrType, attrLength, &leaf) ) != AME_OK)
		{
		AMerrno = err;
		return err;
	}

	/* If the key is not in the tree */
	if (err = AME_KEYNOTFOUND)
	{
		AMerrno = AME_KEYNOTFOUND;
		return AME_KEYNOTFOUND;
	}
	/* To check if it returns error value */
	if( (err = PF_GetThisPage(AM_fd, leaf->pagenum ,&pagebuf)) != PFE_OK)
		{
		AMerrno = err;
		return err;
	}

	/* To search the list for recId */
	for (  int i = 0; i < num_records(attrLength); i++) // while next record != 0 //scan table is wrong..ocrrect!
	{	
		/* to get the pointer and key value */
	if (node_p(pagebuf,i, attrLength)== -1) continue;

//check v and p parameters
	if (node_p(pagebuf, i, attrLength) == recId){
		if(compare(EQ_OP, value, node_v(pagebuf, i, attrLength), attrType, attrLength))
		{
			set_p(pagebuf, i, -1, attrLength);
			((struct node_header *) pagebuf)->numRecords = ((struct node_header *)pagebuf)->numRecords - 1;
			break;
		}
	}
	//TODO
	}
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
	free(leaf);
	return err;


}

int AM_OpenIndexScan(int AM_fd, int op, char *value)
{
	int i; 	/* for a loop */
	int err;
	struct leaf_nodes *leaf;
//-----Check FD, attribute, value --------------------------------------
	if (AM_fd < 0 )/*|| AM_fd > MAXOPENFILES ) /*|| !scan_table[AM_fd].is_empty) */ 
	{
		AMerrno = AME_FD;
		return AME_FD;
	}

	/**if( (AM_scan_table[AM_fd].attrType != 'i') && (AM_scan_table[AM_fd].attrType != 'c') && (AM_scan_table[AM_fd].attrType != 'f' ) )		//enough? or add more?
	{
		AMerrno = AME_INVALIDATTRTYPE;
		return AME_INVALIDATTRTYPE;
	}*/

	// to find a free place in the scan table
	for( i = 0; i < MAXSCANS; i++)				
		if(AM_scan_table[i].is_empty) break;	
	
	// if no a free place found, then the scan table is full 
	if ( i == MAXSCANS)
	{
		AMerrno =AME_SCANTABLEFULL;
		return AME_SCANTABLEFULL;					
	} 	
//------------------------------------------------------------------
	if( value != NULL)
		if( !(AM_scan_table[i].value = malloc(AM_scan_table[i].attrLength)) )
		{
			AMerrno = AME_NOMEM;
			return AME_NOMEM;
		}

	memcpy(AM_scan_table[i].value, value, AM_scan_table[i].attrLength);
	
	AM_scan_table[i].fd = AM_fd;				
	AM_scan_table[i].op = op;
	AM_scan_table[i].index = -1;		//last record or last bucket number 

	//AM_scan_table[i].attrType = ((struct node_header *)???)->attrType;
	//AM_scan_table[i].attrLength = ((struct node_header*)AM_fd)->attrLength;
	

	//open an index scan over the index
	// The scan descriptor returned is an index into the index scan table (in HF layer similar)
	// If the index scan table is full, AM error code is returned in place of a scan descriptor
	//the value parameter will point to a (binary) value that indexed attribute values are to be compared with
	//The scan will return the record ids of those records whose indexed attribute value matches the value parameter in the desired way

	//case on OP
	switch (op)	//needed?? TODO
	{
		case LT_OP:
		{
			/*AM_scan_table[i].pageId = leftmost_pagenum;
					AM_scan_table[i].next_index = 1;
					AM_scan_table[i].cur_index = 1;
					if( find_pagenum != leftmost_pagenum)
					{
						if ( (err = PF_GetThisPage(AM_fd, leftmost_pagenum, &pagebuf) )!=PFE_OK)
						AMerrno = err;
						return err;
					}*/
		}
		case LE_OP:
				{
				AM_scan_table[i].pageId = leaf_node(AM_fd, AM_scan_table[i].attrLength);
					//Value is not in the leaf , no match
					//if(err != AME_KEYNOTFOUND){}
					//AM_scan_table[i].is_empty = /*OVER*/;
					//else
					//{
					//	AM_scan_table[i].pageId = pagenum;
						//AM_scan_table[i].next_index = index;
						//bcopy(pagebuf + sizeof(node_header)+(index-1)*rec_size + attrLength);
						//AM_scan_table[i].cur_index = index;
						//AM_scan_table[i].prev_pagenum= pagenum;
						//AM_scan_table[i].prev_index = index;
					}
					break;
		default: 
		if ((err = find_leaf(AM_fd, value, AM_scan_table[i].attrType, AM_scan_table[i].attrLength, &leaf) ) < 0)
		{
			AMerrno = err;
			free(AM_scan_table[i].value);
			return err;
		}

		AM_scan_table[i].pageId = leaf->pagenum;
		free(leaf);
		break;
	}

		//case GT:
		//	break;
		//case LTQ:
		//	break;
		//case GTQ: 
			//break;
		//case NQ: 
			//break;
		//default: 
			//AMerrno = AME_INVALIDOP; //invalid op to scan
			//return AME_INVALIDOP;
			//break;
	
	//errVal = PF_UnpinPage(AM_fd, find_paagenum, FALSE);
	//AM_ErrorCheck;
	
	AM_scan_table[i].is_empty = 0;
	return i;		
}

/**RECID**/ int AM_FindNextEntry(int scanDesc)
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

	int attrLength;
	attrLength = ((struct node_header *)pagebuf)->attrLength;

	struct scan_entryindex *table_scan;
	
	//Check if a scan is valid
	if( (scanDesc < 0) || (scanDesc >= MAXSCANS) || (!AM_scan_table->is_empty) ) //check this condition!
	{
		AMerrno = AME_INVALIDSCANDESC;
		return AME_INVALIDSCANDESC;
	}
	
	// Check if scan over??
	//if(scan_entryindex[scNDesc].is_empty == OVER) return AME_EOF;
//-------------------------------------------------
	/* Get the page (block) to start the scanning */
	if( (err = PF_GetThisPage(AM_scan_table->fd, AM_scan_table->pageId, &pagebuf) != PFE_OK) )//delete??---------------------------------------------------------------------------
		{
			AMerrno = err;
			return err;
		}

//--------------------------------------------
		table_scan = &AM_scan_table[scanDesc];

	/* Start the search of the next record of that is needed */
	while(1)
	{
		if( (err = PF_GetThisPage(AM_scan_table->fd, AM_scan_table->pageId, &pagebuf) != PFE_OK) )
			{
				AMerrno = err;
				return err;
			}
	//...TODO
	//find an entry
	//compare
	//last node
	// unpin page?
		for ( table_scan->index < num_records(attrLength); table_scan->index++;)
		{
			if(node_p(pagebuf, table_scan->index, table_scan->attrLength) == -1) continue;

			if(compare(table_scan->op, table_scan->value, node_v(pagebuf, table_scan->index, table_scan->attrLength), table_scan->attrType, table_scan->attrLength)) break;


			//if equal..to do smth??
		}
	
		/* to check if op is not equal, then check if we have to skip this value */
		//if( AM_scan_table->op == NEQ)
			//...TODO
		/* if op is equal , then check if you are done */	
		//if( AM_scan_table->op == EQ)
			//..TODO
		/*to check if you are at the l
		ast record if io is < or <= */
		//if( AM_scan_table->op == LT || LTQ)
			//..TODOs
		//return recId;


	//-------------------------------------------------	

	/* the end of node (last node) */
		if(table_scan->index == (num_records(attrLength) )) //check!  index == a record is a pair <v, p>
		{
			next_leaf_page = ((struct node_header *)(pagebuf))->pageId;  // node get next block
			if( (err = PF_UnpinPage(AM_scan_table->fd, AM_scan_table->pageId, FALSE) != PFE_OK) )// unpin block
			{
				AMerrno = err;
				return err;
			}
		
			if (next_leaf_page == -1 )	return AME_EOF; //no next page

			AM_scan_table->pageId = next_leaf_page;
			AM_scan_table->index -1;
			continue;	
		}
		//TODO with a pair of <p , v> ???
		recId = node_p(pagebuf,AM_scan_table->index, AM_scan_table->attrLength);
		//recId = (*(int*)((char*)(pagebuf) + sizeof(struct node_header) + (index *(AM_scan_table[i].attrLength + sizeof(int) ) ) + (AM_scan_table->attrLength) ) ); //node gets pointer (recId)
		/* unpin the current page*/
		if( (err = PF_UnpinPage(AM_scan_table->fd, AM_scan_table->pageId, FALSE) != PFE_OK) )
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
	if( (scanDesc < 0) /*&&*/|| (scanDesc >= MAXSCANS) /*&&*/|| (!AM_scan_table[scanDesc].is_empty)) //check this condition!
	{
		AMerrno = AME_INVALIDSCANDESC;
		return AME_INVALIDSCANDESC;
	}
	AM_scan_table[scanDesc].is_empty = 1; //is_empty = 1 false, not empty

	free(AM_scan_table[scanDesc].value); 
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
//printf("%s.%s\n", errString, AM_errors[(AMerrno+1)* -1] );
}



