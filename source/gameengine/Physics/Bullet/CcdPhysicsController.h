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


#ifndef BULLET2_PHYSICSCONTROLLER_H
#define BULLET2_PHYSICSCONTROLLER_H

#include "PHY_IPhysicsController.h"

///	PHY_IPhysicsController is the abstract simplified Interface to a physical object.
///	It contains the IMotionState and IDeformableMesh Interfaces.
#include "btBulletDynamicsCommon.h"

#include "PHY_IMotionState.h"

extern float gDeactivationTime;
extern float gLinearSleepingTreshold;
extern float gAngularSleepingTreshold;
extern bool gDisableDeactivation;
class CcdPhysicsEnvironment;
class btMotionState;



struct CcdConstructionInfo
{

	///CollisionFilterGroups provides some optional usage of basic collision filtering
	///this is done during broadphase, so very early in the pipeline
	///more advanced collision filtering should be done in btCollisionDispatcher::NeedsCollision
	enum CollisionFilterGroups
	{
	        DefaultFilter = 1,
	        StaticFilter = 2,
	        KinematicFilter = 4,
	        DebrisFilter = 8,
			SensorFilter = 16,
	        AllFilter = DefaultFilter | StaticFilter | KinematicFilter | DebrisFilter | SensorFilter,
	};


	CcdConstructionInfo()
		: m_gravity(0,0,0),
		m_scaling(1.f,1.f,1.f),
		m_mass(0.f),
		m_restitution(0.1f),
		m_friction(0.5f),
		m_linearDamping(0.1f),
		m_angularDamping(0.1f),
		m_collisionFlags(0),
		m_bRigid(false),
		m_collisionFilterGroup(DefaultFilter),
		m_collisionFilterMask(AllFilter),
		m_collisionShape(0),
		m_MotionState(0),
		m_physicsEnv(0),
		m_inertiaFactor(1.f)
	{
	}

	btVector3	m_localInertiaTensor;
	btVector3	m_gravity;
	btVector3	m_scaling;
	btScalar	m_mass;
	btScalar	m_restitution;
	btScalar	m_friction;
	btScalar	m_linearDamping;
	btScalar	m_angularDamping;
	int			m_collisionFlags;
	bool		m_bRigid;

	///optional use of collision group/mask:
	///only collision with object goups that match the collision mask.
	///this is very basic early out. advanced collision filtering should be
	///done in the btCollisionDispatcher::NeedsCollision and NeedsResponse
	///both values default to 1
	short int	m_collisionFilterGroup;
	short int	m_collisionFilterMask;

	class btCollisionShape*	m_collisionShape;
	class PHY_IMotionState*	m_MotionState;
	
	CcdPhysicsEnvironment*	m_physicsEnv; //needed for self-replication
	float	m_inertiaFactor;//tweak the inertia (hooked up to Blender 'formfactor'
};


class btRigidBody;


///CcdPhysicsController is a physics object that supports continuous collision detection and time of impact based physics resolution.
class CcdPhysicsController : public PHY_IPhysicsController	
{
	btRigidBody* m_body;
	class PHY_IMotionState*		m_MotionState;
	btMotionState* 	m_bulletMotionState;
	friend class CcdPhysicsEnvironment;	// needed when updating the controller


	void*		m_newClientInfo;

	CcdConstructionInfo	m_cci;//needed for replication
	void GetWorldOrientation(btMatrix3x3& mat);

	void CreateRigidbody();

	protected:
		void setWorldOrientation(const btMatrix3x3& mat);

	public:
	
		int				m_collisionDelay;
	

		CcdPhysicsController (const CcdConstructionInfo& ci);

		virtual ~CcdPhysicsController();


		btRigidBody* GetRigidBody() { return m_body;}

		btCollisionShape*	GetCollisionShape() { 
			return m_body->getCollisionShape();
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
		virtual PHY_IPhysicsController*	GetReplica();
		
		///There should be no 'SetCollisionFilterGroup' method, as changing this during run-time is will result in errors
		short int	GetCollisionFilterGroup() const
		{
			return m_cci.m_collisionFilterGroup;
		}
		///There should be no 'SetCollisionFilterGroup' method, as changing this during run-time is will result in errors
		short int	GetCollisionFilterMask() const
		{
			return m_cci.m_collisionFilterMask;
		}

		virtual void	calcXform() {} ;
		virtual void SetMargin(float margin) {};
		virtual float GetMargin() const {return 0.f;};


		bool	wantsSleeping();

		void	UpdateDeactivation(float timeStep);

		static btTransform	GetTransformFromMotionState(PHY_IMotionState* motionState);

		void	setAabb(const btVector3& aabbMin,const btVector3& aabbMax);


		class	PHY_IMotionState*			GetMotionState()
		{
			return m_MotionState;
		}

		const class	PHY_IMotionState*			GetMotionState() const
		{
			return m_MotionState;
		}

		class CcdPhysicsEnvironment* GetPhysicsEnvironment()
		{
			return m_cci.m_physicsEnv;
		}
		
};




///DefaultMotionState implements standard motionstate, using btTransform
class	DefaultMotionState : public PHY_IMotionState

{
	public:
		DefaultMotionState();

		virtual ~DefaultMotionState();

		virtual void	getWorldPosition(float& posX,float& posY,float& posZ);
		virtual void	getWorldScaling(float& scaleX,float& scaleY,float& scaleZ);
		virtual void	getWorldOrientation(float& quatIma0,float& quatIma1,float& quatIma2,float& quatReal);
		
		virtual void	setWorldPosition(float posX,float posY,float posZ);
		virtual	void	setWorldOrientation(float quatIma0,float quatIma1,float quatIma2,float quatReal);
		
		virtual	void	calculateWorldTransformations();
		
		btTransform	m_worldTransform;
		btVector3		m_localScaling;

};


#endif //BULLET2_PHYSICSCONTROLLER_H
