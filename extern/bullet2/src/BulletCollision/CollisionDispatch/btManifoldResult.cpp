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


#include "btManifoldResult.h"
#include "BulletCollision/NarrowPhaseCollision/btPersistentManifold.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"


///This is to allow MaterialCombiner/Custom Friction/Restitution values
ContactAddedCallback		gContactAddedCallback=0;

///User can override this material combiner by implementing gContactAddedCallback and setting body0->m_collisionFlags |= btCollisionObject::customMaterialCallback;
inline btScalar	calculateCombinedFriction(const btCollisionObject* body0,const btCollisionObject* body1)
{
	btScalar friction = body0->getFriction() * body1->getFriction();

	const btScalar MAX_FRICTION  = 10.f;
	if (friction < -MAX_FRICTION)
		friction = -MAX_FRICTION;
	if (friction > MAX_FRICTION)
		friction = MAX_FRICTION;
	return friction;

}

inline btScalar	calculateCombinedRestitution(const btCollisionObject* body0,const btCollisionObject* body1)
{
	return body0->getRestitution() * body1->getRestitution();
}



btManifoldResult::btManifoldResult(btCollisionObject* body0,btCollisionObject* body1)
		:m_manifoldPtr(0),
		m_body0(body0),
		m_body1(body1)
{
	m_rootTransA = body0->m_worldTransform;
	m_rootTransB = body1->m_worldTransform;
}


void btManifoldResult::addContactPoint(const btVector3& normalOnBInWorld,const btVector3& pointInWorld,float depth)
{
	assert(m_manifoldPtr);
	//order in manifold needs to match
	
	if (depth > m_manifoldPtr->getContactBreakingThreshold())
		return;

	bool isSwapped = m_manifoldPtr->getBody0() != m_body0;

	btTransform transAInv = isSwapped? m_rootTransB.inverse() : m_rootTransA.inverse();
	btTransform transBInv = isSwapped? m_rootTransA.inverse() : m_rootTransB.inverse();

	btVector3 pointA = pointInWorld + normalOnBInWorld * depth;
	btVector3 localA = transAInv(pointA );
	btVector3 localB = transBInv(pointInWorld);
	btManifoldPoint newPt(localA,localB,normalOnBInWorld,depth);

	

	int insertIndex = m_manifoldPtr->getCacheEntry(newPt);

	newPt.m_combinedFriction = calculateCombinedFriction(m_body0,m_body1);
	newPt.m_combinedRestitution = calculateCombinedRestitution(m_body0,m_body1);

	//User can override friction and/or restitution
	if (gContactAddedCallback &&
		//and if either of the two bodies requires custom material
		 ((m_body0->m_collisionFlags & btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK) ||
		   (m_body1->m_collisionFlags & btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK)))
	{
		//experimental feature info, for per-triangle material etc.
		btCollisionObject* obj0 = isSwapped? m_body1 : m_body0;
		btCollisionObject* obj1 = isSwapped? m_body0 : m_body1;
		(*gContactAddedCallback)(newPt,obj0,m_partId0,m_index0,obj1,m_partId1,m_index1);
	}

	if (insertIndex >= 0)
	{
		//const btManifoldPoint& oldPoint = m_manifoldPtr->getContactPoint(insertIndex);
		m_manifoldPtr->replaceContactPoint(newPt,insertIndex);
	} else
	{
		m_manifoldPtr->AddManifoldPoint(newPt);
	}
}

