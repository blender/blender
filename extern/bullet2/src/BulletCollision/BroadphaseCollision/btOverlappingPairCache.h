
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


#include "btBroadphaseInterface.h"
#include "btBroadphaseProxy.h"
#include "LinearMath/btPoint3.h"
#include "LinearMath/btAlignedObjectArray.h"
class btDispatcher;

///disable the USE_HASH_PAIRCACHE define to use a pair manager that sorts the pairs to find duplicates/non-overlap
#define USE_HASH_PAIRCACHE 1


struct	btOverlapCallback
{
	virtual ~btOverlapCallback()
	{}
	//return true for deletion of the pair
	virtual bool	processOverlap(btBroadphasePair& pair) = 0;
};

struct btOverlapFilterCallback
{
	virtual ~btOverlapFilterCallback()
	{}
	// return true when pairs need collision
	virtual bool	needBroadphaseCollision(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1) const = 0;
};

typedef btAlignedObjectArray<btBroadphasePair>	btBroadphasePairArray;

#ifdef USE_HASH_PAIRCACHE


/// Hash-space based Pair Cache, thanks to Erin Catto, Box2D, http://www.box2d.org, and Pierre Terdiman, Codercorner, http://codercorner.com

extern int gRemovePairs;
extern int gAddedPairs;
extern int gFindPairs;

const int BT_NULL_PAIR=0xffffffff;

class btOverlappingPairCache
{
	btBroadphasePairArray	m_overlappingPairArray;
	btOverlapFilterCallback* m_overlapFilterCallback;
	bool		m_blockedForChanges;


public:
	btOverlappingPairCache();
	virtual ~btOverlappingPairCache();

	
	void	removeOverlappingPairsContainingProxy(btBroadphaseProxy* proxy,btDispatcher* dispatcher);

	void*	removeOverlappingPair(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1,btDispatcher* dispatcher);
	
	SIMD_FORCE_INLINE bool needsBroadphaseCollision(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1) const
	{
		if (m_overlapFilterCallback)
			return m_overlapFilterCallback->needBroadphaseCollision(proxy0,proxy1);

		bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
		collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);
		
		return collides;
	}

	// Add a pair and return the new pair. If the pair already exists,
	// no new pair is created and the old one is returned.
	SIMD_FORCE_INLINE	btBroadphasePair* 	addOverlappingPair(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1)
	{
		gAddedPairs++;

		if (!needsBroadphaseCollision(proxy0,proxy1))
			return 0;

		return internalAddPair(proxy0,proxy1);
	}

	

	void	cleanProxyFromPairs(btBroadphaseProxy* proxy,btDispatcher* dispatcher);

	
	virtual void	processAllOverlappingPairs(btOverlapCallback*,btDispatcher* dispatcher);

	btBroadphasePair*	getOverlappingPairArrayPtr()
	{
		return &m_overlappingPairArray[0];
	}

	const btBroadphasePair*	getOverlappingPairArrayPtr() const
	{
		return &m_overlappingPairArray[0];
	}

	btBroadphasePairArray&	getOverlappingPairArray()
	{
		return m_overlappingPairArray;
	}

	const btBroadphasePairArray&	getOverlappingPairArray() const
	{
		return m_overlappingPairArray;
	}

	void	cleanOverlappingPair(btBroadphasePair& pair,btDispatcher* dispatcher);



	btBroadphasePair* findPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1);

	int GetCount() const { return m_overlappingPairArray.size(); }
//	btBroadphasePair* GetPairs() { return m_pairs; }

	btOverlapFilterCallback* getOverlapFilterCallback()
	{
		return m_overlapFilterCallback;
	}

	void setOverlapFilterCallback(btOverlapFilterCallback* callback)
	{
		m_overlapFilterCallback = callback;
	}

	int	getNumOverlappingPairs() const
	{
		return m_overlappingPairArray.size();
	}
