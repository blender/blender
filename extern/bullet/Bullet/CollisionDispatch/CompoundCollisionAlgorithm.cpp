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

#include "CollisionDispatch/CompoundCollisionAlgorithm.h"
#include "CollisionDispatch/CollisionObject.h"
#include "CollisionShapes/CompoundShape.h"


CompoundCollisionAlgorithm::CompoundCollisionAlgorithm( const CollisionAlgorithmConstructionInfo& ci,BroadphaseProxy* proxy0,BroadphaseProxy* proxy1)
:m_dispatcher(ci.m_dispatcher),
m_compoundProxy(*proxy0),
m_otherProxy(*proxy1)
{
	CollisionObject* colObj = static_cast<CollisionObject*>(m_compoundProxy.m_clientObject);
	assert (colObj->m_collisionShape->IsCompound());
	
	CompoundShape* compoundShape = static_cast<CompoundShape*>(colObj->m_collisionShape);
	int numChildren = compoundShape->GetNumChildShapes();
	m_childProxies.resize( numChildren );
	int i;
	for (i=0;i<numChildren;i++)
	{
		m_childProxies[i] = BroadphaseProxy(*proxy0);
	}

	m_childCollisionAlgorithms.resize(numChildren);
	for (i=0;i<numChildren;i++)
	{
		CollisionShape* childShape = compoundShape->GetChildShape(i);
		CollisionObject* colObj = static_cast<CollisionObject*>(m_childProxies[i].m_clientObject);
		CollisionShape* orgShape = colObj->m_collisionShape;
		colObj->m_collisionShape = childShape;
		m_childCollisionAlgorithms[i] = m_dispatcher->FindAlgorithm(m_childProxies[i],m_otherProxy);
		colObj->m_collisionShape =orgShape;
	}
}


CompoundCollisionAlgorithm::~CompoundCollisionAlgorithm()
{
	int numChildren = m_childCollisionAlgorithms.size();
	int i;
	for (i=0;i<numChildren;i++)
	{
		delete m_childCollisionAlgorithms[i];
	}
}

void CompoundCollisionAlgorithm::ProcessCollision (BroadphaseProxy* ,BroadphaseProxy* ,const DispatcherInfo& dispatchInfo)
{
	CollisionObject* colObj = static_cast<CollisionObject*>(m_compoundProxy.m_clientObject);
	assert (colObj->m_collisionShape->IsCompound());
	
	CompoundShape* compoundShape = static_cast<CompoundShape*>(colObj->m_collisionShape);

	//We will use the OptimizedBVH, AABB tree to cull potential child-overlaps
	//If both proxies are Compound, we will deal with that directly, by performing sequential/parallel tree traversals
	//given Proxy0 and Proxy1, if both have a tree, Tree0 and Tree1, this means:
	//determine overlapping nodes of Proxy1 using Proxy0 AABB against Tree1
	//then use each overlapping node AABB against Tree0
	//and vise versa.


	int numChildren = m_childCollisionAlgorithms.size();
	int i;
	for (i=0;i<numChildren;i++)
	{
		//temporarily exchange parent CollisionShape with childShape, and recurse
		CollisionShape* childShape = compoundShape->GetChildShape(i);
		CollisionObject* colObj = static_cast<CollisionObject*>(m_childProxies[i].m_clientObject);

		//backup
		SimdTransform	orgTrans = colObj->m_worldTransform;
		CollisionShape* orgShape = colObj->m_collisionShape;

		SimdTransform childTrans = compoundShape->GetChildTransform(i);
		SimdTransform	newChildWorldTrans = orgTrans*childTrans ;
		colObj->m_worldTransform = newChildWorldTrans;

		colObj->m_collisionShape = childShape;
		m_childCollisionAlgorithms[i]->ProcessCollision(&m_childProxies[i],&m_otherProxy,dispatchInfo);
		//revert back
		colObj->m_collisionShape =orgShape;
		colObj->m_worldTransform = orgTrans;
	}
}

float	CompoundCollisionAlgorithm::CalculateTimeOfImpact(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1,const DispatcherInfo& dispatchInfo)
{
	CollisionObject* colObj = static_cast<CollisionObject*>(m_compoundProxy.m_clientObject);
	assert (colObj->m_collisionShape->IsCompound());
	
	CompoundShape* compoundShape = static_cast<CompoundShape*>(colObj->m_collisionShape);

	//We will use the OptimizedBVH, AABB tree to cull potential child-overlaps
	//If both proxies are Compound, we will deal with that directly, by performing sequential/parallel tree traversals
	//given Proxy0 and Proxy1, if both have a tree, Tree0 and Tree1, this means:
	//determine overlapping nodes of Proxy1 using Proxy0 AABB against Tree1
	//then use each overlapping node AABB against Tree0
	//and vise versa.

	float hitFraction = 1.f;

	int numChildren = m_childCollisionAlgorithms.size();
	int i;
	for (i=0;i<numChildren;i++)
	{
		//temporarily exchange parent CollisionShape with childShape, and recurse
		CollisionShape* childShape = compoundShape->GetChildShape(i);
		CollisionObject* colObj = static_cast<CollisionObject*>(m_childProxies[i].m_clientObject);

		//backup
		SimdTransform	orgTrans = colObj->m_worldTransform;
		CollisionShape* orgShape = colObj->m_collisionShape;

		SimdTransform childTrans = compoundShape->GetChildTransform(i);
		SimdTransform	newChildWorldTrans = orgTrans*childTrans ;
		colObj->m_worldTransform = newChildWorldTrans;

		colObj->m_collisionShape = childShape;
		float frac = m_childCollisionAlgorithms[i]->CalculateTimeOfImpact(&m_childProxies[i],&m_otherProxy,dispatchInfo);
		if (frac<hitFraction)
		{
			hitFraction = frac;
		}
		//revert back
		colObj->m_collisionShape =orgShape;
		colObj->m_worldTransform = orgTrans;
	}
	return hitFraction;

}