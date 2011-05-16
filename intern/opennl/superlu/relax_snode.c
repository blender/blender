/** \file opennl/superlu/relax_snode.c
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

void
relax_snode (
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
    register int j, parent;
    register int snode_start;	/* beginning of a snode */
    
    ifill (relax_end, n, EMPTY);
    for (j = 0; j < n; j++) descendants[j] = 0;

    /* Compute the number of descendants of each node in the etree */
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
	/* Found a supernode with j being the last column. */
	relax_end[snode_start] = j;		/* Last column is recorded */
	j++;
	/* Search for a new leaf */
	while ( descendants[j] != 0 && j < n ) j++;
    }

    /*printf("No of relaxed snodes: %d; relaxed columns: %d\n", 
		nsuper, no_relaxed_col); */
}
