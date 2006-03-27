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

#include "SorLcp.h"

#ifdef USE_SOR_SOLVER

// SOR LCP taken from ode quickstep, 
// todo: write own successive overrelaxation gauss-seidel, or jacobi iterative solver


#include "SimdScalar.h"

#include "Dynamics/RigidBody.h"
#include <math.h>
#include <float.h>//FLT_MAX
#ifdef WIN32
#include <memory.h>
#endif
#include <string.h>
#include <stdio.h>

#if defined (WIN32)
#include <malloc.h>
#else
#if defined (__FreeBSD__)
#include <stdlib.h>
#else
#include <alloca.h>
#endif
#endif

#include "Dynamics/BU_Joint.h"
#include "ContactSolverInfo.h"

////////////////////////////////////////////////////////////////////
//math stuff

typedef SimdScalar dVector4[4];
typedef SimdScalar dMatrix3[4*3];
#define dInfinity FLT_MAX



#define dRecip(x) ((float)(1.0f/(x)))				/* reciprocal */



#define dMULTIPLY0_331NEW(A,op,B,C) \
{\
	float tmp[3];\
	tmp[0] = C.getX();\
	tmp[1] = C.getY();\
	tmp[2] = C.getZ();\
	dMULTIPLYOP0_331(A,op,B,tmp);\
}

#define dMULTIPLY0_331(A,B,C) dMULTIPLYOP0_331(A,=,B,C)
#define dMULTIPLYOP0_331(A,op,B,C) \
  (A)[0] op dDOT1((B),(C)); \
  (A)[1] op dDOT1((B+4),(C)); \
  (A)[2] op dDOT1((B+8),(C));

#define dAASSERT ASSERT
#define dIASSERT ASSERT

#define REAL float
#define dDOTpq(a,b,p,q) ((a)[0]*(b)[0] + (a)[p]*(b)[q] + (a)[2*(p)]*(b)[2*(q)])
SimdScalar dDOT1  (const SimdScalar *a, const SimdScalar *b) { return dDOTpq(a,b,1,1); }
#define dDOT14(a,b) dDOTpq(a,b,1,4)

#define dCROSS(a,op,b,c) \
  (a)[0] op ((b)[1]*(c)[2] - (b)[2]*(c)[1]); \
  (a)[1] op ((b)[2]*(c)[0] - (b)[0]*(c)[2]); \
  (a)[2] op ((b)[0]*(c)[1] - (b)[1]*(c)[0]);


#define dMULTIPLYOP2_333(A,op,B,C) \
  (A)[0] op dDOT1((B),(C)); \
  (A)[1] op dDOT1((B),(C+4)); \
  (A)[2] op dDOT1((B),(C+8)); \
  (A)[4] op dDOT1((B+4),(C)); \
  (A)[5] op dDOT1((B+4),(C+4)); \
  (A)[6] op dDOT1((B+4),(C+8)); \
  (A)[8] op dDOT1((B+8),(C)); \
  (A)[9] op dDOT1((B+8),(C+4)); \
  (A)[10] op dDOT1((B+8),(C+8));
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

#define dMULTIPLY2_333(A,B,C) dMULTIPLYOP2_333(A,=,B,C)
#define dMULTIPLY0_333(A,B,C) dMULTIPLYOP0_333(A,=,B,C)
#define dMULTIPLYADD0_331(A,B,C) dMULTIPLYOP0_331(A,+=,B,C)


////////////////////////////////////////////////////////////////////
#define EFFICIENT_ALIGNMENT 16
#define dEFFICIENT_SIZE(x) ((((x)-1)|(EFFICIENT_ALIGNMENT-1))+1)
/* alloca aligned to the EFFICIENT_ALIGNMENT. note that this can waste
 * up to 15 bytes per allocation, depending on what alloca() returns.
 */

#define dALLOCA16(n) \
  ((char*)dEFFICIENT_SIZE(((size_t)(alloca((n)+(EFFICIENT_ALIGNMENT-1))))))



/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

#ifdef DEBUG
#define ANSI_FTOL 1

