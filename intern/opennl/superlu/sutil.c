/** \file opennl/superlu/sutil.c
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

#include <math.h>
#include "ssp_defs.h"

/* prototypes */
void sprint_lu_col(char *msg, int jcol, int pivrow, int *xprune, GlobalLU_t *Glu);
void scheck_tempv(int n, double *tempv);
void sPrintPerf(SuperMatrix *, SuperMatrix *, mem_usage_t *,double , double , 
				 double *, double *, char *, SuperLUStat_t *);
int print_double_vec(char *what, int n, double *vec);
/* ********** */

void
sCreate_CompCol_Matrix(SuperMatrix *A, int m, int n, int nnz, 
		       double *nzval, int *rowind, int *colptr,
		       Stype_t stype, Dtype_t dtype, Mtype_t mtype)
{
    NCformat *Astore;

    A->Stype = stype;
    A->Dtype = dtype;
    A->Mtype = mtype;
    A->nrow = m;
    A->ncol = n;
    A->Store = (void *) SUPERLU_MALLOC( sizeof(NCformat) );
    if ( !(A->Store) ) ABORT("SUPERLU_MALLOC fails for A->Store");
    Astore = A->Store;
    Astore->nnz = nnz;
    Astore->nzval = nzval;
    Astore->rowind = rowind;
    Astore->colptr = colptr;
}

void
sCreate_CompRow_Matrix(SuperMatrix *A, int m, int n, int nnz, 
		       double *nzval, int *colind, int *rowptr,
		       Stype_t stype, Dtype_t dtype, Mtype_t mtype)
{
    NRformat *Astore;

    A->Stype = stype;
    A->Dtype = dtype;
    A->Mtype = mtype;
    A->nrow = m;
    A->ncol = n;
    A->Store = (void *) SUPERLU_MALLOC( sizeof(NRformat) );
    if ( !(A->Store) ) ABORT("SUPERLU_MALLOC fails for A->Store");
    Astore = A->Store;
    Astore->nnz = nnz;
    Astore->nzval = nzval;
    Astore->colind = colind;
    Astore->rowptr = rowptr;
}

/* Copy matrix A into matrix B. */
void
sCopy_CompCol_Matrix(SuperMatrix *A, SuperMatrix *B)
{
    NCformat *Astore, *Bstore;
    int      ncol, nnz, i;

    B->Stype = A->Stype;
    B->Dtype = A->Dtype;
    B->Mtype = A->Mtype;
    B->nrow  = A->nrow;;
    B->ncol  = ncol = A->ncol;
    Astore   = (NCformat *) A->Store;
    Bstore   = (NCformat *) B->Store;
    Bstore->nnz = nnz = Astore->nnz;
    for (i = 0; i < nnz; ++i)
	((double *)Bstore->nzval)[i] = ((double *)Astore->nzval)[i];
    for (i = 0; i < nnz; ++i) Bstore->rowind[i] = Astore->rowind[i];
    for (i = 0; i <= ncol; ++i) Bstore->colptr[i] = Astore->colptr[i];
}


void
sCreate_Dense_Matrix(SuperMatrix *X, int m, int n, double *x, int ldx,
		    Stype_t stype, Dtype_t dtype, Mtype_t mtype)
{
    DNformat    *Xstore;
    
    X->Stype = stype;
    X->Dtype = dtype;
    X->Mtype = mtype;
    X->nrow = m;
    X->ncol = n;
    X->Store = (void *) SUPERLU_MALLOC( sizeof(DNformat) );
    if ( !(X->Store) ) ABORT("SUPERLU_MALLOC fails for X->Store");
    Xstore = (DNformat *) X->Store;
    Xstore->lda = ldx;
    Xstore->nzval = (double *) x;
}

void
sCopy_Dense_Matrix(int M, int N, double *X, int ldx,
			double *Y, int ldy)
{
/*
 *
 *  Purpose
 *  =======
 *
 *  Copies a two-dimensional matrix X to another matrix Y.
 */
    int    i, j;
    
    for (j = 0; j < N; ++j)
        for (i = 0; i < M; ++i)
            Y[i + j*ldy] = X[i + j*ldx];
}

