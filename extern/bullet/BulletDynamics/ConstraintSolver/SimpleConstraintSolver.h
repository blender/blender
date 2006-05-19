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

#ifndef SIMPLE_CONSTRAINT_SOLVER_H
#define SIMPLE_CONSTRAINT_SOLVER_H

#include "ConstraintSolver.h"
class IDebugDraw;

/// SimpleConstraintSolver uses a Propagation Method and Sequentially applies impulses
/// The approach is the 3D version of Erin Catto's GDC 2006 tutorial. See http://www.gphysics.com
/// Although Sequential Impulse is more intuitive, it is mathematically equivalent to Projected Successive Overrelaxation (iterative LCP)
/// Applies impulses for combined restitution and penetration recovery and to simulate friction
class SimpleConstraintSolver : public ConstraintSolver
{
	float Solve(PersistentManifold* manifold, const ContactSolverInfo& info,int iter,IDebugDraw* debugDrawer);
	float SolveFriction(PersistentManifold* manifoldPtr, const ContactSolverInfo& info,int iter,IDebugDraw* debugDrawer);
	

public:

	SimpleConstraintSolver();

	virtual ~SimpleConstraintSolver() {}
	
	virtual float SolveGroup(PersistentManifold** manifold,int numManifolds,const ContactSolverInfo& info, IDebugDraw* debugDrawer=0);

};

#endif //SIMPLE_CONSTRAINT_SOLVER_H

