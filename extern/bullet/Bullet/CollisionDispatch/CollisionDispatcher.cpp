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
#include "CollisionShapes/CollisionShape.h"
#include "CollisionDispatch/CollisionObject.h"
#include <algorithm>

void CollisionDispatcher::FindUnions()
{
	if (m_useIslands)
	{
		for (int i=0;i<GetNumManifolds();i++)
		{
			const PersistentManifold* manifold = this->GetManifoldByIndexInternal(i);
			//static objects (invmass 0.f) don't merge !

			 const  CollisionObject* colObj0 = static_cast<const CollisionObject*>(manifold->GetBody0());
			 const  CollisionObject* colObj1 = static_cast<const CollisionObject*>(manifold->GetBody1());

			 if (colObj0 && colObj1 && NeedsResponse(*colObj0,*colObj1))
			 {
				if (((colObj0) && ((colObj0)->mergesSimulationIslands())) &&
					((colObj1) && ((colObj1)->mergesSimulationIslands())))
				{

					m_unionFind.unite((colObj0)->m_islandTag1,
						(colObj1)->m_islandTag1);
				}
			 }
			
			
		}
	}
	
}
	

	
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

	CollisionObject* body0 = (CollisionObject*)b0;
	CollisionObject* body1 = (CollisionObject*)b1;
	
	PersistentManifold* manifold = new PersistentManifold (body0,body1);
	m_manifoldsPtr.push_back(manifold);

	return manifold;
}

	
void CollisionDispatcher::ReleaseManifold(PersistentManifold* manifold)
{
	manifold->ClearManifold();

	std::vector<PersistentManifold*>::iterator i =
		std::find(m_manifoldsPtr.begin(), m_manifoldsPtr.end(), manifold);
	if (!(i == m_manifoldsPtr.end()))
	{
		std::swap(*i, m_manifoldsPtr.back());
		m_manifoldsPtr.pop_back();
		delete manifold;

	}
	
	
}

	
//
// todo: this is random access, it can be walked 'cache friendly'!
//
void CollisionDispatcher::BuildAndProcessIslands(int numBodies, IslandCallback* callback)
{
	for (int islandId=0;islandId<numBodies;islandId++)
	{

		std::vector<PersistentManifold*>  islandmanifold;
		
		//int numSleeping = 0;

		bool allSleeping = true;

		for (int i=0;i<GetNumManifolds();i++)
		{
			 PersistentManifold* manifold = this->GetManifoldByIndexInternal(i);
			 
			 //filtering for response

			 CollisionObject* colObj0 = static_cast<CollisionObject*>(manifold->GetBody0());
			 CollisionObject* colObj1 = static_cast<CollisionObject*>(manifold->GetBody1());


			 
			 {
				if (((colObj0) && (colObj0)->m_islandTag1 == (islandId)) ||
					((colObj1) && (colObj1)->m_islandTag1 == (islandId)))
				{

					if (((colObj0) && (colObj0)->GetActivationState()== ACTIVE_TAG) ||
						((colObj1) && (colObj1)->GetActivationState() == ACTIVE_TAG))
					{
						allSleeping = false;
					}
					if (((colObj0) && (colObj0)->GetActivationState()== DISABLE_DEACTIVATION) ||
						((colObj1) && (colObj1)->GetActivationState() == DISABLE_DEACTIVATION))
					{
						allSleeping = false;
					}

					if (NeedsResponse(*colObj0,*colObj1))
						islandmanifold.push_back(manifold);
				}
			 }
		}
		if (allSleeping)
		{
			//tag all as 'ISLAND_SLEEPING'
			for (size_t i=0;i<islandmanifold.size();i++)
			{
				PersistentManifold* manifold = islandmanifold[i];

				CollisionObject* colObj0 = static_cast<CollisionObject*>(manifold->GetBody0());
				CollisionObject* colObj1 = static_cast<CollisionObject*>(manifold->GetBody1());

				if ((colObj0))	
				{
					(colObj0)->SetActivationState( ISLAND_SLEEPING );
				}
				if ((colObj1))	
				{
					(colObj1)->SetActivationState( ISLAND_SLEEPING);
				}

			}
		} else
		{

			//tag all as 'ISLAND_SLEEPING'
			for (size_t i=0;i<islandmanifold.size();i++)
			{
				 PersistentManifold* manifold = islandmanifold[i];
				 CollisionObject* body0 = (CollisionObject*)manifold->GetBody0();
				 CollisionObject* body1 = (CollisionObject*)manifold->GetBody1();

				if (body0)	
				{
					if ( body0->GetActivationState() == ISLAND_SLEEPING)
					{
						body0->SetActivationState( WANTS_DEACTIVATION);
					}
				}
				if (body1)	
				{
					if ( body1->GetActivationState() == ISLAND_SLEEPING)
					{
						body1->SetActivationState(WANTS_DEACTIVATION);
					}
				}

			}

			if (islandmanifold.size())
			{
				callback->ProcessIsland(&islandmanifold[0],islandmanifold.size());
			}

		}
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

	//failed to find an algorithm
	return new EmptyAlgorithm(ci);
	
}

bool	CollisionDispatcher::NeedsResponse(const  CollisionObject& colObj0,const CollisionObject& colObj1)
{
	//here you can do filtering
	bool hasResponse = 
		(!(colObj0.m_collisionFlags & CollisionObject::noContactResponse)) &&
		(!(colObj1.m_collisionFlags & CollisionObject::noContactResponse));
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
	
	if ((body0->GetActivationState() == 2) &&(body1->GetActivationState() == 2))
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
