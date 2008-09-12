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

#define USE_SOR_SOLVER
#ifdef USE_SOR_SOLVER

#ifndef SOR_LCP_H
#define SOR_LCP_H
struct	btOdeSolverBody;
class btOdeJoint;
#include "LinearMath/btScalar.h"
#include "LinearMath/btAlignedObjectArray.h"
#include "LinearMath/btStackAlloc.h"

struct btContactSolverInfo;


//=============================================================================
class btSorLcpSolver //Remotion: 11.10.2007
{
public:
	btSorLcpSolver()
	{
		dRand2_seed = 0;
	}

	void SolveInternal1 (float global_cfm,
		float global_erp,
		const btAlignedObjectArray<btOdeSolverBody*> &body, int nb,
		btAlignedObjectArray<btOdeJoint*> &joint, 
		int nj, const btContactSolverInfo& solverInfo,
		btStackAlloc* stackAlloc
		);

public: //data
	unsigned long dRand2_seed;

protected: //typedef
	typedef const btScalar *dRealPtr;
	typedef btScalar *dRealMutablePtr;

protected: //members
	//------------------------------------------------------------------------------
	SIMD_FORCE_INLINE unsigned long dRand2()
	{
		dRand2_seed = (1664525L*dRand2_seed + 1013904223L) & 0xffffffff;
		return dRand2_seed;
	}
	//------------------------------------------------------------------------------
	SIMD_FORCE_INLINE int dRandInt2 (int n)
	{
		float a = float(n) / 4294967296.0f;
		return (int) (float(dRand2()) * a);
	}
	//------------------------------------------------------------------------------
	void SOR_LCP(int m, int nb, dRealMutablePtr J, int *jb, 
		const btAlignedObjectArray<btOdeSolverBody*> &body,
		dRealPtr invI, dRealMutablePtr lambda, dRealMutablePtr invMforce, dRealMutablePtr rhs,
		dRealMutablePtr lo, dRealMutablePtr hi, dRealPtr cfm, int *findex,
		int numiter,float overRelax,
		btStackAlloc* stackAlloc
		);
};


//=============================================================================
class AutoBlockSa //Remotion: 10.10.2007
{
	btStackAlloc* stackAlloc;
	btBlock*	  saBlock;
public:
	AutoBlockSa(btStackAlloc* stackAlloc_)
	{
		stackAlloc = stackAlloc_;
		saBlock = stackAlloc->beginBlock();
	}
	~AutoBlockSa()
	{
		stackAlloc->endBlock(saBlock);
	}
	//operator btBlock* () { return saBlock; }
};
// //Usage
//void function(btStackAlloc* stackAlloc)
//{
//	AutoBlockSa(stackAlloc);
//   ...
//	if(...) return;
//	return;
//}
//------------------------------------------------------------------------------


#endif //SOR_LCP_H

#endif //USE_SOR_SOLVER

