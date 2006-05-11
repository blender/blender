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
#include "SimdTransform.h"
#include "Dynamics/RigidBody.h"
#include "BroadphaseCollision/BroadphaseInterface.h"
#include "BroadphaseCollision/SimpleBroadphase.h"
#include "BroadphaseCollision/AxisSweep3.h"

#include "CollisionDispatch/CollisionWorld.h"

#include "CollisionShapes/ConvexShape.h"
#include "BroadphaseCollision/Dispatcher.h"
#include "NarrowPhaseCollision/PersistentManifold.h"
#include "CollisionShapes/TriangleMeshShape.h"
#include "ConstraintSolver/OdeConstraintSolver.h"
#include "ConstraintSolver/SimpleConstraintSolver.h"


//profiling/timings
#include "quickprof.h"


#include "IDebugDraw.h"

#include "NarrowPhaseCollision/VoronoiSimplexSolver.h"
#include "NarrowPhaseCollision/SubSimplexConvexCast.h"
#include "NarrowPhaseCollision/GjkConvexCast.h"
#include "NarrowPhaseCollision/ContinuousConvexCollision.h"


#include "CollisionDispatch/CollisionDispatcher.h"
#include "PHY_IMotionState.h"

#include "CollisionDispatch/EmptyCollisionAlgorithm.h"
#include "CollisionDispatch/UnionFind.h"


#include "CollisionShapes/SphereShape.h"

bool useIslands = true;

#ifdef NEW_BULLET_VEHICLE_SUPPORT
#include "Vehicle/RaycastVehicle.h"
#include "Vehicle/VehicleRaycaster.h"

#include "Vehicle/WheelInfo.h"
#include "PHY_IVehicle.h"
RaycastVehicle::VehicleTuning	gTuning;

#endif //NEW_BULLET_VEHICLE_SUPPORT
#include "AabbUtil2.h"

#include "ConstraintSolver/ConstraintSolver.h"
#include "ConstraintSolver/Point2PointConstraint.h"
#include "ConstraintSolver/HingeConstraint.h"


//#include "BroadphaseCollision/QueryDispatcher.h"
//#include "BroadphaseCollision/QueryBox.h"
//todo: change this to allow dynamic registration of types!

#ifdef WIN32
void DrawRasterizerLine(const float* from,const float* to,int color);
#endif


#include "ConstraintSolver/ContactConstraint.h"


#include <stdio.h>

#ifdef NEW_BULLET_VEHICLE_SUPPORT
class WrapperVehicle : public PHY_IVehicle
{

	RaycastVehicle*	m_vehicle;
	PHY_IPhysicsController*	m_chassis;

public:

	WrapperVehicle(RaycastVehicle* vehicle,PHY_IPhysicsController* chassis)
		:m_vehicle(vehicle),
		m_chassis(chassis)
	{
	}

	RaycastVehicle*	GetVehicle()
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
		SimdVector3 connectionPointCS0(connectionPoint[0],connectionPoint[1],connectionPoint[2]);
		SimdVector3 wheelDirectionCS0(downDirection[0],downDirection[1],downDirection[2]);
		SimdVector3 wheelAxle(axleDirection[0],axleDirection[1],axleDirection[2]);


