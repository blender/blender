

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
scopy_to_ucol(
	      int        jcol,	  /* in */
	      int        nseg,	  /* in */
	      int        *segrep,  /* in */
	      int        *repfnz,  /* in */
	      int        *perm_r,  /* in */
	      float     *dense,   /* modified - reset to zero on return */
	      GlobalLU_t *Glu      /* modified */
	      )
{
/* 
 * Gather from SPA dense[*] to global ucol[*].
 */
    int ksub, krep, ksupno;
    int i, k, kfnz, segsze;
    int fsupc, isub, irow;
    int jsupno, nextu;
    int new_next, mem_error;
    int       *xsup, *supno;
    int       *lsub, *xlsub;
    float    *ucol;
    int       *usub, *xusub;
    int       nzumax;

    float zero = 0.0;

    xsup    = Glu->xsup;
    supno   = Glu->supno;
    lsub    = Glu->lsub;
    xlsub   = Glu->xlsub;
    ucol    = Glu->ucol;
    usub    = Glu->usub;
    xusub   = Glu->xusub;
    nzumax  = Glu->nzumax;
    
    jsupno = supno[jcol];
    nextu  = xusub[jcol];
    k = nseg - 1;
    for (ksub = 0; ksub < nseg; ksub++) {
	krep = segrep[k--];
	ksupno = supno[krep];

	if ( ksupno != jsupno ) { /* Should go into ucol[] */
	    kfnz = repfnz[krep];
	    if ( kfnz != EMPTY ) {	/* Nonzero U-segment */

	    	fsupc = xsup[ksupno];
	        isub = xlsub[fsupc] + kfnz - fsupc;
	        segsze = krep - kfnz + 1;

		new_next = nextu + segsze;
		while ( new_next > nzumax ) {
		    if ((mem_error = sLUMemXpand(jcol, nextu, UCOL, &nzumax, Glu)))
			return (mem_error);
		    ucol = Glu->ucol;
		    if ((mem_error = sLUMemXpand(jcol, nextu, USUB, &nzumax, Glu)))
			return (mem_error);
		    usub = Glu->usub;
		    lsub = Glu->lsub;
		}
		
		for (i = 0; i < segsze; i++) {
		    irow = lsub[isub];
		    usub[nextu] = perm_r[irow];
		    ucol[nextu] = dense[irow];
		    dense[irow] = zero;
		    nextu++;
		    isub++;
		} 

	    }

	}

    } /* for each segment... */

    xusub[jcol + 1] = nextu;      /* Close U[*,jcol] */
    return 0;
}
