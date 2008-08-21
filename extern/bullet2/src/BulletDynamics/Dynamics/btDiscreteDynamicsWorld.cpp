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


#include "btDiscreteDynamicsWorld.h"

//collision detection
#include "BulletCollision/CollisionDispatch/btCollisionDispatcher.h"
#include "BulletCollision/BroadphaseCollision/btSimpleBroadphase.h"
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "BulletCollision/CollisionDispatch/btSimulationIslandManager.h"
#include <LinearMath/btTransformUtil.h>

//rigidbody & constraints
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h"
#include "BulletDynamics/ConstraintSolver/btContactSolverInfo.h"
#include "BulletDynamics/ConstraintSolver/btTypedConstraint.h"

//for debug rendering
#include "BulletCollision/CollisionShapes/btBoxShape.h"
#include "BulletCollision/CollisionShapes/btCapsuleShape.h"
#include "BulletCollision/CollisionShapes/btCompoundShape.h"
#include "BulletCollision/CollisionShapes/btConeShape.h"
#include "BulletCollision/CollisionShapes/btConvexTriangleMeshShape.h"
#include "BulletCollision/CollisionShapes/btCylinderShape.h"
#include "BulletCollision/CollisionShapes/btMultiSphereShape.h"
#include "BulletCollision/CollisionShapes/btPolyhedralConvexShape.h"
#include "BulletCollision/CollisionShapes/btSphereShape.h"
#include "BulletCollision/CollisionShapes/btTriangleCallback.h"
#include "BulletCollision/CollisionShapes/btTriangleMeshShape.h"
#include "LinearMath/btIDebugDraw.h"



//vehicle
#include "BulletDynamics/Vehicle/btRaycastVehicle.h"
#include "BulletDynamics/Vehicle/btVehicleRaycaster.h"
#include "BulletDynamics/Vehicle/btWheelInfo.h"
#include "LinearMath/btIDebugDraw.h"
#include "LinearMath/btQuickprof.h"
#include "LinearMath/btMotionState.h"





btDiscreteDynamicsWorld::btDiscreteDynamicsWorld(btDispatcher* dispatcher,btOverlappingPairCache* pairCache,btConstraintSolver* constraintSolver)
:btDynamicsWorld(dispatcher,pairCache),
m_constraintSolver(constraintSolver? constraintSolver: new btSequentialImpulseConstraintSolver),
m_debugDrawer(0),
m_gravity(0,-10,0),
m_localTime(btScalar(1.)/btScalar(60.)),
m_profileTimings(0)
{
	m_islandManager = new btSimulationIslandManager();
	m_ownsIslandManager = true;
	m_ownsConstraintSolver = (constraintSolver==0);
}


btDiscreteDynamicsWorld::~btDiscreteDynamicsWorld()
{
	//only delete it when we created it
	if (m_ownsIslandManager)
		delete m_islandManager;
	if (m_ownsConstraintSolver)
		 delete m_constraintSolver;
}

void	btDiscreteDynamicsWorld::saveKinematicState(btScalar timeStep)
{

	for (int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body)
		{
				btTransform predictedTrans;
				if (body->getActivationState() != ISLAND_SLEEPING)
				{
					if (body->isKinematicObject())
					{
						//to calculate velocities next frame
						body->saveKinematicState(timeStep);
					}
				}
		}
	}
}

