/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.       *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org          *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of EITHER:                                  *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *       Software Foundation; either version 2.1 of the License, or (at  *
 *       your option) any later version. The text of the GNU Lesser      *
 *       General Public License is included with this library in the     *
 *       file LICENSE.TXT.                                               *
 *   (2) The BSD-style license that is included with this library in     *
 *       the file LICENSE-BSD.TXT.                                       *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.                     *
 *                                                                       *
 *************************************************************************/

/*


THE ALGORITHM
-------------

solve A*x = b+w, with x and w subject to certain LCP conditions.
each x(i),w(i) must lie on one of the three line segments in the following
diagram. each line segment corresponds to one index set :

     w(i)
     /|\      |           :
      |       |           :
      |       |i in N     :
  w>0 |       |state[i]=0 :
      |       |           :
      |       |           :  i in C
  w=0 +       +-----------------------+
      |                   :           |
      |                   :           |
  w<0 |                   :           |i in N
      |                   :           |state[i]=1
      |                   :           |
      |                   :           |
      +-------|-----------|-----------|----------> x(i)
             lo           0           hi

the Dantzig algorithm proceeds as follows:
  for i=1:n
    * if (x(i),w(i)) is not on the line, push x(i) and w(i) positive or
      negative towards the line. as this is done, the other (x(j),w(j))
      for j<i are constrained to be on the line. if any (x,w) reaches the
      end of a line segment then it is switched between index sets.
    * i is added to the appropriate index set depending on what line segment
      it hits.

we restrict lo(i) <= 0 and hi(i) >= 0. this makes the algorithm a bit
simpler, because the starting point for x(i),w(i) is always on the dotted
line x=0 and x will only ever increase in one direction, so it can only hit
two out of the three line segments.


NOTES
-----

this is an implementation of "lcp_dantzig2_ldlt.m" and "lcp_dantzig_lohi.m".
the implementation is split into an LCP problem object (dLCP) and an LCP
driver function. most optimization occurs in the dLCP object.

a naive implementation of the algorithm requires either a lot of data motion
or a lot of permutation-array lookup, because we are constantly re-ordering
rows and columns. to avoid this and make a more optimized algorithm, a
non-trivial data structure is used to represent the matrix A (this is
implemented in the fast version of the dLCP object).

during execution of this algorithm, some indexes in A are clamped (set C),
some are non-clamped (set N), and some are "don't care" (where x=0).
A,x,b,w (and other problem vectors) are permuted such that the clamped
indexes are first, the unclamped indexes are next, and the don't-care
indexes are last. this permutation is recorded in the array `p'.
initially p = 0..n-1, and as the rows and columns of A,x,b,w are swapped,
the corresponding elements of p are swapped.

because the C and N elements are grouped together in the rows of A, we can do
lots of work with a fast dot product function. if A,x,etc were not permuted
and we only had a permutation array, then those dot products would be much
slower as we would have a permutation array lookup in some inner loops.

A is accessed through an array of row pointers, so that element (i,j) of the
permuted matrix is A[i][j]. this makes row swapping fast. for column swapping
we still have to actually move the data.

during execution of this algorithm we maintain an L*D*L' factorization of
the clamped submatrix of A (call it `AC') which is the top left nC*nC
submatrix of A. there are two ways we could arrange the rows/columns in AC.

(1) AC is always permuted such that L*D*L' = AC. this causes a problem
    when a row/column is removed from C, because then all the rows/columns of A
    between the deleted index and the end of C need to be rotated downward.
    this results in a lot of data motion and slows things down.
(2) L*D*L' is actually a factorization of a *permutation* of AC (which is
    itself a permutation of the underlying A). this is what we do - the
    permutation is recorded in the vector C. call this permutation A[C,C].
    when a row/column is removed from C, all we have to do is swap two
    rows/columns and manipulate C.

*/

#include <ode/common.h>
#include "lcp.h"
#include <ode/matrix.h>
#include <ode/misc.h>
#include "mat.h"		// for testing
#include <ode/timer.h>		// for testing

//***************************************************************************
// code generation parameters

// LCP debugging (mosty for fast dLCP) - this slows things down a lot
//#define DEBUG_LCP

//#define dLCP_SLOW		// use slow dLCP object
#define dLCP_FAST		// use fast dLCP object

// option 1 : matrix row pointers (less data copying)
#define ROWPTRS
#define ATYPE dReal **
#define AROW(i) (A[i])

// option 2 : no matrix row pointers (slightly faster inner loops)
//#define NOROWPTRS
//#define ATYPE dReal *
//#define AROW(i) (A+(i)*nskip)

// misc defines
#define ALLOCA dALLOCA16
//#define dDot myDot
#define NUB_OPTIMIZATIONS

//***************************************************************************

// an alternative inline dot product, for speed comparisons

static inline dReal myDot (dReal *a, dReal *b, int n)
{
  dReal sum=0;
  while (n > 0) {
    sum += (*a) * (*b);
    a++;
    b++;
    n--;
  }
  return sum;
}


// swap row/column i1 with i2 in the n*n matrix A. the leading dimension of
// A is nskip. this only references and swaps the lower triangle.
// if `do_fast_row_swaps' is nonzero and row pointers are being used, then
// rows will be swapped by exchanging row pointers. otherwise the data will
// be copied.

static void swapRowsAndCols (ATYPE A, int n, int i1, int i2, int nskip,
			     int do_fast_row_swaps)
{
  int i;
  dIASSERT (A && n > 0 && i1 >= 0 && i2 >= 0 && i1 < n && i2 < n &&
	    nskip >= n && i1 < i2);

# ifdef ROWPTRS
  for (i=i1+1; i<i2; i++) A[i1][i] = A[i][i1];
  for (i=i1+1; i<i2; i++) A[i][i1] = A[i2][i];
  A[i1][i2] = A[i1][i1];
  A[i1][i1] = A[i2][i1];
  A[i2][i1] = A[i2][i2];
  // swap rows, by swapping row pointers
  if (do_fast_row_swaps) {
    dReal *tmpp;
    tmpp = A[i1];
    A[i1] = A[i2];
    A[i2] = tmpp;
  }
  else {
    dReal *tmprow = (dReal*) ALLOCA (n * sizeof(dReal));
    memcpy (tmprow,A[i1],n * sizeof(dReal));
    memcpy (A[i1],A[i2],n * sizeof(dReal));
    memcpy (A[i2],tmprow,n * sizeof(dReal));
  }
  // swap columns the hard way
  for (i=i2+1; i<n; i++) {
    dReal tmp = A[i][i1];
    A[i][i1] = A[i][i2];
    A[i][i2] = tmp;
  }
# else
  dReal tmp,*tmprow = (dReal*) ALLOCA (n * sizeof(dReal));
  if (i1 > 0) {
    memcpy (tmprow,A+i1*nskip,i1*sizeof(dReal));
    memcpy (A+i1*nskip,A+i2*nskip,i1*sizeof(dReal));
    memcpy (A+i2*nskip,tmprow,i1*sizeof(dReal));
  }
  for (i=i1+1; i<i2; i++) {
    tmp = A[i2*nskip+i];
    A[i2*nskip+i] = A[i*nskip+i1];
    A[i*nskip+i1] = tmp;
  }
  tmp = A[i1*nskip+i1];
  A[i1*nskip+i1] = A[i2*nskip+i2];
  A[i2*nskip+i2] = tmp;
  for (i=i2+1; i<n; i++) {
    tmp = A[i*nskip+i1];
    A[i*nskip+i1] = A[i*nskip+i2];
    A[i*nskip+i2] = tmp;
  }
# endif
}


