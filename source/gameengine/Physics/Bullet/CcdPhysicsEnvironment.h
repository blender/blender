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

	btContactSolverInfo	m_solverInfo;
	
	void	processFhSprings(double curTime,float timeStep);

	public:
		CcdPhysicsEnvironment(bool useDbvtCulling, btDispatcher* dispatcher=0, btOverlappingPairCache* pairCache=0);

		virtual		~CcdPhysicsEnvironment();

		/////////////////////////////////////
		//PHY_IPhysicsEnvironment interface
		/////////////////////////////////////

		/// Perform an integration step of duration 'timeStep'.

		virtual void setDebugDrawer(btIDebugDraw* debugDrawer);

		virtual void		setNumIterations(int numIter);
		virtual void		setNumTimeSubSteps(int numTimeSubSteps)
		{
			m_numTimeSubSteps = numTimeSubSteps;
		}
		virtual void		setDeactivationTime(float dTime);
		virtual	void		setDeactivationLinearTreshold(float linTresh) ;
		virtual	void		setDeactivationAngularTreshold(float angTresh) ;
		virtual void		setContactBreakingTreshold(float contactBreakingTreshold) ;
		virtual void		setCcdMode(int ccdMode);
		virtual void		setSolverType(int solverType);
		virtual void		setSolverSorConstant(float sor);
		virtual void		setSolverTau(float tau);
		virtual void		setSolverDamping(float damping);
		virtual void		setLinearAirDamping(float damping);
		virtual void		setUseEpa(bool epa) ;

		virtual	void		beginFrame();
		virtual void		endFrame() {};
		/// Perform an integration step of duration 'timeStep'.
		virtual	bool		proceedDeltaTime(double curTime,float timeStep,float interval);
		
		virtual void		debugDrawWorld();
//		virtual bool		proceedDeltaTimeOneStep(float timeStep);

		virtual	void		setFixedTimeStep(bool useFixedTimeStep,float fixedTimeStep)
		{
			//based on DEFAULT_PHYSICS_TIC_RATE of 60 hertz
			setNumTimeSubSteps((int)(fixedTimeStep / 60.f));
		}
		//returns 0.f if no fixed timestep is used

		virtual	float		getFixedTimeStep(){ return 0.f;};

		virtual void		setDebugMode(int debugMode);

		virtual	void		setGravity(float x,float y,float z);
		virtual	void		getGravity(PHY__Vector3& grav);


		virtual int			createConstraint(class PHY_IPhysicsController* ctrl,class PHY_IPhysicsController* ctrl2,PHY_ConstraintType type,
			float pivotX,float pivotY,float pivotZ,
			float axisX,float axisY,float axisZ,
			float axis1X=0,float axis1Y=0,float axis1Z=0,
			float axis2X=0,float axis2Y=0,float axis2Z=0,int flag=0
			);


		//Following the COLLADA physics specification for constraints
		virtual int			createUniversalD6Constraint(
		class PHY_IPhysicsController* ctrlRef,class PHY_IPhysicsController* ctrlOther,
			btTransform& localAttachmentFrameRef,
			btTransform& localAttachmentOther,
			const btVector3& linearMinLimits,
			const btVector3& linearMaxLimits,
			const btVector3& angularMinLimits,
			const btVector3& angularMaxLimits,int flags
			);

		
		virtual void	setConstraintParam(int constraintId,int param,float value,float value1);
		
		virtual float	getConstraintParam(int constraintId,int param);

		virtual void		removeConstraint(int	constraintid);

		virtual float		getAppliedImpulse(int	constraintid);


		virtual void	CallbackTriggers();


#ifdef NEW_BULLET_VEHICLE_SUPPORT
		//complex constraint for vehicles
		virtual PHY_IVehicle*	getVehicleConstraint(int constraintId);
#else
		virtual class PHY_IVehicle*	getVehicleConstraint(int constraintId)
		{
			return 0;
		}
#endif //NEW_BULLET_VEHICLE_SUPPORT

		btTypedConstraint*	getConstraintById(int constraintId);

		virtual PHY_IPhysicsController* rayTest(PHY_IRayCastFilterCallback &filterCallback, float fromX,float fromY,float fromZ, float toX,float toY,float toZ);
		virtual bool cullingTest(PHY_CullingCallback callback, void* userData, PHY__Vector4* planes, int nplanes, int occlusionRes);


		//Methods for gamelogic collision/physics callbacks
		virtual void addSensor(PHY_IPhysicsController* ctrl);
		virtual void removeSensor(PHY_IPhysicsController* ctrl);
		virtual void addTouchCallback(int response_class, PHY_ResponseCallback callback, void *user);
		virtual bool requestCollisionCallback(PHY_IPhysicsController* ctrl);
		virtual bool removeCollisionCallback(PHY_IPhysicsController* ctrl);
		//These two methods are used *solely* to create controllers for Near/Radar sensor! Don't use for anything else
		virtual PHY_IPhysicsController*	CreateSphereController(float radius,const PHY__Vector3& position);
		virtual PHY_IPhysicsController* CreateConeController(float coneradius,float coneheight);
	

		virtual int	getNumContactPoints();

		virtual void getContactPoint(int i,float& hitX,float& hitY,float& hitZ,float& normalX,float& normalY,float& normalZ);

		//////////////////////
		//CcdPhysicsEnvironment interface
		////////////////////////
	
		void	addCcdPhysicsController(CcdPhysicsController* ctrl);

		bool	removeCcdPhysicsController(CcdPhysicsController* ctrl);

		void	updateCcdPhysicsController(CcdPhysicsController* ctrl, btScalar newMass, int newCollisionFlags, short int newCollisionGroup, short int newCollisionMask);

		void	disableCcdPhysicsController(CcdPhysicsController* ctrl);

		void	enableCcdPhysicsController(CcdPhysicsController* ctrl);

		void	refreshCcdPhysicsController(CcdPhysicsController* ctrl);

		void	addCcdGraphicController(CcdGraphicController* ctrl);

		void	removeCcdGraphicController(CcdGraphicController* ctrl);

		btBroadphaseInterface*	getBroadphase();
		btDbvtBroadphase*	getCullingTree() { return m_cullingTree; }

		btDispatcher*	getDispatcher();
		

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

		class	btSoftRigidDynamicsWorld*	getDynamicsWorld()
		{
			return m_dynamicsWorld;
		}
	
		class btConstraintSolver*	GetConstraintSolver();

		void MergeEnvironment(CcdPhysicsEnvironment *other);

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

		class btDispatcher* m_ownDispatcher;

		bool	m_scalingPropagated;

		virtual void	exportFile(const char* filename);

		
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:CcdPhysicsEnvironment"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__CCDPHYSICSENVIRONMENT_H__
