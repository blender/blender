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




#include "CcdPhysicsEnvironment.h"
#include "CcdPhysicsController.h"

#include <algorithm>
#include "btBulletDynamicsCommon.h"
#include "LinearMath/btIDebugDraw.h"
#include "BulletCollision/CollisionDispatch/btSimulationIslandManager.h"

//profiling/timings
#include "LinearMath/btQuickprof.h"


#include "PHY_IMotionState.h"


bool useIslands = true;

#ifdef NEW_BULLET_VEHICLE_SUPPORT
#include "BulletDynamics/Vehicle/btRaycastVehicle.h"
#include "BulletDynamics/Vehicle/btVehicleRaycaster.h"
#include "BulletDynamics/Vehicle/btWheelInfo.h"
#include "PHY_IVehicle.h"
btRaycastVehicle::btVehicleTuning	gTuning;

#endif //NEW_BULLET_VEHICLE_SUPPORT
#include "LinearMath/btAabbUtil2.h"


#ifdef WIN32
void DrawRasterizerLine(const float* from,const float* to,int color);
#endif


#include "BulletDynamics/ConstraintSolver/btContactConstraint.h"


#include <stdio.h>
#include <string.h>		// for memset

#ifdef NEW_BULLET_VEHICLE_SUPPORT
class WrapperVehicle : public PHY_IVehicle
{

	btRaycastVehicle*	m_vehicle;
	PHY_IPhysicsController*	m_chassis;

public:

	WrapperVehicle(btRaycastVehicle* vehicle,PHY_IPhysicsController* chassis)
		:m_vehicle(vehicle),
		m_chassis(chassis)
	{
	}

	btRaycastVehicle*	GetVehicle()
	{
		return m_vehicle;
	}

	PHY_IPhysicsController*	GetChassis()
	{
		return m_chassis;
	}

	virtual void	AddWheel(
		PHY_IMotionState*	motionState,
		PHY__Vector3	connectionPoint,
		PHY__Vector3	downDirection,
		PHY__Vector3	axleDirection,
		float	suspensionRestLength,
		float wheelRadius,
		bool hasSteering
		)
	{
		btVector3 connectionPointCS0(connectionPoint[0],connectionPoint[1],connectionPoint[2]);
		btVector3 wheelDirectionCS0(downDirection[0],downDirection[1],downDirection[2]);
		btVector3 wheelAxle(axleDirection[0],axleDirection[1],axleDirection[2]);


		btWheelInfo& info = m_vehicle->addWheel(connectionPointCS0,wheelDirectionCS0,wheelAxle,
			suspensionRestLength,wheelRadius,gTuning,hasSteering);
		info.m_clientInfo = motionState;

	}

	void	SyncWheels()
	{
		int numWheels = GetNumWheels();
		int i;
		for (i=0;i<numWheels;i++)
		{
			btWheelInfo& info = m_vehicle->getWheelInfo(i);
			PHY_IMotionState* motionState = (PHY_IMotionState*)info.m_clientInfo ;
	//		m_vehicle->updateWheelTransformsWS(info,false);
			m_vehicle->updateWheelTransform(i,false);
			btTransform trans = m_vehicle->getWheelInfo(i).m_worldTransform;
			btQuaternion orn = trans.getRotation();
			const btVector3& pos = trans.getOrigin();
			motionState->setWorldOrientation(orn.x(),orn.y(),orn.z(),orn[3]);
			motionState->setWorldPosition(pos.x(),pos.y(),pos.z());

		}
	}

	virtual	int		GetNumWheels() const
	{
		return m_vehicle->getNumWheels();
	}

	virtual void	GetWheelPosition(int wheelIndex,float& posX,float& posY,float& posZ) const
	{
		btTransform	trans = m_vehicle->getWheelTransformWS(wheelIndex);
		posX = trans.getOrigin().x();
		posY = trans.getOrigin().y();
		posZ = trans.getOrigin().z();
	}
	virtual void	GetWheelOrientationQuaternion(int wheelIndex,float& quatX,float& quatY,float& quatZ,float& quatW) const
	{
		btTransform	trans = m_vehicle->getWheelTransformWS(wheelIndex);
		btQuaternion quat = trans.getRotation();
		btMatrix3x3 orn2(quat);

		quatX = trans.getRotation().x();
		quatY = trans.getRotation().y();
		quatZ = trans.getRotation().z();
		quatW = trans.getRotation()[3];


		//printf("test");


	}

	virtual float	GetWheelRotation(int wheelIndex) const
	{
		float rotation = 0.f;

		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->getNumWheels()))
		{
			btWheelInfo& info = m_vehicle->getWheelInfo(wheelIndex);
			rotation = info.m_rotation;
		}
		return rotation;

	}



	virtual int	GetUserConstraintId() const
	{
		return m_vehicle->getUserConstraintId();
	}

	virtual int	GetUserConstraintType() const
	{
		return m_vehicle->getUserConstraintType();
	}

	virtual	void	SetSteeringValue(float steering,int wheelIndex)
	{
		m_vehicle->setSteeringValue(steering,wheelIndex);
	}

	virtual	void	ApplyEngineForce(float force,int wheelIndex)
	{
		m_vehicle->applyEngineForce(force,wheelIndex);
	}

	virtual	void	ApplyBraking(float braking,int wheelIndex)
	{
		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->getNumWheels()))
		{
			btWheelInfo& info = m_vehicle->getWheelInfo(wheelIndex);
			info.m_brake = braking;
		}
	}

	virtual	void	SetWheelFriction(float friction,int wheelIndex)
	{
		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->getNumWheels()))
		{
			btWheelInfo& info = m_vehicle->getWheelInfo(wheelIndex);
			info.m_frictionSlip = friction;
		}

	}

	virtual	void	SetSuspensionStiffness(float suspensionStiffness,int wheelIndex)
	{
		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->getNumWheels()))
		{
			btWheelInfo& info = m_vehicle->getWheelInfo(wheelIndex);
			info.m_suspensionStiffness = suspensionStiffness;

		}
	}

	virtual	void	SetSuspensionDamping(float suspensionDamping,int wheelIndex)
	{
		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->getNumWheels()))
		{
			btWheelInfo& info = m_vehicle->getWheelInfo(wheelIndex);
			info.m_wheelsDampingRelaxation = suspensionDamping;
		}
	}

	virtual	void	SetSuspensionCompression(float suspensionCompression,int wheelIndex)
	{
		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->getNumWheels()))
		{
			btWheelInfo& info = m_vehicle->getWheelInfo(wheelIndex);
			info.m_wheelsDampingCompression = suspensionCompression;
		}
	}



	virtual	void	SetRollInfluence(float rollInfluence,int wheelIndex)
	{
		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->getNumWheels()))
		{
			btWheelInfo& info = m_vehicle->getWheelInfo(wheelIndex);
			info.m_rollInfluence = rollInfluence;
		}
	}

	virtual void	SetCoordinateSystem(int rightIndex,int upIndex,int forwardIndex)
	{
		m_vehicle->setCoordinateSystem(rightIndex,upIndex,forwardIndex);
	}



};
#endif //NEW_BULLET_VEHICLE_SUPPORT

