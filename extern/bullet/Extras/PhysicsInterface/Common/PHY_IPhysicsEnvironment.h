/*
 * Copyright (c) 2001-2005 Erwin Coumans <phy@erwincoumans.com>
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Erwin Coumans makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */
#ifndef _IPHYSICSENVIRONMENT
#define _IPHYSICSENVIRONMENT

#include <vector>
#include "PHY_DynamicTypes.h"

/**
*	Physics Environment takes care of stepping the simulation and is a container for physics entities (rigidbodies,constraints, materials etc.)
*	A derived class may be able to 'construct' entities by loading and/or converting
*/
class PHY_IPhysicsEnvironment
{
	public:
		virtual		~PHY_IPhysicsEnvironment();
		/// Perform an integration step of duration 'timeStep'.
		virtual	bool		proceedDeltaTime(double curTime,float timeStep)=0;


		virtual	void		setGravity(float x,float y,float z)=0;

		virtual int			createConstraint(class PHY_IPhysicsController* ctrl,class PHY_IPhysicsController* ctrl2,PHY_ConstraintType type,
			float pivotX,float pivotY,float pivotZ,
			float axisX,float axisY,float axisZ)=0;
		virtual void		removeConstraint(int constraintid)=0;

		virtual PHY_IPhysicsController* rayTest(PHY_IPhysicsController* ignoreClient, float fromX,float fromY,float fromZ, float toX,float toY,float toZ, 
										float& hitX,float& hitY,float& hitZ,float& normalX,float& normalY,float& normalZ)=0;


		//Methods for gamelogic collision/physics callbacks
		//todo:
		virtual void addSensor(PHY_IPhysicsController* ctrl)=0;
		virtual void removeSensor(PHY_IPhysicsController* ctrl)=0;
		virtual void addTouchCallback(int response_class, PHY_ResponseCallback callback, void *user)=0;
		virtual void requestCollisionCallback(PHY_IPhysicsController* ctrl)=0;
		virtual PHY_IPhysicsController*	CreateSphereController(float radius,const PHY__Vector3& position) =0;
		virtual PHY_IPhysicsController* CreateConeController(float coneradius,float coneheight)=0;
		
};

#endif //_IPHYSICSENVIRONMENT

