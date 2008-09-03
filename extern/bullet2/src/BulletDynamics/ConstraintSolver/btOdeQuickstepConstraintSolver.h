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

#ifndef ODE_CONSTRAINT_SOLVER_H
#define ODE_CONSTRAINT_SOLVER_H

#include "BulletDynamics/ConstraintSolver/btConstraintSolver.h"

#include "LinearMath/btAlignedObjectArray.h"
#include "btOdeContactJoint.h"
#include "btOdeTypedJoint.h"
#include "btOdeSolverBody.h"
#include "btSorLcp.h"

class btRigidBody;
struct	btOdeSolverBody;
class btOdeJoint;

/// btOdeQuickstepConstraintSolver is one of the available solvers for Bullet dynamics framework
/// It uses an adapted version quickstep solver from the Open Dynamics Engine project
class btOdeQuickstepConstraintSolver : public btConstraintSolver
{
private:
	int		m_CurBody;
	int		m_CurJoint;
	int		m_CurTypedJoint;

	float	m_cfm;
	float	m_erp;

	btSorLcpSolver	m_SorLcpSolver;

	btAlignedObjectArray<btOdeSolverBody*> m_odeBodies;
	btAlignedObjectArray<btOdeJoint*>		 m_joints;

	btAlignedObjectArray<btOdeSolverBody>  m_SolverBodyArray;
	btAlignedObjectArray<btOdeContactJoint>   m_JointArray;
	btAlignedObjectArray<btOdeTypedJoint>  m_TypedJointArray;


private:
	int  ConvertBody(btRigidBody* body,btAlignedObjectArray< btOdeSolverBody*> &bodies,int& numBodies);
	void ConvertConstraint(btPersistentManifold* manifold,
							btAlignedObjectArray<btOdeJoint*> &joints,int& numJoints,
							const btAlignedObjectArray< btOdeSolverBody*> &bodies,
							int _bodyId0,int _bodyId1,btIDebugDraw* debugDrawer);

	void ConvertTypedConstraint(
							btTypedConstraint * constraint,
							btAlignedObjectArray<btOdeJoint*> &joints,int& numJoints,
							const btAlignedObjectArray< btOdeSolverBody*> &bodies,int _bodyId0,int _bodyId1,btIDebugDraw* debugDrawer);


public:

	btOdeQuickstepConstraintSolver();

	virtual ~btOdeQuickstepConstraintSolver() {}

	virtual btScalar solveGroup(btCollisionObject** bodies,int numBodies,btPersistentManifold** manifold,int numManifolds,btTypedConstraint** constraints,int numConstraints,const btContactSolverInfo& info,btIDebugDraw* debugDrawer,btStackAlloc* stackAlloc,btDispatcher* dispatcher);

	///setConstraintForceMixing, the cfm adds some positive value to the main diagonal
	///This can improve convergence (make matrix positive semidefinite), but it can make the simulation look more 'springy'
	void	setConstraintForceMixing(float cfm) {
		m_cfm  = cfm;
	}

	///setErrorReductionParamter sets the maximum amount of error reduction
	///which limits energy addition during penetration depth recovery
	void	setErrorReductionParamter(float erp)
	{
		m_erp = erp;
	}

	///clear internal cached data and reset random seed
	void reset()
	{
		m_SorLcpSolver.dRand2_seed = 0;
	}

	void	setRandSeed(unsigned long seed)
	{
		m_SorLcpSolver.dRand2_seed = seed;
	}
	unsigned long	getRandSeed() const
	{
		return m_SorLcpSolver.dRand2_seed;
	}
};




#endif //ODE_CONSTRAINT_SOLVER_H
