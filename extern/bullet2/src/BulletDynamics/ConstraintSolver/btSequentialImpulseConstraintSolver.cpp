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

//#define COMPUTE_IMPULSE_DENOM 1
//It is not necessary (redundant) to refresh contact manifolds, this refresh has been moved to the collision algorithms.
//#define FORCE_REFESH_CONTACT_MANIFOLDS 1

#include "btSequentialImpulseConstraintSolver.h"
#include "BulletCollision/NarrowPhaseCollision/btPersistentManifold.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "btContactConstraint.h"
#include "btSolve2LinearConstraint.h"
#include "btContactSolverInfo.h"
#include "LinearMath/btIDebugDraw.h"
#include "btJacobianEntry.h"
#include "LinearMath/btMinMax.h"
#include "BulletDynamics/ConstraintSolver/btTypedConstraint.h"
#include <new>
#include "LinearMath/btStackAlloc.h"
#include "LinearMath/btQuickprof.h"
#include "btSolverBody.h"
#include "btSolverConstraint.h"

#include "LinearMath/btAlignedObjectArray.h"

#ifdef USE_PROFILE
#include "LinearMath/btQuickprof.h"
#endif //USE_PROFILE

int totalCpd = 0;

int	gTotalContactPoints = 0;

struct	btOrderIndex
{
	int	m_manifoldIndex;
	int	m_pointIndex;
};



#define SEQUENTIAL_IMPULSE_MAX_SOLVER_POINTS 16384
static btOrderIndex	gOrder[SEQUENTIAL_IMPULSE_MAX_SOLVER_POINTS];


unsigned long btSequentialImpulseConstraintSolver::btRand2()
{
  m_btSeed2 = (1664525L*m_btSeed2 + 1013904223L) & 0xffffffff;
  return m_btSeed2;
}



//See ODE: adam's all-int straightforward(?) dRandInt (0..n-1)
int btSequentialImpulseConstraintSolver::btRandInt2 (int n)
{
  // seems good; xor-fold and modulus
  const unsigned long un = n;
  unsigned long r = btRand2();

  // note: probably more aggressive than it needs to be -- might be
  //       able to get away without one or two of the innermost branches.
  if (un <= 0x00010000UL) {
    r ^= (r >> 16);
    if (un <= 0x00000100UL) {
      r ^= (r >> 8);
      if (un <= 0x00000010UL) {
        r ^= (r >> 4);
        if (un <= 0x00000004UL) {
          r ^= (r >> 2);
          if (un <= 0x00000002UL) {
            r ^= (r >> 1);
          }
        }
     }
    }
   }

  return (int) (r % un);
}





bool  MyContactDestroyedCallback(void* userPersistentData)
{
	assert (userPersistentData);
	btConstraintPersistentData* cpd = (btConstraintPersistentData*)userPersistentData;
	btAlignedFree(cpd);
	totalCpd--;
	//printf("totalCpd = %i. DELETED Ptr %x\n",totalCpd,userPersistentData);
	return true;
}



btSequentialImpulseConstraintSolver::btSequentialImpulseConstraintSolver()
:m_solverMode(SOLVER_RANDMIZE_ORDER | SOLVER_CACHE_FRIENDLY), //not using SOLVER_USE_WARMSTARTING,
m_btSeed2(0)
{
	gContactDestroyedCallback = &MyContactDestroyedCallback;

	//initialize default friction/contact funcs
	int i,j;
	for (i=0;i<MAX_CONTACT_SOLVER_TYPES;i++)
		for (j=0;j<MAX_CONTACT_SOLVER_TYPES;j++)
		{

			m_contactDispatch[i][j] = resolveSingleCollision;
			m_frictionDispatch[i][j] = resolveSingleFriction;
		}
}

btSequentialImpulseConstraintSolver::~btSequentialImpulseConstraintSolver()
{

}

void	initSolverBody(btSolverBody* solverBody, btRigidBody* rigidbody)
{
/*	int size = sizeof(btSolverBody);
	int sizeofrb = sizeof(btRigidBody);
	int sizemanifold = sizeof(btPersistentManifold);
	int sizeofmp = sizeof(btManifoldPoint);
	int sizeofPersistData = sizeof (btConstraintPersistentData);
*/

	solverBody->m_angularVelocity = rigidbody->getAngularVelocity();
	solverBody->m_centerOfMassPosition = rigidbody->getCenterOfMassPosition();
	solverBody->m_friction = rigidbody->getFriction();
//	solverBody->m_invInertiaWorld = rigidbody->getInvInertiaTensorWorld();
	solverBody->m_invMass = rigidbody->getInvMass();
	solverBody->m_linearVelocity = rigidbody->getLinearVelocity();
	solverBody->m_originalBody = rigidbody;
	solverBody->m_angularFactor = rigidbody->getAngularFactor();
}

btScalar penetrationResolveFactor = btScalar(0.9);
btScalar restitutionCurve(btScalar rel_vel, btScalar restitution)
{
	btScalar rest = restitution * -rel_vel;
	return rest;
}






