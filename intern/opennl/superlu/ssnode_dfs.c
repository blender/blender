/** \file opennl/superlu/ssnode_dfs.c
 *  \ingroup opennl
 */


/*
 * -- SuperLU routine (version 2.0) --
 * Univ. of California Berkeley, Xerox Palo Alto Research Center,
 * and Lawrence Berkeley National Lab.
 * November 15, 1997
 *
 */
/*
  Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 
  THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY
  EXPRESSED OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 
  Permission is hereby granted to use or copy this program for any
  purpose, provided the above notices are retained on all copies.
  Permission to modify the code and to distribute modified code is
  granted, provided the above notices are retained, and a notice that
  the code was modified is included with the above copyright notice.
*/

#include "ssp_defs.h"
#include "util.h"

int
ssnode_dfs (
	   const int  jcol,	    /* in - start of the supernode */
	   const int  kcol, 	    /* in - end of the supernode */
	   const int  *asub,        /* in */
	   const int  *xa_begin,    /* in */
	   const int  *xa_end,      /* in */
	   int        *xprune,      /* out */
	   int        *marker,      /* modified */
	   GlobalLU_t *Glu          /* modified */
	   )
{
/* Purpose
 * =======
 *    ssnode_dfs() - Determine the union of the row structures of those 
 *    columns within the relaxed snode.
 *    Note: The relaxed snodes are leaves of the supernodal etree, therefore, 
 *    the portion outside the rectangular supernode must be zero.
 *
 * Return value
 * ============
 *     0   success;
 *    >0   number of bytes allocated when run out of memory.
 *
 */
    register int i, k, ifrom, ito, nextl, new_next;
    int          nsuper, krow, kmark, mem_error;
    int          *xsup, *supno;
    int          *lsub, *xlsub;
    int          nzlmax;
    
    xsup    = Glu->xsup;
    supno   = Glu->supno;
    lsub    = Glu->lsub;
    xlsub   = Glu->xlsub;
    nzlmax  = Glu->nzlmax;

    nsuper = ++supno[jcol];	/* Next available supernode number */
    nextl = xlsub[jcol];

    for (i = jcol; i <= kcol; i++) {
	/* For each nonzero in A[*,i] */
	for (k = xa_begin[i]; k < xa_end[i]; k++) {	
	    krow = asub[k];
	    kmark = marker[krow];
	    if ( kmark != kcol ) { /* First time visit krow */
		marker[krow] = kcol;
		lsub[nextl++] = krow;
		if ( nextl >= nzlmax ) {
		    if ( (mem_error = sLUMemXpand(jcol, nextl, LSUB, &nzlmax, Glu)) )
			return (mem_error);
		    lsub = Glu->lsub;
		}
	    }
    	}
	supno[i] = nsuper;
    }

    /* Supernode > 1, then make a copy of the subscripts for pruning */
    if ( jcol < kcol ) {
	new_next = nextl + (nextl - xlsub[jcol]);
	while ( new_next > nzlmax ) {
	    if ( (mem_error = sLUMemXpand(jcol, nextl, LSUB, &nzlmax, Glu)) )
		return (mem_error);
	    lsub = Glu->lsub;
	}
	ito = nextl;
	for (ifrom = xlsub[jcol]; ifrom < nextl; )
	    lsub[ito++] = lsub[ifrom++];	
        for (i = jcol+1; i <= kcol; i++) xlsub[i] = nextl;
	nextl = ito;
    }

    xsup[nsuper+1] = kcol + 1;
    supno[kcol+1]  = nsuper;
    xprune[kcol]   = nextl;
    xlsub[kcol+1]  = nextl;

    return 0;
}

