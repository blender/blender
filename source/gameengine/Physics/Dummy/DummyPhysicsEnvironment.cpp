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

void DummyPhysicsEnvironment::proceed(double timeStep)
{
	//step physics simulation, typically perform
	
	//collision detection 
	//solve constraints
	//integrate solution

}

void DummyPhysicsEnvironment::setGravity(float x,float y,float z)
{
}







int			DummyPhysicsEnvironment::createConstraint(class PHY_IPhysicsController* ctrl,class PHY_IPhysicsController* ctrl2,PHY_ConstraintType type,
		float pivotX,float pivotY,float pivotZ,float axisX,float axisY,float axisZ)
{
	
	int constraintid = 0;
	return constraintid;

}

void		DummyPhysicsEnvironment::removeConstraint(int constraintid)
{
	if (constraintid)
	{
	}
}

PHY_IPhysicsController* DummyPhysicsEnvironment::rayTest(void* ignoreClient,float fromX,float fromY,float fromZ, float toX,float toY,float toZ, 
									float& hitX,float& hitY,float& hitZ,float& normalX,float& normalY,float& normalZ)
{
	//collision detection / raytesting
	return NULL;
}