//velocity + friction
//response  between two dynamic objects with friction
//SIMD_FORCE_INLINE 
btScalar resolveSingleCollisionCombinedCacheFriendly(
	btSolverBody& body1,
	btSolverBody& body2,
	const btSolverConstraint& contactConstraint,
	const btContactSolverInfo& solverInfo)
{
	(void)solverInfo;

	btScalar normalImpulse;
	
	//  Optimized version of projected relative velocity, use precomputed cross products with normal
	//	body1.getVelocityInLocalPoint(contactConstraint.m_rel_posA,vel1);
	//	body2.getVelocityInLocalPoint(contactConstraint.m_rel_posB,vel2);
	//	btVector3 vel = vel1 - vel2;
	//	btScalar  rel_vel = contactConstraint.m_contactNormal.dot(vel);

	btScalar rel_vel;
	btScalar vel1Dotn = contactConstraint.m_contactNormal.dot(body1.m_linearVelocity) 
				+ contactConstraint.m_relpos1CrossNormal.dot(body1.m_angularVelocity);
	btScalar vel2Dotn = contactConstraint.m_contactNormal.dot(body2.m_linearVelocity) 
				+ contactConstraint.m_relpos2CrossNormal.dot(body2.m_angularVelocity);

	rel_vel = vel1Dotn-vel2Dotn;


	btScalar positionalError = contactConstraint.m_penetration;
	btScalar velocityError = contactConstraint.m_restitution - rel_vel;// * damping;

	btScalar penetrationImpulse = positionalError * contactConstraint.m_jacDiagABInv;
	btScalar	velocityImpulse = velocityError * contactConstraint.m_jacDiagABInv;
	normalImpulse = penetrationImpulse+velocityImpulse;
	
	// See Erin Catto's GDC 2006 paper: Clamp the accumulated impulse
	btScalar oldNormalImpulse = contactConstraint.m_appliedImpulse;
	btScalar sum = oldNormalImpulse + normalImpulse;
	contactConstraint.m_appliedImpulse = btScalar(0.) > sum ? btScalar(0.): sum;

	btScalar oldVelocityImpulse = contactConstraint.m_appliedVelocityImpulse;
	btScalar velocitySum = oldVelocityImpulse + velocityImpulse;
	contactConstraint.m_appliedVelocityImpulse = btScalar(0.) > velocitySum ? btScalar(0.): velocitySum;

	normalImpulse = contactConstraint.m_appliedImpulse - oldNormalImpulse;

	if (body1.m_invMass)
	{
		body1.internalApplyImpulse(contactConstraint.m_contactNormal*body1.m_invMass,
			contactConstraint.m_angularComponentA,normalImpulse);
	}
	if (body2.m_invMass)
	{
		body2.internalApplyImpulse(contactConstraint.m_contactNormal*body2.m_invMass,
			contactConstraint.m_angularComponentB,-normalImpulse);
	}



	

	return normalImpulse;
}


#ifndef NO_FRICTION_TANGENTIALS

//SIMD_FORCE_INLINE 
btScalar resolveSingleFrictionCacheFriendly(
	btSolverBody& body1,
	btSolverBody& body2,
	const btSolverConstraint& contactConstraint,
	const btContactSolverInfo& solverInfo,
	btScalar appliedNormalImpulse)
{
	(void)solverInfo;

	
	const btScalar combinedFriction = contactConstraint.m_friction;
	
	const btScalar limit = appliedNormalImpulse * combinedFriction;
	
	if (appliedNormalImpulse>btScalar(0.))
	//friction
	{
		
		btScalar j1;
		{

			btScalar rel_vel;
			const btScalar vel1Dotn = contactConstraint.m_contactNormal.dot(body1.m_linearVelocity) 
						+ contactConstraint.m_relpos1CrossNormal.dot(body1.m_angularVelocity);
			const btScalar vel2Dotn = contactConstraint.m_contactNormal.dot(body2.m_linearVelocity) 
				+ contactConstraint.m_relpos2CrossNormal.dot(body2.m_angularVelocity);
			rel_vel = vel1Dotn-vel2Dotn;

			// calculate j that moves us to zero relative velocity
			j1 = -rel_vel * contactConstraint.m_jacDiagABInv;
#define CLAMP_ACCUMULATED_FRICTION_IMPULSE 1
#ifdef CLAMP_ACCUMULATED_FRICTION_IMPULSE
			btScalar oldTangentImpulse = contactConstraint.m_appliedImpulse;
			contactConstraint.m_appliedImpulse = oldTangentImpulse + j1;
			
			if (limit < contactConstraint.m_appliedImpulse)
			{
				contactConstraint.m_appliedImpulse = limit;
			} else
			{
				if (contactConstraint.m_appliedImpulse < -limit)
					contactConstraint.m_appliedImpulse = -limit;
			}
			j1 = contactConstraint.m_appliedImpulse - oldTangentImpulse;
#else
			if (limit < j1)
			{
				j1 = limit;
			} else
			{
				if (j1 < -limit)
					j1 = -limit;
			}

#endif //CLAMP_ACCUMULATED_FRICTION_IMPULSE

			//GEN_set_min(contactConstraint.m_appliedImpulse, limit);
			//GEN_set_max(contactConstraint.m_appliedImpulse, -limit);

			

		}
	
		if (body1.m_invMass)
		{
			body1.internalApplyImpulse(contactConstraint.m_contactNormal*body1.m_invMass,contactConstraint.m_angularComponentA,j1);
		}
		if (body2.m_invMass)
		{
			body2.internalApplyImpulse(contactConstraint.m_contactNormal*body2.m_invMass,contactConstraint.m_angularComponentB,-j1);
		}

	} 
	return 0.f;
}


#else

//velocity + friction
//response  between two dynamic objects with friction
btScalar resolveSingleFrictionCacheFriendly(
	btSolverBody& body1,
	btSolverBody& body2,
	btSolverConstraint& contactConstraint,
	const btContactSolverInfo& solverInfo)
{

	btVector3 vel1;
	btVector3 vel2;
	btScalar normalImpulse(0.f);

	{
		const btVector3& normal = contactConstraint.m_contactNormal;
		if (contactConstraint.m_penetration < 0.f)
			return 0.f;


		body1.getVelocityInLocalPoint(contactConstraint.m_rel_posA,vel1);
		body2.getVelocityInLocalPoint(contactConstraint.m_rel_posB,vel2);
		btVector3 vel = vel1 - vel2;
		btScalar rel_vel;
		rel_vel = normal.dot(vel);

		btVector3 lat_vel = vel - normal * rel_vel;
		btScalar lat_rel_vel = lat_vel.length2();

		btScalar combinedFriction = contactConstraint.m_friction;
		const btVector3& rel_pos1 = contactConstraint.m_rel_posA;
		const btVector3& rel_pos2 = contactConstraint.m_rel_posB;


		//if (contactConstraint.m_appliedVelocityImpulse > 0.f)
		if (lat_rel_vel > SIMD_EPSILON*SIMD_EPSILON)
		{
			lat_rel_vel = btSqrt(lat_rel_vel);

			lat_vel /= lat_rel_vel;
			btVector3 temp1 = body1.m_invInertiaWorld * rel_pos1.cross(lat_vel);
			btVector3 temp2 = body2.m_invInertiaWorld * rel_pos2.cross(lat_vel);
			btScalar friction_impulse = lat_rel_vel /
				(body1.m_invMass + body2.m_invMass + lat_vel.dot(temp1.cross(rel_pos1) + temp2.cross(rel_pos2)));
			btScalar normal_impulse = contactConstraint.m_appliedVelocityImpulse * combinedFriction;

			GEN_set_min(friction_impulse, normal_impulse);
			GEN_set_max(friction_impulse, -normal_impulse);
			body1.applyImpulse(lat_vel * -friction_impulse, rel_pos1);
			body2.applyImpulse(lat_vel * friction_impulse, rel_pos2);
		}
	}

	return normalImpulse;
}

