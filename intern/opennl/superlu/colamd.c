/* ========================================================================== */
/* === colamd - a sparse matrix column ordering algorithm =================== */
/* ========================================================================== */

/*
    colamd:  An approximate minimum degree column ordering algorithm.

    Purpose:

	Colamd computes a permutation Q such that the Cholesky factorization of
	(AQ)'(AQ) has less fill-in and requires fewer floating point operations
	than A'A.  This also provides a good ordering for sparse partial
	pivoting methods, P(AQ) = LU, where Q is computed prior to numerical
	factorization, and P is computed during numerical factorization via
	conventional partial pivoting with row interchanges.  Colamd is the
	column ordering method used in SuperLU, part of the ScaLAPACK library.
	It is also available as user-contributed software for Matlab 5.2,
	available from MathWorks, Inc. (http://www.mathworks.com).  This
	routine can be used in place of COLMMD in Matlab.  By default, the \
	and / operators in Matlab perform a column ordering (using COLMMD)
	prior to LU factorization using sparse partial pivoting, in the
	built-in Matlab LU(A) routine.

    Authors:

	The authors of the code itself are Stefan I. Larimore and Timothy A.
	Davis (davis@cise.ufl.edu), University of Florida.  The algorithm was
	developed in collaboration with John Gilbert, Xerox PARC, and Esmond
	Ng, Oak Ridge National Laboratory.

    Date:

	August 3, 1998.  Version 1.0.

    Acknowledgements:

	This work was supported by the National Science Foundation, under
	grants DMS-9504974 and DMS-9803599.

    Notice:

	Copyright (c) 1998 by the University of Florida.  All Rights Reserved.

	THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY
	EXPRESSED OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.

	Permission is hereby granted to use or copy this program for any
	purpose, provided the above notices are retained on all copies.
	User documentation of any code that uses this code must cite the
	Authors, the Copyright, and "Used by permission."  If this code is
	accessible from within Matlab, then typing "help colamd" or "colamd"
	(with no arguments) must cite the Authors.  Permission to modify the
	code and to distribute modified code is granted, provided the above
	notices are retained, and a notice that the code was modified is
	included with the above copyright notice.  You must also retain the
	Availability information below, of the original version.

	This software is provided free of charge.

    Availability:

	This file is located at

		http://www.cise.ufl.edu/~davis/colamd/colamd.c

	The colamd.h file is required, located in the same directory.
	The colamdmex.c file provides a Matlab interface for colamd.
	The symamdmex.c file provides a Matlab interface for symamd, which is
	a symmetric ordering based on this code, colamd.c.  All codes are
	purely ANSI C compliant (they use no Unix-specific routines, include
	files, etc.).
*/

/* ========================================================================== */
/* === Description of user-callable routines ================================ */
/* ========================================================================== */

/*
    Each user-callable routine (declared as PUBLIC) is briefly described below.
    Refer to the comments preceding each routine for more details.

    ----------------------------------------------------------------------------
    colamd_recommended:
    ----------------------------------------------------------------------------

	Usage:

	    Alen = colamd_recommended (nnz, n_row, n_col) ;

	Purpose:

	    Returns recommended value of Alen for use by colamd.  Returns -1
	    if any input argument is negative.

	Arguments:

	    int nnz ;		Number of nonzeros in the matrix A.  This must
				be the same value as p [n_col] in the call to
				colamd - otherwise you will get a wrong value
				of the recommended memory to use.
	    int n_row ;		Number of rows in the matrix A.
	    int n_col ;		Number of columns in the matrix A.

    ----------------------------------------------------------------------------
    colamd_set_defaults:
    ----------------------------------------------------------------------------

	Usage:

	    colamd_set_defaults (knobs) ;

	Purpose:

	    Sets the default parameters.

	Arguments:

	    double knobs [COLAMD_KNOBS] ;	Output only.

		Rows with more than (knobs [COLAMD_DENSE_ROW] * n_col) entries
		are removed prior to ordering.  Columns with more than
		(knobs [COLAMD_DENSE_COL] * n_row) entries are removed
		prior to ordering, and placed last in the output column
		ordering.  Default values of these two knobs are both 0.5.
		Currently, only knobs [0] and knobs [1] are used, but future
		versions may use more knobs.  If so, they will be properly set
		to their defaults by the future version of colamd_set_defaults,
		so that the code that calls colamd will not need to change,
		assuming that you either use colamd_set_defaults, or pass a
		(double *) NULL pointer as the knobs array to colamd.

    ----------------------------------------------------------------------------
    colamd:
    ----------------------------------------------------------------------------

	Usage:

	    colamd (n_row, n_col, Alen, A, p, knobs) ;

	Purpose:

	    Computes a column ordering (Q) of A such that P(AQ)=LU or
	    (AQ)'AQ=LL' have less fill-in and require fewer floating point
	    operations than factorizing the unpermuted matrix A or A'A,
	    respectively.

	Arguments:

	    int n_row ;

		Number of rows in the matrix A.
		Restriction:  n_row >= 0.
		Colamd returns FALSE if n_row is negative.

	    int n_col ;

		Number of columns in the matrix A.
		Restriction:  n_col >= 0.
		Colamd returns FALSE if n_col is negative.

	    int Alen ;

		Restriction (see note):
		Alen >= 2*nnz + 6*(n_col+1) + 4*(n_row+1) + n_col + COLAMD_STATS
		Colamd returns FALSE if these conditions are not met.

		Note:  this restriction makes an modest assumption regarding
		the size of the two typedef'd structures, below.  We do,
		however, guarantee that
		Alen >= colamd_recommended (nnz, n_row, n_col)
		will be sufficient.

	    int A [Alen] ;	Input argument, stats on output.

		A is an integer array of size Alen.  Alen must be at least as
		large as the bare minimum value given above, but this is very
		low, and can result in excessive run time.  For best
		performance, we recommend that Alen be greater than or equal to
		colamd_recommended (nnz, n_row, n_col), which adds
		nnz/5 to the bare minimum value given above.

		On input, the row indices of the entries in column c of the
		matrix are held in A [(p [c]) ... (p [c+1]-1)].  The row indices
		in a given column c need not be in ascending order, and
		duplicate row indices may be be present.  However, colamd will
		work a little faster if both of these conditions are met
		(Colamd puts the matrix into this format, if it finds that the
		the conditions are not met).

		The matrix is 0-based.  That is, rows are in the range 0 to
		n_row-1, and columns are in the range 0 to n_col-1.  Colamd
		returns FALSE if any row index is out of range.

		The contents of A are modified during ordering, and are thus
		undefined on output with the exception of a few statistics
		about the ordering (A [0..COLAMD_STATS-1]):
		A [0]:  number of dense or empty rows ignored.
		A [1]:  number of dense or empty columns ignored (and ordered
			last in the output permutation p)
		A [2]:  number of garbage collections performed.
		A [3]:  0, if all row indices in each column were in sorted
			  order, and no duplicates were present.
			1, otherwise (in which case colamd had to do more work)
		Note that a row can become "empty" if it contains only
		"dense" and/or "empty" columns, and similarly a column can
		become "empty" if it only contains "dense" and/or "empty" rows.
		Future versions may return more statistics in A, but the usage
		of these 4 entries in A will remain unchanged.

	    int p [n_col+1] ;	Both input and output argument.

		p is an integer array of size n_col+1.  On input, it holds the
		"pointers" for the column form of the matrix A.  Column c of
		the matrix A is held in A [(p [c]) ... (p [c+1]-1)].  The first
		entry, p [0], must be zero, and p [c] <= p [c+1] must hold
		for all c in the range 0 to n_col-1.  The value p [n_col] is
		thus the total number of entries in the pattern of the matrix A.
		Colamd returns FALSE if these conditions are not met.

		On output, if colamd returns TRUE, the array p holds the column
		permutation (Q, for P(AQ)=LU or (AQ)'(AQ)=LL'), where p [0] is
		the first column index in the new ordering, and p [n_col-1] is
		the last.  That is, p [k] = j means that column j of A is the
		kth pivot column, in AQ, where k is in the range 0 to n_col-1
		(p [0] = j means that column j of A is the first column in AQ).

		If colamd returns FALSE, then no permutation is returned, and
		p is undefined on output.

	    double knobs [COLAMD_KNOBS] ;	Input only.

		See colamd_set_defaults for a description.  If the knobs array
		is not present (that is, if a (double *) NULL pointer is passed
		in its place), then the default values of the parameters are
		used instead.

*/


/* ========================================================================== */
/* === Include files ======================================================== */
/* ========================================================================== */

/* limits.h:  the largest positive integer (INT_MAX) */
#include <limits.h>

/* colamd.h:  knob array size, stats output size, and global prototypes */
#include "colamd.h"

/* ========================================================================== */
/* === Scaffolding code definitions  ======================================== */
/* ========================================================================== */

/* Ensure that debugging is turned off: */
#ifndef NDEBUG
#define NDEBUG
#endif

/* assert.h:  the assert macro (no debugging if NDEBUG is defined) */
#include <assert.h>

/*
   Our "scaffolding code" philosophy:  In our opinion, well-written library
   code should keep its "debugging" code, and just normally have it turned off
   by the compiler so as not to interfere with performance.  This serves
   several purposes:

   (1) assertions act as comments to the reader, telling you what the code
	expects at that point.  All assertions will always be true (unless
	there really is a bug, of course).

   (2) leaving in the scaffolding code assists anyone who would like to modify
	the code, or understand the algorithm (by reading the debugging output,
	one can get a glimpse into what the code is doing).

   (3) (gasp!) for actually finding bugs.  This code has been heavily tested
	and "should" be fully functional and bug-free ... but you never know...

    To enable debugging, comment out the "#define NDEBUG" above.  The code will
    become outrageously slow when debugging is enabled.  To control the level of
    debugging output, set an environment variable D to 0 (little), 1 (some),
    2, 3, or 4 (lots).
*/

/* ========================================================================== */
/* === Row and Column structures ============================================ */
/* ========================================================================== */

