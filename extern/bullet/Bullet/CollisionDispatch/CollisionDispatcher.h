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

#ifndef COLLISION__DISPATCHER_H
#define COLLISION__DISPATCHER_H

#include "BroadphaseCollision/Dispatcher.h"
#include "NarrowPhaseCollision/PersistentManifold.h"
#include "CollisionDispatch/UnionFind.h"
#include "BroadphaseCollision/BroadphaseProxy.h"
#include <vector>


class IDebugDraw;




struct CollisionAlgorithmCreateFunc
{
	bool m_swapped;
	
	CollisionAlgorithmCreateFunc()
		:m_swapped(false)
	{
	}
	virtual	CollisionAlgorithm* CreateCollisionAlgorithm(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1)
	{
		return 0;
	}
};


///CollisionDispatcher supports algorithms that handle ConvexConvex and ConvexConcave collision pairs.
///Time of Impact, Closest Points and Penetration Depth.
class CollisionDispatcher : public Dispatcher
{
	
	std::vector<PersistentManifold*>	m_manifoldsPtr;

	UnionFind m_unionFind;

	bool m_useIslands;
	
	
	CollisionAlgorithmCreateFunc* m_doubleDispatch[MAX_BROADPHASE_COLLISION_TYPES][MAX_BROADPHASE_COLLISION_TYPES];
	
public:
	
	UnionFind& GetUnionFind() { return m_unionFind;}

	struct	IslandCallback
	{
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
	
	CollisionDispatcher ();

	virtual PersistentManifold*	GetNewManifold(void* b0,void* b1);
	
	virtual void ReleaseManifold(PersistentManifold* manifold);

	
	virtual void BuildAndProcessIslands(int numBodies, IslandCallback* callback);

		
	CollisionAlgorithm* FindAlgorithm(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1)
	{
		CollisionAlgorithm* algo = InternalFindAlgorithm(proxy0,proxy1);
		return algo;
	}
	
	CollisionAlgorithm* InternalFindAlgorithm(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1);
	
	virtual bool	NeedsCollision(BroadphaseProxy& proxy0,BroadphaseProxy& proxy1);


	virtual int GetUniqueId() { return RIGIDBODY_DISPATCHER;}
	
	

};

#endif //COLLISION__DISPATCHER_H