extern "C" { 
    __declspec(naked) void _ftol2() {
        __asm    {
#if ANSI_FTOL
            fnstcw   WORD PTR [esp-2]
            mov      ax, WORD PTR [esp-2]
			
            OR AX,	 0C00h
			
            mov      WORD PTR [esp-4], ax
            fldcw    WORD PTR [esp-4]
            fistp    QWORD PTR [esp-12]
            fldcw    WORD PTR [esp-2]
            mov      eax, DWORD PTR [esp-12]
            mov      edx, DWORD PTR [esp-8]
#else
            fistp    DWORD PTR [esp-12]
            mov		 eax, DWORD PTR [esp-12]
            mov		 ecx, DWORD PTR [esp-8]
#endif
            ret
        }
    }
}
#endif //DEBUG



  


#define ALLOCA dALLOCA16

typedef const SimdScalar *dRealPtr;
typedef SimdScalar *dRealMutablePtr;
#define dRealArray(name,n) SimdScalar name[n];
#define dRealAllocaArray(name,n) SimdScalar *name = (SimdScalar*) ALLOCA ((n)*sizeof(SimdScalar));

void dSetZero1 (SimdScalar *a, int n)
{
  dAASSERT (a && n >= 0);
  while (n > 0) {
    *(a++) = 0;
    n--;
  }
}

void dSetValue1 (SimdScalar *a, int n, SimdScalar value)
{
  dAASSERT (a && n >= 0);
  while (n > 0) {
    *(a++) = value;
    n--;
  }
}


//***************************************************************************
// configuration

// for the SOR and CG methods:
// uncomment the following line to use warm starting. this definitely
// help for motor-driven joints. unfortunately it appears to hurt
// with high-friction contacts using the SOR method. use with care

//#define WARM_STARTING 1

// for the SOR method:
// uncomment the following line to randomly reorder constraint rows
// during the solution. depending on the situation, this can help a lot
// or hardly at all, but it doesn't seem to hurt.

#define RANDOMLY_REORDER_CONSTRAINTS 1



//***************************************************************************
// various common computations involving the matrix J

// compute iMJ = inv(M)*J'

static void compute_invM_JT (int m, dRealMutablePtr J, dRealMutablePtr iMJ, int *jb,
	RigidBody * const *body, dRealPtr invI)
{
	int i,j;
	dRealMutablePtr iMJ_ptr = iMJ;
	dRealMutablePtr J_ptr = J;
	for (i=0; i<m; i++) {
		int b1 = jb[i*2];	
		int b2 = jb[i*2+1];
		SimdScalar k = body[b1]->getInvMass();
		for (j=0; j<3; j++) iMJ_ptr[j] = k*J_ptr[j];
		dMULTIPLY0_331 (iMJ_ptr + 3, invI + 12*b1, J_ptr + 3);
		if (b2 >= 0) {
			k = body[b2]->getInvMass();
			for (j=0; j<3; j++) iMJ_ptr[j+6] = k*J_ptr[j+6];
			dMULTIPLY0_331 (iMJ_ptr + 9, invI + 12*b2, J_ptr + 9);
		}
		J_ptr += 12;
		iMJ_ptr += 12;
	}
}

#if 0
static void multiply_invM_JTSpecial (int m, int nb, dRealMutablePtr iMJ, int *jb,
	dRealMutablePtr in, dRealMutablePtr out,int onlyBody1,int onlyBody2)
{
	int i,j;

		

	dRealMutablePtr out_ptr1 = out + onlyBody1*6;
	
	for (j=0; j<6; j++) 
		out_ptr1[j] = 0;

	if (onlyBody2 >= 0)
	{
		out_ptr1 = out + onlyBody2*6;

		for (j=0; j<6; j++) 
			out_ptr1[j] = 0;
	}

	dRealPtr iMJ_ptr = iMJ;
	for (i=0; i<m; i++) {

		int b1 = jb[i*2];	

		dRealMutablePtr out_ptr = out + b1*6;
		if ((b1 == onlyBody1) || (b1 == onlyBody2))
		{
			for (j=0; j<6; j++) 
				out_ptr[j] += iMJ_ptr[j] * in[i] ;
		}
			
		iMJ_ptr += 6;

		int b2 = jb[i*2+1];
		if ((b2 == onlyBody1) || (b2 == onlyBody2))
		{
			if (b2 >= 0) 
			{
					out_ptr = out + b2*6;
					for (j=0; j<6; j++) 
						out_ptr[j] += iMJ_ptr[j] * in[i];
			}
		}
		
		iMJ_ptr += 6;
		
	}
}
#endif