typedef struct ColInfo_struct
{
    int start ;		/* index for A of first row in this column, or DEAD */
			/* if column is dead */
    int length ;	/* number of rows in this column */
    union
    {
	int thickness ;	/* number of original columns represented by this */
			/* col, if the column is alive */
	int parent ;	/* parent in parent tree super-column structure, if */
			/* the column is dead */
    } shared1 ;
    union
    {
	int score ;	/* the score used to maintain heap, if col is alive */
	int order ;	/* pivot ordering of this column, if col is dead */
    } shared2 ;
    union
    {
	int headhash ;	/* head of a hash bucket, if col is at the head of */
			/* a degree list */
	int hash ;	/* hash value, if col is not in a degree list */
	int prev ;	/* previous column in degree list, if col is in a */
			/* degree list (but not at the head of a degree list) */
    } shared3 ;
    union
    {
	int degree_next ;	/* next column, if col is in a degree list */
	int hash_next ;		/* next column, if col is in a hash list */
    } shared4 ;

} ColInfo ;

typedef struct RowInfo_struct
{
    int start ;		/* index for A of first col in this row */
    int length ;	/* number of principal columns in this row */
    union
    {
	int degree ;	/* number of principal & non-principal columns in row */
	int p ;		/* used as a row pointer in init_rows_cols () */
    } shared1 ;
    union
    {
	int mark ;	/* for computing set differences and marking dead rows*/
	int first_column ;/* first column in row (used in garbage collection) */
    } shared2 ;

} RowInfo ;

/* ========================================================================== */
/* === Definitions ========================================================== */
/* ========================================================================== */

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#define ONES_COMPLEMENT(r) (-(r)-1)

#define TRUE	(1)
#define FALSE	(0)
#define EMPTY	(-1)

/* Row and column status */
#define ALIVE	(0)
#define DEAD	(-1)

/* Column status */
#define DEAD_PRINCIPAL		(-1)
#define DEAD_NON_PRINCIPAL	(-2)

/* Macros for row and column status update and checking. */
#define ROW_IS_DEAD(r)			ROW_IS_MARKED_DEAD (Row[r].shared2.mark)
#define ROW_IS_MARKED_DEAD(row_mark)	(row_mark < ALIVE)
#define ROW_IS_ALIVE(r)			(Row [r].shared2.mark >= ALIVE)
#define COL_IS_DEAD(c)			(Col [c].start < ALIVE)
#define COL_IS_ALIVE(c)			(Col [c].start >= ALIVE)
#define COL_IS_DEAD_PRINCIPAL(c)	(Col [c].start == DEAD_PRINCIPAL)
#define KILL_ROW(r)			{ Row [r].shared2.mark = DEAD ; }
#define KILL_PRINCIPAL_COL(c)		{ Col [c].start = DEAD_PRINCIPAL ; }
#define KILL_NON_PRINCIPAL_COL(c)	{ Col [c].start = DEAD_NON_PRINCIPAL ; }

/* Routines are either PUBLIC (user-callable) or PRIVATE (not user-callable) */
#define PUBLIC
#define PRIVATE static

/* ========================================================================== */
/* === Prototypes of PRIVATE routines ======================================= */
/* ========================================================================== */

PRIVATE int init_rows_cols
(
    int n_row,
    int n_col,
    RowInfo Row [],
    ColInfo Col [],
    int A [],
    int p []
) ;

PRIVATE void init_scoring
(
    int n_row,
    int n_col,
    RowInfo Row [],
    ColInfo Col [],
    int A [],
    int head [],
    double knobs [COLAMD_KNOBS],
    int *p_n_row2,
    int *p_n_col2,
    int *p_max_deg
) ;

PRIVATE int find_ordering
(
    int n_row,
    int n_col,
    int Alen,
    RowInfo Row [],
    ColInfo Col [],
    int A [],
    int head [],
    int n_col2,
    int max_deg,
    int pfree
) ;

PRIVATE void order_children
(
    int n_col,
    ColInfo Col [],
    int p []
) ;

PRIVATE void detect_super_cols
(
#ifndef NDEBUG
    int n_col,
    RowInfo Row [],
#endif
    ColInfo Col [],
    int A [],
    int head [],
    int row_start,
    int row_length
) ;

PRIVATE int garbage_collection
(
    int n_row,
    int n_col,
    RowInfo Row [],
    ColInfo Col [],
    int A [],
    int *pfree
) ;

PRIVATE int clear_mark
(
    int n_row,
    RowInfo Row []
) ;

/* ========================================================================== */
/* === Debugging definitions ================================================ */
/* ========================================================================== */

#ifndef NDEBUG

/* === With debugging ======================================================= */

/* stdlib.h: for getenv and atoi, to get debugging level from environment */
#include <stdlib.h>

/* stdio.h:  for printf (no printing if debugging is turned off) */
#include <stdio.h>

PRIVATE void debug_deg_lists
(
    int n_row,
    int n_col,
    RowInfo Row [],
    ColInfo Col [],
    int head [],
    int min_score,
    int should,
    int max_deg
) ;

PRIVATE void debug_mark
(
    int n_row,
    RowInfo Row [],
    int tag_mark,
    int max_mark
) ;

PRIVATE void debug_matrix
(
    int n_row,
    int n_col,
    RowInfo Row [],
    ColInfo Col [],
    int A []
) ;

PRIVATE void debug_structures
(
    int n_row,
    int n_col,
    RowInfo Row [],
    ColInfo Col [],
    int A [],
    int n_col2
) ;

/* the following is the *ONLY* global variable in this file, and is only */
/* present when debugging */

PRIVATE int debug_colamd ;	/* debug print level */

#define DEBUG0(params) { (void) printf params ; }
#define DEBUG1(params) { if (debug_colamd >= 1) (void) printf params ; }
#define DEBUG2(params) { if (debug_colamd >= 2) (void) printf params ; }
#define DEBUG3(params) { if (debug_colamd >= 3) (void) printf params ; }
#define DEBUG4(params) { if (debug_colamd >= 4) (void) printf params ; }

#else

/* === No debugging ========================================================= */

#define DEBUG0(params) ;
#define DEBUG1(params) ;
#define DEBUG2(params) ;
#define DEBUG3(params) ;
#define DEBUG4(params) ;

#endif

/* ========================================================================== */


/* ========================================================================== */
/* === USER-CALLABLE ROUTINES: ============================================== */
/* ========================================================================== */


/* ========================================================================== */
/* === colamd_recommended =================================================== */
/* ========================================================================== */

/*
    The colamd_recommended routine returns the suggested size for Alen.  This
    value has been determined to provide good balance between the number of
    garbage collections and the memory requirements for colamd.
*/

PUBLIC int colamd_recommended	/* returns recommended value of Alen. */
(
    /* === Parameters ======================================================= */

    int nnz,			/* number of nonzeros in A */
    int n_row,			/* number of rows in A */
    int n_col			/* number of columns in A */
)
{
    /* === Local variables ================================================== */

    int minimum ;		/* bare minimum requirements */
    int recommended ;		/* recommended value of Alen */

    if (nnz < 0 || n_row < 0 || n_col < 0)
    {
	/* return -1 if any input argument is corrupted */
	DEBUG0 (("colamd_recommended error!")) ;
	DEBUG0 ((" nnz: %d, n_row: %d, n_col: %d\n", nnz, n_row, n_col)) ;
	return (-1) ;
    }

    minimum =
	2 * (nnz)		/* for A */
	+ (((n_col) + 1) * sizeof (ColInfo) / sizeof (int))	/* for Col */
	+ (((n_row) + 1) * sizeof (RowInfo) / sizeof (int))	/* for Row */
	+ n_col			/* minimum elbow room to guarrantee success */
	+ COLAMD_STATS ;	/* for output statistics */

    /* recommended is equal to the minumum plus enough memory to keep the */
    /* number garbage collections low */
    recommended = minimum + nnz/5 ;

    return (recommended) ;
}


/* ========================================================================== */
/* === colamd_set_defaults ================================================== */
/* ========================================================================== */

/*
    The colamd_set_defaults routine sets the default values of the user-
    controllable parameters for colamd:

	knobs [0]	rows with knobs[0]*n_col entries or more are removed
			prior to ordering.

	knobs [1]	columns with knobs[1]*n_row entries or more are removed
			prior to ordering, and placed last in the column
			permutation.

	knobs [2..19]	unused, but future versions might use this
*/

PUBLIC void colamd_set_defaults
(
    /* === Parameters ======================================================= */

    double knobs [COLAMD_KNOBS]		/* knob array */
)
{
    /* === Local variables ================================================== */

    int i ;

    if (!knobs)
    {
	return ;			/* no knobs to initialize */
    }
    for (i = 0 ; i < COLAMD_KNOBS ; i++)
    {
	knobs [i] = 0 ;
    }
    knobs [COLAMD_DENSE_ROW] = 0.5 ;	/* ignore rows over 50% dense */
    knobs [COLAMD_DENSE_COL] = 0.5 ;	/* ignore columns over 50% dense */
}


/* ========================================================================== */
/* === colamd =============================================================== */
/* ========================================================================== */

