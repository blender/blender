/*
 * Copyright (c) 2005 Erwin Coumans http://www.erwincoumans.com
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#include "ContactConstraint.h"
#include "Dynamics/RigidBody.h"
#include "SimdVector3.h"
#include "JacobianEntry.h"
#include "ContactSolverInfo.h"
#include "GEN_MinMax.h"

#define ASSERT2 assert


static SimdScalar ContactThreshold = -10.0f;  

float useGlobalSettingContacts = false;//true;

SimdScalar contactDamping = 0.2f;
SimdScalar contactTau = .02f;//0.02f;//*0.02f;



SimdScalar restitutionCurve(SimdScalar rel_vel, SimdScalar restitution)
{
	return 0.f;
//	return restitution * GEN_min(1.0f, rel_vel / ContactThreshold);
}

float MAX_FRICTION = 100.f;

SimdScalar	calculateCombinedFriction(RigidBody& body0,RigidBody& body1)
{
	SimdScalar friction = body0.getFriction() * body1.getFriction();
	if (friction < 0.f)
		friction = 0.f;
	if (friction > MAX_FRICTION)
		friction = MAX_FRICTION;
	return friction;

}

void	applyFrictionInContactPointOld(RigidBody& body1, const SimdVector3& pos1,
                      RigidBody& body2, const SimdVector3& pos2,
		const SimdVector3& normal,float normalImpulse, 
		const ContactSolverInfo& solverInfo)
{


	if (normalImpulse>0.f)
	{
		SimdVector3 rel_pos1 = pos1 - body1.getCenterOfMassPosition(); 
		SimdVector3 rel_pos2 = pos2 - body2.getCenterOfMassPosition();

		SimdVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
		SimdVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
		SimdVector3 vel = vel1 - vel2;
	
		SimdScalar rel_vel = normal.dot(vel);

		float combinedFriction = calculateCombinedFriction(body1,body2);

#define PER_CONTACT_FRICTION
#ifdef PER_CONTACT_FRICTION
		SimdVector3 lat_vel = vel - normal * rel_vel;
		SimdScalar lat_rel_vel = lat_vel.length();

		if (lat_rel_vel > SIMD_EPSILON)
		{
			lat_vel /= lat_rel_vel;
			SimdVector3 temp1 = body1.getInvInertiaTensorWorld() * rel_pos1.cross(lat_vel); 
			SimdVector3 temp2 = body2.getInvInertiaTensorWorld() * rel_pos2.cross(lat_vel); 
			SimdScalar frictionMaxImpulse = lat_rel_vel / 
				(body1.getInvMass() + body2.getInvMass() + lat_vel.dot(temp1.cross(rel_pos1) + temp2.cross(rel_pos2)));
			SimdScalar frictionImpulse = (normalImpulse) * combinedFriction;
			GEN_set_min(frictionImpulse,frictionMaxImpulse );
			
			body1.applyImpulse(lat_vel * -frictionImpulse, rel_pos1);
			body2.applyImpulse(lat_vel * frictionImpulse, rel_pos2);
			
		}
#endif
		}
}


//bilateral constraint between two dynamic objects
void resolveSingleBilateral(RigidBody& body1, const SimdVector3& pos1,
                      RigidBody& body2, const SimdVector3& pos2,
                      SimdScalar depth, const SimdVector3& normal,SimdScalar& impulse ,float timeStep)
{
	float normalLenSqr = normal.length2();
	ASSERT2(fabs(normalLenSqr) < 1.1f);
	if (normalLenSqr > 1.1f)
	{
		impulse = 0.f;
		return;
	}
	SimdVector3 rel_pos1 = pos1 - body1.getCenterOfMassPosition(); 
	SimdVector3 rel_pos2 = pos2 - body2.getCenterOfMassPosition();
	//this jacobian entry could be re-used for all iterations
	
	SimdVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
	SimdVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
	SimdVector3 vel = vel1 - vel2;
	
	SimdScalar rel_vel;

	/*
	
	  JacobianEntry jac(body1.getCenterOfMassTransform().getBasis().transpose(),
		body2.getCenterOfMassTransform().getBasis().transpose(),
		rel_pos1,rel_pos2,normal,body1.getInvInertiaDiagLocal(),body1.getInvMass(),
		body2.getInvInertiaDiagLocal(),body2.getInvMass());

	SimdScalar jacDiagAB = jac.getDiagonal();
	SimdScalar jacDiagABInv = 1.f / jacDiagAB;
	
	  SimdScalar rel_vel = jac.getRelativeVelocity(
		body1.getLinearVelocity(),
		body1.getCenterOfMassTransform().getBasis().transpose() * body1.getAngularVelocity(),
		body2.getLinearVelocity(),
		body2.getCenterOfMassTransform().getBasis().transpose() * body2.getAngularVelocity()); 
	float a;
	a=jacDiagABInv;

	*/
	rel_vel = normal.dot(vel);
	



	/*int color = 255+255*256;

	DrawRasterizerLine(pos1,pos1+normal,color);
*/

		
	SimdScalar massTerm = 1.f / (body1.getInvMass() + body2.getInvMass());

	impulse = - contactDamping * rel_vel * massTerm;//jacDiagABInv;

	//SimdScalar velocityImpulse = -contactDamping * rel_vel * jacDiagABInv;
	//impulse = velocityImpulse;

}