class CcdOverlapFilterCallBack : public btOverlapFilterCallback
{
private:
	class CcdPhysicsEnvironment* m_physEnv;
public:
	CcdOverlapFilterCallBack(CcdPhysicsEnvironment* env) : 
		m_physEnv(env)
	{
	}
	virtual ~CcdOverlapFilterCallBack()
	{
	}
	// return true when pairs need collision
	virtual bool	needBroadphaseCollision(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1) const;
};


void CcdPhysicsEnvironment::setDebugDrawer(btIDebugDraw* debugDrawer)
{
	if (debugDrawer && m_dynamicsWorld)
		m_dynamicsWorld->setDebugDrawer(debugDrawer);
	m_debugDrawer = debugDrawer;
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






CcdPhysicsEnvironment::CcdPhysicsEnvironment(btDispatcher* dispatcher,btOverlappingPairCache* pairCache)
:m_scalingPropagated(false),
m_numIterations(10),
m_numTimeSubSteps(1),
m_ccdMode(0),
m_solverType(-1),
m_profileTimings(0),
m_enableSatCollisionDetection(false),
m_solver(NULL),
m_ownPairCache(NULL),
m_ownDispatcher(NULL),
m_filterCallback(NULL)
{

	for (int i=0;i<PHY_NUM_RESPONSE;i++)
	{
		m_triggerCallbacks[i] = 0;
	}
	if (!dispatcher)
	{
		dispatcher = new btCollisionDispatcher();
		m_ownDispatcher = dispatcher;
	}

	if(!pairCache)
	{

		//todo: calculate/let user specify this world sizes
		btVector3 worldMin(-10000,-10000,-10000);
		btVector3 worldMax(10000,10000,10000);

		pairCache = new btAxisSweep3(worldMin,worldMax);
		// remember that this was allocated by us so that we can release it
		m_ownPairCache = pairCache;
		//broadphase = new btSimpleBroadphase();
	}

	m_filterCallback = new CcdOverlapFilterCallBack(this);
	pairCache->setOverlapFilterCallback(m_filterCallback);

	setSolverType(1);//issues with quickstep and memory allocations
	m_dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher,pairCache,m_solver);
	m_debugDrawer = 0;
	m_gravity = btVector3(0.f,-10.f,0.f);
	m_dynamicsWorld->setGravity(m_gravity);


}

void	CcdPhysicsEnvironment::addCcdPhysicsController(CcdPhysicsController* ctrl)
{
	btRigidBody* body = ctrl->GetRigidBody();

	//this m_userPointer is just used for triggers, see CallbackTriggers
	body->setUserPointer(ctrl);

	body->setGravity( m_gravity );
	m_controllers.insert(ctrl);

	//use explicit group/filter for finer control over collision in bullet => near/radar sensor
	m_dynamicsWorld->addRigidBody(body, ctrl->GetCollisionFilterGroup(), ctrl->GetCollisionFilterMask());
	if (body->isStaticOrKinematicObject())
	{
		body->setActivationState(ISLAND_SLEEPING);
	}


	//CollisionObject(body,ctrl->GetCollisionFilterGroup(),ctrl->GetCollisionFilterMask());

	assert(body->getBroadphaseHandle());

	btBroadphaseInterface* scene =  getBroadphase();


	btCollisionShape* shapeinterface = ctrl->GetCollisionShape();

	assert(shapeinterface);

	const btTransform& t = ctrl->GetRigidBody()->getCenterOfMassTransform();
	

	btPoint3 minAabb,maxAabb;

	shapeinterface->getAabb(t,minAabb,maxAabb);

	float timeStep = 0.02f;


	//extent it with the motion

	btVector3 linMotion = body->getLinearVelocity()*timeStep;

	float maxAabbx = maxAabb.getX();
	float maxAabby = maxAabb.getY();
	float maxAabbz = maxAabb.getZ();
	float minAabbx = minAabb.getX();
	float minAabby = minAabb.getY();
	float minAabbz = minAabb.getZ();

	if (linMotion.x() > 0.f)
		maxAabbx += linMotion.x(); 
	else
		minAabbx += linMotion.x();
	if (linMotion.y() > 0.f)
		maxAabby += linMotion.y(); 
	else
		minAabby += linMotion.y();
	if (linMotion.z() > 0.f)
		maxAabbz += linMotion.z(); 
	else
		minAabbz += linMotion.z();


	minAabb = btVector3(minAabbx,minAabby,minAabbz);
	maxAabb = btVector3(maxAabbx,maxAabby,maxAabbz);




}

void	CcdPhysicsEnvironment::removeCcdPhysicsController(CcdPhysicsController* ctrl)
{
	//also remove constraint

	m_dynamicsWorld->removeRigidBody(ctrl->GetRigidBody());
	m_controllers.erase(ctrl);

	if (ctrl->m_registerCount != 0)
		printf("Warning: removing controller with non-zero m_registerCount: %d\n", ctrl->m_registerCount);

	//remove it from the triggers
	m_triggerControllers.erase(ctrl);
}

