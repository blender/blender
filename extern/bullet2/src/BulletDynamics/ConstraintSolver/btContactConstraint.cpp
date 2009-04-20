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


#include "btContactConstraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btVector3.h"
#include "btJacobianEntry.h"
#include "btContactSolverInfo.h"
#include "LinearMath/btMinMax.h"
#include "BulletCollision/NarrowPhaseCollision/btManifoldPoint.h"

#define ASSERT2 btAssert

#define USE_INTERNAL_APPLY_IMPULSE 1


//bilateral constraint between two dynamic objects
void resolveSingleBilateral(btRigidBody& body1, const btVector3& pos1,
                      btRigidBody& body2, const btVector3& pos2,
                      btScalar distance, const btVector3& normal,btScalar& impulse ,btScalar timeStep)
{
	(void)timeStep;
	(void)distance;


	btScalar normalLenSqr = normal.length2();
	ASSERT2(btFabs(normalLenSqr) < btScalar(1.1));
	if (normalLenSqr > btScalar(1.1))
	{
		impulse = btScalar(0.);
		return;
	}
	btVector3 rel_pos1 = pos1 - body1.getCenterOfMassPosition(); 
	btVector3 rel_pos2 = pos2 - body2.getCenterOfMassPosition();
	//this jacobian entry could be re-used for all iterations
	
	btVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
	btVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
	btVector3 vel = vel1 - vel2;
	

	   btJacobianEntry jac(body1.getCenterOfMassTransform().getBasis().transpose(),
		body2.getCenterOfMassTransform().getBasis().transpose(),
		rel_pos1,rel_pos2,normal,body1.getInvInertiaDiagLocal(),body1.getInvMass(),
		body2.getInvInertiaDiagLocal(),body2.getInvMass());

	btScalar jacDiagAB = jac.getDiagonal();
	btScalar jacDiagABInv = btScalar(1.) / jacDiagAB;
	
	  btScalar rel_vel = jac.getRelativeVelocity(
		body1.getLinearVelocity(),
		body1.getCenterOfMassTransform().getBasis().transpose() * body1.getAngularVelocity(),
		body2.getLinearVelocity(),
		body2.getCenterOfMassTransform().getBasis().transpose() * body2.getAngularVelocity()); 
	btScalar a;
	a=jacDiagABInv;


	rel_vel = normal.dot(vel);
	
	//todo: move this into proper structure
	btScalar contactDamping = btScalar(0.2);

#ifdef ONLY_USE_LINEAR_MASS
	btScalar massTerm = btScalar(1.) / (body1.getInvMass() + body2.getInvMass());
	impulse = - contactDamping * rel_vel * massTerm;
#else	
	btScalar velocityImpulse = -contactDamping * rel_vel * jacDiagABInv;
	impulse = velocityImpulse;
#endif
}



//response  between two dynamic objects with friction
btScalar resolveSingleCollision(
	btRigidBody& body1,
	btRigidBody& body2,
	btManifoldPoint& contactPoint,
	const btContactSolverInfo& solverInfo)
{

	const btVector3& pos1_ = contactPoint.getPositionWorldOnA();
	const btVector3& pos2_ = contactPoint.getPositionWorldOnB();
   	const btVector3& normal = contactPoint.m_normalWorldOnB;

	//constant over all iterations
	btVector3 rel_pos1 = pos1_ - body1.getCenterOfMassPosition(); 
	btVector3 rel_pos2 = pos2_ - body2.getCenterOfMassPosition();
	
	btVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
	btVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
	btVector3 vel = vel1 - vel2;
	btScalar rel_vel;
	rel_vel = normal.dot(vel);
	
	btScalar Kfps = btScalar(1.) / solverInfo.m_timeStep ;

	// btScalar damping = solverInfo.m_damping ;
	btScalar Kerp = solverInfo.m_erp;
	btScalar Kcor = Kerp *Kfps;

	btConstraintPersistentData* cpd = (btConstraintPersistentData*) contactPoint.m_userPersistentData;
	btAssert(cpd);
	btScalar distance = cpd->m_penetration;
	btScalar positionalError = Kcor *-distance;
	btScalar velocityError = cpd->m_restitution - rel_vel;// * damping;

	btScalar penetrationImpulse = positionalError * cpd->m_jacDiagABInv;

	btScalar	velocityImpulse = velocityError * cpd->m_jacDiagABInv;

	btScalar normalImpulse = penetrationImpulse+velocityImpulse;
	
	// See Erin Catto's GDC 2006 paper: Clamp the accumulated impulse
	btScalar oldNormalImpulse = cpd->m_appliedImpulse;
	btScalar sum = oldNormalImpulse + normalImpulse;
	cpd->m_appliedImpulse = btScalar(0.) > sum ? btScalar(0.): sum;

	normalImpulse = cpd->m_appliedImpulse - oldNormalImpulse;

#ifdef USE_INTERNAL_APPLY_IMPULSE
	if (body1.getInvMass())
	{
		body1.internalApplyImpulse(contactPoint.m_normalWorldOnB*body1.getInvMass(),cpd->m_angularComponentA,normalImpulse);
	}
	if (body2.getInvMass())
	{
		body2.internalApplyImpulse(contactPoint.m_normalWorldOnB*body2.getInvMass(),cpd->m_angularComponentB,-normalImpulse);
	}
#else //USE_INTERNAL_APPLY_IMPULSE
	body1.applyImpulse(normal*(normalImpulse), rel_pos1);
	body2.applyImpulse(-normal*(normalImpulse), rel_pos2);
#endif //USE_INTERNAL_APPLY_IMPULSE

	return normalImpulse;
}