// compute out = inv(M)*J'*in.

#if 0
static void multiply_invM_JT (int m, int nb, dRealMutablePtr iMJ, int *jb,
	dRealMutablePtr in, dRealMutablePtr out)
{
	int i,j;
	dSetZero1 (out,6*nb);
	dRealPtr iMJ_ptr = iMJ;
	for (i=0; i<m; i++) {
		int b1 = jb[i*2];	
		int b2 = jb[i*2+1];
		dRealMutablePtr out_ptr = out + b1*6;
		for (j=0; j<6; j++) 
			out_ptr[j] += iMJ_ptr[j] * in[i];
		iMJ_ptr += 6;
		if (b2 >= 0) {
			out_ptr = out + b2*6;
			for (j=0; j<6; j++) out_ptr[j] += iMJ_ptr[j] * in[i];
		}
		iMJ_ptr += 6;
	}
}
#endif


// compute out = J*in.

static void multiply_J (int m, dRealMutablePtr J, int *jb,
	dRealMutablePtr in, dRealMutablePtr out)
{
	int i,j;
	dRealPtr J_ptr = J;
	for (i=0; i<m; i++) {
		int b1 = jb[i*2];	
		int b2 = jb[i*2+1];
		SimdScalar sum = 0;
		dRealMutablePtr in_ptr = in + b1*6;
		for (j=0; j<6; j++) sum += J_ptr[j] * in_ptr[j];				
		J_ptr += 6;
		if (b2 >= 0) {
			in_ptr = in + b2*6;
			for (j=0; j<6; j++) sum += J_ptr[j] * in_ptr[j];				
		}
		J_ptr += 6;
		out[i] = sum;
	}
}

//***************************************************************************
// SOR-LCP method

// nb is the number of bodies in the body array.
// J is an m*12 matrix of constraint rows
// jb is an array of first and second body numbers for each constraint row
// invI is the global frame inverse inertia for each body (stacked 3x3 matrices)
//
// this returns lambda and fc (the constraint force).
// note: fc is returned as inv(M)*J'*lambda, the constraint force is actually J'*lambda
//
// b, lo and hi are modified on exit


struct IndexError {
	SimdScalar error;		// error to sort on
	int findex;
	int index;		// row index
};

static unsigned long seed2 = 0;

unsigned long dRand2()
{
  seed2 = (1664525L*seed2 + 1013904223L) & 0xffffffff;
  return seed2;
}

int dRandInt2 (int n)
{
  float a = float(n) / 4294967296.0f;
  return (int) (float(dRand2()) * a);
}


