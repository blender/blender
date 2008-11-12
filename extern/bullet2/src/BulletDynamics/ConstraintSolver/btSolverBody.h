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

#ifndef BT_SOLVER_BODY_H
#define BT_SOLVER_BODY_H

class	btRigidBody;
#include "LinearMath/btVector3.h"
#include "LinearMath/btMatrix3x3.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btAlignedAllocator.h"
#include "LinearMath/btTransformUtil.h"


///btSolverBody is an internal datastructure for the constraint solver. Only necessary data is packed to increase cache coherence/performance.
ATTRIBUTE_ALIGNED16 (struct)	btSolverBody
{
	BT_DECLARE_ALIGNED_ALLOCATOR();
	
	btVector3		m_angularVelocity;
	float			m_angularFactor;
	float			m_invMass;
	float			m_friction;
	btRigidBody*	m_originalBody;
	btVector3		m_linearVelocity;
	btVector3		m_centerOfMassPosition;
	
	btVector3		m_pushVelocity;
	btVector3		m_turnVelocity;
	
	
	SIMD_FORCE_INLINE void	getVelocityInLocalPoint(const btVector3& rel_pos, btVector3& velocity ) const
	{
		velocity = m_linearVelocity + m_angularVelocity.cross(rel_pos);
	}

	//Optimization for the iterative solver: avoid calculating constant terms involving inertia, normal, relative position
	SIMD_FORCE_INLINE void internalApplyImpulse(const btVector3& linearComponent, const btVector3& angularComponent,btScalar impulseMagnitude)
	{
		if (m_invMass)
		{
			m_linearVelocity += linearComponent*impulseMagnitude;
			m_angularVelocity += angularComponent*(impulseMagnitude*m_angularFactor);
		}
	}
	
	SIMD_FORCE_INLINE void internalApplyPushImpulse(const btVector3& linearComponent, const btVector3& angularComponent,btScalar impulseMagnitude)
	{
		if (m_invMass)
		{
			m_pushVelocity += linearComponent*impulseMagnitude;
			m_turnVelocity += angularComponent*(impulseMagnitude*m_angularFactor);
		}
	}

	
	void	writebackVelocity()
	{
		if (m_invMass)
		{
			m_originalBody->setLinearVelocity(m_linearVelocity);
			m_originalBody->setAngularVelocity(m_angularVelocity);
			
			//m_originalBody->setCompanionId(-1);
		}
	}
	

	void	writebackVelocity(btScalar timeStep)
	{
		if (m_invMass)
		{
			m_originalBody->setLinearVelocity(m_linearVelocity);
			m_originalBody->setAngularVelocity(m_angularVelocity);
			
			//correct the position/orientation based on push/turn recovery
			btTransform newTransform;
			btTransformUtil::integrateTransform(m_originalBody->getWorldTransform(),m_pushVelocity,m_turnVelocity,timeStep,newTransform);
			m_originalBody->setWorldTransform(newTransform);
			
			//m_originalBody->setCompanionId(-1);
		}
	}

	void	readVelocity()
	{
		if (m_invMass)
		{
			m_linearVelocity = m_originalBody->getLinearVelocity();
			m_angularVelocity = m_originalBody->getAngularVelocity();
		}
	}

	


};

#endif //BT_SOLVER_BODY_H