//velocity + friction
//response  between two dynamic objects with friction
float resolveSingleCollision(RigidBody& body1, const SimdVector3& pos1,
                      RigidBody& body2, const SimdVector3& pos2,
                      SimdScalar depth, const SimdVector3& normal, 
					  		const ContactSolverInfo& solverInfo
					  )
{
	
	float normalLenSqr = normal.length2();
	ASSERT2(fabs(normalLenSqr) < 1.1f);
	if (normalLenSqr > 1.1f)
		return 0.f;

	SimdVector3 rel_pos1 = pos1 - body1.getCenterOfMassPosition(); 
	SimdVector3 rel_pos2 = pos2 - body2.getCenterOfMassPosition();
	//this jacobian entry could be re-used for all iterations
	
	SimdVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
	SimdVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
	SimdVector3 vel = vel1 - vel2;
	SimdScalar rel_vel;
	rel_vel = normal.dot(vel);
	
//	if (rel_vel< 0.f)//-SIMD_EPSILON) 
//	{
	float combinedRestitution = body1.getRestitution() * body2.getRestitution();

	SimdScalar rest = restitutionCurve(rel_vel, combinedRestitution);

	SimdScalar timeCorrection =  360.f*solverInfo.m_timeStep;
	float damping = solverInfo.m_damping ;
	float tau = solverInfo.m_tau;

	if (useGlobalSettingContacts)
	{
		damping = contactDamping;
		tau = contactTau;
	} 

	if (depth < 0.f)
		return 0.f;//bdepth = 0.f;

	SimdScalar penetrationImpulse = (depth*tau*timeCorrection);// * massTerm;//jacDiagABInv
	
	SimdScalar velocityImpulse = -(1.0f + rest) * damping * rel_vel;

	SimdScalar impulse = penetrationImpulse + velocityImpulse;
	SimdVector3 temp1 = body1.getInvInertiaTensorWorld() * rel_pos1.cross(normal); 
		SimdVector3 temp2 = body2.getInvInertiaTensorWorld() * rel_pos2.cross(normal); 
		impulse /=
			(body1.getInvMass() + body2.getInvMass() + normal.dot(temp1.cross(rel_pos1) + temp2.cross(rel_pos2)));
	
	if (impulse > 0.f)
	{
	
		body1.applyImpulse(normal*(impulse), rel_pos1);
		body2.applyImpulse(-normal*(impulse), rel_pos2);
	} else
	{
		impulse = 0.f;
	}

	return impulse;//velocityImpulse;//impulse;
	
}



