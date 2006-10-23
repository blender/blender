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

#include "btRaycastVehicle.h"
#include "BulletDynamics/ConstraintSolver/btSolve2LinearConstraint.h"
#include "BulletDynamics/ConstraintSolver/btJacobianEntry.h"
#include "LinearMath/btQuaternion.h"
#include "LinearMath/btVector3.h"
#include "btVehicleRaycaster.h"
#include "btWheelInfo.h"


#include "BulletDynamics/ConstraintSolver/btContactConstraint.h"



static btRigidBody s_fixedObject( 0,btTransform(btQuaternion(0,0,0,1)),0);

btRaycastVehicle::btRaycastVehicle(const btVehicleTuning& tuning,btRigidBody* chassis,	btVehicleRaycaster* raycaster )
:m_vehicleRaycaster(raycaster),
m_pitchControl(0.f)
{
	m_chassisBody = chassis;
	m_indexRightAxis = 0;
	m_indexUpAxis = 2;
	m_indexForwardAxis = 1;
	defaultInit(tuning);
}


void btRaycastVehicle::defaultInit(const btVehicleTuning& tuning)
{
	m_currentVehicleSpeedKmHour = 0.f;
	m_steeringValue = 0.f;
	
}

	

btRaycastVehicle::~btRaycastVehicle()
{
}


//
// basically most of the code is general for 2 or 4 wheel vehicles, but some of it needs to be reviewed
//
btWheelInfo&	btRaycastVehicle::addWheel( const btVector3& connectionPointCS, const btVector3& wheelDirectionCS0,const btVector3& wheelAxleCS, btScalar suspensionRestLength, btScalar wheelRadius,const btVehicleTuning& tuning, bool isFrontWheel)
{

	btWheelInfoConstructionInfo ci;

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

	m_wheelInfo.push_back( btWheelInfo(ci));
	
	btWheelInfo& wheel = m_wheelInfo[getNumWheels()-1];
	
	updateWheelTransformsWS( wheel );
	updateWheelTransform(getNumWheels()-1);
	return wheel;
}




const btTransform&	btRaycastVehicle::getWheelTransformWS( int wheelIndex ) const
{
	assert(wheelIndex < getNumWheels());
	const btWheelInfo& wheel = m_wheelInfo[wheelIndex];
	return wheel.m_worldTransform;

}

void	btRaycastVehicle::updateWheelTransform( int wheelIndex )
{
	
	btWheelInfo& wheel = m_wheelInfo[ wheelIndex ];
	updateWheelTransformsWS(wheel);
	btVector3 up = -wheel.m_raycastInfo.m_wheelDirectionWS;
	const btVector3& right = wheel.m_raycastInfo.m_wheelAxleWS;
	btVector3 fwd = up.cross(right);
	fwd = fwd.normalize();
	//rotate around steering over de wheelAxleWS
	float steering = wheel.m_steering;
	
	btQuaternion steeringOrn(up,steering);//wheel.m_steering);
	btMatrix3x3 steeringMat(steeringOrn);

	btQuaternion rotatingOrn(right,wheel.m_rotation);
	btMatrix3x3 rotatingMat(rotatingOrn);

	btMatrix3x3 basis2(
		right[0],fwd[0],up[0],
		right[1],fwd[1],up[1],
		right[2],fwd[2],up[2]
	);
	
	wheel.m_worldTransform.setBasis(steeringMat * rotatingMat * basis2);
	wheel.m_worldTransform.setOrigin(
		wheel.m_raycastInfo.m_hardPointWS + wheel.m_raycastInfo.m_wheelDirectionWS * wheel.m_raycastInfo.m_suspensionLength
	);
}

void btRaycastVehicle::resetSuspension()
{

	std::vector<btWheelInfo>::iterator wheelIt;
	for (wheelIt = m_wheelInfo.begin();
	!(wheelIt == m_wheelInfo.end());wheelIt++)
	{
			btWheelInfo& wheel = *wheelIt;
			wheel.m_raycastInfo.m_suspensionLength = wheel.getSuspensionRestLength();
			wheel.m_suspensionRelativeVelocity = 0.0f;
			
			wheel.m_raycastInfo.m_contactNormalWS = - wheel.m_raycastInfo.m_wheelDirectionWS;
			//wheel_info.setContactFriction(0.0f);
			wheel.m_clippedInvContactDotSuspension = 1.0f;
	}
}