// swap two indexes in the n*n LCP problem. i1 must be <= i2.

static void swapProblem (ATYPE A, dReal *x, dReal *b, dReal *w, dReal *lo,
			 dReal *hi, int *p, int *state, int *findex,
			 int n, int i1, int i2, int nskip,
			 int do_fast_row_swaps)
{
  dReal tmp;
  int tmpi;
  dIASSERT (n>0 && i1 >=0 && i2 >= 0 && i1 < n && i2 < n && nskip >= n &&
	    i1 <= i2);
  if (i1==i2) return;
  swapRowsAndCols (A,n,i1,i2,nskip,do_fast_row_swaps);
  tmp = x[i1];
  x[i1] = x[i2];
  x[i2] = tmp;
  tmp = b[i1];
  b[i1] = b[i2];
  b[i2] = tmp;
  tmp = w[i1];
  w[i1] = w[i2];
  w[i2] = tmp;
  tmp = lo[i1];
  lo[i1] = lo[i2];
  lo[i2] = tmp;
  tmp = hi[i1];
  hi[i1] = hi[i2];
  hi[i2] = tmp;
  tmpi = p[i1];
  p[i1] = p[i2];
  p[i2] = tmpi;
  tmpi = state[i1];
  state[i1] = state[i2];
  state[i2] = tmpi;
  if (findex) {
    tmpi = findex[i1];
    findex[i1] = findex[i2];
    findex[i2] = tmpi;
  }
}


// for debugging - check that L,d is the factorization of A[C,C].
// A[C,C] has size nC*nC and leading dimension nskip.
// L has size nC*nC and leading dimension nskip.
// d has size nC.

#ifdef DEBUG_LCP

static void checkFactorization (ATYPE A, dReal *_L, dReal *_d,
				int nC, int *C, int nskip)
{
  int i,j;
  if (nC==0) return;

  // get A1=A, copy the lower triangle to the upper triangle, get A2=A[C,C]
  dMatrix A1 (nC,nC);
  for (i=0; i<nC; i++) {
    for (j=0; j<=i; j++) A1(i,j) = A1(j,i) = AROW(i)[j];
  }
  dMatrix A2 = A1.select (nC,C,nC,C);

  // printf ("A1=\n"); A1.print(); printf ("\n");
  // printf ("A2=\n"); A2.print(); printf ("\n");

  // compute A3 = L*D*L'
  dMatrix L (nC,nC,_L,nskip,1);
  dMatrix D (nC,nC);
  for (i=0; i<nC; i++) D(i,i) = 1/_d[i];
  L.clearUpperTriangle();
  for (i=0; i<nC; i++) L(i,i) = 1;
  dMatrix A3 = L * D * L.transpose();

  // printf ("L=\n"); L.print(); printf ("\n");
  // printf ("D=\n"); D.print(); printf ("\n");
  // printf ("A3=\n"); A2.print(); printf ("\n");

  // compare A2 and A3
  dReal diff = A2.maxDifference (A3);
  if (diff > 1e-8)
    dDebug (0,"L*D*L' check, maximum difference = %.6e\n",diff);
}

#endif


// for debugging

#ifdef DEBUG_LCP

static void checkPermutations (int i, int n, int nC, int nN, int *p, int *C)
{
  int j,k;
  dIASSERT (nC>=0 && nN>=0 && (nC+nN)==i && i < n);
  for (k=0; k<i; k++) dIASSERT (p[k] >= 0 && p[k] < i);
  for (k=i; k<n; k++) dIASSERT (p[k] == k);
  for (j=0; j<nC; j++) {
    int C_is_bad = 1;
    for (k=0; k<nC; k++) if (C[k]==j) C_is_bad = 0;
    dIASSERT (C_is_bad==0);
  }
}

#endif

//***************************************************************************
// dLCP manipulator object. this represents an n*n LCP problem.
//
// two index sets C and N are kept. each set holds a subset of
// the variable indexes 0..n-1. an index can only be in one set.
// initially both sets are empty.
//
// the index set C is special: solutions to A(C,C)\A(C,i) can be generated.

#ifdef dLCP_SLOW

// simple but slow implementation of dLCP, for testing the LCP drivers.

#include "array.h"

struct dLCP {
  int n,nub,nskip;
  dReal *Adata,*x,*b,*w,*lo,*hi;	// LCP problem data
  ATYPE A;				// A rows
  dArray<int> C,N;			// index sets
  int last_i_for_solve1;		// last i value given to solve1

  dLCP (int _n, int _nub, dReal *_Adata, dReal *_x, dReal *_b, dReal *_w,
	dReal *_lo, dReal *_hi, dReal *_L, dReal *_d,
	dReal *_Dell, dReal *_ell, dReal *_tmp,
	int *_state, int *_findex, int *_p, int *_C, dReal **Arows);
  // the constructor is given an initial problem description (A,x,b,w) and
  // space for other working data (which the caller may allocate on the stack).
  // some of this data is specific to the fast dLCP implementation.
  // the matrices A and L have size n*n, vectors have size n*1.
  // A represents a symmetric matrix but only the lower triangle is valid.
  // `nub' is the number of unbounded indexes at the start. all the indexes
  // 0..nub-1 will be put into C.

  ~dLCP();

  int getNub() { return nub; }
  // return the value of `nub'. the constructor may want to change it,
  // so the caller should find out its new value.

  // transfer functions: transfer index i to the given set (C or N). indexes
  // less than `nub' can never be given. A,x,b,w,etc may be permuted by these
  // functions, the caller must be robust to this.

