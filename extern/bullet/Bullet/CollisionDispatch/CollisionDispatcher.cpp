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


#include "CollisionDispatcher.h"


#include "BroadphaseCollision/CollisionAlgorithm.h"
#include "CollisionDispatch/ConvexConvexAlgorithm.h"
#include "CollisionDispatch/EmptyCollisionAlgorithm.h"
#include "CollisionDispatch/ConvexConcaveCollisionAlgorithm.h"
#include "CollisionDispatch/CompoundCollisionAlgorithm.h"
#include "CollisionShapes/CollisionShape.h"
#include "CollisionDispatch/CollisionObject.h"
#include <algorithm>
#include "BroadphaseCollision/OverlappingPairCache.h"

int gNumManifold = 0;


	

	
CollisionDispatcher::CollisionDispatcher (): 
	m_useIslands(true),
		m_defaultManifoldResult(0,0,0),
		m_count(0)
{
	int i;
	
	for (i=0;i<MAX_BROADPHASE_COLLISION_TYPES;i++)
	{
		for (int j=0;j<MAX_BROADPHASE_COLLISION_TYPES;j++)
		{
			m_doubleDispatch[i][j] = 0;
		}
	}
	
	
};
	
PersistentManifold*	CollisionDispatcher::GetNewManifold(void* b0,void* b1) 
{ 
	gNumManifold++;
	
	//ASSERT(gNumManifold < 65535);
	

	CollisionObject* body0 = (CollisionObject*)b0;
	CollisionObject* body1 = (CollisionObject*)b1;
	
	PersistentManifold* manifold = new PersistentManifold (body0,body1);
	m_manifoldsPtr.push_back(manifold);

	return manifold;
}

void CollisionDispatcher::ClearManifold(PersistentManifold* manifold)
{
	manifold->ClearManifold();
}

	
void CollisionDispatcher::ReleaseManifold(PersistentManifold* manifold)
{
	
	gNumManifold--;

	//printf("ReleaseManifold: gNumManifold %d\n",gNumManifold);

	ClearManifold(manifold);

	std::vector<PersistentManifold*>::iterator i =
		std::find(m_manifoldsPtr.begin(), m_manifoldsPtr.end(), manifold);
	if (!(i == m_manifoldsPtr.end()))
	{
		std::swap(*i, m_manifoldsPtr.back());
		m_manifoldsPtr.pop_back();
		delete manifold;

	}
	
	
}

	



CollisionAlgorithm* CollisionDispatcher::InternalFindAlgorithm(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1)
{
	m_count++;
	CollisionObject* body0 = (CollisionObject*)proxy0.m_clientObject;
	CollisionObject* body1 = (CollisionObject*)proxy1.m_clientObject;

	CollisionAlgorithmConstructionInfo ci;
	ci.m_dispatcher = this;
	
	if (body0->m_collisionShape->IsConvex() && body1->m_collisionShape->IsConvex() )
	{
		return new ConvexConvexAlgorithm(0,ci,&proxy0,&proxy1);			
	}

	if (body0->m_collisionShape->IsConvex() && body1->m_collisionShape->IsConcave())
	{
		return new ConvexConcaveCollisionAlgorithm(ci,&proxy0,&proxy1);
	}

	if (body1->m_collisionShape->IsConvex() && body0->m_collisionShape->IsConcave())
	{
		return new ConvexConcaveCollisionAlgorithm(ci,&proxy1,&proxy0);
	}

	if (body0->m_collisionShape->IsCompound())
	{
		return new CompoundCollisionAlgorithm(ci,&proxy0,&proxy1);
	} else
	{
		if (body1->m_collisionShape->IsCompound())
		{
			return new CompoundCollisionAlgorithm(ci,&proxy1,&proxy0);
		}
	}

	//failed to find an algorithm
	return new EmptyAlgorithm(ci);
	
}

bool	CollisionDispatcher::NeedsResponse(const  CollisionObject& colObj0,const CollisionObject& colObj1)
{

	
	//here you can do filtering
	bool hasResponse = 
		(!(colObj0.m_collisionFlags & CollisionObject::noContactResponse)) &&
		(!(colObj1.m_collisionFlags & CollisionObject::noContactResponse));
	hasResponse = hasResponse &&
		(colObj0.IsActive() || colObj1.IsActive());
	return hasResponse;
}

bool	CollisionDispatcher::NeedsCollision(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1)
{

	CollisionObject* body0 = (CollisionObject*)proxy0.m_clientObject;
	CollisionObject* body1 = (CollisionObject*)proxy1.m_clientObject;

	assert(body0);
	assert(body1);

	bool needsCollision = true;

	if ((body0->m_collisionFlags & CollisionObject::isStatic) && 
		(body1->m_collisionFlags & CollisionObject::isStatic))
		needsCollision = false;
		
	if ((!body0->IsActive()) && (!body1->IsActive()))
		needsCollision = false;
	
	return needsCollision ;

}

///allows the user to get contact point callbacks 
ManifoldResult*	CollisionDispatcher::GetNewManifoldResult(CollisionObject* obj0,CollisionObject* obj1,PersistentManifold* manifold)
{


	//in-place, this prevents parallel dispatching, but just adding a list would fix that.
	ManifoldResult* manifoldResult = new (&m_defaultManifoldResult)	ManifoldResult(obj0,obj1,manifold);
	return manifoldResult;
}
	
///allows the user to get contact point callbacks 
void	CollisionDispatcher::ReleaseManifoldResult(ManifoldResult*)
{

}


void	CollisionDispatcher::DispatchAllCollisionPairs(BroadphasePair* pairs,int numPairs,DispatcherInfo& dispatchInfo)
{
	//m_blockedForChanges = true;

	int i;

	int dispatcherId = GetUniqueId();

	

	for (i=0;i<numPairs;i++)
	{

		BroadphasePair& pair = pairs[i];

		if (dispatcherId>= 0)
		{
			//dispatcher will keep algorithms persistent in the collision pair
			if (!pair.m_algorithms[dispatcherId])
			{
				pair.m_algorithms[dispatcherId] = FindAlgorithm(
					*pair.m_pProxy0,
					*pair.m_pProxy1);
			}

			if (pair.m_algorithms[dispatcherId])
			{
				if (dispatchInfo.m_dispatchFunc == 		DispatcherInfo::DISPATCH_DISCRETE)
				{
					pair.m_algorithms[dispatcherId]->ProcessCollision(pair.m_pProxy0,pair.m_pProxy1,dispatchInfo);
				} else
				{
					float toi = pair.m_algorithms[dispatcherId]->CalculateTimeOfImpact(pair.m_pProxy0,pair.m_pProxy1,dispatchInfo);
					if (dispatchInfo.m_timeOfImpact > toi)
						dispatchInfo.m_timeOfImpact = toi;

				}
			}
		} else
		{
			//non-persistent algorithm dispatcher
			CollisionAlgorithm* algo = FindAlgorithm(
				*pair.m_pProxy0,
				*pair.m_pProxy1);

			if (algo)
			{
				if (dispatchInfo.m_dispatchFunc == 		DispatcherInfo::DISPATCH_DISCRETE)
				{
					algo->ProcessCollision(pair.m_pProxy0,pair.m_pProxy1,dispatchInfo);
				} else
				{
					float toi = algo->CalculateTimeOfImpact(pair.m_pProxy0,pair.m_pProxy1,dispatchInfo);
					if (dispatchInfo.m_timeOfImpact > toi)
						dispatchInfo.m_timeOfImpact = toi;
				}
			}
		}

	}

	//m_blockedForChanges = false;

}