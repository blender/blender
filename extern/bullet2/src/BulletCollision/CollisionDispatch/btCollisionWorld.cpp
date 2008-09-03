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

#include "btCollisionWorld.h"
#include "btCollisionDispatcher.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "BulletCollision/CollisionShapes/btConvexShape.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.h"
#include "BulletCollision/CollisionShapes/btSphereShape.h" //for raycasting
#include "BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h" //for raycasting
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "BulletCollision/CollisionShapes/btCompoundShape.h"
#include "BulletCollision/NarrowPhaseCollision/btSubSimplexConvexCast.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkConvexCast.h"
#include "BulletCollision/NarrowPhaseCollision/btContinuousConvexCollision.h"

#include "BulletCollision/BroadphaseCollision/btBroadphaseInterface.h"
#include "LinearMath/btAabbUtil2.h"
#include "LinearMath/btQuickprof.h"
#include "LinearMath/btStackAlloc.h"


//When the user doesn't provide dispatcher or broadphase, create basic versions (and delete them in destructor)
#include "BulletCollision/CollisionDispatch/btCollisionDispatcher.h"
#include "BulletCollision/BroadphaseCollision/btSimpleBroadphase.h"
#include "BulletCollision/CollisionDispatch/btCollisionConfiguration.h"


btCollisionWorld::btCollisionWorld(btDispatcher* dispatcher,btBroadphaseInterface* pairCache, btCollisionConfiguration* collisionConfiguration)
:m_dispatcher1(dispatcher),
m_broadphasePairCache(pairCache),
m_debugDrawer(0)
{
	m_stackAlloc = collisionConfiguration->getStackAllocator();
	m_dispatchInfo.m_stackAllocator = m_stackAlloc;
}


btCollisionWorld::~btCollisionWorld()
{

	//clean up remaining objects
	int i;
	for (i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* collisionObject= m_collisionObjects[i];

		btBroadphaseProxy* bp = collisionObject->getBroadphaseHandle();
		if (bp)
		{
			//
			// only clear the cached algorithms
			//
			getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(bp,m_dispatcher1);
			getBroadphase()->destroyProxy(bp,m_dispatcher1);
		}
	}


}










void	btCollisionWorld::addCollisionObject(btCollisionObject* collisionObject,short int collisionFilterGroup,short int collisionFilterMask)
{

	//check that the object isn't already added
		btAssert( m_collisionObjects.findLinearSearch(collisionObject)  == m_collisionObjects.size());

		m_collisionObjects.push_back(collisionObject);

		//calculate new AABB
		btTransform trans = collisionObject->getWorldTransform();

		btVector3	minAabb;
		btVector3	maxAabb;
		collisionObject->getCollisionShape()->getAabb(trans,minAabb,maxAabb);

		int type = collisionObject->getCollisionShape()->getShapeType();
		collisionObject->setBroadphaseHandle( getBroadphase()->createProxy(
			minAabb,
			maxAabb,
			type,
			collisionObject,
			collisionFilterGroup,
			collisionFilterMask,
			m_dispatcher1,0
			))	;





}

void	btCollisionWorld::updateAabbs()
{
	BT_PROFILE("updateAabbs");

	btTransform predictedTrans;
	for ( int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];

		//only update aabb of active objects
		if (colObj->isActive())
		{
			btPoint3 minAabb,maxAabb;
			colObj->getCollisionShape()->getAabb(colObj->getWorldTransform(), minAabb,maxAabb);
			btBroadphaseInterface* bp = (btBroadphaseInterface*)m_broadphasePairCache;

			//moving objects should be moderately sized, probably something wrong if not
			if ( colObj->isStaticObject() || ((maxAabb-minAabb).length2() < btScalar(1e12)))
			{
				bp->setAabb(colObj->getBroadphaseHandle(),minAabb,maxAabb, m_dispatcher1);
			} else
			{
				//something went wrong, investigate
				//this assert is unwanted in 3D modelers (danger of loosing work)
				colObj->setActivationState(DISABLE_SIMULATION);

				static bool reportMe = true;
				if (reportMe && m_debugDrawer)
				{
					reportMe = false;
					m_debugDrawer->reportErrorWarning("Overflow in AABB, object removed from simulation");
					m_debugDrawer->reportErrorWarning("If you can reproduce this, please email bugs@continuousphysics.com\n");
					m_debugDrawer->reportErrorWarning("Please include above information, your Platform, version of OS.\n");
					m_debugDrawer->reportErrorWarning("Thanks.\n");
				}
			}
		}
	}

}



