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
#ifndef __SUMO_PHYSICSCONTROLLER_H
#define __SUMO_PHYSICSCONTROLLER_H

#include "PHY_IPhysicsController.h"
#include "SM_Scene.h"
#include "SM_Callback.h"

/**
	Sumo Physics Controller, a special kind of a PhysicsController.
	A Physics Controller is a special kind of Scene Graph Transformation Controller.
	Each time the scene graph get's updated, the controller get's a chance
	in the 'Update' method to reflect changes.
*/

class SumoPhysicsController : public PHY_IPhysicsController , public SM_Callback

							 
{


public:
	SumoPhysicsController(
				class SM_Scene* sumoScene,
				DT_SceneHandle solidscene,
				class SM_Object* sumoObj,
				class PHY_IMotionState* motionstate,
				bool dyna); 

	virtual ~SumoPhysicsController();
	
		// kinematic methods
	virtual void		RelativeTranslate(float dlocX,float dlocY,float dlocZ,bool local);
	virtual void		RelativeRotate(const float drot[12],bool local);
	virtual	void		getOrientation(float &quatImag0,float &quatImag1,float &quatImag2,float &quatReal);
	virtual	void		setOrientation(float quatImag0,float quatImag1,float quatImag2,float quatReal);
	virtual	void		setPosition(float posX,float posY,float posZ);
	virtual	void		setScaling(float scaleX,float scaleY,float scaleZ);
	
	// physics methods
	virtual void		ApplyTorque(float torqueX,float torqueY,float torqueZ,bool local);
	virtual void		ApplyForce(float forceX,float forceY,float forceZ,bool local);
	virtual void		SetAngularVelocity(float ang_velX,float ang_velY,float ang_velZ,bool local);
	virtual void		SetLinearVelocity(float lin_velX,float lin_velY,float lin_velZ,bool local);
	virtual void		resolveCombinedVelocities(const MT_Vector3 & lin_vel, const MT_Vector3 & ang_vel );
	virtual void		applyImpulse(float attachX,float attachY,float attachZ, float impulseX,float impulseY,float impulseZ);
	virtual void		SetActive(bool active){};
	virtual void		SuspendDynamics();
	virtual void		RestoreDynamics();


	/**  
		reading out information from physics
	*/
	virtual void		GetLinearVelocity(float& linvX,float& linvY,float& linvZ);
	/** 
		GetVelocity parameters are in geometric coordinates (Origin is not center of mass!).
	*/
	virtual void		GetVelocity(const float posX,const float posY,const float posZ,float& linvX,float& linvY,float& linvZ); 
	virtual	float		getMass();
	virtual	void		getReactionForce(float& forceX,float& forceY,float& forceZ);
	virtual	void		setRigidBody(bool rigid);
		
	
	virtual	void		PostProcessReplica(class PHY_IMotionState* motionstate,class PHY_IPhysicsController* parentctrl);
	
	// todo: remove next line !
	virtual void			SetSimulatedTime(float time);
	

	virtual	void		WriteDynamicsToMotionState() {};
	virtual void	WriteMotionStateToDynamics(bool nondynaonly);

	/** 
		call from Scene Graph Node to 'update'.
	*/
	virtual bool	SynchronizeMotionStates(float time);

		// clientinfo for raycasts for example
	virtual	void*				getClientInfo() { return m_clientInfo;}
	virtual	void				setClientInfo(void* clientinfo) {m_clientInfo = clientinfo;};
	void*						m_clientInfo;


	float	getFriction() { return m_friction;}
	float	getRestitution() { return m_restitution;}

	// sumo callback
	virtual void do_me();

	class SM_Object*	GetSumoObject ()
	{
		return m_sumoObj;
	};

	void GetWorldOrientation(class MT_Matrix3x3& mat);
	void GetWorldPosition(MT_Point3& pos);
	void GetWorldScaling(MT_Vector3& scale);


//	void	SetSumoObject(class SM_Object* sumoObj)	{
//		m_sumoObj = sumoObj;
//	}
//	void	SetSumoScene(class SM_Scene* sumoScene)	{
//		m_sumoScene = sumoScene;
//	}

	void	setSumoTransform(bool nondynaonly);


private:
	class SM_Object*	m_sumoObj;
	class SM_Scene*		m_sumoScene; // needed for replication
	DT_SceneHandle		m_solidscene;
	bool				m_bFirstTime;
	bool				m_bDyna;

	float						m_friction;
	float						m_restitution;


	bool						m_suspendDynamics;

	bool						m_firstTime;
	bool						m_bFullRigidBody;
	bool						m_bPhantom;				// special flag for objects that are not affected by physics 'resolver'

	// data to calculate fake velocities for kinematic objects (non-dynas)
	bool						m_bKinematic;
	bool						m_bPrevKinematic;
	
	float						m_lastTime;

	class	PHY_IMotionState*			m_MotionState;
	
};

#endif //__SUMO_PHYSICSCONTROLLER_H

