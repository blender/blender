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