/*
    The colamd routine computes a column ordering Q of a sparse matrix
    A such that the LU factorization P(AQ) = LU remains sparse, where P is
    selected via partial pivoting.   The routine can also be viewed as
    providing a permutation Q such that the Cholesky factorization
    (AQ)'(AQ) = LL' remains sparse.

    On input, the nonzero patterns of the columns of A are stored in the
    array A, in order 0 to n_col-1.  A is held in 0-based form (rows in the
    range 0 to n_row-1 and columns in the range 0 to n_col-1).  Row indices
    for column c are located in A [(p [c]) ... (p [c+1]-1)], where p [0] = 0,
    and thus p [n_col] is the number of entries in A.  The matrix is
    destroyed on output.  The row indices within each column do not have to
    be sorted (from small to large row indices), and duplicate row indices
    may be present.  However, colamd will work a little faster if columns are
    sorted and no duplicates are present.  Matlab 5.2 always passes the matrix
    with sorted columns, and no duplicates.

    The integer array A is of size Alen.  Alen must be at least of size
    (where nnz is the number of entries in A):

	nnz			for the input column form of A
	+ nnz			for a row form of A that colamd generates
	+ 6*(n_col+1)		for a ColInfo Col [0..n_col] array
				(this assumes sizeof (ColInfo) is 6 int's).
	+ 4*(n_row+1)		for a RowInfo Row [0..n_row] array
				(this assumes sizeof (RowInfo) is 4 int's).
	+ elbow_room		must be at least n_col.  We recommend at least
				nnz/5 in addition to that.  If sufficient,
				changes in the elbow room affect the ordering
				time only, not the ordering itself.
	+ COLAMD_STATS		for the output statistics

    Colamd returns FALSE is memory is insufficient, or TRUE otherwise.

    On input, the caller must specify:

	n_row			the number of rows of A
	n_col			the number of columns of A
	Alen			the size of the array A
	A [0 ... nnz-1]		the row indices, where nnz = p [n_col]
	A [nnz ... Alen-1]	(need not be initialized by the user)
	p [0 ... n_col]		the column pointers,  p [0] = 0, and p [n_col]
				is the number of entries in A.  Column c of A
				is stored in A [p [c] ... p [c+1]-1].
	knobs [0 ... 19]	a set of parameters that control the behavior
				of colamd.  If knobs is a NULL pointer the
				defaults are used.  The user-callable
				colamd_set_defaults routine sets the default
				parameters.  See that routine for a description
				of the user-controllable parameters.

    If the return value of Colamd is TRUE, then on output:

	p [0 ... n_col-1]	the column permutation. p [0] is the first
				column index, and p [n_col-1] is the last.
				That is, p [k] = j means that column j of A
				is the kth column of AQ.

	A			is undefined on output (the matrix pattern is
				destroyed), except for the following statistics:

	A [0]			the number of dense (or empty) rows ignored
	A [1]			the number of dense (or empty) columms.  These
				are ordered last, in their natural order.
	A [2]			the number of garbage collections performed.
				If this is excessive, then you would have
				gotten your results faster if Alen was larger.
	A [3]			0, if all row indices in each column were in
				sorted order and no duplicates were present.
				1, if there were unsorted or duplicate row
				indices in the input.  You would have gotten
				your results faster if A [3] was returned as 0.

    If the return value of Colamd is FALSE, then A and p are undefined on
    output.
*/

PUBLIC int colamd		/* returns TRUE if successful */
(
    /* === Parameters ======================================================= */

    int n_row,			/* number of rows in A */
    int n_col,			/* number of columns in A */
    int Alen,			/* length of A */
    int A [],			/* row indices of A */
    int p [],			/* pointers to columns in A */
    double knobs [COLAMD_KNOBS]	/* parameters (uses defaults if NULL) */
)
{
    /* === Local variables ================================================== */

    int i ;			/* loop index */
    int nnz ;			/* nonzeros in A */
    int Row_size ;		/* size of Row [], in integers */
    int Col_size ;		/* size of Col [], in integers */
    int elbow_room ;		/* remaining free space */
    RowInfo *Row ;		/* pointer into A of Row [0..n_row] array */
    ColInfo *Col ;		/* pointer into A of Col [0..n_col] array */
    int n_col2 ;		/* number of non-dense, non-empty columns */
    int n_row2 ;		/* number of non-dense, non-empty rows */
    int ngarbage ;		/* number of garbage collections performed */
    int max_deg ;		/* maximum row degree */
    double default_knobs [COLAMD_KNOBS] ;	/* default knobs knobs array */
    int init_result ;		/* return code from initialization */

#ifndef NDEBUG
    debug_colamd = 0 ;		/* no debug printing */
    /* get "D" environment variable, which gives the debug printing level */
    if (getenv ("D")) debug_colamd = atoi (getenv ("D")) ;
    DEBUG0 (("debug version, D = %d (THIS WILL BE SLOOOOW!)\n", debug_colamd)) ;
#endif

    /* === Check the input arguments ======================================== */

    if (n_row < 0 || n_col < 0 || !A || !p)
    {
	/* n_row and n_col must be non-negative, A and p must be present */
	DEBUG0 (("colamd error! %d %d %d\n", n_row, n_col, Alen)) ;
	return (FALSE) ;
    }
    nnz = p [n_col] ;
    if (nnz < 0 || p [0] != 0)
    {
	/* nnz must be non-negative, and p [0] must be zero */
	DEBUG0 (("colamd error! %d %d\n", nnz, p [0])) ;
	return (FALSE) ;
    }

    /* === If no knobs, set default parameters ============================== */

    if (!knobs)
    {
	knobs = default_knobs ;
	colamd_set_defaults (knobs) ;
    }

    /* === Allocate the Row and Col arrays from array A ===================== */

    Col_size = (n_col + 1) * sizeof (ColInfo) / sizeof (int) ;
    Row_size = (n_row + 1) * sizeof (RowInfo) / sizeof (int) ;
    elbow_room = Alen - (2*nnz + Col_size + Row_size) ;
    if (elbow_room < n_col + COLAMD_STATS)
    {
	/* not enough space in array A to perform the ordering */
	DEBUG0 (("colamd error! elbow_room %d, %d\n", elbow_room,n_col)) ;
	return (FALSE) ;
    }
    Alen = 2*nnz + elbow_room ;
    Col  = (ColInfo *) &A [Alen] ;
    Row  = (RowInfo *) &A [Alen + Col_size] ;

    /* === Construct the row and column data structures ===================== */

    init_result = init_rows_cols (n_row, n_col, Row, Col, A, p) ;
    if (init_result == -1)
    {
	/* input matrix is invalid */
	DEBUG0 (("colamd error! matrix invalid\n")) ;
	return (FALSE) ;
    }

    /* === Initialize scores, kill dense rows/columns ======================= */

    init_scoring (n_row, n_col, Row, Col, A, p, knobs,
	&n_row2, &n_col2, &max_deg) ;

    /* === Order the supercolumns =========================================== */

    ngarbage = find_ordering (n_row, n_col, Alen, Row, Col, A, p,
	n_col2, max_deg, 2*nnz) ;

    /* === Order the non-principal columns ================================== */

    order_children (n_col, Col, p) ;

    /* === Return statistics in A =========================================== */

    for (i = 0 ; i < COLAMD_STATS ; i++)
    {
	A [i] = 0 ;
    }
    A [COLAMD_DENSE_ROW] = n_row - n_row2 ;
    A [COLAMD_DENSE_COL] = n_col - n_col2 ;
    A [COLAMD_DEFRAG_COUNT] = ngarbage ;
    A [COLAMD_JUMBLED_COLS] = init_result ;

    return (TRUE) ;
}


/* ========================================================================== */
/* === NON-USER-CALLABLE ROUTINES: ========================================== */
/* ========================================================================== */

/* There are no user-callable routines beyond this point in the file */


/* ========================================================================== */
/* === init_rows_cols ======================================================= */
/* ========================================================================== */

/*
    Takes the column form of the matrix in A and creates the row form of the
    matrix.  Also, row and column attributes are stored in the Col and Row
    structs.  If the columns are un-sorted or contain duplicate row indices,
    this routine will also sort and remove duplicate row indices from the
    column form of the matrix.  Returns -1 on error, 1 if columns jumbled,
    or 0 if columns not jumbled.  Not user-callable.
*/

