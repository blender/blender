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
#include "SumoPhysicsEnvironment.h"
#include "PHY_IMotionState.h"
#include "SumoPhysicsController.h"
#include "SM_Scene.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//#include <SOLID/SOLID.h>


SumoPhysicsEnvironment::SumoPhysicsEnvironment()
{
	m_sumoScene = new SM_Scene();
}



SumoPhysicsEnvironment::~SumoPhysicsEnvironment()
{
	delete m_sumoScene;
}

void SumoPhysicsEnvironment::proceed(double timeStep)
{
	m_sumoScene->proceed(timeStep,UpperBoundForFuzzicsIntegrator);
}

void SumoPhysicsEnvironment::setGravity(float x,float y,float z)
{
	m_sumoScene->setForceField(MT_Vector3(x,y,z));
}

int SumoPhysicsEnvironment::createConstraint(
	class PHY_IPhysicsController* ctrl,
	class PHY_IPhysicsController* ctrl2,
	PHY_ConstraintType type,
	float pivotX,float pivotY,float pivotZ,
	float axisX,float axisY,float axisZ)
{
	int constraintid = 0;
	return constraintid;
}

void SumoPhysicsEnvironment::removeConstraint(int constraintid)
{
	if (constraintid)
	{
	}
}

PHY_IPhysicsController* SumoPhysicsEnvironment::rayTest(void* ignoreClient, 
	float fromX,float fromY,float fromZ, 
	float toX,float toY,float toZ, 
	float& hitX,float& hitY,float& hitZ,
	float& normalX,float& normalY,float& normalZ)
{
	//collision detection / raytesting
	MT_Point3 hit, normal;
	/* FIXME: Return type is not a PHY_IPhysicsController */
	PHY_IPhysicsController *ret = (PHY_IPhysicsController *) m_sumoScene->rayTest(ignoreClient,MT_Point3(fromX, fromY, fromZ),MT_Point3(toX, toY, toZ), hit, normal);
	
	hitX = hit[0];
	hitY = hit[1];
	hitZ = hit[2];

	normalX = normal[0];
	normalY = normal[1];
	normalZ = normal[2];
	
	assert(false);
	
	return ret;
}