void	btDiscreteDynamicsWorld::synchronizeMotionStates()
{
	//debug vehicle wheels
	
	
	{
		//todo: iterate over awake simulation islands!
		for ( int i=0;i<m_collisionObjects.size();i++)
		{
			btCollisionObject* colObj = m_collisionObjects[i];
			if (getDebugDrawer() && getDebugDrawer()->getDebugMode() & btIDebugDraw::DBG_DrawWireframe)
			{
				btVector3 color(btScalar(255.),btScalar(255.),btScalar(255.));
				switch(colObj->getActivationState())
				{
				case  ACTIVE_TAG:
					color = btVector3(btScalar(255.),btScalar(255.),btScalar(255.)); break;
				case ISLAND_SLEEPING:
					color =  btVector3(btScalar(0.),btScalar(255.),btScalar(0.));break;
				case WANTS_DEACTIVATION:
					color = btVector3(btScalar(0.),btScalar(255.),btScalar(255.));break;
				case DISABLE_DEACTIVATION:
					color = btVector3(btScalar(255.),btScalar(0.),btScalar(0.));break;
				case DISABLE_SIMULATION:
					color = btVector3(btScalar(255.),btScalar(255.),btScalar(0.));break;
				default:
					{
						color = btVector3(btScalar(255.),btScalar(0.),btScalar(0.));
					}
				};

				debugDrawObject(colObj->getWorldTransform(),colObj->getCollisionShape(),color);
			}
			btRigidBody* body = btRigidBody::upcast(colObj);
			if (body && body->getMotionState() && !body->isStaticOrKinematicObject())
			{
				//we need to call the update at least once, even for sleeping objects
				//otherwise the 'graphics' transform never updates properly
				//so todo: add 'dirty' flag
				//if (body->getActivationState() != ISLAND_SLEEPING)
				{
					btTransform interpolatedTransform;
					btTransformUtil::integrateTransform(body->getInterpolationWorldTransform(),
						body->getInterpolationLinearVelocity(),body->getInterpolationAngularVelocity(),m_localTime,interpolatedTransform);
					body->getMotionState()->setWorldTransform(interpolatedTransform);
				}
			}
		}
	}

	if (getDebugDrawer() && getDebugDrawer()->getDebugMode() & btIDebugDraw::DBG_DrawWireframe)
	{
		for ( int i=0;i<this->m_vehicles.size();i++)
		{
			for (int v=0;v<m_vehicles[i]->getNumWheels();v++)
			{
				btVector3 wheelColor(0,255,255);
				if (m_vehicles[i]->getWheelInfo(v).m_raycastInfo.m_isInContact)
				{
					wheelColor.setValue(0,0,255);
				} else
				{
					wheelColor.setValue(255,0,255);
				}

				//synchronize the wheels with the (interpolated) chassis worldtransform
				m_vehicles[i]->updateWheelTransform(v,true);
					
				btVector3 wheelPosWS = m_vehicles[i]->getWheelInfo(v).m_worldTransform.getOrigin();

				btVector3 axle = btVector3(	
					m_vehicles[i]->getWheelInfo(v).m_worldTransform.getBasis()[0][m_vehicles[i]->getRightAxis()],
					m_vehicles[i]->getWheelInfo(v).m_worldTransform.getBasis()[1][m_vehicles[i]->getRightAxis()],
					m_vehicles[i]->getWheelInfo(v).m_worldTransform.getBasis()[2][m_vehicles[i]->getRightAxis()]);


				//m_vehicles[i]->getWheelInfo(v).m_raycastInfo.m_wheelAxleWS
				//debug wheels (cylinders)
				m_debugDrawer->drawLine(wheelPosWS,wheelPosWS+axle,wheelColor);
				m_debugDrawer->drawLine(wheelPosWS,m_vehicles[i]->getWheelInfo(v).m_raycastInfo.m_contactPointWS,wheelColor);

			}
		}
	}

}


int	btDiscreteDynamicsWorld::stepSimulation( btScalar timeStep,int maxSubSteps, btScalar fixedTimeStep)
{
	int numSimulationSubSteps = 0;

	if (maxSubSteps)
	{
		//fixed timestep with interpolation
		m_localTime += timeStep;
		if (m_localTime >= fixedTimeStep)
		{
			numSimulationSubSteps = int( m_localTime / fixedTimeStep);
			m_localTime -= numSimulationSubSteps * fixedTimeStep;
		}
	} else
	{
		//variable timestep
		fixedTimeStep = timeStep;
		m_localTime = timeStep;
		if (btFuzzyZero(timeStep))
		{
			numSimulationSubSteps = 0;
			maxSubSteps = 0;
		} else
		{
			numSimulationSubSteps = 1;
			maxSubSteps = 1;
		}
	}

	//process some debugging flags
	if (getDebugDrawer())
	{
		gDisableDeactivation = (getDebugDrawer()->getDebugMode() & btIDebugDraw::DBG_NoDeactivation) != 0;
	}
	if (numSimulationSubSteps)
	{

		saveKinematicState(fixedTimeStep);

		//clamp the number of substeps, to prevent simulation grinding spiralling down to a halt
		int clampedSimulationSteps = (numSimulationSubSteps > maxSubSteps)? maxSubSteps : numSimulationSubSteps;

		for (int i=0;i<clampedSimulationSteps;i++)
		{
			internalSingleStepSimulation(fixedTimeStep);
			synchronizeMotionStates();
		}

	} 

	synchronizeMotionStates();

	return numSimulationSubSteps;
}

