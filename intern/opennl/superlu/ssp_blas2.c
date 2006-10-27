
/*
 * -- SuperLU routine (version 3.0) --
 * Univ. of California Berkeley, Xerox Palo Alto Research Center,
 * and Lawrence Berkeley National Lab.
 * October 15, 2003
 *
 */
/*
 * File name:		ssp_blas2.c
 * Purpose:		Sparse BLAS 2, using some dense BLAS 2 operations.
 */

#include "ssp_defs.h"

/* 
 * Function prototypes 
 */
void susolve(int, int, float*, float*);
void slsolve(int, int, float*, float*);
void smatvec(int, int, int, float*, float*, float*);
int strsv_(char*, char*, char*, int*, float*, int*, float*, int*);

int
sp_strsv(char *uplo, char *trans, char *diag, SuperMatrix *L, 
         SuperMatrix *U, float *x, SuperLUStat_t *stat, int *info)
{
/*
 *   Purpose
 *   =======
 *
 *   sp_strsv() solves one of the systems of equations   
 *       A*x = b,   or   A'*x = b,
 *   where b and x are n element vectors and A is a sparse unit , or   
 *   non-unit, upper or lower triangular matrix.   
 *   No test for singularity or near-singularity is included in this   
 *   routine. Such tests must be performed before calling this routine.   
 *
 *   Parameters   
 *   ==========   
 *
 *   uplo   - (input) char*
 *            On entry, uplo specifies whether the matrix is an upper or   
 *             lower triangular matrix as follows:   
 *                uplo = 'U' or 'u'   A is an upper triangular matrix.   
 *                uplo = 'L' or 'l'   A is a lower triangular matrix.   
 *
 *   trans  - (input) char*
 *             On entry, trans specifies the equations to be solved as   
 *             follows:   
 *                trans = 'N' or 'n'   A*x = b.   
 *                trans = 'T' or 't'   A'*x = b.
 *                trans = 'C' or 'c'   A'*x = b.   
 *
 *   diag   - (input) char*
 *             On entry, diag specifies whether or not A is unit   
 *             triangular as follows:   
 *                diag = 'U' or 'u'   A is assumed to be unit triangular.   
 *                diag = 'N' or 'n'   A is not assumed to be unit   
 *                                    triangular.   
 *	     
 *   L       - (input) SuperMatrix*
 *	       The factor L from the factorization Pr*A*Pc=L*U. Use
 *             compressed row subscripts storage for supernodes,
 *             i.e., L has types: Stype = SC, Dtype = SLU_S, Mtype = TRLU.
 *
 *   U       - (input) SuperMatrix*
 *	        The factor U from the factorization Pr*A*Pc=L*U.
 *	        U has types: Stype = NC, Dtype = SLU_S, Mtype = TRU.
 *    
 *   x       - (input/output) float*
 *             Before entry, the incremented array X must contain the n   
 *             element right-hand side vector b. On exit, X is overwritten 
 *             with the solution vector x.
 *
 *   info    - (output) int*
 *             If *info = -i, the i-th argument had an illegal value.
 *
 */
#ifdef _CRAY
    _fcd ftcs1 = _cptofcd("L", strlen("L")),
	 ftcs2 = _cptofcd("N", strlen("N")),
	 ftcs3 = _cptofcd("U", strlen("U"));
#endif
    SCformat *Lstore;
    NCformat *Ustore;
    float   *Lval, *Uval;
    int incx = 1;
    int nrow;
    int fsupc, nsupr, nsupc, luptr, istart, irow;
    int i, k, iptr, jcol;
    float *work;
    flops_t solve_ops;

    /* Test the input parameters */
    *info = 0;
    if ( !lsame_(uplo,"L") && !lsame_(uplo, "U") ) *info = -1;
    else if ( !lsame_(trans, "N") && !lsame_(trans, "T") && 
              !lsame_(trans, "C")) *info = -2;
    else if ( !lsame_(diag, "U") && !lsame_(diag, "N") ) *info = -3;
    else if ( L->nrow != L->ncol || L->nrow < 0 ) *info = -4;
    else if ( U->nrow != U->ncol || U->nrow < 0 ) *info = -5;
    if ( *info ) {
	i = -(*info);
	xerbla_("sp_strsv", &i);
	return 0;
    }

    Lstore = L->Store;
    Lval = Lstore->nzval;
    Ustore = U->Store;
    Uval = Ustore->nzval;
    solve_ops = 0;

    if ( !(work = floatCalloc(L->nrow)) )
	ABORT("Malloc fails for work in sp_strsv().");
    
    if ( lsame_(trans, "N") ) {	/* Form x := inv(A)*x. */
	
	if ( lsame_(uplo, "L") ) {
	    /* Form x := inv(L)*x */
    	    if ( L->nrow == 0 ) {
			SUPERLU_FREE(work);
			return 0; /* Quick return */
		}
	    
	    for (k = 0; k <= Lstore->nsuper; k++) {
		fsupc = L_FST_SUPC(k);
		istart = L_SUB_START(fsupc);
		nsupr = L_SUB_START(fsupc+1) - istart;
		nsupc = L_FST_SUPC(k+1) - fsupc;
		luptr = L_NZ_START(fsupc);
		nrow = nsupr - nsupc;

	        solve_ops += nsupc * (nsupc - 1);
	        solve_ops += 2 * nrow * nsupc;

		if ( nsupc == 1 ) {
		    for (iptr=istart+1; iptr < L_SUB_START(fsupc+1); ++iptr) {
			irow = L_SUB(iptr);
			++luptr;
			x[irow] -= x[fsupc] * Lval[luptr];
		    }
		} else {
#ifdef USE_VENDOR_BLAS
#ifdef _CRAY
		    STRSV(ftcs1, ftcs2, ftcs3, &nsupc, &Lval[luptr], &nsupr,
		       	&x[fsupc], &incx);
		
		    SGEMV(ftcs2, &nrow, &nsupc, &alpha, &Lval[luptr+nsupc], 
		       	&nsupr, &x[fsupc], &incx, &beta, &work[0], &incy);
#else
		    strsv_("L", "N", "U", &nsupc, &Lval[luptr], &nsupr,
		       	&x[fsupc], &incx);
		
		    sgemv_("N", &nrow, &nsupc, &alpha, &Lval[luptr+nsupc], 
		       	&nsupr, &x[fsupc], &incx, &beta, &work[0], &incy);
#endif
#else
		    slsolve ( nsupr, nsupc, &Lval[luptr], &x[fsupc]);
		
		    smatvec ( nsupr, nsupr-nsupc, nsupc, &Lval[luptr+nsupc],
                             &x[fsupc], &work[0] );
#endif		
		
		    iptr = istart + nsupc;
		    for (i = 0; i < nrow; ++i, ++iptr) {
			irow = L_SUB(iptr);
			x[irow] -= work[i];	/* Scatter */
			work[i] = 0.0;

		    }
	 	}
	    } /* for k ... */
	    
	} else {
	    /* Form x := inv(U)*x */
	    
	    if ( U->nrow == 0 ) return 0; /* Quick return */
	    
	    for (k = Lstore->nsuper; k >= 0; k--) {
	    	fsupc = L_FST_SUPC(k);
	    	nsupr = L_SUB_START(fsupc+1) - L_SUB_START(fsupc);
	    	nsupc = L_FST_SUPC(k+1) - fsupc;
	    	luptr = L_NZ_START(fsupc);
		
    	        solve_ops += nsupc * (nsupc + 1);

		if ( nsupc == 1 ) {
		    x[fsupc] /= Lval[luptr];
		    for (i = U_NZ_START(fsupc); i < U_NZ_START(fsupc+1); ++i) {
			irow = U_SUB(i);
			x[irow] -= x[fsupc] * Uval[i];
		    }
		} else {
#ifdef USE_VENDOR_BLAS
#ifdef _CRAY
		    STRSV(ftcs3, ftcs2, ftcs2, &nsupc, &Lval[luptr], &nsupr,
		       &x[fsupc], &incx);
#else
		    strsv_("U", "N", "N", &nsupc, &Lval[luptr], &nsupr,
                           &x[fsupc], &incx);
#endif
#else		
		    susolve ( nsupr, nsupc, &Lval[luptr], &x[fsupc] );
#endif		

		    for (jcol = fsupc; jcol < L_FST_SUPC(k+1); jcol++) {
		        solve_ops += 2*(U_NZ_START(jcol+1) - U_NZ_START(jcol));
		    	for (i = U_NZ_START(jcol); i < U_NZ_START(jcol+1); 
				i++) {
			    irow = U_SUB(i);
			    x[irow] -= x[jcol] * Uval[i];
		    	}
                    }
		}
	    } /* for k ... */
	    
	}
    } else { /* Form x := inv(A')*x */
	
	if ( lsame_(uplo, "L") ) {
	    /* Form x := inv(L')*x */
    	    if ( L->nrow == 0 ) return 0; /* Quick return */
	    
	    for (k = Lstore->nsuper; k >= 0; --k) {
	    	fsupc = L_FST_SUPC(k);
	    	istart = L_SUB_START(fsupc);
	    	nsupr = L_SUB_START(fsupc+1) - istart;
	    	nsupc = L_FST_SUPC(k+1) - fsupc;
	    	luptr = L_NZ_START(fsupc);

		solve_ops += 2 * (nsupr - nsupc) * nsupc;

		for (jcol = fsupc; jcol < L_FST_SUPC(k+1); jcol++) {
		    iptr = istart + nsupc;
		    for (i = L_NZ_START(jcol) + nsupc; 
				i < L_NZ_START(jcol+1); i++) {
			irow = L_SUB(iptr);
			x[jcol] -= x[irow] * Lval[i];
			iptr++;
		    }
		}
		
		if ( nsupc > 1 ) {
		    solve_ops += nsupc * (nsupc - 1);
#ifdef _CRAY
                    ftcs1 = _cptofcd("L", strlen("L"));
                    ftcs2 = _cptofcd("T", strlen("T"));
                    ftcs3 = _cptofcd("U", strlen("U"));
		    STRSV(ftcs1, ftcs2, ftcs3, &nsupc, &Lval[luptr], &nsupr,
			&x[fsupc], &incx);
#else
		    strsv_("L", "T", "U", &nsupc, &Lval[luptr], &nsupr,
			&x[fsupc], &incx);
#endif
		}
	    }
	} else {
	    /* Form x := inv(U')*x */
	    if ( U->nrow == 0 ) return 0; /* Quick return */
	    
	    for (k = 0; k <= Lstore->nsuper; k++) {
	    	fsupc = L_FST_SUPC(k);
	    	nsupr = L_SUB_START(fsupc+1) - L_SUB_START(fsupc);
	    	nsupc = L_FST_SUPC(k+1) - fsupc;
	    	luptr = L_NZ_START(fsupc);

		for (jcol = fsupc; jcol < L_FST_SUPC(k+1); jcol++) {
		    solve_ops += 2*(U_NZ_START(jcol+1) - U_NZ_START(jcol));
		    for (i = U_NZ_START(jcol); i < U_NZ_START(jcol+1); i++) {
			irow = U_SUB(i);
			x[jcol] -= x[irow] * Uval[i];
		    }
		}

		solve_ops += nsupc * (nsupc + 1);

		if ( nsupc == 1 ) {
		    x[fsupc] /= Lval[luptr];
		} else {
#ifdef _CRAY
                    ftcs1 = _cptofcd("U", strlen("U"));
                    ftcs2 = _cptofcd("T", strlen("T"));
                    ftcs3 = _cptofcd("N", strlen("N"));
		    STRSV( ftcs1, ftcs2, ftcs3, &nsupc, &Lval[luptr], &nsupr,
			    &x[fsupc], &incx);
#else
		    strsv_("U", "T", "N", &nsupc, &Lval[luptr], &nsupr,
			    &x[fsupc], &incx);
#endif
		}
	    } /* for k ... */
	}
    }

    stat->ops[SOLVE] += solve_ops;
    SUPERLU_FREE(work);
    return 0;
}




