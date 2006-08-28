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


///This is to allow MaterialCombiner/Custom Friction/Restitution values
ContactAddedCallback		gContactAddedCallback=0;

///User can override this material combiner by implementing gContactAddedCallback and setting body0->m_collisionFlags |= CollisionObject::customMaterialCallback;
inline SimdScalar	calculateCombinedFriction(const CollisionObject* body0,const CollisionObject* body1)
{
	SimdScalar friction = body0->getFriction() * body1->getFriction();

	const SimdScalar MAX_FRICTION  = 10.f;
	if (friction < -MAX_FRICTION)
		friction = -MAX_FRICTION;
	if (friction > MAX_FRICTION)
		friction = MAX_FRICTION;
	return friction;

}

inline SimdScalar	calculateCombinedRestitution(const CollisionObject* body0,const CollisionObject* body1)
{
	return body0->getRestitution() * body1->getRestitution();
}



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


	SimdTransform transAInv = m_body0->m_cachedInvertedWorldTransform;
	SimdTransform transBInv= m_body1->m_cachedInvertedWorldTransform;

	//transAInv = m_body0->m_worldTransform.inverse();
	//transBInv= m_body1->m_worldTransform.inverse();
	SimdVector3 pointA = pointInWorld + normalOnBInWorld * depth;
	SimdVector3 localA = transAInv(pointA );
	SimdVector3 localB = transBInv(pointInWorld);
	ManifoldPoint newPt(localA,localB,normalOnBInWorld,depth);

	

	int insertIndex = m_manifoldPtr->GetCacheEntry(newPt);
	if (insertIndex >= 0)
	{

// This is not needed, just use the old info!
//		const ManifoldPoint& oldPoint = m_manifoldPtr->GetContactPoint(insertIndex);
//		newPt.CopyPersistentInformation(oldPoint);
//		m_manifoldPtr->ReplaceContactPoint(newPt,insertIndex);


	} else
	{

		newPt.m_combinedFriction = calculateCombinedFriction(m_body0,m_body1);
		newPt.m_combinedRestitution = calculateCombinedRestitution(m_body0,m_body1);

		//User can override friction and/or restitution
		if (gContactAddedCallback &&
			//and if either of the two bodies requires custom material
			 ((m_body0->m_collisionFlags & CollisionObject::customMaterialCallback) ||
			   (m_body1->m_collisionFlags & CollisionObject::customMaterialCallback)))
		{
			//experimental feature info, for per-triangle material etc.
			(*gContactAddedCallback)(newPt,m_body0,m_partId0,m_index0,m_body1,m_partId1,m_index1);
		}

		m_manifoldPtr->AddManifoldPoint(newPt);
	}
}