void
sCreate_SuperNode_Matrix(SuperMatrix *L, int m, int n, int nnz, 
			double *nzval, int *nzval_colptr, int *rowind,
			int *rowind_colptr, int *col_to_sup, int *sup_to_col,
			Stype_t stype, Dtype_t dtype, Mtype_t mtype)
{
    SCformat *Lstore;

    L->Stype = stype;
    L->Dtype = dtype;
    L->Mtype = mtype;
    L->nrow = m;
    L->ncol = n;
    L->Store = (void *) SUPERLU_MALLOC( sizeof(SCformat) );
    if ( !(L->Store) ) ABORT("SUPERLU_MALLOC fails for L->Store");
    Lstore = L->Store;
    Lstore->nnz = nnz;
    Lstore->nsuper = col_to_sup[n];
    Lstore->nzval = nzval;
    Lstore->nzval_colptr = nzval_colptr;
    Lstore->rowind = rowind;
    Lstore->rowind_colptr = rowind_colptr;
    Lstore->col_to_sup = col_to_sup;
    Lstore->sup_to_col = sup_to_col;

}


/*
 * Convert a row compressed storage into a column compressed storage.
 */
void
sCompRow_to_CompCol(int m, int n, int nnz, 
		    double *a, int *colind, int *rowptr,
		    double **at, int **rowind, int **colptr)
{
    register int i, j, col, relpos;
    int *marker;

    /* Allocate storage for another copy of the matrix. */
    *at = (double *) doubleMalloc(nnz);
    *rowind = (int *) intMalloc(nnz);
    *colptr = (int *) intMalloc(n+1);
    marker = (int *) intCalloc(n);
    
    /* Get counts of each column of A, and set up column pointers */
    for (i = 0; i < m; ++i)
	for (j = rowptr[i]; j < rowptr[i+1]; ++j) ++marker[colind[j]];
    (*colptr)[0] = 0;
    for (j = 0; j < n; ++j) {
	(*colptr)[j+1] = (*colptr)[j] + marker[j];
	marker[j] = (*colptr)[j];
    }

    /* Transfer the matrix into the compressed column storage. */
    for (i = 0; i < m; ++i) {
	for (j = rowptr[i]; j < rowptr[i+1]; ++j) {
	    col = colind[j];
	    relpos = marker[col];
	    (*rowind)[relpos] = i;
	    (*at)[relpos] = a[j];
	    ++marker[col];
	}
    }

    SUPERLU_FREE(marker);
}


void
sPrint_CompCol_Matrix(char *what, SuperMatrix *A)
{
    NCformat     *Astore;
    register int i,n;
    double       *dp;
    
    printf("\nCompCol matrix %s:\n", what);
    printf("Stype %d, Dtype %d, Mtype %d\n", A->Stype,A->Dtype,A->Mtype);
    n = A->ncol;
    Astore = (NCformat *) A->Store;
    dp = (double *) Astore->nzval;
    printf("nrow %d, ncol %d, nnz %d\n", A->nrow,A->ncol,Astore->nnz);
    printf("nzval: ");
    for (i = 0; i < Astore->colptr[n]; ++i) printf("%f  ", dp[i]);
    printf("\nrowind: ");
    for (i = 0; i < Astore->colptr[n]; ++i) printf("%d  ", Astore->rowind[i]);
    printf("\ncolptr: ");
    for (i = 0; i <= n; ++i) printf("%d  ", Astore->colptr[i]);
    printf("\n");
    fflush(stdout);
}