void	btCollisionWorld::performDiscreteCollisionDetection()
{
	BT_PROFILE("performDiscreteCollisionDetection");

	btDispatcherInfo& dispatchInfo = getDispatchInfo();

	updateAabbs();

	{
		BT_PROFILE("calculateOverlappingPairs");
		m_broadphasePairCache->calculateOverlappingPairs(m_dispatcher1);
	}


	btDispatcher* dispatcher = getDispatcher();
	{
		BT_PROFILE("dispatchAllCollisionPairs");
		if (dispatcher)
			dispatcher->dispatchAllCollisionPairs(m_broadphasePairCache->getOverlappingPairCache(),dispatchInfo,m_dispatcher1);
	}

}



void	btCollisionWorld::removeCollisionObject(btCollisionObject* collisionObject)
{


	//bool removeFromBroadphase = false;

	{

		btBroadphaseProxy* bp = collisionObject->getBroadphaseHandle();
		if (bp)
		{
			//
			// only clear the cached algorithms
			//
			getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(bp,m_dispatcher1);
			getBroadphase()->destroyProxy(bp,m_dispatcher1);
			collisionObject->setBroadphaseHandle(0);
		}
	}


	//swapremove
	m_collisionObjects.remove(collisionObject);

}



void	btCollisionWorld::rayTestSingle(const btTransform& rayFromTrans,const btTransform& rayToTrans,
					  btCollisionObject* collisionObject,
					  const btCollisionShape* collisionShape,
					  const btTransform& colObjWorldTransform,
					  RayResultCallback& resultCallback)
{
	btSphereShape pointShape(btScalar(0.0));
	pointShape.setMargin(0.f);
	const btConvexShape* castShape = &pointShape;

	if (collisionShape->isConvex())
	{
		btConvexCast::CastResult castResult;
		castResult.m_fraction = resultCallback.m_closestHitFraction;

		btConvexShape* convexShape = (btConvexShape*) collisionShape;
		btVoronoiSimplexSolver	simplexSolver;
#define USE_SUBSIMPLEX_CONVEX_CAST 1
#ifdef USE_SUBSIMPLEX_CONVEX_CAST
		btSubsimplexConvexCast convexCaster(castShape,convexShape,&simplexSolver);
#else
		//btGjkConvexCast	convexCaster(castShape,convexShape,&simplexSolver);
		//btContinuousConvexCollision convexCaster(castShape,convexShape,&simplexSolver,0);
#endif //#USE_SUBSIMPLEX_CONVEX_CAST

		if (convexCaster.calcTimeOfImpact(rayFromTrans,rayToTrans,colObjWorldTransform,colObjWorldTransform,castResult))
		{
			//add hit
			if (castResult.m_normal.length2() > btScalar(0.0001))
			{
				if (castResult.m_fraction < resultCallback.m_closestHitFraction)
				{
#ifdef USE_SUBSIMPLEX_CONVEX_CAST
					//rotate normal into worldspace
					castResult.m_normal = rayFromTrans.getBasis() * castResult.m_normal;
#endif //USE_SUBSIMPLEX_CONVEX_CAST

					castResult.m_normal.normalize();
					btCollisionWorld::LocalRayResult localRayResult
						(
							collisionObject,
							0,
							castResult.m_normal,
							castResult.m_fraction
						);

					bool normalInWorldSpace = true;
					resultCallback.addSingleResult(localRayResult, normalInWorldSpace);

				}
			}
		}
	} else {
		if (collisionShape->isConcave())
		{
			if (collisionShape->getShapeType()==TRIANGLE_MESH_SHAPE_PROXYTYPE)
			{
				///optimized version for btBvhTriangleMeshShape
				btBvhTriangleMeshShape* triangleMesh = (btBvhTriangleMeshShape*)collisionShape;
				btTransform worldTocollisionObject = colObjWorldTransform.inverse();
				btVector3 rayFromLocal = worldTocollisionObject * rayFromTrans.getOrigin();
				btVector3 rayToLocal = worldTocollisionObject * rayToTrans.getOrigin();

				//ConvexCast::CastResult
				struct BridgeTriangleRaycastCallback : public btTriangleRaycastCallback
				{
					btCollisionWorld::RayResultCallback* m_resultCallback;
					btCollisionObject*	m_collisionObject;
					btTriangleMeshShape*	m_triangleMesh;

					BridgeTriangleRaycastCallback( const btVector3& from,const btVector3& to,
						btCollisionWorld::RayResultCallback* resultCallback, btCollisionObject* collisionObject,btTriangleMeshShape*	triangleMesh):
						btTriangleRaycastCallback(from,to),
							m_resultCallback(resultCallback),
							m_collisionObject(collisionObject),
							m_triangleMesh(triangleMesh)
						{
						}


					virtual btScalar reportHit(const btVector3& hitNormalLocal, btScalar hitFraction, int partId, int triangleIndex )
					{
						btCollisionWorld::LocalShapeInfo	shapeInfo;
						shapeInfo.m_shapePart = partId;
						shapeInfo.m_triangleIndex = triangleIndex;

						btCollisionWorld::LocalRayResult rayResult
						(m_collisionObject,
							&shapeInfo,
							hitNormalLocal,
							hitFraction);

						bool	normalInWorldSpace = false;
						return m_resultCallback->addSingleResult(rayResult,normalInWorldSpace);
					}

				};

				BridgeTriangleRaycastCallback rcb(rayFromLocal,rayToLocal,&resultCallback,collisionObject,triangleMesh);
				rcb.m_hitFraction = resultCallback.m_closestHitFraction;
				triangleMesh->performRaycast(&rcb,rayFromLocal,rayToLocal);
			} else
			{
				btTriangleMeshShape* triangleMesh = (btTriangleMeshShape*)collisionShape;

				btTransform worldTocollisionObject = colObjWorldTransform.inverse();

				btVector3 rayFromLocal = worldTocollisionObject * rayFromTrans.getOrigin();
				btVector3 rayToLocal = worldTocollisionObject * rayToTrans.getOrigin();

				//ConvexCast::CastResult

				struct BridgeTriangleRaycastCallback : public btTriangleRaycastCallback
				{
					btCollisionWorld::RayResultCallback* m_resultCallback;
					btCollisionObject*	m_collisionObject;
					btTriangleMeshShape*	m_triangleMesh;

					BridgeTriangleRaycastCallback( const btVector3& from,const btVector3& to,
						btCollisionWorld::RayResultCallback* resultCallback, btCollisionObject* collisionObject,btTriangleMeshShape*	triangleMesh):
						btTriangleRaycastCallback(from,to),
							m_resultCallback(resultCallback),
							m_collisionObject(collisionObject),
							m_triangleMesh(triangleMesh)
						{
						}


					virtual btScalar reportHit(const btVector3& hitNormalLocal, btScalar hitFraction, int partId, int triangleIndex )
					{
						btCollisionWorld::LocalShapeInfo	shapeInfo;
						shapeInfo.m_shapePart = partId;
						shapeInfo.m_triangleIndex = triangleIndex;

						btCollisionWorld::LocalRayResult rayResult
						(m_collisionObject,
							&shapeInfo,
							hitNormalLocal,
							hitFraction);

						bool	normalInWorldSpace = false;
						return m_resultCallback->addSingleResult(rayResult,normalInWorldSpace);


					}

				};


				BridgeTriangleRaycastCallback	rcb(rayFromLocal,rayToLocal,&resultCallback,collisionObject,triangleMesh);
				rcb.m_hitFraction = resultCallback.m_closestHitFraction;

				btVector3 rayAabbMinLocal = rayFromLocal;
				rayAabbMinLocal.setMin(rayToLocal);
				btVector3 rayAabbMaxLocal = rayFromLocal;
				rayAabbMaxLocal.setMax(rayToLocal);

				triangleMesh->processAllTriangles(&rcb,rayAabbMinLocal,rayAabbMaxLocal);
			}
		} else {
			//todo: use AABB tree or other BVH acceleration structure!
			if (collisionShape->isCompound())
			{
				const btCompoundShape* compoundShape = static_cast<const btCompoundShape*>(collisionShape);
				int i=0;
				for (i=0;i<compoundShape->getNumChildShapes();i++)
				{
					btTransform childTrans = compoundShape->getChildTransform(i);
					const btCollisionShape* childCollisionShape = compoundShape->getChildShape(i);
					btTransform childWorldTrans = colObjWorldTransform * childTrans;
					rayTestSingle(rayFromTrans,rayToTrans,
						collisionObject,
						childCollisionShape,
						childWorldTrans,
						resultCallback);

				}
			}
		}
	}
}