int
sp_sgemv(char *trans, float alpha, SuperMatrix *A, float *x, 
	 int incx, float beta, float *y, int incy)
{
/*  Purpose   
    =======   

    sp_sgemv()  performs one of the matrix-vector operations   
       y := alpha*A*x + beta*y,   or   y := alpha*A'*x + beta*y,   
    where alpha and beta are scalars, x and y are vectors and A is a
    sparse A->nrow by A->ncol matrix.   

    Parameters   
    ==========   

    TRANS  - (input) char*
             On entry, TRANS specifies the operation to be performed as   
             follows:   
                TRANS = 'N' or 'n'   y := alpha*A*x + beta*y.   
                TRANS = 'T' or 't'   y := alpha*A'*x + beta*y.   
                TRANS = 'C' or 'c'   y := alpha*A'*x + beta*y.   

    ALPHA  - (input) float
             On entry, ALPHA specifies the scalar alpha.   

    A      - (input) SuperMatrix*
             Matrix A with a sparse format, of dimension (A->nrow, A->ncol).
             Currently, the type of A can be:
                 Stype = NC or NCP; Dtype = SLU_S; Mtype = GE. 
             In the future, more general A can be handled.

    X      - (input) float*, array of DIMENSION at least   
             ( 1 + ( n - 1 )*abs( INCX ) ) when TRANS = 'N' or 'n'   
             and at least   
             ( 1 + ( m - 1 )*abs( INCX ) ) otherwise.   
             Before entry, the incremented array X must contain the   
             vector x.   

    INCX   - (input) int
             On entry, INCX specifies the increment for the elements of   
             X. INCX must not be zero.   

    BETA   - (input) float
             On entry, BETA specifies the scalar beta. When BETA is   
             supplied as zero then Y need not be set on input.   

    Y      - (output) float*,  array of DIMENSION at least   
             ( 1 + ( m - 1 )*abs( INCY ) ) when TRANS = 'N' or 'n'   
             and at least   
             ( 1 + ( n - 1 )*abs( INCY ) ) otherwise.   
             Before entry with BETA non-zero, the incremented array Y   
             must contain the vector y. On exit, Y is overwritten by the 
             updated vector y.
	     
    INCY   - (input) int
             On entry, INCY specifies the increment for the elements of   
             Y. INCY must not be zero.   

    ==== Sparse Level 2 Blas routine.   
*/

    /* Local variables */
    NCformat *Astore;
    float   *Aval;
    int info;
    float temp;
    int lenx, leny, i, j, irow;
    int iy, jx, jy, kx, ky;
    int notran;

    notran = lsame_(trans, "N");
    Astore = A->Store;
    Aval = Astore->nzval;
    
    /* Test the input parameters */
    info = 0;
    if ( !notran && !lsame_(trans, "T") && !lsame_(trans, "C")) info = 1;
    else if ( A->nrow < 0 || A->ncol < 0 ) info = 3;
    else if (incx == 0) info = 5;
    else if (incy == 0)	info = 8;
    if (info != 0) {
	xerbla_("sp_sgemv ", &info);
	return 0;
    }

    /* Quick return if possible. */
    if (A->nrow == 0 || A->ncol == 0 || (alpha == 0. && beta == 1.))
	return 0;

    /* Set  LENX  and  LENY, the lengths of the vectors x and y, and set 
       up the start points in  X  and  Y. */
    if (lsame_(trans, "N")) {
	lenx = A->ncol;
	leny = A->nrow;
    } else {
	lenx = A->nrow;
	leny = A->ncol;
    }
    if (incx > 0) kx = 0;
    else kx =  - (lenx - 1) * incx;
    if (incy > 0) ky = 0;
    else ky =  - (leny - 1) * incy;

    /* Start the operations. In this version the elements of A are   
       accessed sequentially with one pass through A. */
    /* First form  y := beta*y. */
    if (beta != 1.) {
	if (incy == 1) {
	    if (beta == 0.)
		for (i = 0; i < leny; ++i) y[i] = 0.;
	    else
		for (i = 0; i < leny; ++i) y[i] = beta * y[i];
	} else {
	    iy = ky;
	    if (beta == 0.)
		for (i = 0; i < leny; ++i) {
		    y[iy] = 0.;
		    iy += incy;
		}
	    else
		for (i = 0; i < leny; ++i) {
		    y[iy] = beta * y[iy];
		    iy += incy;
		}
	}
    }
    
    if (alpha == 0.) return 0;

    if ( notran ) {
	/* Form  y := alpha*A*x + y. */
	jx = kx;
	if (incy == 1) {
	    for (j = 0; j < A->ncol; ++j) {
		if (x[jx] != 0.) {
		    temp = alpha * x[jx];
		    for (i = Astore->colptr[j]; i < Astore->colptr[j+1]; ++i) {
			irow = Astore->rowind[i];
			y[irow] += temp * Aval[i];
		    }
		}
		jx += incx;
	    }
	} else {
	    ABORT("Not implemented.");
	}
    } else {
	/* Form  y := alpha*A'*x + y. */
	jy = ky;
	if (incx == 1) {
	    for (j = 0; j < A->ncol; ++j) {
		temp = 0.;
		for (i = Astore->colptr[j]; i < Astore->colptr[j+1]; ++i) {
		    irow = Astore->rowind[i];
		    temp += Aval[i] * x[irow];
		}
		y[jy] += alpha * temp;
		jy += incy;
	    }
	} else {
	    ABORT("Not implemented.");
	}
    }
    return 0;
} /* sp_sgemv */