#endif //NO_FRICTION_TANGENTIALS





void	btSequentialImpulseConstraintSolver::addFrictionConstraint(const btVector3& normalAxis,int solverBodyIdA,int solverBodyIdB,int frictionIndex,btManifoldPoint& cp,const btVector3& rel_pos1,const btVector3& rel_pos2,btRigidBody* rb0,btRigidBody* rb1, btScalar relaxation)
{

	btSolverConstraint& solverConstraint = m_tmpSolverFrictionConstraintPool.expand();
	solverConstraint.m_contactNormal = normalAxis;

	solverConstraint.m_solverBodyIdA = solverBodyIdA;
	solverConstraint.m_solverBodyIdB = solverBodyIdB;
	solverConstraint.m_constraintType = btSolverConstraint::BT_SOLVER_FRICTION_1D;
	solverConstraint.m_frictionIndex = frictionIndex;

	solverConstraint.m_friction = cp.m_combinedFriction;

	solverConstraint.m_appliedImpulse = btScalar(0.);
	solverConstraint.m_appliedVelocityImpulse = 0.f;
	solverConstraint.m_penetration = 0.f;
	{
		btVector3 ftorqueAxis1 = rel_pos1.cross(solverConstraint.m_contactNormal);
		solverConstraint.m_relpos1CrossNormal = ftorqueAxis1;
		solverConstraint.m_angularComponentA = rb0->getInvInertiaTensorWorld()*ftorqueAxis1;
	}
	{
		btVector3 ftorqueAxis1 = rel_pos2.cross(solverConstraint.m_contactNormal);
		solverConstraint.m_relpos2CrossNormal = ftorqueAxis1;
		solverConstraint.m_angularComponentB = rb1->getInvInertiaTensorWorld()*ftorqueAxis1;
	}

#ifdef COMPUTE_IMPULSE_DENOM
	btScalar denom0 = rb0->computeImpulseDenominator(pos1,solverConstraint.m_contactNormal);
	btScalar denom1 = rb1->computeImpulseDenominator(pos2,solverConstraint.m_contactNormal);
#else
	btVector3 vec;
	vec = ( solverConstraint.m_angularComponentA).cross(rel_pos1);
	btScalar denom0 = rb0->getInvMass() + normalAxis.dot(vec);
	vec = ( solverConstraint.m_angularComponentB).cross(rel_pos2);
	btScalar denom1 = rb1->getInvMass() + normalAxis.dot(vec);


#endif //COMPUTE_IMPULSE_DENOM
	btScalar denom = relaxation/(denom0+denom1);
	solverConstraint.m_jacDiagABInv = denom;

}