PRIVATE int init_rows_cols	/* returns status code */
(
    /* === Parameters ======================================================= */

    int n_row,			/* number of rows of A */
    int n_col,			/* number of columns of A */
    RowInfo Row [],		/* of size n_row+1 */
    ColInfo Col [],		/* of size n_col+1 */
    int A [],			/* row indices of A, of size Alen */
    int p []			/* pointers to columns in A, of size n_col+1 */
)
{
    /* === Local variables ================================================== */

    int col ;			/* a column index */
    int row ;			/* a row index */
    int *cp ;			/* a column pointer */
    int *cp_end ;		/* a pointer to the end of a column */
    int *rp ;			/* a row pointer */
    int *rp_end ;		/* a pointer to the end of a row */
    int last_start ;		/* start index of previous column in A */
    int start ;			/* start index of column in A */
    int last_row ;		/* previous row */
    int jumbled_columns ;	/* indicates if columns are jumbled */

    /* === Initialize columns, and check column pointers ==================== */

    last_start = 0 ;
    for (col = 0 ; col < n_col ; col++)
    {
	start = p [col] ;
	if (start < last_start)
	{
	    /* column pointers must be non-decreasing */
	    DEBUG0 (("colamd error!  last p %d p [col] %d\n",last_start,start));
	    return (-1) ;
	}
	Col [col].start = start ;
	Col [col].length = p [col+1] - start ;
	Col [col].shared1.thickness = 1 ;
	Col [col].shared2.score = 0 ;
	Col [col].shared3.prev = EMPTY ;
	Col [col].shared4.degree_next = EMPTY ;
	last_start = start ;
    }
    /* must check the end pointer for last column */
    if (p [n_col] < last_start)
    {
	/* column pointers must be non-decreasing */
	DEBUG0 (("colamd error!  last p %d p [n_col] %d\n",p[col],last_start)) ;
	return (-1) ;
    }

    /* p [0..n_col] no longer needed, used as "head" in subsequent routines */

    /* === Scan columns, compute row degrees, and check row indices ========= */

    jumbled_columns = FALSE ;

    for (row = 0 ; row < n_row ; row++)
    {
	Row [row].length = 0 ;
	Row [row].shared2.mark = -1 ;
    }

    for (col = 0 ; col < n_col ; col++)
    {
	last_row = -1 ;

	cp = &A [p [col]] ;
	cp_end = &A [p [col+1]] ;

	while (cp < cp_end)
	{
	    row = *cp++ ;

	    /* make sure row indices within range */
	    if (row < 0 || row >= n_row)
	    {
		DEBUG0 (("colamd error!  col %d row %d last_row %d\n",
			 col, row, last_row)) ;
		return (-1) ;
	    }
	    else if (row <= last_row)
	    {
		/* row indices are not sorted or repeated, thus cols */
		/* are jumbled */
		jumbled_columns = TRUE ;
	    }
	    /* prevent repeated row from being counted */
	    if (Row [row].shared2.mark != col)
	    {
		Row [row].length++ ;
		Row [row].shared2.mark = col ;
		last_row = row ;
	    }
	    else
	    {
		/* this is a repeated entry in the column, */
		/* it will be removed */
		Col [col].length-- ;
	    }
	}
    }

    /* === Compute row pointers ============================================= */

    /* row form of the matrix starts directly after the column */
    /* form of matrix in A */
    Row [0].start = p [n_col] ;
    Row [0].shared1.p = Row [0].start ;
    Row [0].shared2.mark = -1 ;
    for (row = 1 ; row < n_row ; row++)
    {
	Row [row].start = Row [row-1].start + Row [row-1].length ;
	Row [row].shared1.p = Row [row].start ;
	Row [row].shared2.mark = -1 ;
    }

    /* === Create row form ================================================== */

    if (jumbled_columns)
    {
	/* if cols jumbled, watch for repeated row indices */
	for (col = 0 ; col < n_col ; col++)
	{
	    cp = &A [p [col]] ;
	    cp_end = &A [p [col+1]] ;
	    while (cp < cp_end)
	    {
		row = *cp++ ;
		if (Row [row].shared2.mark != col)
		{
		    A [(Row [row].shared1.p)++] = col ;
		    Row [row].shared2.mark = col ;
		}
	    }
	}
    }
    else
    {
	/* if cols not jumbled, we don't need the mark (this is faster) */
	for (col = 0 ; col < n_col ; col++)
	{
	    cp = &A [p [col]] ;
	    cp_end = &A [p [col+1]] ;
	    while (cp < cp_end)
	    {
		A [(Row [*cp++].shared1.p)++] = col ;
	    }
	}
    }

    /* === Clear the row marks and set row degrees ========================== */

    for (row = 0 ; row < n_row ; row++)
    {
	Row [row].shared2.mark = 0 ;
	Row [row].shared1.degree = Row [row].length ;
    }

    /* === See if we need to re-create columns ============================== */

    if (jumbled_columns)
    {

#ifndef NDEBUG
	/* make sure column lengths are correct */
	for (col = 0 ; col < n_col ; col++)
	{
	    p [col] = Col [col].length ;
	}
	for (row = 0 ; row < n_row ; row++)
	{
	    rp = &A [Row [row].start] ;
	    rp_end = rp + Row [row].length ;
	    while (rp < rp_end)
	    {
		p [*rp++]-- ;
	    }
	}
	for (col = 0 ; col < n_col ; col++)
	{
	    assert (p [col] == 0) ;
	}
	/* now p is all zero (different than when debugging is turned off) */
#endif

	/* === Compute col pointers ========================================= */

	/* col form of the matrix starts at A [0]. */
	/* Note, we may have a gap between the col form and the row */
	/* form if there were duplicate entries, if so, it will be */
	/* removed upon the first garbage collection */
	Col [0].start = 0 ;
	p [0] = Col [0].start ;
	for (col = 1 ; col < n_col ; col++)
	{
	    /* note that the lengths here are for pruned columns, i.e. */
	    /* no duplicate row indices will exist for these columns */
	    Col [col].start = Col [col-1].start + Col [col-1].length ;
	    p [col] = Col [col].start ;
	}

	/* === Re-create col form =========================================== */

	for (row = 0 ; row < n_row ; row++)
	{
	    rp = &A [Row [row].start] ;
	    rp_end = rp + Row [row].length ;
	    while (rp < rp_end)
	    {
		A [(p [*rp++])++] = row ;
	    }
	}
	return (1) ;
    }
    else
    {
	/* no columns jumbled (this is faster) */
	return (0) ;
    }
}


/* ========================================================================== */
/* === init_scoring ========================================================= */
/* ========================================================================== */

/*
    Kills dense or empty columns and rows, calculates an initial score for
    each column, and places all columns in the degree lists.  Not user-callable.
*/

PRIVATE void init_scoring
(
    /* === Parameters ======================================================= */

    int n_row,			/* number of rows of A */
    int n_col,			/* number of columns of A */
    RowInfo Row [],		/* of size n_row+1 */
    ColInfo Col [],		/* of size n_col+1 */
    int A [],			/* column form and row form of A */
    int head [],		/* of size n_col+1 */
    double knobs [COLAMD_KNOBS],/* parameters */
    int *p_n_row2,		/* number of non-dense, non-empty rows */
    int *p_n_col2,		/* number of non-dense, non-empty columns */
    int *p_max_deg		/* maximum row degree */
)
{
    /* === Local variables ================================================== */

    int c ;			/* a column index */
    int r, row ;		/* a row index */
    int *cp ;			/* a column pointer */
    int deg ;			/* degree (# entries) of a row or column */
    int *cp_end ;		/* a pointer to the end of a column */
    int *new_cp ;		/* new column pointer */
    int col_length ;		/* length of pruned column */
    int score ;			/* current column score */
    int n_col2 ;		/* number of non-dense, non-empty columns */
    int n_row2 ;		/* number of non-dense, non-empty rows */
    int dense_row_count ;	/* remove rows with more entries than this */
    int dense_col_count ;	/* remove cols with more entries than this */
    int min_score ;		/* smallest column score */
    int max_deg ;		/* maximum row degree */
    int next_col ;		/* Used to add to degree list.*/
#ifndef NDEBUG
    int debug_count ;		/* debug only. */
#endif

    /* === Extract knobs ==================================================== */

    dense_row_count = MAX (0, MIN (knobs [COLAMD_DENSE_ROW] * n_col, n_col)) ;
    dense_col_count = MAX (0, MIN (knobs [COLAMD_DENSE_COL] * n_row, n_row)) ;
    DEBUG0 (("densecount: %d %d\n", dense_row_count, dense_col_count)) ;
    max_deg = 0 ;
    n_col2 = n_col ;
    n_row2 = n_row ;

    /* === Kill empty columns =============================================== */

    /* Put the empty columns at the end in their natural, so that LU */
    /* factorization can proceed as far as possible. */
    for (c = n_col-1 ; c >= 0 ; c--)
    {
	deg = Col [c].length ;
	if (deg == 0)
	{
	    /* this is a empty column, kill and order it last */
	    Col [c].shared2.order = --n_col2 ;
	    KILL_PRINCIPAL_COL (c) ;
	}
    }
    DEBUG0 (("null columns killed: %d\n", n_col - n_col2)) ;

    /* === Kill dense columns =============================================== */

    /* Put the dense columns at the end, in their natural order */
    for (c = n_col-1 ; c >= 0 ; c--)
    {
	/* skip any dead columns */
	if (COL_IS_DEAD (c))
	{
	    continue ;
	}
	deg = Col [c].length ;
	if (deg > dense_col_count)
	{
	    /* this is a dense column, kill and order it last */
	    Col [c].shared2.order = --n_col2 ;
	    /* decrement the row degrees */
	    cp = &A [Col [c].start] ;
	    cp_end = cp + Col [c].length ;
	    while (cp < cp_end)
	    {
		Row [*cp++].shared1.degree-- ;
	    }
	    KILL_PRINCIPAL_COL (c) ;
	}
    }
    DEBUG0 (("Dense and null columns killed: %d\n", n_col - n_col2)) ;

    /* === Kill dense and empty rows ======================================== */

    for (r = 0 ; r < n_row ; r++)
    {
	deg = Row [r].shared1.degree ;
	assert (deg >= 0 && deg <= n_col) ;
	if (deg > dense_row_count || deg == 0)
	{
	    /* kill a dense or empty row */
	    KILL_ROW (r) ;
	    --n_row2 ;
	}
	else
	{
	    /* keep track of max degree of remaining rows */
	    max_deg = MAX (max_deg, deg) ;
	}
    }
    DEBUG0 (("Dense and null rows killed: %d\n", n_row - n_row2)) ;

    /* === Compute initial column scores ==================================== */

    /* At this point the row degrees are accurate.  They reflect the number */
    /* of "live" (non-dense) columns in each row.  No empty rows exist. */
    /* Some "live" columns may contain only dead rows, however.  These are */
    /* pruned in the code below. */

    /* now find the initial matlab score for each column */
    for (c = n_col-1 ; c >= 0 ; c--)
    {
	/* skip dead column */
	if (COL_IS_DEAD (c))
	{
	    continue ;
	}
	score = 0 ;
	cp = &A [Col [c].start] ;
	new_cp = cp ;
	cp_end = cp + Col [c].length ;
	while (cp < cp_end)
	{
	    /* get a row */
	    row = *cp++ ;
	    /* skip if dead */
	    if (ROW_IS_DEAD (row))
	    {
		continue ;
	    }
	    /* compact the column */
	    *new_cp++ = row ;
	    /* add row's external degree */
	    score += Row [row].shared1.degree - 1 ;
	    /* guard against integer overflow */
	    score = MIN (score, n_col) ;
	}
	/* determine pruned column length */
	col_length = (int) (new_cp - &A [Col [c].start]) ;
	if (col_length == 0)
	{
	    /* a newly-made null column (all rows in this col are "dense" */
	    /* and have already been killed) */
	    DEBUG0 (("Newly null killed: %d\n", c)) ;
	    Col [c].shared2.order = --n_col2 ;
	    KILL_PRINCIPAL_COL (c) ;
	}
	else
	{
	    /* set column length and set score */
	    assert (score >= 0) ;
	    assert (score <= n_col) ;
	    Col [c].length = col_length ;
	    Col [c].shared2.score = score ;
	}
    }
    DEBUG0 (("Dense, null, and newly-null columns killed: %d\n",n_col-n_col2)) ;

    /* At this point, all empty rows and columns are dead.  All live columns */
    /* are "clean" (containing no dead rows) and simplicial (no supercolumns */
    /* yet).  Rows may contain dead columns, but all live rows contain at */
    /* least one live column. */

#ifndef NDEBUG
    debug_structures (n_row, n_col, Row, Col, A, n_col2) ;
#endif

    /* === Initialize degree lists ========================================== */

#ifndef NDEBUG
    debug_count = 0 ;
#endif

    /* clear the hash buckets */
    for (c = 0 ; c <= n_col ; c++)
    {
	head [c] = EMPTY ;
    }
    min_score = n_col ;
    /* place in reverse order, so low column indices are at the front */
    /* of the lists.  This is to encourage natural tie-breaking */
    for (c = n_col-1 ; c >= 0 ; c--)
    {
	/* only add principal columns to degree lists */
	if (COL_IS_ALIVE (c))
	{
	    DEBUG4 (("place %d score %d minscore %d ncol %d\n",
		c, Col [c].shared2.score, min_score, n_col)) ;

	    /* === Add columns score to DList =============================== */

	    score = Col [c].shared2.score ;

	    assert (min_score >= 0) ;
	    assert (min_score <= n_col) ;
	    assert (score >= 0) ;
	    assert (score <= n_col) ;
	    assert (head [score] >= EMPTY) ;

	    /* now add this column to dList at proper score location */
	    next_col = head [score] ;
	    Col [c].shared3.prev = EMPTY ;
	    Col [c].shared4.degree_next = next_col ;

	    /* if there already was a column with the same score, set its */
	    /* previous pointer to this new column */
	    if (next_col != EMPTY)
	    {
		Col [next_col].shared3.prev = c ;
	    }
	    head [score] = c ;

	    /* see if this score is less than current min */
	    min_score = MIN (min_score, score) ;

#ifndef NDEBUG
	    debug_count++ ;
#endif
	}
    }

#ifndef NDEBUG
    DEBUG0 (("Live cols %d out of %d, non-princ: %d\n",
	debug_count, n_col, n_col-debug_count)) ;
    assert (debug_count == n_col2) ;
    debug_deg_lists (n_row, n_col, Row, Col, head, min_score, n_col2, max_deg) ;
#endif

    /* === Return number of remaining columns, and max row degree =========== */

    *p_n_col2 = n_col2 ;
    *p_n_row2 = n_row2 ;
    *p_max_deg = max_deg ;
}