  void transfer_i_to_C (int i);
    // this assumes C and N span 1:i-1. this also assumes that solve1() has
    // been recently called for the same i without any other transfer
    // functions in between (thereby allowing some data reuse for the fast
    // implementation).
  void transfer_i_to_N (int i);
    // this assumes C and N span 1:i-1.
  void transfer_i_from_N_to_C (int i);
  void transfer_i_from_C_to_N (int i);

  int numC();
  int numN();
  // return the number of indexes in set C/N

  int indexC (int i);
  int indexN (int i);
  // return index i in set C/N.

  // accessor and arithmetic functions. Aij translates as A(i,j), etc.
  // make sure that only the lower triangle of A is ever referenced.

  dReal Aii (int i);
  dReal AiC_times_qC (int i, dReal *q);
  dReal AiN_times_qN (int i, dReal *q);			// for all Nj
  void pN_equals_ANC_times_qC (dReal *p, dReal *q);	// for all Nj
  void pN_plusequals_ANi (dReal *p, int i, int sign=1);
    // for all Nj. sign = +1,-1. assumes i > maximum index in N.
  void pC_plusequals_s_times_qC (dReal *p, dReal s, dReal *q);
  void pN_plusequals_s_times_qN (dReal *p, dReal s, dReal *q); // for all Nj
  void solve1 (dReal *a, int i, int dir=1, int only_transfer=0);
    // get a(C) = - dir * A(C,C) \ A(C,i). dir must be +/- 1.
    // the fast version of this function computes some data that is needed by
    // transfer_i_to_C(). if only_transfer is nonzero then this function
    // *only* computes that data, it does not set a(C).

  void unpermute();
  // call this at the end of the LCP function. if the x/w values have been
  // permuted then this will unscramble them.
};


dLCP::dLCP (int _n, int _nub, dReal *_Adata, dReal *_x, dReal *_b, dReal *_w,
	    dReal *_lo, dReal *_hi, dReal *_L, dReal *_d,
	    dReal *_Dell, dReal *_ell, dReal *_tmp,
	    int *_state, int *_findex, int *_p, int *_C, dReal **Arows)
{
  dUASSERT (_findex==0,"slow dLCP object does not support findex array");

  n = _n;
  nub = _nub;
  Adata = _Adata;
  A = 0;
  x = _x;
  b = _b;
  w = _w;
  lo = _lo;
  hi = _hi;
  nskip = dPAD(n);
  dSetZero (x,n);
  last_i_for_solve1 = -1;

  int i,j;
  C.setSize (n);
  N.setSize (n);
  for (int i=0; i<n; i++) {
    C[i] = 0;
    N[i] = 0;
  }

# ifdef ROWPTRS
  // make matrix row pointers
  A = Arows;
  for (i=0; i<n; i++) A[i] = Adata + i*nskip;
# else
  A = Adata;
# endif

  // lets make A symmetric
  for (i=0; i<n; i++) {
    for (j=i+1; j<n; j++) AROW(i)[j] = AROW(j)[i];
  }

  // if nub>0, put all indexes 0..nub-1 into C and solve for x
  if (nub > 0) {
    for (i=0; i<nub; i++) memcpy (_L+i*nskip,AROW(i),(i+1)*sizeof(dReal));
    dFactorLDLT (_L,_d,nub,nskip);
    memcpy (x,b,nub*sizeof(dReal));
    dSolveLDLT (_L,_d,x,nub,nskip);
    dSetZero (_w,nub);
    for (i=0; i<nub; i++) C[i] = 1;
  }
}


dLCP::~dLCP()
{
}


void dLCP::transfer_i_to_C (int i)
{
  if (i < nub) dDebug (0,"bad i");
  if (C[i]) dDebug (0,"i already in C");
  if (N[i]) dDebug (0,"i already in N");
  for (int k=0; k<i; k++) {
    if (!(C[k] ^ N[k])) dDebug (0,"assumptions for C and N violated");
  }
  for (int k=i; k<n; k++)
    if (C[k] || N[k]) dDebug (0,"assumptions for C and N violated");
  if (i != last_i_for_solve1) dDebug (0,"assumptions for i violated");
  last_i_for_solve1 = -1;
  C[i] = 1;
}


void dLCP::transfer_i_to_N (int i)
{
  if (i < nub) dDebug (0,"bad i");
  if (C[i]) dDebug (0,"i already in C");
  if (N[i]) dDebug (0,"i already in N");
  for (int k=0; k<i; k++)
    if (!C[k] && !N[k]) dDebug (0,"assumptions for C and N violated");
  for (int k=i; k<n; k++)
    if (C[k] || N[k]) dDebug (0,"assumptions for C and N violated");
  last_i_for_solve1 = -1;
  N[i] = 1;
}


void dLCP::transfer_i_from_N_to_C (int i)
{
  if (i < nub) dDebug (0,"bad i");
  if (C[i]) dDebug (0,"i already in C");
  if (!N[i]) dDebug (0,"i not in N");
  last_i_for_solve1 = -1;
  N[i] = 0;
  C[i] = 1;
}


void dLCP::transfer_i_from_C_to_N (int i)
{
  if (i < nub) dDebug (0,"bad i");
  if (N[i]) dDebug (0,"i already in N");
  if (!C[i]) dDebug (0,"i not in C");
  last_i_for_solve1 = -1;
  C[i] = 0;
  N[i] = 1;
}


int dLCP::numC()
{
  int i,count=0;
  for (i=0; i<n; i++) if (C[i]) count++;
  return count;
}


int dLCP::numN()
{
  int i,count=0;
  for (i=0; i<n; i++) if (N[i]) count++;
  return count;
}


int dLCP::indexC (int i)
{
  int k,count=0;
  for (k=0; k<n; k++) {
    if (C[k]) {
      if (count==i) return k;
      count++;
    }
  }
  dDebug (0,"bad index C (%d)",i);
  return 0;
}


int dLCP::indexN (int i)
{
  int k,count=0;
  for (k=0; k<n; k++) {
    if (N[k]) {
      if (count==i) return k;
      count++;
    }
  }
  dDebug (0,"bad index into N");
  return 0;
}


dReal dLCP::Aii (int i)
{
  return AROW(i)[i];
}


dReal dLCP::AiC_times_qC (int i, dReal *q)
{
  dReal sum = 0;
  for (int k=0; k<n; k++) if (C[k]) sum += AROW(i)[k] * q[k];
  return sum;
}


dReal dLCP::AiN_times_qN (int i, dReal *q)
{
  dReal sum = 0;
  for (int k=0; k<n; k++) if (N[k]) sum += AROW(i)[k] * q[k];
  return sum;
}


