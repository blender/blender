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

/* optimized and unoptimized vector and matrix functions */

#ifndef _ODE_MATRIX_H_
#define _ODE_MATRIX_H_

#include <ode/common.h>


#ifdef __cplusplus
extern "C" {
#endif


/* set a vector/matrix of size n to all zeros, or to a specific value. */

void dSetZero (dReal *a, int n);
void dSetValue (dReal *a, int n, dReal value);


/* get the dot product of two n*1 vectors. if n <= 0 then
 * zero will be returned (in which case a and b need not be valid).
 */

dReal dDot (const dReal *a, const dReal *b, int n);


/* get the dot products of (a0,b), (a1,b), etc and return them in outsum.
 * all vectors are n*1. if n <= 0 then zeroes will be returned (in which case
 * the input vectors need not be valid). this function is somewhat faster
 * than calling dDot() for all of the combinations separately.
 */

/* NOT INCLUDED in the library for now.
void dMultidot2 (const dReal *a0, const dReal *a1,
		 const dReal *b, dReal *outsum, int n);
*/


/* matrix multiplication. all matrices are stored in standard row format.
 * the digit refers to the argument that is transposed:
 *   0:   A = B  * C   (sizes: A:p*r B:p*q C:q*r)
 *   1:   A = B' * C   (sizes: A:p*r B:q*p C:q*r)
 *   2:   A = B  * C'  (sizes: A:p*r B:p*q C:r*q)
 * case 1,2 are equivalent to saying that the operation is A=B*C but
 * B or C are stored in standard column format.
 */

void dMultiply0 (dReal *A, const dReal *B, const dReal *C, int p,int q,int r);
void dMultiply1 (dReal *A, const dReal *B, const dReal *C, int p,int q,int r);
void dMultiply2 (dReal *A, const dReal *B, const dReal *C, int p,int q,int r);


/* do an in-place cholesky decomposition on the lower triangle of the n*n
 * symmetric matrix A (which is stored by rows). the resulting lower triangle
 * will be such that L*L'=A. return 1 on success and 0 on failure (on failure
 * the matrix is not positive definite).
 */

int dFactorCholesky (dReal *A, int n);


/* solve for x: L*L'*x = b, and put the result back into x.
 * L is size n*n, b is size n*1. only the lower triangle of L is considered.
 */

void dSolveCholesky (const dReal *L, dReal *b, int n);


/* compute the inverse of the n*n positive definite matrix A and put it in
 * Ainv. this is not especially fast. this returns 1 on success (A was
 * positive definite) or 0 on failure (not PD).
 */

int dInvertPDMatrix (const dReal *A, dReal *Ainv, int n);


/* check whether an n*n matrix A is positive definite, return 1/0 (yes/no).
 * positive definite means that x'*A*x > 0 for any x. this performs a
 * cholesky decomposition of A. if the decomposition fails then the matrix
 * is not positive definite. A is stored by rows. A is not altered.
 */

int dIsPositiveDefinite (const dReal *A, int n);


/* factorize a matrix A into L*D*L', where L is lower triangular with ones on
 * the diagonal, and D is diagonal.
 * A is an n*n matrix stored by rows, with a leading dimension of n rounded
 * up to 4. L is written into the strict lower triangle of A (the ones are not
 * written) and the reciprocal of the diagonal elements of D are written into
 * d.
 */
void dFactorLDLT (dReal *A, dReal *d, int n, int nskip);


/* solve L*x=b, where L is n*n lower triangular with ones on the diagonal,
 * and x,b are n*1. b is overwritten with x.
 * the leading dimension of L is `nskip'.
 */
void dSolveL1 (const dReal *L, dReal *b, int n, int nskip);


/* solve L'*x=b, where L is n*n lower triangular with ones on the diagonal,
 * and x,b are n*1. b is overwritten with x.
 * the leading dimension of L is `nskip'.
 */
void dSolveL1T (const dReal *L, dReal *b, int n, int nskip);


/* in matlab syntax: a(1:n) = a(1:n) .* d(1:n) */

void dVectorScale (dReal *a, const dReal *d, int n);


/* given `L', a n*n lower triangular matrix with ones on the diagonal,
 * and `d', a n*1 vector of the reciprocal diagonal elements of an n*n matrix
 * D, solve L*D*L'*x=b where x,b are n*1. x overwrites b.
 * the leading dimension of L is `nskip'.
 */

void dSolveLDLT (const dReal *L, const dReal *d, dReal *b, int n, int nskip);


/* given an L*D*L' factorization of an n*n matrix A, return the updated
 * factorization L2*D2*L2' of A plus the following "top left" matrix:
 *
 *    [ b a' ]     <-- b is a[0]
 *    [ a 0  ]     <-- a is a[1..n-1]
 *
 *   - L has size n*n, its leading dimension is nskip. L is lower triangular
 *     with ones on the diagonal. only the lower triangle of L is referenced.
 *   - d has size n. d contains the reciprocal diagonal elements of D.
 *   - a has size n.
 * the result is written into L, except that the left column of L and d[0]
 * are not actually modified. see ldltaddTL.m for further comments. 
 */
void dLDLTAddTL (dReal *L, dReal *d, const dReal *a, int n, int nskip);


/* given an L*D*L' factorization of a permuted matrix A, produce a new
 * factorization for row and column `r' removed.
 *   - A has size n1*n1, its leading dimension in nskip. A is symmetric and
 *     positive definite. only the lower triangle of A is referenced.
 *     A itself may actually be an array of row pointers.
 *   - L has size n2*n2, its leading dimension in nskip. L is lower triangular
 *     with ones on the diagonal. only the lower triangle of L is referenced.
 *   - d has size n2. d contains the reciprocal diagonal elements of D.
 *   - p is a permutation vector. it contains n2 indexes into A. each index
 *     must be in the range 0..n1-1.
 *   - r is the row/column of L to remove.
 * the new L will be written within the old L, i.e. will have the same leading
 * dimension. the last row and column of L, and the last element of d, are
 * undefined on exit.
 *
 * a fast O(n^2) algorithm is used. see ldltremove.m for further comments.
 */
void dLDLTRemove (dReal **A, const int *p, dReal *L, dReal *d,
		  int n1, int n2, int r, int nskip);


/* given an n*n matrix A (with leading dimension nskip), remove the r'th row
 * and column by moving elements. the new matrix will have the same leading
 * dimension. the last row and column of A are untouched on exit.
 */
void dRemoveRowCol (dReal *A, int n, int nskip, int r);


#ifdef __cplusplus
}
#endif

#endif
