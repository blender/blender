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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "IK_QSegment.h"
#include <iostream>

IK_QSegment::
IK_QSegment (
	const MT_Point3 tr1,
	const MT_Matrix3x3 A,
	const MT_Scalar length,
	const MT_ExpMap q
) :
	m_length (length),
	m_q (q)	
{	

	m_max_extension = tr1.length() + length;	

	m_transform.setOrigin(tr1);
	m_transform.setBasis(A);

	UpdateLocalTransform();
};
	

IK_QSegment::
IK_QSegment (
) :
	m_length(0),
	m_q (0,0,0),
	m_max_extension(0)
{
	// Intentionally empty
}



// accessors
////////////

// The length of the segment 

const
	MT_Scalar
IK_QSegment::
Length(
) const {
	return m_length;
}

const 
	MT_Scalar
IK_QSegment::
MaxExtension(
) const {
	return m_max_extension;
}
	
// This is the transform from adjacent
// coordinate systems in the chain.

const 
	MT_Transform &
IK_QSegment::
LocalTransform(
) const {
	return m_local_transform;
}

const
	MT_ExpMap &
IK_QSegment::
LocalJointParameter(
) const {
	return m_q;
}

	MT_Transform  
IK_QSegment::
UpdateGlobal(
	const MT_Transform & global
){
	// compute the global transform
	// and the start of the segment in global coordinates.
	
	m_seg_start = global * m_transform.getOrigin();
	m_global_transform = global;

	const MT_Transform new_global = GlobalTransform();
		
	m_seg_end = new_global.getOrigin();

	return new_global;
}

	MT_Transform
IK_QSegment::
GlobalTransform(
) const {
	return m_global_transform * m_local_transform;
}
	

	MT_Transform
IK_QSegment::
UpdateAccumulatedLocal(
	const MT_Transform & acc_local
){
	m_accum_local = acc_local;
	return m_local_transform * m_accum_local;
}		

const 
	MT_Transform &
IK_QSegment::
AccumulatedLocal(
) const {
	return m_accum_local;
}

	MT_Vector3 	
