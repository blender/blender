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

#ifndef COLLISION_WORLD_H
#define COLLISION_WORLD_H


class CollisionShape;
class CollisionDispatcher;
class BroadphaseInterface;
#include "SimdVector3.h"
#include "SimdTransform.h"
#include "CollisionObject.h"

#include <vector>

///CollisionWorld is interface and container for the collision detection
class CollisionWorld
{

	

	std::vector<CollisionObject*>	m_collisionObjects;
	
	CollisionDispatcher*	m_dispatcher;

	BroadphaseInterface*	m_broadphase;

public:

	CollisionWorld(CollisionDispatcher* dispatcher,BroadphaseInterface* broadphase)
		:m_dispatcher(dispatcher),
		m_broadphase(broadphase)
	{

	}
	virtual ~CollisionWorld() {}

	virtual	void	UpdateActivationState();

	BroadphaseInterface*	GetBroadphase()
	{
		return m_broadphase;
	}

	CollisionDispatcher*	GetDispatcher()
	{
		return m_dispatcher;
	}

	///LocalShapeInfo gives extra information for complex shapes
	///Currently, only TriangleMeshShape is available, so it just contains triangleIndex and subpart
	struct	LocalShapeInfo
	{
		int	m_shapePart;
		int	m_triangleIndex;
		
		//const CollisionShape*	m_shapeTemp;
		//const SimdTransform*	m_shapeLocalTransform;
	};

	struct	LocalRayResult
	{
		LocalRayResult(const CollisionObject*	collisionObject, 
			LocalShapeInfo*	localShapeInfo,
			const SimdVector3&		hitNormalLocal,
			float hitFraction)
		:m_collisionObject(collisionObject),
		m_localShapeInfo(m_localShapeInfo),
		m_hitNormalLocal(hitNormalLocal),
		m_hitFraction(hitFraction)
		{
		}

		const CollisionObject*	m_collisionObject;
		LocalShapeInfo*			m_localShapeInfo;
		const SimdVector3&		m_hitNormalLocal;
		float					m_hitFraction;

	};

	///RayResultCallback is used to report new raycast results
	struct	RayResultCallback
	{
		virtual ~RayResultCallback()
		{
		}
		float	m_closestHitFraction;
		bool	HasHit()
		{
			return (m_closestHitFraction < 1.f);
		}

		RayResultCallback()
			:m_closestHitFraction(1.f)
		{
		}
		virtual	float	AddSingleResult(const LocalRayResult& rayResult) = 0;
	};

	struct	ClosestRayResultCallback : public RayResultCallback
	{
		ClosestRayResultCallback(SimdVector3	rayFromWorld,SimdVector3	rayToWorld)
		:m_rayFromWorld(rayFromWorld),
		m_rayToWorld(rayToWorld),
		m_collisionObject(0)
		{
		}

		SimdVector3	m_rayFromWorld;//used to calculate hitPointWorld from hitFraction
		SimdVector3	m_rayToWorld;

		SimdVector3	m_hitNormalWorld;
		SimdVector3	m_hitPointWorld;
		const CollisionObject*	m_collisionObject;
		
		virtual	float	AddSingleResult(const LocalRayResult& rayResult)
		{

//caller already does the filter on the m_closestHitFraction
			assert(rayResult.m_hitFraction <= m_closestHitFraction);
			
			m_closestHitFraction = rayResult.m_hitFraction;
			m_collisionObject = rayResult.m_collisionObject;
			m_hitNormalWorld = m_collisionObject->m_worldTransform.getBasis()*rayResult.m_hitNormalLocal;
			m_hitPointWorld.setInterpolate3(m_rayFromWorld,m_rayToWorld,rayResult.m_hitFraction);
			return rayResult.m_hitFraction;
		}
	};


	

	int	GetNumCollisionObjects() const
	{
		return m_collisionObjects.size();
	}

	void	RayTest(const SimdVector3& rayFromWorld, const SimdVector3& rayToWorld, RayResultCallback& resultCallback);


	void	AddCollisionObject(CollisionObject* collisionObject);

	void	RemoveCollisionObject(CollisionObject* collisionObject);

	virtual void	PerformDiscreteCollisionDetection();

};


#endif //COLLISION_WORLD_H
