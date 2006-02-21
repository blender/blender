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
#include "WheelInfo.h"
#include "Dynamics/RigidBody.h" // for pointvelocity


SimdScalar WheelInfo::GetSuspensionRestLength() const
{

	return m_suspensionRestLength1;

}

void	WheelInfo::UpdateWheel(const RigidBody& chassis,RaycastInfo& raycastInfo)
{

	
	if (m_raycastInfo.m_isInContact)

	{
		SimdScalar	project= m_raycastInfo.m_contactNormalWS.dot( m_raycastInfo.m_wheelDirectionWS );
		SimdVector3	 chassis_velocity_at_contactPoint;
		SimdVector3 relpos = m_raycastInfo.m_contactPointWS - chassis.getCenterOfMassPosition();
		chassis_velocity_at_contactPoint = chassis.getVelocityInLocalPoint( relpos );
		SimdScalar projVel = m_raycastInfo.m_contactNormalWS.dot( chassis_velocity_at_contactPoint );
		if ( project >= -0.1f)
		{
			m_suspensionRelativeVelocity = 0.0f;
			m_clippedInvContactDotSuspension = 1.0f / 0.1f;
		}
		else
		{
			SimdScalar inv = -1.f / project;
			m_suspensionRelativeVelocity = projVel * inv;
			m_clippedInvContactDotSuspension = inv;
		}
		
	}

	else	// Not in contact : position wheel in a nice (rest length) position
	{
		m_raycastInfo.m_suspensionLength = this->GetSuspensionRestLength();
		m_suspensionRelativeVelocity = 0.0f;
		m_raycastInfo.m_contactNormalWS = -m_raycastInfo.m_wheelDirectionWS;
		m_clippedInvContactDotSuspension = 1.0f;
	}
}
