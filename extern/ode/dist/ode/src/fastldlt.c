/* generated code, do not edit. */

#include "ode/matrix.h"

/* solve L*X=B, with B containing 1 right hand sides.
 * L is an n*n lower triangular matrix with ones on the diagonal.
 * L is stored by rows and its leading dimension is lskip.
 * B is an n*1 matrix that contains the right hand sides.
 * B is stored by columns and its leading dimension is also lskip.
 * B is overwritten with X.
 * this processes blocks of 2*2.
 * if this is in the factorizer source file, n must be a multiple of 2.
 */

static void dSolveL1_1 (const dReal *L, dReal *B, int n, int lskip1)
{  
  /* declare variables - Z matrix, p and q vectors, etc */
  dReal Z11,m11,Z21,m21,p1,q1,p2,*ex;
  const dReal *ell;
  int i,j;
  /* compute all 2 x 1 blocks of X */
  for (i=0; i < n; i+=2) {
    /* compute all 2 x 1 block of X, from rows i..i+2-1 */
    /* set the Z matrix to 0 */
    Z11=0;
    Z21=0;
    ell = L + i*lskip1;
    ex = B;
    /* the inner loop that computes outer products and adds them to Z */
    for (j=i-2; j >= 0; j -= 2) {
      /* compute outer product and add it to the Z matrix */
      p1=ell[0];
      q1=ex[0];
      m11 = p1 * q1;
      p2=ell[lskip1];
      m21 = p2 * q1;
      Z11 += m11;
      Z21 += m21;
      /* compute outer product and add it to the Z matrix */
      p1=ell[1];
      q1=ex[1];
      m11 = p1 * q1;
      p2=ell[1+lskip1];
      m21 = p2 * q1;
      /* advance pointers */
      ell += 2;
      ex += 2;
      Z11 += m11;
      Z21 += m21;
      /* end of inner loop */
    }
    /* compute left-over iterations */
    j += 2;
    for (; j > 0; j--) {
      /* compute outer product and add it to the Z matrix */
      p1=ell[0];
      q1=ex[0];
      m11 = p1 * q1;
      p2=ell[lskip1];
      m21 = p2 * q1;
      /* advance pointers */
      ell += 1;
      ex += 1;
      Z11 += m11;
      Z21 += m21;
    }
    /* finish computing the X(i) block */
    Z11 = ex[0] - Z11;
    ex[0] = Z11;
    p1 = ell[lskip1];
    Z21 = ex[1] - Z21 - p1*Z11;
    ex[1] = Z21;
    /* end of outer loop */
  }
}

/* solve L*X=B, with B containing 2 right hand sides.
 * L is an n*n lower triangular matrix with ones on the diagonal.
 * L is stored by rows and its leading dimension is lskip.
 * B is an n*2 matrix that contains the right hand sides.
 * B is stored by columns and its leading dimension is also lskip.
 * B is overwritten with X.
 * this processes blocks of 2*2.
 * if this is in the factorizer source file, n must be a multiple of 2.
 */

