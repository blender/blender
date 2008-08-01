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
#ifndef __KX_IPHYSICSCONTROLLER_H
#define __KX_IPHYSICSCONTROLLER_H

#include "SG_Controller.h"
#include "MT_Vector3.h"
#include "MT_Point3.h"
#include "MT_Matrix3x3.h"

struct KX_ClientObjectInfo;

/**
	Physics Controller, a special kind of Scene Graph Transformation Controller.
	It get's callbacks from Physics in case a transformation change took place.
	Each time the scene graph get's updated, the controller get's a chance
	in the 'Update' method to reflect changed.
*/

class KX_IPhysicsController : public SG_Controller
							 
{
protected:
	bool		m_bDyna;
	bool		m_suspendDynamics;
	void*		m_userdata;
public:
	KX_IPhysicsController(bool dyna,void* userdata);
	virtual ~KX_IPhysicsController();


	virtual void	applyImpulse(const MT_Point3& attach, const MT_Vector3& impulse)=0;
	virtual void	SetObject (SG_IObject* object)=0;
	virtual void	setMargin (float collisionMargin)=0;

	virtual void	RelativeTranslate(const MT_Vector3& dloc,bool local)=0;
	virtual void	RelativeRotate(const MT_Matrix3x3& drot,bool local)=0;
	virtual void	ApplyTorque(const MT_Vector3& torque,bool local)=0;
	virtual void	ApplyForce(const MT_Vector3& force,bool local)=0;
	virtual MT_Vector3 GetLinearVelocity()=0;
	virtual MT_Vector3 GetAngularVelocity()=0;
	virtual MT_Vector3 GetVelocity(const MT_Point3& pos)=0;
	virtual void	SetAngularVelocity(const MT_Vector3& ang_vel,bool local)=0;
	virtual void	SetLinearVelocity(const MT_Vector3& lin_vel,bool local)=0;
	virtual void	resolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ) = 0;

	virtual	void	getOrientation(MT_Quaternion& orn)=0;
	virtual	void setOrientation(const MT_Matrix3x3& orn)=0;
	//virtual	void setOrientation(const MT_Quaternion& orn)=0;
	virtual	void setPosition(const MT_Point3& pos)=0;
	virtual	void setScaling(const MT_Vector3& scaling)=0;
	virtual	MT_Scalar	GetMass()=0;
	virtual	MT_Vector3	getReactionForce()=0;
	virtual void	setRigidBody(bool rigid)=0;

	virtual void	SuspendDynamics(bool ghost=false)=0;
	virtual void	RestoreDynamics()=0;

	virtual	SG_Controller*	GetReplica(class SG_Node* destnode)=0;

	void	SetDyna(bool isDynamic) {
		m_bDyna = isDynamic;
	}


	virtual void	SetSumoTransform(bool nondynaonly)=0;
	// todo: remove next line !
	virtual void	SetSimulatedTime(double time)=0;
	
	// call from scene graph to update
	virtual bool Update(double time)=0;
	void*	GetUserData() { return m_userdata;}
};

#endif //__KX_IPHYSICSCONTROLLER_H