private:
	
	btBroadphasePair* 	internalAddPair(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1);

	void	growTables();

	SIMD_FORCE_INLINE bool equalsPair(const btBroadphasePair& pair, int proxyId1, int proxyId2)
	{	
		return pair.m_pProxy0->getUid() == proxyId1 && pair.m_pProxy1->getUid() == proxyId2;
	}

	/*
	// Thomas Wang's hash, see: http://www.concentric.net/~Ttwang/tech/inthash.htm
	// This assumes proxyId1 and proxyId2 are 16-bit.
	SIMD_FORCE_INLINE int getHash(int proxyId1, int proxyId2)
	{
		int key = (proxyId2 << 16) | proxyId1;
		key = ~key + (key << 15);
		key = key ^ (key >> 12);
		key = key + (key << 2);
		key = key ^ (key >> 4);
		key = key * 2057;
		key = key ^ (key >> 16);
		return key;
	}
	*/


	
	SIMD_FORCE_INLINE	unsigned int getHash(unsigned int proxyId1, unsigned int proxyId2)
	{
		int key = ((unsigned int)proxyId1) | (((unsigned int)proxyId1) <<16);
		// Thomas Wang's hash

		key += ~(key << 15);
		key ^=  (key >> 10);
		key +=  (key << 3);
		key ^=  (key >> 6);
		key += ~(key << 11);
		key ^=  (key >> 16);
		return key;
	}
	




	SIMD_FORCE_INLINE btBroadphasePair* internalFindPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1, int hash)
	{
		int proxyId1 = proxy0->getUid();
		int proxyId2 = proxy1->getUid();
		if (proxyId1 > proxyId2) 
			btSwap(proxyId1, proxyId2);

		int index = m_hashTable[hash];
		
		while( index != BT_NULL_PAIR && equalsPair(m_overlappingPairArray[index], proxyId1, proxyId2) == false)
		{
			index = m_next[index];
		}

		if ( index == BT_NULL_PAIR )
		{
			return NULL;
		}

		btAssert(index < m_overlappingPairArray.size());

		return &m_overlappingPairArray[index];
	}


public:
	
	btAlignedObjectArray<int>	m_hashTable;
	btAlignedObjectArray<int>	m_next;
	
};



#else//USE_HASH_PAIRCACHE

#define USE_LAZY_REMOVAL 1

///btOverlappingPairCache maintains the objects with overlapping AABB
///Typically managed by the Broadphase, Axis3Sweep or btSimpleBroadphase
class	btOverlappingPairCache
{
	protected:
		//avoid brute-force finding all the time
		btBroadphasePairArray	m_overlappingPairArray;

		//during the dispatch, check that user doesn't destroy/create proxy
		bool		m_blockedForChanges;
		
		//if set, use the callback instead of the built in filter in needBroadphaseCollision
		btOverlapFilterCallback* m_overlapFilterCallback;

	public:
			
		btOverlappingPairCache();	
		virtual ~btOverlappingPairCache();

		virtual void	processAllOverlappingPairs(btOverlapCallback*,btDispatcher* dispatcher);

		void*	removeOverlappingPair(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1,btDispatcher* dispatcher);

		void	cleanOverlappingPair(btBroadphasePair& pair,btDispatcher* dispatcher);
		
		btBroadphasePair*	addOverlappingPair(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1);

		btBroadphasePair*	findPair(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1);
			
		
		void	cleanProxyFromPairs(btBroadphaseProxy* proxy,btDispatcher* dispatcher);

		void	removeOverlappingPairsContainingProxy(btBroadphaseProxy* proxy,btDispatcher* dispatcher);


		inline bool needsBroadphaseCollision(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1) const
		{
			if (m_overlapFilterCallback)
				return m_overlapFilterCallback->needBroadphaseCollision(proxy0,proxy1);

			bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
			collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);
			
			return collides;
		}
		
		btBroadphasePairArray&	getOverlappingPairArray()
		{
			return m_overlappingPairArray;
		}

		const btBroadphasePairArray&	getOverlappingPairArray() const
		{
			return m_overlappingPairArray;
		}

		


		btBroadphasePair*	getOverlappingPairArrayPtr()
		{
			return &m_overlappingPairArray[0];
		}

		const btBroadphasePair*	getOverlappingPairArrayPtr() const
		{
			return &m_overlappingPairArray[0];
		}

		int	getNumOverlappingPairs() const
		{
			return m_overlappingPairArray.size();
		}
		
		btOverlapFilterCallback* getOverlapFilterCallback()
		{
			return m_overlapFilterCallback;
		}

		void setOverlapFilterCallback(btOverlapFilterCallback* callback)
		{
			m_overlapFilterCallback = callback;
		}

};
#endif //USE_HASH_PAIRCACHE

#endif //OVERLAPPING_PAIR_CACHE_H


