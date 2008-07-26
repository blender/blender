/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#ifndef __KX_SUMOPHYSICSCONTROLLER_H
#define __KX_SUMOPHYSICSCONTROLLER_H

#include "PHY_IPhysicsController.h"

/**
	Physics Controller, a special kind of Scene Graph Transformation Controller.
	It get's callbacks from Sumo in case a transformation change took place.
	Each time the scene graph get's updated, the controller get's a chance
	in the 'Update' method to reflect changed.
*/

#include "SumoPhysicsController.h"
#include "KX_IPhysicsController.h"

class KX_SumoPhysicsController : public KX_IPhysicsController,	
									public SumoPhysicsController

{


public:
	KX_SumoPhysicsController(
		class SM_Scene* sumoScene,
		class SM_Object* sumoObj,	
		class PHY_IMotionState* motionstate
		,bool dyna) 
		: KX_IPhysicsController(dyna,NULL) ,
		  SumoPhysicsController(sumoScene,/*solidscene,*/sumoObj,motionstate,dyna)
	{
	};
	virtual ~KX_SumoPhysicsController();

	void	applyImpulse(const MT_Point3& attach, const MT_Vector3& impulse);
	virtual void	SetObject (SG_IObject* object);
	virtual void	setMargin (float collisionMargin);
	
	void	RelativeTranslate(const MT_Vector3& dloc,bool local);
	void	RelativeRotate(const MT_Matrix3x3& drot,bool local);
	void	ApplyTorque(const MT_Vector3& torque,bool local);
	void	ApplyForce(const MT_Vector3& force,bool local);
	MT_Vector3 GetLinearVelocity();
	MT_Vector3 GetAngularVelocity()		// to keep compiler happy
		{ return MT_Vector3(0.0,0.0,0.0); }
	MT_Vector3 GetVelocity(const MT_Point3& pos);
	void	SetAngularVelocity(const MT_Vector3& ang_vel,bool local);
	void	SetLinearVelocity(const MT_Vector3& lin_vel,bool local);
	void	resolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ);


	void	SuspendDynamics(bool);
	void	RestoreDynamics();
	virtual	void	getOrientation(MT_Quaternion& orn);
	virtual	void setOrientation(const MT_Matrix3x3& orn);
	
	virtual	void setPosition(const MT_Point3& pos);
	virtual	void setScaling(const MT_Vector3& scaling);
	virtual	MT_Scalar	GetMass();
	virtual	MT_Vector3	getReactionForce();
	virtual	void	setRigidBody(bool rigid);
	

	virtual	SG_Controller*	GetReplica(class SG_Node* destnode);

	
	void	SetSumoTransform(bool nondynaonly);
	// todo: remove next line !
	virtual void	SetSimulatedTime(double time);
	
	// call from scene graph to update
	virtual bool Update(double time);

		void
	SetOption(
		int option,
		int value
	){
		// intentionally empty
	};
};

#endif //__KX_SUMOPHYSICSCONTROLLER_H

