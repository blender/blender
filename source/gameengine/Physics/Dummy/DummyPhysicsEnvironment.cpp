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
#include "DummyPhysicsEnvironment.h"
#include "PHY_IMotionState.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

DummyPhysicsEnvironment::DummyPhysicsEnvironment()
{
	// create physicsengine data
}



DummyPhysicsEnvironment::~DummyPhysicsEnvironment()
{
	//destroy physicsengine data
}

void DummyPhysicsEnvironment::beginFrame()
{
	// beginning of logic frame: apply forces
}

void DummyPhysicsEnvironment::endFrame()
{
	// end of logic frame: clear forces
}



bool		DummyPhysicsEnvironment::proceedDeltaTime(double  curTime,float timeStep,float interval)
{
	//step physics simulation, typically perform
	
	//collision detection 
	//solve constraints
	//integrate solution
	// return true if an update was done.
	return true;
}
void		DummyPhysicsEnvironment::setFixedTimeStep(bool useFixedTimeStep,float fixedTimeStep)
{
}
 
float		DummyPhysicsEnvironment::getFixedTimeStep()
{
	return 0.f;
}




void DummyPhysicsEnvironment::setGravity(float x,float y,float z)
{
}







int			DummyPhysicsEnvironment::createConstraint(class PHY_IPhysicsController* ctrl,class PHY_IPhysicsController* ctrl2,PHY_ConstraintType type,
		float pivotX,float pivotY,float pivotZ,float axisX,float axisY,float axisZ,
			float axis1X,float axis1Y,float axis1Z,
			float axis2X,float axis2Y,float axis2Z,int flag
		)
{
	
	int constraintid = 0;
	return constraintid;

}

void		DummyPhysicsEnvironment::removeConstraint(int	constraintid)
{
	if (constraintid)
	{
	}
}

PHY_IPhysicsController* DummyPhysicsEnvironment::rayTest(PHY_IRayCastFilterCallback &filterCallback,float fromX,float fromY,float fromZ, float toX,float toY,float toZ)
{
	//collision detection / raytesting
	return NULL;
}

