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

#ifndef PARALLEL_ISLAND_DISPATCHER_H
#define PARALLEL_ISLAND_DISPATCHER_H

#include "BroadphaseCollision/Dispatcher.h"
#include "NarrowPhaseCollision/PersistentManifold.h"
#include "CollisionDispatch/UnionFind.h"
#include "CollisionDispatch/ManifoldResult.h"

#include "BroadphaseCollision/BroadphaseProxy.h"
#include <vector>

class IDebugDraw;


#include "CollisionDispatch/CollisionCreateFunc.h"




///ParallelIslandDispatcher dispatches entire simulation islands in parallel.
///For both collision detection and constraint solving.
///This development heads toward multi-core, CELL SPU and GPU approach
class ParallelIslandDispatcher : public Dispatcher
{
	
	std::vector<PersistentManifold*>	m_manifoldsPtr;

	UnionFind m_unionFind;

	bool m_useIslands;
	
	ManifoldResult	m_defaultManifoldResult;
	
	CollisionAlgorithmCreateFunc* m_doubleDispatch[MAX_BROADPHASE_COLLISION_TYPES][MAX_BROADPHASE_COLLISION_TYPES];
	
public:
	
	UnionFind& GetUnionFind() { return m_unionFind;}

	struct	IslandCallback
	{
		virtual ~IslandCallback() {};

		virtual	void	ProcessIsland(PersistentManifold**	manifolds,int numManifolds) = 0;
	};


	int	GetNumManifolds() const
	{ 
		return m_manifoldsPtr.size();
	}

	 PersistentManifold* GetManifoldByIndexInternal(int index)
	{
		return m_manifoldsPtr[index];
	}

	 const PersistentManifold* GetManifoldByIndexInternal(int index) const
	{
		return m_manifoldsPtr[index];
	}

	void InitUnionFind(int n)
	{
		if (m_useIslands)
			m_unionFind.reset(n);
	}
	
	void FindUnions();
	
	int m_count;
	
	ParallelIslandDispatcher ();
	virtual ~ParallelIslandDispatcher() {};

	virtual PersistentManifold*	GetNewManifold(void* b0,void* b1);
	
	virtual void ReleaseManifold(PersistentManifold* manifold);

	
	virtual void BuildAndProcessIslands(CollisionObjectArray& collisionObjects, IslandCallback* callback);

	///allows the user to get contact point callbacks 
	virtual	ManifoldResult*	GetNewManifoldResult(CollisionObject* obj0,CollisionObject* obj1,PersistentManifold* manifold);

	///allows the user to get contact point callbacks 
	virtual	void	ReleaseManifoldResult(ManifoldResult*);

	virtual void ClearManifold(PersistentManifold* manifold);

			
	CollisionAlgorithm* FindAlgorithm(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1)
	{
		CollisionAlgorithm* algo = InternalFindAlgorithm(proxy0,proxy1);
		return algo;
	}
	
	CollisionAlgorithm* InternalFindAlgorithm(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1);
	
	virtual bool	NeedsCollision(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1);
	
	virtual bool	NeedsResponse(const CollisionObject& colObj0,const CollisionObject& colObj1);

	virtual int GetUniqueId() { return RIGIDBODY_DISPATCHER;}

	virtual void	DispatchAllCollisionPairs(BroadphasePair* pairs,int numPairs,DispatcherInfo& dispatchInfo);
	
	

};

#endif //PARALLEL_ISLAND_DISPATCHER_H