void	CcdPhysicsEnvironment::updateCcdPhysicsController(CcdPhysicsController* ctrl, btScalar newMass, int newCollisionFlags, short int newCollisionGroup, short int newCollisionMask)
{
	// this function is used when the collisionning group of a controller is changed
	// remove and add the collistioning object
	btRigidBody* body = ctrl->GetRigidBody();
	btVector3 inertia;

	m_dynamicsWorld->removeCollisionObject(body);
	body->setCollisionFlags(newCollisionFlags);
	body->getCollisionShape()->calculateLocalInertia(newMass, inertia);
	body->setMassProps(newMass, inertia);
	m_dynamicsWorld->addCollisionObject(body, newCollisionGroup, newCollisionMask);
	// to avoid nasty interaction, we must update the property of the controller as well
	ctrl->m_cci.m_mass = newMass;
	ctrl->m_cci.m_collisionFilterGroup = newCollisionGroup;
	ctrl->m_cci.m_collisionFilterMask = newCollisionMask;
	ctrl->m_cci.m_collisionFlags = newCollisionFlags;
}

void CcdPhysicsEnvironment::enableCcdPhysicsController(CcdPhysicsController* ctrl)
{
	if (m_controllers.insert(ctrl).second)
	{
		btRigidBody* body = ctrl->GetRigidBody();
		body->setUserPointer(ctrl);
		m_dynamicsWorld->addCollisionObject(body, 
			ctrl->GetCollisionFilterGroup(), ctrl->GetCollisionFilterMask());
	}
}

void CcdPhysicsEnvironment::disableCcdPhysicsController(CcdPhysicsController* ctrl)
{
	if (m_controllers.erase(ctrl))
	{
		m_dynamicsWorld->removeRigidBody(ctrl->GetRigidBody());
	}
}


void	CcdPhysicsEnvironment::beginFrame()
{

}


bool	CcdPhysicsEnvironment::proceedDeltaTime(double curTime,float timeStep)
{
	std::set<CcdPhysicsController*>::iterator it;
	int i;

	for (it=m_controllers.begin(); it!=m_controllers.end(); it++)
	{
		(*it)->SynchronizeMotionStates(timeStep);
	}

	float subStep = timeStep / float(m_numTimeSubSteps);
	for (i=0;i<m_numTimeSubSteps;i++)
	{
		m_dynamicsWorld->stepSimulation(subStep,0);//perform always a full simulation step
	}

	for (it=m_controllers.begin(); it!=m_controllers.end(); it++)
	{
		(*it)->SynchronizeMotionStates(timeStep);
	}

	for (i=0;i<m_wrapperVehicles.size();i++)
	{
		WrapperVehicle* veh = m_wrapperVehicles[i];
		veh->SyncWheels();
	}

	CallbackTriggers();

	return true;
}


void		CcdPhysicsEnvironment::setDebugMode(int debugMode)
{
	if (m_debugDrawer){
		m_debugDrawer->setDebugMode(debugMode);
	}
}

void		CcdPhysicsEnvironment::setNumIterations(int numIter)
{
	m_numIterations = numIter;
}
void		CcdPhysicsEnvironment::setDeactivationTime(float dTime)
{
	gDeactivationTime = dTime;
}
void		CcdPhysicsEnvironment::setDeactivationLinearTreshold(float linTresh)
{
	gLinearSleepingTreshold = linTresh;
}
void		CcdPhysicsEnvironment::setDeactivationAngularTreshold(float angTresh) 
{
	gAngularSleepingTreshold = angTresh;
}

void		CcdPhysicsEnvironment::setContactBreakingTreshold(float contactBreakingTreshold)
{
	gContactBreakingThreshold = contactBreakingTreshold;

}


void		CcdPhysicsEnvironment::setCcdMode(int ccdMode)
{
	m_ccdMode = ccdMode;
}


void		CcdPhysicsEnvironment::setSolverSorConstant(float sor)
{
	m_solverInfo.m_sor = sor;
}

void		CcdPhysicsEnvironment::setSolverTau(float tau)
{
	m_solverInfo.m_tau = tau;
}
void		CcdPhysicsEnvironment::setSolverDamping(float damping)
{
	m_solverInfo.m_damping = damping;
}


void		CcdPhysicsEnvironment::setLinearAirDamping(float damping)
{
	gLinearAirDamping = damping;
}

void		CcdPhysicsEnvironment::setUseEpa(bool epa)
{
	//gUseEpa = epa;
}

void		CcdPhysicsEnvironment::setSolverType(int solverType)
{

	switch (solverType)
	{
	case 1:
		{
			if (m_solverType != solverType)
			{

				m_solver = new btSequentialImpulseConstraintSolver();
				((btSequentialImpulseConstraintSolver*)m_solver)->setSolverMode(btSequentialImpulseConstraintSolver::SOLVER_USE_WARMSTARTING | btSequentialImpulseConstraintSolver::SOLVER_RANDMIZE_ORDER);
				break;
			}
		}

	case 0:
	default:
		if (m_solverType != solverType)
		{
//			m_solver = new OdeConstraintSolver();

			break;
		}

	};

	m_solverType = solverType ;
}






void		CcdPhysicsEnvironment::setGravity(float x,float y,float z)
{
	m_gravity = btVector3(x,y,z);
	m_dynamicsWorld->setGravity(m_gravity);

}




static int gConstraintUid = 1;

