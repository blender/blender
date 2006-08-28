
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

#ifndef OVERLAPPING_PAIR_CACHE_H
#define OVERLAPPING_PAIR_CACHE_H


#include "BroadphaseInterface.h"
#include "BroadphaseProxy.h"
#include "SimdPoint3.h"


///OverlappingPairCache maintains the objects with overlapping AABB
///Typically managed by the Broadphase, Axis3Sweep or SimpleBroadphase
class	OverlappingPairCache : public BroadphaseInterface
{

	BroadphasePair*	m_OverlappingPairs;
	int	m_NumOverlapBroadphasePair;
	int m_maxOverlap;
	
	//during the dispatch, check that user doesn't destroy/create proxy
	bool		m_blockedForChanges;

	
	public:
		
	OverlappingPairCache(int maxOverlap);	
	virtual ~OverlappingPairCache();
	
	int		GetNumOverlappingPairs() const
	{
		return m_NumOverlapBroadphasePair;
	}

	BroadphasePair& GetOverlappingPair(int index)
	{
		return m_OverlappingPairs[index];
	}

	void	RemoveOverlappingPair(BroadphasePair& pair);

	void	CleanOverlappingPair(BroadphasePair& pair);
	
	void	AddOverlappingPair(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1);

	BroadphasePair*	FindPair(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1);
	
	
	
	void	CleanProxyFromPairs(BroadphaseProxy* proxy);

	void	RemoveOverlappingPairsContainingProxy(BroadphaseProxy* proxy);


	inline bool NeedsCollision(BroadphaseProxy* proxy0,BroadphaseProxy* proxy1) const
	{
		bool collides = proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask;
		collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);
		
		return collides;
	}
		
	

	virtual void	RefreshOverlappingPairs() =0;




};
#endif //OVERLAPPING_PAIR_CACHE_H