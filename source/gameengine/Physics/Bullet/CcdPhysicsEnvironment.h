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

/** \file CcdPhysicsEnvironment.h
 *  \ingroup physbullet
 *  See also \ref bulletdoc
 */

#ifndef __CCDPHYSICSENVIRONMENT_H__
#define __CCDPHYSICSENVIRONMENT_H__

#include "PHY_IPhysicsEnvironment.h"
#include <vector>
#include <set>
#include <map>
class CcdPhysicsController;
class CcdGraphicController;
#include "LinearMath/btVector3.h"
#include "LinearMath/btTransform.h"




class btTypedConstraint;
class btSimulationIslandManager;
class btCollisionDispatcher;
class btDispatcher;
//#include "btBroadphaseInterface.h"

//switch on/off new vehicle support
#define NEW_BULLET_VEHICLE_SUPPORT 1

#include "BulletDynamics/ConstraintSolver/btContactSolverInfo.h"

class WrapperVehicle;
class btPersistentManifold;
class btBroadphaseInterface;
struct btDbvtBroadphase;
class btOverlappingPairCache;
class btIDebugDraw;
class PHY_IVehicle;
class CcdOverlapFilterCallBack;

/** CcdPhysicsEnvironment is an experimental mainloop for physics simulation using optional continuous collision detection.
 * Physics Environment takes care of stepping the simulation and is a container for physics entities.
 * It stores rigidbodies,constraints, materials etc.
 * A derived class may be able to 'construct' entities by loading and/or converting
 */
class CcdPhysicsEnvironment : public PHY_IPhysicsEnvironment
{
	friend class CcdOverlapFilterCallBack;
	btVector3 m_gravity;

protected:
	btIDebugDraw*	m_debugDrawer;
	
	class btDefaultCollisionConfiguration* m_collisionConfiguration;
	class btBroadphaseInterface*		m_broadphase;	// broadphase for dynamic world
	// for culling only
	btOverlappingPairCache*				m_cullingCache;
	struct btDbvtBroadphase*			m_cullingTree;	// broadphase for culling

	//solver iterations
	int	m_numIterations;
	
	//timestep subdivisions
	int	m_numTimeSubSteps;


	int	m_ccdMode;
	int	m_solverType;
	int	m_profileTimings;
	bool m_enableSatCollisionDetection;

	void	ProcessFhSprings(double curTime,float timeStep);

	public:
		CcdPhysicsEnvironment(bool useDbvtCulling, btDispatcher* dispatcher=0, btOverlappingPairCache* pairCache=0);

		virtual		~CcdPhysicsEnvironment();

		/////////////////////////////////////
		//PHY_IPhysicsEnvironment interface
		/////////////////////////////////////

		/// Perform an integration step of duration 'timeStep'.

		virtual void SetDebugDrawer(btIDebugDraw* debugDrawer);

		virtual void		SetNumIterations(int numIter);
		virtual void		SetNumTimeSubSteps(int numTimeSubSteps)
		{
			m_numTimeSubSteps = numTimeSubSteps;
		}
		virtual void		SetDeactivationTime(float dTime);
		virtual	void		SetDeactivationLinearTreshold(float linTresh);
		virtual	void		SetDeactivationAngularTreshold(float angTresh);
		virtual void		SetContactBreakingTreshold(float contactBreakingTreshold);
		virtual void		SetCcdMode(int ccdMode);
		virtual void		SetSolverType(int solverType);
		virtual void		SetSolverSorConstant(float sor);
		virtual void		SetSolverTau(float tau);
		virtual void		SetSolverDamping(float damping);
		virtual void		SetLinearAirDamping(float damping);
		virtual void		SetUseEpa(bool epa);

		virtual int			GetNumTimeSubSteps()
		{
			return m_numTimeSubSteps;
		}

		virtual	void		BeginFrame();
		virtual void		EndFrame() {}
		/// Perform an integration step of duration 'timeStep'.
		virtual	bool		ProceedDeltaTime(double curTime,float timeStep,float interval);

		virtual void		DebugDrawWorld();
//		virtual bool		proceedDeltaTimeOneStep(float timeStep);

		virtual	void		SetFixedTimeStep(bool useFixedTimeStep,float fixedTimeStep)
		{
			//based on DEFAULT_PHYSICS_TIC_RATE of 60 hertz
			SetNumTimeSubSteps((int)(fixedTimeStep / 60.f));
		}
		//returns 0.f if no fixed timestep is used

		virtual	float		GetFixedTimeStep() { return 0.f; }

		virtual void		SetDebugMode(int debugMode);
		virtual int			GetDebugMode()const;

		virtual	void		SetGravity(float x,float y,float z);
		virtual	void		GetGravity(MT_Vector3& grav);


		virtual int			CreateConstraint(class PHY_IPhysicsController* ctrl,class PHY_IPhysicsController* ctrl2,PHY_ConstraintType type,
			float pivotX,float pivotY,float pivotZ,
			float axisX,float axisY,float axisZ,
			float axis1X=0,float axis1Y=0,float axis1Z=0,
			float axis2X=0,float axis2Y=0,float axis2Z=0,int flag=0
			);


		//Following the COLLADA physics specification for constraints
		virtual int			CreateUniversalD6Constraint(
		class PHY_IPhysicsController* ctrlRef,class PHY_IPhysicsController* ctrlOther,
			btTransform& localAttachmentFrameRef,
			btTransform& localAttachmentOther,
			const btVector3& linearMinLimits,
			const btVector3& linearMaxLimits,
			const btVector3& angularMinLimits,
			const btVector3& angularMaxLimits,int flags
			);

		
		virtual void	SetConstraintParam(int constraintId,int param,float value,float value1);
		