void	btCollisionWorld::objectQuerySingle(const btConvexShape* castShape,const btTransform& convexFromTrans,const btTransform& convexToTrans,
					  btCollisionObject* collisionObject,
					  const btCollisionShape* collisionShape,
					  const btTransform& colObjWorldTransform,
					  ConvexResultCallback& resultCallback, btScalar allowedPenetration)
{
	if (collisionShape->isConvex())
	{
		btConvexCast::CastResult castResult;
		castResult.m_allowedPenetration = allowedPenetration;
		castResult.m_fraction = btScalar(1.);//??

		btConvexShape* convexShape = (btConvexShape*) collisionShape;
		btVoronoiSimplexSolver	simplexSolver;
		btGjkEpaPenetrationDepthSolver	gjkEpaPenetrationSolver;
		
		btContinuousConvexCollision convexCaster1(castShape,convexShape,&simplexSolver,&gjkEpaPenetrationSolver);
		//btGjkConvexCast convexCaster2(castShape,convexShape,&simplexSolver);
		//btSubsimplexConvexCast convexCaster3(castShape,convexShape,&simplexSolver);

		btConvexCast* castPtr = &convexCaster1;
	
	
		
		if (castPtr->calcTimeOfImpact(convexFromTrans,convexToTrans,colObjWorldTransform,colObjWorldTransform,castResult))
		{
			//add hit
			if (castResult.m_normal.length2() > btScalar(0.0001))
			{
				if (castResult.m_fraction < resultCallback.m_closestHitFraction)
				{
					castResult.m_normal.normalize();
					btCollisionWorld::LocalConvexResult localConvexResult
								(
									collisionObject,
									0,
									castResult.m_normal,
									castResult.m_hitPoint,
									castResult.m_fraction
								);

					bool normalInWorldSpace = true;
					resultCallback.addSingleResult(localConvexResult, normalInWorldSpace);

				}
			}
		}
	} else {
		if (collisionShape->isConcave())
		{
			if (collisionShape->getShapeType()==TRIANGLE_MESH_SHAPE_PROXYTYPE)
			{
				btBvhTriangleMeshShape* triangleMesh = (btBvhTriangleMeshShape*)collisionShape;
				btTransform worldTocollisionObject = colObjWorldTransform.inverse();
				btVector3 convexFromLocal = worldTocollisionObject * convexFromTrans.getOrigin();
				btVector3 convexToLocal = worldTocollisionObject * convexToTrans.getOrigin();
				// rotation of box in local mesh space = MeshRotation^-1 * ConvexToRotation
				btTransform rotationXform = btTransform(worldTocollisionObject.getBasis() * convexToTrans.getBasis());

				//ConvexCast::CastResult
				struct BridgeTriangleConvexcastCallback : public btTriangleConvexcastCallback
				{
					btCollisionWorld::ConvexResultCallback* m_resultCallback;
					btCollisionObject*	m_collisionObject;
					btTriangleMeshShape*	m_triangleMesh;

					BridgeTriangleConvexcastCallback(const btConvexShape* castShape, const btTransform& from,const btTransform& to,
						btCollisionWorld::ConvexResultCallback* resultCallback, btCollisionObject* collisionObject,btTriangleMeshShape*	triangleMesh, const btTransform& triangleToWorld):
						btTriangleConvexcastCallback(castShape, from,to, triangleToWorld, triangleMesh->getMargin()),
							m_resultCallback(resultCallback),
							m_collisionObject(collisionObject),
							m_triangleMesh(triangleMesh)
						{
						}


					virtual btScalar reportHit(const btVector3& hitNormalLocal, const btVector3& hitPointLocal, btScalar hitFraction, int partId, int triangleIndex )
					{
						btCollisionWorld::LocalShapeInfo	shapeInfo;
						shapeInfo.m_shapePart = partId;
						shapeInfo.m_triangleIndex = triangleIndex;
						if (hitFraction <= m_resultCallback->m_closestHitFraction)
						{

							btCollisionWorld::LocalConvexResult convexResult
							(m_collisionObject,
								&shapeInfo,
								hitNormalLocal,
								hitPointLocal,
								hitFraction);

							bool	normalInWorldSpace = true;


							return m_resultCallback->addSingleResult(convexResult,normalInWorldSpace);
						}
						return hitFraction;
					}

				};

				BridgeTriangleConvexcastCallback tccb(castShape, convexFromTrans,convexToTrans,&resultCallback,collisionObject,triangleMesh, colObjWorldTransform);
				tccb.m_hitFraction = resultCallback.m_closestHitFraction;
				btVector3 boxMinLocal, boxMaxLocal;
				castShape->getAabb(rotationXform, boxMinLocal, boxMaxLocal);
				triangleMesh->performConvexcast(&tccb,convexFromLocal,convexToLocal,boxMinLocal, boxMaxLocal);
			} else
			{
				btBvhTriangleMeshShape* triangleMesh = (btBvhTriangleMeshShape*)collisionShape;
				btTransform worldTocollisionObject = colObjWorldTransform.inverse();
				btVector3 convexFromLocal = worldTocollisionObject * convexFromTrans.getOrigin();
				btVector3 convexToLocal = worldTocollisionObject * convexToTrans.getOrigin();
				// rotation of box in local mesh space = MeshRotation^-1 * ConvexToRotation
				btTransform rotationXform = btTransform(worldTocollisionObject.getBasis() * convexToTrans.getBasis());

				//ConvexCast::CastResult
				struct BridgeTriangleConvexcastCallback : public btTriangleConvexcastCallback
				{
					btCollisionWorld::ConvexResultCallback* m_resultCallback;
					btCollisionObject*	m_collisionObject;
					btTriangleMeshShape*	m_triangleMesh;

					BridgeTriangleConvexcastCallback(const btConvexShape* castShape, const btTransform& from,const btTransform& to,
						btCollisionWorld::ConvexResultCallback* resultCallback, btCollisionObject* collisionObject,btTriangleMeshShape*	triangleMesh, const btTransform& triangleToWorld):
						btTriangleConvexcastCallback(castShape, from,to, triangleToWorld, triangleMesh->getMargin()),
							m_resultCallback(resultCallback),
							m_collisionObject(collisionObject),
							m_triangleMesh(triangleMesh)
						{
						}


					virtual btScalar reportHit(const btVector3& hitNormalLocal, const btVector3& hitPointLocal, btScalar hitFraction, int partId, int triangleIndex )
					{
						btCollisionWorld::LocalShapeInfo	shapeInfo;
						shapeInfo.m_shapePart = partId;
						shapeInfo.m_triangleIndex = triangleIndex;
						if (hitFraction <= m_resultCallback->m_closestHitFraction)
						{

							btCollisionWorld::LocalConvexResult convexResult
							(m_collisionObject,
								&shapeInfo,
								hitNormalLocal,
								hitPointLocal,
								hitFraction);

							bool	normalInWorldSpace = false;

							return m_resultCallback->addSingleResult(convexResult,normalInWorldSpace);
						}
						return hitFraction;
					}

				};

				BridgeTriangleConvexcastCallback tccb(castShape, convexFromTrans,convexToTrans,&resultCallback,collisionObject,triangleMesh, colObjWorldTransform);
				tccb.m_hitFraction = resultCallback.m_closestHitFraction;
				btVector3 boxMinLocal, boxMaxLocal;
				castShape->getAabb(rotationXform, boxMinLocal, boxMaxLocal);

				btVector3 rayAabbMinLocal = convexFromLocal;
				rayAabbMinLocal.setMin(convexToLocal);
				btVector3 rayAabbMaxLocal = convexFromLocal;
				rayAabbMaxLocal.setMax(convexToLocal);
				rayAabbMinLocal += boxMinLocal;
				rayAabbMaxLocal += boxMaxLocal;
				triangleMesh->processAllTriangles(&tccb,rayAabbMinLocal,rayAabbMaxLocal);
			}
		} else {
			//todo: use AABB tree or other BVH acceleration structure!
			if (collisionShape->isCompound())
			{
				const btCompoundShape* compoundShape = static_cast<const btCompoundShape*>(collisionShape);
				int i=0;
				for (i=0;i<compoundShape->getNumChildShapes();i++)
				{
					btTransform childTrans = compoundShape->getChildTransform(i);
					const btCollisionShape* childCollisionShape = compoundShape->getChildShape(i);
					btTransform childWorldTrans = colObjWorldTransform * childTrans;
					objectQuerySingle(castShape, convexFromTrans,convexToTrans,
						collisionObject,
						childCollisionShape,
						childWorldTrans,
						resultCallback, allowedPenetration);
				}
			}
		}
	}
}