void	btDiscreteDynamicsWorld::internalSingleStepSimulation(btScalar timeStep)
{
	
	startProfiling(timeStep);

	///update aabbs information
	updateAabbs();

	///apply gravity, predict motion
	predictUnconstraintMotion(timeStep);

	btDispatcherInfo& dispatchInfo = getDispatchInfo();

	dispatchInfo.m_timeStep = timeStep;
	dispatchInfo.m_stepCount = 0;
	dispatchInfo.m_debugDraw = getDebugDrawer();

	///perform collision detection
	performDiscreteCollisionDetection();

	calculateSimulationIslands();

	
	getSolverInfo().m_timeStep = timeStep;
	


	///solve contact and other joint constraints
	solveConstraints(getSolverInfo());
	
	///CallbackTriggers();

	///integrate transforms
	integrateTransforms(timeStep);

	///update vehicle simulation
	updateVehicles(timeStep);


	updateActivationState( timeStep );

	

}

void	btDiscreteDynamicsWorld::setGravity(const btVector3& gravity)
{
	m_gravity = gravity;
	for ( int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body)
		{
			body->setGravity(gravity);
		}
	}
}


void	btDiscreteDynamicsWorld::removeRigidBody(btRigidBody* body)
{
	removeCollisionObject(body);
}

void	btDiscreteDynamicsWorld::addRigidBody(btRigidBody* body)
{
	if (!body->isStaticOrKinematicObject())
	{
		body->setGravity(m_gravity);
	}

	if (body->getCollisionShape())
	{
		bool isDynamic = !(body->isStaticObject() || body->isKinematicObject());
		short collisionFilterGroup = isDynamic? short(btBroadphaseProxy::DefaultFilter) : short(btBroadphaseProxy::StaticFilter);
		short collisionFilterMask = isDynamic? 	short(btBroadphaseProxy::AllFilter) : 	short(btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::StaticFilter);

		addCollisionObject(body,collisionFilterGroup,collisionFilterMask);
	}
}

void	btDiscreteDynamicsWorld::addRigidBody(btRigidBody* body, short group, short mask)
{
	if (!body->isStaticOrKinematicObject())
	{
		body->setGravity(m_gravity);
	}

	if (body->getCollisionShape())
	{
		addCollisionObject(body,group,mask);
	}
}


void	btDiscreteDynamicsWorld::updateVehicles(btScalar timeStep)
{
	BEGIN_PROFILE("updateVehicles");

	for ( int i=0;i<m_vehicles.size();i++)
	{
		btRaycastVehicle* vehicle = m_vehicles[i];
		vehicle->updateVehicle( timeStep);
	}
	END_PROFILE("updateVehicles");
}

void	btDiscreteDynamicsWorld::updateActivationState(btScalar timeStep)
{
	BEGIN_PROFILE("updateActivationState");

	for ( int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body)
		{
			body->updateDeactivation(timeStep);

			if (body->wantsSleeping())
			{
				if (body->isStaticOrKinematicObject())
				{
					body->setActivationState(ISLAND_SLEEPING);
				} else
				{
					if (body->getActivationState() == ACTIVE_TAG)
						body->setActivationState( WANTS_DEACTIVATION );
				}
			} else
			{
				if (body->getActivationState() != DISABLE_DEACTIVATION)
					body->setActivationState( ACTIVE_TAG );
			}
		}
	}
	END_PROFILE("updateActivationState");
}

void	btDiscreteDynamicsWorld::addConstraint(btTypedConstraint* constraint,bool disableCollisionsBetweenLinkedBodies)
{
	m_constraints.push_back(constraint);
	if (disableCollisionsBetweenLinkedBodies)
	{
		constraint->getRigidBodyA().addConstraintRef(constraint);
		constraint->getRigidBodyB().addConstraintRef(constraint);
	}
}

