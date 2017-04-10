#include <sys/types.h>

#include "minirel.h"

typedef struct PFhdr_str {
    int    numpages;      /* number of pages in the file */
} PFhdr_str;

typedef struct PFftab_ele {
    bool_t    valid;       /* set to TRUE when a file is open. */
    ino_t     inode;       /* inode number of the file         */
    char      *fname;      /* file name                        */
    int       unixfd;      /* Unix file descriptor             */
    PFhdr_str hdr;         /* file header                      */
    short     hdrchanged;  /* TRUE if file header has changed  */
} PFftab_ele;
