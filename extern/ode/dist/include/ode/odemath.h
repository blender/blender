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

#ifndef _ODE_ODEMATH_H_
#define _ODE_ODEMATH_H_

#include <ode/common.h>

#ifdef __cplusplus
extern "C" {
#endif


/* 3-way dot product. dDOTpq means that elements of `a' and `b' are spaced
 * p and q indexes apart respectively. dDOT() means dDOT11.
 */

#ifdef __cplusplus
inline dReal dDOT (const dReal *a, const dReal *b)
  { return ((a)[0]*(b)[0] + (a)[1]*(b)[1] + (a)[2]*(b)[2]); }
inline dReal dDOT14(const dReal *a, const dReal *b)
  { return ((a)[0]*(b)[0] + (a)[1]*(b)[4] + (a)[2]*(b)[8]); }
inline dReal dDOT41(const dReal *a, const dReal *b)
  { return ((a)[0]*(b)[0] + (a)[4]*(b)[1] + (a)[8]*(b)[2]); }
inline dReal dDOT44(const dReal *a, const dReal *b)
  { return ((a)[0]*(b)[0] + (a)[4]*(b)[4] + (a)[8]*(b)[8]); }
#else
#define dDOT(a,b)   ((a)[0]*(b)[0] + (a)[1]*(b)[1] + (a)[2]*(b)[2])
#define dDOT14(a,b) ((a)[0]*(b)[0] + (a)[1]*(b)[4] + (a)[2]*(b)[8])
#define dDOT41(a,b) ((a)[0]*(b)[0] + (a)[4]*(b)[1] + (a)[8]*(b)[2])
#define dDOT44(a,b) ((a)[0]*(b)[0] + (a)[4]*(b)[4] + (a)[8]*(b)[8])
#endif


/* cross product, set a = b x c. dCROSSpqr means that elements of `a', `b'
 * and `c' are spaced p, q and r indexes apart respectively.
 * dCROSS() means dCROSS111. `op' is normally `=', but you can set it to
 * +=, -= etc to get other effects.
 */

#define dCROSS(a,op,b,c) \
  (a)[0] op ((b)[1]*(c)[2] - (b)[2]*(c)[1]); \
  (a)[1] op ((b)[2]*(c)[0] - (b)[0]*(c)[2]); \
  (a)[2] op ((b)[0]*(c)[1] - (b)[1]*(c)[0]);
#define dCROSSpqr(a,op,b,c,p,q,r) \
  (a)[  0] op ((b)[  q]*(c)[2*r] - (b)[2*q]*(c)[  r]); \
  (a)[  p] op ((b)[2*q]*(c)[  0] - (b)[  0]*(c)[2*r]); \
  (a)[2*p] op ((b)[  0]*(c)[  r] - (b)[  q]*(c)[  0]);
#define dCROSS114(a,op,b,c) dCROSSpqr(a,op,b,c,1,1,4)
#define dCROSS141(a,op,b,c) dCROSSpqr(a,op,b,c,1,4,1)
#define dCROSS144(a,op,b,c) dCROSSpqr(a,op,b,c,1,4,4)
#define dCROSS411(a,op,b,c) dCROSSpqr(a,op,b,c,4,1,1)
#define dCROSS414(a,op,b,c) dCROSSpqr(a,op,b,c,4,1,4)
#define dCROSS441(a,op,b,c) dCROSSpqr(a,op,b,c,4,4,1)
#define dCROSS444(a,op,b,c) dCROSSpqr(a,op,b,c,4,4,4)


/* set a 3x3 submatrix of A to a matrix such that submatrix(A)*b = a x b.
 * A is stored by rows, and has `skip' elements per row. the matrix is
 * assumed to be already zero, so this does not write zero elements!
 * if (plus,minus) is (+,-) then a positive version will be written.
 * if (plus,minus) is (-,+) then a negative version will be written.
 */

#define dCROSSMAT(A,a,skip,plus,minus) \
  (A)[1] = minus (a)[2]; \
  (A)[2] = plus (a)[1]; \
  (A)[(skip)+0] = plus (a)[2]; \
  (A)[(skip)+2] = minus (a)[0]; \
  (A)[2*(skip)+0] = minus (a)[1]; \
  (A)[2*(skip)+1] = plus (a)[0];


/* compute the distance between two 3-vectors (oops, C++!) */
#ifdef __cplusplus
inline dReal dDISTANCE (const dVector3 a, const dVector3 b)
  { return dSqrt( (a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) +
		   (a[2]-b[2])*(a[2]-b[2]) ); }
#else
#define dDISTANCE(a,b) \
 (dSqrt( ((a)[0]-(b)[0])*((a)[0]-(b)[0]) + ((a)[1]-(b)[1])*((a)[1]-(b)[1]) + \
	 ((a)[2]-(b)[2])*((a)[2]-(b)[2]) ))
#endif


/* normalize 3x1 and 4x1 vectors (i.e. scale them to unit length) */
void dNormalize3 (dVector3 a);
void dNormalize4 (dVector4 a);


/* given a unit length "normal" vector n, generate vectors p and q vectors
 * that are an orthonormal basis for the plane space perpendicular to n.
 * i.e. this makes p,q such that n,p,q are all perpendicular to each other.
 * q will equal n x p. if n is not unit length then p will be unit length but
 * q wont be.
 */

void dPlaneSpace (const dVector3 n, dVector3 p, dVector3 q);


/* special case matrix multipication, with operator selection */

#define dMULTIPLYOP0_331(A,op,B,C) \
  (A)[0] op dDOT((B),(C)); \
  (A)[1] op dDOT((B+4),(C)); \
  (A)[2] op dDOT((B+8),(C));
#define dMULTIPLYOP1_331(A,op,B,C) \
  (A)[0] op dDOT41((B),(C)); \
  (A)[1] op dDOT41((B+1),(C)); \
  (A)[2] op dDOT41((B+2),(C));
#define dMULTIPLYOP0_133(A,op,B,C) \
  (A)[0] op dDOT14((B),(C)); \
  (A)[1] op dDOT14((B),(C+1)); \
  (A)[2] op dDOT14((B),(C+2));
#define dMULTIPLYOP0_333(A,op,B,C) \
  (A)[0] op dDOT14((B),(C)); \
  (A)[1] op dDOT14((B),(C+1)); \
  (A)[2] op dDOT14((B),(C+2)); \
  (A)[4] op dDOT14((B+4),(C)); \
  (A)[5] op dDOT14((B+4),(C+1)); \
  (A)[6] op dDOT14((B+4),(C+2)); \
  (A)[8] op dDOT14((B+8),(C)); \
  (A)[9] op dDOT14((B+8),(C+1)); \
  (A)[10] op dDOT14((B+8),(C+2));
#define dMULTIPLYOP1_333(A,op,B,C) \
  (A)[0] op dDOT44((B),(C)); \
  (A)[1] op dDOT44((B),(C+1)); \
  (A)[2] op dDOT44((B),(C+2)); \
  (A)[4] op dDOT44((B+1),(C)); \
  (A)[5] op dDOT44((B+1),(C+1)); \
  (A)[6] op dDOT44((B+1),(C+2)); \
  (A)[8] op dDOT44((B+2),(C)); \
  (A)[9] op dDOT44((B+2),(C+1)); \
  (A)[10] op dDOT44((B+2),(C+2));
#define dMULTIPLYOP2_333(A,op,B,C) \
  (A)[0] op dDOT((B),(C)); \
  (A)[1] op dDOT((B),(C+4)); \
  (A)[2] op dDOT((B),(C+8)); \
  (A)[4] op dDOT((B+4),(C)); \
  (A)[5] op dDOT((B+4),(C+4)); \
  (A)[6] op dDOT((B+4),(C+8)); \
  (A)[8] op dDOT((B+8),(C)); \
  (A)[9] op dDOT((B+8),(C+4)); \
  (A)[10] op dDOT((B+8),(C+8));

#ifdef __cplusplus

inline void dMULTIPLY0_331(dReal *A, const dReal *B, const dReal *C)
  { dMULTIPLYOP0_331(A,=,B,C) }
inline void dMULTIPLY1_331(dReal *A, const dReal *B, const dReal *C)
  { dMULTIPLYOP1_331(A,=,B,C) }
inline void dMULTIPLY0_133(dReal *A, const dReal *B, const dReal *C)
  { dMULTIPLYOP0_133(A,=,B,C) }
inline void dMULTIPLY0_333(dReal *A, const dReal *B, const dReal *C)
  { dMULTIPLYOP0_333(A,=,B,C) }
inline void dMULTIPLY1_333(dReal *A, const dReal *B, const dReal *C)
  { dMULTIPLYOP1_333(A,=,B,C) }
inline void dMULTIPLY2_333(dReal *A, const dReal *B, const dReal *C)
  { dMULTIPLYOP2_333(A,=,B,C) }

inline void dMULTIPLYADD0_331(dReal *A, const dReal *B, const dReal *C)
  { dMULTIPLYOP0_331(A,+=,B,C) }
inline void dMULTIPLYADD1_331(dReal *A, const dReal *B, const dReal *C)
  { dMULTIPLYOP1_331(A,+=,B,C) }
inline void dMULTIPLYADD0_133(dReal *A, const dReal *B, const dReal *C)
  { dMULTIPLYOP0_133(A,+=,B,C) }
inline void dMULTIPLYADD0_333(dReal *A, const dReal *B, const dReal *C)
  { dMULTIPLYOP0_333(A,+=,B,C) }
inline void dMULTIPLYADD1_333(dReal *A, const dReal *B, const dReal *C)
  { dMULTIPLYOP1_333(A,+=,B,C) }
inline void dMULTIPLYADD2_333(dReal *A, const dReal *B, const dReal *C)
  { dMULTIPLYOP2_333(A,+=,B,C) }

#else

#define dMULTIPLY0_331(A,B,C) dMULTIPLYOP0_331(A,=,B,C)
#define dMULTIPLY1_331(A,B,C) dMULTIPLYOP1_331(A,=,B,C)
#define dMULTIPLY0_133(A,B,C) dMULTIPLYOP0_133(A,=,B,C)
#define dMULTIPLY0_333(A,B,C) dMULTIPLYOP0_333(A,=,B,C)
#define dMULTIPLY1_333(A,B,C) dMULTIPLYOP1_333(A,=,B,C)
#define dMULTIPLY2_333(A,B,C) dMULTIPLYOP2_333(A,=,B,C)

#define dMULTIPLYADD0_331(A,B,C) dMULTIPLYOP0_331(A,+=,B,C)
#define dMULTIPLYADD1_331(A,B,C) dMULTIPLYOP1_331(A,+=,B,C)
#define dMULTIPLYADD0_133(A,B,C) dMULTIPLYOP0_133(A,+=,B,C)
#define dMULTIPLYADD0_333(A,B,C) dMULTIPLYOP0_333(A,+=,B,C)
#define dMULTIPLYADD1_333(A,B,C) dMULTIPLYOP1_333(A,+=,B,C)
#define dMULTIPLYADD2_333(A,B,C) dMULTIPLYOP2_333(A,+=,B,C)

#endif


#ifdef __cplusplus
}
#endif

#endif
