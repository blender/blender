/** \file opennl/superlu/util.c
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
#include "util.h"

/* prototypes */
flops_t LUFactFlops(SuperLUStat_t *stat);
flops_t LUSolveFlops(SuperLUStat_t *stat);
double SpaSize(int n, int np, double sum_npw);
double DenseSize(int n, double sum_nw);

/* 
 * Global statistics variale
 */

void superlu_abort_and_exit(char* msg)
{
    fprintf(stderr, "%s", msg);
    exit (-1);
}

/*
 * Set the default values for the options argument.
 */
void set_default_options(superlu_options_t *options)
{
    options->Fact = DOFACT;
    options->Equil = YES;
    options->ColPerm = COLAMD;
    options->DiagPivotThresh = 1.0;
    options->Trans = NOTRANS;
    options->IterRefine = NOREFINE;
    options->SymmetricMode = NO;
    options->PivotGrowth = NO;
    options->ConditionNumber = NO;
    options->PrintStat = YES;
}

/* Deallocate the structure pointing to the actual storage of the matrix. */
void
Destroy_SuperMatrix_Store(SuperMatrix *A)
{
    SUPERLU_FREE ( A->Store );
}

void
Destroy_CompCol_Matrix(SuperMatrix *A)
{
    SUPERLU_FREE( ((NCformat *)A->Store)->rowind );
    SUPERLU_FREE( ((NCformat *)A->Store)->colptr );
    SUPERLU_FREE( ((NCformat *)A->Store)->nzval );
    SUPERLU_FREE( A->Store );
}

void
Destroy_CompRow_Matrix(SuperMatrix *A)
{
    SUPERLU_FREE( ((NRformat *)A->Store)->colind );
    SUPERLU_FREE( ((NRformat *)A->Store)->rowptr );
    SUPERLU_FREE( ((NRformat *)A->Store)->nzval );
    SUPERLU_FREE( A->Store );
}

void
Destroy_SuperNode_Matrix(SuperMatrix *A)
{
    SUPERLU_FREE ( ((SCformat *)A->Store)->rowind );
    SUPERLU_FREE ( ((SCformat *)A->Store)->rowind_colptr );
    SUPERLU_FREE ( ((SCformat *)A->Store)->nzval );
    SUPERLU_FREE ( ((SCformat *)A->Store)->nzval_colptr );
    SUPERLU_FREE ( ((SCformat *)A->Store)->col_to_sup );
    SUPERLU_FREE ( ((SCformat *)A->Store)->sup_to_col );
    SUPERLU_FREE ( A->Store );
}

/* A is of type Stype==NCP */
void
Destroy_CompCol_Permuted(SuperMatrix *A)
{
    SUPERLU_FREE ( ((NCPformat *)A->Store)->colbeg );
    SUPERLU_FREE ( ((NCPformat *)A->Store)->colend );
    SUPERLU_FREE ( A->Store );
}

/* A is of type Stype==DN */
void
Destroy_Dense_Matrix(SuperMatrix *A)
{
    DNformat* Astore = A->Store;
    SUPERLU_FREE (Astore->nzval);
    SUPERLU_FREE ( A->Store );
}

/*
 * Reset repfnz[] for the current column 
 */
void
resetrep_col (const int nseg, const int *segrep, int *repfnz)
{
    int i, irep;
    
    for (i = 0; i < nseg; i++) {
	irep = segrep[i];
	repfnz[irep] = EMPTY;
    }
}


/*
 * Count the total number of nonzeros in factors L and U,  and in the 
 * symmetrically reduced L. 
 */
void
countnz(const int n, int *xprune, int *nnzL, int *nnzU, GlobalLU_t *Glu)
{
    int          nsuper, fsupc, i, j;
    int          nnzL0, jlen, irep;
    int          *xsup, *xlsub;

    xsup   = Glu->xsup;
    xlsub  = Glu->xlsub;
    *nnzL  = 0;
    *nnzU  = (Glu->xusub)[n];
    nnzL0  = 0;
    nsuper = (Glu->supno)[n];

    if ( n <= 0 ) return;

    /* 
     * For each supernode
     */
    for (i = 0; i <= nsuper; i++) {
	fsupc = xsup[i];
	jlen = xlsub[fsupc+1] - xlsub[fsupc];

	for (j = fsupc; j < xsup[i+1]; j++) {
	    *nnzL += jlen;
	    *nnzU += j - fsupc + 1;
	    jlen--;
	}
	irep = xsup[i+1] - 1;
	nnzL0 += xprune[irep] - xlsub[irep];
    }
    
    /* printf("\tNo of nonzeros in symm-reduced L = %d\n", nnzL0);*/
}



/*
 * Fix up the data storage lsub for L-subscripts. It removes the subscript
 * sets for structural pruning,	and applies permuation to the remaining
 * subscripts.
 */
void
fixupL(const int n, const int *perm_r, GlobalLU_t *Glu)
{
    register int nsuper, fsupc, nextl, i, j, k, jstrt;
    int          *xsup, *lsub, *xlsub;

    if ( n <= 1 ) return;

    xsup   = Glu->xsup;
    lsub   = Glu->lsub;
    xlsub  = Glu->xlsub;
    nextl  = 0;
    nsuper = (Glu->supno)[n];
    
    /* 
     * For each supernode ...
     */
    for (i = 0; i <= nsuper; i++) {
	fsupc = xsup[i];
	jstrt = xlsub[fsupc];
	xlsub[fsupc] = nextl;
	for (j = jstrt; j < xlsub[fsupc+1]; j++) {
	    lsub[nextl] = perm_r[lsub[j]]; /* Now indexed into P*A */
	    nextl++;
  	}
	for (k = fsupc+1; k < xsup[i+1]; k++) 
	    	xlsub[k] = nextl;	/* Other columns in supernode i */

    }

    xlsub[n] = nextl;
}


