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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "IK_Chain.h"

using namespace std;

IK_Chain::
IK_Chain(
)
{
	// nothing to do;
};

const 
	vector<IK_Segment> &
IK_Chain::
Segments(
) const {
	return m_segments;
};

	vector<IK_Segment> &
IK_Chain::
Segments(
){
	return m_segments;
};	

	void
IK_Chain::
UpdateGlobalTransformations(
){

	// now iterate through the segment list
	// compute their local transformations if needed
		
	// assign their global transformations
	// (relative to chain origin)

	vector<IK_Segment>::const_iterator s_end = m_segments.end();
	vector<IK_Segment>::iterator s_it = m_segments.begin();

	MT_Transform global;

	global.setIdentity();

	for (; s_it != s_end; ++s_it) {
		s_it->UpdateGlobal(global);
		global = s_it->GlobalTransform();
	}
	// compute the position of the end effector and it's pose

	const MT_Transform &last_t = m_segments.back().GlobalTransform();
	m_end_effector = last_t.getOrigin();

	MT_Matrix3x3 last_basis = last_t.getBasis();
	last_basis.transpose();
	MT_Vector3 m_end_pose = last_basis[2];
	
	
};
	
const 
	TNT::Matrix<MT_Scalar> &
IK_Chain::
Jacobian(
) const  {
	return m_jacobian;
} ;
 

const 
	TNT::Matrix<MT_Scalar> &
IK_Chain::
TransposedJacobian(
) const {
	return m_t_jacobian;
};

	void
IK_Chain::
ComputeJacobian(
){
	// let's assume that the chain's global transfomations
	// have already been computed.
	
	int dof = DoF();

	int num_segs = m_segments.size();
	vector<IK_Segment>::const_iterator segs = m_segments.begin();

	m_t_jacobian.newsize(dof,3);
	m_jacobian.newsize(3,dof);

	// compute the transposed jacobian first

	int n;
	int i = 0;

	
	for (n= 0; n < num_segs; n++) {

		const MT_Matrix3x3 &basis = segs[n].GlobalTransform().getBasis();
		const MT_Vector3 &origin  = segs[n].GlobalSegmentStart();
				
		MT_Vector3 p = origin-m_end_effector;

		// iterate through the active angle vectors of this segment

		int angle_ind =0;
		int seg_dof = segs[n].DoF();

		const std::vector<MT_Vector3> & angle_vectors = segs[n].AngleVectors();

		for (angle_ind = 0;angle_ind <seg_dof; angle_ind++,i++) {

			MT_Vector3 angle_axis = angle_vectors[angle_ind];

			MT_Vector3 a = basis * angle_axis;
			MT_Vector3 pca = p.cross(a);

			m_t_jacobian(i + 1,1) = pca.x();
			m_t_jacobian(i + 1,2) = pca.y();
			m_t_jacobian(i + 1,3) = pca.z();

		}
	}

	// get the origina1 jacobain

	TNT::transpose(m_t_jacobian,m_jacobian);
};

	MT_Vector3 
IK_Chain::
EndEffector(
) const {
	return m_end_effector;
};

	MT_Vector3 
IK_Chain::
EndPose(
) const {
	return m_end_pose;
};
		

	int
IK_Chain::
DoF(
) const {

	// iterate through the segs and compute the DOF
	
	vector<IK_Segment>::const_iterator s_end = m_segments.end();
	vector<IK_Segment>::const_iterator s_it = m_segments.begin();

	int dof = 0;

	for (;s_it != s_end; ++s_it) {
		dof += s_it->DoF();
	}

	return dof;
}




	







