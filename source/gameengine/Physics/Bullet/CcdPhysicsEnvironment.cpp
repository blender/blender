/** \file gameengine/Physics/Bullet/CcdPhysicsEnvironment.cpp
 *  \ingroup physbullet
 */
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
#include "CcdGraphicController.h"

#include <algorithm>
#include "btBulletDynamicsCommon.h"
#include "LinearMath/btIDebugDraw.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletCollision/CollisionDispatch/btSimulationIslandManager.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"
#include "BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h"
#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"

//profiling/timings
#include "LinearMath/btQuickprof.h"


#include "PHY_IMotionState.h"
#include "PHY_ICharacter.h"
#include "PHY_Pro.h"
#include "KX_GameObject.h"
#include "KX_PythonInit.h" // for KX_RasterizerDrawDebugLine
#include "RAS_MeshObject.h"
#include "RAS_Polygon.h"
#include "RAS_TexVert.h"

#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "DNA_object_force.h"

extern "C" {
	#include "BLI_utildefines.h"
	#include "BKE_object.h"
}

#define CCD_CONSTRAINT_DISABLE_LINKED_COLLISION 0x80

#ifdef NEW_BULLET_VEHICLE_SUPPORT
#include "BulletDynamics/Vehicle/btRaycastVehicle.h"
#include "BulletDynamics/Vehicle/btVehicleRaycaster.h"
#include "BulletDynamics/Vehicle/btWheelInfo.h"
#include "PHY_IVehicle.h"
static btRaycastVehicle::btVehicleTuning	gTuning;

#endif //NEW_BULLET_VEHICLE_SUPPORT
#include "LinearMath/btAabbUtil2.h"
#include "MT_Matrix4x4.h"
#include "MT_Vector3.h"
#include "MT_MinMax.h"

#ifdef WIN32
void DrawRasterizerLine(const float* from,const float* to,int color);
#endif


#include "BulletDynamics/ConstraintSolver/btContactConstraint.h"


#include <stdio.h>
#include <string.h>		// for memset

// This was copied from the old KX_ConvertPhysicsObjects
#ifdef WIN32
#if defined(_MSC_VER) && (_MSC_VER >= 1310)
//only use SIMD Hull code under Win32
//#define TEST_HULL 1
#ifdef TEST_HULL
#define USE_HULL 1
//#define TEST_SIMD_HULL 1

#include "NarrowPhaseCollision/Hull.h"
#endif //#ifdef TEST_HULL