static void SOR_LCP (int m, int nb, dRealMutablePtr J, int *jb, RigidBody * const *body,
	dRealPtr invI, dRealMutablePtr lambda, dRealMutablePtr invMforce, dRealMutablePtr rhs,
	dRealMutablePtr lo, dRealMutablePtr hi, dRealPtr cfm, int *findex,
	int numiter,float overRelax)
{
	const int num_iterations = numiter;
	const float sor_w = overRelax;		// SOR over-relaxation parameter

	int i,j;

#ifdef WARM_STARTING
	// for warm starting, this seems to be necessary to prevent
	// jerkiness in motor-driven joints. i have no idea why this works.
	for (i=0; i<m; i++) lambda[i] *= 0.9;
#else
	dSetZero1 (lambda,m);
#endif

	// the lambda computed at the previous iteration.
	// this is used to measure error for when we are reordering the indexes.
	dRealAllocaArray (last_lambda,m);

	// a copy of the 'hi' vector in case findex[] is being used
	dRealAllocaArray (hicopy,m);
	memcpy (hicopy,hi,m*sizeof(float));

	// precompute iMJ = inv(M)*J'
	dRealAllocaArray (iMJ,m*12);
	compute_invM_JT (m,J,iMJ,jb,body,invI);

	// compute fc=(inv(M)*J')*lambda. we will incrementally maintain fc
	// as we change lambda.
#ifdef WARM_STARTING
	multiply_invM_JT (m,nb,iMJ,jb,lambda,fc);
#else
	dSetZero1 (invMforce,nb*6);
#endif

	// precompute 1 / diagonals of A
	dRealAllocaArray (Ad,m);
	dRealPtr iMJ_ptr = iMJ;
	dRealMutablePtr J_ptr = J;
	for (i=0; i<m; i++) {
		float sum = 0;
		for (j=0; j<6; j++) sum += iMJ_ptr[j] * J_ptr[j];
		if (jb[i*2+1] >= 0) {
			for (j=6; j<12; j++) sum += iMJ_ptr[j] * J_ptr[j];
		}
		iMJ_ptr += 12;
		J_ptr += 12;
		Ad[i] = sor_w / sum;//(sum + cfm[i]);
	}

	// scale J and b by Ad
	J_ptr = J;
	for (i=0; i<m; i++) {
		for (j=0; j<12; j++) {
			J_ptr[0] *= Ad[i];
			J_ptr++;
		}
		rhs[i] *= Ad[i];
	}

	// scale Ad by CFM
	for (i=0; i<m; i++) 
		Ad[i] *= cfm[i];

	// order to solve constraint rows in
	IndexError *order = (IndexError*) alloca (m*sizeof(IndexError));

#ifndef REORDER_CONSTRAINTS
	// make sure constraints with findex < 0 come first.
	j=0;
	for (i=0; i<m; i++) 
		if (findex[i] < 0) 
			order[j++].index = i;
	for (i=0; i<m; i++) 
		if (findex[i] >= 0) 
			order[j++].index = i;
	dIASSERT (j==m);
#endif

	for (int iteration=0; iteration < num_iterations; iteration++) {

#ifdef REORDER_CONSTRAINTS
		// constraints with findex < 0 always come first.
		if (iteration < 2) {
			// for the first two iterations, solve the constraints in
			// the given order
			for (i=0; i<m; i++) {
				order[i].error = i;
				order[i].findex = findex[i];
				order[i].index = i;
			}
		}
		else {
			// sort the constraints so that the ones converging slowest
			// get solved last. use the absolute (not relative) error.
			for (i=0; i<m; i++) {
				float v1 = dFabs (lambda[i]);
				float v2 = dFabs (last_lambda[i]);
				float max = (v1 > v2) ? v1 : v2;
				if (max > 0) {
					//@@@ relative error: order[i].error = dFabs(lambda[i]-last_lambda[i])/max;
					order[i].error = dFabs(lambda[i]-last_lambda[i]);
				}
				else {
					order[i].error = dInfinity;
				}
				order[i].findex = findex[i];
				order[i].index = i;
			}
		}
		qsort (order,m,sizeof(IndexError),&compare_index_error);
#endif
#ifdef RANDOMLY_REORDER_CONSTRAINTS
                if ((iteration & 7) == 0) {
			for (i=1; i<m; ++i) {
				IndexError tmp = order[i];
				int swapi = dRandInt2(i+1);
				order[i] = order[swapi];
				order[swapi] = tmp;
			}
                }
#endif

		//@@@ potential optimization: swap lambda and last_lambda pointers rather
		//    than copying the data. we must make sure lambda is properly
		//    returned to the caller
		memcpy (last_lambda,lambda,m*sizeof(float));

		for (int i=0; i<m; i++) {
			// @@@ potential optimization: we could pre-sort J and iMJ, thereby
			//     linearizing access to those arrays. hmmm, this does not seem
			//     like a win, but we should think carefully about our memory
			//     access pattern.
		
			int index = order[i].index;
			J_ptr = J + index*12;
			iMJ_ptr = iMJ + index*12;
		
			// set the limits for this constraint. note that 'hicopy' is used.
			// this is the place where the QuickStep method differs from the
			// direct LCP solving method, since that method only performs this
			// limit adjustment once per time step, whereas this method performs
			// once per iteration per constraint row.
			// the constraints are ordered so that all lambda[] values needed have
			// already been computed.
			if (findex[index] >= 0) {
				hi[index] = SimdFabs (hicopy[index] * lambda[findex[index]]);
				lo[index] = -hi[index];
			}

			int b1 = jb[index*2];
			int b2 = jb[index*2+1];
			float delta = rhs[index] - lambda[index]*Ad[index];
			dRealMutablePtr fc_ptr = invMforce + 6*b1;
			
			// @@@ potential optimization: SIMD-ize this and the b2 >= 0 case
			delta -=fc_ptr[0] * J_ptr[0] + fc_ptr[1] * J_ptr[1] +
				fc_ptr[2] * J_ptr[2] + fc_ptr[3] * J_ptr[3] +
				fc_ptr[4] * J_ptr[4] + fc_ptr[5] * J_ptr[5];
			// @@@ potential optimization: handle 1-body constraints in a separate
			//     loop to avoid the cost of test & jump?
			if (b2 >= 0) {
				fc_ptr = invMforce + 6*b2;
				delta -=fc_ptr[0] * J_ptr[6] + fc_ptr[1] * J_ptr[7] +
					fc_ptr[2] * J_ptr[8] + fc_ptr[3] * J_ptr[9] +
					fc_ptr[4] * J_ptr[10] + fc_ptr[5] * J_ptr[11];
			}

			// compute lambda and clamp it to [lo,hi].
			// @@@ potential optimization: does SSE have clamping instructions
			//     to save test+jump penalties here?
			float new_lambda = lambda[index] + delta;
			if (new_lambda < lo[index]) {
				delta = lo[index]-lambda[index];
				lambda[index] = lo[index];
			}
			else if (new_lambda > hi[index]) {
				delta = hi[index]-lambda[index];
				lambda[index] = hi[index];
			}
			else {
				lambda[index] = new_lambda;
			}

			//@@@ a trick that may or may not help
			//float ramp = (1-((float)(iteration+1)/(float)num_iterations));
			//delta *= ramp;
		
			// update invMforce.
			// @@@ potential optimization: SIMD for this and the b2 >= 0 case
			fc_ptr = invMforce + 6*b1;
			fc_ptr[0] += delta * iMJ_ptr[0];
			fc_ptr[1] += delta * iMJ_ptr[1];
			fc_ptr[2] += delta * iMJ_ptr[2];
			fc_ptr[3] += delta * iMJ_ptr[3];
			fc_ptr[4] += delta * iMJ_ptr[4];
			fc_ptr[5] += delta * iMJ_ptr[5];
			// @@@ potential optimization: handle 1-body constraints in a separate
			//     loop to avoid the cost of test & jump?
			if (b2 >= 0) {
				fc_ptr = invMforce + 6*b2;
				fc_ptr[0] += delta * iMJ_ptr[6];
				fc_ptr[1] += delta * iMJ_ptr[7];
				fc_ptr[2] += delta * iMJ_ptr[8];
				fc_ptr[3] += delta * iMJ_ptr[9];
				fc_ptr[4] += delta * iMJ_ptr[10];
				fc_ptr[5] += delta * iMJ_ptr[11];
			}
		}
	}
}



