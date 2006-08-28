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

#include "NarrowPhaseCollision/DiscreteCollisionDetectorInterface.h"
struct CollisionObject;
class PersistentManifold;
class ManifoldPoint;

typedef bool (*ContactAddedCallback)(ManifoldPoint& cp,	const CollisionObject* colObj0,int partId0,int index0,const CollisionObject* colObj1,int partId1,int index1);
extern ContactAddedCallback		gContactAddedCallback;



///ManifoldResult is a helper class to manage  contact results.
class ManifoldResult : public DiscreteCollisionDetectorInterface::Result
{
	PersistentManifold* m_manifoldPtr;
	CollisionObject* m_body0;
	CollisionObject* m_body1;
	int	m_partId0;
	int m_partId1;
	int m_index0;
	int m_index1;
public:

	ManifoldResult(CollisionObject* body0,CollisionObject* body1,PersistentManifold* manifoldPtr);

	virtual ~ManifoldResult() {};

	virtual void SetShapeIdentifiers(int partId0,int index0,	int partId1,int index1)
	{
			m_partId0=partId0;
			m_partId1=partId1;
			m_index0=index0;
			m_index1=index1;		
	}

	virtual void AddContactPoint(const SimdVector3& normalOnBInWorld,const SimdVector3& pointInWorld,float depth);



};

#endif //MANIFOLD_RESULT_H