void	btRaycastVehicle::updateWheelTransformsWS(btWheelInfo& wheel )
{
	wheel.m_raycastInfo.m_isInContact = false;

	const btTransform& chassisTrans = getRigidBody()->getCenterOfMassTransform();

	wheel.m_raycastInfo.m_hardPointWS = chassisTrans( wheel.m_chassisConnectionPointCS );
	wheel.m_raycastInfo.m_wheelDirectionWS = chassisTrans.getBasis() *  wheel.m_wheelDirectionCS ;
	wheel.m_raycastInfo.m_wheelAxleWS = chassisTrans.getBasis() * wheel.m_wheelAxleCS;
}

btScalar btRaycastVehicle::rayCast(btWheelInfo& wheel)
{
	updateWheelTransformsWS( wheel );

	
	btScalar depth = -1;
	
	btScalar raylen = wheel.getSuspensionRestLength()+wheel.m_wheelsRadius;

	btVector3 rayvector = wheel.m_raycastInfo.m_wheelDirectionWS * (raylen);
	const btVector3& source = wheel.m_raycastInfo.m_hardPointWS;
	wheel.m_raycastInfo.m_contactPointWS = source + rayvector;
	const btVector3& target = wheel.m_raycastInfo.m_contactPointWS;

	btScalar param = 0.f;
	
	btVehicleRaycaster::btVehicleRaycasterResult	rayResults;

	void* object = m_vehicleRaycaster->castRay(source,target,rayResults);

	wheel.m_raycastInfo.m_groundObject = 0;

	if (object)
	{
		param = rayResults.m_distFraction;
		depth = raylen * rayResults.m_distFraction;
		wheel.m_raycastInfo.m_contactNormalWS  = rayResults.m_hitNormalInWorld;
		wheel.m_raycastInfo.m_isInContact = true;
		
		wheel.m_raycastInfo.m_groundObject = &s_fixedObject;//todo for driving on dynamic/movable objects!;
		//wheel.m_raycastInfo.m_groundObject = object;


		btScalar hitDistance = param*raylen;
		wheel.m_raycastInfo.m_suspensionLength = hitDistance - wheel.m_wheelsRadius;
		//clamp on max suspension travel

		float  minSuspensionLength = wheel.getSuspensionRestLength() - wheel.m_maxSuspensionTravelCm*0.01f;
		float maxSuspensionLength = wheel.getSuspensionRestLength()+ wheel.m_maxSuspensionTravelCm*0.01f;
		if (wheel.m_raycastInfo.m_suspensionLength < minSuspensionLength)
		{
			wheel.m_raycastInfo.m_suspensionLength = minSuspensionLength;
		}
		if (wheel.m_raycastInfo.m_suspensionLength > maxSuspensionLength)
		{
			wheel.m_raycastInfo.m_suspensionLength = maxSuspensionLength;
		}

		wheel.m_raycastInfo.m_contactPointWS = rayResults.m_hitPointInWorld;

		btScalar denominator= wheel.m_raycastInfo.m_contactNormalWS.dot( wheel.m_raycastInfo.m_wheelDirectionWS );

		btVector3 chassis_velocity_at_contactPoint;
		btVector3 relpos = wheel.m_raycastInfo.m_contactPointWS-getRigidBody()->getCenterOfMassPosition();

		chassis_velocity_at_contactPoint = getRigidBody()->getVelocityInLocalPoint(relpos);

		btScalar projVel = wheel.m_raycastInfo.m_contactNormalWS.dot( chassis_velocity_at_contactPoint );

		if ( denominator >= -0.1f)
		{
			wheel.m_suspensionRelativeVelocity = 0.0f;
			wheel.m_clippedInvContactDotSuspension = 1.0f / 0.1f;
		}
		else
		{
			btScalar inv = -1.f / denominator;
			wheel.m_suspensionRelativeVelocity = projVel * inv;
			wheel.m_clippedInvContactDotSuspension = inv;
		}
			
	} else
	{
		//put wheel info as in rest position
		wheel.m_raycastInfo.m_suspensionLength = wheel.getSuspensionRestLength();
		wheel.m_suspensionRelativeVelocity = 0.0f;
		wheel.m_raycastInfo.m_contactNormalWS = - wheel.m_raycastInfo.m_wheelDirectionWS;
		wheel.m_clippedInvContactDotSuspension = 1.0f;
	}

	return depth;
}


