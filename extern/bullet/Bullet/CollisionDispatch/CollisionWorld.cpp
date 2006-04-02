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

#include "CollisionWorld.h"
#include "CollisionDispatcher.h"
#include "CollisionDispatch/CollisionObject.h"
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

void	CollisionWorld::PerformDiscreteCollisionDetection()
{
	DispatcherInfo	dispatchInfo;
	dispatchInfo.m_timeStep = 0.f;
	dispatchInfo.m_stepCount = 0;

	//update aabb (of all moved objects)

	SimdVector3 aabbMin,aabbMax;
	for (size_t i=0;i<m_collisionObjects.size();i++)
	{
		m_collisionObjects[i]->m_collisionShape->GetAabb(m_collisionObjects[i]->m_worldTransform,aabbMin,aabbMax);
		m_broadphase->SetAabb(m_collisionObjects[i]->m_broadphaseHandle,aabbMin,aabbMax);
	}

	m_broadphase->DispatchAllCollisionPairs(*GetDispatcher(),dispatchInfo);
}


void	CollisionWorld::RemoveCollisionObject(CollisionObject* collisionObject)
{
	
	
	//bool removeFromBroadphase = false;
	
	{
		
		BroadphaseProxy* bp = collisionObject->m_broadphaseHandle;
		if (bp)
		{
			//
			// only clear the cached algorithms
			//
			GetBroadphase()->CleanProxyFromPairs(bp);
			GetBroadphase()->DestroyProxy(bp);
		}
	}


	std::vector<CollisionObject*>::iterator i =	std::find(m_collisionObjects.begin(), m_collisionObjects.end(), collisionObject);
		
	if (!(i == m_collisionObjects.end()))
		{
			std::swap(*i, m_collisionObjects.back());
			m_collisionObjects.pop_back();
		}
}