/*
 * Diagnostic print of segment info after panel_dfs().
 */
void print_panel_seg(int n, int w, int jcol, int nseg, 
		     int *segrep, int *repfnz)
{
    int j, k;
    
    for (j = jcol; j < jcol+w; j++) {
	printf("\tcol %d:\n", j);
	for (k = 0; k < nseg; k++)
	    printf("\t\tseg %d, segrep %d, repfnz %d\n", k, 
			segrep[k], repfnz[(j-jcol)*n + segrep[k]]);
    }

}


void
StatInit(SuperLUStat_t *stat)
{
    register int i, w, panel_size, relax;

    panel_size = sp_ienv(1);
    relax = sp_ienv(2);
    w = SUPERLU_MAX(panel_size, relax);
    stat->panel_histo = intCalloc(w+1);
    stat->utime = (double *) SUPERLU_MALLOC(NPHASES * sizeof(double));
    if (!stat->utime) ABORT("SUPERLU_MALLOC fails for stat->utime");
    stat->ops = (flops_t *) SUPERLU_MALLOC(NPHASES * sizeof(flops_t));
    if (!stat->ops) ABORT("SUPERLU_MALLOC fails for stat->ops");
    for (i = 0; i < NPHASES; ++i) {
        stat->utime[i] = 0.;
        stat->ops[i] = 0.;
    }
}


void
StatPrint(SuperLUStat_t *stat)
{
    double         *utime;
    flops_t        *ops;

    utime = stat->utime;
    ops   = stat->ops;
    printf("Factor time  = %8.2f\n", utime[FACT]);
    if ( utime[FACT] != 0.0 )
      printf("Factor flops = %e\tMflops = %8.2f\n", ops[FACT],
	     ops[FACT]*1e-6/utime[FACT]);

    printf("Solve time   = %8.2f\n", utime[SOLVE]);
    if ( utime[SOLVE] != 0.0 )
      printf("Solve flops = %e\tMflops = %8.2f\n", ops[SOLVE],
	     ops[SOLVE]*1e-6/utime[SOLVE]);

}


void
StatFree(SuperLUStat_t *stat)
{
    SUPERLU_FREE(stat->panel_histo);
    SUPERLU_FREE(stat->utime);
    SUPERLU_FREE(stat->ops);
}


flops_t
LUFactFlops(SuperLUStat_t *stat)
{
    return (stat->ops[FACT]);
}

flops_t
LUSolveFlops(SuperLUStat_t *stat)
{
    return (stat->ops[SOLVE]);
}





/* 
 * Fills an integer array with a given value.
 */
void ifill(int *a, int alen, int ival)
{
    register int i;
    for (i = 0; i < alen; i++) a[i] = ival;
}



/* 
 * Get the statistics of the supernodes 
 */
#define NBUCKS 10
static 	int	max_sup_size;

void super_stats(int nsuper, int *xsup)
{
    register int nsup1 = 0;
    int          i, isize, whichb, bl, bh;
    int          bucket[NBUCKS];

    max_sup_size = 0;

    for (i = 0; i <= nsuper; i++) {
	isize = xsup[i+1] - xsup[i];
	if ( isize == 1 ) nsup1++;
	if ( max_sup_size < isize ) max_sup_size = isize;	
    }

    printf("    Supernode statistics:\n\tno of super = %d\n", nsuper+1);
    printf("\tmax supernode size = %d\n", max_sup_size);
    printf("\tno of size 1 supernodes = %d\n", nsup1);

    /* Histogram of the supernode sizes */
    ifill (bucket, NBUCKS, 0);

    for (i = 0; i <= nsuper; i++) {
        isize = xsup[i+1] - xsup[i];
        whichb = (double) isize / max_sup_size * NBUCKS;
        if (whichb >= NBUCKS) whichb = NBUCKS - 1;
        bucket[whichb]++;
    }
    
    printf("\tHistogram of supernode sizes:\n");
    for (i = 0; i < NBUCKS; i++) {
        bl = (double) i * max_sup_size / NBUCKS;
        bh = (double) (i+1) * max_sup_size / NBUCKS;
        printf("\tsnode: %d-%d\t\t%d\n", bl+1, bh, bucket[i]);
    }

}


double SpaSize(int n, int np, double sum_npw)
{
    return (sum_npw*8 + np*8 + n*4)/1024.;
}

double DenseSize(int n, double sum_nw)
{
    return (sum_nw*8 + n*8)/1024.;;
}



/*
 * Check whether repfnz[] == EMPTY after reset.
 */
void check_repfnz(int n, int w, int jcol, int *repfnz)
{
    int jj, k;

    for (jj = jcol; jj < jcol+w; jj++) 
	for (k = 0; k < n; k++)
	    if ( repfnz[(jj-jcol)*n + k] != EMPTY ) {
		fprintf(stderr, "col %d, repfnz_col[%d] = %d\n", jj,
			k, repfnz[(jj-jcol)*n + k]);
		ABORT("check_repfnz");
	    }
}


/* Print a summary of the testing results. */
void
PrintSumm(char *type, int nfail, int nrun, int nerrs)
{
    if ( nfail > 0 )
	printf("%3s driver: %d out of %d tests failed to pass the threshold\n",
	       type, nfail, nrun);
    else
	printf("All tests for %3s driver passed the threshold (%6d tests run)\n", type, nrun);

    if ( nerrs > 0 )
	printf("%6d error messages recorded\n", nerrs);
}


int print_int_vec(char *what, int n, int *vec)
{
    int i;
    printf("%s\n", what);
    for (i = 0; i < n; ++i) printf("%d\t%d\n", i, vec[i]);
    return 0;
}
