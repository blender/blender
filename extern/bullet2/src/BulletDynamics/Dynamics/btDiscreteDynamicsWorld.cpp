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
#include "BulletCollision/CollisionShapes/btCompoundShape.h"
#include "BulletCollision/CollisionShapes/btSphereShape.h"
#include "BulletCollision/CollisionShapes/btBoxShape.h"
#include "BulletCollision/CollisionShapes/btCylinderShape.h"
#include "BulletCollision/CollisionShapes/btConeShape.h"
#include "BulletCollision/CollisionShapes/btTriangleMeshShape.h"
#include "BulletCollision/CollisionShapes/btPolyhedralConvexShape.h"
#include "BulletCollision/CollisionShapes/btConvexTriangleMeshShape.h"
#include "BulletCollision/CollisionShapes/btTriangleCallback.h"
#include "LinearMath/btIDebugDraw.h"



//vehicle
#include "BulletDynamics/Vehicle/btRaycastVehicle.h"
#include "BulletDynamics/Vehicle/btVehicleRaycaster.h"
#include "BulletDynamics/Vehicle/btWheelInfo.h"
#include "LinearMath/btIDebugDraw.h"
#include "LinearMath/btQuickprof.h"
#include "LinearMath/btMotionState.h"



#include <algorithm>


btDiscreteDynamicsWorld::btDiscreteDynamicsWorld(btConstraintSolver* constraintSolver)
:btDynamicsWorld(),
m_constraintSolver(constraintSolver? constraintSolver: new btSequentialImpulseConstraintSolver),
m_debugDrawer(0),
m_gravity(0,-10,0),
m_localTime(1.f/60.f),
m_profileTimings(0)
{
	m_islandManager = new btSimulationIslandManager();
	m_ownsIslandManager = true;
	m_ownsConstraintSolver = (constraintSolver==0);
}


btDiscreteDynamicsWorld::btDiscreteDynamicsWorld(btDispatcher* dispatcher,btOverlappingPairCache* pairCache,btConstraintSolver* constraintSolver)
:btDynamicsWorld(dispatcher,pairCache),
m_constraintSolver(constraintSolver? constraintSolver: new btSequentialImpulseConstraintSolver),
m_debugDrawer(0),
m_gravity(0,-10,0),
m_localTime(1.f/60.f),
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

