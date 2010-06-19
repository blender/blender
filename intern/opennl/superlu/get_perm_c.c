/*
 * -- SuperLU routine (version 2.0) --
 * Univ. of California Berkeley, Xerox Palo Alto Research Center,
 * and Lawrence Berkeley National Lab.
 * November 15, 1997
 *
 */

#include "ssp_defs.h"
#include "colamd.h"

extern int  genmmd_(int *, int *, int *, int *, int *, int *, int *, 
		    int *, int *, int *, int *, int *);

static void
get_colamd(
	   const int m,  /* number of rows in matrix A. */
	   const int n,  /* number of columns in matrix A. */
	   const int nnz,/* number of nonzeros in matrix A. */
	   int *colptr,  /* column pointer of size n+1 for matrix A. */
	   int *rowind,  /* row indices of size nz for matrix A. */
	   int *perm_c   /* out - the column permutation vector. */
	   )
{
    int Alen, *A, i, info, *p;
    double *knobs;

    Alen = colamd_recommended(nnz, m, n);

    if ( !(knobs = (double *) SUPERLU_MALLOC(COLAMD_KNOBS * sizeof(double))) )
        ABORT("Malloc fails for knobs");
    colamd_set_defaults(knobs);

    if (!(A = (int *) SUPERLU_MALLOC(Alen * sizeof(int))) )
        ABORT("Malloc fails for A[]");
    if (!(p = (int *) SUPERLU_MALLOC((n+1) * sizeof(int))) )
        ABORT("Malloc fails for p[]");
    for (i = 0; i <= n; ++i) p[i] = colptr[i];
    for (i = 0; i < nnz; ++i) A[i] = rowind[i];
    info = colamd(m, n, Alen, A, p, knobs);
    if ( info == FALSE ) ABORT("COLAMD failed");

    for (i = 0; i < n; ++i) perm_c[p[i]] = i;

    SUPERLU_FREE(knobs);
    SUPERLU_FREE(A);
    SUPERLU_FREE(p);
}

static void
getata(
       const int m,      /* number of rows in matrix A. */
       const int n,      /* number of columns in matrix A. */
       const int nz,     /* number of nonzeros in matrix A */
       int *colptr,      /* column pointer of size n+1 for matrix A. */
       int *rowind,      /* row indices of size nz for matrix A. */
       int *atanz,       /* out - on exit, returns the actual number of
                            nonzeros in matrix A'*A. */
       int **ata_colptr, /* out - size n+1 */
       int **ata_rowind  /* out - size *atanz */
       )
