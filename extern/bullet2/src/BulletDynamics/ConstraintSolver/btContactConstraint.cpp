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



btContactConstraint::btContactConstraint(btPersistentManifold* contactManifold,btRigidBody& rbA,btRigidBody& rbB)
:btTypedConstraint(CONTACT_CONSTRAINT_TYPE,rbA,rbB),
	m_contactManifold(*contactManifold)
{

}

btContactConstraint::~btContactConstraint()
{

}

void	btContactConstraint::setContactManifold(btPersistentManifold* contactManifold)
{
	m_contactManifold = *contactManifold;
}

void btContactConstraint::getInfo1 (btConstraintInfo1* info)
{

}

void btContactConstraint::getInfo2 (btConstraintInfo2* info)
{

}

void	btContactConstraint::buildJacobian()
{

}





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