#endif //_MSC_VER
#endif //WIN32

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

	~WrapperVehicle()
	{
		delete m_vehicle;
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
		MT_Vector3	connectionPoint,
		MT_Vector3	downDirection,
		MT_Vector3	axleDirection,
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
			PHY_IMotionState* motionState = (PHY_IMotionState*)info.m_clientInfo;
	//		m_vehicle->updateWheelTransformsWS(info,false);
			m_vehicle->updateWheelTransform(i,false);
			btTransform trans = m_vehicle->getWheelInfo(i).m_worldTransform;
			btQuaternion orn = trans.getRotation();
			const btVector3& pos = trans.getOrigin();
			motionState->SetWorldOrientation(orn.x(),orn.y(),orn.z(),orn[3]);
			motionState->SetWorldPosition(pos.x(),pos.y(),pos.z());

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

class BlenderVehicleRaycaster: public btDefaultVehicleRaycaster
{
	btDynamicsWorld*	m_dynamicsWorld;
public:
	BlenderVehicleRaycaster(btDynamicsWorld* world)
		:btDefaultVehicleRaycaster(world), m_dynamicsWorld(world)
	{
	}

	virtual void* castRay(const btVector3& from,const btVector3& to, btVehicleRaycasterResult& result)
	{
	//	RayResultCallback& resultCallback;

		btCollisionWorld::ClosestRayResultCallback rayCallback(from,to);

		// We override btDefaultVehicleRaycaster so we can set this flag, otherwise our
		// vehicles go crazy (http://bulletphysics.org/Bullet/phpBB3/viewtopic.php?t=9662)
		rayCallback.m_flags |= btTriangleRaycastCallback::kF_UseSubSimplexConvexCastRaytest;

		m_dynamicsWorld->rayTest(from, to, rayCallback);

		if (rayCallback.hasHit())
		{

			const btRigidBody* body = btRigidBody::upcast(rayCallback.m_collisionObject);
			if (body && body->hasContactResponse())
			{
				result.m_hitPointInWorld = rayCallback.m_hitPointWorld;
				result.m_hitNormalInWorld = rayCallback.m_hitNormalWorld;
				result.m_hitNormalInWorld.normalize();
				result.m_distFraction = rayCallback.m_closestHitFraction;
				return (void*)body;
			}
		}
		return 0;
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


void CcdPhysicsEnvironment::SetDebugDrawer(btIDebugDraw* debugDrawer)
{
	if (debugDrawer && m_dynamicsWorld)
		m_dynamicsWorld->setDebugDrawer(debugDrawer);
	m_debugDrawer = debugDrawer;
}

#if 0
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
#endif





CcdPhysicsEnvironment::CcdPhysicsEnvironment(bool useDbvtCulling,btDispatcher* dispatcher,btOverlappingPairCache* pairCache)
:m_cullingCache(NULL),
m_cullingTree(NULL),
m_numIterations(10),
m_numTimeSubSteps(1),
m_ccdMode(0),
m_solverType(-1),
m_profileTimings(0),
m_enableSatCollisionDetection(false),
m_solver(NULL),
m_ownPairCache(NULL),
m_filterCallback(NULL),
m_ghostPairCallback(NULL),
m_ownDispatcher(NULL),
m_scalingPropagated(false)
{

	for (int i=0;i<PHY_NUM_RESPONSE;i++)
	{
		m_triggerCallbacks[i] = 0;
	}

//	m_collisionConfiguration = new btDefaultCollisionConfiguration();
	m_collisionConfiguration = new btSoftBodyRigidBodyCollisionConfiguration();
	//m_collisionConfiguration->setConvexConvexMultipointIterations();

	if (!dispatcher)
	{
		btCollisionDispatcher* disp = new btCollisionDispatcher(m_collisionConfiguration);
		dispatcher = disp;
		btGImpactCollisionAlgorithm::registerAlgorithm(disp);
		m_ownDispatcher = dispatcher;
	}

	//m_broadphase = new btAxisSweep3(btVector3(-1000,-1000,-1000),btVector3(1000,1000,1000));
	//m_broadphase = new btSimpleBroadphase();
	m_broadphase = new btDbvtBroadphase();
	// avoid any collision in the culling tree
	if (useDbvtCulling) {
		m_cullingCache = new btNullPairCache();
		m_cullingTree = new btDbvtBroadphase(m_cullingCache);
	}

	m_filterCallback = new CcdOverlapFilterCallBack(this);
	m_ghostPairCallback = new btGhostPairCallback();
	m_broadphase->getOverlappingPairCache()->setOverlapFilterCallback(m_filterCallback);
	m_broadphase->getOverlappingPairCache()->setInternalGhostPairCallback(m_ghostPairCallback);

	SetSolverType(1);//issues with quickstep and memory allocations
//	m_dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher,m_broadphase,m_solver,m_collisionConfiguration);
	m_dynamicsWorld = new btSoftRigidDynamicsWorld(dispatcher,m_broadphase,m_solver,m_collisionConfiguration);
	//m_dynamicsWorld->getSolverInfo().m_linearSlop = 0.01f;
	//m_dynamicsWorld->getSolverInfo().m_solverMode=	SOLVER_USE_WARMSTARTING +	SOLVER_USE_2_FRICTION_DIRECTIONS +	SOLVER_RANDMIZE_ORDER +	SOLVER_USE_FRICTION_WARMSTARTING;

	m_debugDrawer = 0;
	SetGravity(0.f,0.f,-9.81f);
}

void	CcdPhysicsEnvironment::AddCcdPhysicsController(CcdPhysicsController* ctrl)
{
	btRigidBody* body = ctrl->GetRigidBody();
	btCollisionObject* obj = ctrl->GetCollisionObject();

	//this m_userPointer is just used for triggers, see CallbackTriggers
	obj->setUserPointer(ctrl);
	if (body)
		body->setGravity( m_gravity );

	m_controllers.insert(ctrl);

	if (body)
	{
		//use explicit group/filter for finer control over collision in bullet => near/radar sensor
		m_dynamicsWorld->addRigidBody(body, ctrl->GetCollisionFilterGroup(), ctrl->GetCollisionFilterMask());
	} else
	{
		if (ctrl->GetSoftBody())
		{
			btSoftBody* softBody = ctrl->GetSoftBody();
			m_dynamicsWorld->addSoftBody(softBody);
		} else
		{
			if (obj->getCollisionShape())
			{
				m_dynamicsWorld->addCollisionObject(obj, ctrl->GetCollisionFilterGroup(), ctrl->GetCollisionFilterMask());
			}
			if (ctrl->GetCharacterController())
			{
				m_dynamicsWorld->addAction(ctrl->GetCharacterController());
			}
		}
	}
	if (obj->isStaticOrKinematicObject())
	{
		obj->setActivationState(ISLAND_SLEEPING);
	}

	assert(obj->getBroadphaseHandle());
}

		

bool	CcdPhysicsEnvironment::RemoveCcdPhysicsController(CcdPhysicsController* ctrl)
{
	//also remove constraint
	btRigidBody* body = ctrl->GetRigidBody();
	if (body)
	{
		for (int i=body->getNumConstraintRefs()-1;i>=0;i--)
		{
			btTypedConstraint* con = body->getConstraintRef(i);
			m_dynamicsWorld->removeConstraint(con);
			body->removeConstraintRef(con);
			//delete con; //might be kept by python KX_ConstraintWrapper
		}
		m_dynamicsWorld->removeRigidBody(ctrl->GetRigidBody());

		// Handle potential vehicle constraints
		int numVehicles = m_wrapperVehicles.size();
		int vehicle_constraint = 0;
		for (int i=0;i<numVehicles;i++)
		{
			WrapperVehicle* wrapperVehicle = m_wrapperVehicles[i];
			if (wrapperVehicle->GetChassis() == ctrl)
				vehicle_constraint = wrapperVehicle->GetVehicle()->getUserConstraintId();
		}

		if (vehicle_constraint > 0)
			RemoveConstraint(vehicle_constraint);
	} else
	{
		//if a softbody
		if (ctrl->GetSoftBody())
		{
			m_dynamicsWorld->removeSoftBody(ctrl->GetSoftBody());
		} else
		{
			m_dynamicsWorld->removeCollisionObject(ctrl->GetCollisionObject());

			if (ctrl->GetCharacterController())
			{
				m_dynamicsWorld->removeAction(ctrl->GetCharacterController());
			}
		}
	}
	if (ctrl->m_registerCount != 0)
		printf("Warning: removing controller with non-zero m_registerCount: %d\n", ctrl->m_registerCount);

	//remove it from the triggers
	m_triggerControllers.erase(ctrl);

	return (m_controllers.erase(ctrl) != 0);
}

void	CcdPhysicsEnvironment::UpdateCcdPhysicsController(CcdPhysicsController* ctrl, btScalar newMass, int newCollisionFlags, short int newCollisionGroup, short int newCollisionMask)
{
	// this function is used when the collisionning group of a controller is changed
	// remove and add the collistioning object
	btRigidBody* body = ctrl->GetRigidBody();
	btCollisionObject* obj = ctrl->GetCollisionObject();
	if (obj)
	{
		btVector3 inertia(0.0,0.0,0.0);
		m_dynamicsWorld->removeCollisionObject(obj);
		obj->setCollisionFlags(newCollisionFlags);
		if (body)
		{
			if (newMass)
				body->getCollisionShape()->calculateLocalInertia(newMass, inertia);
			body->setMassProps(newMass, inertia);
			m_dynamicsWorld->addRigidBody(body, newCollisionGroup, newCollisionMask);
		}
		else {
			m_dynamicsWorld->addCollisionObject(obj, newCollisionGroup, newCollisionMask);
		}
	}
	// to avoid nasty interaction, we must update the property of the controller as well
	ctrl->m_cci.m_mass = newMass;
	ctrl->m_cci.m_collisionFilterGroup = newCollisionGroup;
	ctrl->m_cci.m_collisionFilterMask = newCollisionMask;
	ctrl->m_cci.m_collisionFlags = newCollisionFlags;
}

void CcdPhysicsEnvironment::EnableCcdPhysicsController(CcdPhysicsController* ctrl)
{
	if (m_controllers.insert(ctrl).second)
	{
		btCollisionObject* obj = ctrl->GetCollisionObject();
		obj->setUserPointer(ctrl);
		// update the position of the object from the user
		if (ctrl->GetMotionState()) 
		{
			btTransform xform = CcdPhysicsController::GetTransformFromMotionState(ctrl->GetMotionState());
			ctrl->SetCenterOfMassTransform(xform);
		}
		m_dynamicsWorld->addCollisionObject(obj, 
			ctrl->GetCollisionFilterGroup(), ctrl->GetCollisionFilterMask());
	}
}

void CcdPhysicsEnvironment::DisableCcdPhysicsController(CcdPhysicsController* ctrl)
{
	if (m_controllers.erase(ctrl))
	{
		btRigidBody* body = ctrl->GetRigidBody();
		if (body)
		{
			m_dynamicsWorld->removeRigidBody(body);
		} else
		{
			if (ctrl->GetSoftBody())
			{
			} else
			{
				m_dynamicsWorld->removeCollisionObject(ctrl->GetCollisionObject());
			}
		}
	}
}

void CcdPhysicsEnvironment::RefreshCcdPhysicsController(CcdPhysicsController* ctrl)
{
	btCollisionObject* obj = ctrl->GetCollisionObject();
	if (obj)
	{
		btBroadphaseProxy* proxy = obj->getBroadphaseHandle();
		if (proxy)
		{
			m_dynamicsWorld->getPairCache()->cleanProxyFromPairs(proxy,m_dynamicsWorld->getDispatcher());
		}
	}
}

void CcdPhysicsEnvironment::AddCcdGraphicController(CcdGraphicController* ctrl)
{
	if (m_cullingTree && !ctrl->GetBroadphaseHandle())
	{
		btVector3	minAabb;
		btVector3	maxAabb;
		ctrl->GetAabb(minAabb, maxAabb);

		ctrl->SetBroadphaseHandle(m_cullingTree->createProxy(
				minAabb,
				maxAabb,
				INVALID_SHAPE_PROXYTYPE,	// this parameter is not used
				ctrl,
				0,							// this object does not collision with anything
				0,
				NULL,						// dispatcher => this parameter is not used
				0));

		assert(ctrl->GetBroadphaseHandle());
	}
}

void CcdPhysicsEnvironment::RemoveCcdGraphicController(CcdGraphicController* ctrl)
{
	if (m_cullingTree)
	{
		btBroadphaseProxy* bp = ctrl->GetBroadphaseHandle();
		if (bp)
		{
			m_cullingTree->destroyProxy(bp,NULL);
			ctrl->SetBroadphaseHandle(0);
		}
	}
}

void	CcdPhysicsEnvironment::BeginFrame()
{

}

void CcdPhysicsEnvironment::DebugDrawWorld()
{
	if (m_dynamicsWorld->getDebugDrawer() &&  m_dynamicsWorld->getDebugDrawer()->getDebugMode() >0)
			m_dynamicsWorld->debugDrawWorld();
}

bool	CcdPhysicsEnvironment::ProceedDeltaTime(double curTime,float timeStep,float interval)
{
	std::set<CcdPhysicsController*>::iterator it;
	int i;

	for (it=m_controllers.begin(); it!=m_controllers.end(); it++)
	{
		(*it)->SynchronizeMotionStates(timeStep);
	}

	float subStep = timeStep / float(m_numTimeSubSteps);
	i = m_dynamicsWorld->stepSimulation(interval,25,subStep);//perform always a full simulation step
//uncomment next line to see where Bullet spend its time (printf in console)
//CProfileManager::dumpAll();

	ProcessFhSprings(curTime,i*subStep);

	for (it=m_controllers.begin(); it!=m_controllers.end(); it++)
	{
		(*it)->SynchronizeMotionStates(timeStep);
	}

	//for (it=m_controllers.begin(); it!=m_controllers.end(); it++)
	//{
	//	(*it)->SynchronizeMotionStates(timeStep);
	//}

	for (i=0;i<m_wrapperVehicles.size();i++)
	{
		WrapperVehicle* veh = m_wrapperVehicles[i];
		veh->SyncWheels();
	}


	CallbackTriggers();

	return true;
}

class ClosestRayResultCallbackNotMe : public btCollisionWorld::ClosestRayResultCallback
{
	btCollisionObject* m_owner;
	btCollisionObject* m_parent;

public:
	ClosestRayResultCallbackNotMe(const btVector3& rayFromWorld,const btVector3& rayToWorld,btCollisionObject* owner,btCollisionObject* parent)
		:btCollisionWorld::ClosestRayResultCallback(rayFromWorld,rayToWorld),
		m_owner(owner),
		m_parent(parent)
	{
		
	}

	virtual bool needsCollision(btBroadphaseProxy* proxy0) const
	{
		//don't collide with self
		if (proxy0->m_clientObject == m_owner)
			return false;

		if (proxy0->m_clientObject == m_parent)
			return false;

		return btCollisionWorld::ClosestRayResultCallback::needsCollision(proxy0);
	}

};

void	CcdPhysicsEnvironment::ProcessFhSprings(double curTime,float interval)
{
	std::set<CcdPhysicsController*>::iterator it;
	// dynamic of Fh spring is based on a timestep of 1/60
	int numIter = (int)(interval*60.0001f);
	
	for (it=m_controllers.begin(); it!=m_controllers.end(); it++)
	{
		CcdPhysicsController* ctrl = (*it);
		btRigidBody* body = ctrl->GetRigidBody();

		if (body && (ctrl->GetConstructionInfo().m_do_fh || ctrl->GetConstructionInfo().m_do_rot_fh))
		{
			//printf("has Fh or RotFh\n");
			//re-implement SM_FhObject.cpp using btCollisionWorld::rayTest and info from ctrl->getConstructionInfo()
			//send a ray from {0.0, 0.0, 0.0} towards {0.0, 0.0, -10.0}, in local coordinates
			CcdPhysicsController* parentCtrl = ctrl->GetParentCtrl();
			btRigidBody* parentBody = parentCtrl?parentCtrl->GetRigidBody() : 0;
			btRigidBody* cl_object = parentBody ? parentBody : body;

			if (body->isStaticOrKinematicObject())
				continue;

			btVector3 rayDirLocal(0,0,-10);

			//m_dynamicsWorld
			//ctrl->GetRigidBody();
			btVector3 rayFromWorld = body->getCenterOfMassPosition();
			//btVector3	rayToWorld = rayFromWorld + body->getCenterOfMassTransform().getBasis() * rayDirLocal;
			//ray always points down the z axis in world space...
			btVector3	rayToWorld = rayFromWorld + rayDirLocal;
			
			ClosestRayResultCallbackNotMe	resultCallback(rayFromWorld,rayToWorld,body,parentBody);

			m_dynamicsWorld->rayTest(rayFromWorld,rayToWorld,resultCallback);
			if (resultCallback.hasHit())
			{
				//we hit this one: resultCallback.m_collisionObject;
				CcdPhysicsController* controller = static_cast<CcdPhysicsController*>(resultCallback.m_collisionObject->getUserPointer());

				if (controller)
				{
					if (controller->GetConstructionInfo().m_fh_distance < SIMD_EPSILON)
						continue;

					btRigidBody* hit_object = controller->GetRigidBody();
					if (!hit_object)
						continue;

					CcdConstructionInfo& hitObjShapeProps = controller->GetConstructionInfo();

					float distance = resultCallback.m_closestHitFraction*rayDirLocal.length()-ctrl->GetConstructionInfo().m_radius;
					if (distance >= hitObjShapeProps.m_fh_distance)
						continue;
					
					

					//btVector3 ray_dir = cl_object->getCenterOfMassTransform().getBasis()* rayDirLocal.normalized();
					btVector3 ray_dir = rayDirLocal.normalized();
					btVector3 normal = resultCallback.m_hitNormalWorld;
					normal.normalize();

					for (int i=0; i<numIter; i++)
					{
						if (ctrl->GetConstructionInfo().m_do_fh)
						{
							btVector3 lspot = cl_object->getCenterOfMassPosition() +
							        rayDirLocal * resultCallback.m_closestHitFraction;


								

							lspot -= hit_object->getCenterOfMassPosition();
							btVector3 rel_vel = cl_object->getLinearVelocity() - hit_object->getVelocityInLocalPoint(lspot);
							btScalar rel_vel_ray = ray_dir.dot(rel_vel);
							btScalar spring_extent = 1.0 - distance / hitObjShapeProps.m_fh_distance; 

							btScalar i_spring = spring_extent * hitObjShapeProps.m_fh_spring;
							btScalar i_damp =   rel_vel_ray * hitObjShapeProps.m_fh_damping;
							
							cl_object->setLinearVelocity(cl_object->getLinearVelocity() + (-(i_spring + i_damp) * ray_dir)); 
							if (hitObjShapeProps.m_fh_normal) 
							{
								cl_object->setLinearVelocity(cl_object->getLinearVelocity()+(i_spring + i_damp) *(normal - normal.dot(ray_dir) * ray_dir));
							}
							
							btVector3 lateral = rel_vel - rel_vel_ray * ray_dir;
							
							
							if (ctrl->GetConstructionInfo().m_do_anisotropic) {
								//Bullet basis contains no scaling/shear etc.
								const btMatrix3x3& lcs = cl_object->getCenterOfMassTransform().getBasis();
								btVector3 loc_lateral = lateral * lcs;
								const btVector3& friction_scaling = cl_object->getAnisotropicFriction();
								loc_lateral *= friction_scaling;
								lateral = lcs * loc_lateral;
							}

							btScalar rel_vel_lateral = lateral.length();
							
							if (rel_vel_lateral > SIMD_EPSILON) {
								btScalar friction_factor = hit_object->getFriction();//cl_object->getFriction();

								btScalar max_friction = friction_factor * btMax(btScalar(0.0), i_spring);
								
								btScalar rel_mom_lateral = rel_vel_lateral / cl_object->getInvMass();
								
								btVector3 friction = (rel_mom_lateral > max_friction) ?
									-lateral * (max_friction / rel_vel_lateral) :
									-lateral;
								
									cl_object->applyCentralImpulse(friction);
							}
						}

						
						if (ctrl->GetConstructionInfo().m_do_rot_fh) {
							btVector3 up2 = cl_object->getWorldTransform().getBasis().getColumn(2);

							btVector3 t_spring = up2.cross(normal) * hitObjShapeProps.m_fh_spring;
							btVector3 ang_vel = cl_object->getAngularVelocity();
							
							// only rotations that tilt relative to the normal are damped
							ang_vel -= ang_vel.dot(normal) * normal;
							
							btVector3 t_damp = ang_vel * hitObjShapeProps.m_fh_damping;  
							
							cl_object->setAngularVelocity(cl_object->getAngularVelocity() + (t_spring - t_damp));
						}
					}
				}
			}
		}
	}
}

void		CcdPhysicsEnvironment::SetDebugMode(int debugMode)
{
	if (m_debugDrawer) {
		m_debugDrawer->setDebugMode(debugMode);
	}
}

void		CcdPhysicsEnvironment::SetNumIterations(int numIter)
{
	m_numIterations = numIter;
}
void		CcdPhysicsEnvironment::SetDeactivationTime(float dTime)
{
	gDeactivationTime = dTime;
}
void		CcdPhysicsEnvironment::SetDeactivationLinearTreshold(float linTresh)
{
	gLinearSleepingTreshold = linTresh;
}
void		CcdPhysicsEnvironment::SetDeactivationAngularTreshold(float angTresh)
{
	gAngularSleepingTreshold = angTresh;
}

void		CcdPhysicsEnvironment::SetContactBreakingTreshold(float contactBreakingTreshold)
{
	gContactBreakingThreshold = contactBreakingTreshold;

}


void		CcdPhysicsEnvironment::SetCcdMode(int ccdMode)
{
	m_ccdMode = ccdMode;
}


void		CcdPhysicsEnvironment::SetSolverSorConstant(float sor)
{
	m_dynamicsWorld->getSolverInfo().m_sor = sor;
}

void		CcdPhysicsEnvironment::SetSolverTau(float tau)
{
	m_dynamicsWorld->getSolverInfo().m_tau = tau;
}
void		CcdPhysicsEnvironment::SetSolverDamping(float damping)
{
	m_dynamicsWorld->getSolverInfo().m_damping = damping;
}


void		CcdPhysicsEnvironment::SetLinearAirDamping(float damping)
{
	//gLinearAirDamping = damping;
}

void		CcdPhysicsEnvironment::SetUseEpa(bool epa)
{
	//gUseEpa = epa;
}

void		CcdPhysicsEnvironment::SetSolverType(int solverType)
{

	switch (solverType)
	{
	case 1:
		{
			if (m_solverType != solverType)
			{

				m_solver = new btSequentialImpulseConstraintSolver();
				
				
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

	m_solverType = solverType;
}



void		CcdPhysicsEnvironment::GetGravity(MT_Vector3& grav)
{
		const btVector3& gravity = m_dynamicsWorld->getGravity();
		grav[0] = gravity.getX();
		grav[1] = gravity.getY();
		grav[2] = gravity.getZ();
}


void		CcdPhysicsEnvironment::SetGravity(float x,float y,float z)
{
	m_gravity = btVector3(x,y,z);
	m_dynamicsWorld->setGravity(m_gravity);
	m_dynamicsWorld->getWorldInfo().m_gravity.setValue(x,y,z);
}




static int gConstraintUid = 1;

//Following the COLLADA physics specification for constraints
int			CcdPhysicsEnvironment::CreateUniversalD6Constraint(
						class PHY_IPhysicsController* ctrlRef,class PHY_IPhysicsController* ctrlOther,
						btTransform& frameInA,
						btTransform& frameInB,
						const btVector3& linearMinLimits,
						const btVector3& linearMaxLimits,
						const btVector3& angularMinLimits,
						const btVector3& angularMaxLimits,int flags
)
{

	bool disableCollisionBetweenLinkedBodies = (0!=(flags & CCD_CONSTRAINT_DISABLE_LINKED_COLLISION));

	//we could either add some logic to recognize ball-socket and hinge, or let that up to the user
	//perhaps some warning or hint that hinge/ball-socket is more efficient?
	
	
	btGeneric6DofConstraint* genericConstraint = 0;
	CcdPhysicsController* ctrl0 = (CcdPhysicsController*) ctrlRef;
	CcdPhysicsController* ctrl1 = (CcdPhysicsController*) ctrlOther;
	
	btRigidBody* rb0 = ctrl0->GetRigidBody();
	btRigidBody* rb1 = ctrl1->GetRigidBody();

	if (rb1)
	{
		

		bool useReferenceFrameA = true;
		genericConstraint = new btGeneric6DofSpringConstraint(
			*rb0,*rb1,
			frameInA,frameInB,useReferenceFrameA);
		genericConstraint->setLinearLowerLimit(linearMinLimits);
		genericConstraint->setLinearUpperLimit(linearMaxLimits);
		genericConstraint->setAngularLowerLimit(angularMinLimits);
		genericConstraint->setAngularUpperLimit(angularMaxLimits);
	} else
	{
		// TODO: Implement single body case...
		//No, we can use a fixed rigidbody in above code, rather than unnecessary duplation of code

	}
	
	if (genericConstraint)
	{
	//	m_constraints.push_back(genericConstraint);
		m_dynamicsWorld->addConstraint(genericConstraint,disableCollisionBetweenLinkedBodies);

		genericConstraint->setUserConstraintId(gConstraintUid++);
		genericConstraint->setUserConstraintType(PHY_GENERIC_6DOF_CONSTRAINT);
		//64 bit systems can't cast pointer to int. could use size_t instead.
		return genericConstraint->getUserConstraintId();
	}
	return 0;
}



void		CcdPhysicsEnvironment::RemoveConstraint(int	constraintId)
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

	WrapperVehicle *vehicle;
	if ((vehicle = (WrapperVehicle*)GetVehicleConstraint(constraintId)))
	{
		m_dynamicsWorld->removeVehicle(vehicle->GetVehicle());
		m_wrapperVehicles.erase(std::remove(m_wrapperVehicles.begin(), m_wrapperVehicles.end(), vehicle));
		delete vehicle;
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

	virtual bool needsCollision(btBroadphaseProxy* proxy0) const
	{
		if (!(proxy0->m_collisionFilterGroup & m_collisionFilterMask))
			return false;
		if (!(m_collisionFilterGroup & proxy0->m_collisionFilterMask))
			return false;
		btCollisionObject* object = (btCollisionObject*)proxy0->m_clientObject;
		CcdPhysicsController* phyCtrl = static_cast<CcdPhysicsController*>(object->getUserPointer());
		if (phyCtrl == m_phyRayFilter.m_ignoreController)
			return false;
		return m_phyRayFilter.needBroadphaseRayCast(phyCtrl);
	}

	virtual	btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult,bool normalInWorldSpace)
	{
		//CcdPhysicsController* curHit = static_cast<CcdPhysicsController*>(rayResult.m_collisionObject->getUserPointer());
		// save shape information as ClosestRayResultCallback::AddSingleResult() does not do it
		if (rayResult.m_localShapeInfo)
		{
			m_hitTriangleShape = rayResult.m_collisionObject->getCollisionShape();
			m_hitTriangleIndex = rayResult.m_localShapeInfo->m_triangleIndex;
		} else 
		{
			m_hitTriangleShape = NULL;
			m_hitTriangleIndex = 0;
		}
		return ClosestRayResultCallback::addSingleResult(rayResult,normalInWorldSpace);
	}

};

static bool GetHitTriangle(btCollisionShape* shape, CcdShapeConstructionInfo* shapeInfo, int hitTriangleIndex, btVector3 triangle[])
{
	// this code is copied from Bullet 
	const unsigned char *vertexbase;
	int numverts;
	PHY_ScalarType type;
	int stride;
	const unsigned char *indexbase;
	int indexstride;
	int numfaces;
	PHY_ScalarType indicestype;
	btStridingMeshInterface* meshInterface = NULL;
	btTriangleMeshShape* triangleShape = shapeInfo->GetMeshShape();

	if (triangleShape)
		meshInterface = triangleShape->getMeshInterface();
	else
	{
		// other possibility is gImpact
		if (shape->getShapeType() == GIMPACT_SHAPE_PROXYTYPE)
			meshInterface = (static_cast<btGImpactMeshShape*>(shape))->getMeshInterface();
	}
	if (!meshInterface)
		return false;

	meshInterface->getLockedReadOnlyVertexIndexBase(
		&vertexbase,
		numverts,
		type,
		stride,
		&indexbase,
		indexstride,
		numfaces,
		indicestype,
		0);

	unsigned int* gfxbase = (unsigned int*)(indexbase+hitTriangleIndex*indexstride);
	const btVector3& meshScaling = shape->getLocalScaling();
	for (int j=2;j>=0;j--)
	{
		int graphicsindex = (indicestype == PHY_SHORT) ? ((unsigned short *)gfxbase)[j] : gfxbase[j];

		btScalar* graphicsbase = (btScalar*)(vertexbase+graphicsindex*stride);

		triangle[j] = btVector3(graphicsbase[0]*meshScaling.getX(),graphicsbase[1]*meshScaling.getY(),graphicsbase[2]*meshScaling.getZ());
	}
	meshInterface->unLockReadOnlyVertexBase(0);
	return true;
}

PHY_IPhysicsController* CcdPhysicsEnvironment::RayTest(PHY_IRayCastFilterCallback &filterCallback, float fromX,float fromY,float fromZ, float toX,float toY,float toZ)
{
	btVector3 rayFrom(fromX,fromY,fromZ);
	btVector3 rayTo(toX,toY,toZ);

	btVector3	hitPointWorld,normalWorld;

	//Either Ray Cast with or without filtering

	//btCollisionWorld::ClosestRayResultCallback rayCallback(rayFrom,rayTo);
	FilterClosestRayResultCallback	 rayCallback(filterCallback,rayFrom,rayTo);


	PHY_RayCastResult result;
	memset(&result, 0, sizeof(result));

	// don't collision with sensor object
	rayCallback.m_collisionFilterMask = CcdConstructionInfo::AllFilter ^ CcdConstructionInfo::SensorFilter;
	// use faster (less accurate) ray callback, works better with 0 collision margins
	rayCallback.m_flags |= btTriangleRaycastCallback::kF_UseSubSimplexConvexCastRaytest;
	//, ,filterCallback.m_faceNormal);

	m_dynamicsWorld->rayTest(rayFrom,rayTo,rayCallback);
	if (rayCallback.hasHit())
	{
		CcdPhysicsController* controller = static_cast<CcdPhysicsController*>(rayCallback.m_collisionObject->getUserPointer());
		result.m_controller = controller;
		result.m_hitPoint[0] = rayCallback.m_hitPointWorld.getX();
		result.m_hitPoint[1] = rayCallback.m_hitPointWorld.getY();
		result.m_hitPoint[2] = rayCallback.m_hitPointWorld.getZ();

		if (rayCallback.m_hitTriangleShape != NULL)
		{
			// identify the mesh polygon
			CcdShapeConstructionInfo* shapeInfo = controller->m_shapeInfo;
			if (shapeInfo)
			{
				btCollisionShape* shape = controller->GetCollisionObject()->getCollisionShape();
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
					// save original collision shape triangle for soft body
					int hitTriangleIndex = rayCallback.m_hitTriangleIndex;

					result.m_meshObject = shapeInfo->GetMesh();
					if (shape->isSoftBody())
					{
						// soft body using different face numbering because of randomization
						// hopefully we have stored the original face number in m_tag
						const btSoftBody* softBody = static_cast<const btSoftBody*>(rayCallback.m_collisionObject);
						if (softBody->m_faces[hitTriangleIndex].m_tag != 0)
						{
							rayCallback.m_hitTriangleIndex = (int)((uintptr_t)(softBody->m_faces[hitTriangleIndex].m_tag)-1);
						}
					}
					// retrieve the original mesh polygon (in case of quad->tri conversion)
					result.m_polygon = shapeInfo->m_polygonIndexArray.at(rayCallback.m_hitTriangleIndex);
					// hit triangle in world coordinate, for face normal and UV coordinate
					btVector3 triangle[3];
					bool triangleOK = false;
					if (filterCallback.m_faceUV && (3*rayCallback.m_hitTriangleIndex) < shapeInfo->m_triFaceUVcoArray.size())
					{
						// interpolate the UV coordinate of the hit point
						CcdShapeConstructionInfo::UVco* uvCo = &shapeInfo->m_triFaceUVcoArray[3*rayCallback.m_hitTriangleIndex];
						// 1. get the 3 coordinate of the triangle in world space
						btVector3 v1, v2, v3;
						if (shape->isSoftBody())
						{
							// soft body give points directly in world coordinate
							const btSoftBody* softBody = static_cast<const btSoftBody*>(rayCallback.m_collisionObject);
							v1 = softBody->m_faces[hitTriangleIndex].m_n[0]->m_x;
							v2 = softBody->m_faces[hitTriangleIndex].m_n[1]->m_x;
							v3 = softBody->m_faces[hitTriangleIndex].m_n[2]->m_x;
						} else 
						{
							// for rigid body we must apply the world transform
							triangleOK = GetHitTriangle(shape, shapeInfo, hitTriangleIndex, triangle);
							if (!triangleOK)
								// if we cannot get the triangle, no use to continue
								goto SKIP_UV_NORMAL;
							v1 = rayCallback.m_collisionObject->getWorldTransform()(triangle[0]);
							v2 = rayCallback.m_collisionObject->getWorldTransform()(triangle[1]);
							v3 = rayCallback.m_collisionObject->getWorldTransform()(triangle[2]);
						}
						// 2. compute barycentric coordinate of the hit point
						btVector3 v = v2-v1;
						btVector3 w = v3-v1;
						btVector3 u = v.cross(w);
						btScalar A = u.length();

						v = v2-rayCallback.m_hitPointWorld;
						w = v3-rayCallback.m_hitPointWorld;
						u = v.cross(w);
						btScalar A1 = u.length();

						v = rayCallback.m_hitPointWorld-v1;
						w = v3-v1;
						u = v.cross(w);
						btScalar A2 = u.length();

						btVector3 baryCo;
						baryCo.setX(A1/A);
						baryCo.setY(A2/A);
						baryCo.setZ(1.0f-baryCo.getX()-baryCo.getY());
						// 3. compute UV coordinate
						result.m_hitUV[0] = baryCo.getX()*uvCo[0].uv[0] + baryCo.getY()*uvCo[1].uv[0] + baryCo.getZ()*uvCo[2].uv[0];
						result.m_hitUV[1] = baryCo.getX()*uvCo[0].uv[1] + baryCo.getY()*uvCo[1].uv[1] + baryCo.getZ()*uvCo[2].uv[1];
						result.m_hitUVOK = 1;
					}
						
					// Bullet returns the normal from "outside".
					// If the user requests the real normal, compute it now
					if (filterCallback.m_faceNormal)
					{
						if (shape->isSoftBody()) 
						{
							// we can get the real normal directly from the body
							const btSoftBody* softBody = static_cast<const btSoftBody*>(rayCallback.m_collisionObject);
							rayCallback.m_hitNormalWorld = softBody->m_faces[hitTriangleIndex].m_normal;
						} else
						{
							if (!triangleOK)
								triangleOK = GetHitTriangle(shape, shapeInfo, hitTriangleIndex, triangle);
							if (triangleOK)
							{
								btVector3 triangleNormal; 
								triangleNormal = (triangle[1]-triangle[0]).cross(triangle[2]-triangle[0]);
								rayCallback.m_hitNormalWorld = rayCallback.m_collisionObject->getWorldTransform().getBasis()*triangleNormal;
							}
						}
					}
				SKIP_UV_NORMAL:
					;
				}
			}
		}
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
		filterCallback.reportHit(&result);
	}


	return result.m_controller;
}

// Handles occlusion culling. 
// The implementation is based on the CDTestFramework
struct OcclusionBuffer
{
	struct WriteOCL
	{
		static inline bool Process(btScalar& q,btScalar v) { if (q<v) q=v;return(false); }
		static inline void Occlusion(bool& flag) { flag = true; }
	};
	struct QueryOCL
	{
		static inline bool Process(btScalar& q,btScalar v) { return(q<=v); }
		static inline void Occlusion(bool& flag) { }
	};
	btScalar*						m_buffer;
	size_t							m_bufferSize;
	bool							m_initialized;
	bool							m_occlusion;
	int								m_sizes[2];
	btScalar						m_scales[2];
	btScalar						m_offsets[2];
	btScalar						m_wtc[16];		// world to clip transform
	btScalar						m_mtc[16];		// model to clip transform
	// constructor: size=largest dimension of the buffer. 
	// Buffer size depends on aspect ratio
	OcclusionBuffer()
	{
		m_initialized=false;
		m_occlusion = false;
		m_buffer = NULL;
		m_bufferSize = 0;
	}
	// multiplication of column major matrices: m=m1*m2
	template<typename T1, typename T2>
	void		CMmat4mul(btScalar* m, const T1* m1, const T2* m2)
	{
		m[ 0] = btScalar(m1[ 0]*m2[ 0]+m1[ 4]*m2[ 1]+m1[ 8]*m2[ 2]+m1[12]*m2[ 3]);
		m[ 1] = btScalar(m1[ 1]*m2[ 0]+m1[ 5]*m2[ 1]+m1[ 9]*m2[ 2]+m1[13]*m2[ 3]);
		m[ 2] = btScalar(m1[ 2]*m2[ 0]+m1[ 6]*m2[ 1]+m1[10]*m2[ 2]+m1[14]*m2[ 3]);
		m[ 3] = btScalar(m1[ 3]*m2[ 0]+m1[ 7]*m2[ 1]+m1[11]*m2[ 2]+m1[15]*m2[ 3]);

		m[ 4] = btScalar(m1[ 0]*m2[ 4]+m1[ 4]*m2[ 5]+m1[ 8]*m2[ 6]+m1[12]*m2[ 7]);
		m[ 5] = btScalar(m1[ 1]*m2[ 4]+m1[ 5]*m2[ 5]+m1[ 9]*m2[ 6]+m1[13]*m2[ 7]);
		m[ 6] = btScalar(m1[ 2]*m2[ 4]+m1[ 6]*m2[ 5]+m1[10]*m2[ 6]+m1[14]*m2[ 7]);
		m[ 7] = btScalar(m1[ 3]*m2[ 4]+m1[ 7]*m2[ 5]+m1[11]*m2[ 6]+m1[15]*m2[ 7]);

		m[ 8] = btScalar(m1[ 0]*m2[ 8]+m1[ 4]*m2[ 9]+m1[ 8]*m2[10]+m1[12]*m2[11]);
		m[ 9] = btScalar(m1[ 1]*m2[ 8]+m1[ 5]*m2[ 9]+m1[ 9]*m2[10]+m1[13]*m2[11]);
		m[10] = btScalar(m1[ 2]*m2[ 8]+m1[ 6]*m2[ 9]+m1[10]*m2[10]+m1[14]*m2[11]);
		m[11] = btScalar(m1[ 3]*m2[ 8]+m1[ 7]*m2[ 9]+m1[11]*m2[10]+m1[15]*m2[11]);

		m[12] = btScalar(m1[ 0]*m2[12]+m1[ 4]*m2[13]+m1[ 8]*m2[14]+m1[12]*m2[15]);
		m[13] = btScalar(m1[ 1]*m2[12]+m1[ 5]*m2[13]+m1[ 9]*m2[14]+m1[13]*m2[15]);
		m[14] = btScalar(m1[ 2]*m2[12]+m1[ 6]*m2[13]+m1[10]*m2[14]+m1[14]*m2[15]);
		m[15] = btScalar(m1[ 3]*m2[12]+m1[ 7]*m2[13]+m1[11]*m2[14]+m1[15]*m2[15]);
	}
	void		setup(int size, const int *view, double modelview[16], double projection[16])
	{
		m_initialized=false;
		m_occlusion=false;
		// compute the size of the buffer
		int			maxsize;
		double		ratio;
		maxsize = (view[2] > view[3]) ? view[2] : view[3];
		assert(maxsize > 0);
		ratio = 1.0/(2*maxsize);
		// ensure even number
		m_sizes[0] = 2*((int)(size*view[2]*ratio+0.5));
		m_sizes[1] = 2*((int)(size*view[3]*ratio+0.5));
		m_scales[0]=btScalar(m_sizes[0]/2);
		m_scales[1]=btScalar(m_sizes[1]/2);
		m_offsets[0]=m_scales[0]+0.5f;
		m_offsets[1]=m_scales[1]+0.5f;
		// prepare matrix
		// at this time of the rendering, the modelview matrix is the 
		// world to camera transformation and the projection matrix is
		// camera to clip transformation. combine both so that
		CMmat4mul(m_wtc, projection, modelview);
	}
	void		initialize()
	{
		size_t newsize = (m_sizes[0]*m_sizes[1])*sizeof(btScalar);
		if (m_buffer)
		{
			// see if we can reuse
			if (newsize > m_bufferSize)
			{
				free(m_buffer);
				m_buffer = NULL;
				m_bufferSize = 0;
			}
		}
		if (!m_buffer)
		{
			m_buffer = (btScalar*)calloc(1, newsize);
			m_bufferSize = newsize;
		} else
		{
			// buffer exists already, just clears it
			memset(m_buffer, 0, newsize);
		}
		// memory allocate must succeed
		assert(m_buffer != NULL);
		m_initialized = true;
		m_occlusion = false;
	}
	void		SetModelMatrix(double *fl)
	{
		CMmat4mul(m_mtc,m_wtc,fl);
		if (!m_initialized)
			initialize();
	}

	// transform a segment in world coordinate to clip coordinate
	void		transformW(const btVector3& x, btVector4& t)
	{
		t[0]	=	x[0]*m_wtc[0]+x[1]*m_wtc[4]+x[2]*m_wtc[8]+m_wtc[12];
		t[1]	=	x[0]*m_wtc[1]+x[1]*m_wtc[5]+x[2]*m_wtc[9]+m_wtc[13];
		t[2]	=	x[0]*m_wtc[2]+x[1]*m_wtc[6]+x[2]*m_wtc[10]+m_wtc[14];
		t[3]	=	x[0]*m_wtc[3]+x[1]*m_wtc[7]+x[2]*m_wtc[11]+m_wtc[15];
	}
	void		transformM(const float* x, btVector4& t)
	{
		t[0]	=	x[0]*m_mtc[0]+x[1]*m_mtc[4]+x[2]*m_mtc[8]+m_mtc[12];
		t[1]	=	x[0]*m_mtc[1]+x[1]*m_mtc[5]+x[2]*m_mtc[9]+m_mtc[13];
		t[2]	=	x[0]*m_mtc[2]+x[1]*m_mtc[6]+x[2]*m_mtc[10]+m_mtc[14];
		t[3]	=	x[0]*m_mtc[3]+x[1]*m_mtc[7]+x[2]*m_mtc[11]+m_mtc[15];
	}
	// convert polygon to device coordinates
	static bool	project(btVector4* p,int n)
	{
		for (int i=0;i<n;++i)
		{
			p[i][2]=1/p[i][3];
			p[i][0]*=p[i][2];
			p[i][1]*=p[i][2];
		}
		return(true);
	}
	// pi: closed polygon in clip coordinate, NP = number of segments
	// po: same polygon with clipped segments removed
	template <const int NP>
	static int	clip(const btVector4* pi,btVector4* po)
	{
		btScalar	s[2*NP];
		btVector4	pn[2*NP];
		int			i, j, m, n, ni;
		// deal with near clipping
		for (i=0, m=0;i<NP;++i)
		{
			s[i]=pi[i][2]+pi[i][3];
			if (s[i]<0) m+=1<<i;
		}
		if (m==((1<<NP)-1)) 
			return(0);
		if (m!=0)
		{
			for (i=NP-1,j=0,n=0;j<NP;i=j++)
			{
				const btVector4&	a=pi[i];
				const btVector4&	b=pi[j];
				const btScalar		t=s[i]/(a[3]+a[2]-b[3]-b[2]);
				if ((t>0)&&(t<1))
				{
					pn[n][0]	=	a[0]+(b[0]-a[0])*t;
					pn[n][1]	=	a[1]+(b[1]-a[1])*t;
					pn[n][2]	=	a[2]+(b[2]-a[2])*t;
					pn[n][3]	=	a[3]+(b[3]-a[3])*t;
					++n;
				}
				if (s[j]>0) pn[n++]=b;
			}
			// ready to test far clipping, start from the modified polygon
			pi = pn;
			ni = n;
		} else
		{
			// no clipping on the near plane, keep same vector
			ni = NP;
		}
		// now deal with far clipping
		for (i=0, m=0;i<ni;++i)
		{
			s[i]=pi[i][2]-pi[i][3];
			if (s[i]>0) m+=1<<i;
		}
		if (m==((1<<ni)-1)) 
			return(0);
		if (m!=0)
		{
			for (i=ni-1,j=0,n=0;j<ni;i=j++)
			{
				const btVector4&	a=pi[i];
				const btVector4&	b=pi[j];
				const btScalar		t=s[i]/(a[2]-a[3]-b[2]+b[3]);
				if ((t>0)&&(t<1))
				{
					po[n][0]	=	a[0]+(b[0]-a[0])*t;
					po[n][1]	=	a[1]+(b[1]-a[1])*t;
					po[n][2]	=	a[2]+(b[2]-a[2])*t;
					po[n][3]	=	a[3]+(b[3]-a[3])*t;
					++n;
				}
				if (s[j]<0) po[n++]=b;
			}
			return(n);
		}
		for (int i=0;i<ni;++i) po[i]=pi[i];
		return(ni);
	}
	// write or check a triangle to buffer. a,b,c in device coordinates (-1,+1)
	template <typename POLICY>
	inline bool	draw(	const btVector4& a,
						const btVector4& b,
						const btVector4& c,
						const float face,
						const btScalar minarea)
	{
		const btScalar		a2=btCross(b-a,c-a)[2];
		if ((face*a2)<0.f || btFabs(a2)<minarea)
			return false;
		// further down we are normally going to write to the Zbuffer, mark it so
		POLICY::Occlusion(m_occlusion);

		int x[3], y[3], ib=1, ic=2;
		btScalar z[3];
		x[0]=(int)(a.x()*m_scales[0]+m_offsets[0]);
		y[0]=(int)(a.y()*m_scales[1]+m_offsets[1]);
		z[0]=a.z();
		if (a2 < 0.f)
		{
			// negative aire is possible with double face => must
			// change the order of b and c otherwise the algorithm doesn't work
			ib=2;
			ic=1;
		}
		x[ib]=(int)(b.x()*m_scales[0]+m_offsets[0]);
		x[ic]=(int)(c.x()*m_scales[0]+m_offsets[0]);
		y[ib]=(int)(b.y()*m_scales[1]+m_offsets[1]);
		y[ic]=(int)(c.y()*m_scales[1]+m_offsets[1]);
		z[ib]=b.z();
		z[ic]=c.z();
		const int		mix=btMax(0,btMin(x[0],btMin(x[1],x[2])));
		const int		mxx=btMin(m_sizes[0],1+btMax(x[0],btMax(x[1],x[2])));
		const int		miy=btMax(0,btMin(y[0],btMin(y[1],y[2])));
		const int		mxy=btMin(m_sizes[1],1+btMax(y[0],btMax(y[1],y[2])));
		const int		width=mxx-mix;
		const int		height=mxy-miy;
		if ((width*height) <= 1)
		{
			// degenerated in at most one single pixel
			btScalar* scan=&m_buffer[miy*m_sizes[0]+mix];
			// use for loop to detect the case where width or height == 0
			for (int iy=miy;iy<mxy;++iy)
			{
				for (int ix=mix;ix<mxx;++ix)
				{
					if (POLICY::Process(*scan,z[0])) 
						return(true);
					if (POLICY::Process(*scan,z[1])) 
						return(true);
					if (POLICY::Process(*scan,z[2])) 
						return(true);
				}
			}
		}
		else if (width == 1) {
			// Degenerated in at least 2 vertical lines
			// The algorithm below doesn't work when face has a single pixel width
			// We cannot use general formulas because the plane is degenerated. 
			// We have to interpolate along the 3 edges that overlaps and process each pixel.
			// sort the y coord to make formula simpler
			int ytmp;
			btScalar ztmp;
			if (y[0] > y[1]) { ytmp=y[1];y[1]=y[0];y[0]=ytmp;ztmp=z[1];z[1]=z[0];z[0]=ztmp; }
			if (y[0] > y[2]) { ytmp=y[2];y[2]=y[0];y[0]=ytmp;ztmp=z[2];z[2]=z[0];z[0]=ztmp; }
			if (y[1] > y[2]) { ytmp=y[2];y[2]=y[1];y[1]=ytmp;ztmp=z[2];z[2]=z[1];z[1]=ztmp; }
			int	dy[] = {y[0] - y[1],
			            y[1] - y[2],
			            y[2] - y[0]};
			btScalar dzy[3];
			dzy[0] = (dy[0]) ? (z[0] - z[1]) / dy[0] : btScalar(0.f);
			dzy[1] = (dy[1]) ? (z[1] - z[2]) / dy[1] : btScalar(0.f);
			dzy[2] = (dy[2]) ? (z[2] - z[0]) / dy[2] : btScalar(0.f);
			btScalar v[3] = {dzy[0] * (miy - y[0]) + z[0],
			                 dzy[1] * (miy - y[1]) + z[1],
			                 dzy[2] * (miy - y[2]) + z[2]};
			dy[0] = y[1]-y[0];
			dy[1] = y[0]-y[1];
			dy[2] = y[2]-y[0];
			btScalar* scan=&m_buffer[miy*m_sizes[0]+mix];
			for (int iy=miy;iy<mxy;++iy)
			{
				if (dy[0] >= 0 && POLICY::Process(*scan,v[0])) 
					return(true);
				if (dy[1] >= 0 && POLICY::Process(*scan,v[1])) 
					return(true);
				if (dy[2] >= 0 && POLICY::Process(*scan,v[2])) 
					return(true);
				scan+=m_sizes[0];
				v[0] += dzy[0]; v[1] += dzy[1]; v[2] += dzy[2];
				dy[0]--; dy[1]++, dy[2]--;
			}
		} else if (height == 1)
		{
			// Degenerated in at least 2 horizontal lines
			// The algorithm below doesn't work when face has a single pixel width
			// We cannot use general formulas because the plane is degenerated. 
			// We have to interpolate along the 3 edges that overlaps and process each pixel.
			int xtmp;
			btScalar ztmp;
			if (x[0] > x[1]) { xtmp=x[1];x[1]=x[0];x[0]=xtmp;ztmp=z[1];z[1]=z[0];z[0]=ztmp; }
			if (x[0] > x[2]) { xtmp=x[2];x[2]=x[0];x[0]=xtmp;ztmp=z[2];z[2]=z[0];z[0]=ztmp; }
			if (x[1] > x[2]) { xtmp=x[2];x[2]=x[1];x[1]=xtmp;ztmp=z[2];z[2]=z[1];z[1]=ztmp; }
			int dx[] = {x[0] - x[1],
			            x[1] - x[2],
			            x[2] - x[0]};
			btScalar dzx[3];
			dzx[0] = (dx[0]) ? (z[0]-z[1])/dx[0] : btScalar(0.f);
			dzx[1] = (dx[1]) ? (z[1]-z[2])/dx[1] : btScalar(0.f);
			dzx[2] = (dx[2]) ? (z[2]-z[0])/dx[2] : btScalar(0.f);
			btScalar v[3] = {dzx[0] * (mix - x[0]) + z[0],
			                 dzx[1] * (mix - x[1]) + z[1],
			                 dzx[2] * (mix - x[2]) + z[2]};
			dx[0] = x[1]-x[0];
			dx[1] = x[0]-x[1];
			dx[2] = x[2]-x[0];
			btScalar* scan=&m_buffer[miy*m_sizes[0]+mix];
			for (int ix=mix;ix<mxx;++ix)
			{
				if (dx[0] >= 0 && POLICY::Process(*scan,v[0])) 
					return(true);
				if (dx[1] >= 0 && POLICY::Process(*scan,v[1])) 
					return(true);
				if (dx[2] >= 0 && POLICY::Process(*scan,v[2])) 
					return(true);
				scan++;
				v[0] += dzx[0]; v[1] += dzx[1]; v[2] += dzx[2];
				dx[0]--; dx[1]++, dx[2]--;
			}
		}
		else {
			// general case
			const int       dx[] = {y[0] - y[1],
			                        y[1] - y[2],
			                        y[2] - y[0]};
			const int       dy[] = {x[1] - x[0] - dx[0] * width,
			                        x[2] - x[1] - dx[1] * width,
			                        x[0] - x[2] - dx[2] * width};
			const int       a = x[2] * y[0] + x[0] * y[1] - x[2] * y[1] - x[0] * y[2] + x[1] * y[2] - x[1] * y[0];
			const btScalar  ia = 1 / (btScalar)a;
			const btScalar  dzx = ia*(y[2]*(z[1]-z[0])+y[1]*(z[0]-z[2])+y[0]*(z[2]-z[1]));
			const btScalar  dzy = ia*(x[2]*(z[0]-z[1])+x[0]*(z[1]-z[2])+x[1]*(z[2]-z[0]))-(dzx*width);
			int             c[] = {miy*x[1]+mix*y[0]-x[1]*y[0]-mix*y[1]+x[0]*y[1]-miy*x[0],
			                        miy*x[2]+mix*y[1]-x[2]*y[1]-mix*y[2]+x[1]*y[2]-miy*x[1],
			                        miy*x[0]+mix*y[2]-x[0]*y[2]-mix*y[0]+x[2]*y[0]-miy*x[2]};
			btScalar        v = ia*((z[2]*c[0])+(z[0]*c[1])+(z[1]*c[2]));
			btScalar       *scan = &m_buffer[miy*m_sizes[0]];
			for (int iy=miy;iy<mxy;++iy)
			{
				for (int ix=mix;ix<mxx;++ix)
				{
					if ((c[0]>=0)&&(c[1]>=0)&&(c[2]>=0))
					{
						if (POLICY::Process(scan[ix],v)) 
							return(true);
					}
					c[0]+=dx[0];c[1]+=dx[1];c[2]+=dx[2];v+=dzx;
				}
				c[0]+=dy[0];c[1]+=dy[1];c[2]+=dy[2];v+=dzy;
				scan+=m_sizes[0];
			}
		}
		return(false);
	}
	// clip than write or check a polygon 
	template <const int NP,typename POLICY>
	inline bool	clipDraw(	const btVector4* p,
							const float face,
							btScalar minarea)
	{
		btVector4	o[NP*2];
		int			n=clip<NP>(p,o);
		bool		earlyexit=false;
		if (n)
		{
			project(o,n);
			for (int i=2;i<n && !earlyexit;++i)
			{
				earlyexit|=draw<POLICY>(o[0],o[i-1],o[i],face,minarea);
			}
		}
		return(earlyexit);
	}
	// add a triangle (in model coordinate)
	// face =  0.f if face is double side, 
	//      =  1.f if face is single sided and scale is positive
	//      = -1.f if face is single sided and scale is negative
	void		appendOccluderM(const float* a,
								const float* b,
								const float* c,
								const float face)
	{
		btVector4	p[3];
		transformM(a,p[0]);
		transformM(b,p[1]);
		transformM(c,p[2]);
		clipDraw<3,WriteOCL>(p,face,btScalar(0.f));
	}
	// add a quad (in model coordinate)
	void		appendOccluderM(const float* a,
								const float* b,
								const float* c,
								const float* d,
								const float face)
	{
		btVector4	p[4];
		transformM(a,p[0]);
		transformM(b,p[1]);
		transformM(c,p[2]);
		transformM(d,p[3]);
		clipDraw<4,WriteOCL>(p,face,btScalar(0.f));
	}
	// query occluder for a box (c=center, e=extend) in world coordinate
	inline bool	queryOccluderW(	const btVector3& c,
								const btVector3& e)
	{
		if (!m_occlusion)
			// no occlusion yet, no need to check
			return true;
		btVector4	x[8];
		transformW(btVector3(c[0]-e[0],c[1]-e[1],c[2]-e[2]),x[0]);
		transformW(btVector3(c[0]+e[0],c[1]-e[1],c[2]-e[2]),x[1]);
		transformW(btVector3(c[0]+e[0],c[1]+e[1],c[2]-e[2]),x[2]);
		transformW(btVector3(c[0]-e[0],c[1]+e[1],c[2]-e[2]),x[3]);
		transformW(btVector3(c[0]-e[0],c[1]-e[1],c[2]+e[2]),x[4]);
		transformW(btVector3(c[0]+e[0],c[1]-e[1],c[2]+e[2]),x[5]);
		transformW(btVector3(c[0]+e[0],c[1]+e[1],c[2]+e[2]),x[6]);
		transformW(btVector3(c[0]-e[0],c[1]+e[1],c[2]+e[2]),x[7]);
		for (int i=0;i<8;++i)
		{
			// the box is clipped, it's probably a large box, don't waste our time to check
			if ((x[i][2]+x[i][3])<=0) return(true);
		}
		static const int d[] = {1,0,3,2,
		                        4,5,6,7,
		                        4,7,3,0,
		                        6,5,1,2,
		                        7,6,2,3,
		                        5,4,0,1};
		for (unsigned int i = 0; i < (sizeof(d) / sizeof(d[0]));) {
			const btVector4 p[] = {x[d[i + 0]],
			                       x[d[i + 1]],
			                       x[d[i + 2]],
			                       x[d[i + 3]]};
			i += 4;
			if (clipDraw<4, QueryOCL>(p, 1.0f, 0.0f)) {
				return true;
			}
		}
		return false;
	}
};


struct	DbvtCullingCallback : btDbvt::ICollide
{
	PHY_CullingCallback m_clientCallback;
	void* m_userData;
	OcclusionBuffer *m_ocb;

	DbvtCullingCallback(PHY_CullingCallback clientCallback, void* userData)
	{
		m_clientCallback = clientCallback;
		m_userData = userData;
		m_ocb = NULL;
	}
	bool Descent(const btDbvtNode* node)
	{
		return(m_ocb->queryOccluderW(node->volume.Center(),node->volume.Extents()));
	}
	void Process(const btDbvtNode* node,btScalar depth)
	{
		Process(node);
	}
	void Process(const btDbvtNode* leaf)
	{
		btBroadphaseProxy*	proxy=(btBroadphaseProxy*)leaf->data;
		// the client object is a graphic controller
		CcdGraphicController* ctrl = static_cast<CcdGraphicController*>(proxy->m_clientObject);
		KX_ClientObjectInfo *info = (KX_ClientObjectInfo*)ctrl->GetNewClientInfo();
		if (m_ocb)
		{
			// means we are doing occlusion culling. Check if this object is an occluders
			KX_GameObject* gameobj = KX_GameObject::GetClientObject(info);
			if (gameobj && gameobj->GetOccluder())
			{
				double* fl = gameobj->GetOpenGLMatrixPtr()->getPointer();
				// this will create the occlusion buffer if not already done
				// and compute the transformation from model local space to clip space
				m_ocb->SetModelMatrix(fl);
				float face = (gameobj->IsNegativeScaling()) ? -1.0f : 1.0f;
				// walk through the meshes and for each add to buffer
				for (int i=0; i<gameobj->GetMeshCount(); i++)
				{
					RAS_MeshObject* meshobj = gameobj->GetMesh(i);
					const float *v1, *v2, *v3, *v4;

					int polycount = meshobj->NumPolygons();
					for (int j=0; j<polycount; j++)
					{
						RAS_Polygon* poly = meshobj->GetPolygon(j);
						switch (poly->VertexCount())
						{
						case 3:
							v1 = poly->GetVertex(0)->getXYZ();
							v2 = poly->GetVertex(1)->getXYZ();
							v3 = poly->GetVertex(2)->getXYZ();
							m_ocb->appendOccluderM(v1,v2,v3,((poly->IsTwoside())?0.f:face));
							break;
						case 4:
							v1 = poly->GetVertex(0)->getXYZ();
							v2 = poly->GetVertex(1)->getXYZ();
							v3 = poly->GetVertex(2)->getXYZ();
							v4 = poly->GetVertex(3)->getXYZ();
							m_ocb->appendOccluderM(v1,v2,v3,v4,((poly->IsTwoside())?0.f:face));
							break;
						}
					}
				}
			}
		}
		if (info)
			(*m_clientCallback)(info, m_userData);
	}
};

static OcclusionBuffer gOcb;
bool CcdPhysicsEnvironment::CullingTest(PHY_CullingCallback callback, void* userData, MT_Vector4 *planes, int nplanes, int occlusionRes, const int *viewport, double modelview[16], double projection[16])
{
	if (!m_cullingTree)
		return false;
	DbvtCullingCallback dispatcher(callback, userData);
	btVector3 planes_n[6];
	btScalar planes_o[6];
	if (nplanes > 6)
		nplanes = 6;
	for (int i=0; i<nplanes; i++)
	{
		planes_n[i].setValue(planes[i][0], planes[i][1], planes[i][2]);
		planes_o[i] = planes[i][3];
	}
	// if occlusionRes != 0 => occlusion culling
	if (occlusionRes)
	{
		gOcb.setup(occlusionRes, viewport, modelview, projection);
		dispatcher.m_ocb = &gOcb;
		// occlusion culling, the direction of the view is taken from the first plan which MUST be the near plane
		btDbvt::collideOCL(m_cullingTree->m_sets[1].m_root,planes_n,planes_o,planes_n[0],nplanes,dispatcher);
		btDbvt::collideOCL(m_cullingTree->m_sets[0].m_root,planes_n,planes_o,planes_n[0],nplanes,dispatcher);
	}
	else {
		btDbvt::collideKDOP(m_cullingTree->m_sets[1].m_root,planes_n,planes_o,nplanes,dispatcher);
		btDbvt::collideKDOP(m_cullingTree->m_sets[0].m_root,planes_n,planes_o,nplanes,dispatcher);
	}
	return true;
}

int	CcdPhysicsEnvironment::GetNumContactPoints()
{
	return 0;
}

void CcdPhysicsEnvironment::GetContactPoint(int i,float& hitX,float& hitY,float& hitZ,float& normalX,float& normalY,float& normalZ)
{

}




btBroadphaseInterface*	CcdPhysicsEnvironment::GetBroadphase()
{ 
	return m_dynamicsWorld->getBroadphase(); 
}

btDispatcher*	CcdPhysicsEnvironment::GetDispatcher()
{ 
	return m_dynamicsWorld->getDispatcher();
}

void CcdPhysicsEnvironment::MergeEnvironment(PHY_IPhysicsEnvironment *other_env)
{
	CcdPhysicsEnvironment *other = dynamic_cast<CcdPhysicsEnvironment*>(other_env);
	if (other == NULL) {
		printf("KX_Scene::MergeScene: Other scene is not using Bullet physics, not merging physics.\n");
		return;
	}

	std::set<CcdPhysicsController*>::iterator it;

	while (other->m_controllers.begin() != other->m_controllers.end())
	{
		it= other->m_controllers.begin();
		CcdPhysicsController* ctrl= (*it);

		other->RemoveCcdPhysicsController(ctrl);
		this->AddCcdPhysicsController(ctrl);
	}
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

	if (NULL != m_ghostPairCallback)
		delete m_ghostPairCallback;

	if (NULL != m_collisionConfiguration)
		delete m_collisionConfiguration;

	if (NULL != m_broadphase)
		delete m_broadphase;

	if (NULL != m_cullingTree)
		delete m_cullingTree;

	if (NULL != m_cullingCache)
		delete m_cullingCache;

}


float	CcdPhysicsEnvironment::GetConstraintParam(int constraintId,int param)
{
	btTypedConstraint* typedConstraint = GetConstraintById(constraintId);
	switch (typedConstraint->getUserConstraintType())
	{
	case PHY_GENERIC_6DOF_CONSTRAINT:
		{
			
			switch (param)
			{
			case 0:	case 1: case 2: 
				{
					//param = 0..2 are linear constraint values
					btGeneric6DofConstraint* genCons = (btGeneric6DofConstraint*)typedConstraint;
					genCons->calculateTransforms();
					return genCons->getRelativePivotPosition(param);
					break;
				}
				case 3: case 4: case 5:
				{
					//param = 3..5 are relative constraint (Euler) angles
					btGeneric6DofConstraint* genCons = (btGeneric6DofConstraint*)typedConstraint;
					genCons->calculateTransforms();
					return genCons->getAngle(param-3);
					break;
				}
			default:
				{
				}
			}
			break;
		};
	default:
		{
		};
	};
	return 0.f;
}

void	CcdPhysicsEnvironment::SetConstraintParam(int constraintId,int param,float value0,float value1)
{
	btTypedConstraint* typedConstraint = GetConstraintById(constraintId);
	switch (typedConstraint->getUserConstraintType())
	{
	case PHY_GENERIC_6DOF_CONSTRAINT:
		{
			
			switch (param)
			{
			case 0:	case 1: case 2: case 3: case 4: case 5:
				{
					//param = 0..5 are constraint limits, with low/high limit value
					btGeneric6DofConstraint* genCons = (btGeneric6DofConstraint*)typedConstraint;
					genCons->setLimit(param,value0,value1);
					break;
				}
			case 6: case 7: case 8:
				{
					//param = 6,7,8 are translational motors, with value0=target velocity, value1 = max motor force
					btGeneric6DofConstraint* genCons = (btGeneric6DofConstraint*)typedConstraint;
					int transMotorIndex = param-6;
					btTranslationalLimitMotor* transMotor = genCons->getTranslationalLimitMotor();
					transMotor->m_targetVelocity[transMotorIndex] = value0;
					transMotor->m_maxMotorForce[transMotorIndex] = value1;
					transMotor->m_enableMotor[transMotorIndex] = (value1>0.f);
					break;
				}
			case 9: case 10: case 11:
				{
					//param = 9,10,11 are rotational motors, with value0=target velocity, value1 = max motor force
					btGeneric6DofConstraint* genCons = (btGeneric6DofConstraint*)typedConstraint;
					int angMotorIndex = param-9;
					btRotationalLimitMotor* rotMotor = genCons->getRotationalLimitMotor(angMotorIndex);
					rotMotor->m_enableMotor = (value1 > 0.f);
					rotMotor->m_targetVelocity = value0;
					rotMotor->m_maxMotorForce = value1;
					break;
				}

			case 12: case 13: case 14: case 15: case 16: case 17:
			{
				//param 13-17 are for motorized springs on each of the degrees of freedom
					btGeneric6DofSpringConstraint* genCons = (btGeneric6DofSpringConstraint*)typedConstraint;
					int springIndex = param-12;
					if (value0!=0.f)
					{
						bool springEnabled = true;
						genCons->setStiffness(springIndex,value0);
						genCons->setDamping(springIndex,value1);
						genCons->enableSpring(springIndex,springEnabled);
						genCons->setEquilibriumPoint(springIndex);
					} else
					{
						bool springEnabled = false;
						genCons->enableSpring(springIndex,springEnabled);
					}
					break;
			}

			default:
				{
				}
			};
			break;
		};
	case PHY_CONE_TWIST_CONSTRAINT:
		{
			switch (param)
			{
			case 3: case 4: case 5:
				{
					//param = 3,4,5 are constraint limits, high limit values
					btConeTwistConstraint* coneTwist = (btConeTwistConstraint*)typedConstraint;
					if (value1<0.0f)
						coneTwist->setLimit(param,btScalar(BT_LARGE_FLOAT));
					else
						coneTwist->setLimit(param,value1);
					break;
				}
			default:
				{
				}
			};
			break;
		};
	case PHY_ANGULAR_CONSTRAINT:
	case PHY_LINEHINGE_CONSTRAINT:
		{
			switch (param)
			{
			case 3:
				{
					//param = 3 is a constraint limit, with low/high limit value
					btHingeConstraint* hingeCons = (btHingeConstraint*)typedConstraint;
					hingeCons->setLimit(value0,value1);
					break;
				}
			default:
				{
				}
			}
			break;
		};
	default:
		{
		};
	};
}

btTypedConstraint*	CcdPhysicsEnvironment::GetConstraintById(int constraintId)
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


void CcdPhysicsEnvironment::AddSensor(PHY_IPhysicsController* ctrl)
{

	CcdPhysicsController* ctrl1 = (CcdPhysicsController* )ctrl;
	// addSensor() is a "light" function for bullet because it is used
	// dynamically when the sensor is activated. Use enableCcdPhysicsController() instead 
	//if (m_controllers.insert(ctrl1).second)
	//{
	//	addCcdPhysicsController(ctrl1);
	//}
	EnableCcdPhysicsController(ctrl1);
}

bool CcdPhysicsEnvironment::RemoveCollisionCallback(PHY_IPhysicsController* ctrl)
{
	CcdPhysicsController* ccdCtrl = (CcdPhysicsController*)ctrl;
	if (!ccdCtrl->Unregister())
		return false;
	m_triggerControllers.erase(ccdCtrl);
	return true;
}


void CcdPhysicsEnvironment::RemoveSensor(PHY_IPhysicsController* ctrl)
{
	DisableCcdPhysicsController((CcdPhysicsController*)ctrl);
}

void CcdPhysicsEnvironment::AddTouchCallback(int response_class, PHY_ResponseCallback callback, void *user)
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
bool CcdPhysicsEnvironment::RequestCollisionCallback(PHY_IPhysicsController* ctrl)
{
	CcdPhysicsController* ccdCtrl = static_cast<CcdPhysicsController*>(ctrl);

	if (!ccdCtrl->Register())
		return false;
	m_triggerControllers.insert(ccdCtrl);
	return true;
}

void	CcdPhysicsEnvironment::CallbackTriggers()
{
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
				const btRigidBody* rb0 = static_cast<const btRigidBody*>(manifold->getBody0());
				const btRigidBody* rb1 = static_cast<const btRigidBody*>(manifold->getBody1());
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
				const btRigidBody* obj0 = rb0;
				const btRigidBody* obj1 = rb1;

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

	KX_GameObject *kxObj0 = KX_GameObject::GetClientObject(
			(KX_ClientObjectInfo*)
			((CcdPhysicsController*)
					(((btCollisionObject*)proxy0->m_clientObject)->getUserPointer()))
			->GetNewClientInfo());
	KX_GameObject *kxObj1 = KX_GameObject::GetClientObject(
			(KX_ClientObjectInfo*)
			((CcdPhysicsController*)
					(((btCollisionObject*)proxy1->m_clientObject)->getUserPointer()))
			->GetNewClientInfo());

	// First check the filters. Note that this is called during scene
	// conversion, so we can't assume the KX_GameObject instances exist. This
	// may make some objects erroneously collide on the first frame, but the
	// alternative is to have them erroneously miss.
	bool collides;
	collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
	collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);
	if (kxObj0 && kxObj1) {
		collides = collides && kxObj0->CheckCollision(kxObj1);
		collides = collides && kxObj1->CheckCollision(kxObj0);
	}
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
PHY_IVehicle*	CcdPhysicsEnvironment::GetVehicleConstraint(int constraintId)
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


PHY_ICharacter* CcdPhysicsEnvironment::GetCharacterController(KX_GameObject *ob)
{
	CcdPhysicsController* controller = (CcdPhysicsController*)ob->GetPhysicsController();
	return (controller) ? dynamic_cast<BlenderBulletCharacterController*>(controller->GetCharacterController()) : NULL;
}


PHY_IPhysicsController*	CcdPhysicsEnvironment::CreateSphereController(float radius,const MT_Vector3& position)
{
	
	CcdConstructionInfo	cinfo;
	memset(&cinfo, 0, sizeof(cinfo)); /* avoid uninitialized values */
	cinfo.m_collisionShape = new btSphereShape(radius); // memory leak! The shape is not deleted by Bullet and we cannot add it to the KX_Scene.m_shapes list
	cinfo.m_MotionState = 0;
	cinfo.m_physicsEnv = this;
	// declare this object as Dyamic rather than static!!
	// The reason as it is designed to detect all type of object, including static object
	// It would cause static-static message to be printed on the console otherwise
	cinfo.m_collisionFlags |= btCollisionObject::CF_NO_CONTACT_RESPONSE | btCollisionObject::CF_STATIC_OBJECT;
	DefaultMotionState* motionState = new DefaultMotionState();
	cinfo.m_MotionState = motionState;
	// we will add later the possibility to select the filter from option
	cinfo.m_collisionFilterMask = CcdConstructionInfo::AllFilter ^ CcdConstructionInfo::SensorFilter;
	cinfo.m_collisionFilterGroup = CcdConstructionInfo::SensorFilter;
	cinfo.m_bSensor = true;
	motionState->m_worldTransform.setIdentity();
	motionState->m_worldTransform.setOrigin(btVector3(position[0],position[1],position[2]));

	CcdPhysicsController* sphereController = new CcdPhysicsController(cinfo);
	
	return sphereController;
}

int findClosestNode(btSoftBody* sb,const btVector3& worldPoint);
int findClosestNode(btSoftBody* sb,const btVector3& worldPoint)
{
	int node = -1;

	btSoftBody::tNodeArray&   nodes(sb->m_nodes);
	float maxDistSqr = 1e30f;

	for (int n=0;n<nodes.size();n++)
	{
		btScalar distSqr = (nodes[n].m_x - worldPoint).length2();
		if (distSqr<maxDistSqr)
		{
			maxDistSqr = distSqr;
			node = n;
		}
	}
	return node;
}

int			CcdPhysicsEnvironment::CreateConstraint(class PHY_IPhysicsController* ctrl0,class PHY_IPhysicsController* ctrl1,PHY_ConstraintType type,
													float pivotX,float pivotY,float pivotZ,
													float axisX,float axisY,float axisZ,
													float axis1X,float axis1Y,float axis1Z,
													float axis2X,float axis2Y,float axis2Z,int flags
													)
{

	bool disableCollisionBetweenLinkedBodies = (0!=(flags & CCD_CONSTRAINT_DISABLE_LINKED_COLLISION));



	CcdPhysicsController* c0 = (CcdPhysicsController*)ctrl0;
	CcdPhysicsController* c1 = (CcdPhysicsController*)ctrl1;

	btRigidBody* rb0 = c0 ? c0->GetRigidBody() : 0;
	btRigidBody* rb1 = c1 ? c1->GetRigidBody() : 0;

	


	bool rb0static = rb0 ? rb0->isStaticOrKinematicObject() : true;
	bool rb1static = rb1 ? rb1->isStaticOrKinematicObject() : true;

	btCollisionObject* colObj0 = c0->GetCollisionObject();
	if (!colObj0)
	{
		return 0;
	}

	btVector3 pivotInA(pivotX,pivotY,pivotZ);

	

	//it might be a soft body, let's try
	btSoftBody* sb0 = c0 ? c0->GetSoftBody() : 0;
	btSoftBody* sb1 = c1 ? c1->GetSoftBody() : 0;
	if (sb0 && sb1)
	{
		//not between two soft bodies?
		return 0;
	}

	if (sb0)
	{
		//either cluster or node attach, let's find closest node first
		//the soft body doesn't have a 'real' world transform, so get its initial world transform for now
		btVector3 pivotPointSoftWorld = sb0->m_initialWorldTransform(pivotInA);
		int node=findClosestNode(sb0,pivotPointSoftWorld);
		if (node >=0)
		{
			bool clusterconstaint = false;
/*
			switch (type)
			{
			case PHY_LINEHINGE_CONSTRAINT:
				{
					if (sb0->clusterCount() && rb1)
					{
						btSoftBody::LJoint::Specs	ls;
						ls.erp=0.5f;
						ls.position=sb0->clusterCom(0);
						sb0->appendLinearJoint(ls,rb1);
						clusterconstaint = true;
						break;
					}
				}
			case PHY_GENERIC_6DOF_CONSTRAINT:
				{
					if (sb0->clusterCount() && rb1)
					{
						btSoftBody::AJoint::Specs as;
						as.erp = 1;
						as.cfm = 1;
						as.axis.setValue(axisX,axisY,axisZ);
						sb0->appendAngularJoint(as,rb1);
						clusterconstaint = true;
						break;
					}

					break;
				}
			default:
				{
				
				}
			};
			*/

			if (!clusterconstaint)
			{
				if (rb1)
				{
					sb0->appendAnchor(node,rb1,disableCollisionBetweenLinkedBodies);
				} else
				{
					sb0->setMass(node,0.f);
				}
			}

			
		}
		return 0;//can't remove soft body anchors yet
	}

	if (sb1)
	{
		btVector3 pivotPointAWorld = colObj0->getWorldTransform()(pivotInA);
		int node=findClosestNode(sb1,pivotPointAWorld);
		if (node >=0)
		{
			bool clusterconstaint = false;

			/*
			switch (type)
			{
			case PHY_LINEHINGE_CONSTRAINT:
				{
					if (sb1->clusterCount() && rb0)
					{
						btSoftBody::LJoint::Specs	ls;
						ls.erp=0.5f;
						ls.position=sb1->clusterCom(0);
						sb1->appendLinearJoint(ls,rb0);
						clusterconstaint = true;
						break;
					}
				}
			case PHY_GENERIC_6DOF_CONSTRAINT:
				{
					if (sb1->clusterCount() && rb0)
					{
						btSoftBody::AJoint::Specs as;
						as.erp = 1;
						as.cfm = 1;
						as.axis.setValue(axisX,axisY,axisZ);
						sb1->appendAngularJoint(as,rb0);
						clusterconstaint = true;
						break;
					}

					break;
				}
			default:
				{
					

				}
			};*/


			if (!clusterconstaint)
			{
				if (rb0)
				{
					sb1->appendAnchor(node,rb0,disableCollisionBetweenLinkedBodies);
				} else
				{
					sb1->setMass(node,0.f);
				}
			}
			

		}
		return 0;//can't remove soft body anchors yet
	}

	if (rb0static && rb1static)
	{
		
		return 0;
	}
	

	if (!rb0)
		return 0;

	
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

			m_dynamicsWorld->addConstraint(p2p,disableCollisionBetweenLinkedBodies);
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
				bool useReferenceFrameA = true;

				genericConstraint = new btGeneric6DofSpringConstraint(
					*rb0,*rb1,
					frameInA,frameInB,useReferenceFrameA);


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

				bool useReferenceFrameA = true;
				genericConstraint = new btGeneric6DofSpringConstraint(
					*rb0,s_fixedObject2,
					frameInA,frameInB,useReferenceFrameA);
			}
			
			if (genericConstraint)
			{
				//m_constraints.push_back(genericConstraint);
				m_dynamicsWorld->addConstraint(genericConstraint,disableCollisionBetweenLinkedBodies);
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
				m_dynamicsWorld->addConstraint(coneTwistContraint,disableCollisionBetweenLinkedBodies);
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
				// We know the orientations so we should use them instead of
				// having btHingeConstraint fill in the blanks any way it wants to.
				btTransform frameInA;
				btTransform frameInB;
				
				btVector3 axis1(axis1X,axis1Y,axis1Z), axis2(axis2X,axis2Y,axis2Z);
				if (axis1.length() == 0.0)
				{
					btPlaneSpace1( axisInA, axis1, axis2 );
				}
				
				// Internally btHingeConstraint's hinge-axis is z
				frameInA.getBasis().setValue( axis1.x(), axis2.x(), axisInA.x(),
											axis1.y(), axis2.y(), axisInA.y(),
											axis1.z(), axis2.z(), axisInA.z() );
											
				frameInA.setOrigin( pivotInA );

				btTransform inv = rb1->getCenterOfMassTransform().inverse();

				btTransform globalFrameA = rb0->getCenterOfMassTransform() * frameInA;
				
				frameInB = inv  * globalFrameA;
				
				hinge = new btHingeConstraint(*rb0,*rb1,frameInA,frameInB);


			} else
			{
				static btRigidBody s_fixedObject2( 0,0,0);

				btTransform frameInA;
				btTransform frameInB;
				
				btVector3 axis1(axis1X,axis1Y,axis1Z), axis2(axis2X,axis2Y,axis2Z);
				if (axis1.length() == 0.0)
				{
					btPlaneSpace1( axisInA, axis1, axis2 );
				}

				// Internally btHingeConstraint's hinge-axis is z
				frameInA.getBasis().setValue( axis1.x(), axis2.x(), axisInA.x(),
											axis1.y(), axis2.y(), axisInA.y(),
											axis1.z(), axis2.z(), axisInA.z() );
				frameInA.setOrigin( pivotInA );
				frameInB = rb0->getCenterOfMassTransform() * frameInA;

				hinge = new btHingeConstraint(*rb0, s_fixedObject2, frameInA, frameInB);
			}
			hinge->setAngularOnly(angularOnly);

			//m_constraints.push_back(hinge);
			m_dynamicsWorld->addConstraint(hinge,disableCollisionBetweenLinkedBodies);
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
			btDefaultVehicleRaycaster* raycaster = new BlenderVehicleRaycaster(m_dynamicsWorld);
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
//don't memset cinfo: this is C++ and values should be set in the constructor!

	// we don't need a CcdShapeConstructionInfo for this shape:
	// it is simple enough for the standard copy constructor (see CcdPhysicsController::GetReplica)
	cinfo.m_collisionShape = new btConeShape(coneradius,coneheight);
	cinfo.m_MotionState = 0;
	cinfo.m_physicsEnv = this;
	cinfo.m_collisionFlags |= btCollisionObject::CF_NO_CONTACT_RESPONSE | btCollisionObject::CF_STATIC_OBJECT;
	DefaultMotionState* motionState = new DefaultMotionState();
	cinfo.m_MotionState = motionState;
	
	// we will add later the possibility to select the filter from option
	cinfo.m_collisionFilterMask = CcdConstructionInfo::AllFilter ^ CcdConstructionInfo::SensorFilter;
	cinfo.m_collisionFilterGroup = CcdConstructionInfo::SensorFilter;
	cinfo.m_bSensor = true;
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

void	CcdPhysicsEnvironment::ExportFile(const char* filename)
{
	btDefaultSerializer*	serializer = new btDefaultSerializer();
	
		
	for (int i=0;i<m_dynamicsWorld->getNumCollisionObjects();i++)
	{

		btCollisionObject* colObj = m_dynamicsWorld->getCollisionObjectArray()[i];

		CcdPhysicsController* controller = static_cast<CcdPhysicsController*>(colObj->getUserPointer());
		if (controller)
		{
			const char* name = KX_GameObject::GetClientObject((KX_ClientObjectInfo*)controller->GetNewClientInfo())->GetName();
			if (name)
			{
				serializer->registerNameForPointer(colObj,name);
			}
		}
	}

	m_dynamicsWorld->serialize(serializer);

	FILE* file = fopen(filename,"wb");
	if (file)
	{
		fwrite(serializer->getBufferPointer(),serializer->getCurrentBufferSize(),1, file);
		fclose(file);
	}
}

struct	BlenderDebugDraw : public btIDebugDraw
{
	BlenderDebugDraw () :
		m_debugMode(0)
	{
	}

	int m_debugMode;

	virtual void	drawLine(const btVector3& from,const btVector3& to,const btVector3& color)
	{
		if (m_debugMode >0)
		{
			MT_Vector3 kxfrom(from[0],from[1],from[2]);
			MT_Vector3 kxto(to[0],to[1],to[2]);
			MT_Vector3 kxcolor(color[0],color[1],color[2]);

			KX_RasterizerDrawDebugLine(kxfrom,kxto,kxcolor);
		}
	}

	virtual void	reportErrorWarning(const char* warningString)
	{

	}

	virtual void	drawContactPoint(const btVector3& PointOnB,const btVector3& normalOnB,float distance,int lifeTime,const btVector3& color)
	{
		//not yet
	}

	virtual void	setDebugMode(int debugMode)
	{
		m_debugMode = debugMode;
	}
	virtual int		getDebugMode() const
	{
		return m_debugMode;
	}
	///todo: find out if Blender can do this
	virtual void	draw3dText(const btVector3& location,const char* textString)
	{

	}

};

CcdPhysicsEnvironment *CcdPhysicsEnvironment::Create(Scene *blenderscene, bool visualizePhysics)
{
	CcdPhysicsEnvironment* ccdPhysEnv = new CcdPhysicsEnvironment((blenderscene->gm.mode & WO_DBVT_CULLING) != 0);
	ccdPhysEnv->SetDebugDrawer(new BlenderDebugDraw());
	ccdPhysEnv->SetDeactivationLinearTreshold(blenderscene->gm.lineardeactthreshold);
	ccdPhysEnv->SetDeactivationAngularTreshold(blenderscene->gm.angulardeactthreshold);
	ccdPhysEnv->SetDeactivationTime(blenderscene->gm.deactivationtime);

	if (visualizePhysics)
		ccdPhysEnv->SetDebugMode(btIDebugDraw::DBG_DrawWireframe|btIDebugDraw::DBG_DrawAabb|btIDebugDraw::DBG_DrawContactPoints|btIDebugDraw::DBG_DrawText|btIDebugDraw::DBG_DrawConstraintLimits|btIDebugDraw::DBG_DrawConstraints);

	return ccdPhysEnv;
}

void CcdPhysicsEnvironment::ConvertObject(KX_GameObject *gameobj, RAS_MeshObject *meshobj, DerivedMesh *dm, KX_Scene *kxscene, PHY_ShapeProps *shapeprops, PHY_MaterialProps *smmaterial, PHY_IMotionState *motionstate, int activeLayerBitInfo, bool isCompoundChild, bool hasCompoundChildren)
{
	Object* blenderobject = gameobj->GetBlenderObject();

	bool isbulletdyna = (blenderobject->gameflag & OB_DYNAMIC) != 0;;
	bool isbulletsensor = (blenderobject->gameflag & OB_SENSOR) != 0;
	bool isbulletchar = (blenderobject->gameflag & OB_CHARACTER) != 0;
	bool isbulletsoftbody = (blenderobject->gameflag & OB_SOFT_BODY) != 0;
	bool isbulletrigidbody = (blenderobject->gameflag & OB_RIGID_BODY) != 0;
	bool useGimpact = false;
	CcdConstructionInfo ci;
	class CcdShapeConstructionInfo *shapeInfo = new CcdShapeConstructionInfo();

	KX_GameObject *parent = gameobj->GetParent();
	if (parent)
	{
		isbulletdyna = false;
		isbulletsoftbody = false;
		shapeprops->m_mass = 0.f;
	}

	if (!isbulletdyna)
	{
		ci.m_collisionFlags |= btCollisionObject::CF_STATIC_OBJECT;
	}
	if ((blenderobject->gameflag & (OB_GHOST | OB_SENSOR | OB_CHARACTER)) != 0)
	{
		ci.m_collisionFlags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
	}

	ci.m_MotionState = motionstate;
	ci.m_gravity = btVector3(0,0,0);
	ci.m_linearFactor = btVector3(((blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_X_AXIS) !=0)? 0 : 1,
									((blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_Y_AXIS) !=0)? 0 : 1,
									((blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_Z_AXIS) !=0)? 0 : 1);
	ci.m_angularFactor = btVector3(((blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_X_ROT_AXIS) !=0)? 0 : 1,
									((blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_Y_ROT_AXIS) !=0)? 0 : 1,
									((blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_Z_ROT_AXIS) !=0)? 0 : 1);
	ci.m_localInertiaTensor =btVector3(0,0,0);
	ci.m_mass = isbulletdyna ? shapeprops->m_mass : 0.f;
	ci.m_clamp_vel_min = shapeprops->m_clamp_vel_min;
	ci.m_clamp_vel_max = shapeprops->m_clamp_vel_max;
	ci.m_stepHeight = isbulletchar ? shapeprops->m_step_height : 0.f;
	ci.m_jumpSpeed = isbulletchar ? shapeprops->m_jump_speed : 0.f;
	ci.m_fallSpeed = isbulletchar ? shapeprops->m_fall_speed : 0.f;

	//mmm, for now, take this for the size of the dynamicobject
	// Blender uses inertia for radius of dynamic object
	shapeInfo->m_radius = ci.m_radius = blenderobject->inertia;
	useGimpact = ((isbulletdyna || isbulletsensor) && !isbulletsoftbody);

	if (isbulletsoftbody)
	{
		if (blenderobject->bsoft)
		{
			ci.m_margin = blenderobject->bsoft->margin;
			ci.m_gamesoftFlag = blenderobject->bsoft->flag;

			ci.m_soft_linStiff = blenderobject->bsoft->linStiff;
			ci.m_soft_angStiff = blenderobject->bsoft->angStiff;		/* angular stiffness 0..1 */
			ci.m_soft_volume = blenderobject->bsoft->volume;			/* volume preservation 0..1 */

			ci.m_soft_viterations = blenderobject->bsoft->viterations;		/* Velocities solver iterations */
			ci.m_soft_piterations = blenderobject->bsoft->piterations;		/* Positions solver iterations */
			ci.m_soft_diterations = blenderobject->bsoft->diterations;		/* Drift solver iterations */
			ci.m_soft_citerations = blenderobject->bsoft->citerations;		/* Cluster solver iterations */

			ci.m_soft_kSRHR_CL = blenderobject->bsoft->kSRHR_CL;		/* Soft vs rigid hardness [0,1] (cluster only) */
			ci.m_soft_kSKHR_CL = blenderobject->bsoft->kSKHR_CL;		/* Soft vs kinetic hardness [0,1] (cluster only) */
			ci.m_soft_kSSHR_CL = blenderobject->bsoft->kSSHR_CL;		/* Soft vs soft hardness [0,1] (cluster only) */
			ci.m_soft_kSR_SPLT_CL = blenderobject->bsoft->kSR_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */

			ci.m_soft_kSK_SPLT_CL = blenderobject->bsoft->kSK_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */
			ci.m_soft_kSS_SPLT_CL = blenderobject->bsoft->kSS_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */
			ci.m_soft_kVCF = blenderobject->bsoft->kVCF;			/* Velocities correction factor (Baumgarte) */
			ci.m_soft_kDP = blenderobject->bsoft->kDP;			/* Damping coefficient [0,1] */

			ci.m_soft_kDG = blenderobject->bsoft->kDG;			/* Drag coefficient [0,+inf] */
			ci.m_soft_kLF = blenderobject->bsoft->kLF;			/* Lift coefficient [0,+inf] */
			ci.m_soft_kPR = blenderobject->bsoft->kPR;			/* Pressure coefficient [-inf,+inf] */
			ci.m_soft_kVC = blenderobject->bsoft->kVC;			/* Volume conversation coefficient [0,+inf] */

			ci.m_soft_kDF = blenderobject->bsoft->kDF;			/* Dynamic friction coefficient [0,1] */
			ci.m_soft_kMT = blenderobject->bsoft->kMT;			/* Pose matching coefficient [0,1] */
			ci.m_soft_kCHR = blenderobject->bsoft->kCHR;			/* Rigid contacts hardness [0,1] */
			ci.m_soft_kKHR = blenderobject->bsoft->kKHR;			/* Kinetic contacts hardness [0,1] */

			ci.m_soft_kSHR = blenderobject->bsoft->kSHR;			/* Soft contacts hardness [0,1] */
			ci.m_soft_kAHR = blenderobject->bsoft->kAHR;			/* Anchors hardness [0,1] */
			ci.m_soft_collisionflags = blenderobject->bsoft->collisionflags;	/* Vertex/Face or Signed Distance Field(SDF) or Clusters, Soft versus Soft or Rigid */
			ci.m_soft_numclusteriterations = blenderobject->bsoft->numclusteriterations;	/* number of iterations to refine collision clusters*/

		}
		else
		{
			ci.m_margin = 0.f;
			ci.m_gamesoftFlag = OB_BSB_BENDING_CONSTRAINTS | OB_BSB_SHAPE_MATCHING | OB_BSB_AERO_VPOINT;

			ci.m_soft_linStiff = 0.5;
			ci.m_soft_angStiff = 1.f;	/* angular stiffness 0..1 */
			ci.m_soft_volume = 1.f;	  /* volume preservation 0..1 */

			ci.m_soft_viterations = 0;
			ci.m_soft_piterations = 1;
			ci.m_soft_diterations = 0;
			ci.m_soft_citerations = 4;

			ci.m_soft_kSRHR_CL = 0.1f;
			ci.m_soft_kSKHR_CL = 1.f;
			ci.m_soft_kSSHR_CL = 0.5;
			ci.m_soft_kSR_SPLT_CL = 0.5f;

			ci.m_soft_kSK_SPLT_CL = 0.5f;
			ci.m_soft_kSS_SPLT_CL = 0.5f;
			ci.m_soft_kVCF = 1;
			ci.m_soft_kDP = 0;

			ci.m_soft_kDG = 0;
			ci.m_soft_kLF = 0;
			ci.m_soft_kPR = 0;
			ci.m_soft_kVC = 0;

			ci.m_soft_kDF = 0.2f;
			ci.m_soft_kMT = 0.05f;
			ci.m_soft_kCHR = 1.0f;
			ci.m_soft_kKHR = 0.1f;

			ci.m_soft_kSHR = 1.f;
			ci.m_soft_kAHR = 0.7f;
			ci.m_soft_collisionflags = OB_BSB_COL_SDF_RS + OB_BSB_COL_VF_SS;
			ci.m_soft_numclusteriterations = 16;
		}
	}
	else
	{
		ci.m_margin = blenderobject->margin;
	}

	ci.m_localInertiaTensor = btVector3(ci.m_mass/3.f,ci.m_mass/3.f,ci.m_mass/3.f);

	btCollisionShape* bm = 0;

	char bounds = isbulletdyna ? OB_BOUND_SPHERE : OB_BOUND_TRIANGLE_MESH;
	if (!(blenderobject->gameflag & OB_BOUNDS))
	{
		if (blenderobject->gameflag & OB_SOFT_BODY)
			bounds = OB_BOUND_TRIANGLE_MESH;
		else if (blenderobject->gameflag & OB_CHARACTER)
			bounds = OB_BOUND_SPHERE;
	}
	else
	{
		if (ELEM(blenderobject->collision_boundtype, OB_BOUND_CONVEX_HULL, OB_BOUND_TRIANGLE_MESH)
		    && blenderobject->type != OB_MESH)
		{
			// Can't use triangle mesh or convex hull on a non-mesh object, fall-back to sphere
			bounds = OB_BOUND_SPHERE;
		}
		else
			bounds = blenderobject->collision_boundtype;
	}

	// Get bounds information
	float bounds_center[3], bounds_extends[3];
	BoundBox *bb= BKE_object_boundbox_get(blenderobject);
	if (bb==NULL)
	{
		bounds_center[0] = bounds_center[1] = bounds_center[2] = 0.0;
		bounds_extends[0] = bounds_extends[1] = bounds_extends[2] = 1.0;
	}
	else
	{
		bounds_extends[0] = 0.5f * fabsf(bb->vec[0][0] - bb->vec[4][0]);
		bounds_extends[1] = 0.5f * fabsf(bb->vec[0][1] - bb->vec[2][1]);
		bounds_extends[2] = 0.5f * fabsf(bb->vec[0][2] - bb->vec[1][2]);

		bounds_center[0] = 0.5f * (bb->vec[0][0] + bb->vec[4][0]);
		bounds_center[1] = 0.5f * (bb->vec[0][1] + bb->vec[2][1]);
		bounds_center[2] = 0.5f * (bb->vec[0][2] + bb->vec[1][2]);
	}

	switch (bounds)
	{
	case OB_BOUND_SPHERE:
		{
			//float radius = objprop->m_radius;
			//btVector3 inertiaHalfExtents (
			//	radius,
			//	radius,
			//	radius);

			//blender doesn't support multisphere, but for testing:

			//bm = new MultiSphereShape(inertiaHalfExtents,,&trans.getOrigin(),&radius,1);
			shapeInfo->m_shapeType = PHY_SHAPE_SPHERE;
			// XXX We calculated the radius but didn't use it?
			// objprop.m_boundobject.c.m_radius = MT_max(bb.m_extends[0], MT_max(bb.m_extends[1], bb.m_extends[2]));
			bm = shapeInfo->CreateBulletShape(ci.m_margin);
			break;
		};
	case OB_BOUND_BOX:
		{
			shapeInfo->m_halfExtend.setValue(
					2.f * bounds_extends[0],
			        2.f * bounds_extends[1],
			        2.f * bounds_extends[2]);

			shapeInfo->m_halfExtend /= 2.0;
			shapeInfo->m_halfExtend = shapeInfo->m_halfExtend.absolute();
			shapeInfo->m_shapeType = PHY_SHAPE_BOX;
			bm = shapeInfo->CreateBulletShape(ci.m_margin);
			break;
		};
	case OB_BOUND_CYLINDER:
		{
			float radius = MT_max(bounds_extends[0], bounds_extends[1]);
			shapeInfo->m_halfExtend.setValue(
				radius,
				radius,
				bounds_extends[2]
			);
			shapeInfo->m_shapeType = PHY_SHAPE_CYLINDER;
			bm = shapeInfo->CreateBulletShape(ci.m_margin);
			break;
		}

	case OB_BOUND_CONE:
		{
			shapeInfo->m_radius = MT_max(bounds_extends[0], bounds_extends[1]);
			shapeInfo->m_height = 2.f * bounds_extends[2];
			shapeInfo->m_shapeType = PHY_SHAPE_CONE;
			bm = shapeInfo->CreateBulletShape(ci.m_margin);
			break;
		}
	case OB_BOUND_CONVEX_HULL:
		{
			shapeInfo->SetMesh(meshobj, dm,true);
			bm = shapeInfo->CreateBulletShape(ci.m_margin);
			break;
		}
	case OB_BOUND_CAPSULE:
		{
			shapeInfo->m_radius = MT_max(bounds_extends[0], bounds_extends[1]);
			shapeInfo->m_height = 2.f * (bounds_extends[2] - shapeInfo->m_radius);
			if (shapeInfo->m_height < 0.f)
				shapeInfo->m_height = 0.f;
			shapeInfo->m_shapeType = PHY_SHAPE_CAPSULE;
			bm = shapeInfo->CreateBulletShape(ci.m_margin);
			break;
		}
	case OB_BOUND_TRIANGLE_MESH:
		{
			// mesh shapes can be shared, check first if we already have a shape on that mesh
			class CcdShapeConstructionInfo *sharedShapeInfo = CcdShapeConstructionInfo::FindMesh(meshobj, dm, false);
			if (sharedShapeInfo != NULL)
			{
				shapeInfo->Release();
				shapeInfo = sharedShapeInfo;
				shapeInfo->AddRef();
			} else
			{
				shapeInfo->SetMesh(meshobj, dm, false);
			}

			// Soft bodies can benefit from welding, don't do it on non-soft bodies
			if (isbulletsoftbody)
			{
				// disable welding: it doesn't bring any additional stability and it breaks the relation between soft body collision shape and graphic mesh
				// shapeInfo->setVertexWeldingThreshold1((blenderobject->bsoft) ? blenderobject->bsoft->welding ? 0.f);
				shapeInfo->setVertexWeldingThreshold1(0.f); //todo: expose this to the UI
			}

			bm = shapeInfo->CreateBulletShape(ci.m_margin, useGimpact, !isbulletsoftbody);
			//should we compute inertia for dynamic shape?
			//bm->calculateLocalInertia(ci.m_mass,ci.m_localInertiaTensor);

			break;
		}
	}


//	ci.m_localInertiaTensor.setValue(0.1f,0.1f,0.1f);

	if (!bm)
	{
		delete motionstate;
		shapeInfo->Release();
		return;
	}

	//bm->setMargin(ci.m_margin);


		if (isCompoundChild)
		{
			//find parent, compound shape and add to it
			//take relative transform into account!
			CcdPhysicsController* parentCtrl = (CcdPhysicsController*)parent->GetPhysicsController();
			assert(parentCtrl);
			CcdShapeConstructionInfo* parentShapeInfo = parentCtrl->GetShapeInfo();
			btRigidBody* rigidbody = parentCtrl->GetRigidBody();
			btCollisionShape* colShape = rigidbody->getCollisionShape();
			assert(colShape->isCompound());
			btCompoundShape* compoundShape = (btCompoundShape*)colShape;

			// compute the local transform from parent, this may include several node in the chain
			SG_Node* gameNode = gameobj->GetSGNode();
			SG_Node* parentNode = parent->GetSGNode();
			// relative transform
			MT_Vector3 parentScale = parentNode->GetWorldScaling();
			parentScale[0] = MT_Scalar(1.0)/parentScale[0];
			parentScale[1] = MT_Scalar(1.0)/parentScale[1];
			parentScale[2] = MT_Scalar(1.0)/parentScale[2];
			MT_Vector3 relativeScale = gameNode->GetWorldScaling() * parentScale;
			MT_Matrix3x3 parentInvRot = parentNode->GetWorldOrientation().transposed();
			MT_Vector3 relativePos = parentInvRot*((gameNode->GetWorldPosition()-parentNode->GetWorldPosition())*parentScale);
			MT_Matrix3x3 relativeRot = parentInvRot*gameNode->GetWorldOrientation();

			shapeInfo->m_childScale.setValue(relativeScale[0],relativeScale[1],relativeScale[2]);
			bm->setLocalScaling(shapeInfo->m_childScale);
			shapeInfo->m_childTrans.getOrigin().setValue(relativePos[0],relativePos[1],relativePos[2]);
			float rot[12];
			relativeRot.getValue(rot);
			shapeInfo->m_childTrans.getBasis().setFromOpenGLSubMatrix(rot);

			parentShapeInfo->AddShape(shapeInfo);
			compoundShape->addChildShape(shapeInfo->m_childTrans,bm);
			//do some recalc?
			//recalc inertia for rigidbody
			if (!rigidbody->isStaticOrKinematicObject())
			{
				btVector3 localInertia;
				float mass = 1.f/rigidbody->getInvMass();
				compoundShape->calculateLocalInertia(mass,localInertia);
				rigidbody->setMassProps(mass,localInertia);
			}
			shapeInfo->Release();
			// delete motionstate as it's not used
			delete motionstate;
			return;
		}

		if (hasCompoundChildren)
		{
			// create a compound shape info
			CcdShapeConstructionInfo *compoundShapeInfo = new CcdShapeConstructionInfo();
			compoundShapeInfo->m_shapeType = PHY_SHAPE_COMPOUND;
			compoundShapeInfo->AddShape(shapeInfo);
			// create the compound shape manually as we already have the child shape
			btCompoundShape* compoundShape = new btCompoundShape();
			compoundShape->addChildShape(shapeInfo->m_childTrans,bm);
			// now replace the shape
			bm = compoundShape;
			shapeInfo->Release();
			shapeInfo = compoundShapeInfo;
		}






#ifdef TEST_SIMD_HULL
	if (bm->IsPolyhedral())
	{
		PolyhedralConvexShape* polyhedron = static_cast<PolyhedralConvexShape*>(bm);
		if (!polyhedron->m_optionalHull)
		{
			//first convert vertices in 'Point3' format
			int numPoints = polyhedron->GetNumVertices();
			Point3* points = new Point3[numPoints+1];
			//first 4 points should not be co-planar, so add central point to satisfy MakeHull
			points[0] = Point3(0.f,0.f,0.f);

			btVector3 vertex;
			for (int p=0;p<numPoints;p++)
			{
				polyhedron->GetVertex(p,vertex);
				points[p+1] = Point3(vertex.getX(),vertex.getY(),vertex.getZ());
			}

			Hull* hull = Hull::MakeHull(numPoints+1,points);
			polyhedron->m_optionalHull = hull;
		}

	}
#endif //TEST_SIMD_HULL


	ci.m_collisionShape = bm;
	ci.m_shapeInfo = shapeInfo;
	ci.m_friction = smmaterial->m_friction;//tweak the friction a bit, so the default 0.5 works nice
	ci.m_restitution = smmaterial->m_restitution;
	ci.m_physicsEnv = this;
	// drag / damping is inverted
	ci.m_linearDamping = 1.f - shapeprops->m_lin_drag;
	ci.m_angularDamping = 1.f - shapeprops->m_ang_drag;
	//need a bit of damping, else system doesn't behave well
	ci.m_inertiaFactor = shapeprops->m_inertia/0.4f;//defaults to 0.4, don't want to change behavior

	ci.m_do_anisotropic = shapeprops->m_do_anisotropic;
	ci.m_anisotropicFriction.setValue(shapeprops->m_friction_scaling[0],shapeprops->m_friction_scaling[1],shapeprops->m_friction_scaling[2]);


//////////
	//do Fh, do Rot Fh
	ci.m_do_fh = shapeprops->m_do_fh;
	ci.m_do_rot_fh = shapeprops->m_do_rot_fh;
	ci.m_fh_damping = smmaterial->m_fh_damping;
	ci.m_fh_distance = smmaterial->m_fh_distance;
	ci.m_fh_normal = smmaterial->m_fh_normal;
	ci.m_fh_spring = smmaterial->m_fh_spring;

	ci.m_collisionFilterGroup =
		(isbulletsensor) ? short(CcdConstructionInfo::SensorFilter) :
		(isbulletdyna) ? short(CcdConstructionInfo::DefaultFilter) :
		(isbulletchar) ? short(CcdConstructionInfo::CharacterFilter) :
		short(CcdConstructionInfo::StaticFilter);
	ci.m_collisionFilterMask =
		(isbulletsensor) ? short(CcdConstructionInfo::AllFilter ^ CcdConstructionInfo::SensorFilter) :
		(isbulletdyna) ? short(CcdConstructionInfo::AllFilter) :
		(isbulletchar) ? short(CcdConstructionInfo::AllFilter) :
		short(CcdConstructionInfo::AllFilter ^ CcdConstructionInfo::StaticFilter);
	ci.m_bRigid = isbulletdyna && isbulletrigidbody;
	ci.m_bSoft = isbulletsoftbody;
	ci.m_bDyna = isbulletdyna;
	ci.m_bSensor = isbulletsensor;
	ci.m_bCharacter = isbulletchar;
	ci.m_bGimpact = useGimpact;
	MT_Vector3 scaling = gameobj->NodeGetWorldScaling();
	ci.m_scaling.setValue(scaling[0], scaling[1], scaling[2]);
	CcdPhysicsController* physicscontroller = new CcdPhysicsController(ci);
	// shapeInfo is reference counted, decrement now as we don't use it anymore
	if (shapeInfo)
		shapeInfo->Release();

	gameobj->SetPhysicsController(physicscontroller,isbulletdyna);

	// record animation for dynamic objects
	if (isbulletdyna)
		gameobj->SetRecordAnimation(true);

	// don't add automatically sensor object, they are added when a collision sensor is registered
	if (!isbulletsensor && (blenderobject->lay & activeLayerBitInfo) != 0)
	{
		this->AddCcdPhysicsController( physicscontroller);
	}
	physicscontroller->SetNewClientInfo(gameobj->getClientInfo());
	{
		btRigidBody* rbody = physicscontroller->GetRigidBody();

		if (rbody)
		{
			if (isbulletrigidbody)
			{
				rbody->setLinearFactor(ci.m_linearFactor);
				rbody->setAngularFactor(ci.m_angularFactor);
			}

			if (rbody && (blenderobject->gameflag & OB_COLLISION_RESPONSE) != 0)
			{
				rbody->setActivationState(DISABLE_DEACTIVATION);
			}
		}
	}

	CcdPhysicsController* parentCtrl = parent ? (CcdPhysicsController*)parent->GetPhysicsController() : 0;
	physicscontroller->SetParentCtrl(parentCtrl);


	//Now done directly in ci.m_collisionFlags so that it propagates to replica
	//if (objprop->m_ghost)
	//{
	//	rbody->setCollisionFlags(rbody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
	//}

	if (isbulletdyna && !isbulletrigidbody)
	{
#if 0
		//setting the inertia could achieve similar results to constraint the up
		//but it is prone to instability, so use special 'Angular' constraint
		btVector3 inertia = physicscontroller->GetRigidBody()->getInvInertiaDiagLocal();
		inertia.setX(0.f);
		inertia.setZ(0.f);

		physicscontroller->GetRigidBody()->setInvInertiaDiagLocal(inertia);
		physicscontroller->GetRigidBody()->updateInertiaTensor();
#endif

		//this->createConstraint(physicscontroller,0,PHY_ANGULAR_CONSTRAINT,0,0,0,0,0,1);

		//Now done directly in ci.m_bRigid so that it propagates to replica
		//physicscontroller->GetRigidBody()->setAngularFactor(0.f);
		;
	}


	STR_String materialname;
	if (meshobj)
		materialname = meshobj->GetMaterialName(0);


#if 0
	///test for soft bodies
	if (objprop->m_softbody && physicscontroller)
	{
		btSoftBody* softBody = physicscontroller->GetSoftBody();
		if (softBody && gameobj->GetMesh(0))//only the first mesh, if any
		{
			//should be a mesh then, so add a soft body deformer
			KX_SoftBodyDeformer* softbodyDeformer = new KX_SoftBodyDeformer( gameobj->GetMesh(0),(BL_DeformableGameObject*)gameobj);
			gameobj->SetDeformer(softbodyDeformer);
		}
	}
#endif
}