		virtual float	GetConstraintParam(int constraintId,int param);

		virtual void		RemoveConstraint(int	constraintid);

		virtual float		getAppliedImpulse(int	constraintid);


		virtual void	CallbackTriggers();


#ifdef NEW_BULLET_VEHICLE_SUPPORT
		//complex constraint for vehicles
		virtual PHY_IVehicle*	GetVehicleConstraint(int constraintId);
#else
		virtual class PHY_IVehicle*	GetVehicleConstraint(int constraintId)
		{
			return 0;
		}
#endif  /* NEW_BULLET_VEHICLE_SUPPORT */
		// Character physics wrapper
		virtual PHY_ICharacter*	GetCharacterController(class KX_GameObject* ob);

		btTypedConstraint*	GetConstraintById(int constraintId);

		virtual PHY_IPhysicsController* RayTest(PHY_IRayCastFilterCallback &filterCallback, float fromX,float fromY,float fromZ, float toX,float toY,float toZ);
		virtual bool CullingTest(PHY_CullingCallback callback, void* userData, MT_Vector4* planes, int nplanes, int occlusionRes, const int *viewport, double modelview[16], double projection[16]);


		//Methods for gamelogic collision/physics callbacks
		virtual void AddSensor(PHY_IPhysicsController* ctrl);
		virtual void RemoveSensor(PHY_IPhysicsController* ctrl);
		virtual void AddTouchCallback(int response_class, PHY_ResponseCallback callback, void *user);
		virtual bool RequestCollisionCallback(PHY_IPhysicsController* ctrl);
		virtual bool RemoveCollisionCallback(PHY_IPhysicsController* ctrl);
		//These two methods are used *solely* to create controllers for Near/Radar sensor! Don't use for anything else
		virtual PHY_IPhysicsController*	CreateSphereController(float radius,const MT_Vector3& position);
		virtual PHY_IPhysicsController* CreateConeController(float coneradius,float coneheight);
	

		virtual int	GetNumContactPoints();

		virtual void GetContactPoint(int i,float& hitX,float& hitY,float& hitZ,float& normalX,float& normalY,float& normalZ);

		//////////////////////
		//CcdPhysicsEnvironment interface
		////////////////////////
	
		void	AddCcdPhysicsController(CcdPhysicsController* ctrl);

		bool	RemoveCcdPhysicsController(CcdPhysicsController* ctrl);

		void	UpdateCcdPhysicsController(CcdPhysicsController* ctrl, btScalar newMass, int newCollisionFlags, short int newCollisionGroup, short int newCollisionMask);

		void	DisableCcdPhysicsController(CcdPhysicsController* ctrl);

		void	EnableCcdPhysicsController(CcdPhysicsController* ctrl);

		void	RefreshCcdPhysicsController(CcdPhysicsController* ctrl);

		void	AddCcdGraphicController(CcdGraphicController* ctrl);

		void	RemoveCcdGraphicController(CcdGraphicController* ctrl);

		btBroadphaseInterface*	GetBroadphase();
		btDbvtBroadphase*	GetCullingTree() { return m_cullingTree; }

		btDispatcher*	GetDispatcher();
		

		bool	IsSatCollisionDetectionEnabled() const
		{
			return m_enableSatCollisionDetection;
		}

		void	EnableSatCollisionDetection(bool enableSat)
		{
			m_enableSatCollisionDetection = enableSat;
		}

	
		const btPersistentManifold*	GetManifold(int index) const;

	
		void	SyncMotionStates(float timeStep);

		class	btSoftRigidDynamicsWorld*	GetDynamicsWorld()
		{
			return m_dynamicsWorld;
		}
	
		class btConstraintSolver*	GetConstraintSolver();

		void MergeEnvironment(PHY_IPhysicsEnvironment *other_env);

		static CcdPhysicsEnvironment *Create(struct Scene *blenderscene, bool visualizePhysics);

		virtual void ConvertObject(KX_GameObject* gameobj,
							RAS_MeshObject* meshobj,
							DerivedMesh* dm,
							KX_Scene* kxscene,
							PHY_ShapeProps* shapeprops,
							PHY_MaterialProps*	smmaterial,
							PHY_IMotionState *motionstate,
							int activeLayerBitInfo,
							bool isCompoundChild,
							bool hasCompoundChildren);

	protected:
		
		

		std::set<CcdPhysicsController*> m_controllers;
		std::set<CcdPhysicsController*> m_triggerControllers;

		PHY_ResponseCallback	m_triggerCallbacks[PHY_NUM_RESPONSE];
		void*			m_triggerCallbacksUserPtrs[PHY_NUM_RESPONSE];
		
		std::vector<WrapperVehicle*>	m_wrapperVehicles;

		//use explicit btSoftRigidDynamicsWorld/btDiscreteDynamicsWorld* so that we have access to 
		//btDiscreteDynamicsWorld::addRigidBody(body,filter,group) 
		//so that we can set the body collision filter/group at the time of creation 
		//and not afterwards (breaks the collision system for radar/near sensor)
		//Ideally we would like to have access to this function from the btDynamicsWorld interface
		//class	btDynamicsWorld*	m_dynamicsWorld;
		class	btSoftRigidDynamicsWorld*	m_dynamicsWorld;
		
		class btConstraintSolver*	m_solver;

		class btOverlappingPairCache* m_ownPairCache;

		class CcdOverlapFilterCallBack* m_filterCallback;

		class btGhostPairCallback*	m_ghostPairCallback;

		class btDispatcher* m_ownDispatcher;

		bool	m_scalingPropagated;

		virtual void	ExportFile(const char* filename);

		
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:CcdPhysicsEnvironment")
#endif
};

#endif  /* __CCDPHYSICSENVIRONMENT_H__ */
