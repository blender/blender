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

#include "RaycastVehicle.h"
#include "ConstraintSolver/Solve2LinearConstraint.h"
#include "ConstraintSolver/JacobianEntry.h"
#include "SimdQuaternion.h"
#include "SimdVector3.h"
#include "VehicleRaycaster.h"
#include "WheelInfo.h"


#include "Dynamics/MassProps.h"
#include "ConstraintSolver/ContactConstraint.h"



static RigidBody s_fixedObject( MassProps ( 0.0f, SimdVector3(0,0,0) ),0.f,0.f,0.f,0.f);

RaycastVehicle::RaycastVehicle(const VehicleTuning& tuning,RigidBody* chassis,	VehicleRaycaster* raycaster )
:m_vehicleRaycaster(raycaster),
m_pitchControl(0.f)
{
	m_chassisBody = chassis;
	m_indexRightAxis = 0;
	m_indexUpAxis = 2;
	m_indexForwardAxis = 1;
	DefaultInit(tuning);
}


void RaycastVehicle::DefaultInit(const VehicleTuning& tuning)
{
	m_currentVehicleSpeedKmHour = 0.f;
	m_steeringValue = 0.f;
	
}

	

RaycastVehicle::~RaycastVehicle()
{
}


//
// basically most of the code is general for 2 or 4 wheel vehicles, but some of it needs to be reviewed
//
WheelInfo&	RaycastVehicle::AddWheel( const SimdVector3& connectionPointCS, const SimdVector3& wheelDirectionCS0,const SimdVector3& wheelAxleCS, SimdScalar suspensionRestLength, SimdScalar wheelRadius,const VehicleTuning& tuning, bool isFrontWheel)
{

	WheelInfoConstructionInfo ci;

	ci.m_chassisConnectionCS = connectionPointCS;
	ci.m_wheelDirectionCS = wheelDirectionCS0;
	ci.m_wheelAxleCS = wheelAxleCS;
	ci.m_suspensionRestLength = suspensionRestLength;
	ci.m_wheelRadius = wheelRadius;
	ci.m_suspensionStiffness = tuning.m_suspensionStiffness;
	ci.m_wheelsDampingCompression = tuning.m_suspensionCompression;
	ci.m_wheelsDampingRelaxation = tuning.m_suspensionDamping;
	ci.m_frictionSlip = tuning.m_frictionSlip;
	ci.m_bIsFrontWheel = isFrontWheel;
	ci.m_maxSuspensionTravelCm = tuning.m_maxSuspensionTravelCm;

	m_wheelInfo.push_back( WheelInfo(ci));
	
	WheelInfo& wheel = m_wheelInfo[GetNumWheels()-1];
	
	UpdateWheelTransformsWS( wheel );
	return wheel;
}




const SimdTransform&	RaycastVehicle::GetWheelTransformWS( int wheelIndex ) const
{
	assert(wheelIndex < GetNumWheels());
	const WheelInfo& wheel = m_wheelInfo[wheelIndex];
	return wheel.m_worldTransform;

}

void	RaycastVehicle::UpdateWheelTransform( int wheelIndex )
{
	
	WheelInfo& wheel = m_wheelInfo[ wheelIndex ];
	UpdateWheelTransformsWS(wheel);
	SimdVector3 up = -wheel.m_raycastInfo.m_wheelDirectionWS;
	const SimdVector3& right = wheel.m_raycastInfo.m_wheelAxleWS;
	SimdVector3 fwd = up.cross(right);
	fwd = fwd.normalize();
	//rotate around steering over de wheelAxleWS
	float steering = wheel.m_steering;
	
	SimdQuaternion steeringOrn(up,steering);//wheel.m_steering);
	SimdMatrix3x3 steeringMat(steeringOrn);

	SimdQuaternion rotatingOrn(right,wheel.m_rotation);
	SimdMatrix3x3 rotatingMat(rotatingOrn);

	SimdMatrix3x3 basis2(
		right[0],fwd[0],up[0],
		right[1],fwd[1],up[1],
		right[2],fwd[2],up[2]
	);
	
	wheel.m_worldTransform.setBasis(steeringMat * rotatingMat * basis2);
	wheel.m_worldTransform.setOrigin(
		wheel.m_raycastInfo.m_hardPointWS + wheel.m_raycastInfo.m_wheelDirectionWS * wheel.m_raycastInfo.m_suspensionLength
	);
}

