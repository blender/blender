
/*  Elimination tree computation and layout routines */

#include <stdio.h>
#include <stdlib.h>
#include "ssp_defs.h"

/* 
 *  Implementation of disjoint set union routines.
 *  Elements are integers in 0..n-1, and the 
 *  names of the sets themselves are of type int.
 *  
 *  Calls are:
 *  initialize_disjoint_sets (n) initial call.
 *  s = make_set (i)             returns a set containing only i.
 *  s = link (t, u)		 returns s = t union u, destroying t and u.
 *  s = find (i)		 return name of set containing i.
 *  finalize_disjoint_sets 	 final call.
 *
 *  This implementation uses path compression but not weighted union.
 *  See Tarjan's book for details.
 *  John Gilbert, CMI, 1987.
 *
 *  Implemented path-halving by XSL 07/05/95.
 */

static int	*pp;		/* parent array for sets */

static 
int *mxCallocInt(int n)
{
    register int i;
    int *buf;

    buf = (int *) SUPERLU_MALLOC( n * sizeof(int) );
    if ( !buf ) {
         ABORT("SUPERLU_MALLOC fails for buf in mxCallocInt()");
       }
    for (i = 0; i < n; i++) buf[i] = 0;
    return (buf);
}
      
static
void initialize_disjoint_sets (
	int n
	)
{
	pp = mxCallocInt(n);
}


static
int make_set (
	int i
	)
{
	pp[i] = i;
	return i;
}


static
int link (
	int s,
	int t
	)
{
	pp[s] = t;
	return t;
}


/* PATH HALVING */
static
int find (int i)
{
    register int p, gp;
    
    p = pp[i];
    gp = pp[p];
    while (gp != p) {
	pp[i] = gp;
	i = gp;
	p = pp[i];
	gp = pp[p];
    }
    return (p);
}

#if 0
/* PATH COMPRESSION */
static
int find (
	int i
	)
{
	if (pp[i] != i) 
		pp[i] = find (pp[i]);
	return pp[i];
}
#endif

static
void finalize_disjoint_sets (
	void
	)
{
	SUPERLU_FREE(pp);
}


/*
 *      Find the elimination tree for A'*A.
 *      This uses something similar to Liu's algorithm. 
 *      It runs in time O(nz(A)*log n) and does not form A'*A.
 *
 *      Input:
 *        Sparse matrix A.  Numeric values are ignored, so any
 *        explicit zeros are treated as nonzero.
 *      Output:
 *        Integer array of parents representing the elimination
 *        tree of the symbolic product A'*A.  Each vertex is a
 *        column of A, and nc means a root of the elimination forest.
 *
 *      John R. Gilbert, Xerox, 10 Dec 1990
 *      Based on code by JRG dated 1987, 1988, and 1990.
 */

/*
 * Nonsymmetric elimination tree
 */
int
sp_coletree(
	    int *acolst, int *acolend, /* column start and end past 1 */
	    int *arow,                 /* row indices of A */
	    int nr, int nc,            /* dimension of A */
	    int *parent	               /* parent in elim tree */
	    )
{
	int	*root;			/* root of subtee of etree 	*/
	int     *firstcol;		/* first nonzero col in each row*/
	int	rset, cset;             
	int	row, col;
	int	rroot;
	int	p;

	root = mxCallocInt (nc);
	initialize_disjoint_sets (nc);

	/* Compute firstcol[row] = first nonzero column in row */

	firstcol = mxCallocInt (nr);
	for (row = 0; row < nr; firstcol[row++] = nc);
	for (col = 0; col < nc; col++) 
		for (p = acolst[col]; p < acolend[col]; p++) {
			row = arow[p];
			firstcol[row] = SUPERLU_MIN(firstcol[row], col);
		}

	/* Compute etree by Liu's algorithm for symmetric matrices,
           except use (firstcol[r],c) in place of an edge (r,c) of A.
	   Thus each row clique in A'*A is replaced by a star
	   centered at its first vertex, which has the same fill. */

	for (col = 0; col < nc; col++) {
		cset = make_set (col);
		root[cset] = col;
		parent[col] = nc; /* Matlab */
		for (p = acolst[col]; p < acolend[col]; p++) {
			row = firstcol[arow[p]];
			if (row >= col) continue;
			rset = find (row);
			rroot = root[rset];
			if (rroot != col) {
				parent[rroot] = col;
				cset = link (cset, rset);
				root[cset] = col;
			}
		}
	}

	SUPERLU_FREE (root);
	SUPERLU_FREE (firstcol);
	finalize_disjoint_sets ();
	return 0;
}

