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

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 *
 * @author Laurence
 */

#include "IK_QChain.h"

using namespace std;

IK_QChain::
IK_QChain(
)
{
	// nothing to do;
};

const 
	vector<IK_QSegment> &
IK_QChain::
Segments(
) const {
	return m_segments;
};

	vector<IK_QSegment> &
IK_QChain::
Segments(
){
	return m_segments;
};	

	void
IK_QChain::
UpdateGlobalTransformations(
){

	// now iterate through the segment list
	// compute their local transformations if needed
		
	// assign their global transformations
	// (relative to chain origin)

	vector<IK_QSegment>::const_iterator s_end = m_segments.end();
	vector<IK_QSegment>::iterator s_it = m_segments.begin();

	MT_Transform global;
	global.setIdentity();

	for (; s_it != s_end; ++s_it) {
		global = s_it->UpdateGlobal(global);
	}

	// we also need to compute the accumulated local transforms
	// for each segment

	MT_Transform acc_local;
	acc_local.setIdentity();

	vector<IK_QSegment>::reverse_iterator s_rit = m_segments.rbegin();
	vector<IK_QSegment>::reverse_iterator s_rend = m_segments.rend();

	for (; s_rit != s_rend; ++s_rit) {
		acc_local = s_rit->UpdateAccumulatedLocal(acc_local);
	}		

	// compute the position of the end effector and it's pose

	const MT_Transform &last_t = m_segments.back().GlobalTransform();
	m_end_effector = last_t.getOrigin();

#if 0
	
	// The end pose is not currently used. 

	MT_Matrix3x3 last_basis = last_t.getBasis();
	last_basis.transpose();
	MT_Vector3 m_end_pose = last_basis[1];
	
#endif
	
};
	
const 
	TNT::Matrix<MT_Scalar> &
IK_QChain::
Jacobian(
) const  {
	return m_jacobian;
} ;
 

const 
	TNT::Matrix<MT_Scalar> &
IK_QChain::
TransposedJacobian(
) const {
	return m_t_jacobian;
};

	void
IK_QChain::
ComputeJacobian(
){
	// let's assume that the chain's global transfomations
	// have already been computed.
	
	int dof = DoF();

	int num_segs = m_segments.size();
	vector<IK_QSegment>::const_iterator segs = m_segments.begin();

	m_t_jacobian.newsize(dof,3);
	m_jacobian.newsize(3,dof);

	// compute the transposed jacobian first

	int n;
	int i = 0;

	for (n= 0; n < num_segs; n++) {
#if 0

		// For euler angle computation we can use a slightly quicker method.

		const MT_Matrix3x3 &basis = segs[n].GlobalTransform().getBasis();
		const MT_Vector3 &origin  = segs[n].GlobalSegmentStart();
				
		const MT_Vector3 p = origin-m_end_effector;

		const MT_Vector3 x_axis(1,0,0);
		const MT_Vector3 y_axis(0,1,0);
		const MT_Vector3 z_axis(0,0,1);
		
		MT_Vector3 a = basis * x_axis;
		MT_Vector3 pca = p.cross(a);

		m_t_jacobian(n*3 + 1,1) = pca.x();
		m_t_jacobian(n*3 + 1,2) = pca.y();
		m_t_jacobian(n*3 + 1,3) = pca.z();

		a = basis * y_axis;
		pca = p.cross(a);

		m_t_jacobian(n*3 + 2,1) = pca.x();
		m_t_jacobian(n*3 + 2,2) = pca.y();
		m_t_jacobian(n*3 + 2,3) = pca.z();

		a = basis * z_axis;
		pca = p.cross(a);

		m_t_jacobian(n*3 + 3,1) = pca.x();
		m_t_jacobian(n*3 + 3,2) = pca.y();
		m_t_jacobian(n*3 + 3,3) = pca.z();
#else
		// user slower general jacobian computation method

		MT_Vector3 j1 = segs[n].ComputeJacobianColumn(0);

		m_t_jacobian(n*3 + 1,1) = j1.x();
		m_t_jacobian(n*3 + 1,2) = j1.y();
		m_t_jacobian(n*3 + 1,3) = j1.z();

		j1 = segs[n].ComputeJacobianColumn(1);

		m_t_jacobian(n*3 + 2,1) = j1.x();
		m_t_jacobian(n*3 + 2,2) = j1.y();
		m_t_jacobian(n*3 + 2,3) = j1.z();

		j1 = segs[n].ComputeJacobianColumn(2);

		m_t_jacobian(n*3 + 3,1) = j1.x();
		m_t_jacobian(n*3 + 3,2) = j1.y();
		m_t_jacobian(n*3 + 3,3) = j1.z();
#endif



	}


	// get the origina1 jacobain

	TNT::transpose(m_t_jacobian,m_jacobian);
};

	MT_Vector3 
IK_QChain::
EndEffector(
) const {
	return m_end_effector;
};

	MT_Vector3 
IK_QChain::
EndPose(
) const {
	return m_end_pose;
};
		

	int
IK_QChain::
DoF(
) const {
	return 3 * m_segments.size();
}

const 
	MT_Scalar
IK_QChain::
MaxExtension(
) const {

	vector<IK_QSegment>::const_iterator s_end = m_segments.end();
	vector<IK_QSegment>::const_iterator s_it = m_segments.begin();

	if (m_segments.size() == 0) return MT_Scalar(0);

	MT_Scalar output = s_it->Length();
	
	++s_it	;
	for (; s_it != s_end; ++s_it) {
		output += s_it->MaxExtension();
	}
	return output;
}

	










	