static void dSolveL1_2 (const dReal *L, dReal *B, int n, int lskip1)
{  
  /* declare variables - Z matrix, p and q vectors, etc */
  dReal Z11,m11,Z12,m12,Z21,m21,Z22,m22,p1,q1,p2,q2,*ex;
  const dReal *ell;
  int i,j;
  /* compute all 2 x 2 blocks of X */
  for (i=0; i < n; i+=2) {
    /* compute all 2 x 2 block of X, from rows i..i+2-1 */
    /* set the Z matrix to 0 */
    Z11=0;
    Z12=0;
    Z21=0;
    Z22=0;
    ell = L + i*lskip1;
    ex = B;
    /* the inner loop that computes outer products and adds them to Z */
    for (j=i-2; j >= 0; j -= 2) {
      /* compute outer product and add it to the Z matrix */
      p1=ell[0];
      q1=ex[0];
      m11 = p1 * q1;
      q2=ex[lskip1];
      m12 = p1 * q2;
      p2=ell[lskip1];
      m21 = p2 * q1;
      m22 = p2 * q2;
      Z11 += m11;
      Z12 += m12;
      Z21 += m21;
      Z22 += m22;
      /* compute outer product and add it to the Z matrix */
      p1=ell[1];
      q1=ex[1];
      m11 = p1 * q1;
      q2=ex[1+lskip1];
      m12 = p1 * q2;
      p2=ell[1+lskip1];
      m21 = p2 * q1;
      m22 = p2 * q2;
      /* advance pointers */
      ell += 2;
      ex += 2;
      Z11 += m11;
      Z12 += m12;
      Z21 += m21;
      Z22 += m22;
      /* end of inner loop */
    }
    /* compute left-over iterations */
    j += 2;
    for (; j > 0; j--) {
      /* compute outer product and add it to the Z matrix */
      p1=ell[0];
      q1=ex[0];
      m11 = p1 * q1;
      q2=ex[lskip1];
      m12 = p1 * q2;
      p2=ell[lskip1];
      m21 = p2 * q1;
      m22 = p2 * q2;
      /* advance pointers */
      ell += 1;
      ex += 1;
      Z11 += m11;
      Z12 += m12;
      Z21 += m21;
      Z22 += m22;
    }
    /* finish computing the X(i) block */
    Z11 = ex[0] - Z11;
    ex[0] = Z11;
    Z12 = ex[lskip1] - Z12;
    ex[lskip1] = Z12;
    p1 = ell[lskip1];
    Z21 = ex[1] - Z21 - p1*Z11;
    ex[1] = Z21;
    Z22 = ex[1+lskip1] - Z22 - p1*Z12;
    ex[1+lskip1] = Z22;
    /* end of outer loop */
  }
}


