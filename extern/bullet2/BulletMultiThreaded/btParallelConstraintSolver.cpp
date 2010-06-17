/*
   Copyright (C) 2010 Sony Computer Entertainment Inc.
   All rights reserved.

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

*/

#include "btParallelConstraintSolver.h"
#include "BulletDynamics/ConstraintSolver/btContactSolverInfo.h"

btParallelConstraintSolver::btParallelConstraintSolver()
{

	//initialize MiniCL here

}
	
btParallelConstraintSolver::~btParallelConstraintSolver()
{
	//exit MiniCL

}

	
btScalar btParallelConstraintSolver::solveGroupCacheFriendlySetup(btCollisionObject** bodies,int numBodies,btPersistentManifold** manifoldPtr, int numManifolds,btTypedConstraint** constraints,int numConstraints,const btContactSolverInfo& infoGlobal,btIDebugDraw* debugDrawer,btStackAlloc* stackAlloc)
{
	{
			int i;
			btPersistentManifold* manifold = 0;
//			btCollisionObject* colObj0=0,*colObj1=0;


			for (i=0;i<numManifolds;i++)
			{
				manifold = manifoldPtr[i];
				convertContact(manifold,infoGlobal);
			}
		
	}

	btContactSolverInfo info = infoGlobal;



	int numConstraintPool = m_tmpSolverContactConstraintPool.size();
	int numFrictionPool = m_tmpSolverContactFrictionConstraintPool.size();

	///@todo: use stack allocator for such temporarily memory, same for solver bodies/constraints
	m_orderTmpConstraintPool.resize(numConstraintPool);
	m_orderFrictionConstraintPool.resize(numFrictionPool);
	{
		int i;
		for (i=0;i<numConstraintPool;i++)
		{
			m_orderTmpConstraintPool[i] = i;
		}
		for (i=0;i<numFrictionPool;i++)
		{
			m_orderFrictionConstraintPool[i] = i;
		}
	}

	return 0.f;
}