void	btDiscreteDynamicsWorld::removeConstraint(btTypedConstraint* constraint)
{
	m_constraints.remove(constraint);
	constraint->getRigidBodyA().removeConstraintRef(constraint);
	constraint->getRigidBodyB().removeConstraintRef(constraint);
}

void	btDiscreteDynamicsWorld::addVehicle(btRaycastVehicle* vehicle)
{
	m_vehicles.push_back(vehicle);
}

void	btDiscreteDynamicsWorld::removeVehicle(btRaycastVehicle* vehicle)
{
	m_vehicles.remove(vehicle);
}

inline	int	btGetConstraintIslandId(const btTypedConstraint* lhs)
{
	int islandId;
	
	const btCollisionObject& rcolObj0 = lhs->getRigidBodyA();
	const btCollisionObject& rcolObj1 = lhs->getRigidBodyB();
	islandId= rcolObj0.getIslandTag()>=0?rcolObj0.getIslandTag():rcolObj1.getIslandTag();
	return islandId;

}


class btSortConstraintOnIslandPredicate
{
	public:

		bool operator() ( const btTypedConstraint* lhs, const btTypedConstraint* rhs )
		{
			int rIslandId0,lIslandId0;
			rIslandId0 = btGetConstraintIslandId(rhs);
			lIslandId0 = btGetConstraintIslandId(lhs);
			return lIslandId0 < rIslandId0;
		}
};




void	btDiscreteDynamicsWorld::solveConstraints(btContactSolverInfo& solverInfo)
{
	
	struct InplaceSolverIslandCallback : public btSimulationIslandManager::IslandCallback
	{

		btContactSolverInfo&	m_solverInfo;
		btConstraintSolver*		m_solver;
		btTypedConstraint**		m_sortedConstraints;
		int						m_numConstraints;
		btIDebugDraw*			m_debugDrawer;
		btStackAlloc*			m_stackAlloc;


		InplaceSolverIslandCallback(
			btContactSolverInfo& solverInfo,
			btConstraintSolver*	solver,
			btTypedConstraint** sortedConstraints,
			int	numConstraints,
			btIDebugDraw*	debugDrawer,
			btStackAlloc*			stackAlloc)
			:m_solverInfo(solverInfo),
			m_solver(solver),
			m_sortedConstraints(sortedConstraints),
			m_numConstraints(numConstraints),
			m_debugDrawer(debugDrawer),
			m_stackAlloc(stackAlloc)
		{

		}

		InplaceSolverIslandCallback& operator=(InplaceSolverIslandCallback& other)
		{
			btAssert(0);
			(void)other;
			return *this;
		}
		virtual	void	ProcessIsland(btCollisionObject** bodies,int numBodies,btPersistentManifold**	manifolds,int numManifolds, int islandId)
		{
			//also add all non-contact constraints/joints for this island
			btTypedConstraint** startConstraint = 0;
			int numCurConstraints = 0;
			int i;
			
			//find the first constraint for this island
			for (i=0;i<m_numConstraints;i++)
			{
				if (btGetConstraintIslandId(m_sortedConstraints[i]) == islandId)
				{
					startConstraint = &m_sortedConstraints[i];
					break;
				}
			}
			//count the number of constraints in this island
			for (;i<m_numConstraints;i++)
			{
				if (btGetConstraintIslandId(m_sortedConstraints[i]) == islandId)
				{
					numCurConstraints++;
				}
			}

			m_solver->solveGroup( bodies,numBodies,manifolds, numManifolds,startConstraint,numCurConstraints,m_solverInfo,m_debugDrawer,m_stackAlloc);
		}

	};

	//sorted version of all btTypedConstraint, based on islandId
	btAlignedObjectArray<btTypedConstraint*>	sortedConstraints;
	sortedConstraints.resize( m_constraints.size());
	int i; 
	for (i=0;i<getNumConstraints();i++)
	{
		sortedConstraints[i] = m_constraints[i];
	}

//	assert(0);
		
	

	sortedConstraints.heapSort(btSortConstraintOnIslandPredicate());
	
	btTypedConstraint** constraintsPtr = getNumConstraints() ? &sortedConstraints[0] : 0;
	
	InplaceSolverIslandCallback	solverCallback(	solverInfo,	m_constraintSolver, constraintsPtr,sortedConstraints.size(),	m_debugDrawer,m_stackAlloc);

	
	
	/// solve all the constraints for this island
	m_islandManager->buildAndProcessIslands(getCollisionWorld()->getDispatcher(),getCollisionWorld()->getCollisionObjectArray(),&solverCallback);


}