/*
 * Purpose
 * =======
 *
 * Form the structure of A'*A. A is an m-by-n matrix in column oriented
 * format represented by (colptr, rowind). The output A'*A is in column
 * oriented format (symmetrically, also row oriented), represented by
 * (ata_colptr, ata_rowind).
 *
 * This routine is modified from GETATA routine by Tim Davis.
 * The complexity of this algorithm is: SUM_{i=1,m} r(i)^2,
 * i.e., the sum of the square of the row counts.
 *
 * Questions
 * =========
 *     o  Do I need to withhold the *dense* rows?
 *     o  How do I know the number of nonzeros in A'*A?
 * 
 */
{
    register int i, j, k, col, num_nz, ti, trow;
    int *marker, *b_colptr, *b_rowind;
    int *t_colptr, *t_rowind; /* a column oriented form of T = A' */

    if ( !(marker = (int*) SUPERLU_MALLOC((SUPERLU_MAX(m,n)+1)*sizeof(int))) )
	ABORT("SUPERLU_MALLOC fails for marker[]");
    if ( !(t_colptr = (int*) SUPERLU_MALLOC((m+1) * sizeof(int))) )
	ABORT("SUPERLU_MALLOC t_colptr[]");
    if ( !(t_rowind = (int*) SUPERLU_MALLOC(nz * sizeof(int))) )
	ABORT("SUPERLU_MALLOC fails for t_rowind[]");

    
    /* Get counts of each column of T, and set up column pointers */
    for (i = 0; i < m; ++i) marker[i] = 0;
    for (j = 0; j < n; ++j) {
	for (i = colptr[j]; i < colptr[j+1]; ++i)
	    ++marker[rowind[i]];
    }
    t_colptr[0] = 0;
    for (i = 0; i < m; ++i) {
	t_colptr[i+1] = t_colptr[i] + marker[i];
	marker[i] = t_colptr[i];
    }

    /* Transpose the matrix from A to T */
    for (j = 0; j < n; ++j)
	for (i = colptr[j]; i < colptr[j+1]; ++i) {
	    col = rowind[i];
	    t_rowind[marker[col]] = j;
	    ++marker[col];
	}

    
    /* ----------------------------------------------------------------
       compute B = T * A, where column j of B is:

       Struct (B_*j) =    UNION   ( Struct (T_*k) )
                        A_kj != 0

       do not include the diagonal entry
   
       ( Partition A as: A = (A_*1, ..., A_*n)
         Then B = T * A = (T * A_*1, ..., T * A_*n), where
         T * A_*j = (T_*1, ..., T_*m) * A_*j.  )
       ---------------------------------------------------------------- */

    /* Zero the diagonal flag */
    for (i = 0; i < n; ++i) marker[i] = -1;

    /* First pass determines number of nonzeros in B */
    num_nz = 0;
    for (j = 0; j < n; ++j) {
	/* Flag the diagonal so it's not included in the B matrix */
	marker[j] = j;

	for (i = colptr[j]; i < colptr[j+1]; ++i) {
	    /* A_kj is nonzero, add pattern of column T_*k to B_*j */
	    k = rowind[i];
	    for (ti = t_colptr[k]; ti < t_colptr[k+1]; ++ti) {
		trow = t_rowind[ti];
		if ( marker[trow] != j ) {
		    marker[trow] = j;
		    num_nz++;
		}
	    }
	}
    }
    *atanz = num_nz;
    
    /* Allocate storage for A'*A */
    if ( !(*ata_colptr = (int*) SUPERLU_MALLOC( (n+1) * sizeof(int)) ) )
	ABORT("SUPERLU_MALLOC fails for ata_colptr[]");
    if ( *atanz ) {
	if ( !(*ata_rowind = (int*) SUPERLU_MALLOC( *atanz * sizeof(int)) ) )
	    ABORT("SUPERLU_MALLOC fails for ata_rowind[]");
    }
    b_colptr = *ata_colptr; /* aliasing */
    b_rowind = *ata_rowind;
    
    /* Zero the diagonal flag */
    for (i = 0; i < n; ++i) marker[i] = -1;
    
    /* Compute each column of B, one at a time */
    num_nz = 0;
    for (j = 0; j < n; ++j) {
	b_colptr[j] = num_nz;
	
	/* Flag the diagonal so it's not included in the B matrix */
	marker[j] = j;

	for (i = colptr[j]; i < colptr[j+1]; ++i) {
	    /* A_kj is nonzero, add pattern of column T_*k to B_*j */
	    k = rowind[i];
	    for (ti = t_colptr[k]; ti < t_colptr[k+1]; ++ti) {
		trow = t_rowind[ti];
		if ( marker[trow] != j ) {
		    marker[trow] = j;
		    b_rowind[num_nz++] = trow;
		}
	    }
	}
    }
    b_colptr[n] = num_nz;
       
    SUPERLU_FREE(marker);
    SUPERLU_FREE(t_colptr);
    SUPERLU_FREE(t_rowind);
}


