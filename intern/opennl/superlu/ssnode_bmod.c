
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

void slsolve(int, int, float*, float*);
void smatvec(int, int, int, float*, float*, float*);

/*
 * Performs numeric block updates within the relaxed snode. 
 */
int
ssnode_bmod (
	    const int  jcol,	  /* in */
	    const int  fsupc,     /* in */
	    float     *dense,    /* in */
	    float     *tempv,    /* working array */
	    GlobalLU_t *Glu,      /* modified */
	    SuperLUStat_t *stat   /* output */
	    )
{
#ifdef USE_VENDOR_BLAS
#ifdef _CRAY
    _fcd ftcs1 = _cptofcd("L", strlen("L")),
	 ftcs2 = _cptofcd("N", strlen("N")),
	 ftcs3 = _cptofcd("U", strlen("U"));
#endif
    int            incx = 1, incy = 1;
    float         alpha = -1.0, beta = 1.0;
#endif

    int            luptr, nsupc, nsupr, nrow;
    int            isub, irow, i, iptr; 
    register int   ufirst, nextlu;
    int            *lsub, *xlsub;
    float         *lusup;
    int            *xlusup;
    flops_t *ops = stat->ops;

    lsub    = Glu->lsub;
    xlsub   = Glu->xlsub;
    lusup   = Glu->lusup;
    xlusup  = Glu->xlusup;

    nextlu = xlusup[jcol];
    
    /*
     *	Process the supernodal portion of L\U[*,j]
     */
    for (isub = xlsub[fsupc]; isub < xlsub[fsupc+1]; isub++) {
  	irow = lsub[isub];
	lusup[nextlu] = dense[irow];
	dense[irow] = 0;
	++nextlu;
    }

    xlusup[jcol + 1] = nextlu;	/* Initialize xlusup for next column */
    
    if ( fsupc < jcol ) {

	luptr = xlusup[fsupc];
	nsupr = xlsub[fsupc+1] - xlsub[fsupc];
	nsupc = jcol - fsupc;	/* Excluding jcol */
	ufirst = xlusup[jcol];	/* Points to the beginning of column
				   jcol in supernode L\U(jsupno). */
	nrow = nsupr - nsupc;

	ops[TRSV] += nsupc * (nsupc - 1);
	ops[GEMV] += 2 * nrow * nsupc;

#ifdef USE_VENDOR_BLAS
#ifdef _CRAY
	STRSV( ftcs1, ftcs2, ftcs3, &nsupc, &lusup[luptr], &nsupr, 
	      &lusup[ufirst], &incx );
	SGEMV( ftcs2, &nrow, &nsupc, &alpha, &lusup[luptr+nsupc], &nsupr, 
		&lusup[ufirst], &incx, &beta, &lusup[ufirst+nsupc], &incy );
#else
	strsv_( "L", "N", "U", &nsupc, &lusup[luptr], &nsupr, 
	      &lusup[ufirst], &incx );
	sgemv_( "N", &nrow, &nsupc, &alpha, &lusup[luptr+nsupc], &nsupr, 
		&lusup[ufirst], &incx, &beta, &lusup[ufirst+nsupc], &incy );
#endif
#else
	slsolve ( nsupr, nsupc, &lusup[luptr], &lusup[ufirst] );
	smatvec ( nsupr, nrow, nsupc, &lusup[luptr+nsupc], 
			&lusup[ufirst], &tempv[0] );

        /* Scatter tempv[*] into lusup[*] */
	iptr = ufirst + nsupc;
	for (i = 0; i < nrow; i++) {
	    lusup[iptr++] -= tempv[i];
	    tempv[i] = 0.0;
	}
#endif

    }

    return 0;
}