		WheelInfo& info = m_vehicle->AddWheel(connectionPointCS0,wheelDirectionCS0,wheelAxle,
			suspensionRestLength,wheelRadius,gTuning,hasSteering);
		info.m_clientInfo = motionState;

	}

	void	SyncWheels()
	{
		int numWheels = GetNumWheels();
		int i;
		for (i=0;i<numWheels;i++)
		{
			WheelInfo& info = m_vehicle->GetWheelInfo(i);
			PHY_IMotionState* motionState = (PHY_IMotionState*)info.m_clientInfo ;
			m_vehicle->UpdateWheelTransform(i);
			SimdTransform trans = m_vehicle->GetWheelTransformWS(i);
			SimdQuaternion orn = trans.getRotation();
			const SimdVector3& pos = trans.getOrigin();
			motionState->setWorldOrientation(orn.x(),orn.y(),orn.z(),orn[3]);
			motionState->setWorldPosition(pos.x(),pos.y(),pos.z());

		}
	}

	virtual	int		GetNumWheels() const
	{
		return m_vehicle->GetNumWheels();
	}

	virtual void	GetWheelPosition(int wheelIndex,float& posX,float& posY,float& posZ) const
	{
		SimdTransform	trans = m_vehicle->GetWheelTransformWS(wheelIndex);
		posX = trans.getOrigin().x();
		posY = trans.getOrigin().y();
		posZ = trans.getOrigin().z();
	}
	virtual void	GetWheelOrientationQuaternion(int wheelIndex,float& quatX,float& quatY,float& quatZ,float& quatW) const
	{
		SimdTransform	trans = m_vehicle->GetWheelTransformWS(wheelIndex);
		SimdQuaternion quat = trans.getRotation();
		SimdMatrix3x3 orn2(quat);

		quatX = trans.getRotation().x();
		quatY = trans.getRotation().y();
		quatZ = trans.getRotation().z();
		quatW = trans.getRotation()[3];


		//printf("test");


	}

	virtual float	GetWheelRotation(int wheelIndex) const
	{
		float rotation = 0.f;

		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->GetNumWheels()))
		{
			WheelInfo& info = m_vehicle->GetWheelInfo(wheelIndex);
			rotation = info.m_rotation;
		}
		return rotation;

	}



	virtual int	GetUserConstraintId() const
	{
		return m_vehicle->GetUserConstraintId();
	}

	virtual int	GetUserConstraintType() const
	{
		return m_vehicle->GetUserConstraintType();
	}

	virtual	void	SetSteeringValue(float steering,int wheelIndex)
	{
		m_vehicle->SetSteeringValue(steering,wheelIndex);
	}

	virtual	void	ApplyEngineForce(float force,int wheelIndex)
	{
		m_vehicle->ApplyEngineForce(force,wheelIndex);
	}

	virtual	void	ApplyBraking(float braking,int wheelIndex)
	{
		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->GetNumWheels()))
		{
			WheelInfo& info = m_vehicle->GetWheelInfo(wheelIndex);
			info.m_brake = braking;
		}
	}

	virtual	void	SetWheelFriction(float friction,int wheelIndex)
	{
		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->GetNumWheels()))
		{
			WheelInfo& info = m_vehicle->GetWheelInfo(wheelIndex);
			info.m_frictionSlip = friction;
		}

	}

	virtual	void	SetSuspensionStiffness(float suspensionStiffness,int wheelIndex)
	{
		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->GetNumWheels()))
		{
			WheelInfo& info = m_vehicle->GetWheelInfo(wheelIndex);
			info.m_suspensionStiffness = suspensionStiffness;

		}
	}

	virtual	void	SetSuspensionDamping(float suspensionDamping,int wheelIndex)
	{
		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->GetNumWheels()))
		{
			WheelInfo& info = m_vehicle->GetWheelInfo(wheelIndex);
			info.m_wheelsDampingRelaxation = suspensionDamping;
		}
	}

	virtual	void	SetSuspensionCompression(float suspensionCompression,int wheelIndex)
	{
		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->GetNumWheels()))
		{
			WheelInfo& info = m_vehicle->GetWheelInfo(wheelIndex);
			info.m_wheelsDampingCompression = suspensionCompression;
		}
	}



	virtual	void	SetRollInfluence(float rollInfluence,int wheelIndex)
	{
		if ((wheelIndex>=0) && (wheelIndex< m_vehicle->GetNumWheels()))
		{
			WheelInfo& info = m_vehicle->GetWheelInfo(wheelIndex);
			info.m_rollInfluence = rollInfluence;
		}
	}



};
#endif //NEW_BULLET_VEHICLE_SUPPORT



static void DrawAabb(IDebugDraw* debugDrawer,const SimdVector3& from,const SimdVector3& to,const SimdVector3& color)
{
	SimdVector3 halfExtents = (to-from)* 0.5f;
	SimdVector3 center = (to+from) *0.5f;
	int i,j;

	SimdVector3 edgecoord(1.f,1.f,1.f),pa,pb;
	for (i=0;i<4;i++)
	{
		for (j=0;j<3;j++)
		{
			pa = SimdVector3(edgecoord[0]*halfExtents[0], edgecoord[1]*halfExtents[1],		
				edgecoord[2]*halfExtents[2]);
			pa+=center;

			int othercoord = j%3;
			edgecoord[othercoord]*=-1.f;
			pb = SimdVector3(edgecoord[0]*halfExtents[0], edgecoord[1]*halfExtents[1],	
				edgecoord[2]*halfExtents[2]);
			pb+=center;

			debugDrawer->DrawLine(pa,pb,color);
		}
		edgecoord = SimdVector3(-1.f,-1.f,-1.f);
		if (i<3)
			edgecoord[i]*=-1.f;
	}


}






CcdPhysicsEnvironment::CcdPhysicsEnvironment(CollisionDispatcher* dispatcher,BroadphaseInterface* broadphase)
:m_scalingPropagated(false),
m_numIterations(10),
m_ccdMode(0),
m_solverType(-1),
m_profileTimings(0),
m_enableSatCollisionDetection(false)
{

	for (int i=0;i<PHY_NUM_RESPONSE;i++)
	{
		m_triggerCallbacks[i] = 0;
	}
	if (!dispatcher)
		dispatcher = new CollisionDispatcher();


	if(!broadphase)
	{

		//todo: calculate/let user specify this world sizes
		SimdVector3 worldMin(-10000,-10000,-10000);
		SimdVector3 worldMax(10000,10000,10000);

		//broadphase = new AxisSweep3(worldMin,worldMax);

		broadphase = new SimpleBroadphase();
	}


	setSolverType(1);//issues with quickstep and memory allocations

	m_collisionWorld = new CollisionWorld(dispatcher,broadphase);

	m_debugDrawer = 0;
	m_gravity = SimdVector3(0.f,-10.f,0.f);


}

