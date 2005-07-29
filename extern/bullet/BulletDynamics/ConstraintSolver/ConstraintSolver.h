/*
 * Copyright (c) 2005 Erwin Coumans http://www.erwincoumans.com
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#ifndef CONSTRAINT_SOLVER_H
#define CONSTRAINT_SOLVER_H

class PersistentManifold;
class RigidBody;

struct ContactSolverInfo;
struct BroadphaseProxy;
class IDebugDraw;

/// ConstraintSolver provides solver interface
class ConstraintSolver
{

public:

	virtual ~ConstraintSolver() {}
	
	virtual float SolveGroup(PersistentManifold** manifold,int numManifolds,const ContactSolverInfo& info,class IDebugDraw* debugDrawer = 0) = 0;

};




#endif //CONSTRAINT_SOLVER_H
