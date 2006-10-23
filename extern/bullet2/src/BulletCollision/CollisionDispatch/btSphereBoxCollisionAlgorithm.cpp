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

#include "btSphereBoxCollisionAlgorithm.h"
#include "BulletCollision/CollisionDispatch/btCollisionDispatcher.h"
#include "BulletCollision/CollisionShapes/btSphereShape.h"
#include "BulletCollision/CollisionShapes/btBoxShape.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
//#include <stdio.h>

btSphereBoxCollisionAlgorithm::btSphereBoxCollisionAlgorithm(btPersistentManifold* mf,const btCollisionAlgorithmConstructionInfo& ci,btCollisionObject* col0,btCollisionObject* col1, bool isSwapped)
: btCollisionAlgorithm(ci),
m_ownManifold(false),
m_manifoldPtr(mf),
m_isSwapped(isSwapped)
{
	btCollisionObject* sphereObj = m_isSwapped? col1 : col0;
	btCollisionObject* boxObj = m_isSwapped? col0 : col1;
	
	if (!m_manifoldPtr && m_dispatcher->needsCollision(sphereObj,boxObj))
	{
		m_manifoldPtr = m_dispatcher->getNewManifold(sphereObj,boxObj);
		m_ownManifold = true;
	}
}


btSphereBoxCollisionAlgorithm::~btSphereBoxCollisionAlgorithm()
{
	if (m_ownManifold)
	{
		if (m_manifoldPtr)
			m_dispatcher->releaseManifold(m_manifoldPtr);
	}
}



void btSphereBoxCollisionAlgorithm::processCollision (btCollisionObject* body0,btCollisionObject* body1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut)
{
	if (!m_manifoldPtr)
		return;

	btCollisionObject* sphereObj = m_isSwapped? body1 : body0;
	btCollisionObject* boxObj = m_isSwapped? body0 : body1;


	btSphereShape* sphere0 = (btSphereShape*)sphereObj ->m_collisionShape;

	btVector3 normalOnSurfaceB;
	btVector3 pOnBox,pOnSphere;
	btVector3 sphereCenter = sphereObj->m_worldTransform.getOrigin();
	btScalar radius = sphere0->getRadius();
	
	float dist = getSphereDistance(boxObj,pOnBox,pOnSphere,sphereCenter,radius);

	if (dist < SIMD_EPSILON)
	{
		btVector3 normalOnSurfaceB = (pOnBox- pOnSphere).normalize();

		/// report a contact. internally this will be kept persistent, and contact reduction is done

		resultOut->setPersistentManifold(m_manifoldPtr);
		resultOut->addContactPoint(normalOnSurfaceB,pOnBox,dist);
		
	}

	

}

float btSphereBoxCollisionAlgorithm::calculateTimeOfImpact(btCollisionObject* col0,btCollisionObject* col1,const btDispatcherInfo& dispatchInfo,btManifoldResult* resultOut)
{
	//not yet
	return 1.f;
}


