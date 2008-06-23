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
 * KX_MouseFocusSensor determines mouse in/out/over events.
 */

#include <stdlib.h>
#include <stdio.h>

#include "KX_RayCast.h"

#include "MT_Point3.h"
#include "MT_Vector3.h"

#include "KX_IPhysicsController.h"
#include "PHY_IPhysicsEnvironment.h"
#include "PHY_IPhysicsController.h"

bool KX_RayCast::RayTest(KX_IPhysicsController* ignore_controller, PHY_IPhysicsEnvironment* physics_environment, const MT_Point3& _frompoint, const MT_Point3& topoint, MT_Point3& result_point, MT_Vector3& result_normal, const KX_RayCast& callback)
{
	// Loops over all physics objects between frompoint and topoint,
	// calling callback.RayHit for each one.
	//
	// callback.RayHit should return true to stop looking, or false to continue.
	//
	// returns true if an object was found, false if not.
	MT_Point3 frompoint(_frompoint);
	const MT_Vector3 todir( (topoint - frompoint).safe_normalized() );
	
	PHY_IPhysicsController* hit_controller;
	PHY__Vector3 phy_pos;
	PHY__Vector3 phy_normal;

	while((hit_controller = physics_environment->rayTest(dynamic_cast<PHY_IPhysicsController*>(ignore_controller),
			frompoint.x(),frompoint.y(),frompoint.z(),
			topoint.x(),topoint.y(),topoint.z(),
			phy_pos[0],phy_pos[1],phy_pos[2],
			phy_normal[0],phy_normal[1],phy_normal[2]))) 
	{
		result_point = MT_Point3(phy_pos);
		result_normal = MT_Vector3(phy_normal);
		KX_ClientObjectInfo* info = static_cast<KX_ClientObjectInfo*>(hit_controller->getNewClientInfo());
		
		if (!info)
		{
			printf("no info!\n");
			MT_assert(info && "Physics controller with no client object info");
			return false;
		}
		
		if (callback.RayHit(info, result_point, result_normal))
			return true;
	
		// There is a bug in the code below: the delta is computed with the wrong
		// sign on the face opposite to the center, resulting in infinite looping.
		// In Blender 2.45 this code was never executed because callback.RayHit() always 
		// returned true, causing the ray sensor to stop on the first object.
		// To avoid changing the behaviour will simply return false here.
		// It should be discussed if we want the ray sensor to "see" through objects
		// that don't have the required property/material (condition to get here)
		return false;
	
		// skip past the object and keep tracing
		/* We add 0.01 of fudge, so that if the margin && radius == 0., we don't endless loop. */
		MT_Scalar marg = 0.01 + hit_controller->GetMargin();
		marg += 2.f * hit_controller->GetMargin();
		/* Calculate the other side of this object */
		PHY__Vector3 hitpos;
		hit_controller->getPosition(hitpos);
		MT_Point3 hitObjPos(hitpos);
		
		MT_Vector3 hitvector = hitObjPos - result_point;
		if (hitvector.dot(hitvector) > MT_EPSILON)
		{
			hitvector.normalize();
			marg *= 2.*todir.dot(hitvector);
		}
		frompoint = result_point + marg * todir;
	}
	
	return hit_controller;
}

