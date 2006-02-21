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
#ifndef WHEEL_INFO_H
#define WHEEL_INFO_H

#include "SimdVector3.h"
#include "SimdTransform.h"

class RigidBody;

struct WheelInfoConstructionInfo
{
	SimdVector3	m_chassisConnectionCS;
	SimdVector3	m_wheelDirectionCS;
	SimdVector3	m_wheelAxleCS;
	SimdScalar	m_suspensionRestLength;
	SimdScalar	m_maxSuspensionTravelCm;
	SimdScalar	m_wheelRadius;
	
	float		m_suspensionStiffness;
	float		m_wheelsDampingCompression;
	float		m_wheelsDampingRelaxation;
	float		m_frictionSlip;
	bool m_bIsFrontWheel;
	
};

/// WheelInfo contains information per wheel about friction and suspension.
struct WheelInfo
{
	struct RaycastInfo
	{
		//set by raycaster
		SimdVector3	m_contactNormalWS;//contactnormal
		SimdVector3	m_contactPointWS;//raycast hitpoint
		SimdScalar	m_suspensionLength;
		SimdVector3	m_hardPointWS;//raycast starting point
		SimdVector3	m_wheelDirectionWS; //direction in worldspace
		SimdVector3	m_wheelAxleWS; // axle in worldspace
		bool		m_isInContact;
		void*		m_groundObject; //could be general void* ptr
	};

	RaycastInfo	m_raycastInfo;

	SimdTransform	m_worldTransform;
	
	SimdVector3	m_chassisConnectionPointCS; //const
	SimdVector3	m_wheelDirectionCS;//const
	SimdVector3	m_wheelAxleCS; // const or modified by steering
	SimdScalar	m_suspensionRestLength1;//const
	SimdScalar	m_maxSuspensionTravelCm;
	SimdScalar GetSuspensionRestLength() const;
	SimdScalar	m_wheelsRadius;//const
	SimdScalar	m_suspensionStiffness;//const
	SimdScalar	m_wheelsDampingCompression;//const
	SimdScalar	m_wheelsDampingRelaxation;//const
	SimdScalar	m_frictionSlip;
	SimdScalar	m_steering;
	SimdScalar	m_rotation;
	SimdScalar	m_deltaRotation;
	SimdScalar	m_rollInfluence;

	SimdScalar	m_engineForce;

	SimdScalar	m_brake;
	
	bool m_bIsFrontWheel;
	
	void*		m_clientInfo;//can be used to store pointer to sync transforms...

	WheelInfo(WheelInfoConstructionInfo& ci)

	{

		m_suspensionRestLength1 = ci.m_suspensionRestLength;
		m_maxSuspensionTravelCm = ci.m_maxSuspensionTravelCm;

		m_wheelsRadius = ci.m_wheelRadius;
		m_suspensionStiffness = ci.m_suspensionStiffness;
		m_wheelsDampingCompression = ci.m_wheelsDampingCompression;
		m_wheelsDampingRelaxation = ci.m_wheelsDampingRelaxation;
		m_chassisConnectionPointCS = ci.m_chassisConnectionCS;
		m_wheelDirectionCS = ci.m_wheelDirectionCS;
		m_wheelAxleCS = ci.m_wheelAxleCS;
		m_frictionSlip = ci.m_frictionSlip;
		m_steering = 0.f;
		m_engineForce = 0.f;
		m_rotation = 0.f;
		m_deltaRotation = 0.f;
		m_brake = 0.f;
		m_rollInfluence = 0.1f;
		m_bIsFrontWheel = ci.m_bIsFrontWheel;

	}

	void	UpdateWheel(const RigidBody& chassis,RaycastInfo& raycastInfo);

	SimdScalar	m_clippedInvContactDotSuspension;
	SimdScalar	m_suspensionRelativeVelocity;
	//calculated by suspension
	SimdScalar	m_wheelsSuspensionForce;
	SimdScalar	m_skidInfo;

};

#endif //WHEEL_INFO_H