btScalar resolveSingleFriction(
	btRigidBody& body1,
	btRigidBody& body2,
	btManifoldPoint& contactPoint,
	const btContactSolverInfo& solverInfo)
{

	(void)solverInfo;

	const btVector3& pos1 = contactPoint.getPositionWorldOnA();
	const btVector3& pos2 = contactPoint.getPositionWorldOnB();

	btVector3 rel_pos1 = pos1 - body1.getCenterOfMassPosition(); 
	btVector3 rel_pos2 = pos2 - body2.getCenterOfMassPosition();
	
	btConstraintPersistentData* cpd = (btConstraintPersistentData*) contactPoint.m_userPersistentData;
	btAssert(cpd);

	btScalar combinedFriction = cpd->m_friction;
	
	btScalar limit = cpd->m_appliedImpulse * combinedFriction;
	
	if (cpd->m_appliedImpulse>btScalar(0.))
	//friction
	{
		//apply friction in the 2 tangential directions
		
		// 1st tangent
		btVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
		btVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
		btVector3 vel = vel1 - vel2;
	
		btScalar j1,j2;

		{
				
			btScalar vrel = cpd->m_frictionWorldTangential0.dot(vel);

			// calculate j that moves us to zero relative velocity
			j1 = -vrel * cpd->m_jacDiagABInvTangent0;
			btScalar oldTangentImpulse = cpd->m_accumulatedTangentImpulse0;
			cpd->m_accumulatedTangentImpulse0 = oldTangentImpulse + j1;
			btSetMin(cpd->m_accumulatedTangentImpulse0, limit);
			btSetMax(cpd->m_accumulatedTangentImpulse0, -limit);
			j1 = cpd->m_accumulatedTangentImpulse0 - oldTangentImpulse;

		}
		{
			// 2nd tangent

			btScalar vrel = cpd->m_frictionWorldTangential1.dot(vel);
			
			// calculate j that moves us to zero relative velocity
			j2 = -vrel * cpd->m_jacDiagABInvTangent1;
			btScalar oldTangentImpulse = cpd->m_accumulatedTangentImpulse1;
			cpd->m_accumulatedTangentImpulse1 = oldTangentImpulse + j2;
			btSetMin(cpd->m_accumulatedTangentImpulse1, limit);
			btSetMax(cpd->m_accumulatedTangentImpulse1, -limit);
			j2 = cpd->m_accumulatedTangentImpulse1 - oldTangentImpulse;
		}

#ifdef USE_INTERNAL_APPLY_IMPULSE
	if (body1.getInvMass())
	{
		body1.internalApplyImpulse(cpd->m_frictionWorldTangential0*body1.getInvMass(),cpd->m_frictionAngularComponent0A,j1);
		body1.internalApplyImpulse(cpd->m_frictionWorldTangential1*body1.getInvMass(),cpd->m_frictionAngularComponent1A,j2);
	}
	if (body2.getInvMass())
	{
		body2.internalApplyImpulse(cpd->m_frictionWorldTangential0*body2.getInvMass(),cpd->m_frictionAngularComponent0B,-j1);
		body2.internalApplyImpulse(cpd->m_frictionWorldTangential1*body2.getInvMass(),cpd->m_frictionAngularComponent1B,-j2);	
	}
#else //USE_INTERNAL_APPLY_IMPULSE
	body1.applyImpulse((j1 * cpd->m_frictionWorldTangential0)+(j2 * cpd->m_frictionWorldTangential1), rel_pos1);
	body2.applyImpulse((j1 * -cpd->m_frictionWorldTangential0)+(j2 * -cpd->m_frictionWorldTangential1), rel_pos2);
#endif //USE_INTERNAL_APPLY_IMPULSE


	} 
	return cpd->m_appliedImpulse;
}