static void
at_plus_a(
	  const int n,      /* number of columns in matrix A. */
	  const int nz,     /* number of nonzeros in matrix A */
	  int *colptr,      /* column pointer of size n+1 for matrix A. */
	  int *rowind,      /* row indices of size nz for matrix A. */
	  int *bnz,         /* out - on exit, returns the actual number of
                               nonzeros in matrix A'*A. */
	  int **b_colptr,   /* out - size n+1 */
	  int **b_rowind    /* out - size *bnz */
	  )
{
/*
 * Purpose
 * =======
 *
 * Form the structure of A'+A. A is an n-by-n matrix in column oriented
 * format represented by (colptr, rowind). The output A'+A is in column
 * oriented format (symmetrically, also row oriented), represented by
 * (b_colptr, b_rowind).
 *
 */
    register int i, j, k, col, num_nz;
    int *t_colptr, *t_rowind; /* a column oriented form of T = A' */
    int *marker;

    if ( !(marker = (int*) SUPERLU_MALLOC( n * sizeof(int)) ) )
	ABORT("SUPERLU_MALLOC fails for marker[]");
    if ( !(t_colptr = (int*) SUPERLU_MALLOC( (n+1) * sizeof(int)) ) )
	ABORT("SUPERLU_MALLOC fails for t_colptr[]");
    if ( !(t_rowind = (int*) SUPERLU_MALLOC( nz * sizeof(int)) ) )
	ABORT("SUPERLU_MALLOC fails t_rowind[]");

    
    /* Get counts of each column of T, and set up column pointers */
    for (i = 0; i < n; ++i) marker[i] = 0;
    for (j = 0; j < n; ++j) {
	for (i = colptr[j]; i < colptr[j+1]; ++i)
	    ++marker[rowind[i]];
    }
    t_colptr[0] = 0;
    for (i = 0; i < n; ++i) {
	t_colptr[i+1] = t_colptr[i] + marker[i];
	marker[i] = t_colptr[i];
    }

    /* Transpose the matrix from A to T */
    for (j = 0; j < n; ++j)
	for (i = colptr[j]; i < colptr[j+1]; ++i) {
	    col = rowind[i];
	    t_rowind[marker[col]] = j;
	    ++marker[col];
	}


    /* ----------------------------------------------------------------
       compute B = A + T, where column j of B is:

       Struct (B_*j) = Struct (A_*k) UNION Struct (T_*k)

       do not include the diagonal entry
       ---------------------------------------------------------------- */

    /* Zero the diagonal flag */
    for (i = 0; i < n; ++i) marker[i] = -1;

    /* First pass determines number of nonzeros in B */
    num_nz = 0;
    for (j = 0; j < n; ++j) {
	/* Flag the diagonal so it's not included in the B matrix */
	marker[j] = j;

	/* Add pattern of column A_*k to B_*j */
	for (i = colptr[j]; i < colptr[j+1]; ++i) {
	    k = rowind[i];
	    if ( marker[k] != j ) {
		marker[k] = j;
		++num_nz;
	    }
	}

	/* Add pattern of column T_*k to B_*j */
	for (i = t_colptr[j]; i < t_colptr[j+1]; ++i) {
	    k = t_rowind[i];
	    if ( marker[k] != j ) {
		marker[k] = j;
		++num_nz;
	    }
	}
    }
    *bnz = num_nz;
    
    /* Allocate storage for A+A' */
    if ( !(*b_colptr = (int*) SUPERLU_MALLOC( (n+1) * sizeof(int)) ) )
	ABORT("SUPERLU_MALLOC fails for b_colptr[]");
    if ( *bnz) {
      if ( !(*b_rowind = (int*) SUPERLU_MALLOC( *bnz * sizeof(int)) ) )
	ABORT("SUPERLU_MALLOC fails for b_rowind[]");
    }
    
    /* Zero the diagonal flag */
    for (i = 0; i < n; ++i) marker[i] = -1;
    
    /* Compute each column of B, one at a time */
    num_nz = 0;
    for (j = 0; j < n; ++j) {
	(*b_colptr)[j] = num_nz;
	
	/* Flag the diagonal so it's not included in the B matrix */
	marker[j] = j;

	/* Add pattern of column A_*k to B_*j */
	for (i = colptr[j]; i < colptr[j+1]; ++i) {
	    k = rowind[i];
	    if ( marker[k] != j ) {
		marker[k] = j;
		(*b_rowind)[num_nz++] = k;
	    }
	}

	/* Add pattern of column T_*k to B_*j */
	for (i = t_colptr[j]; i < t_colptr[j+1]; ++i) {
	    k = t_rowind[i];
	    if ( marker[k] != j ) {
		marker[k] = j;
		(*b_rowind)[num_nz++] = k;
	    }
	}
    }
    (*b_colptr)[n] = num_nz;
       
    SUPERLU_FREE(marker);
    SUPERLU_FREE(t_colptr);
    SUPERLU_FREE(t_rowind);
}