void
sPrint_SuperNode_Matrix(char *what, SuperMatrix *A)
{
    SCformat     *Astore;
    register int i, j, k, c, d, n, nsup;
    double       *dp;
    int *col_to_sup, *sup_to_col, *rowind, *rowind_colptr;
    
    printf("\nSuperNode matrix %s:\n", what);
    printf("Stype %d, Dtype %d, Mtype %d\n", A->Stype,A->Dtype,A->Mtype);
    n = A->ncol;
    Astore = (SCformat *) A->Store;
    dp = (double *) Astore->nzval;
    col_to_sup = Astore->col_to_sup;
    sup_to_col = Astore->sup_to_col;
    rowind_colptr = Astore->rowind_colptr;
    rowind = Astore->rowind;
    printf("nrow %d, ncol %d, nnz %d, nsuper %d\n", 
	   A->nrow,A->ncol,Astore->nnz,Astore->nsuper);
    printf("nzval:\n");
    for (k = 0; k <= Astore->nsuper; ++k) {
      c = sup_to_col[k];
      nsup = sup_to_col[k+1] - c;
      for (j = c; j < c + nsup; ++j) {
	d = Astore->nzval_colptr[j];
	for (i = rowind_colptr[c]; i < rowind_colptr[c+1]; ++i) {
	  printf("%d\t%d\t%e\n", rowind[i], j, dp[d++]);
	}
      }
    }
#if 0
    for (i = 0; i < Astore->nzval_colptr[n]; ++i) printf("%f  ", dp[i]);
#endif
    printf("\nnzval_colptr: ");
    for (i = 0; i <= n; ++i) printf("%d  ", Astore->nzval_colptr[i]);
    printf("\nrowind: ");
    for (i = 0; i < Astore->rowind_colptr[n]; ++i) 
        printf("%d  ", Astore->rowind[i]);
    printf("\nrowind_colptr: ");
    for (i = 0; i <= n; ++i) printf("%d  ", Astore->rowind_colptr[i]);
    printf("\ncol_to_sup: ");
    for (i = 0; i < n; ++i) printf("%d  ", col_to_sup[i]);
    printf("\nsup_to_col: ");
    for (i = 0; i <= Astore->nsuper+1; ++i) 
        printf("%d  ", sup_to_col[i]);
    printf("\n");
    fflush(stdout);
}

void
sPrint_Dense_Matrix(char *what, SuperMatrix *A)
{
    DNformat     *Astore;
    register int i;
    double       *dp;
    
    printf("\nDense matrix %s:\n", what);
    printf("Stype %d, Dtype %d, Mtype %d\n", A->Stype,A->Dtype,A->Mtype);
    Astore = (DNformat *) A->Store;
    dp = (double *) Astore->nzval;
    printf("nrow %d, ncol %d, lda %d\n", A->nrow,A->ncol,Astore->lda);
    printf("\nnzval: ");
    for (i = 0; i < A->nrow; ++i) printf("%f  ", dp[i]);
    printf("\n");
    fflush(stdout);
}

/*
 * Diagnostic print of column "jcol" in the U/L factor.
 */
void
sprint_lu_col(char *msg, int jcol, int pivrow, int *xprune, GlobalLU_t *Glu)
{
    int     i, k, fsupc;
    int     *xsup, *supno;
    int     *xlsub, *lsub;
    double  *lusup;
    int     *xlusup;
    double  *ucol;
    int     *usub, *xusub;

    xsup    = Glu->xsup;
    supno   = Glu->supno;
    lsub    = Glu->lsub;
    xlsub   = Glu->xlsub;
    lusup   = Glu->lusup;
    xlusup  = Glu->xlusup;
    ucol    = Glu->ucol;
    usub    = Glu->usub;
    xusub   = Glu->xusub;
    
    printf("%s", msg);
    printf("col %d: pivrow %d, supno %d, xprune %d\n", 
	   jcol, pivrow, supno[jcol], xprune[jcol]);
    
    printf("\tU-col:\n");
    for (i = xusub[jcol]; i < xusub[jcol+1]; i++)
	printf("\t%d%10.4f\n", usub[i], ucol[i]);
    printf("\tL-col in rectangular snode:\n");
    fsupc = xsup[supno[jcol]];	/* first col of the snode */
    i = xlsub[fsupc];
    k = xlusup[jcol];
    while ( i < xlsub[fsupc+1] && k < xlusup[jcol+1] ) {
	printf("\t%d\t%10.4f\n", lsub[i], lusup[k]);
	i++; k++;
    }
    fflush(stdout);
}


/*
 * Check whether tempv[] == 0. This should be true before and after 
 * calling any numeric routines, i.e., "panel_bmod" and "column_bmod". 
 */
void scheck_tempv(int n, double *tempv)
{
    int i;
	
    for (i = 0; i < n; i++) {
	if (tempv[i] != 0.0) 
	{
	    fprintf(stderr,"tempv[%d] = %f\n", i,tempv[i]);
	    ABORT("scheck_tempv");
	}
    }
}


void
sGenXtrue(int n, int nrhs, double *x, int ldx)
{
    int  i, j;
    for (j = 0; j < nrhs; ++j)
	for (i = 0; i < n; ++i) {
	    x[i + j*ldx] = 1.0;/* + (double)(i+1.)/n;*/
	}
}