void	btCollisionWorld::rayTest(const btVector3& rayFromWorld, const btVector3& rayToWorld, RayResultCallback& resultCallback) const
{


	btTransform	rayFromTrans,rayToTrans;
	rayFromTrans.setIdentity();
	rayFromTrans.setOrigin(rayFromWorld);
	rayToTrans.setIdentity();

	rayToTrans.setOrigin(rayToWorld);

	/// go over all objects, and if the ray intersects their aabb, do a ray-shape query using convexCaster (CCD)

	int i;
	for (i=0;i<m_collisionObjects.size();i++)
	{
		///terminate further ray tests, once the closestHitFraction reached zero
		if (resultCallback.m_closestHitFraction == btScalar(0.f))
			break;

		btCollisionObject*	collisionObject= m_collisionObjects[i];
		//only perform raycast if filterMask matches
		if(resultCallback.needsCollision(collisionObject->getBroadphaseHandle())) {
			//RigidcollisionObject* collisionObject = ctrl->GetRigidcollisionObject();
			btVector3 collisionObjectAabbMin,collisionObjectAabbMax;
			collisionObject->getCollisionShape()->getAabb(collisionObject->getWorldTransform(),collisionObjectAabbMin,collisionObjectAabbMax);

			btScalar hitLambda = resultCallback.m_closestHitFraction;
			btVector3 hitNormal;
			if (btRayAabb(rayFromWorld,rayToWorld,collisionObjectAabbMin,collisionObjectAabbMax,hitLambda,hitNormal))
			{
				rayTestSingle(rayFromTrans,rayToTrans,
					collisionObject,
						collisionObject->getCollisionShape(),
						collisionObject->getWorldTransform(),
						resultCallback);
			}
		}

	}

}