void	CcdPhysicsEnvironment::addCcdPhysicsController(CcdPhysicsController* ctrl)
{
	RigidBody* body = ctrl->GetRigidBody();

	//this m_userPointer is just used for triggers, see CallbackTriggers
	body->m_userPointer = ctrl;

	body->setGravity( m_gravity );
	m_controllers.push_back(ctrl);

	m_collisionWorld->AddCollisionObject(body);

	assert(body->m_broadphaseHandle);

	BroadphaseInterface* scene =  GetBroadphase();


	CollisionShape* shapeinterface = ctrl->GetCollisionShape();

	assert(shapeinterface);

	const SimdTransform& t = ctrl->GetRigidBody()->getCenterOfMassTransform();


	SimdPoint3 minAabb,maxAabb;

	shapeinterface->GetAabb(t,minAabb,maxAabb);

	float timeStep = 0.02f;


	//extent it with the motion

	SimdVector3 linMotion = body->getLinearVelocity()*timeStep;

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


	minAabb = SimdVector3(minAabbx,minAabby,minAabbz);
	maxAabb = SimdVector3(maxAabbx,maxAabby,maxAabbz);




}

void	CcdPhysicsEnvironment::removeCcdPhysicsController(CcdPhysicsController* ctrl)
{

	//also remove constraint

	{
		std::vector<TypedConstraint*>::iterator i;

		for (i=m_constraints.begin();
			!(i==m_constraints.end()); i++)
		{
			TypedConstraint* constraint = (*i);
			if  ((&constraint->GetRigidBodyA() == ctrl->GetRigidBody() ||
				(&constraint->GetRigidBodyB() == ctrl->GetRigidBody())))
			{
				removeConstraint(constraint->GetUserConstraintId());
				//only 1 constraint per constroller
				break;
			}
		}
	}

	{
		std::vector<TypedConstraint*>::iterator i;

		for (i=m_constraints.begin();
			!(i==m_constraints.end()); i++)
		{
			TypedConstraint* constraint = (*i);
			if  ((&constraint->GetRigidBodyA() == ctrl->GetRigidBody() ||
				(&constraint->GetRigidBodyB() == ctrl->GetRigidBody())))
			{
				removeConstraint(constraint->GetUserConstraintId());
				//only 1 constraint per constroller
				break;
			}
		}
	}


	m_collisionWorld->RemoveCollisionObject(ctrl->GetRigidBody());


	{
		std::vector<CcdPhysicsController*>::iterator i =
			std::find(m_controllers.begin(), m_controllers.end(), ctrl);
		if (!(i == m_controllers.end()))
		{
			std::swap(*i, m_controllers.back());
			m_controllers.pop_back();
		}
	}

	//remove it from the triggers
	{
		std::vector<CcdPhysicsController*>::iterator i =
			std::find(m_triggerControllers.begin(), m_triggerControllers.end(), ctrl);
		if (!(i == m_triggerControllers.end()))
		{
			std::swap(*i, m_triggerControllers.back());
			m_triggerControllers.pop_back();
		}
	}


}


void	CcdPhysicsEnvironment::beginFrame()
{

}


bool	CcdPhysicsEnvironment::proceedDeltaTime(double curTime,float timeStep)
{

#ifdef USE_QUICKPROF
	//toggle Profiler
	if ( m_debugDrawer->GetDebugMode() & IDebugDraw::DBG_ProfileTimings)
	{
		if (!m_profileTimings)
		{
			m_profileTimings = 1;
			// To disable profiling, simply comment out the following line.
			static int counter = 0;

			char filename[128];
			sprintf(filename,"quickprof_bullet_timings%i.csv",counter++);
			Profiler::init(filename, Profiler::BLOCK_CYCLE_SECONDS);//BLOCK_TOTAL_MICROSECONDS

		}
	} else
	{
		if (m_profileTimings)
		{
			m_profileTimings = 0;
			Profiler::destroy();
		}
	}
#endif //USE_QUICKPROF



	if (!SimdFuzzyZero(timeStep))
	{

		// define this in blender, the stepsize is 30 hertz, 60 hertz works much better 
//#define SPLIT_TIMESTEP 1

#ifdef SPLIT_TIMESTEP
		proceedDeltaTimeOneStep(0.5f*timeStep);
		proceedDeltaTimeOneStep(0.5f*timeStep);
#else		
		proceedDeltaTimeOneStep(timeStep);
#endif
	} else
	{
		//todo: interpolate
	}

	return true;
}