void dLCP::pN_equals_ANC_times_qC (dReal *p, dReal *q)
{
  dReal sum;
  for (int ii=0; ii<n; ii++) if (N[ii]) {
    sum = 0;
    for (int jj=0; jj<n; jj++) if (C[jj]) sum += AROW(ii)[jj] * q[jj];
    p[ii] = sum;
  }
}


void dLCP::pN_plusequals_ANi (dReal *p, int i, int sign)
{
  int k;
  for (k=0; k<n; k++) if (N[k] && k >= i) dDebug (0,"N assumption violated");
  if (sign > 0) {
    for (k=0; k<n; k++) if (N[k]) p[k] += AROW(i)[k];
  }
  else {
    for (k=0; k<n; k++) if (N[k]) p[k] -= AROW(i)[k];
  }
}


void dLCP::pC_plusequals_s_times_qC (dReal *p, dReal s, dReal *q)
{
  for (int k=0; k<n; k++) if (C[k]) p[k] += s*q[k];
}


void dLCP::pN_plusequals_s_times_qN (dReal *p, dReal s, dReal *q)
{
  for (int k=0; k<n; k++) if (N[k]) p[k] += s*q[k];
}


void dLCP::solve1 (dReal *a, int i, int dir, int only_transfer)
{
  dReal *AA = (dReal*) ALLOCA (n*nskip*sizeof(dReal));
  dReal *dd = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *bb = (dReal*) ALLOCA (n*sizeof(dReal));
  int ii,jj,AAi,AAj;

  last_i_for_solve1 = i;
  AAi = 0;
  for (ii=0; ii<n; ii++) if (C[ii]) {
    AAj = 0;
    for (jj=0; jj<n; jj++) if (C[jj]) {
      AA[AAi*nskip+AAj] = AROW(ii)[jj];
      AAj++;
    }
    bb[AAi] = AROW(i)[ii];
    AAi++;
  }
  if (AAi==0) return;

  dFactorLDLT (AA,dd,AAi,nskip);
  dSolveLDLT (AA,dd,bb,AAi,nskip);

  AAi=0;
  if (dir > 0) {
    for (ii=0; ii<n; ii++) if (C[ii]) a[ii] = -bb[AAi++];
  }
  else {
    for (ii=0; ii<n; ii++) if (C[ii]) a[ii] = bb[AAi++];
  }
}


void dLCP::unpermute()
{
}

#endif // dLCP_SLOW

//***************************************************************************
// fast implementation of dLCP. see the above definition of dLCP for
// interface comments.
//
// `p' records the permutation of A,x,b,w,etc. p is initially 1:n and is
// permuted as the other vectors/matrices are permuted.
//
// A,x,b,w,lo,hi,state,findex,p,c are permuted such that sets C,N have
// contiguous indexes. the don't-care indexes follow N.
//
// an L*D*L' factorization is maintained of A(C,C), and whenever indexes are
// added or removed from the set C the factorization is updated.
// thus L*D*L'=A[C,C], i.e. a permuted top left nC*nC submatrix of A.
// the leading dimension of the matrix L is always `nskip'.
//
// at the start there may be other indexes that are unbounded but are not
// included in `nub'. dLCP will permute the matrix so that absolutely all
// unbounded vectors are at the start. thus there may be some initial
// permutation.
//
// the algorithms here assume certain patterns, particularly with respect to
// index transfer.

#ifdef dLCP_FAST

struct dLCP {
  int n,nskip,nub;
  ATYPE A;				// A rows
  dReal *Adata,*x,*b,*w,*lo,*hi;	// permuted LCP problem data
  dReal *L,*d;				// L*D*L' factorization of set C
  dReal *Dell,*ell,*tmp;
  int *state,*findex,*p,*C;
  int nC,nN;				// size of each index set

  dLCP (int _n, int _nub, dReal *_Adata, dReal *_x, dReal *_b, dReal *_w,
	dReal *_lo, dReal *_hi, dReal *_L, dReal *_d,
	dReal *_Dell, dReal *_ell, dReal *_tmp,
	int *_state, int *_findex, int *_p, int *_C, dReal **Arows);
  int getNub() { return nub; }
  void transfer_i_to_C (int i);
  void transfer_i_to_N (int i)
    { nN++; }			// because we can assume C and N span 1:i-1
  void transfer_i_from_N_to_C (int i);
  void transfer_i_from_C_to_N (int i);
  int numC() { return nC; }
  int numN() { return nN; }
  int indexC (int i) { return i; }
  int indexN (int i) { return i+nC; }
  dReal Aii (int i) { return AROW(i)[i]; }
  dReal AiC_times_qC (int i, dReal *q) { return dDot (AROW(i),q,nC); }
  dReal AiN_times_qN (int i, dReal *q) { return dDot (AROW(i)+nC,q+nC,nN); }
  void pN_equals_ANC_times_qC (dReal *p, dReal *q);
  void pN_plusequals_ANi (dReal *p, int i, int sign=1);
  void pC_plusequals_s_times_qC (dReal *p, dReal s, dReal *q)
    { for (int i=0; i<nC; i++) p[i] += s*q[i]; }
  void pN_plusequals_s_times_qN (dReal *p, dReal s, dReal *q)
    { for (int i=0; i<nN; i++) p[i+nC] += s*q[i+nC]; }
  void solve1 (dReal *a, int i, int dir=1, int only_transfer=0);
  void unpermute();
};


