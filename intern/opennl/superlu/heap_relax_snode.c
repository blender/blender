/** \file opennl/superlu/heap_relax_snode.c
 *  \ingroup opennl
 */
/*
 * -- SuperLU routine (version 3.0) --
 * Univ. of California Berkeley, Xerox Palo Alto Research Center,
 * and Lawrence Berkeley National Lab.
 * October 15, 2003
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

void
heap_relax_snode (
	     const     int n,
	     int       *et,           /* column elimination tree */
	     const int relax_columns, /* max no of columns allowed in a
					 relaxed snode */
	     int       *descendants,  /* no of descendants of each node
					 in the etree */
	     int       *relax_end     /* last column in a supernode */
	     )
{
/*
 * Purpose
 * =======
 *    relax_snode() - Identify the initial relaxed supernodes, assuming that 
 *    the matrix has been reordered according to the postorder of the etree.
 *
 */ 
    register int i, j, k, l, parent;
    register int snode_start;	/* beginning of a snode */
    int *et_save, *post, *inv_post, *iwork;
    int nsuper_et = 0, nsuper_et_post = 0;

    /* The etree may not be postordered, but is heap ordered. */

    iwork = (int*) intMalloc(3*n+2); 
    if ( !iwork ) ABORT("SUPERLU_MALLOC fails for iwork[]");
    inv_post = iwork + n+1;
    et_save = inv_post + n+1;

    /* Post order etree */
    post = (int *) TreePostorder(n, et);
    for (i = 0; i < n+1; ++i) inv_post[post[i]] = i;

    /* Renumber etree in postorder */
    for (i = 0; i < n; ++i) {
        iwork[post[i]] = post[et[i]];
	et_save[i] = et[i]; /* Save the original etree */
    }
    for (i = 0; i < n; ++i) et[i] = iwork[i];

    /* Compute the number of descendants of each node in the etree */
    ifill (relax_end, n, EMPTY);
    for (j = 0; j < n; j++) descendants[j] = 0;
    for (j = 0; j < n; j++) {
	parent = et[j];
	if ( parent != n )  /* not the dummy root */
	    descendants[parent] += descendants[j] + 1;
    }

    /* Identify the relaxed supernodes by postorder traversal of the etree. */
    for (j = 0; j < n; ) { 
     	parent = et[j];
        snode_start = j;
 	while ( parent != n && descendants[parent] < relax_columns ) {
	    j = parent;
	    parent = et[j];
	}
	/* Found a supernode in postordered etree; j is the last column. */
	++nsuper_et_post;
	k = n;
	for (i = snode_start; i <= j; ++i)
	    k = SUPERLU_MIN(k, inv_post[i]);
	l = inv_post[j];
	if ( (l - k) == (j - snode_start) ) {
	    /* It's also a supernode in the original etree */
	    relax_end[k] = l;		/* Last column is recorded */
	    ++nsuper_et;
	} else {
	    for (i = snode_start; i <= j; ++i) {
	        l = inv_post[i];
	        if ( descendants[i] == 0 ) relax_end[l] = l;
	    }
	}
	j++;
	/* Search for a new leaf */
	while ( j < n && descendants[j] != 0 ) j++;
    }

#if ( PRNTlevel>=1 )
    printf(".. heap_snode_relax:\n"
	   "\tNo of relaxed snodes in postordered etree:\t%d\n"
	   "\tNo of relaxed snodes in original etree:\t%d\n",
	   nsuper_et_post, nsuper_et);
#endif

    /* Recover the original etree */
    for (i = 0; i < n; ++i) et[i] = et_save[i];

    SUPERLU_FREE(post);
    SUPERLU_FREE(iwork);
}