btScalar resolveSingleFrictionOriginal(
	btRigidBody& body1,
	btRigidBody& body2,
	btManifoldPoint& contactPoint,
	const btContactSolverInfo& solverInfo);

btScalar resolveSingleFrictionOriginal(
	btRigidBody& body1,
	btRigidBody& body2,
	btManifoldPoint& contactPoint,
	const btContactSolverInfo& solverInfo)
{

	(void)solverInfo;

	const btVector3& pos1 = contactPoint.getPositionWorldOnA();
	const btVector3& pos2 = contactPoint.getPositionWorldOnB();

	btVector3 rel_pos1 = pos1 - body1.getCenterOfMassPosition(); 
	btVector3 rel_pos2 = pos2 - body2.getCenterOfMassPosition();
	
	btConstraintPersistentData* cpd = (btConstraintPersistentData*) contactPoint.m_userPersistentData;
	btAssert(cpd);

	btScalar combinedFriction = cpd->m_friction;
	
	btScalar limit = cpd->m_appliedImpulse * combinedFriction;
	//if (contactPoint.m_appliedImpulse>btScalar(0.))
	//friction
	{
		//apply friction in the 2 tangential directions
		
		{
			// 1st tangent
			btVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
			btVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
			btVector3 vel = vel1 - vel2;
			
			btScalar vrel = cpd->m_frictionWorldTangential0.dot(vel);

			// calculate j that moves us to zero relative velocity
			btScalar j = -vrel * cpd->m_jacDiagABInvTangent0;
			btScalar total = cpd->m_accumulatedTangentImpulse0 + j;
			btSetMin(total, limit);
			btSetMax(total, -limit);
			j = total - cpd->m_accumulatedTangentImpulse0;
			cpd->m_accumulatedTangentImpulse0 = total;
			body1.applyImpulse(j * cpd->m_frictionWorldTangential0, rel_pos1);
			body2.applyImpulse(j * -cpd->m_frictionWorldTangential0, rel_pos2);
		}

				
		{
			// 2nd tangent
			btVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
			btVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
			btVector3 vel = vel1 - vel2;

			btScalar vrel = cpd->m_frictionWorldTangential1.dot(vel);
			
			// calculate j that moves us to zero relative velocity
			btScalar j = -vrel * cpd->m_jacDiagABInvTangent1;
			btScalar total = cpd->m_accumulatedTangentImpulse1 + j;
			btSetMin(total, limit);
			btSetMax(total, -limit);
			j = total - cpd->m_accumulatedTangentImpulse1;
			cpd->m_accumulatedTangentImpulse1 = total;
			body1.applyImpulse(j * cpd->m_frictionWorldTangential1, rel_pos1);
			body2.applyImpulse(j * -cpd->m_frictionWorldTangential1, rel_pos2);
		}
	} 
	return cpd->m_appliedImpulse;
}


