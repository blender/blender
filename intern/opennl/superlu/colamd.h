/* ========================================================================== */
/* === colamd prototypes and definitions ==================================== */
/* ========================================================================== */

/*
    This is the colamd include file,

	http://www.cise.ufl.edu/~davis/colamd/colamd.h

    for use in the colamd.c, colamdmex.c, and symamdmex.c files located at

	http://www.cise.ufl.edu/~davis/colamd/

    See those files for a description of colamd and symamd, and for the
    copyright notice, which also applies to this file.

    August 3, 1998.  Version 1.0.
*/

/* ========================================================================== */
/* === Definitions ========================================================== */
/* ========================================================================== */

/* size of the knobs [ ] array.  Only knobs [0..1] are currently used. */
#define COLAMD_KNOBS 20

/* number of output statistics.  Only A [0..2] are currently used. */
#define COLAMD_STATS 20

/* knobs [0] and A [0]: dense row knob and output statistic. */
#define COLAMD_DENSE_ROW 0

/* knobs [1] and A [1]: dense column knob and output statistic. */
#define COLAMD_DENSE_COL 1

/* A [2]: memory defragmentation count output statistic */
#define COLAMD_DEFRAG_COUNT 2

/* A [3]: whether or not the input columns were jumbled or had duplicates */
#define COLAMD_JUMBLED_COLS 3

/* ========================================================================== */
/* === Prototypes of user-callable routines ================================= */
/* ========================================================================== */

int colamd_recommended		/* returns recommended value of Alen */
(
    int nnz,			/* nonzeros in A */
    int n_row,			/* number of rows in A */
    int n_col			/* number of columns in A */
) ;

void colamd_set_defaults	/* sets default parameters */
(				/* knobs argument is modified on output */
    double knobs [COLAMD_KNOBS]	/* parameter settings for colamd */
) ;

int colamd			/* returns TRUE if successful, FALSE otherwise*/
(				/* A and p arguments are modified on output */
    int n_row,			/* number of rows in A */
    int n_col,			/* number of columns in A */
    int Alen,			/* size of the array A */
    int A [],			/* row indices of A, of size Alen */
    int p [],			/* column pointers of A, of size n_col+1 */
    double knobs [COLAMD_KNOBS]	/* parameter settings for colamd */
) ;