void RaycastVehicle::ResetSuspension()
{

	std::vector<WheelInfo>::iterator wheelIt;
	for (wheelIt = m_wheelInfo.begin();
	!(wheelIt == m_wheelInfo.end());wheelIt++)
	{
			WheelInfo& wheel = *wheelIt;
			wheel.m_raycastInfo.m_suspensionLength = wheel.GetSuspensionRestLength();
			wheel.m_suspensionRelativeVelocity = 0.0f;
			
			wheel.m_raycastInfo.m_contactNormalWS = - wheel.m_raycastInfo.m_wheelDirectionWS;
			//wheel_info.setContactFriction(0.0f);
			wheel.m_clippedInvContactDotSuspension = 1.0f;
	}
}

void	RaycastVehicle::UpdateWheelTransformsWS(WheelInfo& wheel )
{
	wheel.m_raycastInfo.m_isInContact = false;

	const SimdTransform& chassisTrans = GetRigidBody()->getCenterOfMassTransform();

	wheel.m_raycastInfo.m_hardPointWS = chassisTrans( wheel.m_chassisConnectionPointCS );
	wheel.m_raycastInfo.m_wheelDirectionWS = chassisTrans.getBasis() *  wheel.m_wheelDirectionCS ;
	wheel.m_raycastInfo.m_wheelAxleWS = chassisTrans.getBasis() * wheel.m_wheelAxleCS;
}

SimdScalar RaycastVehicle::Raycast(WheelInfo& wheel)
{
	UpdateWheelTransformsWS( wheel );

	
	SimdScalar depth = -1;
	
	SimdScalar raylen = wheel.GetSuspensionRestLength()+wheel.m_wheelsRadius;

	SimdVector3 rayvector = wheel.m_raycastInfo.m_wheelDirectionWS * (raylen);
	const SimdVector3& source = wheel.m_raycastInfo.m_hardPointWS;
	wheel.m_raycastInfo.m_contactPointWS = source + rayvector;
	const SimdVector3& target = wheel.m_raycastInfo.m_contactPointWS;

	SimdScalar param = 0.f;
	
	VehicleRaycaster::VehicleRaycasterResult	rayResults;

	void* object = m_vehicleRaycaster->CastRay(source,target,rayResults);

	wheel.m_raycastInfo.m_groundObject = 0;

	if (object)
	{
		param = rayResults.m_distFraction;
		depth = raylen * rayResults.m_distFraction;
		wheel.m_raycastInfo.m_contactNormalWS  = rayResults.m_hitNormalInWorld;
		wheel.m_raycastInfo.m_isInContact = true;
		
		wheel.m_raycastInfo.m_groundObject = &s_fixedObject;//todo for driving on dynamic/movable objects!;
		//wheel.m_raycastInfo.m_groundObject = object;


		SimdScalar hitDistance = param*raylen;
		wheel.m_raycastInfo.m_suspensionLength = hitDistance - wheel.m_wheelsRadius;
		//clamp on max suspension travel

		float  minSuspensionLength = wheel.GetSuspensionRestLength() - wheel.m_maxSuspensionTravelCm*0.01f;
		float maxSuspensionLength = wheel.GetSuspensionRestLength()+ wheel.m_maxSuspensionTravelCm*0.01f;
		if (wheel.m_raycastInfo.m_suspensionLength < minSuspensionLength)
		{
			wheel.m_raycastInfo.m_suspensionLength = minSuspensionLength;
		}
		if (wheel.m_raycastInfo.m_suspensionLength > maxSuspensionLength)
		{
			wheel.m_raycastInfo.m_suspensionLength = maxSuspensionLength;
		}

		wheel.m_raycastInfo.m_contactPointWS = rayResults.m_hitPointInWorld;

		SimdScalar denominator= wheel.m_raycastInfo.m_contactNormalWS.dot( wheel.m_raycastInfo.m_wheelDirectionWS );

		SimdVector3 chassis_velocity_at_contactPoint;
		SimdVector3 relpos = wheel.m_raycastInfo.m_contactPointWS-GetRigidBody()->getCenterOfMassPosition();

		chassis_velocity_at_contactPoint = GetRigidBody()->getVelocityInLocalPoint(relpos);

		SimdScalar projVel = wheel.m_raycastInfo.m_contactNormalWS.dot( chassis_velocity_at_contactPoint );

		if ( denominator >= -0.1f)
		{
			wheel.m_suspensionRelativeVelocity = 0.0f;
			wheel.m_clippedInvContactDotSuspension = 1.0f / 0.1f;
		}
		else
		{
			SimdScalar inv = -1.f / denominator;
			wheel.m_suspensionRelativeVelocity = projVel * inv;
			wheel.m_clippedInvContactDotSuspension = inv;
		}
			
	} else
	{
		//put wheel info as in rest position
		wheel.m_raycastInfo.m_suspensionLength = wheel.GetSuspensionRestLength();
		wheel.m_suspensionRelativeVelocity = 0.0f;
		wheel.m_raycastInfo.m_contactNormalWS = - wheel.m_raycastInfo.m_wheelDirectionWS;
		wheel.m_clippedInvContactDotSuspension = 1.0f;
	}

	return depth;
}


