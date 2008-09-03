/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#ifndef SEQUENTIAL_IMPULSE_CONSTRAINT_SOLVER_H
#define SEQUENTIAL_IMPULSE_CONSTRAINT_SOLVER_H

#include "btConstraintSolver.h"
class btIDebugDraw;
#include "btContactConstraint.h"
	


/// btSequentialImpulseConstraintSolver uses a Propagation Method and Sequentially applies impulses
/// The approach is the 3D version of Erin Catto's GDC 2006 tutorial. See http://www.gphysics.com
/// Although Sequential Impulse is more intuitive, it is mathematically equivalent to Projected Successive Overrelaxation (iterative LCP)
/// Applies impulses for combined restitution and penetration recovery and to simulate friction
class btSequentialImpulseConstraintSolver : public btConstraintSolver
{

protected:
	btScalar solve(btRigidBody* body0,btRigidBody* body1, btManifoldPoint& cp, const btContactSolverInfo& info,int iter,btIDebugDraw* debugDrawer);
	btScalar solveFriction(btRigidBody* body0,btRigidBody* body1, btManifoldPoint& cp, const btContactSolverInfo& info,int iter,btIDebugDraw* debugDrawer);
	void  prepareConstraints(btPersistentManifold* manifoldPtr, const btContactSolverInfo& info,btIDebugDraw* debugDrawer);

	ContactSolverFunc m_contactDispatch[MAX_CONTACT_SOLVER_TYPES][MAX_CONTACT_SOLVER_TYPES];
	ContactSolverFunc m_frictionDispatch[MAX_CONTACT_SOLVER_TYPES][MAX_CONTACT_SOLVER_TYPES];

	//choose between several modes, different friction model etc.
	int	m_solverMode;
	///m_btSeed2 is used for re-arranging the constraint rows. improves convergence/quality of friction
	unsigned long	m_btSeed2;

public:

	enum	eSolverMode
	{
		SOLVER_RANDMIZE_ORDER = 1,
		SOLVER_FRICTION_SEPARATE = 2,
		SOLVER_USE_WARMSTARTING = 4,
		SOLVER_CACHE_FRIENDLY = 8
	};

	btSequentialImpulseConstraintSolver();

	///Advanced: Override the default contact solving function for contacts, for certain types of rigidbody
	///See btRigidBody::m_contactSolverType and btRigidBody::m_frictionSolverType
	void	setContactSolverFunc(ContactSolverFunc func,int type0,int type1)
	{
		m_contactDispatch[type0][type1] = func;
	}
	
	///Advanced: Override the default friction solving function for contacts, for certain types of rigidbody
	///See btRigidBody::m_contactSolverType and btRigidBody::m_frictionSolverType
	void	SetFrictionSolverFunc(ContactSolverFunc func,int type0,int type1)
	{
		m_frictionDispatch[type0][type1] = func;
	}

	virtual ~btSequentialImpulseConstraintSolver() {}
	
	virtual btScalar solveGroup(btCollisionObject** bodies,int numBodies,btPersistentManifold** manifold,int numManifolds,btTypedConstraint** constraints,int numConstraints,const btContactSolverInfo& info, btIDebugDraw* debugDrawer, btStackAlloc* stackAlloc);

	virtual btScalar solveGroupCacheFriendly(btCollisionObject** bodies,int numBodies,btPersistentManifold** manifoldPtr, int numManifolds,btTypedConstraint** constraints,int numConstraints,const btContactSolverInfo& infoGlobal,btIDebugDraw* debugDrawer,btStackAlloc* stackAlloc);

	btScalar solveCombinedContactFriction(btRigidBody* body0,btRigidBody* body1, btManifoldPoint& cp, const btContactSolverInfo& info,int iter,btIDebugDraw* debugDrawer);


	void	setSolverMode(int mode)
	{
		m_solverMode = mode;
	}

	int	getSolverMode() const
	{
		return m_solverMode;
	}

	unsigned long btRand2();

	int btRandInt2 (int n);

	void	setRandSeed(unsigned long seed)
	{
		m_btSeed2 = seed;
	}
	unsigned long	getRandSeed() const
	{
		return m_btSeed2;
	}

};




#endif //SEQUENTIAL_IMPULSE_CONSTRAINT_SOLVER_H