btScalar btSequentialImpulseConstraintSolver::solveGroupCacheFriendlySetup(btCollisionObject** bodies,int numBodies,btPersistentManifold** manifoldPtr, int numManifolds,btTypedConstraint** constraints,int numConstraints,const btContactSolverInfo& infoGlobal,btIDebugDraw* debugDrawer,btStackAlloc* stackAlloc)
{
	(void)stackAlloc;
	(void)debugDrawer;

	if (!(numConstraints + numManifolds))
	{
//		printf("empty\n");
		return 0.f;
	}
	btPersistentManifold* manifold = 0;
	btRigidBody* rb0=0,*rb1=0;

#ifdef FORCE_REFESH_CONTACT_MANIFOLDS

	BEGIN_PROFILE("refreshManifolds");

	int i;
	
	

	for (i=0;i<numManifolds;i++)
	{
		manifold = manifoldPtr[i];
		rb1 = (btRigidBody*)manifold->getBody1();
		rb0 = (btRigidBody*)manifold->getBody0();
		
		manifold->refreshContactPoints(rb0->getCenterOfMassTransform(),rb1->getCenterOfMassTransform());

	}

	END_PROFILE("refreshManifolds");
#endif //FORCE_REFESH_CONTACT_MANIFOLDS

	btVector3 color(0,1,0);


	BEGIN_PROFILE("gatherSolverData");

	//int sizeofSB = sizeof(btSolverBody);
	//int sizeofSC = sizeof(btSolverConstraint);


	//if (1)
	{
		//if m_stackAlloc, try to pack bodies/constraints to speed up solving
//		btBlock*					sablock;
//		sablock = stackAlloc->beginBlock();

	//	int memsize = 16;
//		unsigned char* stackMemory = stackAlloc->allocate(memsize);

		
		//todo: use stack allocator for this temp memory
		int minReservation = numManifolds*2;

		//m_tmpSolverBodyPool.reserve(minReservation);

		//don't convert all bodies, only the one we need so solver the constraints
/*
		{
			for (int i=0;i<numBodies;i++)
			{
				btRigidBody* rb = btRigidBody::upcast(bodies[i]);
				if (rb && 	(rb->getIslandTag() >= 0))
				{
					btAssert(rb->getCompanionId() < 0);
					int solverBodyId = m_tmpSolverBodyPool.size();
					btSolverBody& solverBody = m_tmpSolverBodyPool.expand();
					initSolverBody(&solverBody,rb);
					rb->setCompanionId(solverBodyId);
				} 
			}
		}
*/
		
		//m_tmpSolverConstraintPool.reserve(minReservation);
		//m_tmpSolverFrictionConstraintPool.reserve(minReservation);

		{
			int i;

			for (i=0;i<numManifolds;i++)
			{
				manifold = manifoldPtr[i];
				rb1 = (btRigidBody*)manifold->getBody1();

				rb0 = (btRigidBody*)manifold->getBody0();
				
			
				int solverBodyIdA=-1;
				int solverBodyIdB=-1;

				if (manifold->getNumContacts())
				{

					

					if (rb0->getIslandTag() >= 0)
					{
						if (rb0->getCompanionId() >= 0)
						{
							//body has already been converted
							solverBodyIdA = rb0->getCompanionId();
						} else
						{
							solverBodyIdA = m_tmpSolverBodyPool.size();
							btSolverBody& solverBody = m_tmpSolverBodyPool.expand();
							initSolverBody(&solverBody,rb0);
							rb0->setCompanionId(solverBodyIdA);
						}
					} else
					{
						//create a static body
						solverBodyIdA = m_tmpSolverBodyPool.size();
						btSolverBody& solverBody = m_tmpSolverBodyPool.expand();
						initSolverBody(&solverBody,rb0);
					}

					if (rb1->getIslandTag() >= 0)
					{
						if (rb1->getCompanionId() >= 0)
						{
							solverBodyIdB = rb1->getCompanionId();
						} else
						{
							solverBodyIdB = m_tmpSolverBodyPool.size();
							btSolverBody& solverBody = m_tmpSolverBodyPool.expand();
							initSolverBody(&solverBody,rb1);
							rb1->setCompanionId(solverBodyIdB);
						}
					} else
					{
						//create a static body
						solverBodyIdB = m_tmpSolverBodyPool.size();
						btSolverBody& solverBody = m_tmpSolverBodyPool.expand();
						initSolverBody(&solverBody,rb1);
					}
				}

				btVector3 rel_pos1;
				btVector3 rel_pos2;
				btScalar relaxation;

				for (int j=0;j<manifold->getNumContacts();j++)
				{
					
					btManifoldPoint& cp = manifold->getContactPoint(j);
					
					if (debugDrawer)
						debugDrawer->drawContactPoint(cp.m_positionWorldOnB,cp.m_normalWorldOnB,cp.getDistance(),cp.getLifeTime(),color);

					
					if (cp.getDistance() <= btScalar(0.))
					{
						
						const btVector3& pos1 = cp.getPositionWorldOnA();
						const btVector3& pos2 = cp.getPositionWorldOnB();

						 rel_pos1 = pos1 - rb0->getCenterOfMassPosition(); 
						 rel_pos2 = pos2 - rb1->getCenterOfMassPosition();

						
						relaxation = 1.f;
						btScalar rel_vel;
						btVector3 vel;

						int frictionIndex = m_tmpSolverConstraintPool.size();

						{
							btSolverConstraint& solverConstraint = m_tmpSolverConstraintPool.expand();

							solverConstraint.m_solverBodyIdA = solverBodyIdA;
							solverConstraint.m_solverBodyIdB = solverBodyIdB;
							solverConstraint.m_constraintType = btSolverConstraint::BT_SOLVER_CONTACT_1D;

							btVector3 torqueAxis0 = rel_pos1.cross(cp.m_normalWorldOnB);
							solverConstraint.m_angularComponentA = rb0->getInvInertiaTensorWorld()*torqueAxis0;
							btVector3 torqueAxis1 = rel_pos2.cross(cp.m_normalWorldOnB);		
							solverConstraint.m_angularComponentB = rb1->getInvInertiaTensorWorld()*torqueAxis1;
							{
#ifdef COMPUTE_IMPULSE_DENOM
								btScalar denom0 = rb0->computeImpulseDenominator(pos1,cp.m_normalWorldOnB);
								btScalar denom1 = rb1->computeImpulseDenominator(pos2,cp.m_normalWorldOnB);
#else							
								btVector3 vec;
								vec = ( solverConstraint.m_angularComponentA).cross(rel_pos1);
								btScalar denom0 = rb0->getInvMass() + cp.m_normalWorldOnB.dot(vec);
								vec = ( solverConstraint.m_angularComponentB).cross(rel_pos2);
								btScalar denom1 = rb1->getInvMass() + cp.m_normalWorldOnB.dot(vec);
#endif //COMPUTE_IMPULSE_DENOM		
								
								btScalar denom = relaxation/(denom0+denom1);
								solverConstraint.m_jacDiagABInv = denom;
							}

							solverConstraint.m_contactNormal = cp.m_normalWorldOnB;
							solverConstraint.m_relpos1CrossNormal = rel_pos1.cross(cp.m_normalWorldOnB);
							solverConstraint.m_relpos2CrossNormal = rel_pos2.cross(cp.m_normalWorldOnB);


							btVector3 vel1 = rb0->getVelocityInLocalPoint(rel_pos1);
							btVector3 vel2 = rb1->getVelocityInLocalPoint(rel_pos2);
				
							vel  = vel1 - vel2;
							
							rel_vel = cp.m_normalWorldOnB.dot(vel);
							

							solverConstraint.m_penetration = cp.getDistance();///btScalar(infoGlobal.m_numIterations);
							solverConstraint.m_friction = cp.m_combinedFriction;
							solverConstraint.m_restitution =  restitutionCurve(rel_vel, cp.m_combinedRestitution);
							if (solverConstraint.m_restitution <= btScalar(0.))
							{
								solverConstraint.m_restitution = 0.f;
							};
							
							btScalar penVel = -solverConstraint.m_penetration/infoGlobal.m_timeStep;
							solverConstraint.m_penetration *= -(infoGlobal.m_erp/infoGlobal.m_timeStep);

							if (solverConstraint.m_restitution > penVel)
							{
								solverConstraint.m_penetration = btScalar(0.);
							}
							
							

							solverConstraint.m_appliedImpulse = 0.f;
							solverConstraint.m_appliedVelocityImpulse = 0.f;
							
					
						
						}

						
						{
							btVector3 frictionDir1 = vel - cp.m_normalWorldOnB * rel_vel;
							btScalar lat_rel_vel = frictionDir1.length2();
							if (lat_rel_vel > SIMD_EPSILON)//0.0f)
							{
								frictionDir1 /= btSqrt(lat_rel_vel);
								addFrictionConstraint(frictionDir1,solverBodyIdA,solverBodyIdB,frictionIndex,cp,rel_pos1,rel_pos2,rb0,rb1, relaxation);
								btVector3 frictionDir2 = frictionDir1.cross(cp.m_normalWorldOnB);
								frictionDir2.normalize();//??
								addFrictionConstraint(frictionDir2,solverBodyIdA,solverBodyIdB,frictionIndex,cp,rel_pos1,rel_pos2,rb0,rb1, relaxation);
							} else
							{
								//re-calculate friction direction every frame, todo: check if this is really needed
								btVector3	frictionDir1,frictionDir2;
								btPlaneSpace1(cp.m_normalWorldOnB,frictionDir1,frictionDir2);
								addFrictionConstraint(frictionDir1,solverBodyIdA,solverBodyIdB,frictionIndex,cp,rel_pos1,rel_pos2,rb0,rb1, relaxation);
								addFrictionConstraint(frictionDir2,solverBodyIdA,solverBodyIdB,frictionIndex,cp,rel_pos1,rel_pos2,rb0,rb1, relaxation);
							}

						}
					}
				}
			}
		}
	}
	END_PROFILE("gatherSolverData");

	BEGIN_PROFILE("prepareConstraints");

	btContactSolverInfo info = infoGlobal;

	{
		int j;
		for (j=0;j<numConstraints;j++)
		{
			btTypedConstraint* constraint = constraints[j];
			constraint->buildJacobian();
		}
	}
	
	

	int numConstraintPool = m_tmpSolverConstraintPool.size();
	int numFrictionPool = m_tmpSolverFrictionConstraintPool.size();

	///todo: use stack allocator for such temporarily memory, same for solver bodies/constraints
	m_orderTmpConstraintPool.resize(numConstraintPool);
	m_orderFrictionConstraintPool.resize(numFrictionPool);
	{
		int i;
		for (i=0;i<numConstraintPool;i++)
		{
			m_orderTmpConstraintPool[i] = i;
		}
		for (i=0;i<numFrictionPool;i++)
		{
			m_orderFrictionConstraintPool[i] = i;
		}
	}




	END_PROFILE("prepareConstraints");
	return 0.f;

}