//Following the COLLADA physics specification for constraints
int			CcdPhysicsEnvironment::createUniversalD6Constraint(
						class PHY_IPhysicsController* ctrlRef,class PHY_IPhysicsController* ctrlOther,
						btTransform& frameInA,
						btTransform& frameInB,
						const btVector3& linearMinLimits,
						const btVector3& linearMaxLimits,
						const btVector3& angularMinLimits,
						const btVector3& angularMaxLimits
)
{

	//we could either add some logic to recognize ball-socket and hinge, or let that up to the user
	//perhaps some warning or hint that hinge/ball-socket is more efficient?
	
	btGeneric6DofConstraint* genericConstraint = 0;
	CcdPhysicsController* ctrl0 = (CcdPhysicsController*) ctrlRef;
	CcdPhysicsController* ctrl1 = (CcdPhysicsController*) ctrlOther;
	
	btRigidBody* rb0 = ctrl0->GetRigidBody();
	btRigidBody* rb1 = ctrl1->GetRigidBody();

	if (rb1)
	{
		

		genericConstraint = new btGeneric6DofConstraint(
			*rb0,*rb1,
			frameInA,frameInB);
		genericConstraint->setLinearLowerLimit(linearMinLimits);
		genericConstraint->setLinearUpperLimit(linearMaxLimits);
		genericConstraint->setAngularLowerLimit(angularMinLimits);
		genericConstraint->setAngularUpperLimit(angularMaxLimits);
	} else
	{
		// TODO: Implement single body case...
		//No, we can use a fixed rigidbody in above code, rather then unnecessary duplation of code

	}
	
	if (genericConstraint)
	{
	//	m_constraints.push_back(genericConstraint);
		m_dynamicsWorld->addConstraint(genericConstraint);

		genericConstraint->setUserConstraintId(gConstraintUid++);
		genericConstraint->setUserConstraintType(PHY_GENERIC_6DOF_CONSTRAINT);
		//64 bit systems can't cast pointer to int. could use size_t instead.
		return genericConstraint->getUserConstraintId();
	}
	return 0;
}



void		CcdPhysicsEnvironment::removeConstraint(int	constraintId)
{
	
	int i;
	int numConstraints = m_dynamicsWorld->getNumConstraints();
	for (i=0;i<numConstraints;i++)
	{
		btTypedConstraint* constraint = m_dynamicsWorld->getConstraint(i);
		if (constraint->getUserConstraintId() == constraintId)
		{
			constraint->getRigidBodyA().activate();
			constraint->getRigidBodyB().activate();
			m_dynamicsWorld->removeConstraint(constraint);
			break;
		}
	}
}


struct	FilterClosestRayResultCallback : public btCollisionWorld::ClosestRayResultCallback
{
	PHY_IRayCastFilterCallback&	m_phyRayFilter;
	const btCollisionShape*		m_hitTriangleShape;
	int							m_hitTriangleIndex;

	FilterClosestRayResultCallback (PHY_IRayCastFilterCallback& phyRayFilter,const btVector3& rayFrom,const btVector3& rayTo)
		: btCollisionWorld::ClosestRayResultCallback(rayFrom,rayTo),
		m_phyRayFilter(phyRayFilter),
		m_hitTriangleShape(NULL),
		m_hitTriangleIndex(0)
	{
	}

	virtual ~FilterClosestRayResultCallback()
	{
	}

	virtual bool    NeedRayCast(btCollisionObject* object)
	{
		CcdPhysicsController* phyCtrl = static_cast<CcdPhysicsController*>(object->getUserPointer());
		if (phyCtrl != m_phyRayFilter.m_ignoreController)
		{
			return m_phyRayFilter.needBroadphaseRayCast(phyCtrl);
		}
		return false;
	}

	virtual	float	AddSingleResult( btCollisionWorld::LocalRayResult& rayResult)
	{
		CcdPhysicsController* curHit = static_cast<CcdPhysicsController*>(rayResult.m_collisionObject->getUserPointer());
		// save shape information as ClosestRayResultCallback::AddSingleResult() does not do it
		if (rayResult.m_localShapeInfo)
		{
			m_hitTriangleShape = rayResult.m_localShapeInfo->m_triangleShape;
			m_hitTriangleIndex = rayResult.m_localShapeInfo->m_triangleIndex;
		} else 
		{
			m_hitTriangleShape = NULL;
			m_hitTriangleIndex = 0;
		}
		return ClosestRayResultCallback::AddSingleResult(rayResult);
	}

};

PHY_IPhysicsController* CcdPhysicsEnvironment::rayTest(PHY_IRayCastFilterCallback &filterCallback, float fromX,float fromY,float fromZ, float toX,float toY,float toZ)
{


	float minFraction = 1.f;

	btVector3 rayFrom(fromX,fromY,fromZ);
	btVector3 rayTo(toX,toY,toZ);

	btVector3	hitPointWorld,normalWorld;

	//Either Ray Cast with or without filtering

	//btCollisionWorld::ClosestRayResultCallback rayCallback(rayFrom,rayTo);
	FilterClosestRayResultCallback	 rayCallback(filterCallback,rayFrom,rayTo);


	PHY_RayCastResult result;
	memset(&result, 0, sizeof(result));

	// don't collision with sensor object
	m_dynamicsWorld->rayTest(rayFrom,rayTo,rayCallback, CcdConstructionInfo::AllFilter ^ CcdConstructionInfo::SensorFilter,filterCallback.m_faceNormal);
	if (rayCallback.HasHit())
	{
		CcdPhysicsController* controller = static_cast<CcdPhysicsController*>(rayCallback.m_collisionObject->getUserPointer());
		result.m_controller = controller;
		result.m_hitPoint[0] = rayCallback.m_hitPointWorld.getX();
		result.m_hitPoint[1] = rayCallback.m_hitPointWorld.getY();
		result.m_hitPoint[2] = rayCallback.m_hitPointWorld.getZ();

		if (rayCallback.m_hitNormalWorld.length2() > (SIMD_EPSILON*SIMD_EPSILON))
		{
			rayCallback.m_hitNormalWorld.normalize();
		} else
		{
			rayCallback.m_hitNormalWorld.setValue(1,0,0);
		}
		result.m_hitNormal[0] = rayCallback.m_hitNormalWorld.getX();
		result.m_hitNormal[1] = rayCallback.m_hitNormalWorld.getY();
		result.m_hitNormal[2] = rayCallback.m_hitNormalWorld.getZ();
		if (rayCallback.m_hitTriangleShape != NULL)
		{
			// identify the mesh polygon
			CcdShapeConstructionInfo* shapeInfo = controller->m_shapeInfo;
			if (shapeInfo)
			{
				btCollisionShape* shape = controller->GetRigidBody()->getCollisionShape();
				if (shape->isCompound())
				{
					btCompoundShape* compoundShape = (btCompoundShape*)shape;
					CcdShapeConstructionInfo* compoundShapeInfo = shapeInfo;
					// need to search which sub-shape has been hit
					for (int i=0; i<compoundShape->getNumChildShapes(); i++)
					{
						shapeInfo = compoundShapeInfo->GetChildShape(i);
						shape=compoundShape->getChildShape(i);
						if (shape == rayCallback.m_hitTriangleShape)
							break;
					}
				}
				if (shape == rayCallback.m_hitTriangleShape && 
					rayCallback.m_hitTriangleIndex < shapeInfo->m_polygonIndexArray.size())
				{
					result.m_meshObject = shapeInfo->m_meshObject;
					result.m_polygon = shapeInfo->m_polygonIndexArray.at(rayCallback.m_hitTriangleIndex);
				}
			}
		}
		filterCallback.reportHit(&result);
	}	


	return result.m_controller;
}



