/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * The contents of this file may be used under the terms of either the GNU
 * General Public License Version 2 or later (the "GPL", see
 * http://www.gnu.org/licenses/gpl.html ), or the Blender License 1.0 or
 * later (the "BL", see http://www.blender.org/BL/ ) which has to be
 * bought from the Blender Foundation to become active, in which case the
 * above mentioned GPL option does not apply.
 *
 * The Original Code is Copyright (C) 2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __KX_ODEPHYSICSCONTROLLER_H
#define __KX_ODEPHYSICSCONTROLLER_H

#include "KX_IPhysicsController.h"
#include "OdePhysicsController.h"

/**
	Physics Controller, a special kind of Scene Graph Transformation Controller.
	It get's callbacks from Physics in case a transformation change took place.
	Each time the scene graph get's updated, the controller get's a chance
	in the 'Update' method to reflect changed.
*/

class KX_OdePhysicsController : public KX_IPhysicsController,	public ODEPhysicsController
							 
{

public:
	KX_OdePhysicsController(
		bool dyna,
		bool fullRigidBody,
		bool phantom,
		class PHY_IMotionState* motionstate,
		struct dxSpace* space,
		struct dxWorld*	world,
		float	mass,
		float	friction,
		float	restitution,
		bool	implicitsphere,
		float	center[3],
		float	extends[3],
		float	radius);
	
	virtual ~KX_OdePhysicsController() {};

	virtual void	applyImpulse(const MT_Point3& attach, const MT_Vector3& impulse);
	virtual void	SetObject (SG_IObject* object);

	virtual void	RelativeTranslate(const MT_Vector3& dloc,bool local);
	virtual void	RelativeRotate(const MT_Matrix3x3& drot,bool local);
	virtual void	ApplyTorque(const MT_Vector3& torque,bool local);
	virtual void	ApplyForce(const MT_Vector3& force,bool local);
	virtual MT_Vector3 GetLinearVelocity();
	virtual MT_Vector3 GetVelocity(const MT_Point3& pos);
	virtual void	SetAngularVelocity(const MT_Vector3& ang_vel,bool local);
	virtual void	SetLinearVelocity(const MT_Vector3& lin_vel,bool local);
	virtual void		resolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ);
	virtual	void		getOrientation(MT_Quaternion& orn);
	virtual	void setOrientation(const MT_Quaternion& orn);
	virtual	void setPosition(const MT_Point3& pos);
	virtual	void setScaling(const MT_Vector3& scaling);
	virtual	MT_Scalar	GetMass();
	virtual	MT_Vector3	getReactionForce();
	virtual void	setRigidBody(bool rigid);

	virtual void	SuspendDynamics(bool);
	virtual void	RestoreDynamics();
	

	virtual	SG_Controller*	GetReplica(class SG_Node* destnode);

	
	virtual void	SetSumoTransform(bool nondynaonly);
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

#endif //__KX_ODEPHYSICSCONTROLLER_H