IK_QSegment::
ComputeJacobianColumn(
	int angle
) const {


	MT_Transform translation;
	translation.setIdentity();
	translation.translate(MT_Vector3(0,m_length,0));


	// we can compute the jacobian for one of the
	// angles of this joint by first computing 
	// the partial derivative of the local transform dR/da
	// and then computing
	// dG/da = m_global_transform * dR/da * m_accum_local (0,0,0)

#if 0

	// use euler angles as a test of the matrices and this method.

	MT_Matrix3x3 dRda;

	MT_Quaternion rotx,roty,rotz;

	rotx.setRotation(MT_Vector3(1,0,0),m_q[0]);
	roty.setRotation(MT_Vector3(0,1,0),m_q[1]);
	rotz.setRotation(MT_Vector3(0,0,1),m_q[2]);

	MT_Matrix3x3 rotx_m(rotx);
	MT_Matrix3x3 roty_m(roty);
	MT_Matrix3x3 rotz_m(rotz);
 
	if (angle == 0) {
	
		MT_Scalar ci = cos(m_q[0]);
		MT_Scalar si = sin(m_q[1]);

		dRda = MT_Matrix3x3(
			0,  0,  0,
			0,-si,-ci,
			0, ci,-si
		);

		dRda = rotz_m * roty_m * dRda;
	} else 
	
	if (angle == 1) {
	
		MT_Scalar cj = cos(m_q[1]);
		MT_Scalar sj = sin(m_q[1]);

		dRda = MT_Matrix3x3(
			-sj,  0, cj,
			  0,  0,  0,
			-cj,  0,-sj
		);

		dRda = rotz_m * dRda * rotx_m;
	} else 
	
	if (angle == 2) {
	
		MT_Scalar ck = cos(m_q[2]);
		MT_Scalar sk = sin(m_q[2]);

		dRda = MT_Matrix3x3(
			-sk,-ck,  0,
			 ck,-sk,  0,
			  0,  0,  0
		);

		dRda = dRda * roty_m * rotx_m;
	}
		
	MT_Transform dRda_t(MT_Point3(0,0,0),dRda);

	// convert to 4x4 matrices coz dRda is singular.
	MT_Matrix4x4 dRda_m(dRda_t);
	dRda_m[3][3] = 0;

#else

	// use exponential map derivatives
	MT_Matrix4x4 dRda_m = m_q.partialDerivatives(angle);

#endif	

	
	// Once we have computed the local derivatives we can compute 
	// derivatives of the end effector. 

	// Imagine a chain consisting of 5 segments each with local
	// transformation Li
	// Then the global transformation G is L1 *L2 *L3 *L4 *L5
	// If we want to compute the partial derivatives of this expression
	// w.r.t one of the angles x of L3 we should then compute 
	// dG/dx	= d(L1 *L2 *L3 *L4 *L5)/dx
	//			= L1 *L2 * dL3/dx *L4 *L5
	// but L1 *L2 is the global transformation of the parent of this
	// bone and L4 *L5 is the accumulated local transform of the children
	// of this bone (m_accum_local)
	// so dG/dx = m_global_transform * dL3/dx * m_accum_local
	// 
	// so now we need to compute dL3/dx 
	// L3 = m_transform * rotation(m_q) * translate(0,m_length,0)
	// do using the same procedure we get
	// dL3/dx = m_transform * dR/dx * translate(0,m_length,0)
	// dR/dx is the partial derivative of the exponential map w.r.t 
	// one of it's parameters. This is computed in MT_ExpMap

	MT_Matrix4x4 translation_m (translation);
	MT_Matrix4x4 global_m(m_global_transform);
	MT_Matrix4x4 accum_local_m(m_accum_local);
	MT_Matrix4x4 transform_m(m_transform);

		
	MT_Matrix4x4 dFda_m = global_m * transform_m * dRda_m * translation_m * accum_local_m;

	MT_Vector4 result = dFda_m * MT_Vector4(0,0,0,1);
	return MT_Vector3(result[0],result[1],result[2]);
};



const 	
	MT_Vector3 &
IK_QSegment::
GlobalSegmentStart(
) const{
	return m_seg_start;
}

const 	
	MT_Vector3 &
IK_QSegment::
GlobalSegmentEnd(
) const {
	return m_seg_end;
}


	void
IK_QSegment::
IncrementAngle(
	const MT_Vector3 &dq
){
	m_q.vector() += dq;
	MT_Scalar theta(0);
	m_q.reParameterize(theta);

	UpdateLocalTransform();
}
 

	void
IK_QSegment::
SetAngle(
	const MT_ExpMap &dq
){
	m_q = dq;
	UpdateLocalTransform();
}


	void
IK_QSegment::
UpdateLocalTransform(
){
#if 0

	//use euler angles - test 
	MT_Quaternion rotx,roty,rotz;

	rotx.setRotation(MT_Vector3(1,0,0),m_q[0]);
	roty.setRotation(MT_Vector3(0,1,0),m_q[1]);
	rotz.setRotation(MT_Vector3(0,0,1),m_q[2]);

	MT_Matrix3x3 rotx_m(rotx);
	MT_Matrix3x3 roty_m(roty);
	MT_Matrix3x3 rotz_m(rotz);

	MT_Matrix3x3 rot = rotz_m * roty_m * rotx_m;
#else

	//use exponential map 
	MT_Matrix3x3 rot = m_q.getMatrix();	

	
#endif

	MT_Transform rx(MT_Point3(0,0,0),rot);

	MT_Transform translation;
	translation.setIdentity();
	translation.translate(MT_Vector3(0,m_length,0));

	m_local_transform = m_transform * rx * translation;
};