int	CcdPhysicsEnvironment::getNumContactPoints()
{
	return 0;
}

void CcdPhysicsEnvironment::getContactPoint(int i,float& hitX,float& hitY,float& hitZ,float& normalX,float& normalY,float& normalZ)
{

}




btBroadphaseInterface*	CcdPhysicsEnvironment::getBroadphase()
{ 
	return m_dynamicsWorld->getBroadphase(); 
}




CcdPhysicsEnvironment::~CcdPhysicsEnvironment()
{

#ifdef NEW_BULLET_VEHICLE_SUPPORT
	m_wrapperVehicles.clear();
#endif //NEW_BULLET_VEHICLE_SUPPORT

	//m_broadphase->DestroyScene();
	//delete broadphase ? release reference on broadphase ?

	//first delete scene, then dispatcher, because pairs have to release manifolds on the dispatcher
	//delete m_dispatcher;
	delete m_dynamicsWorld;
	

	if (NULL != m_ownPairCache)
		delete m_ownPairCache;

	if (NULL != m_ownDispatcher)
		delete m_ownDispatcher;

	if (NULL != m_solver)
		delete m_solver;

	if (NULL != m_debugDrawer)
		delete m_debugDrawer;

	if (NULL != m_filterCallback)
		delete m_filterCallback;
}


void	CcdPhysicsEnvironment::setConstraintParam(int constraintId,int param,float value0,float value1)
{
	btTypedConstraint* typedConstraint = getConstraintById(constraintId);
	switch (typedConstraint->getUserConstraintType())
	{
	case PHY_GENERIC_6DOF_CONSTRAINT:
		{
			//param = 1..12, min0,max0,min1,max1...min6,max6
			btGeneric6DofConstraint* genCons = (btGeneric6DofConstraint*)typedConstraint;
			genCons->SetLimit(param,value0,value1);
			break;
		};
	default:
		{
		};
	};
}

btTypedConstraint*	CcdPhysicsEnvironment::getConstraintById(int constraintId)
{

	int numConstraints = m_dynamicsWorld->getNumConstraints();
	int i;
	for (i=0;i<numConstraints;i++)
	{
		btTypedConstraint* constraint = m_dynamicsWorld->getConstraint(i);
		if (constraint->getUserConstraintId()==constraintId)
		{
			return constraint;
		}
	}
	return 0;
}


void CcdPhysicsEnvironment::addSensor(PHY_IPhysicsController* ctrl)
{

	CcdPhysicsController* ctrl1 = (CcdPhysicsController* )ctrl;
	// addSensor() is a "light" function for bullet because it is used
	// dynamically when the sensor is activated. Use enableCcdPhysicsController() instead 
	//if (m_controllers.insert(ctrl1).second)
	//{
	//	addCcdPhysicsController(ctrl1);
	//}
	enableCcdPhysicsController(ctrl1);

	//Collision filter/mask is now set at the time of the creation of the controller 
	//force collision detection with everything, including static objects (might hurt performance!)
	//ctrl1->GetRigidBody()->getBroadphaseHandle()->m_collisionFilterMask = btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::SensorTrigger;
	//ctrl1->GetRigidBody()->getBroadphaseHandle()->m_collisionFilterGroup = btBroadphaseProxy::SensorTrigger;
	//todo: make this 'sensor'!

	requestCollisionCallback(ctrl);
	//printf("addSensor\n");
}

void CcdPhysicsEnvironment::removeCollisionCallback(PHY_IPhysicsController* ctrl)
{
	CcdPhysicsController* ccdCtrl = (CcdPhysicsController*)ctrl;
	if (ccdCtrl->Unregister())
		m_triggerControllers.erase(ccdCtrl);
}


void CcdPhysicsEnvironment::removeSensor(PHY_IPhysicsController* ctrl)
{
	removeCollisionCallback(ctrl);

	disableCcdPhysicsController((CcdPhysicsController*)ctrl);
}

void CcdPhysicsEnvironment::addTouchCallback(int response_class, PHY_ResponseCallback callback, void *user)
{
	/*	printf("addTouchCallback\n(response class = %i)\n",response_class);

	//map PHY_ convention into SM_ convention
	switch (response_class)
	{
	case	PHY_FH_RESPONSE:
	printf("PHY_FH_RESPONSE\n");
	break;
	case PHY_SENSOR_RESPONSE:
	printf("PHY_SENSOR_RESPONSE\n");
	break;
	case PHY_CAMERA_RESPONSE:
	printf("PHY_CAMERA_RESPONSE\n");
	break;
	case PHY_OBJECT_RESPONSE:
	printf("PHY_OBJECT_RESPONSE\n");
	break;
	case PHY_STATIC_RESPONSE:
	printf("PHY_STATIC_RESPONSE\n");
	break;
	default:
	assert(0);
	return;
	}
	*/

	m_triggerCallbacks[response_class] = callback;
	m_triggerCallbacksUserPtrs[response_class] = user;

}
void CcdPhysicsEnvironment::requestCollisionCallback(PHY_IPhysicsController* ctrl)
{
	CcdPhysicsController* ccdCtrl = static_cast<CcdPhysicsController*>(ctrl);

	if (ccdCtrl->Register())
		m_triggerControllers.insert(ccdCtrl);
}

