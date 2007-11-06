/* generated code, do not edit. */

#include "ode/matrix.h"

/* solve L^T * x=b, with b containing 1 right hand side.
 * L is an n*n lower triangular matrix with ones on the diagonal.
 * L is stored by rows and its leading dimension is lskip.
 * b is an n*1 matrix that contains the right hand side.
 * b is overwritten with x.
 * this processes blocks of 4.
 */

void dSolveL1T (const dReal *L, dReal *B, int n, int lskip1)
{  
  /* declare variables - Z matrix, p and q vectors, etc */
  dReal Z11,m11,Z21,m21,Z31,m31,Z41,m41,p1,q1,p2,p3,p4,*ex;
  const dReal *ell;
  int lskip2,lskip3,i,j;
  /* special handling for L and B because we're solving L1 *transpose* */
  L = L + (n-1)*(lskip1+1);
  B = B + n-1;
  lskip1 = -lskip1;
  /* compute lskip values */
  lskip2 = 2*lskip1;
  lskip3 = 3*lskip1;
  /* compute all 4 x 1 blocks of X */
  for (i=0; i <= n-4; i+=4) {
    /* compute all 4 x 1 block of X, from rows i..i+4-1 */
    /* set the Z matrix to 0 */
    Z11=0;
    Z21=0;
    Z31=0;
    Z41=0;
    ell = L - i;
    ex = B;
    /* the inner loop that computes outer products and adds them to Z */
    for (j=i-4; j >= 0; j -= 4) {
      /* load p and q values */
      p1=ell[0];
      q1=ex[0];
      p2=ell[-1];
      p3=ell[-2];
      p4=ell[-3];
      /* compute outer product and add it to the Z matrix */
      m11 = p1 * q1;
      m21 = p2 * q1;
      m31 = p3 * q1;
      m41 = p4 * q1;
      ell += lskip1;
      Z11 += m11;
      Z21 += m21;
      Z31 += m31;
      Z41 += m41;
      /* load p and q values */
      p1=ell[0];
      q1=ex[-1];
      p2=ell[-1];
      p3=ell[-2];
      p4=ell[-3];
      /* compute outer product and add it to the Z matrix */
      m11 = p1 * q1;
      m21 = p2 * q1;
      m31 = p3 * q1;
      m41 = p4 * q1;
      ell += lskip1;
      Z11 += m11;
      Z21 += m21;
      Z31 += m31;
      Z41 += m41;
      /* load p and q values */
      p1=ell[0];
      q1=ex[-2];
      p2=ell[-1];
      p3=ell[-2];
      p4=ell[-3];
      /* compute outer product and add it to the Z matrix */
      m11 = p1 * q1;
      m21 = p2 * q1;
      m31 = p3 * q1;
      m41 = p4 * q1;
      ell += lskip1;
      Z11 += m11;
      Z21 += m21;
      Z31 += m31;
      Z41 += m41;
      /* load p and q values */
      p1=ell[0];
      q1=ex[-3];
      p2=ell[-1];
      p3=ell[-2];
      p4=ell[-3];
      /* compute outer product and add it to the Z matrix */
      m11 = p1 * q1;
      m21 = p2 * q1;
      m31 = p3 * q1;
      m41 = p4 * q1;
      ell += lskip1;
      ex -= 4;
      Z11 += m11;
      Z21 += m21;
      Z31 += m31;
      Z41 += m41;
      /* end of inner loop */
    }
    /* compute left-over iterations */
    j += 4;
    for (; j > 0; j--) {
      /* load p and q values */
      p1=ell[0];
      q1=ex[0];
      p2=ell[-1];
      p3=ell[-2];
      p4=ell[-3];
      /* compute outer product and add it to the Z matrix */
      m11 = p1 * q1;
      m21 = p2 * q1;
      m31 = p3 * q1;
      m41 = p4 * q1;
      ell += lskip1;
      ex -= 1;
      Z11 += m11;
      Z21 += m21;
      Z31 += m31;
      Z41 += m41;
    }
    /* finish computing the X(i) block */
    Z11 = ex[0] - Z11;
    ex[0] = Z11;
    p1 = ell[-1];
    Z21 = ex[-1] - Z21 - p1*Z11;
    ex[-1] = Z21;
    p1 = ell[-2];
    p2 = ell[-2+lskip1];
    Z31 = ex[-2] - Z31 - p1*Z11 - p2*Z21;
    ex[-2] = Z31;
    p1 = ell[-3];
    p2 = ell[-3+lskip1];
    p3 = ell[-3+lskip2];
    Z41 = ex[-3] - Z41 - p1*Z11 - p2*Z21 - p3*Z31;
    ex[-3] = Z41;
    /* end of outer loop */
  }
  /* compute rows at end that are not a multiple of block size */
  for (; i < n; i++) {
    /* compute all 1 x 1 block of X, from rows i..i+1-1 */
    /* set the Z matrix to 0 */
    Z11=0;
    ell = L - i;
    ex = B;
    /* the inner loop that computes outer products and adds them to Z */
    for (j=i-4; j >= 0; j -= 4) {
      /* load p and q values */
      p1=ell[0];
      q1=ex[0];
      /* compute outer product and add it to the Z matrix */
      m11 = p1 * q1;
      ell += lskip1;
      Z11 += m11;
      /* load p and q values */
      p1=ell[0];
      q1=ex[-1];
      /* compute outer product and add it to the Z matrix */
      m11 = p1 * q1;
      ell += lskip1;
      Z11 += m11;
      /* load p and q values */
      p1=ell[0];
      q1=ex[-2];
      /* compute outer product and add it to the Z matrix */
      m11 = p1 * q1;
      ell += lskip1;
      Z11 += m11;
      /* load p and q values */
      p1=ell[0];
      q1=ex[-3];
      /* compute outer product and add it to the Z matrix */
      m11 = p1 * q1;
      ell += lskip1;
      ex -= 4;
      Z11 += m11;
      /* end of inner loop */
    }
    /* compute left-over iterations */
    j += 4;
    for (; j > 0; j--) {
      /* load p and q values */
      p1=ell[0];
      q1=ex[0];
      /* compute outer product and add it to the Z matrix */
      m11 = p1 * q1;
      ell += lskip1;
      ex -= 1;
      Z11 += m11;
    }
    /* finish computing the X(i) block */
    Z11 = ex[0] - Z11;
    ex[0] = Z11;
  }
}
