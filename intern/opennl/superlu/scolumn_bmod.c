/** \file opennl/superlu/scolumn_bmod.c
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

#include <stdio.h>
#include <stdlib.h>
#include "ssp_defs.h"

/* 
 * Function prototypes 
 */
void susolve(int, int, float*, float*);
void slsolve(int, int, float*, float*);
void smatvec(int, int, int, float*, float*, float*);



/* Return value:   0 - successful return
 *               > 0 - number of bytes allocated when run out of space
 */
int
scolumn_bmod (
	     const int  jcol,	  /* in */
	     const int  nseg,	  /* in */
	     float     *dense,	  /* in */
	     float     *tempv,	  /* working array */
	     int        *segrep,  /* in */
	     int        *repfnz,  /* in */
	     int        fpanelc,  /* in -- first column in the current panel */
	     GlobalLU_t *Glu,     /* modified */
	     SuperLUStat_t *stat  /* output */
	     )
{
/*
 * Purpose:
 * ========
 *    Performs numeric block updates (sup-col) in topological order.
 *    It features: col-col, 2cols-col, 3cols-col, and sup-col updates.
 *    Special processing on the supernodal portion of L\U[*,j]
 *
 */
#ifdef _CRAY
    _fcd ftcs1 = _cptofcd("L", strlen("L")),
         ftcs2 = _cptofcd("N", strlen("N")),
         ftcs3 = _cptofcd("U", strlen("U"));
#endif

#ifdef USE_VENDOR_BLAS
    int         incx = 1, incy = 1;
    float      alpha, beta;
#endif
    
    /* krep = representative of current k-th supernode
     * fsupc = first supernodal column
     * nsupc = no of columns in supernode
     * nsupr = no of rows in supernode (used as leading dimension)
     * luptr = location of supernodal LU-block in storage
     * kfnz = first nonz in the k-th supernodal segment
     * no_zeros = no of leading zeros in a supernodal U-segment
     */
    float       ukj, ukj1, ukj2;
    int          luptr, luptr1, luptr2;
    int          fsupc, nsupc, nsupr, segsze;
    int          nrow;	  /* No of rows in the matrix of matrix-vector */
    int          jcolp1, jsupno, k, ksub, krep, krep_ind, ksupno;
    register int lptr, kfnz, isub, irow, i;
    register int no_zeros, new_next; 
    int          ufirst, nextlu;
    int          fst_col; /* First column within small LU update */
    int          d_fsupc; /* Distance between the first column of the current
			     panel and the first column of the current snode. */
    int          *xsup, *supno;
    int          *lsub, *xlsub;
    float       *lusup;
    int          *xlusup;
    int          nzlumax;
    float       *tempv1;
    float      zero = 0.0;
#ifdef USE_VENDOR_BLAS
    float      one = 1.0;
    float      none = -1.0;
#endif
    int          mem_error;
    flops_t      *ops = stat->ops;

    xsup    = Glu->xsup;
    supno   = Glu->supno;
    lsub    = Glu->lsub;
    xlsub   = Glu->xlsub;
    lusup   = Glu->lusup;
    xlusup  = Glu->xlusup;
    nzlumax = Glu->nzlumax;
    jcolp1 = jcol + 1;
    jsupno = supno[jcol];
    
    /* 
     * For each nonz supernode segment of U[*,j] in topological order 
     */
    k = nseg - 1;
    for (ksub = 0; ksub < nseg; ksub++) {

	krep = segrep[k];
	k--;
	ksupno = supno[krep];
	if ( jsupno != ksupno ) { /* Outside the rectangular supernode */

	    fsupc = xsup[ksupno];
	    fst_col = SUPERLU_MAX ( fsupc, fpanelc );

  	    /* Distance from the current supernode to the current panel; 
	       d_fsupc=0 if fsupc > fpanelc. */
  	    d_fsupc = fst_col - fsupc; 

	    luptr = xlusup[fst_col] + d_fsupc;
	    lptr = xlsub[fsupc] + d_fsupc;

	    kfnz = repfnz[krep];
	    kfnz = SUPERLU_MAX ( kfnz, fpanelc );

	    segsze = krep - kfnz + 1;
	    nsupc = krep - fst_col + 1;
	    nsupr = xlsub[fsupc+1] - xlsub[fsupc];	/* Leading dimension */
	    nrow = nsupr - d_fsupc - nsupc;
	    krep_ind = lptr + nsupc - 1;

	    ops[TRSV] += segsze * (segsze - 1);
	    ops[GEMV] += 2 * nrow * segsze;


	    /* 
	     * Case 1: Update U-segment of size 1 -- col-col update 
	     */
	    if ( segsze == 1 ) {
	  	ukj = dense[lsub[krep_ind]];
		luptr += nsupr*(nsupc-1) + nsupc;

		for (i = lptr + nsupc; i < xlsub[fsupc+1]; ++i) {
		    irow = lsub[i];
		    dense[irow] -=  ukj*lusup[luptr];
		    luptr++;
		}

	    } else if ( segsze <= 3 ) {
		ukj = dense[lsub[krep_ind]];
		luptr += nsupr*(nsupc-1) + nsupc-1;
		ukj1 = dense[lsub[krep_ind - 1]];
		luptr1 = luptr - nsupr;

		if ( segsze == 2 ) { /* Case 2: 2cols-col update */
		    ukj -= ukj1 * lusup[luptr1];
		    dense[lsub[krep_ind]] = ukj;
		    for (i = lptr + nsupc; i < xlsub[fsupc+1]; ++i) {
		    	irow = lsub[i];
		    	luptr++;
		    	luptr1++;
		    	dense[irow] -= ( ukj*lusup[luptr]
					+ ukj1*lusup[luptr1] );
		    }
		} else { /* Case 3: 3cols-col update */
		    ukj2 = dense[lsub[krep_ind - 2]];
		    luptr2 = luptr1 - nsupr;
		    ukj1 -= ukj2 * lusup[luptr2-1];
		    ukj = ukj - ukj1*lusup[luptr1] - ukj2*lusup[luptr2];
		    dense[lsub[krep_ind]] = ukj;
		    dense[lsub[krep_ind-1]] = ukj1;
		    for (i = lptr + nsupc; i < xlsub[fsupc+1]; ++i) {
		    	irow = lsub[i];
		    	luptr++;
		    	luptr1++;
			luptr2++;
		    	dense[irow] -= ( ukj*lusup[luptr]
			     + ukj1*lusup[luptr1] + ukj2*lusup[luptr2] );
		    }
		}



	    } else {
	  	/*
		 * Case: sup-col update
		 * Perform a triangular solve and block update,
		 * then scatter the result of sup-col update to dense
		 */

		no_zeros = kfnz - fst_col;

	        /* Copy U[*,j] segment from dense[*] to tempv[*] */
	        isub = lptr + no_zeros;
	        for (i = 0; i < segsze; i++) {
	  	    irow = lsub[isub];
		    tempv[i] = dense[irow];
		    ++isub; 
	        }

	        /* Dense triangular solve -- start effective triangle */
		luptr += nsupr * no_zeros + no_zeros; 
		
#ifdef USE_VENDOR_BLAS
#ifdef _CRAY
		STRSV( ftcs1, ftcs2, ftcs3, &segsze, &lusup[luptr], 
		       &nsupr, tempv, &incx );
#else		
		strsv_( "L", "N", "U", &segsze, &lusup[luptr], 
		       &nsupr, tempv, &incx );
#endif		
 		luptr += segsze;  /* Dense matrix-vector */
		tempv1 = &tempv[segsze];
                alpha = one;
                beta = zero;
#ifdef _CRAY
		SGEMV( ftcs2, &nrow, &segsze, &alpha, &lusup[luptr], 
		       &nsupr, tempv, &incx, &beta, tempv1, &incy );
#else
		sgemv_( "N", &nrow, &segsze, &alpha, &lusup[luptr], 
		       &nsupr, tempv, &incx, &beta, tempv1, &incy );
#endif
#else
		slsolve ( nsupr, segsze, &lusup[luptr], tempv );

 		luptr += segsze;  /* Dense matrix-vector */
		tempv1 = &tempv[segsze];
		smatvec (nsupr, nrow , segsze, &lusup[luptr], tempv, tempv1);
#endif
		
		
                /* Scatter tempv[] into SPA dense[] as a temporary storage */
                isub = lptr + no_zeros;
                for (i = 0; i < segsze; i++) {
                    irow = lsub[isub];
                    dense[irow] = tempv[i];
                    tempv[i] = zero;
                    ++isub;
                }

		/* Scatter tempv1[] into SPA dense[] */
		for (i = 0; i < nrow; i++) {
		    irow = lsub[isub];
		    dense[irow] -= tempv1[i];
		    tempv1[i] = zero;
		    ++isub;
		}
	    }
	    
	} /* if jsupno ... */

    } /* for each segment... */

    /*
     *	Process the supernodal portion of L\U[*,j]
     */
    nextlu = xlusup[jcol];
    fsupc = xsup[jsupno];

    /* Copy the SPA dense into L\U[*,j] */
    new_next = nextlu + xlsub[fsupc+1] - xlsub[fsupc];
    while ( new_next > nzlumax ) {
	if ((mem_error = sLUMemXpand(jcol, nextlu, LUSUP, &nzlumax, Glu)))
	    return (mem_error);
	lusup = Glu->lusup;
	lsub = Glu->lsub;
    }

    for (isub = xlsub[fsupc]; isub < xlsub[fsupc+1]; isub++) {
  	irow = lsub[isub];
	lusup[nextlu] = dense[irow];
        dense[irow] = zero;
	++nextlu;
    }

    xlusup[jcolp1] = nextlu;	/* Close L\U[*,jcol] */

    /* For more updates within the panel (also within the current supernode), 
     * should start from the first column of the panel, or the first column 
     * of the supernode, whichever is bigger. There are 2 cases:
     *    1) fsupc < fpanelc, then fst_col := fpanelc
     *    2) fsupc >= fpanelc, then fst_col := fsupc
     */
    fst_col = SUPERLU_MAX ( fsupc, fpanelc );

    if ( fst_col < jcol ) {

  	/* Distance between the current supernode and the current panel.
	   d_fsupc=0 if fsupc >= fpanelc. */
  	d_fsupc = fst_col - fsupc;

	luptr = xlusup[fst_col] + d_fsupc;
	nsupr = xlsub[fsupc+1] - xlsub[fsupc];	/* Leading dimension */
	nsupc = jcol - fst_col;	/* Excluding jcol */
	nrow = nsupr - d_fsupc - nsupc;

	/* Points to the beginning of jcol in snode L\U(jsupno) */
	ufirst = xlusup[jcol] + d_fsupc;	

	ops[TRSV] += nsupc * (nsupc - 1);
	ops[GEMV] += 2 * nrow * nsupc;
	
#ifdef USE_VENDOR_BLAS
#ifdef _CRAY
	STRSV( ftcs1, ftcs2, ftcs3, &nsupc, &lusup[luptr], 
	       &nsupr, &lusup[ufirst], &incx );
#else
	strsv_( "L", "N", "U", &nsupc, &lusup[luptr], 
	       &nsupr, &lusup[ufirst], &incx );
#endif
	
	alpha = none; beta = one; /* y := beta*y + alpha*A*x */

#ifdef _CRAY
	SGEMV( ftcs2, &nrow, &nsupc, &alpha, &lusup[luptr+nsupc], &nsupr,
	       &lusup[ufirst], &incx, &beta, &lusup[ufirst+nsupc], &incy );
#else
	sgemv_( "N", &nrow, &nsupc, &alpha, &lusup[luptr+nsupc], &nsupr,
	       &lusup[ufirst], &incx, &beta, &lusup[ufirst+nsupc], &incy );
#endif
#else
	slsolve ( nsupr, nsupc, &lusup[luptr], &lusup[ufirst] );

	smatvec ( nsupr, nrow, nsupc, &lusup[luptr+nsupc],
		&lusup[ufirst], tempv );
	
        /* Copy updates from tempv[*] into lusup[*] */
	isub = ufirst + nsupc;
	for (i = 0; i < nrow; i++) {
	    lusup[isub] -= tempv[i];
	    tempv[i] = 0.0;
	    ++isub;
	}

#endif
	
	
    } /* if fst_col < jcol ... */ 

    return 0;
}