void btRaycastVehicle::updateVehicle( btScalar step )
{

	m_currentVehicleSpeedKmHour = 3.6f * getRigidBody()->getLinearVelocity().length();
	
	const btTransform& chassisTrans = getRigidBody()->getCenterOfMassTransform();
	btVector3 forwardW (
		chassisTrans.getBasis()[0][m_indexForwardAxis],
		chassisTrans.getBasis()[1][m_indexForwardAxis],
		chassisTrans.getBasis()[2][m_indexForwardAxis]);

	if (forwardW.dot(getRigidBody()->getLinearVelocity()) < 0.f)
	{
		m_currentVehicleSpeedKmHour *= -1.f;
	}

	//
	// simulate suspension
	//
	std::vector<btWheelInfo>::iterator wheelIt;
	int i=0;
	for (wheelIt = m_wheelInfo.begin();
	!(wheelIt == m_wheelInfo.end());wheelIt++,i++)
	{
		btScalar depth; 
		depth = rayCast( *wheelIt );
	}

	updateSuspension(step);

	
	for (wheelIt = m_wheelInfo.begin();
	!(wheelIt == m_wheelInfo.end());wheelIt++)
	{
		//apply suspension force
		btWheelInfo& wheel = *wheelIt;
		
		float suspensionForce = wheel.m_wheelsSuspensionForce;
		
		float gMaxSuspensionForce = 6000.f;
		if (suspensionForce > gMaxSuspensionForce)
		{
			suspensionForce = gMaxSuspensionForce;
		}
		btVector3 impulse = wheel.m_raycastInfo.m_contactNormalWS * suspensionForce * step;
		btVector3 relpos = wheel.m_raycastInfo.m_contactPointWS - getRigidBody()->getCenterOfMassPosition();
		
		getRigidBody()->applyImpulse(impulse, relpos);
	
	}
	

	
	updateFriction( step);

	
	for (wheelIt = m_wheelInfo.begin();
	!(wheelIt == m_wheelInfo.end());wheelIt++)
	{
		btWheelInfo& wheel = *wheelIt;
		btVector3 relpos = wheel.m_raycastInfo.m_hardPointWS - getRigidBody()->getCenterOfMassPosition();
		btVector3 vel = getRigidBody()->getVelocityInLocalPoint( relpos );

		if (wheel.m_raycastInfo.m_isInContact)
		{
			btVector3 fwd (
				getRigidBody()->getCenterOfMassTransform().getBasis()[0][m_indexForwardAxis],
				getRigidBody()->getCenterOfMassTransform().getBasis()[1][m_indexForwardAxis],
				getRigidBody()->getCenterOfMassTransform().getBasis()[2][m_indexForwardAxis]);

			btScalar proj = fwd.dot(wheel.m_raycastInfo.m_contactNormalWS);
			fwd -= wheel.m_raycastInfo.m_contactNormalWS * proj;

			btScalar proj2 = fwd.dot(vel);
			
			wheel.m_deltaRotation = (proj2 * step) / (wheel.m_wheelsRadius);
			wheel.m_rotation += wheel.m_deltaRotation;

		} else
		{
			wheel.m_rotation += wheel.m_deltaRotation;
		}
		
		wheel.m_deltaRotation *= 0.99f;//damping of rotation when not in contact

	}



}


void	btRaycastVehicle::setSteeringValue(btScalar steering,int wheel)
{
	assert(wheel>=0 && wheel < getNumWheels());

	btWheelInfo& wheelInfo = getWheelInfo(wheel);
	wheelInfo.m_steering = steering;
}



btScalar	btRaycastVehicle::getSteeringValue(int wheel) const
{
	return getWheelInfo(wheel).m_steering;
}


void	btRaycastVehicle::applyEngineForce(btScalar force, int wheel)
{
	assert(wheel>=0 && wheel < getNumWheels());
	btWheelInfo& wheelInfo = getWheelInfo(wheel);
	wheelInfo.m_engineForce = force;
}


const btWheelInfo&	btRaycastVehicle::getWheelInfo(int index) const
{
	ASSERT((index >= 0) && (index < 	getNumWheels()));
	
	return m_wheelInfo[index];
}

btWheelInfo&	btRaycastVehicle::getWheelInfo(int index) 
{
	ASSERT((index >= 0) && (index < 	getNumWheels()));
	
	return m_wheelInfo[index];
}

void btRaycastVehicle::setBrake(float brake,int wheelIndex)
{
	ASSERT((wheelIndex >= 0) && (wheelIndex < 	getNumWheels()));
	getWheelInfo(wheelIndex).m_brake;
}


