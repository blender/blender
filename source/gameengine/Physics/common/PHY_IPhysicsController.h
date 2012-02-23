/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file PHY_IPhysicsController.h
 *  \ingroup phys
 */

#ifndef __PHY_IPHYSICSCONTROLLER_H__
#define __PHY_IPHYSICSCONTROLLER_H__

#include "PHY_IController.h"

class PHY_IMotionState;
class PHY_IPhysicsEnvironment;

/**
	PHY_IPhysicsController is the abstract simplified Interface to a physical object.
	It contains the IMotionState and IDeformableMesh Interfaces.
*/
class PHY_IPhysicsController : public PHY_IController
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
		virtual class PHY_IMotionState* GetMotionState() = 0;
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
		virtual void		resolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ) = 0;

		virtual void		applyImpulse(float attachX,float attachY,float attachZ, float impulseX,float impulseY,float impulseZ)=0;
		virtual void		SetActive(bool active)=0;

		// reading out information from physics
		virtual void		GetLinearVelocity(float& linvX,float& linvY,float& linvZ)=0;
		virtual void		GetVelocity(const float posX,const float posY,const float posZ,float& linvX,float& linvY,float& linvZ)=0; 
		virtual	void		getReactionForce(float& forceX,float& forceY,float& forceZ)=0;

		// dyna's that are rigidbody are free in orientation, dyna's with non-rigidbody are restricted 
		virtual	void		setRigidBody(bool rigid)=0;

		virtual PHY_IPhysicsController*	GetReplica() {return 0;}

		virtual void	calcXform() =0;
		virtual void SetMargin(float margin) =0;
		virtual float GetMargin() const=0;
		virtual float GetRadius() const=0;
		virtual void  SetRadius(float margin) = 0;

		virtual float GetLinVelocityMin() const=0;
		virtual void  SetLinVelocityMin(float val) = 0;
		virtual float GetLinVelocityMax() const=0;
		virtual void  SetLinVelocityMax(float val) = 0;
		
		PHY__Vector3	GetWorldPosition(PHY__Vector3& localpos);

#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:PHY_IPhysicsController"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__PHY_IPHYSICSCONTROLLER_H__