void RaycastVehicle::UpdateVehicle( SimdScalar step )
{

	m_currentVehicleSpeedKmHour = 3.6f * GetRigidBody()->getLinearVelocity().length();
	
	const SimdTransform& chassisTrans = GetRigidBody()->getCenterOfMassTransform();
	SimdVector3 forwardW (
		chassisTrans.getBasis()[0][m_indexForwardAxis],
		chassisTrans.getBasis()[1][m_indexForwardAxis],
		chassisTrans.getBasis()[2][m_indexForwardAxis]);

	if (forwardW.dot(GetRigidBody()->getLinearVelocity()) < 0.f)
	{
		m_currentVehicleSpeedKmHour *= -1.f;
	}

	//
	// simulate suspension
	//
	std::vector<WheelInfo>::iterator wheelIt;
	int i=0;
	for (wheelIt = m_wheelInfo.begin();
	!(wheelIt == m_wheelInfo.end());wheelIt++,i++)
	{
		WheelInfo& wheelInfo = *wheelIt;
		
		SimdScalar depth; 
		depth = Raycast( *wheelIt );
	}

	UpdateSuspension(step);

	
	for (wheelIt = m_wheelInfo.begin();
	!(wheelIt == m_wheelInfo.end());wheelIt++)
	{
		//apply suspension force
		WheelInfo& wheel = *wheelIt;
		
		float suspensionForce = wheel.m_wheelsSuspensionForce;
		
		float gMaxSuspensionForce = 6000.f;
		if (suspensionForce > gMaxSuspensionForce)
		{
			suspensionForce = gMaxSuspensionForce;
		}
		SimdVector3 impulse = wheel.m_raycastInfo.m_contactNormalWS * suspensionForce * step;
		SimdVector3 relpos = wheel.m_raycastInfo.m_contactPointWS - GetRigidBody()->getCenterOfMassPosition();
		
		GetRigidBody()->applyImpulse(impulse, relpos);
	
	}
	

	
	UpdateFriction( step);

	
	for (wheelIt = m_wheelInfo.begin();
	!(wheelIt == m_wheelInfo.end());wheelIt++)
	{
		WheelInfo& wheel = *wheelIt;
		SimdVector3 relpos = wheel.m_raycastInfo.m_hardPointWS - GetRigidBody()->getCenterOfMassPosition();
		SimdVector3 vel = GetRigidBody()->getVelocityInLocalPoint( relpos );

		if (wheel.m_raycastInfo.m_isInContact)
		{
			SimdVector3 fwd (
				GetRigidBody()->getCenterOfMassTransform().getBasis()[0][m_indexForwardAxis],
				GetRigidBody()->getCenterOfMassTransform().getBasis()[1][m_indexForwardAxis],
				GetRigidBody()->getCenterOfMassTransform().getBasis()[2][m_indexForwardAxis]);

			SimdScalar proj = fwd.dot(wheel.m_raycastInfo.m_contactNormalWS);
			fwd -= wheel.m_raycastInfo.m_contactNormalWS * proj;

			SimdScalar proj2 = fwd.dot(vel);
			
			wheel.m_deltaRotation = (proj2 * step) / (wheel.m_wheelsRadius);
			wheel.m_rotation += wheel.m_deltaRotation;

		} else
		{
			wheel.m_rotation += wheel.m_deltaRotation;
		}
		
		wheel.m_deltaRotation *= 0.99f;//damping of rotation when not in contact

	}



}


