

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

/* What type of supernodes we want */
#define T2_SUPER

int
scolumn_dfs(
	   const int  m,         /* in - number of rows in the matrix */
	   const int  jcol,      /* in */
	   int        *perm_r,   /* in */
	   int        *nseg,     /* modified - with new segments appended */
	   int        *lsub_col, /* in - defines the RHS vector to start the dfs */
	   int        *segrep,   /* modified - with new segments appended */
	   int        *repfnz,   /* modified */
	   int        *xprune,   /* modified */
	   int        *marker,   /* modified */
	   int        *parent,	 /* working array */
	   int        *xplore,   /* working array */
	   GlobalLU_t *Glu       /* modified */
	   )
{
/* 
 * Purpose
 * =======
 *   "column_dfs" performs a symbolic factorization on column jcol, and
 *   decide the supernode boundary.
 *
 *   This routine does not use numeric values, but only use the RHS 
 *   row indices to start the dfs.
 *
 *   A supernode representative is the last column of a supernode.
 *   The nonzeros in U[*,j] are segments that end at supernodal
 *   representatives. The routine returns a list of such supernodal 
 *   representatives in topological order of the dfs that generates them.
 *   The location of the first nonzero in each such supernodal segment
 *   (supernodal entry location) is also returned.
 *
 * Local parameters
 * ================
 *   nseg: no of segments in current U[*,j]
 *   jsuper: jsuper=EMPTY if column j does not belong to the same
 *	supernode as j-1. Otherwise, jsuper=nsuper.
 *
 *   marker2: A-row --> A-row/col (0/1)
 *   repfnz: SuperA-col --> PA-row
 *   parent: SuperA-col --> SuperA-col
 *   xplore: SuperA-col --> index to L-structure
 *
 * Return value
 * ============
 *     0  success;
 *   > 0  number of bytes allocated when run out of space.
 *
 */
    int     jcolp1, jcolm1, jsuper, nsuper, nextl;
    int     k, krep, krow, kmark, kperm;
    int     *marker2;           /* Used for small panel LU */
    int	    fsupc;		/* First column of a snode */
    int     myfnz;		/* First nonz column of a U-segment */
    int	    chperm, chmark, chrep, kchild;
    int     xdfs, maxdfs, kpar, oldrep;
    int     jptr, jm1ptr;
    int     ito, ifrom, istop;	/* Used to compress row subscripts */
    int     mem_error;
    int     *xsup, *supno, *lsub, *xlsub;
    int     nzlmax;
    static  int  first = 1, maxsuper;
    
    xsup    = Glu->xsup;
    supno   = Glu->supno;
    lsub    = Glu->lsub;
    xlsub   = Glu->xlsub;
    nzlmax  = Glu->nzlmax;

    if ( first ) {
	maxsuper = sp_ienv(3);
	first = 0;
    }
    jcolp1  = jcol + 1;
    jcolm1  = jcol - 1;
    nsuper  = supno[jcol];
    jsuper  = nsuper;
    nextl   = xlsub[jcol];
    marker2 = &marker[2*m];


    /* For each nonzero in A[*,jcol] do dfs */
    for (k = 0; lsub_col[k] != EMPTY; k++) {

	krow = lsub_col[k];
    	lsub_col[k] = EMPTY;
	kmark = marker2[krow];    	

	/* krow was visited before, go to the next nonz */
        if ( kmark == jcol ) continue; 

	/* For each unmarked nbr krow of jcol
	 *	krow is in L: place it in structure of L[*,jcol]
	 */
	marker2[krow] = jcol;
	kperm = perm_r[krow];

   	if ( kperm == EMPTY ) {
	    lsub[nextl++] = krow; 	/* krow is indexed into A */
	    if ( nextl >= nzlmax ) {
		if ((mem_error = sLUMemXpand(jcol, nextl, LSUB, &nzlmax, Glu)))
		    return (mem_error);
		lsub = Glu->lsub;
	    }
            if ( kmark != jcolm1 ) jsuper = EMPTY;/* Row index subset testing */
  	} else {
	    /*	krow is in U: if its supernode-rep krep
	     *	has been explored, update repfnz[*]
	     */
	    krep = xsup[supno[kperm]+1] - 1;
	    myfnz = repfnz[krep];

	    if ( myfnz != EMPTY ) {	/* Visited before */
	    	if ( myfnz > kperm ) repfnz[krep] = kperm;
		/* continue; */
	    }
	    else {
		/* Otherwise, perform dfs starting at krep */
		oldrep = EMPTY;
	 	parent[krep] = oldrep;
	  	repfnz[krep] = kperm;
		xdfs = xlsub[krep];
	  	maxdfs = xprune[krep];

		do {
		    /* 
		     * For each unmarked kchild of krep 
		     */
		    while ( xdfs < maxdfs ) {

		   	kchild = lsub[xdfs];
			xdfs++;
		  	chmark = marker2[kchild];

		   	if ( chmark != jcol ) { /* Not reached yet */
		   	    marker2[kchild] = jcol;
		   	    chperm = perm_r[kchild];

		   	    /* Case kchild is in L: place it in L[*,k] */
		   	    if ( chperm == EMPTY ) {
			    	lsub[nextl++] = kchild;
				if ( nextl >= nzlmax ) {
				    if ((mem_error =
					 sLUMemXpand(jcol,nextl,LSUB,&nzlmax,Glu)))
					return (mem_error);
				    lsub = Glu->lsub;
				}
				if ( chmark != jcolm1 ) jsuper = EMPTY;
			    } else {
		    	    	/* Case kchild is in U: 
				 *   chrep = its supernode-rep. If its rep has 
			         *   been explored, update its repfnz[*]
			         */
		   	    	chrep = xsup[supno[chperm]+1] - 1;
		   		myfnz = repfnz[chrep];
		   		if ( myfnz != EMPTY ) { /* Visited before */
				    if ( myfnz > chperm )
     				  	repfnz[chrep] = chperm;
				} else {
		        	    /* Continue dfs at super-rep of kchild */
		   		    xplore[krep] = xdfs;	
		   		    oldrep = krep;
		   		    krep = chrep; /* Go deeper down G(L^t) */
				    parent[krep] = oldrep;
		    		    repfnz[krep] = chperm;
		   		    xdfs = xlsub[krep];     
				    maxdfs = xprune[krep];
				} /* else */

			   } /* else */

			} /* if */

		    } /* while */

		    /* krow has no more unexplored nbrs;
	   	     *    place supernode-rep krep in postorder DFS.
	   	     *    backtrack dfs to its parent
		     */
		    segrep[*nseg] = krep;
		    ++(*nseg);
		    kpar = parent[krep]; /* Pop from stack, mimic recursion */
		    if ( kpar == EMPTY ) break; /* dfs done */
		    krep = kpar;
		    xdfs = xplore[krep];
		    maxdfs = xprune[krep];

		} while ( kpar != EMPTY ); 	/* Until empty stack */

	    } /* else */

	} /* else */

    } /* for each nonzero ... */

    /* Check to see if j belongs in the same supernode as j-1 */
    if ( jcol == 0 ) { /* Do nothing for column 0 */
	nsuper = supno[0] = 0;
    } else {
   	fsupc = xsup[nsuper];
	jptr = xlsub[jcol];	/* Not compressed yet */
	jm1ptr = xlsub[jcolm1];

#ifdef T2_SUPER
	if ( (nextl-jptr != jptr-jm1ptr-1) ) jsuper = EMPTY;
#endif
	/* Make sure the number of columns in a supernode doesn't
	   exceed threshold. */
	if ( jcol - fsupc >= maxsuper ) jsuper = EMPTY;

	/* If jcol starts a new supernode, reclaim storage space in
	 * lsub from the previous supernode. Note we only store
	 * the subscript set of the first and last columns of
   	 * a supernode. (first for num values, last for pruning)
	 */
	if ( jsuper == EMPTY ) {	/* starts a new supernode */
	    if ( (fsupc < jcolm1-1) ) {	/* >= 3 columns in nsuper */
#ifdef CHK_COMPRESS
		printf("  Compress lsub[] at super %d-%d\n", fsupc, jcolm1);
#endif
	        ito = xlsub[fsupc+1];
		xlsub[jcolm1] = ito;
		istop = ito + jptr - jm1ptr;
		xprune[jcolm1] = istop; /* Initialize xprune[jcol-1] */
		xlsub[jcol] = istop;
		for (ifrom = jm1ptr; ifrom < nextl; ++ifrom, ++ito)
		    lsub[ito] = lsub[ifrom];
		nextl = ito;            /* = istop + length(jcol) */
	    }
	    nsuper++;
	    supno[jcol] = nsuper;
	} /* if a new supernode */

    }	/* else: jcol > 0 */ 
    
    /* Tidy up the pointers before exit */
    xsup[nsuper+1] = jcolp1;
    supno[jcolp1]  = nsuper;
    xprune[jcol]   = nextl;	/* Initialize upper bound for pruning */
    xlsub[jcolp1]  = nextl;

    return 0;
}
