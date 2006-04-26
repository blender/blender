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


#include "ManifoldResult.h"
#include "NarrowPhaseCollision/PersistentManifold.h"
#include "CollisionDispatch/CollisionObject.h"

ManifoldResult::ManifoldResult(CollisionObject* body0,CollisionObject* body1,PersistentManifold* manifoldPtr)
		:m_manifoldPtr(manifoldPtr),
		m_body0(body0),
		m_body1(body1)
	{
	}

void ManifoldResult::AddContactPoint(const SimdVector3& normalOnBInWorld,const SimdVector3& pointInWorld,float depth)
{
	if (depth > m_manifoldPtr->GetContactBreakingTreshold())
		return;

	SimdTransform transAInv = m_body0->m_worldTransform.inverse();
	SimdTransform transBInv= m_body1->m_worldTransform.inverse();
	SimdVector3 pointA = pointInWorld + normalOnBInWorld * depth;
	SimdVector3 localA = transAInv(pointA );
	SimdVector3 localB = transBInv(pointInWorld);
	ManifoldPoint newPt(localA,localB,normalOnBInWorld,depth);

	

	int insertIndex = m_manifoldPtr->GetCacheEntry(newPt);
	if (insertIndex >= 0)
	{
		const ManifoldPoint& oldPoint = m_manifoldPtr->GetContactPoint(insertIndex);
		newPt.CopyPersistentInformation(oldPoint);
		m_manifoldPtr->ReplaceContactPoint(newPt,insertIndex);
	} else
	{
		m_manifoldPtr->AddManifoldPoint(newPt);
	}
}

