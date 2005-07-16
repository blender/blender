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
#include "SimpleConstraintSolver.h"
#include "NarrowPhaseCollision/PersistentManifold.h"
#include "Dynamics/RigidBody.h"
#include "ContactConstraint.h"
#include "Solve2LinearConstraint.h"
#include "ContactSolverInfo.h"
#include "Dynamics/BU_Joint.h"
#include "Dynamics/ContactJoint.h"

//debugging
bool doApplyImpulse = true;



bool useImpulseFriction = true;//true;//false;




//iterative lcp and penalty method
float SimpleConstraintSolver::SolveGroup(PersistentManifold** manifoldPtr, int numManifolds,const ContactSolverInfo& infoGlobal)
{

	ContactSolverInfo info = infoGlobal;

	int numiter = infoGlobal.m_numIterations;

	float substep = infoGlobal.m_timeStep / float(numiter);

	for (int i = 0;i<numiter;i++)
	{
		for (int j=0;j<numManifolds;j++)
		{
			Solve(manifoldPtr[j],info,i);
		}
	}
	return 0.f;
}


float penetrationResolveFactor = 0.9f;


float SimpleConstraintSolver::Solve(PersistentManifold* manifoldPtr, const ContactSolverInfo& info,int iter)
{

	RigidBody* body0 = (RigidBody*)manifoldPtr->GetBody0();
	RigidBody* body1 = (RigidBody*)manifoldPtr->GetBody1();

	float maxImpulse = 0.f;


	float invNumIterFl = 1.f / float(info.m_numIterations);

	float timeSubStep = info.m_timeStep * invNumIterFl;
	
	//only necessary to refresh the manifold once (first iteration). The integration is done outside the loop
	if (iter == 0)
	{
		manifoldPtr->RefreshContactPoints(body0->getCenterOfMassTransform(),body1->getCenterOfMassTransform());
	}

	{
		const int numpoints = manifoldPtr->GetNumContacts();

		for (int i=0;i<numpoints ;i++)
		{

			int j=i;
			if (iter % 2)
				j = numpoints-1-i;
			else
				j=i;

			ManifoldPoint& cp = manifoldPtr->GetContactPoint(j);

			{


				float dist =  invNumIterFl * cp.GetDistance() * penetrationResolveFactor / info.m_timeStep;// / timeStep;//penetrationResolveFactor*cp.m_solveDistance /timeStep;//cp.GetDistance();


				float impulse = 0.f;

				if (doApplyImpulse)
				{
					impulse = resolveSingleCollision(*body0, 
						cp.GetPositionWorldOnA(),
						*body1, 
						cp.GetPositionWorldOnB(),
						-dist,
						cp.m_normalWorldOnB,
						info);

					if (useImpulseFriction)
					{
						applyFrictionInContactPointOld(
							*body0,cp.GetPositionWorldOnA(),*body1,cp.GetPositionWorldOnB(),
							cp.m_normalWorldOnB,impulse,info) ;
					}
				}
				if (iter == 0)
				{
					cp.m_appliedImpulse = impulse;
				} else
				{
					cp.m_appliedImpulse += impulse;
				}

				if (maxImpulse < impulse)
					maxImpulse  = impulse;

			}
		}
	}
	return maxImpulse;
}
