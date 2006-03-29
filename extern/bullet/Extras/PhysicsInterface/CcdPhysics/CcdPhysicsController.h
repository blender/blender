
#ifndef BULLET2_PHYSICSCONTROLLER_H
#define BULLET2_PHYSICSCONTROLLER_H

#include "PHY_IPhysicsController.h"

///	PHY_IPhysicsController is the abstract simplified Interface to a physical object.
///	It contains the IMotionState and IDeformableMesh Interfaces.
#include "SimdVector3.h"
#include "SimdScalar.h"	
#include "SimdMatrix3x3.h"
#include "SimdTransform.h"
#include "Dynamics/RigidBody.h"


#include "BroadphaseCollision/BroadphaseProxy.h" //for CollisionShape access
class CollisionShape;

extern float gDeactivationTime;
extern float gLinearSleepingTreshold;
extern float gAngularSleepingTreshold;
extern bool gDisableDeactivation;
class CcdPhysicsEnvironment;


struct CcdConstructionInfo
{
	CcdConstructionInfo()
		: m_gravity(0,0,0),
		m_mass(0.f),
		m_restitution(0.1f),
		m_linearDamping(0.1f),
		m_angularDamping(0.1f),
		m_MotionState(0),
		m_physicsEnv(0),
		m_inertiaFactor(1.f),
		m_scaling(1.f,1.f,1.f)
	{
	}

	SimdVector3	m_localInertiaTensor;
	SimdVector3	m_gravity;
	SimdVector3	m_scaling;
	SimdScalar	m_mass;
	SimdScalar	m_restitution;
	SimdScalar	m_friction;
	SimdScalar	m_linearDamping;
	SimdScalar	m_angularDamping;

	CollisionShape*			m_collisionShape;
	class	PHY_IMotionState*			m_MotionState;

	CcdPhysicsEnvironment*	m_physicsEnv; //needed for self-replication
	float	m_inertiaFactor;//tweak the inertia (hooked up to Blender 'formfactor'
};


class RigidBody;

///CcdPhysicsController is a physics object that supports continuous collision detection and time of impact based physics resolution.
class CcdPhysicsController : public PHY_IPhysicsController	
{
	RigidBody* m_body;
	class	PHY_IMotionState*			m_MotionState;
	

	void*		m_newClientInfo;

	CcdConstructionInfo	m_cci;//needed for replication
	void GetWorldOrientation(SimdMatrix3x3& mat);

	void CreateRigidbody();

	public:
	
		int				m_collisionDelay;
	

		CcdPhysicsController (const CcdConstructionInfo& ci);

		virtual ~CcdPhysicsController();


		RigidBody* GetRigidBody() { return m_body;}

		CollisionShape*	GetCollisionShape() { 
			return m_body->GetCollisionShape();
		}
		////////////////////////////////////
		// PHY_IPhysicsController interface
		////////////////////////////////////


		/**
			SynchronizeMotionStates ynchronizes dynas, kinematic and deformable entities (and do 'late binding')
		*/
		virtual bool		SynchronizeMotionStates(float time);
		/**
			WriteMotionStateToDynamics ynchronizes dynas, kinematic and deformable entities (and do 'late binding')
		*/
		
		virtual void		WriteMotionStateToDynamics(bool nondynaonly);
		virtual	void		WriteDynamicsToMotionState();
		// controller replication
		virtual	void		PostProcessReplica(class PHY_IMotionState* motionstate,class PHY_IPhysicsController* parentctrl);

		// kinematic methods
		virtual void		RelativeTranslate(float dlocX,float dlocY,float dlocZ,bool local);
		virtual void		RelativeRotate(const float drot[9],bool local);
		virtual	void		getOrientation(float &quatImag0,float &quatImag1,float &quatImag2,float &quatReal);
		virtual	void		setOrientation(float quatImag0,float quatImag1,float quatImag2,float quatReal);
		virtual	void		setPosition(float posX,float posY,float posZ);
		virtual	void 		getPosition(PHY__Vector3&	pos) const;

		virtual	void		setScaling(float scaleX,float scaleY,float scaleZ);
		
		// physics methods
		virtual void		ApplyTorque(float torqueX,float torqueY,float torqueZ,bool local);
		virtual void		ApplyForce(float forceX,float forceY,float forceZ,bool local);
		virtual void		SetAngularVelocity(float ang_velX,float ang_velY,float ang_velZ,bool local);
		virtual void		SetLinearVelocity(float lin_velX,float lin_velY,float lin_velZ,bool local);
		virtual void		applyImpulse(float attachX,float attachY,float attachZ, float impulseX,float impulseY,float impulseZ);
		virtual void		SetActive(bool active);

		// reading out information from physics
		virtual void		GetLinearVelocity(float& linvX,float& linvY,float& linvZ);
		virtual void		GetAngularVelocity(float& angVelX,float& angVelY,float& angVelZ);
		virtual void		GetVelocity(const float posX,const float posY,const float posZ,float& linvX,float& linvY,float& linvZ); 
		virtual	void		getReactionForce(float& forceX,float& forceY,float& forceZ);

		// dyna's that are rigidbody are free in orientation, dyna's with non-rigidbody are restricted 
		virtual	void		setRigidBody(bool rigid);

		
		virtual void		resolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ);

		// clientinfo for raycasts for example
		virtual	void*				getNewClientInfo();
		virtual	void				setNewClientInfo(void* clientinfo);
		virtual PHY_IPhysicsController*	GetReplica() {return 0;}

		virtual void	calcXform() {} ;
		virtual void SetMargin(float margin) {};
		virtual float GetMargin() const {return 0.f;};


		bool	wantsSleeping();

		void	UpdateDeactivation(float timeStep);

		static SimdTransform	GetTransformFromMotionState(PHY_IMotionState* motionState);

		void	SetAabb(const SimdVector3& aabbMin,const SimdVector3& aabbMax);


};

#endif //BULLET2_PHYSICSCONTROLLER_H
