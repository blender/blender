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


#include "btSphereTriangleCollisionAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btCollisionDispatcher.h"
#include "BulletCollision/CollisionShapes/btSphereShape.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "SphereTriangleDetector.h"


btSphereTriangleCollisionAlgorithm::btSphereTriangleCollisionAlgorithm(btPersistentManifold* mf,const btCollisionAlgorithmConstructionInfo& ci,btCollisionObject* col0,btCollisionObject* col1,bool swapped)
: btCollisionAlgorithm(ci),
m_ownManifold(false),
m_manifoldPtr(mf),
m_swapped(swapped)
{
	if (!m_manifoldPtr)
	{
		m_manifoldPtr = m_dispatcher->getNewManifold(col0,col1);
		m_ownManifold = true;
	}
}

btSphereTriangleCollisionAlgorithm::~btSphereTriangleCollisionAlgorithm()
{
	if (m_ownManifold)
	{
		if (m_manifoldPtr)
			m_dispatcher->releaseManifold(m_manifoldPtr);
	}
}

void btSphereTriangleCollisionAlgorithm::processCollision (btCollisionObject* col0,btCollisionObject* col1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut)
{
	if (!m_manifoldPtr)
		return;

	btSphereShape* sphere = (btSphereShape*)col0->getCollisionShape();
	btTriangleShape* triangle = (btTriangleShape*)col1->getCollisionShape();
	
	/// report a contact. internally this will be kept persistent, and contact reduction is done
	resultOut->setPersistentManifold(m_manifoldPtr);
	SphereTriangleDetector detector(sphere,triangle);
	
	btDiscreteCollisionDetectorInterface::ClosestPointInput input;
	input.m_maximumDistanceSquared = btScalar(1e30);//todo: tighter bounds
	input.m_transformA = col0->getWorldTransform();
	input.m_transformB = col1->getWorldTransform();

	detector.getClosestPoints(input,*resultOut,dispatchInfo.m_debugDraw);

}

btScalar btSphereTriangleCollisionAlgorithm::calculateTimeOfImpact(btCollisionObject* col0,btCollisionObject* col1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut)
{
	(void)resultOut;
	(void)dispatchInfo;
	(void)col0;
	(void)col1;

	//not yet
	return btScalar(1.);
}
