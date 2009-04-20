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


#ifndef MANIFOLD_RESULT_H
#define MANIFOLD_RESULT_H

class btCollisionObject;
#include "BulletCollision/NarrowPhaseCollision/btPersistentManifold.h"
class btManifoldPoint;

#include "BulletCollision/NarrowPhaseCollision/btDiscreteCollisionDetectorInterface.h"

#include "LinearMath/btTransform.h"

typedef bool (*ContactAddedCallback)(btManifoldPoint& cp,	const btCollisionObject* colObj0,int partId0,int index0,const btCollisionObject* colObj1,int partId1,int index1);
extern ContactAddedCallback		gContactAddedCallback;



///btManifoldResult is a helper class to manage  contact results.
class btManifoldResult : public btDiscreteCollisionDetectorInterface::Result
{
	btPersistentManifold* m_manifoldPtr;

	//we need this for compounds
	btTransform	m_rootTransA;
	btTransform	m_rootTransB;

	btCollisionObject* m_body0;
	btCollisionObject* m_body1;
	int	m_partId0;
	int m_partId1;
	int m_index0;
	int m_index1;
	

public:

	btManifoldResult()
	{
	}

	btManifoldResult(btCollisionObject* body0,btCollisionObject* body1);

	virtual ~btManifoldResult() {};

	void	setPersistentManifold(btPersistentManifold* manifoldPtr)
	{
		m_manifoldPtr = manifoldPtr;
	}

	const btPersistentManifold*	getPersistentManifold() const
	{
		return m_manifoldPtr;
	}
	btPersistentManifold*	getPersistentManifold()
	{
		return m_manifoldPtr;
	}

	virtual void setShapeIdentifiers(int partId0,int index0,	int partId1,int index1)
	{
			m_partId0=partId0;
			m_partId1=partId1;
			m_index0=index0;
			m_index1=index1;		
	}


	virtual void addContactPoint(const btVector3& normalOnBInWorld,const btVector3& pointInWorld,btScalar depth);

	SIMD_FORCE_INLINE	void refreshContactPoints()
	{
		btAssert(m_manifoldPtr);
		if (!m_manifoldPtr->getNumContacts())
			return;

		bool isSwapped = m_manifoldPtr->getBody0() != m_body0;

		if (isSwapped)
		{
			m_manifoldPtr->refreshContactPoints(m_rootTransB,m_rootTransA);
		} else
		{
			m_manifoldPtr->refreshContactPoints(m_rootTransA,m_rootTransB);
		}
	}


};

#endif //MANIFOLD_RESULT_H