void	CcdPhysicsEnvironment::CallbackTriggers()
{
	
	CcdPhysicsController* ctrl0=0,*ctrl1=0;

	if (m_triggerCallbacks[PHY_OBJECT_RESPONSE] || (m_debugDrawer && (m_debugDrawer->getDebugMode() & btIDebugDraw::DBG_DrawContactPoints)))
	{
		//walk over all overlapping pairs, and if one of the involved bodies is registered for trigger callback, perform callback
		btDispatcher* dispatcher = m_dynamicsWorld->getDispatcher();
		int numManifolds = dispatcher->getNumManifolds();
		for (int i=0;i<numManifolds;i++)
		{
			btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
			int numContacts = manifold->getNumContacts();
			if (numContacts)
			{
				btRigidBody* rb0 = static_cast<btRigidBody*>(manifold->getBody0());
				btRigidBody* rb1 = static_cast<btRigidBody*>(manifold->getBody1());
				if (m_debugDrawer && (m_debugDrawer->getDebugMode() & btIDebugDraw::DBG_DrawContactPoints))
				{
					for (int j=0;j<numContacts;j++)
					{
						btVector3 color(1,0,0);
						const btManifoldPoint& cp = manifold->getContactPoint(j);
						if (m_debugDrawer)
							m_debugDrawer->drawContactPoint(cp.m_positionWorldOnB,cp.m_normalWorldOnB,cp.getDistance(),cp.getLifeTime(),color);
					}
				}
				btRigidBody* obj0 = rb0;
				btRigidBody* obj1 = rb1;

				//m_internalOwner is set in 'addPhysicsController'
				CcdPhysicsController* ctrl0 = static_cast<CcdPhysicsController*>(obj0->getUserPointer());
				CcdPhysicsController* ctrl1 = static_cast<CcdPhysicsController*>(obj1->getUserPointer());

				std::set<CcdPhysicsController*>::const_iterator i = m_triggerControllers.find(ctrl0);
				if (i == m_triggerControllers.end())
				{
					i = m_triggerControllers.find(ctrl1);
				}

				if (!(i == m_triggerControllers.end()))
				{
					m_triggerCallbacks[PHY_OBJECT_RESPONSE](m_triggerCallbacksUserPtrs[PHY_OBJECT_RESPONSE],
						ctrl0,ctrl1,0);
				}
				// Bullet does not refresh the manifold contact point for object without contact response
				// may need to remove this when a newer Bullet version is integrated
				if (!dispatcher->needsResponse(rb0, rb1))
				{
					// Refresh algorithm fails sometimes when there is penetration 
					// (usuall the case with ghost and sensor objects)
					// Let's just clear the manifold, in any case, it is recomputed on each frame.
					manifold->clearManifold(); //refreshContactPoints(rb0->getCenterOfMassTransform(),rb1->getCenterOfMassTransform());
				}
			}
		}



	}


}

// This call back is called before a pair is added in the cache
// Handy to remove objects that must be ignored by sensors
bool CcdOverlapFilterCallBack::needBroadphaseCollision(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1) const
{
	btCollisionObject *colObj0, *colObj1;
	CcdPhysicsController *sensorCtrl, *objCtrl;
	bool collides;
	// first check the filters
	collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
	collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);
	if (!collides)
		return false;

	// additional check for sensor object
	if (proxy0->m_collisionFilterGroup & btBroadphaseProxy::SensorTrigger)
	{
		// this is a sensor object, the other one can't be a sensor object because 
		// they exclude each other in the above test
		assert(!(proxy1->m_collisionFilterGroup & btBroadphaseProxy::SensorTrigger));
		colObj0 = (btCollisionObject*)proxy0->m_clientObject;
		colObj1 = (btCollisionObject*)proxy1->m_clientObject;
	}
	else if (proxy1->m_collisionFilterGroup & btBroadphaseProxy::SensorTrigger)
	{
		colObj0 = (btCollisionObject*)proxy1->m_clientObject;
		colObj1 = (btCollisionObject*)proxy0->m_clientObject;
	}
	else
	{
		return true;
	}
	if (!colObj0 || !colObj1)
		return false;
	sensorCtrl = static_cast<CcdPhysicsController*>(colObj0->getUserPointer());
	objCtrl = static_cast<CcdPhysicsController*>(colObj1->getUserPointer());
	if (m_physEnv->m_triggerCallbacks[PHY_BROADPH_RESPONSE])
	{
		return m_physEnv->m_triggerCallbacks[PHY_BROADPH_RESPONSE](m_physEnv->m_triggerCallbacksUserPtrs[PHY_BROADPH_RESPONSE], sensorCtrl, objCtrl, 0);
	}
	return true;
}


#ifdef NEW_BULLET_VEHICLE_SUPPORT

//complex constraint for vehicles
PHY_IVehicle*	CcdPhysicsEnvironment::getVehicleConstraint(int constraintId)
{
	int i;

	int numVehicles = m_wrapperVehicles.size();
	for (i=0;i<numVehicles;i++)
	{
		WrapperVehicle* wrapperVehicle = m_wrapperVehicles[i];
		if (wrapperVehicle->GetVehicle()->getUserConstraintId() == constraintId)
			return wrapperVehicle;
	}

	return 0;
}

#endif //NEW_BULLET_VEHICLE_SUPPORT