void	btCollisionWorld::convexSweepTest(const btConvexShape* castShape, const btTransform& convexFromWorld, const btTransform& convexToWorld, ConvexResultCallback& resultCallback) const
{
	btTransform	convexFromTrans,convexToTrans;
	convexFromTrans = convexFromWorld;
	convexToTrans = convexToWorld;
	btVector3 castShapeAabbMin, castShapeAabbMax;
	/* Compute AABB that encompasses angular movement */
	{
		btVector3 linVel, angVel;
		btTransformUtil::calculateVelocity (convexFromTrans, convexToTrans, 1.0, linVel, angVel);
		btTransform R;
		R.setIdentity ();
		R.setRotation (convexFromTrans.getRotation());
		castShape->calculateTemporalAabb (R, linVel, angVel, 1.0, castShapeAabbMin, castShapeAabbMax);
	}

	/// go over all objects, and if the ray intersects their aabb + cast shape aabb,
	// do a ray-shape query using convexCaster (CCD)
	int i;
	for (i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject*	collisionObject= m_collisionObjects[i];
		//only perform raycast if filterMask matches
		if(resultCallback.needsCollision(collisionObject->getBroadphaseHandle())) {
			//RigidcollisionObject* collisionObject = ctrl->GetRigidcollisionObject();
			btVector3 collisionObjectAabbMin,collisionObjectAabbMax;
			collisionObject->getCollisionShape()->getAabb(collisionObject->getWorldTransform(),collisionObjectAabbMin,collisionObjectAabbMax);
			AabbExpand (collisionObjectAabbMin, collisionObjectAabbMax, castShapeAabbMin, castShapeAabbMax);
			btScalar hitLambda = btScalar(1.); //could use resultCallback.m_closestHitFraction, but needs testing
			btVector3 hitNormal;
			if (btRayAabb(convexFromWorld.getOrigin(),convexToWorld.getOrigin(),collisionObjectAabbMin,collisionObjectAabbMax,hitLambda,hitNormal))
			{
				objectQuerySingle(castShape, convexFromTrans,convexToTrans,
					collisionObject,
						collisionObject->getCollisionShape(),
						collisionObject->getWorldTransform(),
						resultCallback,
						getDispatchInfo().m_allowedCcdPenetration);
			}
		}
	}

}