/* ========================================================================== */
/* === find_ordering ======================================================== */
/* ========================================================================== */

/*
    Order the principal columns of the supercolumn form of the matrix
    (no supercolumns on input).  Uses a minimum approximate column minimum
    degree ordering method.  Not user-callable.
*/

PRIVATE int find_ordering	/* return the number of garbage collections */
(
    /* === Parameters ======================================================= */

    int n_row,			/* number of rows of A */
    int n_col,			/* number of columns of A */
    int Alen,			/* size of A, 2*nnz + elbow_room or larger */
    RowInfo Row [],		/* of size n_row+1 */
    ColInfo Col [],		/* of size n_col+1 */
    int A [],			/* column form and row form of A */
    int head [],		/* of size n_col+1 */
    int n_col2,			/* Remaining columns to order */
    int max_deg,		/* Maximum row degree */
    int pfree			/* index of first free slot (2*nnz on entry) */
)
{
    /* === Local variables ================================================== */

    int k ;			/* current pivot ordering step */
    int pivot_col ;		/* current pivot column */
    int *cp ;			/* a column pointer */
    int *rp ;			/* a row pointer */
    int pivot_row ;		/* current pivot row */
    int *new_cp ;		/* modified column pointer */
    int *new_rp ;		/* modified row pointer */
    int pivot_row_start ;	/* pointer to start of pivot row */
    int pivot_row_degree ;	/* # of columns in pivot row */
    int pivot_row_length ;	/* # of supercolumns in pivot row */
    int pivot_col_score ;	/* score of pivot column */
    int needed_memory ;		/* free space needed for pivot row */
    int *cp_end ;		/* pointer to the end of a column */
    int *rp_end ;		/* pointer to the end of a row */
    int row ;			/* a row index */
    int col ;			/* a column index */
    int max_score ;		/* maximum possible score */
    int cur_score ;		/* score of current column */
    unsigned int hash ;		/* hash value for supernode detection */
    int head_column ;		/* head of hash bucket */
    int first_col ;		/* first column in hash bucket */
    int tag_mark ;		/* marker value for mark array */
    int row_mark ;		/* Row [row].shared2.mark */
    int set_difference ;	/* set difference size of row with pivot row */
    int min_score ;		/* smallest column score */
    int col_thickness ;		/* "thickness" (# of columns in a supercol) */
    int max_mark ;		/* maximum value of tag_mark */
    int pivot_col_thickness ;	/* number of columns represented by pivot col */
    int prev_col ;		/* Used by Dlist operations. */
    int next_col ;		/* Used by Dlist operations. */
    int ngarbage ;		/* number of garbage collections performed */
#ifndef NDEBUG
    int debug_d ;		/* debug loop counter */
    int debug_step = 0 ;	/* debug loop counter */
#endif

    /* === Initialization and clear mark ==================================== */

    max_mark = INT_MAX - n_col ;	/* INT_MAX defined in <limits.h> */
    tag_mark = clear_mark (n_row, Row) ;
    min_score = 0 ;
    ngarbage = 0 ;
    DEBUG0 (("Ordering.. n_col2=%d\n", n_col2)) ;

    /* === Order the columns ================================================ */

    for (k = 0 ; k < n_col2 ; /* 'k' is incremented below */)
    {

#ifndef NDEBUG
	if (debug_step % 100 == 0)
	{
	    DEBUG0 (("\n...       Step k: %d out of n_col2: %d\n", k, n_col2)) ;
	}
	else
	{
	    DEBUG1 (("\n----------Step k: %d out of n_col2: %d\n", k, n_col2)) ;
	}
	debug_step++ ;
	debug_deg_lists (n_row, n_col, Row, Col, head,
		min_score, n_col2-k, max_deg) ;
	debug_matrix (n_row, n_col, Row, Col, A) ;
#endif

	/* === Select pivot column, and order it ============================ */

	/* make sure degree list isn't empty */
	assert (min_score >= 0) ;
	assert (min_score <= n_col) ;
	assert (head [min_score] >= EMPTY) ;

#ifndef NDEBUG
	for (debug_d = 0 ; debug_d < min_score ; debug_d++)
	{
	    assert (head [debug_d] == EMPTY) ;
	}
#endif

	/* get pivot column from head of minimum degree list */
	while (head [min_score] == EMPTY && min_score < n_col)
	{
	    min_score++ ;
	}
	pivot_col = head [min_score] ;
	assert (pivot_col >= 0 && pivot_col <= n_col) ;
	next_col = Col [pivot_col].shared4.degree_next ;
	head [min_score] = next_col ;
	if (next_col != EMPTY)
	{
	    Col [next_col].shared3.prev = EMPTY ;
	}

	assert (COL_IS_ALIVE (pivot_col)) ;
	DEBUG3 (("Pivot col: %d\n", pivot_col)) ;

	/* remember score for defrag check */
	pivot_col_score = Col [pivot_col].shared2.score ;

	/* the pivot column is the kth column in the pivot order */
	Col [pivot_col].shared2.order = k ;

	/* increment order count by column thickness */
	pivot_col_thickness = Col [pivot_col].shared1.thickness ;
	k += pivot_col_thickness ;
	assert (pivot_col_thickness > 0) ;

	/* === Garbage_collection, if necessary ============================= */

	needed_memory = MIN (pivot_col_score, n_col - k) ;
	if (pfree + needed_memory >= Alen)
	{
	    pfree = garbage_collection (n_row, n_col, Row, Col, A, &A [pfree]) ;
	    ngarbage++ ;
	    /* after garbage collection we will have enough */
	    assert (pfree + needed_memory < Alen) ;
	    /* garbage collection has wiped out the Row[].shared2.mark array */
	    tag_mark = clear_mark (n_row, Row) ;
#ifndef NDEBUG
	    debug_matrix (n_row, n_col, Row, Col, A) ;
#endif
	}

	/* === Compute pivot row pattern ==================================== */

	/* get starting location for this new merged row */
	pivot_row_start = pfree ;

	/* initialize new row counts to zero */
	pivot_row_degree = 0 ;

	/* tag pivot column as having been visited so it isn't included */
	/* in merged pivot row */
	Col [pivot_col].shared1.thickness = -pivot_col_thickness ;

	/* pivot row is the union of all rows in the pivot column pattern */
	cp = &A [Col [pivot_col].start] ;
	cp_end = cp + Col [pivot_col].length ;
	while (cp < cp_end)
	{
	    /* get a row */
	    row = *cp++ ;
	    DEBUG4 (("Pivot col pattern %d %d\n", ROW_IS_ALIVE (row), row)) ;
	    /* skip if row is dead */
	    if (ROW_IS_DEAD (row))
	    {
		continue ;
	    }
	    rp = &A [Row [row].start] ;
	    rp_end = rp + Row [row].length ;
	    while (rp < rp_end)
	    {
		/* get a column */
		col = *rp++ ;
		/* add the column, if alive and untagged */
		col_thickness = Col [col].shared1.thickness ;
		if (col_thickness > 0 && COL_IS_ALIVE (col))
		{
		    /* tag column in pivot row */
		    Col [col].shared1.thickness = -col_thickness ;
		    assert (pfree < Alen) ;
		    /* place column in pivot row */
		    A [pfree++] = col ;
		    pivot_row_degree += col_thickness ;
		}
	    }
	}

	/* clear tag on pivot column */
	Col [pivot_col].shared1.thickness = pivot_col_thickness ;
	max_deg = MAX (max_deg, pivot_row_degree) ;

#ifndef NDEBUG
	DEBUG3 (("check2\n")) ;
	debug_mark (n_row, Row, tag_mark, max_mark) ;
#endif

	/* === Kill all rows used to construct pivot row ==================== */

	/* also kill pivot row, temporarily */
	cp = &A [Col [pivot_col].start] ;
	cp_end = cp + Col [pivot_col].length ;
	while (cp < cp_end)
	{
	    /* may be killing an already dead row */
	    row = *cp++ ;
	    DEBUG2 (("Kill row in pivot col: %d\n", row)) ;
	    KILL_ROW (row) ;
	}

	/* === Select a row index to use as the new pivot row =============== */

	pivot_row_length = pfree - pivot_row_start ;
	if (pivot_row_length > 0)
	{
	    /* pick the "pivot" row arbitrarily (first row in col) */
	    pivot_row = A [Col [pivot_col].start] ;
	    DEBUG2 (("Pivotal row is %d\n", pivot_row)) ;
	}
	else
	{
	    /* there is no pivot row, since it is of zero length */
	    pivot_row = EMPTY ;
	    assert (pivot_row_length == 0) ;
	}
	assert (Col [pivot_col].length > 0 || pivot_row_length == 0) ;

	/* === Approximate degree computation =============================== */

	/* Here begins the computation of the approximate degree.  The column */
	/* score is the sum of the pivot row "length", plus the size of the */
	/* set differences of each row in the column minus the pattern of the */
	/* pivot row itself.  The column ("thickness") itself is also */
	/* excluded from the column score (we thus use an approximate */
	/* external degree). */

	/* The time taken by the following code (compute set differences, and */
	/* add them up) is proportional to the size of the data structure */
	/* being scanned - that is, the sum of the sizes of each column in */
	/* the pivot row.  Thus, the amortized time to compute a column score */
	/* is proportional to the size of that column (where size, in this */
	/* context, is the column "length", or the number of row indices */
	/* in that column).  The number of row indices in a column is */
	/* monotonically non-decreasing, from the length of the original */
	/* column on input to colamd. */

	/* === Compute set differences ====================================== */

	DEBUG1 (("** Computing set differences phase. **\n")) ;

	/* pivot row is currently dead - it will be revived later. */

	DEBUG2 (("Pivot row: ")) ;
	/* for each column in pivot row */
	rp = &A [pivot_row_start] ;
	rp_end = rp + pivot_row_length ;
	while (rp < rp_end)
	{
	    col = *rp++ ;
	    assert (COL_IS_ALIVE (col) && col != pivot_col) ;
	    DEBUG2 (("Col: %d\n", col)) ;

	    /* clear tags used to construct pivot row pattern */
	    col_thickness = -Col [col].shared1.thickness ;
	    assert (col_thickness > 0) ;
	    Col [col].shared1.thickness = col_thickness ;

	    /* === Remove column from degree list =========================== */

	    cur_score = Col [col].shared2.score ;
	    prev_col = Col [col].shared3.prev ;
	    next_col = Col [col].shared4.degree_next ;
	    assert (cur_score >= 0) ;
	    assert (cur_score <= n_col) ;
	    assert (cur_score >= EMPTY) ;
	    if (prev_col == EMPTY)
	    {
		head [cur_score] = next_col ;
	    }
	    else
	    {
		Col [prev_col].shared4.degree_next = next_col ;
	    }
	    if (next_col != EMPTY)
	    {
		Col [next_col].shared3.prev = prev_col ;
	    }

	    /* === Scan the column ========================================== */

	    cp = &A [Col [col].start] ;
	    cp_end = cp + Col [col].length ;
	    while (cp < cp_end)
	    {
		/* get a row */
		row = *cp++ ;
		row_mark = Row [row].shared2.mark ;
		/* skip if dead */
		if (ROW_IS_MARKED_DEAD (row_mark))
		{
		    continue ;
		}
		assert (row != pivot_row) ;
		set_difference = row_mark - tag_mark ;
		/* check if the row has been seen yet */
		if (set_difference < 0)
		{
		    assert (Row [row].shared1.degree <= max_deg) ;
		    set_difference = Row [row].shared1.degree ;
		}
		/* subtract column thickness from this row's set difference */
		set_difference -= col_thickness ;
		assert (set_difference >= 0) ;
		/* absorb this row if the set difference becomes zero */
		if (set_difference == 0)
		{
		    DEBUG1 (("aggressive absorption. Row: %d\n", row)) ;
		    KILL_ROW (row) ;
		}
		else
		{
		    /* save the new mark */
		    Row [row].shared2.mark = set_difference + tag_mark ;
		}
	    }
	}

#ifndef NDEBUG
	debug_deg_lists (n_row, n_col, Row, Col, head,
		min_score, n_col2-k-pivot_row_degree, max_deg) ;
#endif

	/* === Add up set differences for each column ======================= */

	DEBUG1 (("** Adding set differences phase. **\n")) ;

	/* for each column in pivot row */
	rp = &A [pivot_row_start] ;
	rp_end = rp + pivot_row_length ;
	while (rp < rp_end)
	{
	    /* get a column */
	    col = *rp++ ;
	    assert (COL_IS_ALIVE (col) && col != pivot_col) ;
	    hash = 0 ;
	    cur_score = 0 ;
	    cp = &A [Col [col].start] ;
	    /* compact the column */
	    new_cp = cp ;
	    cp_end = cp + Col [col].length ;

	    DEBUG2 (("Adding set diffs for Col: %d.\n", col)) ;

	    while (cp < cp_end)
	    {
		/* get a row */
		row = *cp++ ;
		assert(row >= 0 && row < n_row) ;
		row_mark = Row [row].shared2.mark ;
		/* skip if dead */
		if (ROW_IS_MARKED_DEAD (row_mark))
		{
		    continue ;
		}
		assert (row_mark > tag_mark) ;
		/* compact the column */
		*new_cp++ = row ;
		/* compute hash function */
		hash += row ;
		/* add set difference */
		cur_score += row_mark - tag_mark ;
		/* integer overflow... */
		cur_score = MIN (cur_score, n_col) ;
	    }

	    /* recompute the column's length */
	    Col [col].length = (int) (new_cp - &A [Col [col].start]) ;

	    /* === Further mass elimination ================================= */

	    if (Col [col].length == 0)
	    {
		DEBUG1 (("further mass elimination. Col: %d\n", col)) ;
		/* nothing left but the pivot row in this column */
		KILL_PRINCIPAL_COL (col) ;
		pivot_row_degree -= Col [col].shared1.thickness ;
		assert (pivot_row_degree >= 0) ;
		/* order it */
		Col [col].shared2.order = k ;
		/* increment order count by column thickness */
		k += Col [col].shared1.thickness ;
	    }
	    else
	    {
		/* === Prepare for supercolumn detection ==================== */

		DEBUG2 (("Preparing supercol detection for Col: %d.\n", col)) ;

		/* save score so far */
		Col [col].shared2.score = cur_score ;

		/* add column to hash table, for supercolumn detection */
		hash %= n_col + 1 ;

		DEBUG2 ((" Hash = %d, n_col = %d.\n", hash, n_col)) ;
		assert (hash <= n_col) ;

		head_column = head [hash] ;
		if (head_column > EMPTY)
		{
		    /* degree list "hash" is non-empty, use prev (shared3) of */
		    /* first column in degree list as head of hash bucket */
		    first_col = Col [head_column].shared3.headhash ;
		    Col [head_column].shared3.headhash = col ;
		}
		else
		{
		    /* degree list "hash" is empty, use head as hash bucket */
		    first_col = - (head_column + 2) ;
		    head [hash] = - (col + 2) ;
		}
		Col [col].shared4.hash_next = first_col ;

		/* save hash function in Col [col].shared3.hash */
		Col [col].shared3.hash = (int) hash ;
		assert (COL_IS_ALIVE (col)) ;
	    }
	}

	/* The approximate external column degree is now computed.  */

	/* === Supercolumn detection ======================================== */

	DEBUG1 (("** Supercolumn detection phase. **\n")) ;

	detect_super_cols (
#ifndef NDEBUG
		n_col, Row,
#endif
		Col, A, head, pivot_row_start, pivot_row_length) ;

	/* === Kill the pivotal column ====================================== */

	KILL_PRINCIPAL_COL (pivot_col) ;

	/* === Clear mark =================================================== */

	tag_mark += (max_deg + 1) ;
	if (tag_mark >= max_mark)
	{
	    DEBUG1 (("clearing tag_mark\n")) ;
	    tag_mark = clear_mark (n_row, Row) ;
	}
#ifndef NDEBUG
	DEBUG3 (("check3\n")) ;
	debug_mark (n_row, Row, tag_mark, max_mark) ;
#endif

	/* === Finalize the new pivot row, and column scores ================ */

	DEBUG1 (("** Finalize scores phase. **\n")) ;

	/* for each column in pivot row */
	rp = &A [pivot_row_start] ;
	/* compact the pivot row */
	new_rp = rp ;
	rp_end = rp + pivot_row_length ;
	while (rp < rp_end)
	{
	    col = *rp++ ;
	    /* skip dead columns */
	    if (COL_IS_DEAD (col))
	    {
		continue ;
	    }
	    *new_rp++ = col ;
	    /* add new pivot row to column */
	    A [Col [col].start + (Col [col].length++)] = pivot_row ;

	    /* retrieve score so far and add on pivot row's degree. */
	    /* (we wait until here for this in case the pivot */
	    /* row's degree was reduced due to mass elimination). */
	    cur_score = Col [col].shared2.score + pivot_row_degree ;

	    /* calculate the max possible score as the number of */
	    /* external columns minus the 'k' value minus the */
	    /* columns thickness */
	    max_score = n_col - k - Col [col].shared1.thickness ;

	    /* make the score the external degree of the union-of-rows */
	    cur_score -= Col [col].shared1.thickness ;

	    /* make sure score is less or equal than the max score */
	    cur_score = MIN (cur_score, max_score) ;
	    assert (cur_score >= 0) ;

	    /* store updated score */
	    Col [col].shared2.score = cur_score ;

	    /* === Place column back in degree list ========================= */

	    assert (min_score >= 0) ;
	    assert (min_score <= n_col) ;
	    assert (cur_score >= 0) ;
	    assert (cur_score <= n_col) ;
	    assert (head [cur_score] >= EMPTY) ;
	    next_col = head [cur_score] ;
	    Col [col].shared4.degree_next = next_col ;
	    Col [col].shared3.prev = EMPTY ;
	    if (next_col != EMPTY)
	    {
		Col [next_col].shared3.prev = col ;
	    }
	    head [cur_score] = col ;

	    /* see if this score is less than current min */
	    min_score = MIN (min_score, cur_score) ;

	}

#ifndef NDEBUG
	debug_deg_lists (n_row, n_col, Row, Col, head,
		min_score, n_col2-k, max_deg) ;
#endif

	/* === Resurrect the new pivot row ================================== */

	if (pivot_row_degree > 0)
	{
	    /* update pivot row length to reflect any cols that were killed */
	    /* during super-col detection and mass elimination */
	    Row [pivot_row].start  = pivot_row_start ;
	    Row [pivot_row].length = (int) (new_rp - &A[pivot_row_start]) ;
	    Row [pivot_row].shared1.degree = pivot_row_degree ;
	    Row [pivot_row].shared2.mark = 0 ;
	    /* pivot row is no longer dead */
	}
    }

    /* === All principal columns have now been ordered ====================== */

    return (ngarbage) ;
}


