/** \file opennl/superlu/sgstrf.c
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
sgstrf (superlu_options_t *options, SuperMatrix *A,
        int relax, int panel_size, int *etree, void *work, int lwork,
        int *perm_c, int *perm_r, SuperMatrix *L, SuperMatrix *U,
        SuperLUStat_t *stat, int *info)
{
/*
 * Purpose
 * =======
 *
 * SGSTRF computes an LU factorization of a general sparse m-by-n
 * matrix A using partial pivoting with row interchanges.
 * The factorization has the form
 *     Pr * A = L * U
 * where Pr is a row permutation matrix, L is lower triangular with unit
 * diagonal elements (lower trapezoidal if A->nrow > A->ncol), and U is upper 
 * triangular (upper trapezoidal if A->nrow < A->ncol).
 *
 * See supermatrix.h for the definition of 'SuperMatrix' structure.
 *
 * Arguments
 * =========
 *
 * options (input) superlu_options_t*
 *         The structure defines the input parameters to control
 *         how the LU decomposition will be performed.
 *
 * A        (input) SuperMatrix*
 *	    Original matrix A, permuted by columns, of dimension
 *          (A->nrow, A->ncol). The type of A can be:
 *          Stype = SLU_NCP; Dtype = SLU_S; Mtype = SLU_GE.
 *
 * drop_tol (input) float (NOT IMPLEMENTED)
 *	    Drop tolerance parameter. At step j of the Gaussian elimination,
 *          if abs(A_ij)/(max_i abs(A_ij)) < drop_tol, drop entry A_ij.
 *          0 <= drop_tol <= 1. The default value of drop_tol is 0.
 *
 * relax    (input) int
 *          To control degree of relaxing supernodes. If the number
 *          of nodes (columns) in a subtree of the elimination tree is less
 *          than relax, this subtree is considered as one supernode,
 *          regardless of the row structures of those columns.
 *
 * panel_size (input) int
 *          A panel consists of at most panel_size consecutive columns.
 *
 * etree    (input) int*, dimension (A->ncol)
 *          Elimination tree of A'*A.
 *          Note: etree is a vector of parent pointers for a forest whose
 *          vertices are the integers 0 to A->ncol-1; etree[root]==A->ncol.
 *          On input, the columns of A should be permuted so that the
 *          etree is in a certain postorder.
 *
 * work     (input/output) void*, size (lwork) (in bytes)
 *          User-supplied work space and space for the output data structures.
 *          Not referenced if lwork = 0;
 *
 * lwork   (input) int
 *         Specifies the size of work array in bytes.
 *         = 0:  allocate space internally by system malloc;
 *         > 0:  use user-supplied work array of length lwork in bytes,
 *               returns error if space runs out.
 *         = -1: the routine guesses the amount of space needed without
 *               performing the factorization, and returns it in
 *               *info; no other side effects.
 *
 * perm_c   (input) int*, dimension (A->ncol)
 *	    Column permutation vector, which defines the 
 *          permutation matrix Pc; perm_c[i] = j means column i of A is 
 *          in position j in A*Pc.
 *          When searching for diagonal, perm_c[*] is applied to the
 *          row subscripts of A, so that diagonal threshold pivoting
 *          can find the diagonal of A, rather than that of A*Pc.
 *
 * perm_r   (input/output) int*, dimension (A->nrow)
 *          Row permutation vector which defines the permutation matrix Pr,
 *          perm_r[i] = j means row i of A is in position j in Pr*A.
 *          If options->Fact = SamePattern_SameRowPerm, the pivoting routine
 *             will try to use the input perm_r, unless a certain threshold
 *             criterion is violated. In that case, perm_r is overwritten by
 *             a new permutation determined by partial pivoting or diagonal
 *             threshold pivoting.
 *          Otherwise, perm_r is output argument;
 *
 * L        (output) SuperMatrix*
 *          The factor L from the factorization Pr*A=L*U; use compressed row 
 *          subscripts storage for supernodes, i.e., L has type: 
 *          Stype = SLU_SC, Dtype = SLU_S, Mtype = SLU_TRLU.
 *
 * U        (output) SuperMatrix*
 *	    The factor U from the factorization Pr*A*Pc=L*U. Use column-wise
 *          storage scheme, i.e., U has types: Stype = SLU_NC, 
 *          Dtype = SLU_S, Mtype = SLU_TRU.
 *
 * stat     (output) SuperLUStat_t*
 *          Record the statistics on runtime and floating-point operation count.
 *          See util.h for the definition of 'SuperLUStat_t'.
 *
 * info     (output) int*
 *          = 0: successful exit
 *          < 0: if info = -i, the i-th argument had an illegal value
 *          > 0: if info = i, and i is
 *             <= A->ncol: U(i,i) is exactly zero. The factorization has
 *                been completed, but the factor U is exactly singular,
 *                and division by zero will occur if it is used to solve a
 *                system of equations.
 *             > A->ncol: number of bytes allocated when memory allocation
 *                failure occurred, plus A->ncol. If lwork = -1, it is
 *                the estimated amount of space needed, plus A->ncol.
 *
 * ======================================================================
 *
 * Local Working Arrays: 
 * ======================
 *   m = number of rows in the matrix
 *   n = number of columns in the matrix
 *
 *   xprune[0:n-1]: xprune[*] points to locations in subscript 
 *	vector lsub[*]. For column i, xprune[i] denotes the point where 
 *	structural pruning begins. I.e. only xlsub[i],..,xprune[i]-1 need 
 *	to be traversed for symbolic factorization.
 *
 *   marker[0:3*m-1]: marker[i] = j means that node i has been 
 *	reached when working on column j.
 *	Storage: relative to original row subscripts
 *	NOTE: There are 3 of them: marker/marker1 are used for panel dfs, 
 *	      see spanel_dfs.c; marker2 is used for inner-factorization,
 *            see scolumn_dfs.c.
 *
 *   parent[0:m-1]: parent vector used during dfs
 *      Storage: relative to new row subscripts
 *
 *   xplore[0:m-1]: xplore[i] gives the location of the next (dfs) 
 *	unexplored neighbor of i in lsub[*]
 *
 *   segrep[0:nseg-1]: contains the list of supernodal representatives
 *	in topological order of the dfs. A supernode representative is the 
 *	last column of a supernode.
 *      The maximum size of segrep[] is n.
 *
 *   repfnz[0:W*m-1]: for a nonzero segment U[*,j] that ends at a 
 *	supernodal representative r, repfnz[r] is the location of the first 
 *	nonzero in this segment.  It is also used during the dfs: repfnz[r]>0
 *	indicates the supernode r has been explored.
 *	NOTE: There are W of them, each used for one column of a panel. 
 *
 *   panel_lsub[0:W*m-1]: temporary for the nonzeros row indices below 
 *      the panel diagonal. These are filled in during spanel_dfs(), and are
 *      used later in the inner LU factorization within the panel.
 *	panel_lsub[]/dense[] pair forms the SPA data structure.
 *	NOTE: There are W of them.
 *
 *   dense[0:W*m-1]: sparse accumulating (SPA) vector for intermediate values;
 *	    	   NOTE: there are W of them.
 *
 *   tempv[0:*]: real temporary used for dense numeric kernels;
 *	The size of this array is defined by NUM_TEMPV() in ssp_defs.h.
 *
 */
    /* Local working arrays */
    NCPformat *Astore;
    int       *iperm_r = NULL; /* inverse of perm_r;
			   used when options->Fact == SamePattern_SameRowPerm */
    int       *iperm_c; /* inverse of perm_c */
    int       *iwork;
    float    *swork;
    int	      *segrep, *repfnz, *parent, *xplore;
    int	      *panel_lsub; /* dense[]/panel_lsub[] pair forms a w-wide SPA */
    int	      *xprune;
    int	      *marker;
    float    *dense, *tempv;
    int       *relax_end;
    float    *a;
    int       *asub;
    int       *xa_begin, *xa_end;
    int       *xsup, *supno;
    int       *xlsub, *xlusup, *xusub;
    int       nzlumax;
    static GlobalLU_t Glu; /* persistent to facilitate multiple factors. */

    /* Local scalars */
    fact_t    fact = options->Fact;
    double    diag_pivot_thresh = options->DiagPivotThresh;
    int       pivrow;   /* pivotal row number in the original matrix A */
    int       nseg1;	/* no of segments in U-column above panel row jcol */
    int       nseg;	/* no of segments in each U-column */
    register int jcol;	
    register int kcol;	/* end column of a relaxed snode */
    register int icol;
    register int i, k, jj, new_next, iinfo;
    int       m, n, min_mn, jsupno, fsupc, nextlu, nextu;
    int       w_def;	/* upper bound on panel width */
    int       usepr, iperm_r_allocated = 0;
    int       nnzL, nnzU;
    int       *panel_histo = stat->panel_histo;
    flops_t   *ops = stat->ops;

    iinfo    = 0;
    m        = A->nrow;
    n        = A->ncol;
    min_mn   = SUPERLU_MIN(m, n);
    Astore   = A->Store;
    a        = Astore->nzval;
    asub     = Astore->rowind;
    xa_begin = Astore->colbeg;
    xa_end   = Astore->colend;

    /* Allocate storage common to the factor routines */
    *info = sLUMemInit(fact, work, lwork, m, n, Astore->nnz,
                       panel_size, L, U, &Glu, &iwork, &swork);
    if ( *info ) return;
    
    xsup    = Glu.xsup;
    supno   = Glu.supno;
    xlsub   = Glu.xlsub;
    xlusup  = Glu.xlusup;
    xusub   = Glu.xusub;
    
    SetIWork(m, n, panel_size, iwork, &segrep, &parent, &xplore,
	     &repfnz, &panel_lsub, &xprune, &marker);
    sSetRWork(m, panel_size, swork, &dense, &tempv);
    
    usepr = (fact == SamePattern_SameRowPerm);
    if ( usepr ) {
	/* Compute the inverse of perm_r */
	iperm_r = (int *) intMalloc(m);
	for (k = 0; k < m; ++k) iperm_r[perm_r[k]] = k;
	iperm_r_allocated = 1;
    }
    iperm_c = (int *) intMalloc(n);
    for (k = 0; k < n; ++k) iperm_c[perm_c[k]] = k;

    /* Identify relaxed snodes */
    relax_end = (int *) intMalloc(n);
    if ( options->SymmetricMode == YES ) {
        heap_relax_snode(n, etree, relax, marker, relax_end); 
    } else {
        relax_snode(n, etree, relax, marker, relax_end); 
    }
    
    ifill (perm_r, m, EMPTY);
    ifill (marker, m * NO_MARKER, EMPTY);
    supno[0] = -1;
    xsup[0]  = xlsub[0] = xusub[0] = xlusup[0] = 0;
    w_def    = panel_size;

    /* 
     * Work on one "panel" at a time. A panel is one of the following: 
     *	   (a) a relaxed supernode at the bottom of the etree, or
     *	   (b) panel_size contiguous columns, defined by the user
     */
    for (jcol = 0; jcol < min_mn; ) {

	if ( relax_end[jcol] != EMPTY ) { /* start of a relaxed snode */
   	    kcol = relax_end[jcol];	  /* end of the relaxed snode */
	    panel_histo[kcol-jcol+1]++;

	    /* --------------------------------------
	     * Factorize the relaxed supernode(jcol:kcol) 
	     * -------------------------------------- */
	    /* Determine the union of the row structure of the snode */
	    if ( (*info = ssnode_dfs(jcol, kcol, asub, xa_begin, xa_end,
				    xprune, marker, &Glu)) != 0 ) {
		if ( iperm_r_allocated ) SUPERLU_FREE (iperm_r);
		SUPERLU_FREE (iperm_c);
		SUPERLU_FREE (relax_end);
		return;
	    }

            nextu    = xusub[jcol];
	    nextlu   = xlusup[jcol];
	    jsupno   = supno[jcol];
	    fsupc    = xsup[jsupno];
	    new_next = nextlu + (xlsub[fsupc+1]-xlsub[fsupc])*(kcol-jcol+1);
	    nzlumax = Glu.nzlumax;
	    while ( new_next > nzlumax ) {
		if ( (*info = sLUMemXpand(jcol, nextlu, LUSUP, &nzlumax, &Glu)) ) {
			if ( iperm_r_allocated ) SUPERLU_FREE (iperm_r);
			SUPERLU_FREE (iperm_c);
			SUPERLU_FREE (relax_end);
			return;
		}
	    }
    
	    for (icol = jcol; icol<= kcol; icol++) {
		xusub[icol+1] = nextu;
		
    		/* Scatter into SPA dense[*] */
    		for (k = xa_begin[icol]; k < xa_end[icol]; k++)
        	    dense[asub[k]] = a[k];

	       	/* Numeric update within the snode */
	        ssnode_bmod(icol, fsupc, dense, tempv, &Glu, stat);

		if ( (*info = spivotL(icol, diag_pivot_thresh, &usepr, perm_r,
				      iperm_r, iperm_c, &pivrow, &Glu, stat)) )
		    if ( iinfo == 0 ) iinfo = *info;
		
#ifdef DEBUG
		sprint_lu_col("[1]: ", icol, pivrow, xprune, &Glu);
#endif

	    }

	    jcol = icol;

	} else { /* Work on one panel of panel_size columns */
	    
	    /* Adjust panel_size so that a panel won't overlap with the next 
	     * relaxed snode.
	     */
	    panel_size = w_def;
	    for (k = jcol + 1; k < SUPERLU_MIN(jcol+panel_size, min_mn); k++) 
		if ( relax_end[k] != EMPTY ) {
		    panel_size = k - jcol;
		    break;
		}
	    if ( k == min_mn ) panel_size = min_mn - jcol;
	    panel_histo[panel_size]++;

	    /* symbolic factor on a panel of columns */
	    spanel_dfs(m, panel_size, jcol, A, perm_r, &nseg1,
		      dense, panel_lsub, segrep, repfnz, xprune,
		      marker, parent, xplore, &Glu);
	    
	    /* numeric sup-panel updates in topological order */
	    spanel_bmod(m, panel_size, jcol, nseg1, dense,
		        tempv, segrep, repfnz, &Glu, stat);
	    
	    /* Sparse LU within the panel, and below panel diagonal */
    	    for ( jj = jcol; jj < jcol + panel_size; jj++) {
 		k = (jj - jcol) * m; /* column index for w-wide arrays */

		nseg = nseg1;	/* Begin after all the panel segments */

	    	if ((*info = scolumn_dfs(m, jj, perm_r, &nseg, &panel_lsub[k],
					segrep, &repfnz[k], xprune, marker,
					parent, xplore, &Glu)) != 0) {
			if ( iperm_r_allocated ) SUPERLU_FREE (iperm_r);
			SUPERLU_FREE (iperm_c);
			SUPERLU_FREE (relax_end);
			return;
		}

	      	/* Numeric updates */
	    	if ((*info = scolumn_bmod(jj, (nseg - nseg1), &dense[k],
					 tempv, &segrep[nseg1], &repfnz[k],
					 jcol, &Glu, stat)) != 0) {
			if ( iperm_r_allocated ) SUPERLU_FREE (iperm_r);
			SUPERLU_FREE (iperm_c);
			SUPERLU_FREE (relax_end);
			return;
		}
		
	        /* Copy the U-segments to ucol[*] */
		if ((*info = scopy_to_ucol(jj, nseg, segrep, &repfnz[k],
					  perm_r, &dense[k], &Glu)) != 0) {
			if ( iperm_r_allocated ) SUPERLU_FREE (iperm_r);
			SUPERLU_FREE (iperm_c);
			SUPERLU_FREE (relax_end);
			return;
		}

	    	if ( (*info = spivotL(jj, diag_pivot_thresh, &usepr, perm_r,
				      iperm_r, iperm_c, &pivrow, &Glu, stat)) )
		    if ( iinfo == 0 ) iinfo = *info;

		/* Prune columns (0:jj-1) using column jj */
	    	spruneL(jj, perm_r, pivrow, nseg, segrep,
                        &repfnz[k], xprune, &Glu);

		/* Reset repfnz[] for this column */
	    	resetrep_col (nseg, segrep, &repfnz[k]);
		
#ifdef DEBUG
		sprint_lu_col("[2]: ", jj, pivrow, xprune, &Glu);
#endif

	    }

   	    jcol += panel_size;	/* Move to the next panel */

	} /* else */

    } /* for */

    *info = iinfo;
    
    if ( m > n ) {
	k = 0;
        for (i = 0; i < m; ++i) 
            if ( perm_r[i] == EMPTY ) {
    		perm_r[i] = n + k;
		++k;
	    }
    }

    countnz(min_mn, xprune, &nnzL, &nnzU, &Glu);
    fixupL(min_mn, perm_r, &Glu);

    sLUWorkFree(iwork, swork, &Glu); /* Free work space and compress storage */

    if ( fact == SamePattern_SameRowPerm ) {
        /* L and U structures may have changed due to possibly different
	   pivoting, even though the storage is available.
	   There could also be memory expansions, so the array locations
           may have changed, */
        ((SCformat *)L->Store)->nnz = nnzL;
	((SCformat *)L->Store)->nsuper = Glu.supno[n];
	((SCformat *)L->Store)->nzval = Glu.lusup;
	((SCformat *)L->Store)->nzval_colptr = Glu.xlusup;
	((SCformat *)L->Store)->rowind = Glu.lsub;
	((SCformat *)L->Store)->rowind_colptr = Glu.xlsub;
	((NCformat *)U->Store)->nnz = nnzU;
	((NCformat *)U->Store)->nzval = Glu.ucol;
	((NCformat *)U->Store)->rowind = Glu.usub;
	((NCformat *)U->Store)->colptr = Glu.xusub;
    } else {
        sCreate_SuperNode_Matrix(L, A->nrow, A->ncol, nnzL, Glu.lusup, 
	                         Glu.xlusup, Glu.lsub, Glu.xlsub, Glu.supno,
			         Glu.xsup, SLU_SC, SLU_S, SLU_TRLU);
    	sCreate_CompCol_Matrix(U, min_mn, min_mn, nnzU, Glu.ucol, 
			       Glu.usub, Glu.xusub, SLU_NC, SLU_S, SLU_TRU);
    }
    
    ops[FACT] += ops[TRSV] + ops[GEMV];	
    
    if ( iperm_r_allocated ) SUPERLU_FREE (iperm_r);
    SUPERLU_FREE (iperm_c);
    SUPERLU_FREE (relax_end);
}