//velocity + friction
//response  between two dynamic objects with friction
btScalar resolveSingleCollisionCombined(
	btRigidBody& body1,
	btRigidBody& body2,
	btManifoldPoint& contactPoint,
	const btContactSolverInfo& solverInfo)
{

	const btVector3& pos1 = contactPoint.getPositionWorldOnA();
	const btVector3& pos2 = contactPoint.getPositionWorldOnB();
   	const btVector3& normal = contactPoint.m_normalWorldOnB;

	btVector3 rel_pos1 = pos1 - body1.getCenterOfMassPosition(); 
	btVector3 rel_pos2 = pos2 - body2.getCenterOfMassPosition();
	
	btVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
	btVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
	btVector3 vel = vel1 - vel2;
	btScalar rel_vel;
	rel_vel = normal.dot(vel);
	
	btScalar Kfps = btScalar(1.) / solverInfo.m_timeStep ;

	//btScalar damping = solverInfo.m_damping ;
	btScalar Kerp = solverInfo.m_erp;
	btScalar Kcor = Kerp *Kfps;

	btConstraintPersistentData* cpd = (btConstraintPersistentData*) contactPoint.m_userPersistentData;
	btAssert(cpd);
	btScalar distance = cpd->m_penetration;
	btScalar positionalError = Kcor *-distance;
	btScalar velocityError = cpd->m_restitution - rel_vel;// * damping;

	btScalar penetrationImpulse = positionalError * cpd->m_jacDiagABInv;

	btScalar	velocityImpulse = velocityError * cpd->m_jacDiagABInv;

	btScalar normalImpulse = penetrationImpulse+velocityImpulse;
	
	// See Erin Catto's GDC 2006 paper: Clamp the accumulated impulse
	btScalar oldNormalImpulse = cpd->m_appliedImpulse;
	btScalar sum = oldNormalImpulse + normalImpulse;
	cpd->m_appliedImpulse = btScalar(0.) > sum ? btScalar(0.): sum;

	normalImpulse = cpd->m_appliedImpulse - oldNormalImpulse;


#ifdef USE_INTERNAL_APPLY_IMPULSE
	if (body1.getInvMass())
	{
		body1.internalApplyImpulse(contactPoint.m_normalWorldOnB*body1.getInvMass(),cpd->m_angularComponentA,normalImpulse);
	}
	if (body2.getInvMass())
	{
		body2.internalApplyImpulse(contactPoint.m_normalWorldOnB*body2.getInvMass(),cpd->m_angularComponentB,-normalImpulse);
	}
#else //USE_INTERNAL_APPLY_IMPULSE
	body1.applyImpulse(normal*(normalImpulse), rel_pos1);
	body2.applyImpulse(-normal*(normalImpulse), rel_pos2);
#endif //USE_INTERNAL_APPLY_IMPULSE

	{
		//friction
		btVector3 vel1 = body1.getVelocityInLocalPoint(rel_pos1);
		btVector3 vel2 = body2.getVelocityInLocalPoint(rel_pos2);
		btVector3 vel = vel1 - vel2;
	  
		rel_vel = normal.dot(vel);


		btVector3 lat_vel = vel - normal * rel_vel;
		btScalar lat_rel_vel = lat_vel.length();

		btScalar combinedFriction = cpd->m_friction;

		if (cpd->m_appliedImpulse > 0)
		if (lat_rel_vel > SIMD_EPSILON)
		{
			lat_vel /= lat_rel_vel;
			btVector3 temp1 = body1.getInvInertiaTensorWorld() * rel_pos1.cross(lat_vel);
			btVector3 temp2 = body2.getInvInertiaTensorWorld() * rel_pos2.cross(lat_vel);
			btScalar friction_impulse = lat_rel_vel /
				(body1.getInvMass() + body2.getInvMass() + lat_vel.dot(temp1.cross(rel_pos1) + temp2.cross(rel_pos2)));
			btScalar normal_impulse = cpd->m_appliedImpulse * combinedFriction;

			btSetMin(friction_impulse, normal_impulse);
			btSetMax(friction_impulse, -normal_impulse);
			body1.applyImpulse(lat_vel * -friction_impulse, rel_pos1);
			body2.applyImpulse(lat_vel * friction_impulse, rel_pos2);
		}
	}



	return normalImpulse;
}

btScalar resolveSingleFrictionEmpty(
	btRigidBody& body1,
	btRigidBody& body2,
	btManifoldPoint& contactPoint,
	const btContactSolverInfo& solverInfo);

btScalar resolveSingleFrictionEmpty(
	btRigidBody& body1,
	btRigidBody& body2,
	btManifoldPoint& contactPoint,
	const btContactSolverInfo& solverInfo)
{
	(void)contactPoint;
	(void)body1;
	(void)body2;
	(void)solverInfo;


	return btScalar(0.);
}

