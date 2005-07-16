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
#ifndef ODE_CONSTRAINT_SOLVER_H
#define ODE_CONSTRAINT_SOLVER_H

#include "ConstraintSolver.h"


class OdeConstraintSolver : public ConstraintSolver
{
	
public:

	virtual ~OdeConstraintSolver() {}
	
	virtual float SolveGroup(PersistentManifold** manifold,int numManifolds,const ContactSolverInfo& info);

};




#endif //ODE_CONSTRAINT_SOLVER_H
