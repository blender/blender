/*
 * Copyright (c) 2002-2006 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/

#include "CollisionWorld.h"
#include "CollisionDispatcher.h"
#include "NarrowPhaseCollision/CollisionObject.h"
#include "CollisionShapes/CollisionShape.h"

#include "BroadphaseCollision/BroadphaseInterface.h"
#include <algorithm>

void	CollisionWorld::UpdateActivationState()
{
	m_dispatcher->InitUnionFind(m_collisionObjects.size());
	
	// put the index into m_controllers into m_tag	
	{
		std::vector<CollisionObject*>::iterator i;
		
		int index = 0;
		for (i=m_collisionObjects.begin();
		!(i==m_collisionObjects.end()); i++)
		{
			
			CollisionObject*	collisionObject= (*i);
			collisionObject->m_islandTag1 = index;
			collisionObject->m_hitFraction = 1.f;
			index++;
			
		}
	}
	// do the union find
	
	m_dispatcher->FindUnions();
	
	// put the islandId ('find' value) into m_tag	
	{
		UnionFind& unionFind = m_dispatcher->GetUnionFind();
		
		std::vector<CollisionObject*>::iterator i;
		
		int index = 0;
		for (i=m_collisionObjects.begin();
		!(i==m_collisionObjects.end()); i++)
		{
			CollisionObject* collisionObject= (*i);
			
			if (collisionObject->mergesSimulationIslands())
			{
				collisionObject->m_islandTag1 = unionFind.find(index);
			} else
			{
				collisionObject->m_islandTag1 = -1;
			}
			index++;
		}
	}
	
}





void	CollisionWorld::AddCollisionObject(CollisionObject* collisionObject)
{
		m_collisionObjects.push_back(collisionObject);

		//calculate new AABB
		SimdTransform trans = collisionObject->m_worldTransform;

		SimdVector3	minAabb;
		SimdVector3	maxAabb;
		collisionObject->m_collisionShape->GetAabb(trans,minAabb,maxAabb);

		int type = collisionObject->m_collisionShape->GetShapeType();
		collisionObject->m_broadphaseHandle = GetBroadphase()->CreateProxy(
			minAabb,
			maxAabb,
			type,
			collisionObject
			);
		



}


void	CollisionWorld::RemoveCollisionObject(CollisionObject* collisionObject)
{
	
	
	bool removeFromBroadphase = false;
	
	{
		BroadphaseInterface* scene = GetBroadphase();
		BroadphaseProxy* bp = collisionObject->m_broadphaseHandle;
		
		//
		// only clear the cached algorithms
		//
		GetBroadphase()->CleanProxyFromPairs(bp);
		GetBroadphase()->DestroyProxy(bp);
	}


	std::vector<CollisionObject*>::iterator i =	std::find(m_collisionObjects.begin(), m_collisionObjects.end(), collisionObject);
		
	if (!(i == m_collisionObjects.end()))
		{
			std::swap(*i, m_collisionObjects.back());
			m_collisionObjects.pop_back();
		}
}