void SolveInternal1 (float global_cfm,
					 float global_erp,
					 RigidBody * const *body, int nb,
					BU_Joint * const *_joint, 
					int nj, 
					const ContactSolverInfo& solverInfo)
{

	int numIter = solverInfo.m_numIterations;
	float sOr = solverInfo.m_sor;

	int i,j;
	
	SimdScalar stepsize1 = dRecip(solverInfo.m_timeStep);

	// number all bodies in the body list - set their tag values
	for (i=0; i<nb; i++) 
		body[i]->m_odeTag = i;
	
	// make a local copy of the joint array, because we might want to modify it.
	// (the "BU_Joint *const*" declaration says we're allowed to modify the joints
	// but not the joint array, because the caller might need it unchanged).
	//@@@ do we really need to do this? we'll be sorting constraint rows individually, not joints
	BU_Joint **joint = (BU_Joint**) alloca (nj * sizeof(BU_Joint*));
	memcpy (joint,_joint,nj * sizeof(BU_Joint*));
	
	// for all bodies, compute the inertia tensor and its inverse in the global
	// frame, and compute the rotational force and add it to the torque
	// accumulator. I and invI are a vertical stack of 3x4 matrices, one per body.
	dRealAllocaArray (I,3*4*nb);
	dRealAllocaArray (invI,3*4*nb);
/*	for (i=0; i<nb; i++) {
		dMatrix3 tmp;
		// compute inertia tensor in global frame
		dMULTIPLY2_333 (tmp,body[i]->m_I,body[i]->m_R);
		// compute inverse inertia tensor in global frame
		dMULTIPLY2_333 (tmp,body[i]->m_invI,body[i]->m_R);
		dMULTIPLY0_333 (invI+i*12,body[i]->m_R,tmp);
		// compute rotational force
		dCROSS (body[i]->m_tacc,-=,body[i]->getAngularVelocity(),tmp);
	}
*/
	for (i=0; i<nb; i++) {
		dMatrix3 tmp;
		// compute inertia tensor in global frame
		dMULTIPLY2_333 (tmp,body[i]->m_I,body[i]->m_R);
		dMULTIPLY0_333 (I+i*12,body[i]->m_R,tmp);

		// compute inverse inertia tensor in global frame
		dMULTIPLY2_333 (tmp,body[i]->m_invI,body[i]->m_R);
		dMULTIPLY0_333 (invI+i*12,body[i]->m_R,tmp);
		// compute rotational force
		dMULTIPLY0_331 (tmp,I+i*12,body[i]->getAngularVelocity());
		dCROSS (body[i]->m_tacc,-=,body[i]->getAngularVelocity(),tmp);
	}




	// get joint information (m = total constraint dimension, nub = number of unbounded variables).
	// joints with m=0 are inactive and are removed from the joints array
	// entirely, so that the code that follows does not consider them.
	//@@@ do we really need to save all the info1's
	BU_Joint::Info1 *info = (BU_Joint::Info1*) alloca (nj*sizeof(BU_Joint::Info1));
	for (i=0, j=0; j<nj; j++) {	// i=dest, j=src
		joint[j]->GetInfo1 (info+i);
		dIASSERT (info[i].m >= 0 && info[i].m <= 6 && info[i].nub >= 0 && info[i].nub <= info[i].m);
		if (info[i].m > 0) {
			joint[i] = joint[j];
			i++;
		}
	}
	nj = i;

	// create the row offset array
	int m = 0;
	int *ofs = (int*) alloca (nj*sizeof(int));
	for (i=0; i<nj; i++) {
		ofs[i] = m;
		m += info[i].m;
	}

	// if there are constraints, compute the constraint force
	dRealAllocaArray (J,m*12);
	int *jb = (int*) alloca (m*2*sizeof(int));
	if (m > 0) {
		// create a constraint equation right hand side vector `c', a constraint
		// force mixing vector `cfm', and LCP low and high bound vectors, and an
		// 'findex' vector.
		dRealAllocaArray (c,m);
		dRealAllocaArray (cfm,m);
		dRealAllocaArray (lo,m);
		dRealAllocaArray (hi,m);
		int *findex = (int*) alloca (m*sizeof(int));
		dSetZero1 (c,m);
		dSetValue1 (cfm,m,global_cfm);
		dSetValue1 (lo,m,-dInfinity);
		dSetValue1 (hi,m, dInfinity);
		for (i=0; i<m; i++) findex[i] = -1;
		
		// get jacobian data from constraints. an m*12 matrix will be created
		// to store the two jacobian blocks from each constraint. it has this
		// format:
		//
		//   l1 l1 l1 a1 a1 a1 l2 l2 l2 a2 a2 a2 \    .
		//   l1 l1 l1 a1 a1 a1 l2 l2 l2 a2 a2 a2  }-- jacobian for joint 0, body 1 and body 2 (3 rows)
		//   l1 l1 l1 a1 a1 a1 l2 l2 l2 a2 a2 a2 /
		//   l1 l1 l1 a1 a1 a1 l2 l2 l2 a2 a2 a2 }--- jacobian for joint 1, body 1 and body 2 (3 rows)
		//   etc...
		//
		//   (lll) = linear jacobian data
		//   (aaa) = angular jacobian data
		//
		dSetZero1 (J,m*12);
		BU_Joint::Info2 Jinfo;
		Jinfo.rowskip = 12;
		Jinfo.fps = stepsize1;
		Jinfo.erp = global_erp;
		for (i=0; i<nj; i++) {
			Jinfo.J1l = J + ofs[i]*12;
			Jinfo.J1a = Jinfo.J1l + 3;
			Jinfo.J2l = Jinfo.J1l + 6;
			Jinfo.J2a = Jinfo.J1l + 9;
			Jinfo.c = c + ofs[i];
			Jinfo.cfm = cfm + ofs[i];
			Jinfo.lo = lo + ofs[i];
			Jinfo.hi = hi + ofs[i];
			Jinfo.findex = findex + ofs[i];
			joint[i]->GetInfo2 (&Jinfo);

			if (Jinfo.c[0] > solverInfo.m_maxErrorReduction)
				Jinfo.c[0] = solverInfo.m_maxErrorReduction;




			// adjust returned findex values for global index numbering
			for (j=0; j<info[i].m; j++) {
				if (findex[ofs[i] + j] >= 0) 
					findex[ofs[i] + j] += ofs[i];
			}
		}

		// create an array of body numbers for each joint row
		int *jb_ptr = jb;
		for (i=0; i<nj; i++) {
			int b1 = (joint[i]->node[0].body) ? (joint[i]->node[0].body->m_odeTag) : -1;
			int b2 = (joint[i]->node[1].body) ? (joint[i]->node[1].body->m_odeTag) : -1;
			for (j=0; j<info[i].m; j++) {
				jb_ptr[0] = b1;
				jb_ptr[1] = b2;
				jb_ptr += 2;
			}
		}
		dIASSERT (jb_ptr == jb+2*m);

		// compute the right hand side `rhs'
		dRealAllocaArray (tmp1,nb*6);
		// put v/h + invM*fe into tmp1
		for (i=0; i<nb; i++) {
			SimdScalar body_invMass = body[i]->getInvMass();
			for (j=0; j<3; j++) 
				tmp1[i*6+j] = body[i]->m_facc[j] * body_invMass + body[i]->getLinearVelocity()[j] * stepsize1;
			dMULTIPLY0_331NEW (tmp1 + i*6 + 3,=,invI + i*12,body[i]->m_tacc);
			for (j=0; j<3; j++) 
				tmp1[i*6+3+j] += body[i]->getAngularVelocity()[j] * stepsize1;
		}

		// put J*tmp1 into rhs
		dRealAllocaArray (rhs,m);
		multiply_J (m,J,jb,tmp1,rhs);

		// complete rhs
		for (i=0; i<m; i++) rhs[i] = c[i]*stepsize1 - rhs[i];

		// scale CFM
		for (i=0; i<m; i++) 
			cfm[i] *= stepsize1;

		// load lambda from the value saved on the previous iteration
		dRealAllocaArray (lambda,m);
#ifdef WARM_STARTING
		dSetZero1 (lambda,m);	//@@@ shouldn't be necessary
		for (i=0; i<nj; i++) {
			memcpy (lambda+ofs[i],joint[i]->lambda,info[i].m * sizeof(SimdScalar));
		}
#endif

		// solve the LCP problem and get lambda and invM*constraint_force
		dRealAllocaArray (cforce,nb*6);

		SOR_LCP (m,nb,J,jb,body,invI,lambda,cforce,rhs,lo,hi,cfm,findex,numIter,sOr);

#ifdef WARM_STARTING
		// save lambda for the next iteration
		//@@@ note that this doesn't work for contact joints yet, as they are
		// recreated every iteration
		for (i=0; i<nj; i++) {
			memcpy (joint[i]->lambda,lambda+ofs[i],info[i].m * sizeof(SimdScalar));
		}
#endif

	
		// note that the SOR method overwrites rhs and J at this point, so
		// they should not be used again.
		
		// add stepsize * cforce to the body velocity
		for (i=0; i<nb; i++) {
			SimdVector3 linvel = body[i]->getLinearVelocity();
			SimdVector3 angvel = body[i]->getAngularVelocity();
			
			for (j=0; j<3; j++) 
				linvel[j] += solverInfo.m_timeStep* cforce[i*6+j];
			for (j=0; j<3; j++) 
				angvel[j] += solverInfo.m_timeStep* cforce[i*6+3+j];
		
			body[i]->setLinearVelocity(linvel);
			body[i]->setAngularVelocity(angvel);

		}
		
	}



	// compute the velocity update:
	// add stepsize * invM * fe to the body velocity

	for (i=0; i<nb; i++) {
		SimdScalar body_invMass = body[i]->getInvMass();
		SimdVector3 linvel = body[i]->getLinearVelocity();
		SimdVector3 angvel = body[i]->getAngularVelocity();

		for (j=0; j<3; j++) 
		{
			linvel[j] += solverInfo.m_timeStep * body_invMass * body[i]->m_facc[j];
		}
		for (j=0; j<3; j++) 
		{
			body[i]->m_tacc[j] *= solverInfo.m_timeStep;	
		}
		dMULTIPLY0_331NEW(angvel,+=,invI + i*12,body[i]->m_tacc);
		body[i]->setAngularVelocity(angvel);

	}



}


#endif //USE_SOR_SOLVER