btScalar btSphereBoxCollisionAlgorithm::getSphereDistance(btCollisionObject* boxObj, btVector3& pointOnBox, btVector3& v3PointOnSphere, const btVector3& sphereCenter, btScalar fRadius ) 
{

	btScalar margins;
	btVector3 bounds[2];
	btBoxShape* boxShape= (btBoxShape*)boxObj->m_collisionShape;
	
	bounds[0] = -boxShape->getHalfExtents();
	bounds[1] = boxShape->getHalfExtents();

	margins = boxShape->getMargin();//also add sphereShape margin?

	const btTransform&	m44T = boxObj->m_worldTransform;

	btVector3	boundsVec[2];
	btScalar	fPenetration;

	boundsVec[0] = bounds[0];
	boundsVec[1] = bounds[1];

	btVector3	marginsVec( margins, margins, margins );

	// add margins
	bounds[0] += marginsVec;
	bounds[1] -= marginsVec;

	/////////////////////////////////////////////////

	btVector3	tmp, prel, n[6], normal, v3P;
	btScalar   fSep = 10000000.0f, fSepThis;

	n[0].setValue( -1.0f,  0.0f,  0.0f );
	n[1].setValue(  0.0f, -1.0f,  0.0f );
	n[2].setValue(  0.0f,  0.0f, -1.0f );
	n[3].setValue(  1.0f,  0.0f,  0.0f );
	n[4].setValue(  0.0f,  1.0f,  0.0f );
	n[5].setValue(  0.0f,  0.0f,  1.0f );

	// convert  point in local space
	prel = m44T.invXform( sphereCenter);
	
	bool	bFound = false;

	v3P = prel;

	for (int i=0;i<6;i++)
	{
		int j = i<3? 0:1;
		if ( (fSepThis = ((v3P-bounds[j]) .dot(n[i]))) > 0.0f )
		{
			v3P = v3P - n[i]*fSepThis;		
			bFound = true;
		}
	}
	
	//

	if ( bFound )
	{
		bounds[0] = boundsVec[0];
		bounds[1] = boundsVec[1];

		normal = (prel - v3P).normalize();
		pointOnBox = v3P + normal*margins;
		v3PointOnSphere = prel - normal*fRadius;

		if ( ((v3PointOnSphere - pointOnBox) .dot (normal)) > 0.0f )
		{
			return 1.0f;
		}

		// transform back in world space
		tmp = m44T( pointOnBox);
		pointOnBox    = tmp;
		tmp  = m44T( v3PointOnSphere);		
		v3PointOnSphere = tmp;
		btScalar fSeps2 = (pointOnBox-v3PointOnSphere).length2();
		
		//if this fails, fallback into deeper penetration case, below
		if (fSeps2 > SIMD_EPSILON)
		{
			fSep = - btSqrt(fSeps2);
			normal = (pointOnBox-v3PointOnSphere);
			normal *= 1.f/fSep;
		}

		return fSep;
	}

	//////////////////////////////////////////////////
	// Deep penetration case

	fPenetration = getSpherePenetration( boxObj,pointOnBox, v3PointOnSphere, sphereCenter, fRadius,bounds[0],bounds[1] );

	bounds[0] = boundsVec[0];
	bounds[1] = boundsVec[1];

	if ( fPenetration <= 0.0f )
		return (fPenetration-margins);
	else
		return 1.0f;
}

btScalar btSphereBoxCollisionAlgorithm::getSpherePenetration( btCollisionObject* boxObj,btVector3& pointOnBox, btVector3& v3PointOnSphere, const btVector3& sphereCenter, btScalar fRadius, const btVector3& aabbMin, const btVector3& aabbMax) 
{

	btVector3 bounds[2];

	bounds[0] = aabbMin;
	bounds[1] = aabbMax;

	btVector3	p0, tmp, prel, n[6], normal;
	btScalar   fSep = -10000000.0f, fSepThis;

	n[0].setValue( -1.0f,  0.0f,  0.0f );
	n[1].setValue(  0.0f, -1.0f,  0.0f );
	n[2].setValue(  0.0f,  0.0f, -1.0f );
	n[3].setValue(  1.0f,  0.0f,  0.0f );
	n[4].setValue(  0.0f,  1.0f,  0.0f );
	n[5].setValue(  0.0f,  0.0f,  1.0f );

	const btTransform&	m44T = boxObj->m_worldTransform;

	// convert  point in local space
	prel = m44T.invXform( sphereCenter);

	///////////

	for (int i=0;i<6;i++)
	{
		int j = i<3 ? 0:1;
		if ( (fSepThis = ((prel-bounds[j]) .dot( n[i]))-fRadius) > 0.0f )	return 1.0f;
		if ( fSepThis > fSep )
		{
			p0 = bounds[j];	normal = (btVector3&)n[i];
			fSep = fSepThis;
		}
	}

	pointOnBox = prel - normal*(normal.dot((prel-p0)));
	v3PointOnSphere = pointOnBox + normal*fSep;

	// transform back in world space
	tmp  = m44T( pointOnBox);		
	pointOnBox    = tmp;
	tmp  = m44T( v3PointOnSphere);		v3PointOnSphere = tmp;
	normal = (pointOnBox-v3PointOnSphere).normalize();

	return fSep;

}

