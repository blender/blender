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

#ifndef ODE_CONSTRAINT_SOLVER_H
#define ODE_CONSTRAINT_SOLVER_H

#include "ConstraintSolver.h"

class RigidBody;
class BU_Joint;

/// OdeConstraintSolver is one of the available solvers for Bullet dynamics framework
/// It uses the the unmodified version of quickstep solver from the open dynamics project
class OdeConstraintSolver : public ConstraintSolver
{
private:

	int m_CurBody;
	int m_CurJoint;

	float	m_cfm;
	float	m_erp;
	

	int ConvertBody(RigidBody* body,RigidBody** bodies,int& numBodies);
	void ConvertConstraint(PersistentManifold* manifold,BU_Joint** joints,int& numJoints,
					   RigidBody** bodies,int _bodyId0,int _bodyId1,IDebugDraw* debugDrawer);

public:

	OdeConstraintSolver();

	virtual ~OdeConstraintSolver() {}
	
	virtual float SolveGroup(PersistentManifold** manifold,int numManifolds,const ContactSolverInfo& info,IDebugDraw* debugDrawer = 0);

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
};




#endif //ODE_CONSTRAINT_SOLVER_H
