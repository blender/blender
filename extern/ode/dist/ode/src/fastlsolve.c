/* generated code, do not edit. */

#include "ode/matrix.h"

/* solve L*X=B, with B containing 1 right hand sides.
 * L is an n*n lower triangular matrix with ones on the diagonal.
 * L is stored by rows and its leading dimension is lskip.
 * B is an n*1 matrix that contains the right hand sides.
 * B is stored by columns and its leading dimension is also lskip.
 * B is overwritten with X.
 * this processes blocks of 4*4.
 * if this is in the factorizer source file, n must be a multiple of 4.
 */

void dSolveL1 (const dReal *L, dReal *B, int n, int lskip1)
{  
  /* declare variables - Z matrix, p and q vectors, etc */
  dReal Z11,Z21,Z31,Z41,p1,q1,p2,p3,p4,*ex;
  const dReal *ell;
  int lskip2,lskip3,i,j;
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
    ell = L + i*lskip1;
    ex = B;
    /* the inner loop that computes outer products and adds them to Z */
    for (j=i-12; j >= 0; j -= 12) {
      /* load p and q values */
      p1=ell[0];
      q1=ex[0];
      p2=ell[lskip1];
      p3=ell[lskip2];
      p4=ell[lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* load p and q values */
      p1=ell[1];
      q1=ex[1];
      p2=ell[1+lskip1];
      p3=ell[1+lskip2];
      p4=ell[1+lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* load p and q values */
      p1=ell[2];
      q1=ex[2];
      p2=ell[2+lskip1];
      p3=ell[2+lskip2];
      p4=ell[2+lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* load p and q values */
      p1=ell[3];
      q1=ex[3];
      p2=ell[3+lskip1];
      p3=ell[3+lskip2];
      p4=ell[3+lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* load p and q values */
      p1=ell[4];
      q1=ex[4];
      p2=ell[4+lskip1];
      p3=ell[4+lskip2];
      p4=ell[4+lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* load p and q values */
      p1=ell[5];
      q1=ex[5];
      p2=ell[5+lskip1];
      p3=ell[5+lskip2];
      p4=ell[5+lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* load p and q values */
      p1=ell[6];
      q1=ex[6];
      p2=ell[6+lskip1];
      p3=ell[6+lskip2];
      p4=ell[6+lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* load p and q values */
      p1=ell[7];
      q1=ex[7];
      p2=ell[7+lskip1];
      p3=ell[7+lskip2];
      p4=ell[7+lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* load p and q values */
      p1=ell[8];
      q1=ex[8];
      p2=ell[8+lskip1];
      p3=ell[8+lskip2];
      p4=ell[8+lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* load p and q values */
      p1=ell[9];
      q1=ex[9];
      p2=ell[9+lskip1];
      p3=ell[9+lskip2];
      p4=ell[9+lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* load p and q values */
      p1=ell[10];
      q1=ex[10];
      p2=ell[10+lskip1];
      p3=ell[10+lskip2];
      p4=ell[10+lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* load p and q values */
      p1=ell[11];
      q1=ex[11];
      p2=ell[11+lskip1];
      p3=ell[11+lskip2];
      p4=ell[11+lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* advance pointers */
      ell += 12;
      ex += 12;
      /* end of inner loop */
    }
    /* compute left-over iterations */
    j += 12;
    for (; j > 0; j--) {
      /* load p and q values */
      p1=ell[0];
      q1=ex[0];
      p2=ell[lskip1];
      p3=ell[lskip2];
      p4=ell[lskip3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      Z21 += p2 * q1;
      Z31 += p3 * q1;
      Z41 += p4 * q1;
      /* advance pointers */
      ell += 1;
      ex += 1;
    }
    /* finish computing the X(i) block */
    Z11 = ex[0] - Z11;
    ex[0] = Z11;
    p1 = ell[lskip1];
    Z21 = ex[1] - Z21 - p1*Z11;
    ex[1] = Z21;
    p1 = ell[lskip2];
    p2 = ell[1+lskip2];
    Z31 = ex[2] - Z31 - p1*Z11 - p2*Z21;
    ex[2] = Z31;
    p1 = ell[lskip3];
    p2 = ell[1+lskip3];
    p3 = ell[2+lskip3];
    Z41 = ex[3] - Z41 - p1*Z11 - p2*Z21 - p3*Z31;
    ex[3] = Z41;
    /* end of outer loop */
  }
  /* compute rows at end that are not a multiple of block size */
  for (; i < n; i++) {
    /* compute all 1 x 1 block of X, from rows i..i+1-1 */
    /* set the Z matrix to 0 */
    Z11=0;
    ell = L + i*lskip1;
    ex = B;
    /* the inner loop that computes outer products and adds them to Z */
    for (j=i-12; j >= 0; j -= 12) {
      /* load p and q values */
      p1=ell[0];
      q1=ex[0];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* load p and q values */
      p1=ell[1];
      q1=ex[1];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* load p and q values */
      p1=ell[2];
      q1=ex[2];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* load p and q values */
      p1=ell[3];
      q1=ex[3];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* load p and q values */
      p1=ell[4];
      q1=ex[4];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* load p and q values */
      p1=ell[5];
      q1=ex[5];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* load p and q values */
      p1=ell[6];
      q1=ex[6];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* load p and q values */
      p1=ell[7];
      q1=ex[7];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* load p and q values */
      p1=ell[8];
      q1=ex[8];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* load p and q values */
      p1=ell[9];
      q1=ex[9];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* load p and q values */
      p1=ell[10];
      q1=ex[10];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* load p and q values */
      p1=ell[11];
      q1=ex[11];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* advance pointers */
      ell += 12;
      ex += 12;
      /* end of inner loop */
    }
    /* compute left-over iterations */
    j += 12;
    for (; j > 0; j--) {
      /* load p and q values */
      p1=ell[0];
      q1=ex[0];
      /* compute outer product and add it to the Z matrix */
      Z11 += p1 * q1;
      /* advance pointers */
      ell += 1;
      ex += 1;
    }
    /* finish computing the X(i) block */
    Z11 = ex[0] - Z11;
    ex[0] = Z11;
  }
}
