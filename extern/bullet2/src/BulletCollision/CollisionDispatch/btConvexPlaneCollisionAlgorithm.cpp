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

#include "btConvexPlaneCollisionAlgorithm.h"

#include "BulletCollision/CollisionDispatch/btCollisionDispatcher.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionShapes/btConvexShape.h"
#include "BulletCollision/CollisionShapes/btStaticPlaneShape.h"

//#include <stdio.h>

btConvexPlaneCollisionAlgorithm::btConvexPlaneCollisionAlgorithm(btPersistentManifold* mf,const btCollisionAlgorithmConstructionInfo& ci,btCollisionObject* col0,btCollisionObject* col1, bool isSwapped)
: btCollisionAlgorithm(ci),
m_ownManifold(false),
m_manifoldPtr(mf),
m_isSwapped(isSwapped)
{
	btCollisionObject* convexObj = m_isSwapped? col1 : col0;
	btCollisionObject* planeObj = m_isSwapped? col0 : col1;
	
	if (!m_manifoldPtr && m_dispatcher->needsCollision(convexObj,planeObj))
	{
		m_manifoldPtr = m_dispatcher->getNewManifold(convexObj,planeObj);
		m_ownManifold = true;
	}
}


btConvexPlaneCollisionAlgorithm::~btConvexPlaneCollisionAlgorithm()
{
	if (m_ownManifold)
	{
		if (m_manifoldPtr)
			m_dispatcher->releaseManifold(m_manifoldPtr);
	}
}



void btConvexPlaneCollisionAlgorithm::processCollision (btCollisionObject* body0,btCollisionObject* body1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut)
{
	(void)dispatchInfo;
	(void)resultOut;
	if (!m_manifoldPtr)
		return;

	btCollisionObject* convexObj = m_isSwapped? body1 : body0;
	btCollisionObject* planeObj = m_isSwapped? body0: body1;

	btConvexShape* convexShape = (btConvexShape*) convexObj->getCollisionShape();
	btStaticPlaneShape* planeShape = (btStaticPlaneShape*) planeObj->getCollisionShape();

	bool hasCollision = false;
	const btVector3& planeNormal = planeShape->getPlaneNormal();
	const btScalar& planeConstant = planeShape->getPlaneConstant();
	btTransform planeInConvex;
	planeInConvex= convexObj->getWorldTransform().inverse() * planeObj->getWorldTransform();
	btTransform convexInPlaneTrans;
	convexInPlaneTrans= planeObj->getWorldTransform().inverse() * convexObj->getWorldTransform();

	btVector3 vtx = convexShape->localGetSupportingVertex(planeInConvex.getBasis()*-planeNormal);
	btVector3 vtxInPlane = convexInPlaneTrans(vtx);
	btScalar distance = (planeNormal.dot(vtxInPlane) - planeConstant);

	btVector3 vtxInPlaneProjected = vtxInPlane - distance*planeNormal;
	btVector3 vtxInPlaneWorld = planeObj->getWorldTransform() * vtxInPlaneProjected;

	hasCollision = distance < m_manifoldPtr->getContactBreakingThreshold();
	resultOut->setPersistentManifold(m_manifoldPtr);
	if (hasCollision)
	{
		/// report a contact. internally this will be kept persistent, and contact reduction is done
		btVector3 normalOnSurfaceB = planeObj->getWorldTransform().getBasis() * planeNormal;
		btVector3 pOnB = vtxInPlaneWorld;
		resultOut->addContactPoint(normalOnSurfaceB,pOnB,distance);
	}
	if (m_ownManifold)
	{
		if (m_manifoldPtr->getNumContacts())
		{
			resultOut->refreshContactPoints();
		}
	}
}

btScalar btConvexPlaneCollisionAlgorithm::calculateTimeOfImpact(btCollisionObject* col0,btCollisionObject* col1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut)
{
	(void)resultOut;
	(void)dispatchInfo;
	(void)col0;
	(void)col1;

	//not yet
	return btScalar(1.);
}