dLCP::dLCP (int _n, int _nub, dReal *_Adata, dReal *_x, dReal *_b, dReal *_w,
	    dReal *_lo, dReal *_hi, dReal *_L, dReal *_d,
	    dReal *_Dell, dReal *_ell, dReal *_tmp,
	    int *_state, int *_findex, int *_p, int *_C, dReal **Arows)
{
  n = _n;
  nub = _nub;
  Adata = _Adata;
  A = 0;
  x = _x;
  b = _b;
  w = _w;
  lo = _lo;
  hi = _hi;
  L = _L;
  d = _d;
  Dell = _Dell;
  ell = _ell;
  tmp = _tmp;
  state = _state;
  findex = _findex;
  p = _p;
  C = _C;
  nskip = dPAD(n);
  dSetZero (x,n);

  int k;

# ifdef ROWPTRS
  // make matrix row pointers
  A = Arows;
  for (k=0; k<n; k++) A[k] = Adata + k*nskip;
# else
  A = Adata;
# endif

  nC = 0;
  nN = 0;
  for (k=0; k<n; k++) p[k]=k;		// initially unpermuted

  /*
  // for testing, we can do some random swaps in the area i > nub
  if (nub < n) {
    for (k=0; k<100; k++) {
      int i1,i2;
      do {
	i1 = dRandInt(n-nub)+nub;
	i2 = dRandInt(n-nub)+nub;
      }
      while (i1 > i2); 
      //printf ("--> %d %d\n",i1,i2);
      swapProblem (A,x,b,w,lo,hi,p,state,findex,n,i1,i2,nskip,0);
    }
  }
  */

  // permute the problem so that *all* the unbounded variables are at the
  // start, i.e. look for unbounded variables not included in `nub'. we can
  // potentially push up `nub' this way and get a bigger initial factorization.
  // note that when we swap rows/cols here we must not just swap row pointers,
  // as the initial factorization relies on the data being all in one chunk.
  for (k=nub; k<n; k++) {
    if (lo[k]==-dInfinity && hi[k]==dInfinity) {
      swapProblem (A,x,b,w,lo,hi,p,state,findex,n,nub,k,nskip,0);
      nub++;
    }
  }

  // if there are unbounded variables at the start, factorize A up to that
  // point and solve for x. this puts all indexes 0..nub-1 into C.
  if (nub > 0) {
    for (k=0; k<nub; k++) memcpy (L+k*nskip,AROW(k),(k+1)*sizeof(dReal));
    dFactorLDLT (L,d,nub,nskip);
    memcpy (x,b,nub*sizeof(dReal));
    dSolveLDLT (L,d,x,nub,nskip);
    dSetZero (w,nub);
    for (k=0; k<nub; k++) C[k] = k;
    nC = nub;
  }

  // permute the indexes > nub such that all findex variables are at the end
  if (findex) {
    int num_at_end = 0;
    for (k=n-1; k >= nub; k--) {
      if (findex[k] >= 0) {
	swapProblem (A,x,b,w,lo,hi,p,state,findex,n,k,n-1-num_at_end,nskip,1);
	num_at_end++;
      }
    }
  }

  // print info about indexes
  /*
  for (k=0; k<n; k++) {
    if (k<nub) printf ("C");
    else if (lo[k]==-dInfinity && hi[k]==dInfinity) printf ("c");
    else printf (".");
  }
  printf ("\n");
  */
}


void dLCP::transfer_i_to_C (int i)
{
  int j;
  if (nC > 0) {
    // ell,Dell were computed by solve1(). note, ell = D \ L1solve (L,A(i,C))
    for (j=0; j<nC; j++) L[nC*nskip+j] = ell[j];
    d[nC] = dRecip (AROW(i)[i] - dDot(ell,Dell,nC));
  }
  else {
    d[0] = dRecip (AROW(i)[i]);
  }
  swapProblem (A,x,b,w,lo,hi,p,state,findex,n,nC,i,nskip,1);
  C[nC] = nC;
  nC++;

# ifdef DEBUG_LCP
  checkFactorization (A,L,d,nC,C,nskip);
  if (i < (n-1)) checkPermutations (i+1,n,nC,nN,p,C);
# endif
}


void dLCP::transfer_i_from_N_to_C (int i)
{
  int j;
  if (nC > 0) {
    dReal *aptr = AROW(i);
#   ifdef NUB_OPTIMIZATIONS
    // if nub>0, initial part of aptr unpermuted
    for (j=0; j<nub; j++) Dell[j] = aptr[j];
    for (j=nub; j<nC; j++) Dell[j] = aptr[C[j]];
#   else
    for (j=0; j<nC; j++) Dell[j] = aptr[C[j]];
#   endif
    dSolveL1 (L,Dell,nC,nskip);
    for (j=0; j<nC; j++) ell[j] = Dell[j] * d[j];
    for (j=0; j<nC; j++) L[nC*nskip+j] = ell[j];
    d[nC] = dRecip (AROW(i)[i] - dDot(ell,Dell,nC));
  }
  else {
    d[0] = dRecip (AROW(i)[i]);
  }
  swapProblem (A,x,b,w,lo,hi,p,state,findex,n,nC,i,nskip,1);
  C[nC] = nC;
  nN--;
  nC++;

  // @@@ TO DO LATER
  // if we just finish here then we'll go back and re-solve for
  // delta_x. but actually we can be more efficient and incrementally
  // update delta_x here. but if we do this, we wont have ell and Dell
  // to use in updating the factorization later.

# ifdef DEBUG_LCP
  checkFactorization (A,L,d,nC,C,nskip);
# endif
}


void dLCP::transfer_i_from_C_to_N (int i)
{
  // remove a row/column from the factorization, and adjust the
  // indexes (black magic!)
  int j,k;
  for (j=0; j<nC; j++) if (C[j]==i) {
    dLDLTRemove (A,C,L,d,n,nC,j,nskip);
    for (k=0; k<nC; k++) if (C[k]==nC-1) {
      C[k] = C[j];
      if (j < (nC-1)) memmove (C+j,C+j+1,(nC-j-1)*sizeof(int));
      break;
    }
    dIASSERT (k < nC);
    break;
  }
  dIASSERT (j < nC);
  swapProblem (A,x,b,w,lo,hi,p,state,findex,n,i,nC-1,nskip,1);
  nC--;
  nN++;

# ifdef DEBUG_LCP
  checkFactorization (A,L,d,nC,C,nskip);
# endif
}


void dLCP::pN_equals_ANC_times_qC (dReal *p, dReal *q)
{
  // we could try to make this matrix-vector multiplication faster using
  // outer product matrix tricks, e.g. with the dMultidotX() functions.
  // but i tried it and it actually made things slower on random 100x100
  // problems because of the overhead involved. so we'll stick with the
  // simple method for now.
  for (int i=0; i<nN; i++) p[i+nC] = dDot (AROW(i+nC),q,nC);
}


void dLCP::pN_plusequals_ANi (dReal *p, int i, int sign)
{
  dReal *aptr = AROW(i)+nC;
  if (sign > 0) {
    for (int i=0; i<nN; i++) p[i+nC] += aptr[i];
  }
  else {
    for (int i=0; i<nN; i++) p[i+nC] -= aptr[i];
  }
}