void	RaycastVehicle::SetSteeringValue(SimdScalar steering,int wheel)
{
	assert(wheel>=0 && wheel < GetNumWheels());

	WheelInfo& wheelInfo = GetWheelInfo(wheel);
	wheelInfo.m_steering = steering;
}



SimdScalar	RaycastVehicle::GetSteeringValue(int wheel) const
{
	return GetWheelInfo(wheel).m_steering;
}


void	RaycastVehicle::ApplyEngineForce(SimdScalar force, int wheel)
{
	for (int i=0;i<GetNumWheels();i++)
	{
		WheelInfo& wheelInfo = GetWheelInfo(i);

		bool applyOnFrontWheel = !wheel;

		if (applyOnFrontWheel == wheelInfo.m_bIsFrontWheel)
		{
			wheelInfo.m_engineForce = force;
		}
	}
}


const WheelInfo&	RaycastVehicle::GetWheelInfo(int index) const
{
	ASSERT((index >= 0) && (index < 	GetNumWheels()));
	
	return m_wheelInfo[index];
}

WheelInfo&	RaycastVehicle::GetWheelInfo(int index) 
{
	ASSERT((index >= 0) && (index < 	GetNumWheels()));
	
	return m_wheelInfo[index];
}

void RaycastVehicle::SetBrake(float brake,int wheelIndex)
{
	ASSERT((wheelIndex >= 0) && (wheelIndex < 	GetNumWheels()));
	GetWheelInfo(wheelIndex).m_brake;
}


void	RaycastVehicle::UpdateSuspension(SimdScalar deltaTime)
{

	SimdScalar chassisMass = 1.f / m_chassisBody->getInvMass();
	
	for (int w_it=0; w_it<GetNumWheels(); w_it++)
	{
		WheelInfo &wheel_info = m_wheelInfo[w_it];
		
		if ( wheel_info.m_raycastInfo.m_isInContact )
		{
			SimdScalar force;
			//	Spring
			{
				SimdScalar	susp_length			= wheel_info.GetSuspensionRestLength();
				SimdScalar	current_length = wheel_info.m_raycastInfo.m_suspensionLength;

				SimdScalar length_diff = (susp_length - current_length);

				force = wheel_info.m_suspensionStiffness
					* length_diff * wheel_info.m_clippedInvContactDotSuspension;
			}
		
			// Damper
			{
				SimdScalar projected_rel_vel = wheel_info.m_suspensionRelativeVelocity;
				{
					SimdScalar	susp_damping;
					if ( projected_rel_vel < 0.0f )
					{
						susp_damping = wheel_info.m_wheelsDampingCompression;
					}
					else
					{
						susp_damping = wheel_info.m_wheelsDampingRelaxation;
					}
					force -= susp_damping * projected_rel_vel;
				}
			}

			// RESULT
			wheel_info.m_wheelsSuspensionForce = force * chassisMass;
			if (wheel_info.m_wheelsSuspensionForce < 0.f)
			{
				wheel_info.m_wheelsSuspensionForce = 0.f;
			}
		}
		else
		{
			wheel_info.m_wheelsSuspensionForce = 0.0f;
		}
	}

}

