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

#include "SimulationIsland.h"
#include "SimdTransform.h"
#include "CcdPhysicsController.h"
#include "BroadphaseCollision/OverlappingPairCache.h"
#include "CollisionShapes/CollisionShape.h"
#include "BroadphaseCollision/Dispatcher.h"
#include "ConstraintSolver/ContactSolverInfo.h"
#include "ConstraintSolver/ConstraintSolver.h"
#include "ConstraintSolver/TypedConstraint.h"
#include "IDebugDraw.h"

extern float gContactBreakingTreshold;

bool	SimulationIsland::Simulate(IDebugDraw* debugDrawer,int numSolverIterations,TypedConstraint** constraintsBaseAddress,BroadphasePair*	overlappingPairBaseAddress, Dispatcher* dispatcher,BroadphaseInterface* broadphase,class ConstraintSolver*	solver,float timeStep)
{


#ifdef USE_QUICKPROF

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
			//todo: only do this when necessary, it's used for contact points
			body->m_cachedInvertedWorldTransform = body->m_worldTransform.inverse();

			if (body->IsActive())
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
	
	//BroadphaseInterface*	scene = GetBroadphase();


	//
	// collision detection (?)
	//


	#ifdef USE_QUICKPROF
	Profiler::beginBlock("DispatchAllCollisionPairs");
	#endif //USE_QUICKPROF


//	int numsubstep = m_numIterations;


	DispatcherInfo dispatchInfo;
	dispatchInfo.m_timeStep = timeStep;
	dispatchInfo.m_stepCount = 0;
	dispatchInfo.m_enableSatConvex = false;//m_enableSatCollisionDetection;
	dispatchInfo.m_debugDraw = debugDrawer;
	
	std::vector<BroadphasePair>	overlappingPairs;
	overlappingPairs.resize(this->m_overlappingPairIndices.size());

	//gather overlapping pair info
	int i;
	for (i=0;i<m_overlappingPairIndices.size();i++)
	{
		overlappingPairs[i] = overlappingPairBaseAddress[m_overlappingPairIndices[i]];
	}

	
	//pairCache->RefreshOverlappingPairs();
	if (overlappingPairs.size())
	{
		dispatcher->DispatchAllCollisionPairs(&overlappingPairs[0],overlappingPairs.size(),dispatchInfo);///numsubstep,g);
	}

	//scatter overlapping pair info, mainly the created algorithms/contact caches
	
	for (i=0;i<m_overlappingPairIndices.size();i++)
	{
		overlappingPairBaseAddress[m_overlappingPairIndices[i]] = overlappingPairs[i];
	}


	#ifdef USE_QUICKPROF
	Profiler::endBlock("DispatchAllCollisionPairs");
	#endif //USE_QUICKPROF




	int numRigidBodies = m_controllers.size();




	//contacts
	#ifdef USE_QUICKPROF
	Profiler::beginBlock("SolveConstraint");
	#endif //USE_QUICKPROF


	//solve the regular constraints (point 2 point, hinge, etc)

	for (int g=0;g<numSolverIterations;g++)
	{
		//
		// constraint solving
		//

		int i;
		int numConstraints = m_constraintIndices.size();

		//point to point constraints
		for (i=0;i< numConstraints ; i++ )
		{
			TypedConstraint* constraint = constraintsBaseAddress[m_constraintIndices[i]];
			constraint->BuildJacobian();
			constraint->SolveConstraint( timeStep );

		}


	}

	#ifdef USE_QUICKPROF
	Profiler::endBlock("SolveConstraint");
	#endif //USE_QUICKPROF

	/*

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
*/

	/*
	
	Profiler::beginBlock("CallbackTriggers");
	#endif //USE_QUICKPROF

	CallbackTriggers();

	#ifdef USE_QUICKPROF
	Profiler::endBlock("CallbackTriggers");

	}
	*/

	//OverlappingPairCache*	scene = GetCollisionWorld()->GetPairCache();
	
	ContactSolverInfo	solverInfo;

	solverInfo.m_friction = 0.9f;
	solverInfo.m_numIterations = numSolverIterations;
	solverInfo.m_timeStep = timeStep;
	solverInfo.m_restitution = 0.f;//m_restitution;


	if (m_manifolds.size())
	{
		solver->SolveGroup( &m_manifolds[0],m_manifolds.size(),solverInfo,0);
	}


#ifdef USE_QUICKPROF
	Profiler::beginBlock("proceedToTransform");
#endif //USE_QUICKPROF
	{



		{


			UpdateAabbs(debugDrawer,broadphase,timeStep);


			float toi = 1.f;

			//experimental continuous collision detection

			/*		if (m_ccdMode == 3)
			{
				DispatcherInfo dispatchInfo;
				dispatchInfo.m_timeStep = timeStep;
				dispatchInfo.m_stepCount = 0;
				dispatchInfo.m_dispatchFunc = DispatcherInfo::DISPATCH_CONTINUOUS;

				//			GetCollisionWorld()->GetDispatcher()->DispatchAllCollisionPairs(scene,dispatchInfo);
				toi = dispatchInfo.m_timeOfImpact;

			}
			*/



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

					if (body->IsActive())
					{

						if (!body->IsStatic())
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

				if (true)
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

		return true;
	}
}



void	SimulationIsland::SyncMotionStates(float timeStep)
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



void	SimulationIsland::UpdateAabbs(IDebugDraw* debugDrawer,BroadphaseInterface* scene,float	timeStep)
{
	std::vector<CcdPhysicsController*>::iterator i;


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
					body->getLinearVelocity(),
					//body->getAngularVelocity(),
					SimdVector3(0.f,0.f,0.f),//no angular effect for now //body->getAngularVelocity(),
					timeStep,minAabb,maxAabb);


				SimdVector3 manifoldExtraExtents(gContactBreakingTreshold,gContactBreakingTreshold,gContactBreakingTreshold);
				minAabb -= manifoldExtraExtents;
				maxAabb += manifoldExtraExtents;

				BroadphaseProxy* bp = body->m_broadphaseHandle;
				if (bp)
				{

					SimdVector3 color (1,1,0);

					class IDebugDraw*	m_debugDrawer = 0;
/*
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
*/

			
					if ( (maxAabb-minAabb).length2() < 1e12f)
					{
						scene->SetAabb(bp,minAabb,maxAabb);
					} else
					{
						//something went wrong, investigate
						//removeCcdPhysicsController(ctrl);
						body->SetActivationState(DISABLE_SIMULATION);

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

				}
			}
}