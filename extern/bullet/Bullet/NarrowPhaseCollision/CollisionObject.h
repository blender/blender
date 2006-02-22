#ifndef COLLISION_OBJECT_H
#define COLLISION_OBJECT_H

#include "SimdTransform.h"

//island management, m_activationState1
#define ACTIVE_TAG 1
#define ISLAND_SLEEPING 2
#define WANTS_DEACTIVATION 3
#define DISABLE_DEACTIVATION 4

struct	BroadphaseProxy;
class	CollisionShape;

struct	CollisionObject
{
	SimdTransform	m_worldTransform;
	
	//m_nextPredictedWorldTransform is used for CCD and interpolation
	SimdTransform	m_nextPredictedWorldTransform;
	
	enum CollisionFlags
	{
		isStatic = 1,
	};

	int				m_collisionFlags;

	int				m_islandTag1;
	int				m_activationState1;
	float			m_deactivationTime;

	BroadphaseProxy*	m_broadphaseHandle;
	CollisionShape*		m_collisionShape;

	//time of impact calculation
	float			m_hitFraction; 

	bool			mergesSimulationIslands() const;


	CollisionObject()
	:	m_activationState1(1),
		m_deactivationTime(0.f),
		m_collisionFlags(0),
		m_hitFraction(1.f),
		m_broadphaseHandle(0),
		m_collisionShape(0)
	{
	}


	void	SetCollisionShape(CollisionShape* collisionShape)
	{
		m_collisionShape = collisionShape;
	}

	int	GetActivationState() const { return m_activationState1;}
	
	void SetActivationState(int newState);

	void	activate();



};

#endif //COLLISION_OBJECT_H
