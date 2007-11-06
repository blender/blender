

/*
 * -- SuperLU routine (version 3.0) --
 * Univ. of California Berkeley, Xerox Palo Alto Research Center,
 * and Lawrence Berkeley National Lab.
 * October 15, 2003
 *
 */
#include "ssp_defs.h"

void
sgssv(superlu_options_t *options, SuperMatrix *A, int *perm_c, int *perm_r,
      SuperMatrix *L, SuperMatrix *U, SuperMatrix *B,
      SuperLUStat_t *stat, int *info )
{
/*
 * Purpose
 * =======
 *
 * SGSSV solves the system of linear equations A*X=B, using the
 * LU factorization from SGSTRF. It performs the following steps:
 *
 *   1. If A is stored column-wise (A->Stype = SLU_NC):
 *
 *      1.1. Permute the columns of A, forming A*Pc, where Pc
 *           is a permutation matrix. For more details of this step, 
 *           see sp_preorder.c.
 *
 *      1.2. Factor A as Pr*A*Pc=L*U with the permutation Pr determined
 *           by Gaussian elimination with partial pivoting.
 *           L is unit lower triangular with offdiagonal entries
 *           bounded by 1 in magnitude, and U is upper triangular.
 *
 *      1.3. Solve the system of equations A*X=B using the factored
 *           form of A.
 *
 *   2. If A is stored row-wise (A->Stype = SLU_NR), apply the
 *      above algorithm to the transpose of A:
 *
 *      2.1. Permute columns of transpose(A) (rows of A),
 *           forming transpose(A)*Pc, where Pc is a permutation matrix. 
 *           For more details of this step, see sp_preorder.c.
 *
 *      2.2. Factor A as Pr*transpose(A)*Pc=L*U with the permutation Pr
 *           determined by Gaussian elimination with partial pivoting.
 *           L is unit lower triangular with offdiagonal entries
 *           bounded by 1 in magnitude, and U is upper triangular.
 *
 *      2.3. Solve the system of equations A*X=B using the factored
 *           form of A.
 *
 *   See supermatrix.h for the definition of 'SuperMatrix' structure.
 * 
 * Arguments
 * =========
 *
 * options (input) superlu_options_t*
 *         The structure defines the input parameters to control
 *         how the LU decomposition will be performed and how the
 *         system will be solved.
 *
 * A       (input) SuperMatrix*
 *         Matrix A in A*X=B, of dimension (A->nrow, A->ncol). The number
 *         of linear equations is A->nrow. Currently, the type of A can be:
 *         Stype = SLU_NC or SLU_NR; Dtype = SLU_S; Mtype = SLU_GE.
 *         In the future, more general A may be handled.
 *
 * perm_c  (input/output) int*
 *         If A->Stype = SLU_NC, column permutation vector of size A->ncol
 *         which defines the permutation matrix Pc; perm_c[i] = j means 
 *         column i of A is in position j in A*Pc.
 *         If A->Stype = SLU_NR, column permutation vector of size A->nrow
 *         which describes permutation of columns of transpose(A) 
 *         (rows of A) as described above.
 * 
 *         If options->ColPerm = MY_PERMC or options->Fact = SamePattern or
 *            options->Fact = SamePattern_SameRowPerm, it is an input argument.
 *            On exit, perm_c may be overwritten by the product of the input
 *            perm_c and a permutation that postorders the elimination tree
 *            of Pc'*A'*A*Pc; perm_c is not changed if the elimination tree
 *            is already in postorder.
 *         Otherwise, it is an output argument.
 * 
 * perm_r  (input/output) int*
 *         If A->Stype = SLU_NC, row permutation vector of size A->nrow, 
 *         which defines the permutation matrix Pr, and is determined 
 *         by partial pivoting.  perm_r[i] = j means row i of A is in 
 *         position j in Pr*A.
 *         If A->Stype = SLU_NR, permutation vector of size A->ncol, which
 *         determines permutation of rows of transpose(A)
 *         (columns of A) as described above.
 *
 *         If options->RowPerm = MY_PERMR or
 *            options->Fact = SamePattern_SameRowPerm, perm_r is an
 *            input argument.
 *         otherwise it is an output argument.
 *
 * L       (output) SuperMatrix*
 *         The factor L from the factorization 
 *             Pr*A*Pc=L*U              (if A->Stype = SLU_NC) or
 *             Pr*transpose(A)*Pc=L*U   (if A->Stype = SLU_NR).
 *         Uses compressed row subscripts storage for supernodes, i.e.,
 *         L has types: Stype = SLU_SC, Dtype = SLU_S, Mtype = SLU_TRLU.
 *         
 * U       (output) SuperMatrix*
 *	   The factor U from the factorization 
 *             Pr*A*Pc=L*U              (if A->Stype = SLU_NC) or
 *             Pr*transpose(A)*Pc=L*U   (if A->Stype = SLU_NR).
 *         Uses column-wise storage scheme, i.e., U has types:
 *         Stype = SLU_NC, Dtype = SLU_S, Mtype = SLU_TRU.
 *
 * B       (input/output) SuperMatrix*
 *         B has types: Stype = SLU_DN, Dtype = SLU_S, Mtype = SLU_GE.
 *         On entry, the right hand side matrix.
 *         On exit, the solution matrix if info = 0;
 *
 * stat   (output) SuperLUStat_t*
 *        Record the statistics on runtime and floating-point operation count.
 *        See util.h for the definition of 'SuperLUStat_t'.
 *
 * info    (output) int*
 *	   = 0: successful exit
 *         > 0: if info = i, and i is
 *             <= A->ncol: U(i,i) is exactly zero. The factorization has
 *                been completed, but the factor U is exactly singular,
 *                so the solution could not be computed.
 *             > A->ncol: number of bytes allocated when memory allocation
 *                failure occurred, plus A->ncol.
 *   
 */
    DNformat *Bstore;
    SuperMatrix *AA = NULL;/* A in SLU_NC format used by the factorization routine.*/
    SuperMatrix AC; /* Matrix postmultiplied by Pc */
    int      lwork = 0, *etree, i;
    
    /* Set default values for some parameters */
    int      panel_size;     /* panel size */
    int      relax;          /* no of columns in a relaxed snodes */
    int      permc_spec;
    trans_t  trans = NOTRANS;
    double   *utime;
    double   t;	/* Temporary time */

    /* Test the input parameters ... */
    *info = 0;
    Bstore = B->Store;
    if ( options->Fact != DOFACT ) *info = -1;
    else if ( A->nrow != A->ncol || A->nrow < 0 ||
	 (A->Stype != SLU_NC && A->Stype != SLU_NR) ||
	 A->Dtype != SLU_S || A->Mtype != SLU_GE )
	*info = -2;
    else if ( B->ncol < 0 || Bstore->lda < SUPERLU_MAX(0, A->nrow) ||
	B->Stype != SLU_DN || B->Dtype != SLU_S || B->Mtype != SLU_GE )
	*info = -7;
    if ( *info != 0 ) {
	i = -(*info);
	xerbla_("sgssv", &i);
	return;
    }

    utime = stat->utime;

    /* Convert A to SLU_NC format when necessary. */
    if ( A->Stype == SLU_NR ) {
	NRformat *Astore = A->Store;
	AA = (SuperMatrix *) SUPERLU_MALLOC( sizeof(SuperMatrix) );
	sCreate_CompCol_Matrix(AA, A->ncol, A->nrow, Astore->nnz, 
			       Astore->nzval, Astore->colind, Astore->rowptr,
			       SLU_NC, A->Dtype, A->Mtype);
	trans = TRANS;
    } else {
        if ( A->Stype == SLU_NC ) AA = A;
    }

    t = SuperLU_timer_();
    /*
     * Get column permutation vector perm_c[], according to permc_spec:
     *   permc_spec = NATURAL:  natural ordering 
     *   permc_spec = MMD_AT_PLUS_A: minimum degree on structure of A'+A
     *   permc_spec = MMD_ATA:  minimum degree on structure of A'*A
     *   permc_spec = COLAMD:   approximate minimum degree column ordering
     *   permc_spec = MY_PERMC: the ordering already supplied in perm_c[]
     */
    permc_spec = options->ColPerm;
    if ( permc_spec != MY_PERMC && options->Fact == DOFACT )
      get_perm_c(permc_spec, AA, perm_c);
    utime[COLPERM] = SuperLU_timer_() - t;

    etree = intMalloc(A->ncol);

    t = SuperLU_timer_();
    sp_preorder(options, AA, perm_c, etree, &AC);
    utime[ETREE] = SuperLU_timer_() - t;

    panel_size = sp_ienv(1);
    relax = sp_ienv(2);

    /*printf("Factor PA = LU ... relax %d\tw %d\tmaxsuper %d\trowblk %d\n", 
	  relax, panel_size, sp_ienv(3), sp_ienv(4));*/
    t = SuperLU_timer_(); 
    /* Compute the LU factorization of A. */
    sgstrf(options, &AC, relax, panel_size,
	   etree, NULL, lwork, perm_c, perm_r, L, U, stat, info);
    utime[FACT] = SuperLU_timer_() - t;

    t = SuperLU_timer_();
    if ( *info == 0 ) {
        /* Solve the system A*X=B, overwriting B with X. */
        sgstrs (trans, L, U, perm_c, perm_r, B, stat, info);
    }
    utime[SOLVE] = SuperLU_timer_() - t;

    SUPERLU_FREE (etree);
    Destroy_CompCol_Permuted(&AC);
    if ( A->Stype == SLU_NR ) {
	Destroy_SuperMatrix_Store(AA);
	SUPERLU_FREE(AA);
    }

}
