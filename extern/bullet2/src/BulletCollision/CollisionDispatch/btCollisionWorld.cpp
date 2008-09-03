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

#include "BulletCollision/CollisionShapes/btSphereShape.h" //for raycasting
#include "BulletCollision/CollisionShapes/btTriangleMeshShape.h" //for raycasting
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "BulletCollision/CollisionShapes/btCompoundShape.h"
#include "BulletCollision/NarrowPhaseCollision/btSubSimplexConvexCast.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseInterface.h"
#include "LinearMath/btAabbUtil2.h"
#include "LinearMath/btQuickprof.h"
#include "LinearMath/btStackAlloc.h"

//When the user doesn't provide dispatcher or broadphase, create basic versions (and delete them in destructor)
#include "BulletCollision/CollisionDispatch/btCollisionDispatcher.h"
#include "BulletCollision/BroadphaseCollision/btSimpleBroadphase.h"


btCollisionWorld::btCollisionWorld(btDispatcher* dispatcher,btOverlappingPairCache* pairCache, int stackSize)
:m_dispatcher1(dispatcher),
m_broadphasePairCache(pairCache),
m_ownsDispatcher(false),
m_ownsBroadphasePairCache(false)
{
	m_stackAlloc = new btStackAlloc(stackSize);
	m_dispatchInfo.m_stackAllocator = m_stackAlloc;
}


btCollisionWorld::~btCollisionWorld()
{
	m_stackAlloc->destroy();
	delete m_stackAlloc;

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
			getBroadphase()->cleanProxyFromPairs(bp);
			getBroadphase()->destroyProxy(bp);
		}
	}

	if (m_ownsDispatcher)
		delete m_dispatcher1;
	if (m_ownsBroadphasePairCache)
		delete m_broadphasePairCache;

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
			collisionFilterMask
			))	;

		



}




void	btCollisionWorld::performDiscreteCollisionDetection()
{
	btDispatcherInfo& dispatchInfo = getDispatchInfo();

	BEGIN_PROFILE("perform Broadphase Collision Detection");


	//update aabb (of all moved objects)

	btVector3 aabbMin,aabbMax;
	for (int i=0;i<m_collisionObjects.size();i++)
	{
		m_collisionObjects[i]->getCollisionShape()->getAabb(m_collisionObjects[i]->getWorldTransform(),aabbMin,aabbMax);
		m_broadphasePairCache->setAabb(m_collisionObjects[i]->getBroadphaseHandle(),aabbMin,aabbMax);
	}

	m_broadphasePairCache->refreshOverlappingPairs();

	
	END_PROFILE("perform Broadphase Collision Detection");

	BEGIN_PROFILE("performDiscreteCollisionDetection");

	btDispatcher* dispatcher = getDispatcher();
	if (dispatcher)
		dispatcher->dispatchAllCollisionPairs(m_broadphasePairCache,dispatchInfo);

	END_PROFILE("performDiscreteCollisionDetection");

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
			getBroadphase()->cleanProxyFromPairs(bp);
			getBroadphase()->destroyProxy(bp);
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
					  RayResultCallback& resultCallback,
					  short int collisionFilterMask,
					  bool faceNormal)
{
	
	btSphereShape pointShape(btScalar(0.0));
	pointShape.setMargin(0.f);

	objectQuerySingle(&pointShape,rayFromTrans,rayToTrans,
					  collisionObject,
					  collisionShape,
					  colObjWorldTransform,
					  resultCallback,collisionFilterMask,faceNormal);
}