/*
 *  q = TreePostorder (n, p);
 *
 *	Postorder a tree.
 *	Input:
 *	  p is a vector of parent pointers for a forest whose
 *        vertices are the integers 0 to n-1; p[root]==n.
 *	Output:
 *	  q is a vector indexed by 0..n-1 such that q[i] is the
 *	  i-th vertex in a postorder numbering of the tree.
 *
 *        ( 2/7/95 modified by X.Li:
 *          q is a vector indexed by 0:n-1 such that vertex i is the
 *          q[i]-th vertex in a postorder numbering of the tree.
 *          That is, this is the inverse of the previous q. )
 *
 *	In the child structure, lower-numbered children are represented
 *	first, so that a tree which is already numbered in postorder
 *	will not have its order changed.
 *    
 *  Written by John Gilbert, Xerox, 10 Dec 1990.
 *  Based on code written by John Gilbert at CMI in 1987.
 */

static int	*first_kid, *next_kid;	/* Linked list of children.	*/
static int	*post, postnum;

static
/*
 * Depth-first search from vertex v.
 */
void etdfs (
	int	v
	)
{
	int	w;

	for (w = first_kid[v]; w != -1; w = next_kid[w]) {
		etdfs (w);
	}
	/* post[postnum++] = v; in Matlab */
	post[v] = postnum++;    /* Modified by X.Li on 2/14/95 */
}


/*
 * Post order a tree
 */
int *TreePostorder(
	int n,
	int *parent
)
{
	int	v, dad;

	/* Allocate storage for working arrays and results	*/
	first_kid = 	mxCallocInt (n+1);
	next_kid  = 	mxCallocInt (n+1);
	post	  = 	mxCallocInt (n+1);

	/* Set up structure describing children */
	for (v = 0; v <= n; first_kid[v++] = -1);
	for (v = n-1; v >= 0; v--) {
		dad = parent[v];
		next_kid[v] = first_kid[dad];
		first_kid[dad] = v;
	}

	/* Depth-first search from dummy root vertex #n */
	postnum = 0;
	etdfs (n);

	SUPERLU_FREE (first_kid);
	SUPERLU_FREE (next_kid);
	return post;
}


/*
 *      p = spsymetree (A);
 *
 *      Find the elimination tree for symmetric matrix A.
 *      This uses Liu's algorithm, and runs in time O(nz*log n).
 *
 *      Input:
 *        Square sparse matrix A.  No check is made for symmetry;
 *        elements below and on the diagonal are ignored.
 *        Numeric values are ignored, so any explicit zeros are 
 *        treated as nonzero.
 *      Output:
 *        Integer array of parents representing the etree, with n
 *        meaning a root of the elimination forest.
 *      Note:  
 *        This routine uses only the upper triangle, while sparse
 *        Cholesky (as in spchol.c) uses only the lower.  Matlab's
 *        dense Cholesky uses only the upper.  This routine could
 *        be modified to use the lower triangle either by transposing
 *        the matrix or by traversing it by rows with auxiliary
 *        pointer and link arrays.
 *
 *      John R. Gilbert, Xerox, 10 Dec 1990
 *      Based on code by JRG dated 1987, 1988, and 1990.
 *      Modified by X.S. Li, November 1999.
 */

/*
 * Symmetric elimination tree
 */
int
sp_symetree(
	    int *acolst, int *acolend, /* column starts and ends past 1 */
	    int *arow,            /* row indices of A */
	    int n,                /* dimension of A */
	    int *parent	    /* parent in elim tree */
	    )
{
	int	*root;		    /* root of subtree of etree 	*/
	int	rset, cset;             
	int	row, col;
	int	rroot;
	int	p;

	root = mxCallocInt (n);
	initialize_disjoint_sets (n);

	for (col = 0; col < n; col++) {
		cset = make_set (col);
		root[cset] = col;
		parent[col] = n; /* Matlab */
		for (p = acolst[col]; p < acolend[col]; p++) {
			row = arow[p];
			if (row >= col) continue;
			rset = find (row);
			rroot = root[rset];
			if (rroot != col) {
				parent[rroot] = col;
				cset = link (cset, rset);
				root[cset] = col;
			}
		}
	}
	SUPERLU_FREE (root);
	finalize_disjoint_sets ();
	return 0;
} /* SP_SYMETREE */