int currentController = 0;
int numController = 0;




PHY_IPhysicsController*	CcdPhysicsEnvironment::CreateSphereController(float radius,const PHY__Vector3& position)
{
	
	CcdConstructionInfo	cinfo;
	// memory leak! The shape is not deleted by Bullet and we cannot add it to the KX_Scene.m_shapes list
	cinfo.m_collisionShape = new btSphereShape(radius);
	cinfo.m_MotionState = 0;
	cinfo.m_physicsEnv = this;
	// declare this object as Dyamic rather then static!!
	// The reason as it is designed to detect all type of object, including static object
	// It would cause static-static message to be printed on the console otherwise
	cinfo.m_collisionFlags |= btCollisionObject::CF_NO_CONTACT_RESPONSE/* | btCollisionObject::CF_KINEMATIC_OBJECT*/;
	DefaultMotionState* motionState = new DefaultMotionState();
	cinfo.m_MotionState = motionState;
	// we will add later the possibility to select the filter from option
	cinfo.m_collisionFilterMask = CcdConstructionInfo::AllFilter ^ CcdConstructionInfo::SensorFilter;
	cinfo.m_collisionFilterGroup = CcdConstructionInfo::SensorFilter;
	motionState->m_worldTransform.setIdentity();
	motionState->m_worldTransform.setOrigin(btVector3(position[0],position[1],position[2]));

	CcdPhysicsController* sphereController = new CcdPhysicsController(cinfo);
	
	return sphereController;
}

int			CcdPhysicsEnvironment::createConstraint(class PHY_IPhysicsController* ctrl0,class PHY_IPhysicsController* ctrl1,PHY_ConstraintType type,
													float pivotX,float pivotY,float pivotZ,
													float axisX,float axisY,float axisZ,
													float axis1X,float axis1Y,float axis1Z,
													float axis2X,float axis2Y,float axis2Z
													)
{


	CcdPhysicsController* c0 = (CcdPhysicsController*)ctrl0;
	CcdPhysicsController* c1 = (CcdPhysicsController*)ctrl1;

	btRigidBody* rb0 = c0 ? c0->GetRigidBody() : 0;
	btRigidBody* rb1 = c1 ? c1->GetRigidBody() : 0;

	bool rb0static = rb0 ? rb0->isStaticOrKinematicObject() : true;
	bool rb1static = rb1 ? rb1->isStaticOrKinematicObject() : true;
	

	if (rb0static && rb1static)
		return 0;

	btVector3 pivotInA(pivotX,pivotY,pivotZ);
	btVector3 pivotInB = rb1 ? rb1->getCenterOfMassTransform().inverse()(rb0->getCenterOfMassTransform()(pivotInA)) : 
		rb0->getCenterOfMassTransform() * pivotInA;
	btVector3 axisInA(axisX,axisY,axisZ);


	bool angularOnly = false;

	switch (type)
	{
	case PHY_POINT2POINT_CONSTRAINT:
		{

			btPoint2PointConstraint* p2p = 0;

			if (rb1)
			{
				p2p = new btPoint2PointConstraint(*rb0,
					*rb1,pivotInA,pivotInB);
			} else
			{
				p2p = new btPoint2PointConstraint(*rb0,
					pivotInA);
			}

			m_dynamicsWorld->addConstraint(p2p);
//			m_constraints.push_back(p2p);

			p2p->setUserConstraintId(gConstraintUid++);
			p2p->setUserConstraintType(type);
			//64 bit systems can't cast pointer to int. could use size_t instead.
			return p2p->getUserConstraintId();

			break;
		}

	case PHY_GENERIC_6DOF_CONSTRAINT:
		{
			btGeneric6DofConstraint* genericConstraint = 0;

			if (rb1)
			{
				btTransform frameInA;
				btTransform frameInB;
				
				btVector3 axis1(axis1X,axis1Y,axis1Z), axis2(axis2X,axis2Y,axis2Z);
				if (axis1.length() == 0.0)
				{
					btPlaneSpace1( axisInA, axis1, axis2 );
				}
				
				frameInA.getBasis().setValue( axisInA.x(), axis1.x(), axis2.x(),
					                          axisInA.y(), axis1.y(), axis2.y(),
											  axisInA.z(), axis1.z(), axis2.z() );
				frameInA.setOrigin( pivotInA );

				btTransform inv = rb1->getCenterOfMassTransform().inverse();

				btTransform globalFrameA = rb0->getCenterOfMassTransform() * frameInA;
				
				frameInB = inv  * globalFrameA;
				
				genericConstraint = new btGeneric6DofConstraint(
					*rb0,*rb1,
					frameInA,frameInB);


			} else
			{
				static btRigidBody s_fixedObject2( 0,0,0);
				btTransform frameInA;
				btTransform frameInB;
				
				btVector3 axis1, axis2;
				btPlaneSpace1( axisInA, axis1, axis2 );

				frameInA.getBasis().setValue( axisInA.x(), axis1.x(), axis2.x(),
					                          axisInA.y(), axis1.y(), axis2.y(),
											  axisInA.z(), axis1.z(), axis2.z() );

				frameInA.setOrigin( pivotInA );

				///frameInB in worldspace
				frameInB = rb0->getCenterOfMassTransform() * frameInA;

				genericConstraint = new btGeneric6DofConstraint(
					*rb0,s_fixedObject2,
					frameInA,frameInB);
			}
			
			if (genericConstraint)
			{
				//m_constraints.push_back(genericConstraint);
				m_dynamicsWorld->addConstraint(genericConstraint);
				genericConstraint->setUserConstraintId(gConstraintUid++);
				genericConstraint->setUserConstraintType(type);
				//64 bit systems can't cast pointer to int. could use size_t instead.
				return genericConstraint->getUserConstraintId();
			} 

			break;
		}
	case PHY_CONE_TWIST_CONSTRAINT:
		{
			btConeTwistConstraint* coneTwistContraint = 0;

			
			if (rb1)
			{
				btTransform frameInA;
				btTransform frameInB;
				
				btVector3 axis1(axis1X,axis1Y,axis1Z), axis2(axis2X,axis2Y,axis2Z);
				if (axis1.length() == 0.0)
				{
					btPlaneSpace1( axisInA, axis1, axis2 );
				}
				
				frameInA.getBasis().setValue( axisInA.x(), axis1.x(), axis2.x(),
					                          axisInA.y(), axis1.y(), axis2.y(),
											  axisInA.z(), axis1.z(), axis2.z() );
				frameInA.setOrigin( pivotInA );

				btTransform inv = rb1->getCenterOfMassTransform().inverse();

				btTransform globalFrameA = rb0->getCenterOfMassTransform() * frameInA;
				
				frameInB = inv  * globalFrameA;
				
				coneTwistContraint = new btConeTwistConstraint(	*rb0,*rb1,
					frameInA,frameInB);


			} else
			{
				static btRigidBody s_fixedObject2( 0,0,0);
				btTransform frameInA;
				btTransform frameInB;
				
				btVector3 axis1, axis2;
				btPlaneSpace1( axisInA, axis1, axis2 );

				frameInA.getBasis().setValue( axisInA.x(), axis1.x(), axis2.x(),
					                          axisInA.y(), axis1.y(), axis2.y(),
											  axisInA.z(), axis1.z(), axis2.z() );

				frameInA.setOrigin( pivotInA );

				///frameInB in worldspace
				frameInB = rb0->getCenterOfMassTransform() * frameInA;

				coneTwistContraint = new btConeTwistConstraint(
					*rb0,s_fixedObject2,
					frameInA,frameInB);
			}
			
			if (coneTwistContraint)
			{
				//m_constraints.push_back(genericConstraint);
				m_dynamicsWorld->addConstraint(coneTwistContraint);
				coneTwistContraint->setUserConstraintId(gConstraintUid++);
				coneTwistContraint->setUserConstraintType(type);
				//64 bit systems can't cast pointer to int. could use size_t instead.
				return coneTwistContraint->getUserConstraintId();
			} 



			break;
		}
	case PHY_ANGULAR_CONSTRAINT:
		angularOnly = true;


	case PHY_LINEHINGE_CONSTRAINT:
		{
			btHingeConstraint* hinge = 0;

			if (rb1)
			{
				btVector3 axisInB = rb1 ? 
				(rb1->getCenterOfMassTransform().getBasis().inverse()*(rb0->getCenterOfMassTransform().getBasis() * axisInA)) : 
				rb0->getCenterOfMassTransform().getBasis() * axisInA;

				hinge = new btHingeConstraint(
					*rb0,
					*rb1,pivotInA,pivotInB,axisInA,axisInB);


			} else
			{
				hinge = new btHingeConstraint(*rb0,
					pivotInA,axisInA);

			}
			hinge->setAngularOnly(angularOnly);

			//m_constraints.push_back(hinge);
			m_dynamicsWorld->addConstraint(hinge);
			hinge->setUserConstraintId(gConstraintUid++);
			hinge->setUserConstraintType(type);
			//64 bit systems can't cast pointer to int. could use size_t instead.
			return hinge->getUserConstraintId();
			break;
		}
#ifdef NEW_BULLET_VEHICLE_SUPPORT

	case PHY_VEHICLE_CONSTRAINT:
		{
			btRaycastVehicle::btVehicleTuning* tuning = new btRaycastVehicle::btVehicleTuning();
			btRigidBody* chassis = rb0;
			btDefaultVehicleRaycaster* raycaster = new btDefaultVehicleRaycaster(m_dynamicsWorld);
			btRaycastVehicle* vehicle = new btRaycastVehicle(*tuning,chassis,raycaster);
			WrapperVehicle* wrapperVehicle = new WrapperVehicle(vehicle,ctrl0);
			m_wrapperVehicles.push_back(wrapperVehicle);
			m_dynamicsWorld->addVehicle(vehicle);
			vehicle->setUserConstraintId(gConstraintUid++);
			vehicle->setUserConstraintType(type);
			return vehicle->getUserConstraintId();

			break;
		};
#endif //NEW_BULLET_VEHICLE_SUPPORT

	default:
		{
		}
	};

	//btRigidBody& rbA,btRigidBody& rbB, const btVector3& pivotInA,const btVector3& pivotInB

	return 0;

}



