/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
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
		virtual void		resolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ) = 0;

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