void	btDiscreteDynamicsWorld::saveKinematicState(float timeStep)
{

	for (unsigned int i=0;i<m_collisionObjects.size();i++)
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
		for (unsigned int i=0;i<m_collisionObjects.size();i++)
		{
			btCollisionObject* colObj = m_collisionObjects[i];
			if (getDebugDrawer() && getDebugDrawer()->getDebugMode() & btIDebugDraw::DBG_DrawWireframe)
			{
				btVector3 color(255.f,255.f,255.f);
				switch(colObj->getActivationState())
				{
				case  ACTIVE_TAG:
					color = btVector3(255.f,255.f,255.f); break;
				case ISLAND_SLEEPING:
					color =  btVector3(0.f,255.f,0.f);break;
				case WANTS_DEACTIVATION:
					color = btVector3(0.f,255.f,255.f);break;
				case DISABLE_DEACTIVATION:
					color = btVector3(255.f,0.f,0.f);break;
				case DISABLE_SIMULATION:
					color = btVector3(255.f,255.f,0.f);break;
				default:
					{
						color = btVector3(255.f,0.f,0.f);
					}
				};

				debugDrawObject(colObj->getWorldTransform(),colObj->getCollisionShape(),color);
			}
			btRigidBody* body = btRigidBody::upcast(colObj);
			if (body && body->getMotionState() && !body->isStaticOrKinematicObject())
			{
				if (body->getActivationState() != ISLAND_SLEEPING)
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
		for (unsigned int i=0;i<this->m_vehicles.size();i++)
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


int	btDiscreteDynamicsWorld::stepSimulation( float timeStep,int maxSubSteps, float fixedTimeStep)
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

void	btDiscreteDynamicsWorld::internalSingleStepSimulation(float timeStep)
{
	
	startProfiling(timeStep);

	///update aabbs information
	updateAabbs();

	///apply gravity, predict motion
	predictUnconstraintMotion(timeStep);

	btDispatcherInfo	dispatchInfo;
	dispatchInfo.m_timeStep = timeStep;
	dispatchInfo.m_stepCount = 0;
	dispatchInfo.m_debugDraw = getDebugDrawer();


	///perform collision detection
	performDiscreteCollisionDetection(dispatchInfo);

	calculateSimulationIslands();

	btContactSolverInfo infoGlobal;
	infoGlobal.m_timeStep = timeStep;
	


	///solve non-contact constraints
	solveNoncontactConstraints(infoGlobal);
	
	///solve contact constraints
	solveContactConstraints(infoGlobal);

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
		short collisionFilterGroup = isDynamic? btBroadphaseProxy::DefaultFilter : btBroadphaseProxy::StaticFilter;
		short collisionFilterMask = isDynamic? 	btBroadphaseProxy::AllFilter : 	btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::StaticFilter;

		addCollisionObject(body,collisionFilterGroup,collisionFilterMask);
	}
}


void	btDiscreteDynamicsWorld::updateVehicles(float timeStep)
{
	BEGIN_PROFILE("updateVehicles");

	for (unsigned int i=0;i<m_vehicles.size();i++)
	{
		btRaycastVehicle* vehicle = m_vehicles[i];
		vehicle->updateVehicle( timeStep);
	}
	END_PROFILE("updateVehicles");
}

void	btDiscreteDynamicsWorld::updateActivationState(float timeStep)
{
	BEGIN_PROFILE("updateActivationState");

	for (unsigned int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body)
		{
			body->updateDeactivation(timeStep);

			if (body->wantsSleeping())
			{
				if (body->getActivationState() == ACTIVE_TAG)
					body->setActivationState( WANTS_DEACTIVATION );
			} else
			{
				if (body->getActivationState() != DISABLE_DEACTIVATION)
					body->setActivationState( ACTIVE_TAG );
			}
		}
	}
	END_PROFILE("updateActivationState");
}

void	btDiscreteDynamicsWorld::addConstraint(btTypedConstraint* constraint)
{
	m_constraints.push_back(constraint);
}

void	btDiscreteDynamicsWorld::removeConstraint(btTypedConstraint* constraint)
{
	std::vector<btTypedConstraint*>::iterator cit = std::find(m_constraints.begin(),m_constraints.end(),constraint);
	if (!(cit==m_constraints.end()))
	{
		m_constraints.erase(cit);
	}
}

void	btDiscreteDynamicsWorld::addVehicle(btRaycastVehicle* vehicle)
{
	m_vehicles.push_back(vehicle);
}

void	btDiscreteDynamicsWorld::removeVehicle(btRaycastVehicle* vehicle)
{
	std::vector<btRaycastVehicle*>::iterator vit = std::find(m_vehicles.begin(),m_vehicles.end(),vehicle);
	if (!(vit==m_vehicles.end()))
	{
		m_vehicles.erase(vit);
	}
}


void	btDiscreteDynamicsWorld::solveContactConstraints(btContactSolverInfo& solverInfo)
{
	
	BEGIN_PROFILE("solveContactConstraints");

	struct InplaceSolverIslandCallback : public btSimulationIslandManager::IslandCallback
	{

		btContactSolverInfo& m_solverInfo;
		btConstraintSolver*	m_solver;
		btIDebugDraw*	m_debugDrawer;

		InplaceSolverIslandCallback(
			btContactSolverInfo& solverInfo,
			btConstraintSolver*	solver,
			btIDebugDraw*	debugDrawer)
			:m_solverInfo(solverInfo),
			m_solver(solver),
			m_debugDrawer(debugDrawer)
		{

		}

		virtual	void	ProcessIsland(btPersistentManifold**	manifolds,int numManifolds)
		{
			m_solver->solveGroup( manifolds, numManifolds,m_solverInfo,m_debugDrawer);
		}

	};

	
	InplaceSolverIslandCallback	solverCallback(	solverInfo,	m_constraintSolver,	m_debugDrawer);

	
	/// solve all the contact points and contact friction
	m_islandManager->buildAndProcessIslands(getCollisionWorld()->getDispatcher(),getCollisionWorld()->getCollisionObjectArray(),&solverCallback);

	END_PROFILE("solveContactConstraints");

}


void	btDiscreteDynamicsWorld::solveNoncontactConstraints(btContactSolverInfo& solverInfo)
{
	BEGIN_PROFILE("solveNoncontactConstraints");

	int i;
	int numConstraints = int(m_constraints.size());

	///constraint preparation: building jacobians
	for (i=0;i< numConstraints ; i++ )
	{
		btTypedConstraint* constraint = m_constraints[i];
		constraint->buildJacobian();
	}

	//solve the regular non-contact constraints (point 2 point, hinge, generic d6)
	for (int g=0;g<solverInfo.m_numIterations;g++)
	{
		//
		// constraint solving
		//
		for (i=0;i< numConstraints ; i++ )
		{
			btTypedConstraint* constraint = m_constraints[i];
			constraint->solveConstraint( solverInfo.m_timeStep );
		}
	}

	END_PROFILE("solveNoncontactConstraints");

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

static void DrawAabb(btIDebugDraw* debugDrawer,const btVector3& from,const btVector3& to,const btVector3& color)
{

	btVector3 halfExtents = (to-from)* 0.5f;
	btVector3 center = (to+from) *0.5f;
	int i,j;

	btVector3 edgecoord(1.f,1.f,1.f),pa,pb;
	for (i=0;i<4;i++)
	{
		for (j=0;j<3;j++)
		{
			pa = btVector3(edgecoord[0]*halfExtents[0], edgecoord[1]*halfExtents[1],		
				edgecoord[2]*halfExtents[2]);
			pa+=center;

			int othercoord = j%3;
			edgecoord[othercoord]*=-1.f;
			pb = btVector3(edgecoord[0]*halfExtents[0], edgecoord[1]*halfExtents[1],	
				edgecoord[2]*halfExtents[2]);
			pb+=center;

			debugDrawer->drawLine(pa,pb,color);
		}
		edgecoord = btVector3(-1.f,-1.f,-1.f);
		if (i<3)
			edgecoord[i]*=-1.f;
	}


}

void	btDiscreteDynamicsWorld::updateAabbs()
{
	BEGIN_PROFILE("updateAabbs");
	
	btVector3 colorvec(1,0,0);
	btTransform predictedTrans;
	for (unsigned int i=0;i<m_collisionObjects.size();i++)
	{
		btCollisionObject* colObj = m_collisionObjects[i];
		
		btRigidBody* body = btRigidBody::upcast(colObj);
		if (body)
		{
		//	if (body->IsActive() && (!body->IsStatic()))
			{
				btPoint3 minAabb,maxAabb;
				colObj->getCollisionShape()->getAabb(colObj->getWorldTransform(), minAabb,maxAabb);
				btSimpleBroadphase* bp = (btSimpleBroadphase*)m_broadphasePairCache;

				//moving objects should be moderately sized, probably something wrong if not
				if ( colObj->isStaticObject() || ((maxAabb-minAabb).length2() < 1e12f))
				{
					bp->setAabb(body->getBroadphaseHandle(),minAabb,maxAabb);
				} else
				{
					//something went wrong, investigate
					//this assert is unwanted in 3D modelers (danger of loosing work)
					assert(0);
					body->setActivationState(DISABLE_SIMULATION);
					
					static bool reportMe = true;
					if (reportMe)
					{
						reportMe = false;
						printf("Overflow in AABB, object removed from simulation \n");
						printf("If you can reproduce this, please email bugs@continuousphysics.com\n");
						printf("Please include above information, your Platform, version of OS.\n");
						printf("Thanks.\n");
					}


				}
				if (m_debugDrawer && (m_debugDrawer->getDebugMode() & btIDebugDraw::DBG_DrawAabb))
				{
					DrawAabb(m_debugDrawer,minAabb,maxAabb,colorvec);
				}
			}
		}
	}
	
	END_PROFILE("updateAabbs");
}

void	btDiscreteDynamicsWorld::integrateTransforms(float timeStep)
{
	BEGIN_PROFILE("integrateTransforms");
	btTransform predictedTrans;
	for (unsigned int i=0;i<m_collisionObjects.size();i++)
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



void	btDiscreteDynamicsWorld::predictUnconstraintMotion(float timeStep)
{
	BEGIN_PROFILE("predictUnconstraintMotion");
	for (unsigned int i=0;i<m_collisionObjects.size();i++)
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


void	btDiscreteDynamicsWorld::startProfiling(float timeStep)
{
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

	DebugDrawcallback(btIDebugDraw*	debugDrawer,const btTransform& worldTrans,const btVector3& color)
		: m_debugDrawer(debugDrawer),
		m_worldTrans(worldTrans),
		m_color(color)
	{
	}

	virtual void internalProcessTriangleIndex(btVector3* triangle,int partId,int  triangleIndex)
	{
		processTriangle(triangle,partId,triangleIndex);
	}

	virtual void processTriangle(btVector3* triangle,int partId, int triangleIndex)
	{
		btVector3 wv0,wv1,wv2;
		wv0 = m_worldTrans*triangle[0];
		wv1 = m_worldTrans*triangle[1];
		wv2 = m_worldTrans*triangle[2];
		m_debugDrawer->drawLine(wv0,wv1,m_color);
		m_debugDrawer->drawLine(wv1,wv2,m_color);
		m_debugDrawer->drawLine(wv2,wv0,m_color);
	}
};



void btDiscreteDynamicsWorld::debugDrawObject(const btTransform& worldTransform, const btCollisionShape* shape, const btVector3& color)
{

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
				float radius = sphereShape->getMargin();//radius doesn't include the margin, so draw with margin
				btVector3 start = worldTransform.getOrigin();
				getDebugDrawer()->drawLine(start,start+worldTransform.getBasis() * btVector3(radius,0,0),color);
				getDebugDrawer()->drawLine(start,start+worldTransform.getBasis() * btVector3(0,radius,0),color);
				getDebugDrawer()->drawLine(start,start+worldTransform.getBasis() * btVector3(0,0,radius),color);
				//drawSphere					
				break;
			}
		case MULTI_SPHERE_SHAPE_PROXYTYPE:
		case CONE_SHAPE_PROXYTYPE:
			{
				const btConeShape* coneShape = static_cast<const btConeShape*>(shape);
				float radius = coneShape->getRadius();//+coneShape->getMargin();
				float height = coneShape->getHeight();//+coneShape->getMargin();
				btVector3 start = worldTransform.getOrigin();
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * btVector3(0.f,0.f,0.5f*height),start+worldTransform.getBasis() * btVector3(radius,0.f,-0.5f*height),color);
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * btVector3(0.f,0.f,0.5f*height),start+worldTransform.getBasis() * btVector3(-radius,0.f,-0.5f*height),color);
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * btVector3(0.f,0.f,0.5f*height),start+worldTransform.getBasis() * btVector3(0.f,radius,-0.5f*height),color);
				getDebugDrawer()->drawLine(start+worldTransform.getBasis() * btVector3(0.f,0.f,0.5f*height),start+worldTransform.getBasis() * btVector3(0.f,-radius,-0.5f*height),color);
				break;

			}
		case CYLINDER_SHAPE_PROXYTYPE:
			{
				const btCylinderShape* cylinder = static_cast<const btCylinderShape*>(shape);
				int upAxis = cylinder->getUpAxis();
				float radius = cylinder->getRadius();
				float halfHeight = cylinder->getHalfExtents()[upAxis];
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

				if (shape->getShapeType() == TRIANGLE_MESH_SHAPE_PROXYTYPE)
				{
					btTriangleMeshShape* concaveMesh = (btTriangleMeshShape*) shape;
					//btVector3 aabbMax(1e30f,1e30f,1e30f);
					//btVector3 aabbMax(100,100,100);//1e30f,1e30f,1e30f);

					//todo pass camera, for some culling
					btVector3 aabbMax(1e30f,1e30f,1e30f);
					btVector3 aabbMin(-1e30f,-1e30f,-1e30f);

					DebugDrawcallback drawCallback(getDebugDrawer(),worldTransform,color);
					concaveMesh->processAllTriangles(&drawCallback,aabbMin,aabbMax);

				}

				if (shape->getShapeType() == CONVEX_TRIANGLEMESH_SHAPE_PROXYTYPE)
				{
					btConvexTriangleMeshShape* convexMesh = (btConvexTriangleMeshShape*) shape;
					//todo: pass camera for some culling			
					btVector3 aabbMax(1e30f,1e30f,1e30f);
					btVector3 aabbMin(-1e30f,-1e30f,-1e30f);
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