void dLCP::solve1 (dReal *a, int i, int dir, int only_transfer)
{
  // the `Dell' and `ell' that are computed here are saved. if index i is
  // later added to the factorization then they can be reused.
  //
  // @@@ question: do we need to solve for entire delta_x??? yes, but
  //     only if an x goes below 0 during the step.

  int j;
  if (nC > 0) {
    dReal *aptr = AROW(i);
#   ifdef NUB_OPTIMIZATIONS
    // if nub>0, initial part of aptr[] is guaranteed unpermuted
    for (j=0; j<nub; j++) Dell[j] = aptr[j];
    for (j=nub; j<nC; j++) Dell[j] = aptr[C[j]];
#   else
    for (j=0; j<nC; j++) Dell[j] = aptr[C[j]];
#   endif
    dSolveL1 (L,Dell,nC,nskip);
    for (j=0; j<nC; j++) ell[j] = Dell[j] * d[j];

    if (!only_transfer) {
      for (j=0; j<nC; j++) tmp[j] = ell[j];
      dSolveL1T (L,tmp,nC,nskip);
      if (dir > 0) {
	for (j=0; j<nC; j++) a[C[j]] = -tmp[j];
      }
      else {
	for (j=0; j<nC; j++) a[C[j]] = tmp[j];
      }
    }
  }
}


void dLCP::unpermute()
{
  // now we have to un-permute x and w
  int j;
  dReal *tmp = (dReal*) ALLOCA (n*sizeof(dReal));
  memcpy (tmp,x,n*sizeof(dReal));
  for (j=0; j<n; j++) x[p[j]] = tmp[j];
  memcpy (tmp,w,n*sizeof(dReal));
  for (j=0; j<n; j++) w[p[j]] = tmp[j];
}

#endif // dLCP_FAST

//***************************************************************************
// an unoptimized Dantzig LCP driver routine for the basic LCP problem.
// must have lo=0, hi=dInfinity, and nub=0.

void dSolveLCPBasic (int n, dReal *A, dReal *x, dReal *b,
		     dReal *w, int nub, dReal *lo, dReal *hi)
{
  dAASSERT (n>0 && A && x && b && w && nub == 0);

  int i,k;
  int nskip = dPAD(n);
  dReal *L = (dReal*) ALLOCA (n*nskip*sizeof(dReal));
  dReal *d = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *delta_x = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *delta_w = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *Dell = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *ell = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *tmp = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal **Arows = (dReal**) ALLOCA (n*sizeof(dReal*));
  int *p = (int*) ALLOCA (n*sizeof(int));
  int *C = (int*) ALLOCA (n*sizeof(int));
  int *dummy = (int*) ALLOCA (n*sizeof(int));

  dLCP lcp (n,0,A,x,b,w,tmp,tmp,L,d,Dell,ell,tmp,dummy,dummy,p,C,Arows);
  nub = lcp.getNub();

  for (i=0; i<n; i++) {
    w[i] = lcp.AiC_times_qC (i,x) - b[i];
    if (w[i] >= 0) {
      lcp.transfer_i_to_N (i);
    }
    else {
      for (;;) {
	// compute: delta_x(C) = -A(C,C)\A(C,i)
	dSetZero (delta_x,n);
	lcp.solve1 (delta_x,i);
	delta_x[i] = 1;

	// compute: delta_w = A*delta_x
	dSetZero (delta_w,n);
	lcp.pN_equals_ANC_times_qC (delta_w,delta_x);
	lcp.pN_plusequals_ANi (delta_w,i);
        delta_w[i] = lcp.AiC_times_qC (i,delta_x) + lcp.Aii(i);

	// find index to switch
	int si = i;		// si = switch index
	int si_in_N = 0;	// set to 1 if si in N
	dReal s = -w[i]/delta_w[i];

	if (s <= 0) {
	  dMessage (d_ERR_LCP, "LCP internal error, s <= 0 (s=%.4e)",s);
	  if (i < (n-1)) {
	    dSetZero (x+i,n-i);
	    dSetZero (w+i,n-i);
	  }
	  goto done;
	}

	for (k=0; k < lcp.numN(); k++) {
	  if (delta_w[lcp.indexN(k)] < 0) {
	    dReal s2 = -w[lcp.indexN(k)] / delta_w[lcp.indexN(k)];
	    if (s2 < s) {
	      s = s2;
	      si = lcp.indexN(k);
	      si_in_N = 1;
	    }
	  }
	}
	for (k=0; k < lcp.numC(); k++) {
	  if (delta_x[lcp.indexC(k)] < 0) {
	    dReal s2 = -x[lcp.indexC(k)] / delta_x[lcp.indexC(k)];
	    if (s2 < s) {
	      s = s2;
	      si = lcp.indexC(k);
	      si_in_N = 0;
	    }
	  }
	}

	// apply x = x + s * delta_x
	lcp.pC_plusequals_s_times_qC (x,s,delta_x);
	x[i] += s;
	lcp.pN_plusequals_s_times_qN (w,s,delta_w);
	w[i] += s * delta_w[i];

	// switch indexes between sets if necessary
	if (si==i) {
	  w[i] = 0;
	  lcp.transfer_i_to_C (i);
	  break;
	}
	if (si_in_N) {
          w[si] = 0;
	  lcp.transfer_i_from_N_to_C (si);
	}
	else {
          x[si] = 0;
	  lcp.transfer_i_from_C_to_N (si);
	}
      }
    }
  }

 done:
  lcp.unpermute();
}

//***************************************************************************
// an optimized Dantzig LCP driver routine for the lo-hi LCP problem.

