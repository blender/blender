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

#ifndef PERSISTENT_MANIFOLD_H
#define PERSISTENT_MANIFOLD_H


#include "../../LinearMath/btVector3.h"
#include "../../LinearMath/btTransform.h"
#include "btManifoldPoint.h"

struct btCollisionResult;

///contact breaking and merging threshold
extern btScalar gContactBreakingThreshold;

typedef bool (*ContactDestroyedCallback)(void* userPersistentData);
extern ContactDestroyedCallback	gContactDestroyedCallback;




#define MANIFOLD_CACHE_SIZE 4

///btPersistentManifold maintains contact points, and reduces them to 4.
///It does contact filtering/contact reduction.
ATTRIBUTE_ALIGNED16( class) btPersistentManifold 
{

	btManifoldPoint m_pointCache[MANIFOLD_CACHE_SIZE];

	/// this two body pointers can point to the physics rigidbody class.
	/// void* will allow any rigidbody class
	void* m_body0;
	void* m_body1;
	int	m_cachedPoints;

	
	/// sort cached points so most isolated points come first
	int	sortCachedPoints(const btManifoldPoint& pt);

	int		findContactPoint(const btManifoldPoint* unUsed, int numUnused,const btManifoldPoint& pt);

public:

	int m_index1;

	btPersistentManifold();

	btPersistentManifold(void* body0,void* body1)
		: m_body0(body0),m_body1(body1),m_cachedPoints(0)
	{
	}

	inline void* getBody0() { return m_body0;}
	inline void* getBody1() { return m_body1;}

	inline const void* getBody0() const { return m_body0;}
	inline const void* getBody1() const { return m_body1;}

	void	setBodies(void* body0,void* body1)
	{
		m_body0 = body0;
		m_body1 = body1;
	}

	void clearUserCache(btManifoldPoint& pt);

#ifdef DEBUG_PERSISTENCY
	void	DebugPersistency();
#endif //
	
	inline int	getNumContacts() const { return m_cachedPoints;}

	inline const btManifoldPoint& getContactPoint(int index) const
	{
		btAssert(index < m_cachedPoints);
		return m_pointCache[index];
	}

	inline btManifoldPoint& getContactPoint(int index)
	{
		btAssert(index < m_cachedPoints);
		return m_pointCache[index];
	}

	/// todo: get this margin from the current physics / collision environment
	btScalar	getContactBreakingThreshold() const;
	
	int getCacheEntry(const btManifoldPoint& newPoint) const;

	void AddManifoldPoint( const btManifoldPoint& newPoint);

	void removeContactPoint (int index)
	{
		clearUserCache(m_pointCache[index]);

		int lastUsedIndex = getNumContacts() - 1;
//		m_pointCache[index] = m_pointCache[lastUsedIndex];
		if(index != lastUsedIndex) 
		{
			m_pointCache[index] = m_pointCache[lastUsedIndex]; 
			//get rid of duplicated userPersistentData pointer
			m_pointCache[lastUsedIndex].m_userPersistentData = 0;
		}

		btAssert(m_pointCache[lastUsedIndex].m_userPersistentData==0);
		m_cachedPoints--;
	}
	void replaceContactPoint(const btManifoldPoint& newPoint,int insertIndex)
	{
		btAssert(validContactDistance(newPoint));

#define MAINTAIN_PERSISTENCY 1
#ifdef MAINTAIN_PERSISTENCY
		int	lifeTime = m_pointCache[insertIndex].getLifeTime();
		btAssert(lifeTime>=0);
		void* cache = m_pointCache[insertIndex].m_userPersistentData;
		
		m_pointCache[insertIndex] = newPoint;

		m_pointCache[insertIndex].m_userPersistentData = cache;
		m_pointCache[insertIndex].m_lifeTime = lifeTime;
#else
		clearUserCache(m_pointCache[insertIndex]);
		m_pointCache[insertIndex] = newPoint;
	
#endif
	}

	bool validContactDistance(const btManifoldPoint& pt) const
	{
		return pt.m_distance1 <= getContactBreakingThreshold();
	}
	/// calculated new worldspace coordinates and depth, and reject points that exceed the collision margin
	void	refreshContactPoints(  const btTransform& trA,const btTransform& trB);

	void	clearManifold();



}
;





#endif //PERSISTENT_MANIFOLD_H