/* ========================================================================== */
/* === order_children ======================================================= */
/* ========================================================================== */

/*
    The find_ordering routine has ordered all of the principal columns (the
    representatives of the supercolumns).  The non-principal columns have not
    yet been ordered.  This routine orders those columns by walking up the
    parent tree (a column is a child of the column which absorbed it).  The
    final permutation vector is then placed in p [0 ... n_col-1], with p [0]
    being the first column, and p [n_col-1] being the last.  It doesn't look
    like it at first glance, but be assured that this routine takes time linear
    in the number of columns.  Although not immediately obvious, the time
    taken by this routine is O (n_col), that is, linear in the number of
    columns.  Not user-callable.
*/

PRIVATE void order_children
(
    /* === Parameters ======================================================= */

    int n_col,			/* number of columns of A */
    ColInfo Col [],		/* of size n_col+1 */
    int p []			/* p [0 ... n_col-1] is the column permutation*/
)
{
    /* === Local variables ================================================== */

    int i ;			/* loop counter for all columns */
    int c ;			/* column index */
    int parent ;		/* index of column's parent */
    int order ;			/* column's order */

    /* === Order each non-principal column ================================== */

    for (i = 0 ; i < n_col ; i++)
    {
	/* find an un-ordered non-principal column */
	assert (COL_IS_DEAD (i)) ;
	if (!COL_IS_DEAD_PRINCIPAL (i) && Col [i].shared2.order == EMPTY)
	{
	    parent = i ;
	    /* once found, find its principal parent */
	    do
	    {
		parent = Col [parent].shared1.parent ;
	    } while (!COL_IS_DEAD_PRINCIPAL (parent)) ;

	    /* now, order all un-ordered non-principal columns along path */
	    /* to this parent.  collapse tree at the same time */
	    c = i ;
	    /* get order of parent */
	    order = Col [parent].shared2.order ;

	    do
	    {
		assert (Col [c].shared2.order == EMPTY) ;

		/* order this column */
		Col [c].shared2.order = order++ ;
		/* collaps tree */
		Col [c].shared1.parent = parent ;

		/* get immediate parent of this column */
		c = Col [c].shared1.parent ;

		/* continue until we hit an ordered column.  There are */
		/* guarranteed not to be anymore unordered columns */
		/* above an ordered column */
	    } while (Col [c].shared2.order == EMPTY) ;

	    /* re-order the super_col parent to largest order for this group */
	    Col [parent].shared2.order = order ;
	}
    }

    /* === Generate the permutation ========================================= */

    for (c = 0 ; c < n_col ; c++)
    {
	p [Col [c].shared2.order] = c ;
    }
}