/*
 * Let rhs[i] = sum of i-th row of A, so the solution vector is all 1's
 */
void
sFillRHS(trans_t trans, int nrhs, double *x, int ldx,
         SuperMatrix *A, SuperMatrix *B)
{
    DNformat *Bstore;
    double   *rhs;
    double one = 1.0;
    double zero = 0.0;
    int      ldc;
    char transc[1];

    Bstore = B->Store;
    rhs    = Bstore->nzval;
    ldc    = Bstore->lda;
    
    if ( trans == NOTRANS ) *(unsigned char *)transc = 'N';
    else *(unsigned char *)transc = 'T';

    sp_sgemm(transc, nrhs, one, A,
	     x, ldx, zero, rhs, ldc);

}

/* 
 * Fills a double precision array with a given value.
 */
void 
sfill(double *a, int alen, double dval)
{
    register int i;
    for (i = 0; i < alen; i++) a[i] = dval;
}



/* 
 * Check the inf-norm of the error vector 
 */
void sinf_norm_error(int nrhs, SuperMatrix *X, double *xtrue)
{
    DNformat *Xstore;
    double err, xnorm;
    double *Xmat, *soln_work;
    int i, j;

    Xstore = X->Store;
    Xmat = Xstore->nzval;

    for (j = 0; j < nrhs; j++) {
      soln_work = &Xmat[j*Xstore->lda];
      err = xnorm = 0.0;
      for (i = 0; i < X->nrow; i++) {
	err = SUPERLU_MAX(err, fabs(soln_work[i] - xtrue[i]));
	xnorm = SUPERLU_MAX(xnorm, fabs(soln_work[i]));
      }
      err = err / xnorm;
      printf("||X - Xtrue||/||X|| = %e\n", err);
    }
}



/* Print performance of the code. */
void
sPrintPerf(SuperMatrix *L, SuperMatrix *U, mem_usage_t *mem_usage,
           double rpg, double rcond, double *ferr,
           double *berr, char *equed, SuperLUStat_t *stat)
{
    SCformat *Lstore;
    NCformat *Ustore;
    double   *utime;
    flops_t  *ops;
    
    utime = stat->utime;
    ops   = stat->ops;
    
    if ( utime[FACT] != 0. )
	printf("Factor flops = %e\tMflops = %8.2f\n", ops[FACT],
	       ops[FACT]*1e-6/utime[FACT]);
    printf("Identify relaxed snodes	= %8.2f\n", utime[RELAX]);
    if ( utime[SOLVE] != 0. )
	printf("Solve flops = %.0f, Mflops = %8.2f\n", ops[SOLVE],
	       ops[SOLVE]*1e-6/utime[SOLVE]);
    
    Lstore = (SCformat *) L->Store;
    Ustore = (NCformat *) U->Store;
    printf("\tNo of nonzeros in factor L = %d\n", Lstore->nnz);
    printf("\tNo of nonzeros in factor U = %d\n", Ustore->nnz);
    printf("\tNo of nonzeros in L+U = %d\n", Lstore->nnz + Ustore->nnz);
	
    printf("L\\U MB %.3f\ttotal MB needed %.3f\texpansions %d\n",
	   mem_usage->for_lu/1e6, mem_usage->total_needed/1e6,
	   mem_usage->expansions);
	
    printf("\tFactor\tMflops\tSolve\tMflops\tEtree\tEquil\tRcond\tRefine\n");
    printf("PERF:%8.2f%8.2f%8.2f%8.2f%8.2f%8.2f%8.2f%8.2f\n",
	   utime[FACT], ops[FACT]*1e-6/utime[FACT],
	   utime[SOLVE], ops[SOLVE]*1e-6/utime[SOLVE],
	   utime[ETREE], utime[EQUIL], utime[RCOND], utime[REFINE]);
    
    printf("\tRpg\t\tRcond\t\tFerr\t\tBerr\t\tEquil?\n");
    printf("NUM:\t%e\t%e\t%e\t%e\t%s\n",
	   rpg, rcond, ferr[0], berr[0], equed);
    
}




int print_double_vec(char *what, int n, double *vec)
{
    int i;
    printf("%s: n %d\n", what, n);
    for (i = 0; i < n; ++i) printf("%d\t%f\n", i, vec[i]);
    return 0;
}