PHY_IPhysicsController* CcdPhysicsEnvironment::CreateConeController(float coneradius,float coneheight)
{
	CcdConstructionInfo	cinfo;

	// we don't need a CcdShapeConstructionInfo for this shape:
	// it is simple enough for the standard copy constructor (see CcdPhysicsController::GetReplica)
	cinfo.m_collisionShape = new btConeShape(coneradius,coneheight);
	cinfo.m_MotionState = 0;
	cinfo.m_physicsEnv = this;
	cinfo.m_collisionFlags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
	DefaultMotionState* motionState = new DefaultMotionState();
	cinfo.m_MotionState = motionState;
	
	// we will add later the possibility to select the filter from option
	cinfo.m_collisionFilterMask = CcdConstructionInfo::AllFilter ^ CcdConstructionInfo::SensorFilter;
	cinfo.m_collisionFilterGroup = CcdConstructionInfo::SensorFilter;
	motionState->m_worldTransform.setIdentity();
//	motionState->m_worldTransform.setOrigin(btVector3(position[0],position[1],position[2]));

	CcdPhysicsController* sphereController = new CcdPhysicsController(cinfo);


	return sphereController;
}
	
float		CcdPhysicsEnvironment::getAppliedImpulse(int	constraintid)
{
	int i;
	int numConstraints = m_dynamicsWorld->getNumConstraints();
	for (i=0;i<numConstraints;i++)
	{
		btTypedConstraint* constraint = m_dynamicsWorld->getConstraint(i);
		if (constraint->getUserConstraintId() == constraintid)
		{
			return constraint->getAppliedImpulse();
		}
	}

	return 0.f;
}