/* ========================================================================== */
/* === detect_super_cols ==================================================== */
/* ========================================================================== */

/*
    Detects supercolumns by finding matches between columns in the hash buckets.
    Check amongst columns in the set A [row_start ... row_start + row_length-1].
    The columns under consideration are currently *not* in the degree lists,
    and have already been placed in the hash buckets.

    The hash bucket for columns whose hash function is equal to h is stored
    as follows:

	if head [h] is >= 0, then head [h] contains a degree list, so:

		head [h] is the first column in degree bucket h.
		Col [head [h]].headhash gives the first column in hash bucket h.

	otherwise, the degree list is empty, and:

		-(head [h] + 2) is the first column in hash bucket h.

    For a column c in a hash bucket, Col [c].shared3.prev is NOT a "previous
    column" pointer.  Col [c].shared3.hash is used instead as the hash number
    for that column.  The value of Col [c].shared4.hash_next is the next column
    in the same hash bucket.

    Assuming no, or "few" hash collisions, the time taken by this routine is
    linear in the sum of the sizes (lengths) of each column whose score has
    just been computed in the approximate degree computation.
    Not user-callable.
*/

PRIVATE void detect_super_cols
(
    /* === Parameters ======================================================= */

#ifndef NDEBUG
    /* these two parameters are only needed when debugging is enabled: */
    int n_col,			/* number of columns of A */
    RowInfo Row [],		/* of size n_row+1 */
#endif
    ColInfo Col [],		/* of size n_col+1 */
    int A [],			/* row indices of A */
    int head [],		/* head of degree lists and hash buckets */
    int row_start,		/* pointer to set of columns to check */
    int row_length		/* number of columns to check */
)
{
    /* === Local variables ================================================== */

    int hash ;			/* hash # for a column */
    int *rp ;			/* pointer to a row */
    int c ;			/* a column index */
    int super_c ;		/* column index of the column to absorb into */
    int *cp1 ;			/* column pointer for column super_c */
    int *cp2 ;			/* column pointer for column c */
    int length ;		/* length of column super_c */
    int prev_c ;		/* column preceding c in hash bucket */
    int i ;			/* loop counter */
    int *rp_end ;		/* pointer to the end of the row */
    int col ;			/* a column index in the row to check */
    int head_column ;		/* first column in hash bucket or degree list */
    int first_col ;		/* first column in hash bucket */

    /* === Consider each column in the row ================================== */

    rp = &A [row_start] ;
    rp_end = rp + row_length ;
    while (rp < rp_end)
    {
	col = *rp++ ;
	if (COL_IS_DEAD (col))
	{
	    continue ;
	}

	/* get hash number for this column */
	hash = Col [col].shared3.hash ;
	assert (hash <= n_col) ;

	/* === Get the first column in this hash bucket ===================== */

	head_column = head [hash] ;
	if (head_column > EMPTY)
	{
	    first_col = Col [head_column].shared3.headhash ;
	}
	else
	{
	    first_col = - (head_column + 2) ;
	}

	/* === Consider each column in the hash bucket ====================== */

	for (super_c = first_col ; super_c != EMPTY ;
	    super_c = Col [super_c].shared4.hash_next)
	{
	    assert (COL_IS_ALIVE (super_c)) ;
	    assert (Col [super_c].shared3.hash == hash) ;
	    length = Col [super_c].length ;

	    /* prev_c is the column preceding column c in the hash bucket */
	    prev_c = super_c ;

	    /* === Compare super_c with all columns after it ================ */

	    for (c = Col [super_c].shared4.hash_next ;
		 c != EMPTY ; c = Col [c].shared4.hash_next)
	    {
		assert (c != super_c) ;
		assert (COL_IS_ALIVE (c)) ;
		assert (Col [c].shared3.hash == hash) ;

		/* not identical if lengths or scores are different */
		if (Col [c].length != length ||
		    Col [c].shared2.score != Col [super_c].shared2.score)
		{
		    prev_c = c ;
		    continue ;
		}

		/* compare the two columns */
		cp1 = &A [Col [super_c].start] ;
		cp2 = &A [Col [c].start] ;

		for (i = 0 ; i < length ; i++)
		{
		    /* the columns are "clean" (no dead rows) */
		    assert (ROW_IS_ALIVE (*cp1))  ;
		    assert (ROW_IS_ALIVE (*cp2))  ;
		    /* row indices will same order for both supercols, */
		    /* no gather scatter nessasary */
		    if (*cp1++ != *cp2++)
		    {
			break ;
		    }
		}

		/* the two columns are different if the for-loop "broke" */
		if (i != length)
		{
		    prev_c = c ;
		    continue ;
		}

		/* === Got it!  two columns are identical =================== */

		assert (Col [c].shared2.score == Col [super_c].shared2.score) ;

		Col [super_c].shared1.thickness += Col [c].shared1.thickness ;
		Col [c].shared1.parent = super_c ;
		KILL_NON_PRINCIPAL_COL (c) ;
		/* order c later, in order_children() */
		Col [c].shared2.order = EMPTY ;
		/* remove c from hash bucket */
		Col [prev_c].shared4.hash_next = Col [c].shared4.hash_next ;
	    }
	}

	/* === Empty this hash bucket ======================================= */

	if (head_column > EMPTY)
	{
	    /* corresponding degree list "hash" is not empty */
	    Col [head_column].shared3.headhash = EMPTY ;
	}
	else
	{
	    /* corresponding degree list "hash" is empty */
	    head [hash] = EMPTY ;
	}
    }
}


/* ========================================================================== */
/* === garbage_collection =================================================== */
/* ========================================================================== */

/*
    Defragments and compacts columns and rows in the workspace A.  Used when
    all avaliable memory has been used while performing row merging.  Returns
    the index of the first free position in A, after garbage collection.  The
    time taken by this routine is linear is the size of the array A, which is
    itself linear in the number of nonzeros in the input matrix.
    Not user-callable.
*/