btScalar btSequentialImpulseConstraintSolver::solveGroupCacheFriendlyIterations(btCollisionObject** bodies,int numBodies,btPersistentManifold** manifoldPtr, int numManifolds,btTypedConstraint** constraints,int numConstraints,const btContactSolverInfo& infoGlobal,btIDebugDraw* debugDrawer,btStackAlloc* stackAlloc)
{
	BEGIN_PROFILE("solveConstraintsIterations");
	int numConstraintPool = m_tmpSolverConstraintPool.size();
	int numFrictionPool = m_tmpSolverFrictionConstraintPool.size();

	//should traverse the contacts random order...
	int iteration;
	{
		for ( iteration = 0;iteration<infoGlobal.m_numIterations;iteration++)
		{			

			int j;
			if (m_solverMode & SOLVER_RANDMIZE_ORDER)
			{
				if ((iteration & 7) == 0) {
					for (j=0; j<numConstraintPool; ++j) {
						int tmp = m_orderTmpConstraintPool[j];
						int swapi = btRandInt2(j+1);
						m_orderTmpConstraintPool[j] = m_orderTmpConstraintPool[swapi];
						m_orderTmpConstraintPool[swapi] = tmp;
					}

					for (j=0; j<numFrictionPool; ++j) {
						int tmp = m_orderFrictionConstraintPool[j];
						int swapi = btRandInt2(j+1);
						m_orderFrictionConstraintPool[j] = m_orderFrictionConstraintPool[swapi];
						m_orderFrictionConstraintPool[swapi] = tmp;
					}
				}
			}

			for (j=0;j<numConstraints;j++)
			{
				btTypedConstraint* constraint = constraints[j];
				///todo: use solver bodies, so we don't need to copy from/to btRigidBody

				if ((constraint->getRigidBodyA().getIslandTag() >= 0) && (constraint->getRigidBodyA().getCompanionId() >= 0))
				{
					m_tmpSolverBodyPool[constraint->getRigidBodyA().getCompanionId()].writebackVelocity();
				}
				if ((constraint->getRigidBodyB().getIslandTag() >= 0) && (constraint->getRigidBodyB().getCompanionId() >= 0))
				{
					m_tmpSolverBodyPool[constraint->getRigidBodyB().getCompanionId()].writebackVelocity();
				}

				constraint->solveConstraint(infoGlobal.m_timeStep);

				if ((constraint->getRigidBodyA().getIslandTag() >= 0) && (constraint->getRigidBodyA().getCompanionId() >= 0))
				{
					m_tmpSolverBodyPool[constraint->getRigidBodyA().getCompanionId()].readVelocity();
				}
				if ((constraint->getRigidBodyB().getIslandTag() >= 0) && (constraint->getRigidBodyB().getCompanionId() >= 0))
				{
					m_tmpSolverBodyPool[constraint->getRigidBodyB().getCompanionId()].readVelocity();
				}

			}

			{
				int numPoolConstraints = m_tmpSolverConstraintPool.size();
				for (j=0;j<numPoolConstraints;j++)
				{
					const btSolverConstraint& solveManifold = m_tmpSolverConstraintPool[m_orderTmpConstraintPool[j]];
					resolveSingleCollisionCombinedCacheFriendly(m_tmpSolverBodyPool[solveManifold.m_solverBodyIdA],
						m_tmpSolverBodyPool[solveManifold.m_solverBodyIdB],solveManifold,infoGlobal);
				}
			}

			{
				 int numFrictionPoolConstraints = m_tmpSolverFrictionConstraintPool.size();
				
				 for (j=0;j<numFrictionPoolConstraints;j++)
				{
					const btSolverConstraint& solveManifold = m_tmpSolverFrictionConstraintPool[m_orderFrictionConstraintPool[j]];
						

						resolveSingleFrictionCacheFriendly(m_tmpSolverBodyPool[solveManifold.m_solverBodyIdA],
							m_tmpSolverBodyPool[solveManifold.m_solverBodyIdB],solveManifold,infoGlobal,
							m_tmpSolverConstraintPool[solveManifold.m_frictionIndex].m_appliedImpulse);
				}
			}
			


		}
	}

		END_PROFILE("solveConstraintsIterations");

		return 0.f;
}