void	btDiscreteDynamicsWorld::calculateSimulationIslands()
{
	BEGIN_PROFILE("calculateSimulationIslands");

	getSimulationIslandManager()->updateActivationState(getCollisionWorld(),getCollisionWorld()->getDispatcher());

	{
		int i;
		int numConstraints = int(m_constraints.size());
		for (i=0;i< numConstraints ; i++ )
		{
			btTypedConstraint* constraint = m_constraints[i];

			const btRigidBody* colObj0 = &constraint->getRigidBodyA();
			const btRigidBody* colObj1 = &constraint->getRigidBodyB();

			if (((colObj0) && ((colObj0)->mergesSimulationIslands())) &&
				((colObj1) && ((colObj1)->mergesSimulationIslands())))
			{
				if (colObj0->isActive() || colObj1->isActive())
				{

					getSimulationIslandManager()->getUnionFind().unite((colObj0)->getIslandTag(),
						(colObj1)->getIslandTag());
				}
			}
		}
	}

	//Store the island id in each body
	getSimulationIslandManager()->storeIslandActivationState(getCollisionWorld());

	END_PROFILE("calculateSimulationIslands");

}


void	btDiscreteDynamicsWorld::updateAabbs()
{
	BEGIN_PROFILE("updateAabbs");
	
	btVector3 colorvec(1,0,0);
	btTransform predictedTrans;
	for ( int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body)
		{
		//	if (body->IsActive() && (!body->IsStatic()))
			{
				btPoint3 minAabb,maxAabb;
				colObj->getCollisionShape()->getAabb(colObj->getWorldTransform(), minAabb,maxAabb);
				btBroadphaseInterface* bp = (btBroadphaseInterface*)m_broadphasePairCache;

				//moving objects should be moderately sized, probably something wrong if not
				if ( colObj->isStaticObject() || ((maxAabb-minAabb).length2() < btScalar(1e12)))
				{
					bp->setAabb(body->getBroadphaseHandle(),minAabb,maxAabb);
				} else
				{
					//something went wrong, investigate
					//this assert is unwanted in 3D modelers (danger of loosing work)
					body->setActivationState(DISABLE_SIMULATION);
					
					static bool reportMe = true;
					if (reportMe && m_debugDrawer)
					{
						reportMe = false;
						m_debugDrawer->reportErrorWarning("Overflow in AABB, object removed from simulation");
						m_debugDrawer->reportErrorWarning("If you can reproduce this, please email bugs@continuousphysics.com\n");
						m_debugDrawer->reportErrorWarning("Please include above information, your Platform, version of OS.\n");
						m_debugDrawer->reportErrorWarning("Thanks.\n");
					}


				}
				if (m_debugDrawer && (m_debugDrawer->getDebugMode() & btIDebugDraw::DBG_DrawAabb))
				{
					m_debugDrawer->drawAabb(minAabb,maxAabb,colorvec);
				}
			}
		}
	}
	
	END_PROFILE("updateAabbs");
}

void	btDiscreteDynamicsWorld::integrateTransforms(btScalar timeStep)
{
	BEGIN_PROFILE("integrateTransforms");
	btTransform predictedTrans;
	for ( int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body)
		{
			if (body->isActive() && (!body->isStaticOrKinematicObject()))
			{
				body->predictIntegratedTransform(timeStep, predictedTrans);
				body->proceedToTransform( predictedTrans);
			}
		}
	}
	END_PROFILE("integrateTransforms");
}



