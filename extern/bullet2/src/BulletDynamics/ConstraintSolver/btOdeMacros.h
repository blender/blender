/*
 * Quickstep constraint solver re-distributed under the ZLib license with permission from Russell L. Smith
 * Original version is from Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org
 Bullet Continuous Collision Detection and Physics Library
 Bullet is Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#define ODE_MACROS
#ifdef ODE_MACROS

#include "LinearMath/btScalar.h"

typedef btScalar dVector4[4];
typedef btScalar dMatrix3[4*3];
#define dInfinity FLT_MAX



#define dRecip(x) ((float)(1.0f/(x)))				/* reciprocal */



#define dMULTIPLY0_331NEW(A,op,B,C) \
{\
	btScalar tmp[3];\
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

#define dAASSERT btAssert
#define dIASSERT btAssert

#define REAL float
#define dDOTpq(a,b,p,q) ((a)[0]*(b)[0] + (a)[p]*(b)[q] + (a)[2*(p)]*(b)[2*(q)])
inline btScalar dDOT1 (const btScalar *a, const btScalar *b)
{ return dDOTpq(a,b,1,1); }
#define dDOT14(a,b) dDOTpq(a,b,1,4)

#define dCROSS(a,op,b,c) \
  (a)[0] op ((b)[1]*(c)[2] - (b)[2]*(c)[1]); \
  (a)[1] op ((b)[2]*(c)[0] - (b)[0]*(c)[2]); \
  (a)[2] op ((b)[0]*(c)[1] - (b)[1]*(c)[0]);

/*
 * set a 3x3 submatrix of A to a matrix such that submatrix(A)*b = a x b.
 * A is stored by rows, and has `skip' elements per row. the matrix is
 * assumed to be already zero, so this does not write zero elements!
 * if (plus,minus) is (+,-) then a positive version will be written.
 * if (plus,minus) is (-,+) then a negative version will be written.
 */

#define dCROSSMAT(A,a,skip,plus,minus) \
{ \
  (A)[1] = minus (a)[2]; \
  (A)[2] = plus (a)[1]; \
  (A)[(skip)+0] = plus (a)[2]; \
  (A)[(skip)+2] = minus (a)[0]; \
  (A)[2*(skip)+0] = minus (a)[1]; \
  (A)[2*(skip)+1] = plus (a)[0]; \
} 


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

//#define ALLOCA dALLOCA16

typedef const btScalar *dRealPtr;
typedef btScalar *dRealMutablePtr;
//#define dRealArray(name,n) btScalar name[n];
//#define dRealAllocaArray(name,n) btScalar *name = (btScalar*) ALLOCA ((n)*sizeof(btScalar));

///////////////////////////////////////////////////////////////////////////////

 //Remotion: 10.10.2007
#define ALLOCA(size) stackAlloc->allocate( dEFFICIENT_SIZE(size) );

//#define dRealAllocaArray(name,size) btScalar *name = (btScalar*) stackAlloc->allocate(dEFFICIENT_SIZE(size)*sizeof(btScalar));
#define dRealAllocaArray(name,size) btScalar *name = NULL; \
	unsigned int  memNeeded_##name = dEFFICIENT_SIZE(size)*sizeof(btScalar); \
	if (memNeeded_##name < static_cast<size_t>(stackAlloc->getAvailableMemory())) name = (btScalar*) stackAlloc->allocate(memNeeded_##name); \
	else{ btAssert(memNeeded_##name < static_cast<size_t>(stackAlloc->getAvailableMemory())); name = (btScalar*) alloca(memNeeded_##name); } 





///////////////////////////////////////////////////////////////////////////////
#if 0
inline void dSetZero1 (btScalar *a, int n)
{
  dAASSERT (a && n >= 0);
  while (n > 0) {
    *(a++) = 0;
    n--;
  }
}

inline void dSetValue1 (btScalar *a, int n, btScalar value)
{
  dAASSERT (a && n >= 0);
  while (n > 0) {
    *(a++) = value;
    n--;
  }
}
#else

/// This macros are for MSVC and XCode compilers. Remotion.


#include <string.h> //for memset

//Remotion: 10.10.2007
//------------------------------------------------------------------------------
#define IS_ALIGNED_16(x)	((size_t(x)&15)==0)
//------------------------------------------------------------------------------
inline void dSetZero1 (btScalar *dest, int size)
{
	dAASSERT (dest && size >= 0);
	memset(dest, 0, size * sizeof(btScalar));
}
//------------------------------------------------------------------------------
inline void dSetValue1 (btScalar *dest, int size, btScalar val)
{
	dAASSERT (dest && size >= 0);
	int n_mod4 = size & 3;		
	int n4 = size - n_mod4;
/*#ifdef __USE_SSE__
//it is not supported on double precision, todo...
	if(IS_ALIGNED_16(dest)){
		__m128 xmm0 = _mm_set_ps1(val);
		for (int i=0; i<n4; i+=4)
		{
			_mm_store_ps(&dest[i],xmm0);
		}
	}else
#endif
	*/

	{
		for (int i=0; i<n4; i+=4) // Unrolled Loop
		{
			dest[i  ] = val;
			dest[i+1] = val;
			dest[i+2] = val;
			dest[i+3] = val;
		}
	}
	for (int  i=n4; i<size; i++){
		dest[i] = val;
	}
}
#endif
/////////////////////////////////////////////////////////////////////


#endif //USE_SOR_SOLVER