void	btRaycastVehicle::updateSuspension(btScalar deltaTime)
{

	btScalar chassisMass = 1.f / m_chassisBody->getInvMass();
	
	for (int w_it=0; w_it<getNumWheels(); w_it++)
	{
		btWheelInfo &wheel_info = m_wheelInfo[w_it];
		
		if ( wheel_info.m_raycastInfo.m_isInContact )
		{
			btScalar force;
			//	Spring
			{
				btScalar	susp_length			= wheel_info.getSuspensionRestLength();
				btScalar	current_length = wheel_info.m_raycastInfo.m_suspensionLength;

				btScalar length_diff = (susp_length - current_length);

				force = wheel_info.m_suspensionStiffness
					* length_diff * wheel_info.m_clippedInvContactDotSuspension;
			}
		
			// Damper
			{
				btScalar projected_rel_vel = wheel_info.m_suspensionRelativeVelocity;
				{
					btScalar	susp_damping;
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
void	btRaycastVehicle::updateFriction(btScalar	timeStep)
{

		//calculate the impulse, so that the wheels don't move sidewards
		int numWheel = getNumWheels();
		if (!numWheel)
			return;


		btVector3*	forwardWS = new	btVector3[numWheel];
		btVector3*	axle = new btVector3[numWheel];
		btScalar* forwardImpulse = new btScalar[numWheel];
		btScalar* sideImpulse = new btScalar[numWheel];
		
		int numWheelsOnGround = 0;
	

		//collapse all those loops into one!
		for (int i=0;i<getNumWheels();i++)
		{
			btWheelInfo& wheelInfo = m_wheelInfo[i];
			class btRigidBody* groundObject = (class btRigidBody*) wheelInfo.m_raycastInfo.m_groundObject;
			if (groundObject)
				numWheelsOnGround++;
			sideImpulse[i] = 0.f;
			forwardImpulse[i] = 0.f;

		}
	
		{
	
			for (int i=0;i<getNumWheels();i++)
			{

				btWheelInfo& wheelInfo = m_wheelInfo[i];
					
				class btRigidBody* groundObject = (class btRigidBody*) wheelInfo.m_raycastInfo.m_groundObject;

				if (groundObject)
				{

					const btTransform& wheelTrans = getWheelTransformWS( i );

					btMatrix3x3 wheelBasis0 = wheelTrans.getBasis();
					axle[i] = btVector3(	
						wheelBasis0[0][m_indexRightAxis],
						wheelBasis0[1][m_indexRightAxis],
						wheelBasis0[2][m_indexRightAxis]);
					
					const btVector3& surfNormalWS = wheelInfo.m_raycastInfo.m_contactNormalWS;
					btScalar proj = axle[i].dot(surfNormalWS);
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

	btScalar sideFactor = 1.f;
	btScalar fwdFactor = 0.5;

	bool sliding = false;
	{
		for (int wheel =0;wheel <getNumWheels();wheel++)
		{
			btWheelInfo& wheelInfo = m_wheelInfo[wheel];
			class btRigidBody* groundObject = (class btRigidBody*) wheelInfo.m_raycastInfo.m_groundObject;


			forwardImpulse[wheel] = 0.f;
			m_wheelInfo[wheel].m_skidInfo= 1.f;

			if (groundObject)
			{
				m_wheelInfo[wheel].m_skidInfo= 1.f;
				
				btScalar maximp = wheelInfo.m_wheelsSuspensionForce * timeStep * wheelInfo.m_frictionSlip;
				btScalar maximpSide = maximp;

				btScalar maximpSquared = maximp * maximpSide;

				forwardImpulse[wheel] = wheelInfo.m_engineForce* timeStep;

				float x = (forwardImpulse[wheel] ) * fwdFactor;
				float y = (sideImpulse[wheel] ) * sideFactor;
				
				float impulseSquared = (x*x + y*y);

				if (impulseSquared > maximpSquared)
				{
					sliding = true;
					
					btScalar factor = maximp / btSqrt(impulseSquared);
					
					m_wheelInfo[wheel].m_skidInfo *= factor;
				}
			} 

		}
	}

	


		if (sliding)
		{
			for (int wheel = 0;wheel < getNumWheels(); wheel++)
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
			for (int wheel = 0;wheel<getNumWheels() ; wheel++)
			{
				btWheelInfo& wheelInfo = m_wheelInfo[wheel];

				btVector3 rel_pos = wheelInfo.m_raycastInfo.m_contactPointWS - 
						m_chassisBody->getCenterOfMassPosition();

				if (forwardImpulse[wheel] != 0.f)
				{
					m_chassisBody->applyImpulse(forwardWS[wheel]*(forwardImpulse[wheel]),rel_pos);
				}
				if (sideImpulse[wheel] != 0.f)
				{
					class btRigidBody* groundObject = (class btRigidBody*) m_wheelInfo[wheel].m_raycastInfo.m_groundObject;

					btVector3 rel_pos2 = wheelInfo.m_raycastInfo.m_contactPointWS - 
						groundObject->getCenterOfMassPosition();

					
					btVector3 sideImp = axle[wheel] * sideImpulse[wheel];

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
