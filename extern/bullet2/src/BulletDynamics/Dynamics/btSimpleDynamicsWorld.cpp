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

#include "btSimpleDynamicsWorld.h"
#include "BulletCollision/CollisionDispatch/btCollisionDispatcher.h"
#include "BulletCollision/BroadphaseCollision/btSimpleBroadphase.h"
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h"
#include "BulletDynamics/ConstraintSolver/btContactSolverInfo.h"


btSimpleDynamicsWorld::btSimpleDynamicsWorld()
:m_constraintSolver(new btSequentialImpulseConstraintSolver),
m_ownsConstraintSolver(true),
m_debugDrawer(0),
m_gravity(0,0,-10)
{
}

btSimpleDynamicsWorld::btSimpleDynamicsWorld(btDispatcher* dispatcher,btOverlappingPairCache* pairCache,btConstraintSolver* constraintSolver)
:btDynamicsWorld(dispatcher,pairCache),
m_constraintSolver(constraintSolver),
m_ownsConstraintSolver(false),
m_debugDrawer(0),
m_gravity(0,0,-10)
{

}


btSimpleDynamicsWorld::~btSimpleDynamicsWorld()
{
	if (m_ownsConstraintSolver)
		delete m_constraintSolver;
}

int		btSimpleDynamicsWorld::stepSimulation( float timeStep,int maxSubSteps, float fixedTimeStep)
{
	///apply gravity, predict motion
	predictUnconstraintMotion(timeStep);

	btDispatcherInfo	dispatchInfo;
	dispatchInfo.m_timeStep = timeStep;
	dispatchInfo.m_stepCount = 0;
	dispatchInfo.m_debugDraw = getDebugDrawer();

	///perform collision detection
	performDiscreteCollisionDetection(dispatchInfo );

	///solve contact constraints
	int numManifolds = m_dispatcher1->getNumManifolds();
	if (numManifolds)
	{
		btPersistentManifold** manifoldPtr = ((btCollisionDispatcher*)m_dispatcher1)->getInternalManifoldPointer();
		
		btContactSolverInfo infoGlobal;
		infoGlobal.m_timeStep = timeStep;
		
		m_constraintSolver->solveGroup(manifoldPtr, numManifolds,infoGlobal,m_debugDrawer);
	}

	///integrate transforms
	integrateTransforms(timeStep);
		
	updateAabbs();

	synchronizeMotionStates();

	return 1;

}


void	btSimpleDynamicsWorld::setGravity(const btVector3& gravity)
{
	m_gravity = gravity;
	for (unsigned int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body)
		{
			body->setGravity(gravity);
		}
	}
}

void	btSimpleDynamicsWorld::removeRigidBody(btRigidBody* body)
{
	removeCollisionObject(body);
}

void	btSimpleDynamicsWorld::addRigidBody(btRigidBody* body)
{
	body->setGravity(m_gravity);

	addCollisionObject(body);
}

void	btSimpleDynamicsWorld::updateAabbs()
{
	btTransform predictedTrans;
	for (unsigned int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body)
		{
			if (body->isActive() && (!body->isStaticObject()))
			{
				btPoint3 minAabb,maxAabb;
				colObj->getCollisionShape()->getAabb(colObj->getWorldTransform(), minAabb,maxAabb);
				btBroadphaseInterface* bp = getBroadphase();
				bp->setAabb(body->getBroadphaseHandle(),minAabb,maxAabb);
			}
		}
	}
}

void	btSimpleDynamicsWorld::integrateTransforms(float timeStep)
{
	btTransform predictedTrans;
	for (unsigned int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body)
		{
			if (body->isActive() && (!body->isStaticObject()))
			{
				body->predictIntegratedTransform(timeStep, predictedTrans);
				body->proceedToTransform( predictedTrans);
			}
		}
	}
}



void	btSimpleDynamicsWorld::predictUnconstraintMotion(float timeStep)
{
	for (unsigned int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body)
		{
			if (!body->isStaticObject())
			{
				if (body->isActive())
				{
					body->applyForces( timeStep);
					body->integrateVelocities( timeStep);
					body->predictIntegratedTransform(timeStep,body->getInterpolationWorldTransform());
				}
			}
		}
	}
}


void	btSimpleDynamicsWorld::synchronizeMotionStates()
{
	//todo: iterate over awake simulation islands!
	for (unsigned int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body && body->getMotionState())
		{
			if (body->getActivationState() != ISLAND_SLEEPING)
			{
				body->getMotionState()->setWorldTransform(body->getWorldTransform());
			}
		}
	}

}


void	btSimpleDynamicsWorld::setConstraintSolver(btConstraintSolver* solver)
{
	if (m_ownsConstraintSolver)
	{
		delete m_constraintSolver;
	}
	m_ownsConstraintSolver = false;
	m_constraintSolver = solver;
}
