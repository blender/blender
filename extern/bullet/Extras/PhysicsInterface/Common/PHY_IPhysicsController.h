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


#ifndef PHY_IPHYSICSCONTROLLER_H
#define PHY_IPHYSICSCONTROLLER_H

#include "PHY_DynamicTypes.h"

/**
	PHY_IPhysicsController is the abstract simplified Interface to a physical object.
	It contains the IMotionState and IDeformableMesh Interfaces.
*/


class PHY_IPhysicsController	
{

	public:
		
		virtual ~PHY_IPhysicsController();
		/**
			SynchronizeMotionStates ynchronizes dynas, kinematic and deformable entities (and do 'late binding')
		*/
		virtual bool		SynchronizeMotionStates(float time)=0;
		/**
			WriteMotionStateToDynamics ynchronizes dynas, kinematic and deformable entities (and do 'late binding')
		*/
		
		virtual void		WriteMotionStateToDynamics(bool nondynaonly)=0;
		virtual	void		WriteDynamicsToMotionState()=0;
		// controller replication
		virtual	void		PostProcessReplica(class PHY_IMotionState* motionstate,class PHY_IPhysicsController* parentctrl)=0;

		// kinematic methods
		virtual void		RelativeTranslate(float dlocX,float dlocY,float dlocZ,bool local)=0;
		virtual void		RelativeRotate(const float drot[12],bool local)=0;
		virtual	void		getOrientation(float &quatImag0,float &quatImag1,float &quatImag2,float &quatReal)=0;
		virtual	void		setOrientation(float quatImag0,float quatImag1,float quatImag2,float quatReal)=0;
		virtual	void		setPosition(float posX,float posY,float posZ)=0;
		virtual	void 		getPosition(PHY__Vector3&	pos) const=0;
		virtual	void		setScaling(float scaleX,float scaleY,float scaleZ)=0;
		
		// physics methods
		virtual void		ApplyTorque(float torqueX,float torqueY,float torqueZ,bool local)=0;
		virtual void		ApplyForce(float forceX,float forceY,float forceZ,bool local)=0;
		virtual void		SetAngularVelocity(float ang_velX,float ang_velY,float ang_velZ,bool local)=0;
		virtual void		SetLinearVelocity(float lin_velX,float lin_velY,float lin_velZ,bool local)=0;
	
		virtual void		applyImpulse(float attachX,float attachY,float attachZ, float impulseX,float impulseY,float impulseZ)=0;
		virtual void		SetActive(bool active)=0;

		// reading out information from physics
		virtual void		GetLinearVelocity(float& linvX,float& linvY,float& linvZ)=0;
		virtual void		GetVelocity(const float posX,const float posY,const float posZ,float& linvX,float& linvY,float& linvZ)=0; 
		virtual	void		getReactionForce(float& forceX,float& forceY,float& forceZ)=0;

		// dyna's that are rigidbody are free in orientation, dyna's with non-rigidbody are restricted 
		virtual	void		setRigidBody(bool rigid)=0;


		// clientinfo for raycasts for example
		virtual	void*				getNewClientInfo()=0;
		virtual	void				setNewClientInfo(void* clientinfo)=0;
		virtual PHY_IPhysicsController*	GetReplica() {return 0;}

		virtual void	calcXform() =0;
		virtual void SetMargin(float margin) =0;
		virtual float GetMargin() const=0;
		virtual float GetRadius() const { return 0.f;}
		PHY__Vector3	GetWorldPosition(PHY__Vector3& localpos);

};

#endif //PHY_IPHYSICSCONTROLLER_H