void	btDiscreteDynamicsWorld::predictUnconstraintMotion(btScalar timeStep)
{
	BEGIN_PROFILE("predictUnconstraintMotion");
	for ( int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body)
		{
			if (!body->isStaticOrKinematicObject())
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
	END_PROFILE("predictUnconstraintMotion");
}


void	btDiscreteDynamicsWorld::startProfiling(btScalar timeStep)
{
	(void)timeStep;
	#ifdef USE_QUICKPROF


	//toggle btProfiler
	if ( m_debugDrawer && m_debugDrawer->getDebugMode() & btIDebugDraw::DBG_ProfileTimings)
	{
		if (!m_profileTimings)
		{
			m_profileTimings = 1;
			// To disable profiling, simply comment out the following line.
			static int counter = 0;

			char filename[128];
			sprintf(filename,"quickprof_bullet_timings%i.csv",counter++);
			btProfiler::init(filename, btProfiler::BLOCK_CYCLE_SECONDS);//BLOCK_TOTAL_MICROSECONDS
		} else
		{
			btProfiler::endProfilingCycle();
		}

	} else
	{
		if (m_profileTimings)
		{
			btProfiler::endProfilingCycle();

			m_profileTimings = 0;
			btProfiler::destroy();
		}
	}
#endif //USE_QUICKPROF
}




	

class DebugDrawcallback : public btTriangleCallback, public btInternalTriangleIndexCallback
{
	btIDebugDraw*	m_debugDrawer;
	btVector3	m_color;
	btTransform	m_worldTrans;

public:

	DebugDrawcallback(btIDebugDraw*	debugDrawer,const btTransform& worldTrans,const btVector3& color) :
                m_debugDrawer(debugDrawer),
		m_color(color),
		m_worldTrans(worldTrans)
	{
	}

	virtual void internalProcessTriangleIndex(btVector3* triangle,int partId,int  triangleIndex)
	{
		processTriangle(triangle,partId,triangleIndex);
	}

	virtual void processTriangle(btVector3* triangle,int partId, int triangleIndex)
	{
		(void)partId;
		(void)triangleIndex;

		btVector3 wv0,wv1,wv2;
		wv0 = m_worldTrans*triangle[0];
		wv1 = m_worldTrans*triangle[1];
		wv2 = m_worldTrans*triangle[2];
		m_debugDrawer->drawLine(wv0,wv1,m_color);
		m_debugDrawer->drawLine(wv1,wv2,m_color);
		m_debugDrawer->drawLine(wv2,wv0,m_color);
	}
};

void btDiscreteDynamicsWorld::debugDrawSphere(btScalar radius, const btTransform& transform, const btVector3& color)
{
	btVector3 start = transform.getOrigin();

	const btVector3 xoffs = transform.getBasis() * btVector3(radius,0,0);
	const btVector3 yoffs = transform.getBasis() * btVector3(0,radius,0);
	const btVector3 zoffs = transform.getBasis() * btVector3(0,0,radius);

	// XY 
	getDebugDrawer()->drawLine(start-xoffs, start+yoffs, color);
	getDebugDrawer()->drawLine(start+yoffs, start+xoffs, color);
	getDebugDrawer()->drawLine(start+xoffs, start-yoffs, color);
	getDebugDrawer()->drawLine(start-yoffs, start-xoffs, color);

	// XZ
	getDebugDrawer()->drawLine(start-xoffs, start+zoffs, color);
	getDebugDrawer()->drawLine(start+zoffs, start+xoffs, color);
	getDebugDrawer()->drawLine(start+xoffs, start-zoffs, color);
	getDebugDrawer()->drawLine(start-zoffs, start-xoffs, color);

	// YZ
	getDebugDrawer()->drawLine(start-yoffs, start+zoffs, color);
	getDebugDrawer()->drawLine(start+zoffs, start+yoffs, color);
	getDebugDrawer()->drawLine(start+yoffs, start-zoffs, color);
	getDebugDrawer()->drawLine(start-zoffs, start-yoffs, color);
}

void btDiscreteDynamicsWorld::debugDrawObject(const btTransform& worldTransform, const btCollisionShape* shape, const btVector3& color)
{
	// Draw a small simplex at the center of the object
	{
		btVector3 start = worldTransform.getOrigin();
		getDebugDrawer()->drawLine(start, start+worldTransform.getBasis() * btVector3(1,0,0), btVector3(1,0,0));
		getDebugDrawer()->drawLine(start, start+worldTransform.getBasis() * btVector3(0,1,0), btVector3(0,1,0));
		getDebugDrawer()->drawLine(start, start+worldTransform.getBasis() * btVector3(0,0,1), btVector3(0,0,1));
	}

	if (shape->getShapeType() == COMPOUND_SHAPE_PROXYTYPE)
	{
		const btCompoundShape* compoundShape = static_cast<const btCompoundShape*>(shape);
		for (int i=compoundShape->getNumChildShapes()-1;i>=0;i--)
		{
			btTransform childTrans = compoundShape->getChildTransform(i);
			const btCollisionShape* colShape = compoundShape->getChildShape(i);
			debugDrawObject(worldTransform*childTrans,colShape,color);
		}

	} else
	{
		switch (shape->getShapeType())
		{

		case SPHERE_SHAPE_PROXYTYPE:
			{
				const btSphereShape* sphereShape = static_cast<const btSphereShape*>(shape);
				btScalar radius = sphereShape->getMargin();//radius doesn't include the margin, so draw with margin
				
				debugDrawSphere(radius, worldTransform, color);
				break;
			}
		case MULTI_SPHERE_SHAPE_PROXYTYPE:
			{
				const btMultiSphereShape* multiSphereShape = static_cast<const btMultiSphereShape*>(shape);

				for (int i = multiSphereShape->getSphereCount()-1; i>=0;i--)
				{
					btTransform childTransform = worldTransform;
					childTransform.getOrigin() += multiSphereShape->getSpherePosition(i);
					debugDrawSphere(multiSphereShape->getSphereRadius(i), childTransform, color);
				}

				break;
			}
		case CAPSULE_SHAPE_PROXYTYPE:
			{
				const btCapsuleShape* capsuleShape = static_cast<const btCapsuleShape*>(shape);

				btScalar radius = capsuleShape->getRadius();
				btScalar halfHeight = capsuleShape->getHalfHeight();

				// Draw the ends
				{
					btTransform childTransform = worldTransform;
					childTransform.getOrigin() = worldTransform * btVector3(0,halfHeight,0);
					debugDrawSphere(radius, childTransform, color);
				}

				{
					btTransform childTransform = worldTransform;
					childTransform.getOrigin() = worldTransform * btVector3(0,-halfHeight,0);
					debugDrawSphere(radius, childTransform, color);
				}

				// Draw some additional lines
				btVector3 start = worldTransform.getOrigin();
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * btVector3(-radius,halfHeight,0),start+worldTransform.getBasis() * btVector3(-radius,-halfHeight,0), color);
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * btVector3(radius,halfHeight,0),start+worldTransform.getBasis() * btVector3(radius,-halfHeight,0), color);
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * btVector3(0,halfHeight,-radius),start+worldTransform.getBasis() * btVector3(0,-halfHeight,-radius), color);
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * btVector3(0,halfHeight,radius),start+worldTransform.getBasis() * btVector3(0,-halfHeight,radius), color);

				break;
			}
		case CONE_SHAPE_PROXYTYPE:
			{
				const btConeShape* coneShape = static_cast<const btConeShape*>(shape);
				btScalar radius = coneShape->getRadius();//+coneShape->getMargin();
				btScalar height = coneShape->getHeight();//+coneShape->getMargin();
				btVector3 start = worldTransform.getOrigin();
				// insert here Bullet 2.69 that fixes representation of cone
				int upAxis= coneShape->getConeUpIndex();
				
				btVector3	offsetHeight(0,0,0);
				offsetHeight[upAxis] = height * btScalar(0.5);
				btVector3	offsetRadius(0,0,0);
				offsetRadius[(upAxis+1)%3] = radius;
				btVector3	offset2Radius(0,0,0);
				offset2Radius[(upAxis+2)%3] = radius;

				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * (offsetHeight),start+worldTransform.getBasis() * (-offsetHeight+offsetRadius),color);
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * (offsetHeight),start+worldTransform.getBasis() * (-offsetHeight-offsetRadius),color);
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * (offsetHeight),start+worldTransform.getBasis() * (-offsetHeight+offset2Radius),color);
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * (offsetHeight),start+worldTransform.getBasis() * (-offsetHeight-offset2Radius),color);

				// buggy code that does not take into account the direction of the cone
				//getDebugDrawer()->drawLine(start+worldTransform.getBasis() * btVector3(btScalar(0.),btScalar(0.),btScalar(0.5)*height),start+worldTransform.getBasis() * btVector3(radius,btScalar(0.),btScalar(-0.5)*height),color);
				//getDebugDrawer()->drawLine(start+worldTransform.getBasis() * btVector3(btScalar(0.),btScalar(0.),btScalar(0.5)*height),start+worldTransform.getBasis() * btVector3(-radius,btScalar(0.),btScalar(-0.5)*height),color);
				//getDebugDrawer()->drawLine(start+worldTransform.getBasis() * btVector3(btScalar(0.),btScalar(0.),btScalar(0.5)*height),start+worldTransform.getBasis() * btVector3(btScalar(0.),radius,btScalar(-0.5)*height),color);
				//getDebugDrawer()->drawLine(start+worldTransform.getBasis() * btVector3(btScalar(0.),btScalar(0.),btScalar(0.5)*height),start+worldTransform.getBasis() * btVector3(btScalar(0.),-radius,btScalar(-0.5)*height),color);
				break;

			}
		case CYLINDER_SHAPE_PROXYTYPE:
			{
				const btCylinderShape* cylinder = static_cast<const btCylinderShape*>(shape);
				int upAxis = cylinder->getUpAxis();
				btScalar radius = cylinder->getRadius();
				btScalar halfHeight = cylinder->getHalfExtents()[upAxis];
				btVector3 start = worldTransform.getOrigin();
				btVector3	offsetHeight(0,0,0);
				offsetHeight[upAxis] = halfHeight;
				btVector3	offsetRadius(0,0,0);
				offsetRadius[(upAxis+1)%3] = radius;
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * (offsetHeight+offsetRadius),start+worldTransform.getBasis() * (-offsetHeight+offsetRadius),color);
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * (offsetHeight-offsetRadius),start+worldTransform.getBasis() * (-offsetHeight-offsetRadius),color);
				break;
			}
		default:
			{

				if (shape->isConcave())
				{
					btConcaveShape* concaveMesh = (btConcaveShape*) shape;
					
					//todo pass camera, for some culling
					btVector3 aabbMax(btScalar(1e30),btScalar(1e30),btScalar(1e30));
					btVector3 aabbMin(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30));

					DebugDrawcallback drawCallback(getDebugDrawer(),worldTransform,color);
					concaveMesh->processAllTriangles(&drawCallback,aabbMin,aabbMax);

				}

				if (shape->getShapeType() == CONVEX_TRIANGLEMESH_SHAPE_PROXYTYPE)
				{
					btConvexTriangleMeshShape* convexMesh = (btConvexTriangleMeshShape*) shape;
					//todo: pass camera for some culling			
					btVector3 aabbMax(btScalar(1e30),btScalar(1e30),btScalar(1e30));
					btVector3 aabbMin(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30));
					//DebugDrawcallback drawCallback;
					DebugDrawcallback drawCallback(getDebugDrawer(),worldTransform,color);
					convexMesh->getStridingMesh()->InternalProcessAllTriangles(&drawCallback,aabbMin,aabbMax);
				}


				/// for polyhedral shapes
				if (shape->isPolyhedral())
				{
					btPolyhedralConvexShape* polyshape = (btPolyhedralConvexShape*) shape;

					int i;
					for (i=0;i<polyshape->getNumEdges();i++)
					{
						btPoint3 a,b;
						polyshape->getEdge(i,a,b);
						btVector3 wa = worldTransform * a;
						btVector3 wb = worldTransform * b;
						getDebugDrawer()->drawLine(wa,wb,color);

					}

					
				}
			}
		}
	}
}


void	btDiscreteDynamicsWorld::setConstraintSolver(btConstraintSolver* solver)
{
	if (m_ownsConstraintSolver)
	{
		delete m_constraintSolver;
	}
	m_ownsConstraintSolver = false;
	m_constraintSolver = solver;
}

int		btDiscreteDynamicsWorld::getNumConstraints() const
{
	return int(m_constraints.size());
}
btTypedConstraint* btDiscreteDynamicsWorld::getConstraint(int index)
{
	return m_constraints[index];
}
const btTypedConstraint* btDiscreteDynamicsWorld::getConstraint(int index) const
{
	return m_constraints[index];
}
