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


#include "btSequentialImpulseConstraintSolver.h"
#include "BulletCollision/NarrowPhaseCollision/btPersistentManifold.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "btContactConstraint.h"
#include "btSolve2LinearConstraint.h"
#include "btContactSolverInfo.h"
#include "LinearMath/btIDebugDraw.h"
#include "btJacobianEntry.h"
#include "LinearMath/btMinMax.h"

#ifdef USE_PROFILE
#include "LinearMath/btQuickprof.h"
#endif //USE_PROFILE

int totalCpd = 0;

int	gTotalContactPoints = 0;

struct	btOrderIndex
{
	short int	m_manifoldIndex;
	short int	m_pointIndex;
};

#define SEQUENTIAL_IMPULSE_MAX_SOLVER_POINTS 16384
static btOrderIndex	gOrder[SEQUENTIAL_IMPULSE_MAX_SOLVER_POINTS];
static unsigned long btSeed2 = 0;
unsigned long btRand2()
{
  btSeed2 = (1664525L*btSeed2 + 1013904223L) & 0xffffffff;
  return btSeed2;
}

int btRandInt2 (int n)
{
  float a = float(n) / 4294967296.0f;
  return (int) (float(btRand2()) * a);
}

bool  MyContactDestroyedCallback(void* userPersistentData)
{
	assert (userPersistentData);
	btConstraintPersistentData* cpd = (btConstraintPersistentData*)userPersistentData;
	delete cpd;
	totalCpd--;
	//printf("totalCpd = %i. DELETED Ptr %x\n",totalCpd,userPersistentData);
	return true;
}

btSequentialImpulseConstraintSolver2::btSequentialImpulseConstraintSolver2()
{
	setSolverMode(SOLVER_USE_WARMSTARTING);
}


btSequentialImpulseConstraintSolver::btSequentialImpulseConstraintSolver()
:m_solverMode(SOLVER_RANDMIZE_ORDER)
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