void	btCollisionWorld::objectQuerySingle(const btConvexShape* castShape,const btTransform& rayFromTrans,const btTransform& rayToTrans,
					  btCollisionObject* collisionObject,
					  const btCollisionShape* collisionShape,
					  const btTransform& colObjWorldTransform,
					  RayResultCallback& resultCallback,
					  short int collisionFilterMask,
					  bool faceNormal)
{
	

	if (collisionShape->isConvex())
			{
				btConvexCast::CastResult castResult;
				castResult.m_fraction = btScalar(1.);//??

				btConvexShape* convexShape = (btConvexShape*) collisionShape;
				btVoronoiSimplexSolver	simplexSolver;
				btSubsimplexConvexCast convexCaster(castShape,convexShape,&simplexSolver);
				//GjkConvexCast	convexCaster(castShape,convexShape,&simplexSolver);
				//ContinuousConvexCollision convexCaster(castShape,convexShape,&simplexSolver,0);
				
				if (convexCaster.calcTimeOfImpact(rayFromTrans,rayToTrans,colObjWorldTransform,colObjWorldTransform,castResult))
				{
					//add hit
					if (castResult.m_normal.length2() > btScalar(0.0001))
					{
						castResult.m_normal.normalize();
						if (castResult.m_fraction < resultCallback.m_closestHitFraction)
						{

							btCollisionWorld::LocalRayResult localRayResult
								(
									collisionObject, 
									0,
									castResult.m_normal,
									castResult.m_fraction
								);

							resultCallback.AddSingleResult(localRayResult);

						}
					}
				}
			}
			else
			{
				
				if (collisionShape->isConcave())
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

							BridgeTriangleRaycastCallback( const btVector3& from,const btVector3& to,bool faceNormal,
								btCollisionWorld::RayResultCallback* resultCallback, btCollisionObject* collisionObject,btTriangleMeshShape* triangleMesh):
								btTriangleRaycastCallback(from,to,faceNormal),
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
								shapeInfo.m_triangleShape = m_triangleMesh;
								
								btCollisionWorld::LocalRayResult rayResult
								(m_collisionObject, 
									&shapeInfo,
									hitNormalLocal,
									hitFraction);
								
								return m_resultCallback->AddSingleResult(rayResult);
								
								
							}
	
						};


						BridgeTriangleRaycastCallback	rcb(rayFromLocal,rayToLocal,faceNormal,&resultCallback,collisionObject,triangleMesh);
						rcb.m_hitFraction = resultCallback.m_closestHitFraction;

						btVector3 rayAabbMinLocal = rayFromLocal;
						rayAabbMinLocal.setMin(rayToLocal);
						btVector3 rayAabbMaxLocal = rayFromLocal;
						rayAabbMaxLocal.setMax(rayToLocal);

						triangleMesh->processAllTriangles(&rcb,rayAabbMinLocal,rayAabbMaxLocal);
											
					} else
					{
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
								objectQuerySingle(castShape, rayFromTrans,rayToTrans,
									collisionObject,
									childCollisionShape,
									childWorldTrans,
									resultCallback, collisionFilterMask, faceNormal);

							}


						}
					}
			}
}

void	btCollisionWorld::rayTest(const btVector3& rayFromWorld, const btVector3& rayToWorld, RayResultCallback& resultCallback,short int collisionFilterMask, bool faceNormal)
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
		btCollisionObject*	collisionObject= m_collisionObjects[i];
		//only perform raycast if filterMask matches
		if(collisionObject->getBroadphaseHandle()->m_collisionFilterGroup & collisionFilterMask) { 
			//RigidcollisionObject* collisionObject = ctrl->GetRigidcollisionObject();
			btVector3 collisionObjectAabbMin,collisionObjectAabbMax;
			collisionObject->getCollisionShape()->getAabb(collisionObject->getWorldTransform(),collisionObjectAabbMin,collisionObjectAabbMax);

			btScalar hitLambda = btScalar(1.); //could use resultCallback.m_closestHitFraction, but needs testing
			btVector3 hitNormal;
			if (btRayAabb(rayFromWorld,rayToWorld,collisionObjectAabbMin,collisionObjectAabbMax,hitLambda,hitNormal))
			{
				// before testing this object, verify that it is not filtered out
				if (resultCallback.NeedRayCast(collisionObject))
				{
					rayTestSingle(rayFromTrans,rayToTrans,
						collisionObject,
							collisionObject->getCollisionShape(),
							collisionObject->getWorldTransform(),
							resultCallback,
							collisionFilterMask,
							faceNormal);
				}
			}	
		}
	}

}