void dSolveLCP (int n, dReal *A, dReal *x, dReal *b,
		dReal *w, int nub, dReal *lo, dReal *hi, int *findex)
{
  dAASSERT (n>0 && A && x && b && w && lo && hi && nub >= 0 && nub <= n);
  int i,k,hit_first_friction_index = 0;
  int nskip = dPAD(n);

  // if all the variables are unbounded then we can just factor, solve,
  // and return
  if (nub >= n) {
    dFactorLDLT (A,w,n,nskip);		// use w for d
    dSolveLDLT (A,w,b,n,nskip);
    memcpy (x,b,n*sizeof(dReal));
    dSetZero (w,n);
    return;
  }

# ifndef dNODEBUG
  // check restrictions on lo and hi
  for (k=0; k<n; k++) dIASSERT (lo[k] <= 0 && hi[k] >= 0);
# endif

  dReal *L = (dReal*) ALLOCA (n*nskip*sizeof(dReal));
  dReal *d = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *delta_x = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *delta_w = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *Dell = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *ell = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal **Arows = (dReal**) ALLOCA (n*sizeof(dReal*));
  int *p = (int*) ALLOCA (n*sizeof(int));
  int *C = (int*) ALLOCA (n*sizeof(int));
  int dir;
  dReal dirf;

  // for i in N, state[i] is 0 if x(i)==lo(i) or 1 if x(i)==hi(i)
  int *state = (int*) ALLOCA (n*sizeof(int));

  // create LCP object. note that tmp is set to delta_w to save space, this
  // optimization relies on knowledge of how tmp is used, so be careful!
  dLCP lcp (n,nub,A,x,b,w,lo,hi,L,d,Dell,ell,delta_w,state,findex,p,C,Arows);
  nub = lcp.getNub();

  // loop over all indexes nub..n-1. for index i, if x(i),w(i) satisfy the
  // LCP conditions then i is added to the appropriate index set. otherwise
  // x(i),w(i) is driven either +ve or -ve to force it to the valid region.
  // as we drive x(i), x(C) is also adjusted to keep w(C) at zero.
  // while driving x(i) we maintain the LCP conditions on the other variables
  // 0..i-1. we do this by watching out for other x(i),w(i) values going
  // outside the valid region, and then switching them between index sets
  // when that happens.

  for (i=nub; i<n; i++) {
    // the index i is the driving index and indexes i+1..n-1 are "dont care",
    // i.e. when we make changes to the system those x's will be zero and we
    // don't care what happens to those w's. in other words, we only consider
    // an (i+1)*(i+1) sub-problem of A*x=b+w.

    // if we've hit the first friction index, we have to compute the lo and
    // hi values based on the values of x already computed. we have been
    // permuting the indexes, so the values stored in the findex vector are
    // no longer valid. thus we have to temporarily unpermute the x vector. 
    if (hit_first_friction_index == 0 && findex && findex[i] >= 0) {
      // un-permute x into delta_w, which is not being used at the moment
      for (k=0; k<n; k++) delta_w[p[k]] = x[k];
      // set lo and hi values
      for (k=i; k<n; k++) {
	hi[k] = dFabs (hi[k] * delta_w[findex[k]]);
	lo[k] = -hi[k];
      }
      hit_first_friction_index = 1;
    }

    // thus far we have not even been computing the w values for indexes
    // greater than i, so compute w[i] now.
    w[i] = lcp.AiC_times_qC (i,x) + lcp.AiN_times_qN (i,x) - b[i];

    // if lo=hi=0 (which can happen for tangential friction when normals are
    // 0) then the index will be assigned to set N with some state. however,
    // set C's line has zero size, so the index will always remain in set N.
    // with the "normal" switching logic, if w changed sign then the index
    // would have to switch to set C and then back to set N with an inverted
    // state. this is pointless, and also computationally expensive. to
    // prevent this from happening, we use the rule that indexes with lo=hi=0
    // will never be checked for set changes. this means that the state for
    // these indexes may be incorrect, but that doesn't matter.

    // see if x(i),w(i) is in a valid region
    if (lo[i]==0 && w[i] >= 0) {
      lcp.transfer_i_to_N (i);
      state[i] = 0;
    }
    else if (hi[i]==0 && w[i] <= 0) {
      lcp.transfer_i_to_N (i);
      state[i] = 1;
    }
    else if (w[i]==0) {
      // this is a degenerate case. by the time we get to this test we know
      // that lo != 0, which means that lo < 0 as lo is not allowed to be +ve,
      // and similarly that hi > 0. this means that the line segment
      // corresponding to set C is at least finite in extent, and we are on it.
      // NOTE: we must call lcp.solve1() before lcp.transfer_i_to_C()
      lcp.solve1 (delta_x,i,0,1);
      lcp.transfer_i_to_C (i);
    }
    else {
      // we must push x(i) and w(i)
      for (;;) {
	// find direction to push on x(i)
	if (w[i] <= 0) {
	  dir = 1;
	  dirf = REAL(1.0);
	}
	else {
	  dir = -1;
	  dirf = REAL(-1.0);
	}

	// compute: delta_x(C) = -dir*A(C,C)\A(C,i)
	lcp.solve1 (delta_x,i,dir);
	// note that delta_x[i] = dirf, but we wont bother to set it

	// compute: delta_w = A*delta_x ... note we only care about
        // delta_w(N) and delta_w(i), the rest is ignored
	lcp.pN_equals_ANC_times_qC (delta_w,delta_x);
	lcp.pN_plusequals_ANi (delta_w,i,dir);
        delta_w[i] = lcp.AiC_times_qC (i,delta_x) + lcp.Aii(i)*dirf;

	// find largest step we can take (size=s), either to drive x(i),w(i)
	// to the valid LCP region or to drive an already-valid variable
	// outside the valid region.

	int cmd = 1;		// index switching command
	int si = 0;		// si = index to switch if cmd>3
	dReal s = -w[i]/delta_w[i];
	if (dir > 0) {
	  if (hi[i] < dInfinity) {
	    dReal s2 = (hi[i]-x[i])/dirf;		// step to x(i)=hi(i)
	    if (s2 < s) {
	      s = s2;
	      cmd = 3;
	    }
	  }
	}
	else {
	  if (lo[i] > -dInfinity) {
	    dReal s2 = (lo[i]-x[i])/dirf;		// step to x(i)=lo(i)
	    if (s2 < s) {
	      s = s2;
	      cmd = 2;
	    }
	  }
	}

	for (k=0; k < lcp.numN(); k++) {
	  if ((state[lcp.indexN(k)]==0 && delta_w[lcp.indexN(k)] < 0) ||
	      (state[lcp.indexN(k)]!=0 && delta_w[lcp.indexN(k)] > 0)) {
	    // don't bother checking if lo=hi=0
	    if (lo[lcp.indexN(k)] == 0 && hi[lcp.indexN(k)] == 0) continue;
	    dReal s2 = -w[lcp.indexN(k)] / delta_w[lcp.indexN(k)];
	    if (s2 < s) {
	      s = s2;
	      cmd = 4;
	      si = lcp.indexN(k);
	    }
	  }
	}

	for (k=nub; k < lcp.numC(); k++) {
	  if (delta_x[lcp.indexC(k)] < 0 && lo[lcp.indexC(k)] > -dInfinity) {
	    dReal s2 = (lo[lcp.indexC(k)]-x[lcp.indexC(k)]) /
	      delta_x[lcp.indexC(k)];
	    if (s2 < s) {
	      s = s2;
	      cmd = 5;
	      si = lcp.indexC(k);
	    }
	  }
	  if (delta_x[lcp.indexC(k)] > 0 && hi[lcp.indexC(k)] < dInfinity) {
	    dReal s2 = (hi[lcp.indexC(k)]-x[lcp.indexC(k)]) /
	      delta_x[lcp.indexC(k)];
	    if (s2 < s) {
	      s = s2;
	      cmd = 6;
	      si = lcp.indexC(k);
	    }
	  }
	}

	//static char* cmdstring[8] = {0,"->C","->NL","->NH","N->C",
	//			     "C->NL","C->NH"};
	//printf ("cmd=%d (%s), si=%d\n",cmd,cmdstring[cmd],(cmd>3) ? si : i);

	// if s <= 0 then we've got a problem. if we just keep going then
	// we're going to get stuck in an infinite loop. instead, just cross
	// our fingers and exit with the current solution.
	if (s <= 0) {
	  dMessage (d_ERR_LCP, "LCP internal error, s <= 0 (s=%.4e)",s);
	  if (i < (n-1)) {
	    dSetZero (x+i,n-i);
	    dSetZero (w+i,n-i);
	  }
	  goto done;
	}

	// apply x = x + s * delta_x
	lcp.pC_plusequals_s_times_qC (x,s,delta_x);
	x[i] += s * dirf;

	// apply w = w + s * delta_w
	lcp.pN_plusequals_s_times_qN (w,s,delta_w);
	w[i] += s * delta_w[i];

	// switch indexes between sets if necessary
	switch (cmd) {
	case 1:		// done
	  w[i] = 0;
	  lcp.transfer_i_to_C (i);
	  break;
	case 2:		// done
	  x[i] = lo[i];
	  state[i] = 0;
	  lcp.transfer_i_to_N (i);
	  break;
	case 3:		// done
	  x[i] = hi[i];
	  state[i] = 1;
	  lcp.transfer_i_to_N (i);
	  break;
	case 4:		// keep going
	  w[si] = 0;
	  lcp.transfer_i_from_N_to_C (si);
	  break;
	case 5:		// keep going
	  x[si] = lo[si];
	  state[si] = 0;
	  lcp.transfer_i_from_C_to_N (si);
	  break;
	case 6:		// keep going
	  x[si] = hi[si];
	  state[si] = 1;
	  lcp.transfer_i_from_C_to_N (si);
	  break;
	}

	if (cmd <= 3) break;
      }
    }
  }

 done:
  lcp.unpermute();
}