//velocity + friction
//response  between two dynamic objects with friction
float resolveSingleCollisionWithFriction(

		RigidBody& body1, 
		const SimdVector3& pos1,
        RigidBody& body2, 
		const SimdVector3& pos2,
        SimdScalar depth, 
		const SimdVector3& normal, 

		const ContactSolverInfo& solverInfo
		)
{
	float normalLenSqr = normal.length2();
	ASSERT2(fabs(normalLenSqr) < 1.1f);
	if (normalLenSqr > 1.1f)
		return 0.f;

	SimdVector3 rel_pos1 = pos1 - body1.getCenterOfMassPosition(); 
	SimdVector3 rel_pos2 = pos2 - body2.getCenterOfMassPosition();
	//this jacobian entry could be re-used for all iterations
	
	JacobianEntry jac(body1.getCenterOfMassTransform().getBasis().transpose(),
		body2.getCenterOfMassTransform().getBasis().transpose(),
		rel_pos1,rel_pos2,normal,body1.getInvInertiaDiagLocal(),body1.getInvMass(),
		body2.getInvInertiaDiagLocal(),body2.getInvMass());

	SimdScalar jacDiagAB = jac.getDiagonal();
	SimdScalar jacDiagABInv = 1.f / jacDiagAB;
	SimdVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
	SimdVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
	SimdVector3 vel = vel1 - vel2;
SimdScalar rel_vel;
	/*	rel_vel = jac.getRelativeVelocity(
		body1.getLinearVelocity(),
		body1.getTransform().getBasis().transpose() * body1.getAngularVelocity(),
		body2.getLinearVelocity(),
		body2.getTransform().getBasis().transpose() * body2.getAngularVelocity()); 
*/	
	rel_vel = normal.dot(vel);
	
//	if (rel_vel< 0.f)//-SIMD_EPSILON) 
//	{
	float combinedRestitution = body1.getRestitution() * body2.getRestitution();

	SimdScalar rest = restitutionCurve(rel_vel, combinedRestitution);
//	SimdScalar massTerm = 1.f / (body1.getInvMass() + body2.getInvMass());

	SimdScalar timeCorrection = 0.5f / solverInfo.m_timeStep ;

	float damping = solverInfo.m_damping ;
	float tau = solverInfo.m_tau;

	if (useGlobalSettingContacts)
	{
		damping = contactDamping;
		tau = contactTau;
	} 
	SimdScalar penetrationImpulse = (depth* tau *timeCorrection)  * jacDiagABInv;
		
		if (penetrationImpulse  < 0.f)
			penetrationImpulse  = 0.f;



	SimdScalar velocityImpulse = -(1.0f + rest) * damping * rel_vel * jacDiagABInv;

	SimdScalar friction_impulse = 0.f;

	if (velocityImpulse <= 0.f)
		velocityImpulse = 0.f;

//	SimdScalar impulse = penetrationImpulse + velocityImpulse;
	//if (impulse > 0.f)
	{
//		SimdVector3 impulse_vector = normal * impulse;
		body1.applyImpulse(normal*(velocityImpulse+penetrationImpulse), rel_pos1);
		
		body2.applyImpulse(-normal*(velocityImpulse+penetrationImpulse), rel_pos2);
		
		//friction

		{
		
				SimdVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
	SimdVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
	SimdVector3 vel = vel1 - vel2;
	
	rel_vel = normal.dot(vel);

#define PER_CONTACT_FRICTION
#ifdef PER_CONTACT_FRICTION
		SimdVector3 lat_vel = vel - normal * rel_vel;
		SimdScalar lat_rel_vel = lat_vel.length();

		float combinedFriction = calculateCombinedFriction(body1,body2);

		if (lat_rel_vel > SIMD_EPSILON)
		{
			lat_vel /= lat_rel_vel;
			SimdVector3 temp1 = body1.getInvInertiaTensorWorld() * rel_pos1.cross(lat_vel); 
			SimdVector3 temp2 = body2.getInvInertiaTensorWorld() * rel_pos2.cross(lat_vel); 
			 friction_impulse = lat_rel_vel / 
				(body1.getInvMass() + body2.getInvMass() + lat_vel.dot(temp1.cross(rel_pos1) + temp2.cross(rel_pos2)));
			SimdScalar normal_impulse = (penetrationImpulse+
				velocityImpulse) * combinedFriction;
			GEN_set_min(friction_impulse, normal_impulse);
			
			body1.applyImpulse(lat_vel * -friction_impulse, rel_pos1);
			body2.applyImpulse(lat_vel * friction_impulse, rel_pos2);
			
		}
#endif
		}
	} 
	return velocityImpulse + friction_impulse;
}
