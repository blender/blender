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
#ifndef __ODE_PHYSICSCONTROLLER_H
#define __ODE_PHYSICSCONTROLLER_H


#include "PHY_IPhysicsController.h"

/**
	ODE Physics Controller, a special kind of a PhysicsController.
	A Physics Controller is a special kind of Scene Graph Transformation Controller.
	Each time the scene graph get's updated, the controller get's a chance
	in the 'Update' method to reflect changes.
*/

class ODEPhysicsController : public PHY_IPhysicsController
							 
{

	bool	m_OdeDyna;

public:
	ODEPhysicsController(
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
		float radius);

	virtual ~ODEPhysicsController();
	
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
	virtual void		SetActive(bool active){};
	virtual void		SuspendDynamics();
	virtual void		RestoreDynamics();
	virtual void		resolveCombinedVelocities(float linvelX,float linvelY,float linvelZ,float angVelX,float angVelY,float angVelZ)
	{
		//todo ?
	}


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
	
	// \todo remove next line !
	virtual void			SetSimulatedTime(float time);
	

	virtual	void		WriteDynamicsToMotionState() {};
	virtual void	WriteMotionStateToDynamics(bool nondynaonly);

	/** 
		call from Scene Graph Node to 'update'.
	*/
	virtual bool	SynchronizeMotionStates(float time);

	virtual void	calcXform(){}
	virtual void SetMargin(float margin) {}
	virtual float GetMargin() const {return 0.f;}

		// clientinfo for raycasts for example
	virtual	void*				getNewClientInfo() { return m_clientInfo;}
	virtual	void				setNewClientInfo(void* clientinfo) {m_clientInfo = clientinfo;};
	void*						m_clientInfo;

	struct	dxBody*				GetOdeBodyId() { return m_bodyId; }

	float	getFriction() { return m_friction;}
	float	getRestitution() { return m_restitution;}

	

private:

	bool						m_firstTime;
	bool						m_bFullRigidBody;
	bool						m_bPhantom;				// special flag for objects that are not affected by physics 'resolver'

	// data to calculate fake velocities for kinematic objects (non-dynas)
	bool						m_bKinematic;
	bool						m_bPrevKinematic;

	
	float						m_lastTime;
	bool						m_OdeSuspendDynamics;
	class	PHY_IMotionState*			m_MotionState;

	//Ode specific members
	struct	dxBody*				m_bodyId;
	struct dxGeom*				m_geomId;
	struct dxSpace*				m_space;
	struct dxWorld*				m_world;
	float						m_mass;
	float						m_friction;
	float						m_restitution;
	bool						m_implicitsphere;
	float						m_center[3];
	float						m_extends[3];
	float						m_radius;
};

#endif //__ODE_PHYSICSCONTROLLER_H

