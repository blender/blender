/*
 * Copyright (c) 2005 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#include "btWheelInfo.h"
#include "BulletDynamics/Dynamics/btRigidBody.h" // for pointvelocity


btScalar btWheelInfo::getSuspensionRestLength() const
{

	return m_suspensionRestLength1;

}

void	btWheelInfo::updateWheel(const btRigidBody& chassis,RaycastInfo& raycastInfo)
{

	
	if (m_raycastInfo.m_isInContact)

	{
		btScalar	project= m_raycastInfo.m_contactNormalWS.dot( m_raycastInfo.m_wheelDirectionWS );
		btVector3	 chassis_velocity_at_contactPoint;
		btVector3 relpos = m_raycastInfo.m_contactPointWS - chassis.getCenterOfMassPosition();
		chassis_velocity_at_contactPoint = chassis.getVelocityInLocalPoint( relpos );
		btScalar projVel = m_raycastInfo.m_contactNormalWS.dot( chassis_velocity_at_contactPoint );
		if ( project >= -0.1f)
		{
			m_suspensionRelativeVelocity = 0.0f;
			m_clippedInvContactDotSuspension = 1.0f / 0.1f;
		}
		else
		{
			btScalar inv = -1.f / project;
			m_suspensionRelativeVelocity = projVel * inv;
			m_clippedInvContactDotSuspension = inv;
		}
		
	}

	else	// Not in contact : position wheel in a nice (rest length) position
	{
		m_raycastInfo.m_suspensionLength = this->getSuspensionRestLength();
		m_suspensionRelativeVelocity = 0.0f;
		m_raycastInfo.m_contactNormalWS = -m_raycastInfo.m_wheelDirectionWS;
		m_clippedInvContactDotSuspension = 1.0f;
	}
}
