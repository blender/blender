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

#include "ManifoldResult.h"
#include "NarrowPhaseCollision/PersistentManifold.h"
#include "Dynamics/RigidBody.h"

ManifoldResult::ManifoldResult(RigidBody* body0,RigidBody* body1,PersistentManifold* manifoldPtr)
		:m_manifoldPtr(manifoldPtr),
		m_body0(body0),
		m_body1(body1)
	{
	}

void ManifoldResult::AddContactPoint(const SimdVector3& normalOnBInWorld,const SimdVector3& pointInWorld,float depth)
{
	if (depth > m_manifoldPtr->GetManifoldMargin())
		return;

	SimdTransform transAInv = m_body0->getCenterOfMassTransform().inverse();
	SimdTransform transBInv= m_body1->getCenterOfMassTransform().inverse();
	SimdVector3 pointA = pointInWorld + normalOnBInWorld * depth;
	SimdVector3 localA = transAInv(pointA );
	SimdVector3 localB = transBInv(pointInWorld);
	ManifoldPoint newPt(localA,localB,normalOnBInWorld,depth);

	

	int insertIndex = m_manifoldPtr->GetCacheEntry(newPt);
	if (insertIndex >= 0)
	{
		m_manifoldPtr->ReplaceContactPoint(newPt,insertIndex);
	} else
	{
		m_manifoldPtr->AddManifoldPoint(newPt);
	}
}