//***************************************************************************
// accuracy and timing test

extern "C" void dTestSolveLCP()
{
  int n = 100;
  int i,nskip = dPAD(n);
  const dReal tol = REAL(1e-9);
  printf ("dTestSolveLCP()\n");

  dReal *A = (dReal*) ALLOCA (n*nskip*sizeof(dReal));
  dReal *x = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *b = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *w = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *lo = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *hi = (dReal*) ALLOCA (n*sizeof(dReal));

  dReal *A2 = (dReal*) ALLOCA (n*nskip*sizeof(dReal));
  dReal *b2 = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *lo2 = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *hi2 = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *tmp1 = (dReal*) ALLOCA (n*sizeof(dReal));
  dReal *tmp2 = (dReal*) ALLOCA (n*sizeof(dReal));

  double total_time = 0;
  for (int count=0; count < 1000; count++) {

    // form (A,b) = a random positive definite LCP problem
    dMakeRandomMatrix (A2,n,n,1.0);
    dMultiply2 (A,A2,A2,n,n,n);
    dMakeRandomMatrix (x,n,1,1.0);
    dMultiply0 (b,A,x,n,n,1);
    for (i=0; i<n; i++) b[i] += (dRandReal()*REAL(0.2))-REAL(0.1);

    // choose `nub' in the range 0..n-1
    int nub = 50; //dRandInt (n);

    // make limits
    for (i=0; i<nub; i++) lo[i] = -dInfinity;
    for (i=0; i<nub; i++) hi[i] = dInfinity;
    //for (i=nub; i<n; i++) lo[i] = 0;
    //for (i=nub; i<n; i++) hi[i] = dInfinity;
    //for (i=nub; i<n; i++) lo[i] = -dInfinity;
    //for (i=nub; i<n; i++) hi[i] = 0;
    for (i=nub; i<n; i++) lo[i] = -(dRandReal()*REAL(1.0))-REAL(0.01);
    for (i=nub; i<n; i++) hi[i] =  (dRandReal()*REAL(1.0))+REAL(0.01);

    // set a few limits to lo=hi=0
    /*
    for (i=0; i<10; i++) {
      int j = dRandInt (n-nub) + nub;
      lo[j] = 0;
      hi[j] = 0;
    }
    */

    // solve the LCP. we must make copy of A,b,lo,hi (A2,b2,lo2,hi2) for
    // SolveLCP() to permute. also, we'll clear the upper triangle of A2 to
    // ensure that it doesn't get referenced (if it does, the answer will be
    // wrong).

    memcpy (A2,A,n*nskip*sizeof(dReal));
    dClearUpperTriangle (A2,n);
    memcpy (b2,b,n*sizeof(dReal));
    memcpy (lo2,lo,n*sizeof(dReal));
    memcpy (hi2,hi,n*sizeof(dReal));
    dSetZero (x,n);
    dSetZero (w,n);

    dStopwatch sw;
    dStopwatchReset (&sw);
    dStopwatchStart (&sw);

    dSolveLCP (n,A2,x,b2,w,nub,lo2,hi2,0);

    dStopwatchStop (&sw);
    double time = dStopwatchTime(&sw);
    total_time += time;
    double average = total_time / double(count+1) * 1000.0;

    // check the solution

    dMultiply0 (tmp1,A,x,n,n,1);
    for (i=0; i<n; i++) tmp2[i] = b[i] + w[i];
    dReal diff = dMaxDifference (tmp1,tmp2,n,1);
    // printf ("\tA*x = b+w, maximum difference = %.6e - %s (1)\n",diff,
    //	    diff > tol ? "FAILED" : "passed");
    if (diff > tol) dDebug (0,"A*x = b+w, maximum difference = %.6e",diff);
    int n1=0,n2=0,n3=0;
    for (i=0; i<n; i++) {
      if (x[i]==lo[i] && w[i] >= 0) {
	n1++;	// ok
      }
      else if (x[i]==hi[i] && w[i] <= 0) {
	n2++;	// ok
      }
      else if (x[i] >= lo[i] && x[i] <= hi[i] && w[i] == 0) {
	n3++;	// ok
      }
      else {
	dDebug (0,"FAILED: i=%d x=%.4e w=%.4e lo=%.4e hi=%.4e",i,
		x[i],w[i],lo[i],hi[i]);
      }
    }

    // pacifier
    printf ("passed: NL=%3d NH=%3d C=%3d   ",n1,n2,n3);
    printf ("time=%10.3f ms  avg=%10.4f\n",time * 1000.0,average);
  }
}