/// Perform an integration step of duration 'timeStep'.
bool	CcdPhysicsEnvironment::proceedDeltaTimeOneStep(float timeStep)
{


	//	printf("CcdPhysicsEnvironment::proceedDeltaTime\n");

	if (SimdFuzzyZero(timeStep))
		return true;

	if (m_debugDrawer)
	{
		gDisableDeactivation = (m_debugDrawer->GetDebugMode() & IDebugDraw::DBG_NoDeactivation);
	}


#ifdef USE_QUICKPROF
	Profiler::beginBlock("SyncMotionStates");
#endif //USE_QUICKPROF


	//this is needed because scaling is not known in advance, and scaling has to propagate to the shape
	if (!m_scalingPropagated)
	{
		SyncMotionStates(timeStep);
		m_scalingPropagated = true;
	}


#ifdef USE_QUICKPROF
	Profiler::endBlock("SyncMotionStates");

	Profiler::beginBlock("predictIntegratedTransform");
#endif //USE_QUICKPROF

	{
		//		std::vector<CcdPhysicsController*>::iterator i;



		int k;
		for (k=0;k<GetNumControllers();k++)
		{
			CcdPhysicsController* ctrl = m_controllers[k];
			//		SimdTransform predictedTrans;
			RigidBody* body = ctrl->GetRigidBody();
			if (body->GetActivationState() != ISLAND_SLEEPING)
			{
				if (!body->IsStatic())
				{
					body->applyForces( timeStep);
					body->integrateVelocities( timeStep);
					body->predictIntegratedTransform(timeStep,body->m_interpolationWorldTransform);
				}
			}

		}
	}

#ifdef USE_QUICKPROF
	Profiler::endBlock("predictIntegratedTransform");
#endif //USE_QUICKPROF

	BroadphaseInterface*	scene = GetBroadphase();


	//
	// collision detection (?)
	//


#ifdef USE_QUICKPROF
	Profiler::beginBlock("DispatchAllCollisionPairs");
#endif //USE_QUICKPROF


	int numsubstep = m_numIterations;


	DispatcherInfo dispatchInfo;
	dispatchInfo.m_timeStep = timeStep;
	dispatchInfo.m_stepCount = 0;
	dispatchInfo.m_enableSatConvex = m_enableSatCollisionDetection;

	scene->DispatchAllCollisionPairs(*GetDispatcher(),dispatchInfo);///numsubstep,g);


#ifdef USE_QUICKPROF
	Profiler::endBlock("DispatchAllCollisionPairs");
#endif //USE_QUICKPROF


	int numRigidBodies = m_controllers.size();

	m_collisionWorld->UpdateActivationState();



	//contacts
#ifdef USE_QUICKPROF
	Profiler::beginBlock("SolveConstraint");
#endif //USE_QUICKPROF


	//solve the regular constraints (point 2 point, hinge, etc)

	for (int g=0;g<numsubstep;g++)
	{
		//
		// constraint solving
		//


		int i;
		int numConstraints = m_constraints.size();

		//point to point constraints
		for (i=0;i< numConstraints ; i++ )
		{
			TypedConstraint* constraint = m_constraints[i];

			constraint->BuildJacobian();
			constraint->SolveConstraint( timeStep );

		}


	}

#ifdef USE_QUICKPROF
	Profiler::endBlock("SolveConstraint");
#endif //USE_QUICKPROF

	//solve the vehicles

#ifdef NEW_BULLET_VEHICLE_SUPPORT
	//vehicles
	int numVehicles = m_wrapperVehicles.size();
	for (int i=0;i<numVehicles;i++)
	{
		WrapperVehicle* wrapperVehicle = m_wrapperVehicles[i];
		RaycastVehicle* vehicle = wrapperVehicle->GetVehicle();
		vehicle->UpdateVehicle( timeStep);
	}
#endif //NEW_BULLET_VEHICLE_SUPPORT


	struct InplaceSolverIslandCallback : public CollisionDispatcher::IslandCallback
	{

		ContactSolverInfo& m_solverInfo;
		ConstraintSolver*	m_solver;
		IDebugDraw*	m_debugDrawer;

		InplaceSolverIslandCallback(
			ContactSolverInfo& solverInfo,
			ConstraintSolver*	solver,
			IDebugDraw*	debugDrawer)
			:m_solverInfo(solverInfo),
			m_solver(solver),
			m_debugDrawer(debugDrawer)
		{

		}

		virtual	void	ProcessIsland(PersistentManifold**	manifolds,int numManifolds)
		{
			m_solver->SolveGroup( manifolds, numManifolds,m_solverInfo,m_debugDrawer);
		}

	};


	m_solverInfo.m_friction = 0.9f;
	m_solverInfo.m_numIterations = m_numIterations;
	m_solverInfo.m_timeStep = timeStep;
	m_solverInfo.m_restitution = 0.f;//m_restitution;

	InplaceSolverIslandCallback	solverCallback(
		m_solverInfo,
		m_solver,
		m_debugDrawer);

#ifdef USE_QUICKPROF
	Profiler::beginBlock("BuildAndProcessIslands");
#endif //USE_QUICKPROF

	/// solve all the contact points and contact friction
	GetDispatcher()->BuildAndProcessIslands(numRigidBodies,&solverCallback);

#ifdef USE_QUICKPROF
	Profiler::endBlock("BuildAndProcessIslands");

	Profiler::beginBlock("CallbackTriggers");
#endif //USE_QUICKPROF

	CallbackTriggers();

#ifdef USE_QUICKPROF
	Profiler::endBlock("CallbackTriggers");


	Profiler::beginBlock("proceedToTransform");

#endif //USE_QUICKPROF
	{



		{

			
			
			UpdateAabbs(timeStep);


			float toi = 1.f;



			if (m_ccdMode == 3)
			{
				DispatcherInfo dispatchInfo;
				dispatchInfo.m_timeStep = timeStep;
				dispatchInfo.m_stepCount = 0;
				dispatchInfo.m_dispatchFunc = DispatcherInfo::DISPATCH_CONTINUOUS;

				scene->DispatchAllCollisionPairs( *GetDispatcher(),dispatchInfo);///numsubstep,g);
				toi = dispatchInfo.m_timeOfImpact;

			}

			

			//
			// integrating solution
			//

			{
				
				std::vector<CcdPhysicsController*>::iterator i;

				for (i=m_controllers.begin();
					!(i==m_controllers.end()); i++)
				{

					CcdPhysicsController* ctrl = *i;

					SimdTransform predictedTrans;
					RigidBody* body = ctrl->GetRigidBody();
					if (body->GetActivationState() != ISLAND_SLEEPING)
					{

						if (body->IsStatic())
						{
							//to calculate velocities next frame
							body->saveKinematicState(timeStep);
						} else
						{
							body->predictIntegratedTransform(timeStep*	toi, predictedTrans);
							body->proceedToTransform( predictedTrans);
						}

					}
				}

			}





			//
			// disable sleeping physics objects
			//

			std::vector<CcdPhysicsController*> m_sleepingControllers;

			std::vector<CcdPhysicsController*>::iterator i;

			for (i=m_controllers.begin();
				!(i==m_controllers.end()); i++)
			{
				CcdPhysicsController* ctrl = (*i);
				RigidBody* body = ctrl->GetRigidBody();

				ctrl->UpdateDeactivation(timeStep);


				if (ctrl->wantsSleeping())
				{
					if (body->GetActivationState() == ACTIVE_TAG)
						body->SetActivationState( WANTS_DEACTIVATION );
				} else
				{
					if (body->GetActivationState() != DISABLE_DEACTIVATION)
						body->SetActivationState( ACTIVE_TAG );
				}

				if (useIslands)
				{
					if (body->GetActivationState() == ISLAND_SLEEPING)
					{
						m_sleepingControllers.push_back(ctrl);
					}
				} else
				{
					if (ctrl->wantsSleeping())
					{
						m_sleepingControllers.push_back(ctrl);
					}
				}
			}




		}


#ifdef USE_QUICKPROF
		Profiler::endBlock("proceedToTransform");

		Profiler::beginBlock("SyncMotionStates");
#endif //USE_QUICKPROF

		SyncMotionStates(timeStep);

#ifdef USE_QUICKPROF
		Profiler::endBlock("SyncMotionStates");

		Profiler::endProfilingCycle();
#endif //USE_QUICKPROF


#ifdef NEW_BULLET_VEHICLE_SUPPORT
		//sync wheels for vehicles
		int numVehicles = m_wrapperVehicles.size();
		for (int i=0;i<numVehicles;i++)
		{
			WrapperVehicle* wrapperVehicle = m_wrapperVehicles[i];

			wrapperVehicle->SyncWheels();
		}
#endif //NEW_BULLET_VEHICLE_SUPPORT
	}

	return true;
}