PRIVATE int garbage_collection  /* returns the new value of pfree */
(
    /* === Parameters ======================================================= */

    int n_row,			/* number of rows */
    int n_col,			/* number of columns */
    RowInfo Row [],		/* row info */
    ColInfo Col [],		/* column info */
    int A [],			/* A [0 ... Alen-1] holds the matrix */
    int *pfree			/* &A [0] ... pfree is in use */
)
{
    /* === Local variables ================================================== */

    int *psrc ;			/* source pointer */
    int *pdest ;		/* destination pointer */
    int j ;			/* counter */
    int r ;			/* a row index */
    int c ;			/* a column index */
    int length ;		/* length of a row or column */

#ifndef NDEBUG
    int debug_rows ;
    DEBUG0 (("Defrag..\n")) ;
    for (psrc = &A[0] ; psrc < pfree ; psrc++) assert (*psrc >= 0) ;
    debug_rows = 0 ;
#endif

    /* === Defragment the columns =========================================== */

    pdest = &A[0] ;
    for (c = 0 ; c < n_col ; c++)
    {
	if (COL_IS_ALIVE (c))
	{
	    psrc = &A [Col [c].start] ;

	    /* move and compact the column */
	    assert (pdest <= psrc) ;
	    Col [c].start = (int) (pdest - &A [0]) ;
	    length = Col [c].length ;
	    for (j = 0 ; j < length ; j++)
	    {
		r = *psrc++ ;
		if (ROW_IS_ALIVE (r))
		{
		    *pdest++ = r ;
		}
	    }
	    Col [c].length = (int) (pdest - &A [Col [c].start]) ;
	}
    }

    /* === Prepare to defragment the rows =================================== */

    for (r = 0 ; r < n_row ; r++)
    {
	if (ROW_IS_ALIVE (r))
	{
	    if (Row [r].length == 0)
	    {
		/* this row is of zero length.  cannot compact it, so kill it */
		DEBUG0 (("Defrag row kill\n")) ;
		KILL_ROW (r) ;
	    }
	    else
	    {
		/* save first column index in Row [r].shared2.first_column */
		psrc = &A [Row [r].start] ;
		Row [r].shared2.first_column = *psrc ;
		assert (ROW_IS_ALIVE (r)) ;
		/* flag the start of the row with the one's complement of row */
		*psrc = ONES_COMPLEMENT (r) ;
#ifndef NDEBUG
		debug_rows++ ;
#endif
	    }
	}
    }

    /* === Defragment the rows ============================================== */

    psrc = pdest ;
    while (psrc < pfree)
    {
	/* find a negative number ... the start of a row */
	if (*psrc++ < 0)
	{
	    psrc-- ;
	    /* get the row index */
	    r = ONES_COMPLEMENT (*psrc) ;
	    assert (r >= 0 && r < n_row) ;
	    /* restore first column index */
	    *psrc = Row [r].shared2.first_column ;
	    assert (ROW_IS_ALIVE (r)) ;

	    /* move and compact the row */
	    assert (pdest <= psrc) ;
	    Row [r].start = (int) (pdest - &A [0]) ;
	    length = Row [r].length ;
	    for (j = 0 ; j < length ; j++)
	    {
		c = *psrc++ ;
		if (COL_IS_ALIVE (c))
		{
		    *pdest++ = c ;
		}
	    }
	    Row [r].length = (int) (pdest - &A [Row [r].start]) ;
#ifndef NDEBUG
	    debug_rows-- ;
#endif
	}
    }
    /* ensure we found all the rows */
    assert (debug_rows == 0) ;

    /* === Return the new value of pfree ==================================== */

    return ((int) (pdest - &A [0])) ;
}


/* ========================================================================== */
/* === clear_mark =========================================================== */
/* ========================================================================== */

/*
    Clears the Row [].shared2.mark array, and returns the new tag_mark.
    Return value is the new tag_mark.  Not user-callable.
*/

PRIVATE int clear_mark	/* return the new value for tag_mark */
(
    /* === Parameters ======================================================= */

    int n_row,		/* number of rows in A */
    RowInfo Row []	/* Row [0 ... n_row-1].shared2.mark is set to zero */
)
{
    /* === Local variables ================================================== */

    int r ;

    DEBUG0 (("Clear mark\n")) ;
    for (r = 0 ; r < n_row ; r++)
    {
	if (ROW_IS_ALIVE (r))
	{
	    Row [r].shared2.mark = 0 ;
	}
    }
    return (1) ;
}


/* ========================================================================== */
/* === debugging routines =================================================== */
/* ========================================================================== */

/* When debugging is disabled, the remainder of this file is ignored. */

#ifndef NDEBUG


/* ========================================================================== */
/* === debug_structures ===================================================== */
/* ========================================================================== */

/*
    At this point, all empty rows and columns are dead.  All live columns
    are "clean" (containing no dead rows) and simplicial (no supercolumns
    yet).  Rows may contain dead columns, but all live rows contain at
    least one live column.
*/

PRIVATE void debug_structures
(
    /* === Parameters ======================================================= */

    int n_row,
    int n_col,
    RowInfo Row [],
    ColInfo Col [],
    int A [],
    int n_col2
)
{
    /* === Local variables ================================================== */

    int i ;
    int c ;
    int *cp ;
    int *cp_end ;
    int len ;
    int score ;
    int r ;
    int *rp ;
    int *rp_end ;
    int deg ;

    /* === Check A, Row, and Col ============================================ */

    for (c = 0 ; c < n_col ; c++)
    {
	if (COL_IS_ALIVE (c))
	{
	    len = Col [c].length ;
	    score = Col [c].shared2.score ;
	    DEBUG4 (("initial live col %5d %5d %5d\n", c, len, score)) ;
	    assert (len > 0) ;
	    assert (score >= 0) ;
	    assert (Col [c].shared1.thickness == 1) ;
	    cp = &A [Col [c].start] ;
	    cp_end = cp + len ;
	    while (cp < cp_end)
	    {
		r = *cp++ ;
		assert (ROW_IS_ALIVE (r)) ;
	    }
	}
	else
	{
	    i = Col [c].shared2.order ;
	    assert (i >= n_col2 && i < n_col) ;
	}
    }

    for (r = 0 ; r < n_row ; r++)
    {
	if (ROW_IS_ALIVE (r))
	{
	    i = 0 ;
	    len = Row [r].length ;
	    deg = Row [r].shared1.degree ;
	    assert (len > 0) ;
	    assert (deg > 0) ;
	    rp = &A [Row [r].start] ;
	    rp_end = rp + len ;
	    while (rp < rp_end)
	    {
		c = *rp++ ;
		if (COL_IS_ALIVE (c))
		{
		    i++ ;
		}
	    }
	    assert (i > 0) ;
	}
    }
}


/* ========================================================================== */
/* === debug_deg_lists ====================================================== */
/* ========================================================================== */

/*
    Prints the contents of the degree lists.  Counts the number of columns
    in the degree list and compares it to the total it should have.  Also
    checks the row degrees.
*/

PRIVATE void debug_deg_lists
(
    /* === Parameters ======================================================= */

    int n_row,
    int n_col,
    RowInfo Row [],
    ColInfo Col [],
    int head [],
    int min_score,
    int should,
    int max_deg
)
{
    /* === Local variables ================================================== */

    int deg ;
    int col ;
    int have ;
    int row ;

    /* === Check the degree lists =========================================== */

    if (n_col > 10000 && debug_colamd <= 0)
    {
	return ;
    }
    have = 0 ;
    DEBUG4 (("Degree lists: %d\n", min_score)) ;
    for (deg = 0 ; deg <= n_col ; deg++)
    {
	col = head [deg] ;
	if (col == EMPTY)
	{
	    continue ;
	}
	DEBUG4 (("%d:", deg)) ;
	while (col != EMPTY)
	{
	    DEBUG4 ((" %d", col)) ;
	    have += Col [col].shared1.thickness ;
	    assert (COL_IS_ALIVE (col)) ;
	    col = Col [col].shared4.degree_next ;
	}
	DEBUG4 (("\n")) ;
    }
    DEBUG4 (("should %d have %d\n", should, have)) ;
    assert (should == have) ;

    /* === Check the row degrees ============================================ */

    if (n_row > 10000 && debug_colamd <= 0)
    {
	return ;
    }
    for (row = 0 ; row < n_row ; row++)
    {
	if (ROW_IS_ALIVE (row))
	{
	    assert (Row [row].shared1.degree <= max_deg) ;
	}
    }
}


/* ========================================================================== */
/* === debug_mark =========================================================== */
/* ========================================================================== */

/*
    Ensures that the tag_mark is less that the maximum and also ensures that
    each entry in the mark array is less than the tag mark.
*/

PRIVATE void debug_mark
(
    /* === Parameters ======================================================= */

    int n_row,
    RowInfo Row [],
    int tag_mark,
    int max_mark
)
{
    /* === Local variables ================================================== */

    int r ;

    /* === Check the Row marks ============================================== */

    assert (tag_mark > 0 && tag_mark <= max_mark) ;
    if (n_row > 10000 && debug_colamd <= 0)
    {
	return ;
    }
    for (r = 0 ; r < n_row ; r++)
    {
	assert (Row [r].shared2.mark < tag_mark) ;
    }
}


/* ========================================================================== */
/* === debug_matrix ========================================================= */
/* ========================================================================== */

/*
    Prints out the contents of the columns and the rows.
*/

PRIVATE void debug_matrix
(
    /* === Parameters ======================================================= */

    int n_row,
    int n_col,
    RowInfo Row [],
    ColInfo Col [],
    int A []
)
{
    /* === Local variables ================================================== */

    int r ;
    int c ;
    int *rp ;
    int *rp_end ;
    int *cp ;
    int *cp_end ;

    /* === Dump the rows and columns of the matrix ========================== */

    if (debug_colamd < 3)
    {
	return ;
    }
    DEBUG3 (("DUMP MATRIX:\n")) ;
    for (r = 0 ; r < n_row ; r++)
    {
	DEBUG3 (("Row %d alive? %d\n", r, ROW_IS_ALIVE (r))) ;
	if (ROW_IS_DEAD (r))
	{
	    continue ;
	}
	DEBUG3 (("start %d length %d degree %d\n",
		Row [r].start, Row [r].length, Row [r].shared1.degree)) ;
	rp = &A [Row [r].start] ;
	rp_end = rp + Row [r].length ;
	while (rp < rp_end)
	{
	    c = *rp++ ;
	    DEBUG3 (("	%d col %d\n", COL_IS_ALIVE (c), c)) ;
	}
    }

    for (c = 0 ; c < n_col ; c++)
    {
	DEBUG3 (("Col %d alive? %d\n", c, COL_IS_ALIVE (c))) ;
	if (COL_IS_DEAD (c))
	{
	    continue ;
	}
	DEBUG3 (("start %d length %d shared1 %d shared2 %d\n",
		Col [c].start, Col [c].length,
		Col [c].shared1.thickness, Col [c].shared2.score)) ;
	cp = &A [Col [c].start] ;
	cp_end = cp + Col [c].length ;
	while (cp < cp_end)
	{
	    r = *cp++ ;
	    DEBUG3 (("	%d row %d\n", ROW_IS_ALIVE (r), r)) ;
	}
    }
}

#endif