void
get_perm_c(int ispec, SuperMatrix *A, int *perm_c)
/*
 * Purpose
 * =======
 *
 * GET_PERM_C obtains a permutation matrix Pc, by applying the multiple
 * minimum degree ordering code by Joseph Liu to matrix A'*A or A+A'.
 * or using approximate minimum degree column ordering by Davis et. al.
 * The LU factorization of A*Pc tends to have less fill than the LU 
 * factorization of A.
 *
 * Arguments
 * =========
 *
 * ispec   (input) int
 *         Specifies the type of column ordering to reduce fill:
 *         = 1: minimum degree on the structure of A^T * A
 *         = 2: minimum degree on the structure of A^T + A
 *         = 3: approximate minimum degree for unsymmetric matrices
 *         If ispec == 0, the natural ordering (i.e., Pc = I) is returned.
 * 
 * A       (input) SuperMatrix*
 *         Matrix A in A*X=B, of dimension (A->nrow, A->ncol). The number
 *         of the linear equations is A->nrow. Currently, the type of A 
 *         can be: Stype = NC; Dtype = _D; Mtype = GE. In the future,
 *         more general A can be handled.
 *
 * perm_c  (output) int*
 *	   Column permutation vector of size A->ncol, which defines the 
 *         permutation matrix Pc; perm_c[i] = j means column i of A is 
 *         in position j in A*Pc.
 *
 */
{
    NCformat *Astore = A->Store;
    int m, n, bnz, *b_colptr, i;
    int delta, maxint, nofsub, *invp;
    int *b_rowind, *dhead, *qsize, *llist, *marker;
    double t, SuperLU_timer_();
    
    /* make gcc happy */
    b_rowind=NULL;
    b_colptr=NULL;
    
    m = A->nrow;
    n = A->ncol;

    t = SuperLU_timer_();
    switch ( ispec ) {
        case 0: /* Natural ordering */
	      for (i = 0; i < n; ++i) perm_c[i] = i;
#if ( PRNTlevel>=1 )
	      printf("Use natural column ordering.\n");
#endif
	      return;
        case 1: /* Minimum degree ordering on A'*A */
	      getata(m, n, Astore->nnz, Astore->colptr, Astore->rowind,
		     &bnz, &b_colptr, &b_rowind);
#if ( PRNTlevel>=1 )
	      printf("Use minimum degree ordering on A'*A.\n");
#endif
	      t = SuperLU_timer_() - t;
	      /*printf("Form A'*A time = %8.3f\n", t);*/
	      break;
        case 2: /* Minimum degree ordering on A'+A */
	      if ( m != n ) ABORT("Matrix is not square");
	      at_plus_a(n, Astore->nnz, Astore->colptr, Astore->rowind,
			&bnz, &b_colptr, &b_rowind);
#if ( PRNTlevel>=1 )
	      printf("Use minimum degree ordering on A'+A.\n");
#endif
	      t = SuperLU_timer_() - t;
	      /*printf("Form A'+A time = %8.3f\n", t);*/
	      break;
        case 3: /* Approximate minimum degree column ordering. */
	      get_colamd(m, n, Astore->nnz, Astore->colptr, Astore->rowind,
			 perm_c);
#if ( PRNTlevel>=1 )
	      printf(".. Use approximate minimum degree column ordering.\n");
#endif
	      return; 
        default:
	      ABORT("Invalid ISPEC");
		  return;
    }

    if ( bnz != 0 ) {
	t = SuperLU_timer_();

	/* Initialize and allocate storage for GENMMD. */
	delta = 1; /* DELTA is a parameter to allow the choice of nodes
		      whose degree <= min-degree + DELTA. */
	maxint = 2147483647; /* 2**31 - 1 */
	invp = (int *) SUPERLU_MALLOC((n+delta)*sizeof(int));
	if ( !invp ) ABORT("SUPERLU_MALLOC fails for invp.");
	dhead = (int *) SUPERLU_MALLOC((n+delta)*sizeof(int));
	if ( !dhead ) ABORT("SUPERLU_MALLOC fails for dhead.");
	qsize = (int *) SUPERLU_MALLOC((n+delta)*sizeof(int));
	if ( !qsize ) ABORT("SUPERLU_MALLOC fails for qsize.");
	llist = (int *) SUPERLU_MALLOC(n*sizeof(int));
	if ( !llist ) ABORT("SUPERLU_MALLOC fails for llist.");
	marker = (int *) SUPERLU_MALLOC(n*sizeof(int));
	if ( !marker ) ABORT("SUPERLU_MALLOC fails for marker.");

	/* Transform adjacency list into 1-based indexing required by GENMMD.*/
	for (i = 0; i <= n; ++i) ++b_colptr[i];
	for (i = 0; i < bnz; ++i) ++b_rowind[i];
	
	genmmd_(&n, b_colptr, b_rowind, perm_c, invp, &delta, dhead, 
		qsize, llist, marker, &maxint, &nofsub);

	/* Transform perm_c into 0-based indexing. */
	for (i = 0; i < n; ++i) --perm_c[i];

	SUPERLU_FREE(b_colptr);
	SUPERLU_FREE(b_rowind);
	SUPERLU_FREE(invp);
	SUPERLU_FREE(dhead);
	SUPERLU_FREE(qsize);
	SUPERLU_FREE(llist);
	SUPERLU_FREE(marker);

	t = SuperLU_timer_() - t;
	/*  printf("call GENMMD time = %8.3f\n", t);*/

    } else { /* Empty adjacency structure */
	for (i = 0; i < n; ++i) perm_c[i] = i;
    }

}