void		CcdPhysicsEnvironment::setDebugMode(int debugMode)
{
	if (m_debugDrawer){
		m_debugDrawer->SetDebugMode(debugMode);
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
	gContactBreakingTreshold = contactBreakingTreshold;

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
	gUseEpa = epa;
}

void		CcdPhysicsEnvironment::setSolverType(int solverType)
{

	switch (solverType)
	{
	case 1:
		{
			if (m_solverType != solverType)
			{

				m_solver = new SimpleConstraintSolver();

				break;
			}
		}

	case 0:
	default:
		if (m_solverType != solverType)
		{
			m_solver = new OdeConstraintSolver();

			break;
		}

	};

	m_solverType = solverType ;
}





void	CcdPhysicsEnvironment::SyncMotionStates(float timeStep)
{
	std::vector<CcdPhysicsController*>::iterator i;

	//
	// synchronize the physics and graphics transformations
	//

	for (i=m_controllers.begin();
		!(i==m_controllers.end()); i++)
	{
		CcdPhysicsController* ctrl = (*i);
		ctrl->SynchronizeMotionStates(timeStep);

	}

}
void		CcdPhysicsEnvironment::setGravity(float x,float y,float z)
{
	m_gravity = SimdVector3(x,y,z);

	std::vector<CcdPhysicsController*>::iterator i;

	//todo: review this gravity stuff
	for (i=m_controllers.begin();
		!(i==m_controllers.end()); i++)
	{

		CcdPhysicsController* ctrl = (*i);
		ctrl->GetRigidBody()->setGravity(m_gravity);

	}
}


#ifdef NEW_BULLET_VEHICLE_SUPPORT

class DefaultVehicleRaycaster : public VehicleRaycaster
{
	CcdPhysicsEnvironment* m_physEnv;
	PHY_IPhysicsController*	m_chassis;

public:
	DefaultVehicleRaycaster(CcdPhysicsEnvironment* physEnv,PHY_IPhysicsController* chassis):
	  m_physEnv(physEnv),
		  m_chassis(chassis)
	  {
	  }

	  /*	struct VehicleRaycasterResult
	  {
	  VehicleRaycasterResult() :m_distFraction(-1.f){};
	  SimdVector3	m_hitPointInWorld;
	  SimdVector3	m_hitNormalInWorld;
	  SimdScalar	m_distFraction;
	  };
	  */
	  virtual void* CastRay(const SimdVector3& from,const SimdVector3& to, VehicleRaycasterResult& result)
	  {


		  float hit[3];
		  float normal[3];

		  PHY_IPhysicsController*	ignore = m_chassis;
		  void* hitObject = m_physEnv->rayTest(ignore,from.x(),from.y(),from.z(),to.x(),to.y(),to.z(),hit[0],hit[1],hit[2],normal[0],normal[1],normal[2]);
		  if (hitObject)
		  {
			  result.m_hitPointInWorld[0] = hit[0];
			  result.m_hitPointInWorld[1] = hit[1];
			  result.m_hitPointInWorld[2] = hit[2];
			  result.m_hitNormalInWorld[0] = normal[0];
			  result.m_hitNormalInWorld[1] = normal[1];
			  result.m_hitNormalInWorld[2] = normal[2];
			  result.m_hitNormalInWorld.normalize();
			  //calc fraction? or put it in the interface?
			  //calc for now

			  result.m_distFraction = (result.m_hitPointInWorld-from).length() / (to-from).length();
			  //some safety for 'explosion' due to sudden penetration of the full 'ray'
			  /*			if (result.m_distFraction<0.1)
			  {
			  printf("Vehicle Raycast: avoided instability due to penetration. Consider moving the connection points deeper inside vehicle chassis");
			  result.m_distFraction = 1.f;
			  hitObject = 0;
			  }
			  */

			  /*			if (result.m_distFraction>1.)
			  {
			  printf("Vehicle Raycast: avoided instability 1Consider moving the connection points deeper inside vehicle chassis");				
			  result.m_distFraction = 1.f;
			  hitObject = 0;
			  }
			  */



		  }
		  //?
		  return hitObject;
	  }
};
#endif //NEW_BULLET_VEHICLE_SUPPORT

static int gConstraintUid = 1;

int			CcdPhysicsEnvironment::createConstraint(class PHY_IPhysicsController* ctrl0,class PHY_IPhysicsController* ctrl1,PHY_ConstraintType type,
													float pivotX,float pivotY,float pivotZ,
													float axisX,float axisY,float axisZ)
{


	CcdPhysicsController* c0 = (CcdPhysicsController*)ctrl0;
	CcdPhysicsController* c1 = (CcdPhysicsController*)ctrl1;

	RigidBody* rb0 = c0 ? c0->GetRigidBody() : 0;
	RigidBody* rb1 = c1 ? c1->GetRigidBody() : 0;

	ASSERT(rb0);

	SimdVector3 pivotInA(pivotX,pivotY,pivotZ);
	SimdVector3 pivotInB = rb1 ? rb1->getCenterOfMassTransform().inverse()(rb0->getCenterOfMassTransform()(pivotInA)) : pivotInA;
	SimdVector3 axisInA(axisX,axisY,axisZ);
	SimdVector3 axisInB = rb1 ? 
		(rb1->getCenterOfMassTransform().getBasis().inverse()*(rb0->getCenterOfMassTransform().getBasis() * -axisInA)) : 
	rb0->getCenterOfMassTransform().getBasis() * -axisInA;

	bool angularOnly = false;

	switch (type)
	{
	case PHY_POINT2POINT_CONSTRAINT:
		{

			Point2PointConstraint* p2p = 0;

			if (rb1)
			{
				p2p = new Point2PointConstraint(*rb0,
					*rb1,pivotInA,pivotInB);
			} else
			{
				p2p = new Point2PointConstraint(*rb0,
					pivotInA);
			}

			m_constraints.push_back(p2p);
			p2p->SetUserConstraintId(gConstraintUid++);
			p2p->SetUserConstraintType(type);
			//64 bit systems can't cast pointer to int. could use size_t instead.
			return p2p->GetUserConstraintId();

			break;
		}

	case PHY_ANGULAR_CONSTRAINT:
		angularOnly = true;

	case PHY_LINEHINGE_CONSTRAINT:
		{
			HingeConstraint* hinge = 0;

			if (rb1)
			{
				hinge = new HingeConstraint(
					*rb0,
					*rb1,pivotInA,pivotInB,axisInA,axisInB);


			} else
			{
				hinge = new HingeConstraint(*rb0,
					pivotInA,axisInA);

			}
			hinge->setAngularOnly(angularOnly);

			m_constraints.push_back(hinge);
			hinge->SetUserConstraintId(gConstraintUid++);
			hinge->SetUserConstraintType(type);
			//64 bit systems can't cast pointer to int. could use size_t instead.
			return hinge->GetUserConstraintId();
			break;
		}
#ifdef NEW_BULLET_VEHICLE_SUPPORT

	case PHY_VEHICLE_CONSTRAINT:
		{
			RaycastVehicle::VehicleTuning* tuning = new RaycastVehicle::VehicleTuning();
			RigidBody* chassis = rb0;
			DefaultVehicleRaycaster* raycaster = new DefaultVehicleRaycaster(this,ctrl0);
			RaycastVehicle* vehicle = new RaycastVehicle(*tuning,chassis,raycaster);
			WrapperVehicle* wrapperVehicle = new WrapperVehicle(vehicle,ctrl0);
			m_wrapperVehicles.push_back(wrapperVehicle);
			vehicle->SetUserConstraintId(gConstraintUid++);
			vehicle->SetUserConstraintType(type);
			return vehicle->GetUserConstraintId();

			break;
		};
#endif //NEW_BULLET_VEHICLE_SUPPORT

	default:
		{
		}
	};

	//RigidBody& rbA,RigidBody& rbB, const SimdVector3& pivotInA,const SimdVector3& pivotInB

	return 0;

}

void		CcdPhysicsEnvironment::removeConstraint(int	constraintId)
{
	std::vector<TypedConstraint*>::iterator i;

	for (i=m_constraints.begin();
		!(i==m_constraints.end()); i++)
	{
		TypedConstraint* constraint = (*i);
		if (constraint->GetUserConstraintId() == constraintId)
		{
			std::swap(*i, m_constraints.back());
			m_constraints.pop_back();
			break;
		}
	}

}


	struct	FilterClosestRayResultCallback : public CollisionWorld::ClosestRayResultCallback
	{
		PHY_IPhysicsController*	m_ignoreClient;

		FilterClosestRayResultCallback (PHY_IPhysicsController* ignoreClient,const SimdVector3& rayFrom,const SimdVector3& rayTo)
			: CollisionWorld::ClosestRayResultCallback(rayFrom,rayTo),
			m_ignoreClient(ignoreClient)
		{

		}

		virtual ~FilterClosestRayResultCallback()
		{
		}

		virtual	float	AddSingleResult(const CollisionWorld::LocalRayResult& rayResult)
		{
			CcdPhysicsController* curHit = static_cast<CcdPhysicsController*>(rayResult.m_collisionObject->m_userPointer);
			//ignore client...
			if (curHit != m_ignoreClient)
			{		
				//if valid
				return ClosestRayResultCallback::AddSingleResult(rayResult);
			}
			return m_closestHitFraction;
		}

	};

PHY_IPhysicsController* CcdPhysicsEnvironment::rayTest(PHY_IPhysicsController* ignoreClient, float fromX,float fromY,float fromZ, float toX,float toY,float toZ, 
													   float& hitX,float& hitY,float& hitZ,float& normalX,float& normalY,float& normalZ)
{


	float minFraction = 1.f;

	SimdVector3 rayFrom(fromX,fromY,fromZ);
	SimdVector3 rayTo(toX,toY,toZ);

	SimdVector3	hitPointWorld,normalWorld;

	//Either Ray Cast with or without filtering

	//CollisionWorld::ClosestRayResultCallback rayCallback(rayFrom,rayTo);
	FilterClosestRayResultCallback	 rayCallback(ignoreClient,rayFrom,rayTo);


	PHY_IPhysicsController* nearestHit = 0;

	m_collisionWorld->RayTest(rayFrom,rayTo,rayCallback);
	if (rayCallback.HasHit())
	{
		nearestHit = static_cast<CcdPhysicsController*>(rayCallback.m_collisionObject->m_userPointer);
		hitX = 	rayCallback.m_hitPointWorld.getX();
		hitY = 	rayCallback.m_hitPointWorld.getY();
		hitZ = 	rayCallback.m_hitPointWorld.getZ();

		normalX = rayCallback.m_hitNormalWorld.getX();
		normalY = rayCallback.m_hitNormalWorld.getY();
		normalZ = rayCallback.m_hitNormalWorld.getZ();

	}	


	return nearestHit;
}



int	CcdPhysicsEnvironment::getNumContactPoints()
{
	return 0;
}

void CcdPhysicsEnvironment::getContactPoint(int i,float& hitX,float& hitY,float& hitZ,float& normalX,float& normalY,float& normalZ)
{

}




BroadphaseInterface*	CcdPhysicsEnvironment::GetBroadphase()
{ 
	return m_collisionWorld->GetBroadphase(); 
}



const CollisionDispatcher* CcdPhysicsEnvironment::GetDispatcher() const
{
	return m_collisionWorld->GetDispatcher();
}

CollisionDispatcher* CcdPhysicsEnvironment::GetDispatcher()
{
	return m_collisionWorld->GetDispatcher();
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
	delete m_collisionWorld;

}


int	CcdPhysicsEnvironment::GetNumControllers()
{
	return m_controllers.size();
}


CcdPhysicsController* CcdPhysicsEnvironment::GetPhysicsController( int index)
{
	return m_controllers[index];
}


int	CcdPhysicsEnvironment::GetNumManifolds() const
{
	return GetDispatcher()->GetNumManifolds();
}

const PersistentManifold*	CcdPhysicsEnvironment::GetManifold(int index) const
{
	return GetDispatcher()->GetManifoldByIndexInternal(index);
}

TypedConstraint*	CcdPhysicsEnvironment::getConstraintById(int constraintId)
{
	int numConstraint = m_constraints.size();
	int i;
	for (i=0;i<numConstraint;i++)
	{
		TypedConstraint* constraint = m_constraints[i];
		if (constraint->GetUserConstraintId()==constraintId)
		{
			return constraint;
		}
	}
	return 0;
}


void CcdPhysicsEnvironment::addSensor(PHY_IPhysicsController* ctrl)
{
	//printf("addSensor\n");
}
void CcdPhysicsEnvironment::removeSensor(PHY_IPhysicsController* ctrl)
{
	//printf("removeSensor\n");
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

	//printf("requestCollisionCallback\n");
	m_triggerControllers.push_back(ccdCtrl);
}


void	CcdPhysicsEnvironment::CallbackTriggers()
{
	CcdPhysicsController* ctrl0=0,*ctrl1=0;

	if (m_triggerCallbacks[PHY_OBJECT_RESPONSE])
	{
		//walk over all overlapping pairs, and if one of the involved bodies is registered for trigger callback, perform callback
		int numManifolds = m_collisionWorld->GetDispatcher()->GetNumManifolds();
		for (int i=0;i<numManifolds;i++)
		{
			PersistentManifold* manifold = m_collisionWorld->GetDispatcher()->GetManifoldByIndexInternal(i);
			int numContacts = manifold->GetNumContacts();
			if (numContacts)
			{
				RigidBody* obj0 = static_cast<RigidBody* >(manifold->GetBody0());
				RigidBody* obj1 = static_cast<RigidBody* >(manifold->GetBody1());

				//m_userPointer is set in 'addPhysicsController
				CcdPhysicsController* ctrl0 = static_cast<CcdPhysicsController*>(obj0->m_userPointer);
				CcdPhysicsController* ctrl1 = static_cast<CcdPhysicsController*>(obj1->m_userPointer);

				std::vector<CcdPhysicsController*>::iterator i =
					std::find(m_triggerControllers.begin(), m_triggerControllers.end(), ctrl0);
				if (i == m_triggerControllers.end())
				{
					i = std::find(m_triggerControllers.begin(), m_triggerControllers.end(), ctrl1);
				}

				if (!(i == m_triggerControllers.end()))
				{
					m_triggerCallbacks[PHY_OBJECT_RESPONSE](m_triggerCallbacksUserPtrs[PHY_OBJECT_RESPONSE],
						ctrl0,ctrl1,0);
				}
			}
		}



	}

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
		if (wrapperVehicle->GetVehicle()->GetUserConstraintId() == constraintId)
			return wrapperVehicle;
	}

	return 0;
}