float sideFrictionStiffness2 = 1.0f;
void	RaycastVehicle::UpdateFriction(SimdScalar	timeStep)
{

		//calculate the impulse, so that the wheels don't move sidewards
		int numWheel = GetNumWheels();
		if (!numWheel)
			return;


		SimdVector3*	forwardWS = new	SimdVector3[numWheel];
		SimdVector3*	axle = new SimdVector3[numWheel];
		SimdScalar* forwardImpulse = new SimdScalar[numWheel];
		SimdScalar* sideImpulse = new SimdScalar[numWheel];
		
		int numWheelsOnGround = 0;
	

		//collapse all those loops into one!
		for (int i=0;i<GetNumWheels();i++)
		{
			WheelInfo& wheelInfo = m_wheelInfo[i];
			class RigidBody* groundObject = (class RigidBody*) wheelInfo.m_raycastInfo.m_groundObject;
			if (groundObject)
				numWheelsOnGround++;
			sideImpulse[i] = 0.f;
			forwardImpulse[i] = 0.f;

		}
	
		{
	
			for (int i=0;i<GetNumWheels();i++)
			{

				WheelInfo& wheelInfo = m_wheelInfo[i];
					
				class RigidBody* groundObject = (class RigidBody*) wheelInfo.m_raycastInfo.m_groundObject;

				if (groundObject)
				{

					const SimdTransform& wheelTrans = GetWheelTransformWS( i );

					SimdMatrix3x3 wheelBasis0 = wheelTrans.getBasis();
					axle[i] = SimdVector3(	
						wheelBasis0[0][m_indexRightAxis],
						wheelBasis0[1][m_indexRightAxis],
						wheelBasis0[2][m_indexRightAxis]);
					
					const SimdVector3& surfNormalWS = wheelInfo.m_raycastInfo.m_contactNormalWS;
					SimdScalar proj = axle[i].dot(surfNormalWS);
					axle[i] -= surfNormalWS * proj;
					axle[i] = axle[i].normalize();
					
					forwardWS[i] = surfNormalWS.cross(axle[i]);
					forwardWS[i].normalize();

				
					resolveSingleBilateral(*m_chassisBody, wheelInfo.m_raycastInfo.m_contactPointWS,
							  *groundObject, wheelInfo.m_raycastInfo.m_contactPointWS,
							  0.f, axle[i],sideImpulse[i],timeStep);

					sideImpulse[i] *= sideFrictionStiffness2;
						
				}
				

			}
		}

	SimdScalar sideFactor = 1.f;
	SimdScalar fwdFactor = 0.5;

	bool sliding = false;
	{
		for (int wheel =0;wheel <GetNumWheels();wheel++)
		{
			WheelInfo& wheelInfo = m_wheelInfo[wheel];
			class RigidBody* groundObject = (class RigidBody*) wheelInfo.m_raycastInfo.m_groundObject;


			forwardImpulse[wheel] = 0.f;
			m_wheelInfo[wheel].m_skidInfo= 1.f;

			if (groundObject)
			{
				m_wheelInfo[wheel].m_skidInfo= 1.f;
				
				SimdScalar maximp = wheelInfo.m_wheelsSuspensionForce * timeStep * wheelInfo.m_frictionSlip;
				SimdScalar maximpSide = maximp;

				SimdScalar maximpSquared = maximp * maximpSide;

				forwardImpulse[wheel] = wheelInfo.m_engineForce* timeStep;

				float x = (forwardImpulse[wheel] ) * fwdFactor;
				float y = (sideImpulse[wheel] ) * sideFactor;
				
				float impulseSquared = (x*x + y*y);

				if (impulseSquared > maximpSquared)
				{
					sliding = true;
					
					SimdScalar factor = maximp / sqrtf(impulseSquared);
					
					m_wheelInfo[wheel].m_skidInfo *= factor;
				}
			} 

		}
	}

	


		if (sliding)
		{
			for (int wheel = 0;wheel < GetNumWheels(); wheel++)
			{
				if (sideImpulse[wheel] != 0.f)
				{
					if (m_wheelInfo[wheel].m_skidInfo< 1.f)
					{
						forwardImpulse[wheel] *=	m_wheelInfo[wheel].m_skidInfo;
						sideImpulse[wheel] *= m_wheelInfo[wheel].m_skidInfo;
					}
				}
			}
		}

		// apply the impulses
		{
			for (int wheel = 0;wheel<GetNumWheels() ; wheel++)
			{
				WheelInfo& wheelInfo = m_wheelInfo[wheel];

				SimdVector3 rel_pos = wheelInfo.m_raycastInfo.m_contactPointWS - 
						m_chassisBody->getCenterOfMassPosition();

				if (forwardImpulse[wheel] != 0.f)
				{
					m_chassisBody->applyImpulse(forwardWS[wheel]*(forwardImpulse[wheel]),rel_pos);
				}
				if (sideImpulse[wheel] != 0.f)
				{
					class RigidBody* groundObject = (class RigidBody*) m_wheelInfo[wheel].m_raycastInfo.m_groundObject;

					SimdVector3 rel_pos2 = wheelInfo.m_raycastInfo.m_contactPointWS - 
						groundObject->getCenterOfMassPosition();

					
					SimdVector3 sideImp = axle[wheel] * sideImpulse[wheel];

					rel_pos[2] *= wheelInfo.m_rollInfluence;
					m_chassisBody->applyImpulse(sideImp,rel_pos);

					//apply friction impulse on the ground
					groundObject->applyImpulse(-sideImp,rel_pos2);
				}
			}
		}

		delete []forwardWS;
		delete [] axle;
		delete[]forwardImpulse;
		delete[] sideImpulse;
}
