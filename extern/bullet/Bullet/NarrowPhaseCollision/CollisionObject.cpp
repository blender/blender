#include "CollisionObject.h"



void CollisionObject::SetActivationState(int newState) 
{ 
	m_activationState1 = newState;
}

void CollisionObject::activate()
{
		SetActivationState(1);
		m_deactivationTime = 0.f;
}

bool CollisionObject::mergesSimulationIslands() const
{
	return ( !(m_collisionFlags & isStatic));
}
