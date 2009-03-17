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

#ifndef CONTACT_CONSTRAINT_H
#define CONTACT_CONSTRAINT_H

///@todo: make into a proper class working with the iterative constraint solver

class btRigidBody;
#include "LinearMath/btVector3.h"
#include "LinearMath/btScalar.h"
struct btContactSolverInfo;
class btManifoldPoint;

enum {
	DEFAULT_CONTACT_SOLVER_TYPE=0,
	CONTACT_SOLVER_TYPE1,
	CONTACT_SOLVER_TYPE2,
	USER_CONTACT_SOLVER_TYPE1,
	MAX_CONTACT_SOLVER_TYPES
};


typedef btScalar (*ContactSolverFunc)(btRigidBody& body1,
									 btRigidBody& body2,
									 class btManifoldPoint& contactPoint,
									 const btContactSolverInfo& info);

///stores some extra information to each contact point. It is not in the contact point, because that want to keep the collision detection independent from the constraint solver.
struct btConstraintPersistentData
{
	inline btConstraintPersistentData()
	:m_appliedImpulse(btScalar(0.)),
	m_prevAppliedImpulse(btScalar(0.)),
	m_accumulatedTangentImpulse0(btScalar(0.)),
	m_accumulatedTangentImpulse1(btScalar(0.)),
	m_jacDiagABInv(btScalar(0.)),
	m_persistentLifeTime(0),
	m_restitution(btScalar(0.)),
	m_friction(btScalar(0.)),
	m_penetration(btScalar(0.)),
	m_contactSolverFunc(0),
	m_frictionSolverFunc(0)
	{
	}
	
					
				/// total applied impulse during most recent frame
			btScalar	m_appliedImpulse;
			btScalar	m_prevAppliedImpulse;
			btScalar	m_accumulatedTangentImpulse0;
			btScalar	m_accumulatedTangentImpulse1;
			
			btScalar	m_jacDiagABInv;
			btScalar	m_jacDiagABInvTangent0;
			btScalar	m_jacDiagABInvTangent1;
			int		m_persistentLifeTime;
			btScalar	m_restitution;
			btScalar	m_friction;
			btScalar	m_penetration;
			btVector3	m_frictionWorldTangential0;
			btVector3	m_frictionWorldTangential1;

			btVector3	m_frictionAngularComponent0A;
			btVector3	m_frictionAngularComponent0B;
			btVector3	m_frictionAngularComponent1A;
			btVector3	m_frictionAngularComponent1B;

			//some data doesn't need to be persistent over frames: todo: clean/reuse this
			btVector3	m_angularComponentA;
			btVector3	m_angularComponentB;
		
			ContactSolverFunc	m_contactSolverFunc;
			ContactSolverFunc	m_frictionSolverFunc;

};

///bilateral constraint between two dynamic objects
///positive distance = separation, negative distance = penetration
void resolveSingleBilateral(btRigidBody& body1, const btVector3& pos1,
                      btRigidBody& body2, const btVector3& pos2,
                      btScalar distance, const btVector3& normal,btScalar& impulse ,btScalar timeStep);


///contact constraint resolution:
///calculate and apply impulse to satisfy non-penetration and non-negative relative velocity constraint
///positive distance = separation, negative distance = penetration
btScalar resolveSingleCollision(
	btRigidBody& body1,
	btRigidBody& body2,
		btManifoldPoint& contactPoint,
		 const btContactSolverInfo& info);

btScalar resolveSingleFriction(
	btRigidBody& body1,
	btRigidBody& body2,
	btManifoldPoint& contactPoint,
	const btContactSolverInfo& solverInfo
		);



btScalar resolveSingleCollisionCombined(
	btRigidBody& body1,
	btRigidBody& body2,
	btManifoldPoint& contactPoint,
	const btContactSolverInfo& solverInfo
		);

#endif //CONTACT_CONSTRAINT_H