/// btSequentialImpulseConstraintSolver Sequentially applies impulses
float btSequentialImpulseConstraintSolver::solveGroup(btPersistentManifold** manifoldPtr, int numManifolds,const btContactSolverInfo& infoGlobal,btIDebugDraw* debugDrawer)
{
	
	btContactSolverInfo info = infoGlobal;

	int numiter = infoGlobal.m_numIterations;
#ifdef USE_PROFILE
	btProfiler::beginBlock("solve");
#endif //USE_PROFILE

	int totalPoints = 0;


	{
		int j;
		for (j=0;j<numManifolds;j++)
		{
			btPersistentManifold* manifold = manifoldPtr[j];
			prepareConstraints(manifold,info,debugDrawer);
			for (int p=0;p<manifoldPtr[j]->getNumContacts();p++)
			{
				gOrder[totalPoints].m_manifoldIndex = j;
				gOrder[totalPoints].m_pointIndex = p;
				totalPoints++;
			}
		}
	}
	
	//should traverse the contacts random order...
	int iteration;

	{
		for ( iteration = 0;iteration<numiter-1;iteration++)
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
		
#ifdef USE_PROFILE
	btProfiler::endBlock("solve");
#endif //USE_PROFILE

	return 0.f;
}


/// btSequentialImpulseConstraintSolver Sequentially applies impulses
float btSequentialImpulseConstraintSolver2::solveGroup(btPersistentManifold** manifoldPtr, int numManifolds,const btContactSolverInfo& infoGlobal,btIDebugDraw* debugDrawer)
{
	
	btContactSolverInfo info = infoGlobal;

	int numiter = infoGlobal.m_numIterations;
#ifdef USE_PROFILE
	btProfiler::beginBlock("solve");
#endif //USE_PROFILE

	{
		int j;
		for (j=0;j<numManifolds;j++)
		{
			btPersistentManifold* manifold = manifoldPtr[j];
			prepareConstraints(manifold,info,debugDrawer);
			for (int p=0;p<manifoldPtr[j]->getNumContacts();p++)
			{
				//interleaving here gives better results
				solve( (btRigidBody*)manifold->getBody0(),
									(btRigidBody*)manifold->getBody1()
				,manifoldPtr[j]->getContactPoint(p),info,0,debugDrawer);
			}
		}
	}
	
	//should traverse the contacts random order...
	int iteration;

	for ( iteration = 0;iteration<numiter-1;iteration++)
	{
		int j;

		for (j=0;j<numManifolds;j++)
		{
			btPersistentManifold* manifold = manifoldPtr[j];
			for (int p=0;p<manifold->getNumContacts();p++)
			{
				solve( (btRigidBody*)manifold->getBody0(),
									(btRigidBody*)manifold->getBody1()
				,manifold->getContactPoint(p),info,iteration,debugDrawer);
			}
		}
	
	}

	for ( iteration = 0;iteration<numiter-1;iteration++)
	{
		int j;
		for (j=0;j<numManifolds;j++)
		{
			btPersistentManifold* manifold = manifoldPtr[j];
			for (int p=0;p<manifold->getNumContacts();p++)
			{
				solveFriction((btRigidBody*)manifold->getBody0(),
					(btRigidBody*)manifold->getBody1(),manifold->getContactPoint(p),info,iteration,debugDrawer);
			}
		}
	}

		
#ifdef USE_PROFILE
	btProfiler::endBlock("solve");
#endif //USE_PROFILE

	return 0.f;
}


float penetrationResolveFactor = 0.9f;
btScalar restitutionCurve(btScalar rel_vel, btScalar restitution)
{
	btScalar rest = restitution * -rel_vel;
	return rest;
}


void	btSequentialImpulseConstraintSolver::prepareConstraints(btPersistentManifold* manifoldPtr, const btContactSolverInfo& info,btIDebugDraw* debugDrawer)
{

	btRigidBody* body0 = (btRigidBody*)manifoldPtr->getBody0();
	btRigidBody* body1 = (btRigidBody*)manifoldPtr->getBody1();


	//only necessary to refresh the manifold once (first iteration). The integration is done outside the loop
	{
		manifoldPtr->refreshContactPoints(body0->getCenterOfMassTransform(),body1->getCenterOfMassTransform());
		
		int numpoints = manifoldPtr->getNumContacts();

		gTotalContactPoints += numpoints;

		btVector3 color(0,1,0);
		for (int i=0;i<numpoints ;i++)
		{
			btManifoldPoint& cp = manifoldPtr->getContactPoint(i);
			if (cp.getDistance() <= 0.f)
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
						
					cpd = new btConstraintPersistentData();
					totalCpd ++;
					//printf("totalCpd = %i Created Ptr %x\n",totalCpd,cpd);
					cp.m_userPersistentData = cpd;
					cpd->m_persistentLifeTime = cp.getLifeTime();
					//printf("CREATED: %x . cpd->m_persistentLifeTime = %i cp.getLifeTime() = %i\n",cpd,cpd->m_persistentLifeTime,cp.getLifeTime());
					
				}
				assert(cpd);

				cpd->m_jacDiagABInv = 1.f / jacDiagAB;

				//Dependent on Rigidbody A and B types, fetch the contact/friction response func
				//perhaps do a similar thing for friction/restutution combiner funcs...
				
				cpd->m_frictionSolverFunc = m_frictionDispatch[body0->m_frictionSolverType][body1->m_frictionSolverType];
				cpd->m_contactSolverFunc = m_contactDispatch[body0->m_contactSolverType][body1->m_contactSolverType];
				
				btVector3 vel1 = body0->getVelocityInLocalPoint(rel_pos1);
				btVector3 vel2 = body1->getVelocityInLocalPoint(rel_pos2);
				btVector3 vel = vel1 - vel2;
				btScalar rel_vel;
				rel_vel = cp.m_normalWorldOnB.dot(vel);
				
				float combinedRestitution = cp.m_combinedRestitution;
				
				cpd->m_penetration = cp.getDistance();
				cpd->m_friction = cp.m_combinedFriction;
				cpd->m_restitution = restitutionCurve(rel_vel, combinedRestitution);
				if (cpd->m_restitution <= 0.) //0.f)
				{
					cpd->m_restitution = 0.0f;

				};
				
				//restitution and penetration work in same direction so
				//rel_vel 
				
				btScalar penVel = -cpd->m_penetration/info.m_timeStep;

				if (cpd->m_restitution > penVel)
				{
					cpd->m_penetration = 0.f;
				} 				
				
				

				float relaxation = info.m_damping;
				if (m_solverMode & SOLVER_USE_WARMSTARTING)
				{
					cpd->m_appliedImpulse *= relaxation;
				} else
				{
					cpd->m_appliedImpulse =0.f;
				}
	
				//for friction
				cpd->m_prevAppliedImpulse = cpd->m_appliedImpulse;
				
				//re-calculate friction direction every frame, todo: check if this is really needed
				btPlaneSpace1(cp.m_normalWorldOnB,cpd->m_frictionWorldTangential0,cpd->m_frictionWorldTangential1);


#define NO_FRICTION_WARMSTART 1

	#ifdef NO_FRICTION_WARMSTART
				cpd->m_accumulatedTangentImpulse0 = 0.f;
				cpd->m_accumulatedTangentImpulse1 = 0.f;
	#endif //NO_FRICTION_WARMSTART
				float denom0 = body0->computeImpulseDenominator(pos1,cpd->m_frictionWorldTangential0);
				float denom1 = body1->computeImpulseDenominator(pos2,cpd->m_frictionWorldTangential0);
				float denom = relaxation/(denom0+denom1);
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

float btSequentialImpulseConstraintSolver::solve(btRigidBody* body0,btRigidBody* body1, btManifoldPoint& cp, const btContactSolverInfo& info,int iter,btIDebugDraw* debugDrawer)
{

	float maxImpulse = 0.f;

	{

		btVector3 color(0,1,0);
		{
			if (cp.getDistance() <= 0.f)
			{

				if (iter == 0)
				{
					if (debugDrawer)
						debugDrawer->drawContactPoint(cp.m_positionWorldOnB,cp.m_normalWorldOnB,cp.getDistance(),cp.getLifeTime(),color);
				}

				{

					btConstraintPersistentData* cpd = (btConstraintPersistentData*) cp.m_userPersistentData;
					float impulse = cpd->m_contactSolverFunc(
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

float btSequentialImpulseConstraintSolver::solveFriction(btRigidBody* body0,btRigidBody* body1, btManifoldPoint& cp, const btContactSolverInfo& info,int iter,btIDebugDraw* debugDrawer)
{


	{

		btVector3 color(0,1,0);
		{
			
			if (cp.getDistance() <= 0.f)
			{

				btConstraintPersistentData* cpd = (btConstraintPersistentData*) cp.m_userPersistentData;
				cpd->m_frictionSolverFunc(
					*body0,*body1,
					cp,
					info);

				
			}
		}

	
	}
	return 0.f;
}