btScalar btSequentialImpulseConstraintSolver::solveGroupCacheFriendly(btCollisionObject** bodies,int numBodies,btPersistentManifold** manifoldPtr, int numManifolds,btTypedConstraint** constraints,int numConstraints,const btContactSolverInfo& infoGlobal,btIDebugDraw* debugDrawer,btStackAlloc* stackAlloc)
{
	int i;

	solveGroupCacheFriendlySetup( bodies, numBodies, manifoldPtr,  numManifolds,constraints, numConstraints,infoGlobal,debugDrawer, stackAlloc);
	solveGroupCacheFriendlyIterations(bodies, numBodies, manifoldPtr,  numManifolds,constraints, numConstraints,infoGlobal,debugDrawer, stackAlloc);

	BEGIN_PROFILE("solveWriteBackVelocity");
		
	for ( i=0;i<m_tmpSolverBodyPool.size();i++)
	{
		m_tmpSolverBodyPool[i].writebackVelocity();
	}

	END_PROFILE("solveWriteBackVelocity");

//	printf("m_tmpSolverConstraintPool.size() = %i\n",m_tmpSolverConstraintPool.size());

/*
	printf("m_tmpSolverBodyPool.size() = %i\n",m_tmpSolverBodyPool.size());
	printf("m_tmpSolverConstraintPool.size() = %i\n",m_tmpSolverConstraintPool.size());
	printf("m_tmpSolverFrictionConstraintPool.size() = %i\n",m_tmpSolverFrictionConstraintPool.size());

	
	printf("m_tmpSolverBodyPool.capacity() = %i\n",m_tmpSolverBodyPool.capacity());
	printf("m_tmpSolverConstraintPool.capacity() = %i\n",m_tmpSolverConstraintPool.capacity());
	printf("m_tmpSolverFrictionConstraintPool.capacity() = %i\n",m_tmpSolverFrictionConstraintPool.capacity());
*/

	m_tmpSolverBodyPool.resize(0);
	m_tmpSolverConstraintPool.resize(0);
	m_tmpSolverFrictionConstraintPool.resize(0);


	return 0.f;
}

/// btSequentialImpulseConstraintSolver Sequentially applies impulses
btScalar btSequentialImpulseConstraintSolver::solveGroup(btCollisionObject** bodies,int numBodies,btPersistentManifold** manifoldPtr, int numManifolds,btTypedConstraint** constraints,int numConstraints,const btContactSolverInfo& infoGlobal,btIDebugDraw* debugDrawer,btStackAlloc* stackAlloc,btDispatcher* dispatcher)
{

	if (getSolverMode() & SOLVER_CACHE_FRIENDLY)
	{
		//you need to provide at least some bodies
		//btSimpleDynamicsWorld needs to switch off SOLVER_CACHE_FRIENDLY
		btAssert(bodies);
		btAssert(numBodies);
		return solveGroupCacheFriendly(bodies,numBodies,manifoldPtr, numManifolds,constraints,numConstraints,infoGlobal,debugDrawer,stackAlloc);
	}


	BEGIN_PROFILE("prepareConstraints");
	

	btContactSolverInfo info = infoGlobal;

	int numiter = infoGlobal.m_numIterations;
#ifdef USE_PROFILE
	btProfiler::beginBlock("solve");
#endif //USE_PROFILE

	int totalPoints = 0;


	{
		short j;
		for (j=0;j<numManifolds;j++)
		{
			btPersistentManifold* manifold = manifoldPtr[j];
			prepareConstraints(manifold,info,debugDrawer);

			for (short p=0;p<manifoldPtr[j]->getNumContacts();p++)
			{
				gOrder[totalPoints].m_manifoldIndex = j;
				gOrder[totalPoints].m_pointIndex = p;
				totalPoints++;
			}
		}
	}

	{
		int j;
		for (j=0;j<numConstraints;j++)
		{
			btTypedConstraint* constraint = constraints[j];
			constraint->buildJacobian();
		}
	}
	
	END_PROFILE("prepareConstraints");


	BEGIN_PROFILE("solveConstraints");

	//should traverse the contacts random order...
	int iteration;

	{
		for ( iteration = 0;iteration<numiter;iteration++)
		{
			int j;
			if (m_solverMode & SOLVER_RANDMIZE_ORDER)
			{
				if ((iteration & 7) == 0) {
					for (j=0; j<totalPoints; ++j) {
						btOrderIndex tmp = gOrder[j];
						int swapi = btRandInt2(j+1);
						gOrder[j] = gOrder[swapi];
						gOrder[swapi] = tmp;
					}
				}
			}

			for (j=0;j<numConstraints;j++)
			{
				btTypedConstraint* constraint = constraints[j];
				constraint->solveConstraint(info.m_timeStep);
			}

			for (j=0;j<totalPoints;j++)
			{
				btPersistentManifold* manifold = manifoldPtr[gOrder[j].m_manifoldIndex];
				solve( (btRigidBody*)manifold->getBody0(),
									(btRigidBody*)manifold->getBody1()
				,manifold->getContactPoint(gOrder[j].m_pointIndex),info,iteration,debugDrawer);
			}
		
			for (j=0;j<totalPoints;j++)
			{
				btPersistentManifold* manifold = manifoldPtr[gOrder[j].m_manifoldIndex];
				solveFriction((btRigidBody*)manifold->getBody0(),
					(btRigidBody*)manifold->getBody1(),manifold->getContactPoint(gOrder[j].m_pointIndex),info,iteration,debugDrawer);
			}
			
		}
	}
		
	END_PROFILE("solveConstraints");


#ifdef USE_PROFILE
	btProfiler::endBlock("solve");
#endif //USE_PROFILE




	return btScalar(0.);
}







