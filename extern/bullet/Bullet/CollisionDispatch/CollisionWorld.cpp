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

#include "CollisionWorld.h"
#include "CollisionDispatcher.h"
#include "CollisionDispatch/CollisionObject.h"
#include "CollisionShapes/CollisionShape.h"
#include "CollisionShapes/SphereShape.h" //for raycasting
#include "CollisionShapes/TriangleMeshShape.h" //for raycasting
#include "NarrowPhaseCollision/RaycastCallback.h"
#include "CollisionShapes/CompoundShape.h"

#include "NarrowPhaseCollision/SubSimplexConvexCast.h"
#include "BroadphaseCollision/BroadphaseInterface.h"
#include "AabbUtil2.h"

#include <algorithm>

CollisionWorld::~CollisionWorld()
{
	//clean up remaining objects
	std::vector<CollisionObject*>::iterator i;

	int index = 0;
	for (i=m_collisionObjects.begin();
	!(i==m_collisionObjects.end()); i++)

	{
		CollisionObject* collisionObject= (*i);
		
		BroadphaseProxy* bp = collisionObject->m_broadphaseHandle;
		if (bp)
		{
			//
			// only clear the cached algorithms
			//
			GetBroadphase()->CleanProxyFromPairs(bp);
			GetBroadphase()->DestroyProxy(bp);
		}
	}

}










void	CollisionWorld::AddCollisionObject(CollisionObject* collisionObject,short int collisionFilterGroup,short int collisionFilterMask)
{
		m_collisionObjects.push_back(collisionObject);

		//calculate new AABB
		SimdTransform trans = collisionObject->m_worldTransform;

		SimdVector3	minAabb;
		SimdVector3	maxAabb;
		collisionObject->m_collisionShape->GetAabb(trans,minAabb,maxAabb);

		int type = collisionObject->m_collisionShape->GetShapeType();
		collisionObject->m_broadphaseHandle = GetBroadphase()->CreateProxy(
			minAabb,
			maxAabb,
			type,
			collisionObject,
			collisionFilterGroup,
			collisionFilterMask
			);
		



}

void	CollisionWorld::PerformDiscreteCollisionDetection()
{
	DispatcherInfo	dispatchInfo;
	dispatchInfo.m_timeStep = 0.f;
	dispatchInfo.m_stepCount = 0;

	//update aabb (of all moved objects)

	SimdVector3 aabbMin,aabbMax;
	for (size_t i=0;i<m_collisionObjects.size();i++)
	{
		m_collisionObjects[i]->m_collisionShape->GetAabb(m_collisionObjects[i]->m_worldTransform,aabbMin,aabbMax);
		m_pairCache->SetAabb(m_collisionObjects[i]->m_broadphaseHandle,aabbMin,aabbMax);
	}

	Dispatcher* dispatcher = GetDispatcher();
	if (dispatcher)
		dispatcher->DispatchAllCollisionPairs(&m_pairCache->GetOverlappingPair(0),m_pairCache->GetNumOverlappingPairs(),dispatchInfo);

}


void	CollisionWorld::RemoveCollisionObject(CollisionObject* collisionObject)
{
	
	
	//bool removeFromBroadphase = false;
	
	{
		
		BroadphaseProxy* bp = collisionObject->m_broadphaseHandle;
		if (bp)
		{
			//
			// only clear the cached algorithms
			//
			GetBroadphase()->CleanProxyFromPairs(bp);
			GetBroadphase()->DestroyProxy(bp);
			collisionObject->m_broadphaseHandle = 0;
		}
	}


	std::vector<CollisionObject*>::iterator i =	std::find(m_collisionObjects.begin(), m_collisionObjects.end(), collisionObject);
		
	if (!(i == m_collisionObjects.end()))
		{
			std::swap(*i, m_collisionObjects.back());
			m_collisionObjects.pop_back();
		}
}

