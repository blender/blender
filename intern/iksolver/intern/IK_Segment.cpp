/**
 * $Id$
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

#include "IK_Segment.h"


IK_Segment::
IK_Segment (
	const MT_Point3 tr1,
	const MT_Matrix3x3 A,
	const MT_Scalar length,
	const bool pitch_on,
	const bool yaw_on,
	const bool role_on
){	
	m_transform.setOrigin(tr1);
	m_transform.setBasis(A);
	m_angles[0] =MT_Scalar(0);	
	m_angles[1] =MT_Scalar(0);	
	m_angles[2] =MT_Scalar(0);	

	m_active_angles[0] = role_on;
	m_active_angles[1] = yaw_on;
	m_active_angles[2] = pitch_on;
	m_length = length;

	if (role_on) {
		m_angle_vectors.push_back(MT_Vector3(1,0,0));
	}
	if (yaw_on) {
		m_angle_vectors.push_back(MT_Vector3(0,1,0));
	}
	if (pitch_on) {
		m_angle_vectors.push_back(MT_Vector3(0,0,1));
	}
	UpdateLocalTransform();


};
	

IK_Segment::
IK_Segment (
) {
	m_transform.setIdentity();

	m_angles[0] =MT_Scalar(0);	
	m_angles[1] =MT_Scalar(0);	
	m_angles[2] =MT_Scalar(0);	

	m_active_angles[0] = false;
	m_active_angles[1] = false;
	m_active_angles[2] = false;
	m_length = MT_Scalar(1);

	UpdateLocalTransform();
}



// accessors
////////////

// The length of the segment 

const
	MT_Scalar
IK_Segment::
Length(
) const {
	return m_length;
}


// This is the transform from adjacent
// coordinate systems in the chain.

const 
	MT_Transform &
IK_Segment::
LocalTransform(
) const {
	return m_local_transform;
}

	void
IK_Segment::
UpdateGlobal(
	const MT_Transform & global
){

	// compute the global transform
	// and the start of the segment in global coordinates.
	
	m_seg_start = global * m_transform.getOrigin();
	m_global_transform = global * m_local_transform;
}

const 
	MT_Transform &
IK_Segment::
GlobalTransform(
) const {
	return m_global_transform;
}

const 	
	MT_Vector3 &
IK_Segment::
GlobalSegmentStart(
) const{
	return m_seg_start;
}


// Return the number of Degrees of Freedom 
// for this segment

	int
IK_Segment::
DoF(
) const {
	return 
		(m_active_angles[0] == true) + 
		(m_active_angles[1] == true) + 
		(m_active_angles[2] == true);
}


// suspect interface...
// Increment the active angles (at most 3) by 
// d_theta. Which angles are incremented depends 
// on which are active. It returns DoF

	int
IK_Segment::
IncrementAngles(
	MT_Scalar *d_theta
){
	int i =0;
	if (m_active_angles[0]) {
		m_angles[0] += d_theta[i];
		i++;
	}
	if (m_active_angles[1]) {
		m_angles[1] += d_theta[i];
		i++;
	}
	if (m_active_angles[2]) {
		m_angles[2] += d_theta[i];
		i++;
	}
	UpdateLocalTransform();

	return i;
}


	int
IK_Segment::
SetAngles(
	const MT_Scalar *angles
){
	int i =0;
	if (m_active_angles[0]) {
		m_angles[0] = angles[i];
		i++;
	}
	if (m_active_angles[1]) {
		m_angles[1] = angles[i];
		i++;
	}
	if (m_active_angles[2]) {
		m_angles[2] = angles[i];
		i++;
	}
	UpdateLocalTransform();

	return i;
}


	void
IK_Segment::
UpdateLocalTransform(
){
	// The local transformation is defined by
	// a user defined translation and rotation followed by
	// rotation by (roll,pitch,yaw) followed by
	// a translation in x of m_length

	MT_Quaternion rotx,roty,rotz;

	rotx.setRotation(MT_Vector3(1,0,0),m_angles[0]);
	roty.setRotation(MT_Vector3(0,1,0),m_angles[1]);
	rotz.setRotation(MT_Vector3(0,0,1),m_angles[2]);

	MT_Quaternion rot = rotx * roty *rotz;

	MT_Transform rx(MT_Point3(0,0,0),rot);

	MT_Transform translation;
	translation.setIdentity();
	translation.translate(MT_Vector3(0,m_length,0));

	m_local_transform = m_transform * rx * translation;
};


const
	std::vector<MT_Vector3> &
IK_Segment::
AngleVectors(
) const{
	return m_angle_vectors;
};
	
	MT_Scalar
IK_Segment::
ActiveAngle(
	int i
) const {
	MT_assert((i >=0) && (i < DoF()));

	// umm want to return the ith active angle
	// and not the ith angle

	int j;
	int angles = -1;
	for (j=0;j < 3;j++) {
		if (m_active_angles[j]) angles ++;
		if (i == angles) return m_angles[j];
	}
	return m_angles[0];
}

	MT_Scalar
IK_Segment::
Angle(
	int i
) const {
	MT_assert((i >=0) && (i < 3));		
	return m_angles[i];
}