void	btSequentialImpulseConstraintSolver::prepareConstraints(btPersistentManifold* manifoldPtr, const btContactSolverInfo& info,btIDebugDraw* debugDrawer)
{

	(void)debugDrawer;

	btRigidBody* body0 = (btRigidBody*)manifoldPtr->getBody0();
	btRigidBody* body1 = (btRigidBody*)manifoldPtr->getBody1();


	//only necessary to refresh the manifold once (first iteration). The integration is done outside the loop
	{
#ifdef FORCE_REFESH_CONTACT_MANIFOLDS
		manifoldPtr->refreshContactPoints(body0->getCenterOfMassTransform(),body1->getCenterOfMassTransform());
#endif //FORCE_REFESH_CONTACT_MANIFOLDS		
		int numpoints = manifoldPtr->getNumContacts();

		gTotalContactPoints += numpoints;

		btVector3 color(0,1,0);
		for (int i=0;i<numpoints ;i++)
		{
			btManifoldPoint& cp = manifoldPtr->getContactPoint(i);
			if (cp.getDistance() <= btScalar(0.))
			{
				const btVector3& pos1 = cp.getPositionWorldOnA();
				const btVector3& pos2 = cp.getPositionWorldOnB();

				btVector3 rel_pos1 = pos1 - body0->getCenterOfMassPosition(); 
				btVector3 rel_pos2 = pos2 - body1->getCenterOfMassPosition();
				

				//this jacobian entry is re-used for all iterations
				btJacobianEntry jac(body0->getCenterOfMassTransform().getBasis().transpose(),
					body1->getCenterOfMassTransform().getBasis().transpose(),
					rel_pos1,rel_pos2,cp.m_normalWorldOnB,body0->getInvInertiaDiagLocal(),body0->getInvMass(),
					body1->getInvInertiaDiagLocal(),body1->getInvMass());

				
				btScalar jacDiagAB = jac.getDiagonal();

				btConstraintPersistentData* cpd = (btConstraintPersistentData*) cp.m_userPersistentData;
				if (cpd)
				{
					//might be invalid
					cpd->m_persistentLifeTime++;
					if (cpd->m_persistentLifeTime != cp.getLifeTime())
					{
						//printf("Invalid: cpd->m_persistentLifeTime = %i cp.getLifeTime() = %i\n",cpd->m_persistentLifeTime,cp.getLifeTime());
						new (cpd) btConstraintPersistentData;
						cpd->m_persistentLifeTime = cp.getLifeTime();

					} else
					{
						//printf("Persistent: cpd->m_persistentLifeTime = %i cp.getLifeTime() = %i\n",cpd->m_persistentLifeTime,cp.getLifeTime());
						
					}
				} else
				{
						
					//todo: should this be in a pool?
					void* mem = btAlignedAlloc(sizeof(btConstraintPersistentData),16);
					cpd = new (mem)btConstraintPersistentData;
					assert(cpd);

					totalCpd ++;
					//printf("totalCpd = %i Created Ptr %x\n",totalCpd,cpd);
					cp.m_userPersistentData = cpd;
					cpd->m_persistentLifeTime = cp.getLifeTime();
					//printf("CREATED: %x . cpd->m_persistentLifeTime = %i cp.getLifeTime() = %i\n",cpd,cpd->m_persistentLifeTime,cp.getLifeTime());
					
				}
				assert(cpd);

				cpd->m_jacDiagABInv = btScalar(1.) / jacDiagAB;

				//Dependent on Rigidbody A and B types, fetch the contact/friction response func
				//perhaps do a similar thing for friction/restutution combiner funcs...
				
				cpd->m_frictionSolverFunc = m_frictionDispatch[body0->m_frictionSolverType][body1->m_frictionSolverType];
				cpd->m_contactSolverFunc = m_contactDispatch[body0->m_contactSolverType][body1->m_contactSolverType];
				
				btVector3 vel1 = body0->getVelocityInLocalPoint(rel_pos1);
				btVector3 vel2 = body1->getVelocityInLocalPoint(rel_pos2);
				btVector3 vel = vel1 - vel2;
				btScalar rel_vel;
				rel_vel = cp.m_normalWorldOnB.dot(vel);
				
				btScalar combinedRestitution = cp.m_combinedRestitution;
				
				cpd->m_penetration = cp.getDistance();///btScalar(info.m_numIterations);
				cpd->m_friction = cp.m_combinedFriction;
				cpd->m_restitution = restitutionCurve(rel_vel, combinedRestitution);
				if (cpd->m_restitution <= btScalar(0.))
				{
					cpd->m_restitution = btScalar(0.0);

				};
				
				//restitution and penetration work in same direction so
				//rel_vel 
				
				btScalar penVel = -cpd->m_penetration/info.m_timeStep;

				if (cpd->m_restitution > penVel)
				{
					cpd->m_penetration = btScalar(0.);
				} 				
				

				btScalar relaxation = info.m_damping;
				if (m_solverMode & SOLVER_USE_WARMSTARTING)
				{
					cpd->m_appliedImpulse *= relaxation;
				} else
				{
					cpd->m_appliedImpulse =btScalar(0.);
				}
	
				//for friction
				cpd->m_prevAppliedImpulse = cpd->m_appliedImpulse;
				
				//re-calculate friction direction every frame, todo: check if this is really needed
				btPlaneSpace1(cp.m_normalWorldOnB,cpd->m_frictionWorldTangential0,cpd->m_frictionWorldTangential1);


#define NO_FRICTION_WARMSTART 1

	#ifdef NO_FRICTION_WARMSTART
				cpd->m_accumulatedTangentImpulse0 = btScalar(0.);
				cpd->m_accumulatedTangentImpulse1 = btScalar(0.);
	#endif //NO_FRICTION_WARMSTART
				btScalar denom0 = body0->computeImpulseDenominator(pos1,cpd->m_frictionWorldTangential0);
				btScalar denom1 = body1->computeImpulseDenominator(pos2,cpd->m_frictionWorldTangential0);
				btScalar denom = relaxation/(denom0+denom1);
				cpd->m_jacDiagABInvTangent0 = denom;


				denom0 = body0->computeImpulseDenominator(pos1,cpd->m_frictionWorldTangential1);
				denom1 = body1->computeImpulseDenominator(pos2,cpd->m_frictionWorldTangential1);
				denom = relaxation/(denom0+denom1);
				cpd->m_jacDiagABInvTangent1 = denom;

				btVector3 totalImpulse = 
	#ifndef NO_FRICTION_WARMSTART
					cpd->m_frictionWorldTangential0*cpd->m_accumulatedTangentImpulse0+
					cpd->m_frictionWorldTangential1*cpd->m_accumulatedTangentImpulse1+
	#endif //NO_FRICTION_WARMSTART
					cp.m_normalWorldOnB*cpd->m_appliedImpulse;



				///
				{
				btVector3 torqueAxis0 = rel_pos1.cross(cp.m_normalWorldOnB);
				cpd->m_angularComponentA = body0->getInvInertiaTensorWorld()*torqueAxis0;
				btVector3 torqueAxis1 = rel_pos2.cross(cp.m_normalWorldOnB);		
				cpd->m_angularComponentB = body1->getInvInertiaTensorWorld()*torqueAxis1;
				}
				{
					btVector3 ftorqueAxis0 = rel_pos1.cross(cpd->m_frictionWorldTangential0);
					cpd->m_frictionAngularComponent0A = body0->getInvInertiaTensorWorld()*ftorqueAxis0;
				}
				{
					btVector3 ftorqueAxis1 = rel_pos1.cross(cpd->m_frictionWorldTangential1);
					cpd->m_frictionAngularComponent1A = body0->getInvInertiaTensorWorld()*ftorqueAxis1;
				}
				{
					btVector3 ftorqueAxis0 = rel_pos2.cross(cpd->m_frictionWorldTangential0);
					cpd->m_frictionAngularComponent0B = body1->getInvInertiaTensorWorld()*ftorqueAxis0;
				}
				{
					btVector3 ftorqueAxis1 = rel_pos2.cross(cpd->m_frictionWorldTangential1);
					cpd->m_frictionAngularComponent1B = body1->getInvInertiaTensorWorld()*ftorqueAxis1;
				}
				
				///



				//apply previous frames impulse on both bodies
				body0->applyImpulse(totalImpulse, rel_pos1);
				body1->applyImpulse(-totalImpulse, rel_pos2);
			}
			
		}
	}
}