#endif //NEW_BULLET_VEHICLE_SUPPORT



void	CcdPhysicsEnvironment::UpdateAabbs(float	timeStep)
{
	std::vector<CcdPhysicsController*>::iterator i;
	BroadphaseInterface* scene =  GetBroadphase();

	//
			// update aabbs, only for moving objects (!)
			//
			for (i=m_controllers.begin();
				!(i==m_controllers.end()); i++)
			{
				CcdPhysicsController* ctrl = (*i);
				RigidBody* body = ctrl->GetRigidBody();


				SimdPoint3 minAabb,maxAabb;
				CollisionShape* shapeinterface = ctrl->GetCollisionShape();



				shapeinterface->CalculateTemporalAabb(body->getCenterOfMassTransform(),
					body->getLinearVelocity(),body->getAngularVelocity(),
					timeStep,minAabb,maxAabb);


				SimdVector3 manifoldExtraExtents(gContactBreakingTreshold,gContactBreakingTreshold,gContactBreakingTreshold);
				minAabb -= manifoldExtraExtents;
				maxAabb += manifoldExtraExtents;

				BroadphaseProxy* bp = body->m_broadphaseHandle;
				if (bp)
				{

					SimdVector3 color (1,1,0);

					if (m_debugDrawer)
					{	
						//draw aabb
						switch (body->GetActivationState())
						{
						case ISLAND_SLEEPING:
							{
								color.setValue(1,1,1);
								break;
							}
						case WANTS_DEACTIVATION:
							{
								color.setValue(0,0,1);
								break;
							}
						case ACTIVE_TAG:
							{
								break;
							}
						case DISABLE_DEACTIVATION:
							{
								color.setValue(1,0,1);
							};

						};

						if (m_debugDrawer->GetDebugMode() & IDebugDraw::DBG_DrawAabb)
						{
							DrawAabb(m_debugDrawer,minAabb,maxAabb,color);
						}
					}

					scene->SetAabb(bp,minAabb,maxAabb);



				}
			}
}