void	RayTestSingle(const SimdTransform& rayFromTrans,const SimdTransform& rayToTrans,
					  CollisionObject* collisionObject,
					  const CollisionShape* collisionShape,
					  const SimdTransform& colObjWorldTransform,
					  CollisionWorld::RayResultCallback& resultCallback)
{
	
	SphereShape pointShape(0.0f);

	if (collisionShape->IsConvex())
			{
				ConvexCast::CastResult castResult;
				castResult.m_fraction = 1.f;//??

				ConvexShape* convexShape = (ConvexShape*) collisionShape;
				VoronoiSimplexSolver	simplexSolver;
				SubsimplexConvexCast convexCaster(&pointShape,convexShape,&simplexSolver);
				//GjkConvexCast	convexCaster(&pointShape,convexShape,&simplexSolver);
				//ContinuousConvexCollision convexCaster(&pointShape,convexShape,&simplexSolver,0);
				
				if (convexCaster.calcTimeOfImpact(rayFromTrans,rayToTrans,colObjWorldTransform,colObjWorldTransform,castResult))
				{
					//add hit
					if (castResult.m_normal.length2() > 0.0001f)
					{
						castResult.m_normal.normalize();
						if (castResult.m_fraction < resultCallback.m_closestHitFraction)
						{
							

							CollisionWorld::LocalRayResult localRayResult
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
				
				if (collisionShape->IsConcave())
					{

						TriangleMeshShape* triangleMesh = (TriangleMeshShape*)collisionShape;
						
						SimdTransform worldTocollisionObject = colObjWorldTransform.inverse();

						SimdVector3 rayFromLocal = worldTocollisionObject * rayFromTrans.getOrigin();
						SimdVector3 rayToLocal = worldTocollisionObject * rayToTrans.getOrigin();

						//ConvexCast::CastResult

						struct BridgeTriangleRaycastCallback : public TriangleRaycastCallback 
						{
							CollisionWorld::RayResultCallback* m_resultCallback;
							CollisionObject*	m_collisionObject;
							TriangleMeshShape*	m_triangleMesh;

							BridgeTriangleRaycastCallback( const SimdVector3& from,const SimdVector3& to,
								CollisionWorld::RayResultCallback* resultCallback, CollisionObject* collisionObject,TriangleMeshShape*	triangleMesh):
								TriangleRaycastCallback(from,to),
									m_resultCallback(resultCallback),
									m_collisionObject(collisionObject),
									m_triangleMesh(triangleMesh)
								{
								}


							virtual float ReportHit(const SimdVector3& hitNormalLocal, float hitFraction, int partId, int triangleIndex )
							{
								CollisionWorld::LocalShapeInfo	shapeInfo;
								shapeInfo.m_shapePart = partId;
								shapeInfo.m_triangleIndex = triangleIndex;
								
								CollisionWorld::LocalRayResult rayResult
								(m_collisionObject, 
									&shapeInfo,
									hitNormalLocal,
									hitFraction);
								
								return m_resultCallback->AddSingleResult(rayResult);
								
								
							}
	
						};


						BridgeTriangleRaycastCallback	rcb(rayFromLocal,rayToLocal,&resultCallback,collisionObject,triangleMesh);
						rcb.m_hitFraction = resultCallback.m_closestHitFraction;

						SimdVector3 rayAabbMinLocal = rayFromLocal;
						rayAabbMinLocal.setMin(rayToLocal);
						SimdVector3 rayAabbMaxLocal = rayFromLocal;
						rayAabbMaxLocal.setMax(rayToLocal);

						triangleMesh->ProcessAllTriangles(&rcb,rayAabbMinLocal,rayAabbMaxLocal);
											
					} else
					{
						//todo: use AABB tree or other BVH acceleration structure!
						if (collisionShape->IsCompound())
						{
							const CompoundShape* compoundShape = static_cast<const CompoundShape*>(collisionShape);
							int i=0;
							for (i=0;i<compoundShape->GetNumChildShapes();i++)
							{
								SimdTransform childTrans = compoundShape->GetChildTransform(i);
								const CollisionShape* childCollisionShape = compoundShape->GetChildShape(i);
								SimdTransform childWorldTrans = colObjWorldTransform * childTrans;
								RayTestSingle(rayFromTrans,rayToTrans,
									collisionObject,
									childCollisionShape,
									childWorldTrans,
									resultCallback);

							}


						}
					}
			}
}

void	CollisionWorld::RayTest(const SimdVector3& rayFromWorld, const SimdVector3& rayToWorld, RayResultCallback& resultCallback)
{

	
	SimdTransform	rayFromTrans,rayToTrans;
	rayFromTrans.setIdentity();
	rayFromTrans.setOrigin(rayFromWorld);
	rayToTrans.setIdentity();
	
	rayToTrans.setOrigin(rayToWorld);

	//do culling based on aabb (rayFrom/rayTo)
	SimdVector3 rayAabbMin = rayFromWorld;
	SimdVector3 rayAabbMax = rayFromWorld;
	rayAabbMin.setMin(rayToWorld);
	rayAabbMax.setMax(rayToWorld);


	/// brute force go over all objects. Once there is a broadphase, use that, or
	/// add a raycast against aabb first.
	
	std::vector<CollisionObject*>::iterator iter;
	
	for (iter=m_collisionObjects.begin();
	!(iter==m_collisionObjects.end()); iter++)
	{
		
		CollisionObject*	collisionObject= (*iter);

		//RigidcollisionObject* collisionObject = ctrl->GetRigidcollisionObject();
		SimdVector3 collisionObjectAabbMin,collisionObjectAabbMax;
		collisionObject->m_collisionShape->GetAabb(collisionObject->m_worldTransform,collisionObjectAabbMin,collisionObjectAabbMax);

		//check aabb overlap

		if (TestAabbAgainstAabb2(rayAabbMin,rayAabbMax,collisionObjectAabbMin,collisionObjectAabbMax))
		{
			RayTestSingle(rayFromTrans,rayToTrans,
				collisionObject,
					 collisionObject->m_collisionShape,
					  collisionObject->m_worldTransform,
					  resultCallback);
			
		}
	}

}