btScalar btSequentialImpulseConstraintSolver::solveCombinedContactFriction(btRigidBody* body0,btRigidBody* body1, btManifoldPoint& cp, const btContactSolverInfo& info,int iter,btIDebugDraw* debugDrawer)
{
	btScalar maxImpulse = btScalar(0.);

	{

		btVector3 color(0,1,0);
		{
			if (cp.getDistance() <= btScalar(0.))
			{

				if (iter == 0)
				{
					if (debugDrawer)
						debugDrawer->drawContactPoint(cp.m_positionWorldOnB,cp.m_normalWorldOnB,cp.getDistance(),cp.getLifeTime(),color);
				}

				{

					//btConstraintPersistentData* cpd = (btConstraintPersistentData*) cp.m_userPersistentData;
					btScalar impulse = resolveSingleCollisionCombined(
						*body0,*body1,
						cp,
						info);

					if (maxImpulse < impulse)
						maxImpulse  = impulse;

				}
			}
		}
	}
	return maxImpulse;
}



btScalar btSequentialImpulseConstraintSolver::solve(btRigidBody* body0,btRigidBody* body1, btManifoldPoint& cp, const btContactSolverInfo& info,int iter,btIDebugDraw* debugDrawer)
{

	btScalar maxImpulse = btScalar(0.);

	{

		btVector3 color(0,1,0);
		{
			if (cp.getDistance() <= btScalar(0.))
			{

				if (iter == 0)
				{
					if (debugDrawer)
						debugDrawer->drawContactPoint(cp.m_positionWorldOnB,cp.m_normalWorldOnB,cp.getDistance(),cp.getLifeTime(),color);
				}

				{

					btConstraintPersistentData* cpd = (btConstraintPersistentData*) cp.m_userPersistentData;
					btScalar impulse = cpd->m_contactSolverFunc(
						*body0,*body1,
						cp,
						info);

					if (maxImpulse < impulse)
						maxImpulse  = impulse;

				}
			}
		}
	}
	return maxImpulse;
}

btScalar btSequentialImpulseConstraintSolver::solveFriction(btRigidBody* body0,btRigidBody* body1, btManifoldPoint& cp, const btContactSolverInfo& info,int iter,btIDebugDraw* debugDrawer)
{

	(void)debugDrawer;
	(void)iter;


	{

		btVector3 color(0,1,0);
		{
			
			if (cp.getDistance() <= btScalar(0.))
			{

				btConstraintPersistentData* cpd = (btConstraintPersistentData*) cp.m_userPersistentData;
				cpd->m_frictionSolverFunc(
					*body0,*body1,
					cp,
					info);

				
			}
		}

	
	}
	return btScalar(0.);
}


void	btSequentialImpulseConstraintSolver::reset()
{
	m_btSeed2 = 0;
}