void dFactorLDLT (dReal *A, dReal *d, int n, int nskip1)
{  
  int i,j;
  dReal sum,*ell,*dee,dd,p1,p2,q1,q2,Z11,m11,Z21,m21,Z22,m22;
  if (n < 1) return;
  
  for (i=0; i<=n-2; i += 2) {
    /* solve L*(D*l)=a, l is scaled elements in 2 x i block at A(i,0) */
    dSolveL1_2 (A,A+i*nskip1,i,nskip1);
    /* scale the elements in a 2 x i block at A(i,0), and also */
    /* compute Z = the outer product matrix that we'll need. */
    Z11 = 0;
    Z21 = 0;
    Z22 = 0;
    ell = A+i*nskip1;
    dee = d;
    for (j=i-6; j >= 0; j -= 6) {
      p1 = ell[0];
      p2 = ell[nskip1];
      dd = dee[0];
      q1 = p1*dd;
      q2 = p2*dd;
      ell[0] = q1;
      ell[nskip1] = q2;
      m11 = p1*q1;
      m21 = p2*q1;
      m22 = p2*q2;
      Z11 += m11;
      Z21 += m21;
      Z22 += m22;
      p1 = ell[1];
      p2 = ell[1+nskip1];
      dd = dee[1];
      q1 = p1*dd;
      q2 = p2*dd;
      ell[1] = q1;
      ell[1+nskip1] = q2;
      m11 = p1*q1;
      m21 = p2*q1;
      m22 = p2*q2;
      Z11 += m11;
      Z21 += m21;
      Z22 += m22;
      p1 = ell[2];
      p2 = ell[2+nskip1];
      dd = dee[2];
      q1 = p1*dd;
      q2 = p2*dd;
      ell[2] = q1;
      ell[2+nskip1] = q2;
      m11 = p1*q1;
      m21 = p2*q1;
      m22 = p2*q2;
      Z11 += m11;
      Z21 += m21;
      Z22 += m22;
      p1 = ell[3];
      p2 = ell[3+nskip1];
      dd = dee[3];
      q1 = p1*dd;
      q2 = p2*dd;
      ell[3] = q1;
      ell[3+nskip1] = q2;
      m11 = p1*q1;
      m21 = p2*q1;
      m22 = p2*q2;
      Z11 += m11;
      Z21 += m21;
      Z22 += m22;
      p1 = ell[4];
      p2 = ell[4+nskip1];
      dd = dee[4];
      q1 = p1*dd;
      q2 = p2*dd;
      ell[4] = q1;
      ell[4+nskip1] = q2;
      m11 = p1*q1;
      m21 = p2*q1;
      m22 = p2*q2;
      Z11 += m11;
      Z21 += m21;
      Z22 += m22;
      p1 = ell[5];
      p2 = ell[5+nskip1];
      dd = dee[5];
      q1 = p1*dd;
      q2 = p2*dd;
      ell[5] = q1;
      ell[5+nskip1] = q2;
      m11 = p1*q1;
      m21 = p2*q1;
      m22 = p2*q2;
      Z11 += m11;
      Z21 += m21;
      Z22 += m22;
      ell += 6;
      dee += 6;
    }
    /* compute left-over iterations */
    j += 6;
    for (; j > 0; j--) {
      p1 = ell[0];
      p2 = ell[nskip1];
      dd = dee[0];
      q1 = p1*dd;
      q2 = p2*dd;
      ell[0] = q1;
      ell[nskip1] = q2;
      m11 = p1*q1;
      m21 = p2*q1;
      m22 = p2*q2;
      Z11 += m11;
      Z21 += m21;
      Z22 += m22;
      ell++;
      dee++;
    }
    /* solve for diagonal 2 x 2 block at A(i,i) */
    Z11 = ell[0] - Z11;
    Z21 = ell[nskip1] - Z21;
    Z22 = ell[1+nskip1] - Z22;
    dee = d + i;
    /* factorize 2 x 2 block Z,dee */
    /* factorize row 1 */
    dee[0] = dRecip(Z11);
    /* factorize row 2 */
    sum = 0;
    q1 = Z21;
    q2 = q1 * dee[0];
    Z21 = q2;
    sum += q1*q2;
    dee[1] = dRecip(Z22 - sum);
    /* done factorizing 2 x 2 block */
    ell[nskip1] = Z21;
  }
  /* compute the (less than 2) rows at the bottom */
  switch (n-i) {
    case 0:
    break;
    
    case 1:
    dSolveL1_1 (A,A+i*nskip1,i,nskip1);
    /* scale the elements in a 1 x i block at A(i,0), and also */
    /* compute Z = the outer product matrix that we'll need. */
    Z11 = 0;
    ell = A+i*nskip1;
    dee = d;
    for (j=i-6; j >= 0; j -= 6) {
      p1 = ell[0];
      dd = dee[0];
      q1 = p1*dd;
      ell[0] = q1;
      m11 = p1*q1;
      Z11 += m11;
      p1 = ell[1];
      dd = dee[1];
      q1 = p1*dd;
      ell[1] = q1;
      m11 = p1*q1;
      Z11 += m11;
      p1 = ell[2];
      dd = dee[2];
      q1 = p1*dd;
      ell[2] = q1;
      m11 = p1*q1;
      Z11 += m11;
      p1 = ell[3];
      dd = dee[3];
      q1 = p1*dd;
      ell[3] = q1;
      m11 = p1*q1;
      Z11 += m11;
      p1 = ell[4];
      dd = dee[4];
      q1 = p1*dd;
      ell[4] = q1;
      m11 = p1*q1;
      Z11 += m11;
      p1 = ell[5];
      dd = dee[5];
      q1 = p1*dd;
      ell[5] = q1;
      m11 = p1*q1;
      Z11 += m11;
      ell += 6;
      dee += 6;
    }
    /* compute left-over iterations */
    j += 6;
    for (; j > 0; j--) {
      p1 = ell[0];
      dd = dee[0];
      q1 = p1*dd;
      ell[0] = q1;
      m11 = p1*q1;
      Z11 += m11;
      ell++;
      dee++;
    }
    /* solve for diagonal 1 x 1 block at A(i,i) */
    Z11 = ell[0] - Z11;
    dee = d + i;
    /* factorize 1 x 1 block Z,dee */
    /* factorize row 1 */
    dee[0] = dRecip(Z11);
    /* done factorizing 1 x 1 block */
    break;
    
    default: *((char*)0)=0;  /* this should never happen! */
  }
}
