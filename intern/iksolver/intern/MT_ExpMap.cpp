/**
 * $Id$
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
 * Original Author: Laurence
 * Contributor(s): Brecht
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MT_ExpMap.h"

/** 
 * Set the exponential map from a quaternion. The quaternion must be non-zero.
 */

	void
MT_ExpMap::
setRotation(
	const MT_Quaternion &q
) {
	// ok first normalize the quaternion
	// then compute theta the axis-angle and the normalized axis v
	// scale v by theta and that's it hopefully!

	m_q = q.normalized();
	m_v = MT_Vector3(m_q.x(), m_q.y(), m_q.z());

	MT_Scalar cosp = m_q.w();
	m_sinp = m_v.length();
	m_v /= m_sinp;

	m_theta = atan2(double(m_sinp),double(cosp));
	m_v *= m_theta;
}
 	
/** 
 * Convert from an exponential map to a quaternion
 * representation
 */	

	const MT_Quaternion&
MT_ExpMap::
getRotation(
) const {
	return m_q;
}
	
/** 
 * Convert the exponential map to a 3x3 matrix
 */

	MT_Matrix3x3
MT_ExpMap::
getMatrix(
) const {
	return MT_Matrix3x3(m_q);
}

/** 
 * Update & reparameterizate the exponential map
 */

	void
MT_ExpMap::
update(
	const MT_Vector3& dv
){
	m_v += dv;

	angleUpdated();
}

/**
 * Compute the partial derivatives of the exponential
 * map  (dR/de - where R is a 3x3 rotation matrix formed 
 * from the map) and return them as a 3x3 matrix
 */

	void
MT_ExpMap::
partialDerivatives(
	MT_Matrix3x3& dRdx,
	MT_Matrix3x3& dRdy,
	MT_Matrix3x3& dRdz
) const {
	
	MT_Quaternion dQdx[3];

	compute_dQdVi(dQdx);

	compute_dRdVi(dQdx[0], dRdx);
	compute_dRdVi(dQdx[1], dRdy);
	compute_dRdVi(dQdx[2], dRdz);
}

	void
MT_ExpMap::
compute_dRdVi(
	const MT_Quaternion &dQdvi,
	MT_Matrix3x3 & dRdvi
) const {

	MT_Scalar  prod[9];
	
	/* This efficient formulation is arrived at by writing out the
	 * entire chain rule product dRdq * dqdv in terms of 'q' and 
	 * noticing that all the entries are formed from sums of just
	 * nine products of 'q' and 'dqdv' */

	prod[0] = -MT_Scalar(4)*m_q.x()*dQdvi.x();
	prod[1] = -MT_Scalar(4)*m_q.y()*dQdvi.y();
	prod[2] = -MT_Scalar(4)*m_q.z()*dQdvi.z();
	prod[3] = MT_Scalar(2)*(m_q.y()*dQdvi.x() + m_q.x()*dQdvi.y());
	prod[4] = MT_Scalar(2)*(m_q.w()*dQdvi.z() + m_q.z()*dQdvi.w());
	prod[5] = MT_Scalar(2)*(m_q.z()*dQdvi.x() + m_q.x()*dQdvi.z());
	prod[6] = MT_Scalar(2)*(m_q.w()*dQdvi.y() + m_q.y()*dQdvi.w());
	prod[7] = MT_Scalar(2)*(m_q.z()*dQdvi.y() + m_q.y()*dQdvi.z());
	prod[8] = MT_Scalar(2)*(m_q.w()*dQdvi.x() + m_q.x()*dQdvi.w());

	/* first row, followed by second and third */
	dRdvi[0][0] = prod[1] + prod[2];
	dRdvi[0][1] = prod[3] - prod[4];
	dRdvi[0][2] = prod[5] + prod[6];

	dRdvi[1][0] = prod[3] + prod[4];
	dRdvi[1][1] = prod[0] + prod[2];
	dRdvi[1][2] = prod[7] - prod[8];

	dRdvi[2][0] = prod[5] - prod[6];
	dRdvi[2][1] = prod[7] + prod[8];
	dRdvi[2][2] = prod[0] + prod[1];
}

// compute partial derivatives dQ/dVi

	void
MT_ExpMap::
compute_dQdVi(
	MT_Quaternion *dQdX
) const {

	/* This is an efficient implementation of the derivatives given
	 * in Appendix A of the paper with common subexpressions factored out */

	MT_Scalar sinc, termCoeff;

	if (m_theta < MT_EXPMAP_MINANGLE) {
		sinc = 0.5 - m_theta*m_theta/48.0;
		termCoeff = (m_theta*m_theta/40.0 - 1.0)/24.0;
	}
	else {
		MT_Scalar cosp = m_q.w();
		MT_Scalar ang = 1.0/m_theta;

		sinc = m_sinp*ang;
		termCoeff = ang*ang*(0.5*cosp - sinc);
	}

	for (int i = 0; i < 3; i++) {
		MT_Quaternion& dQdx = dQdX[i];
		int i2 = (i+1)%3;
		int i3 = (i+2)%3;

		MT_Scalar term = m_v[i]*termCoeff;
		
		dQdx[i] = term*m_v[i] + sinc;
		dQdx[i2] = term*m_v[i2];
		dQdx[i3] = term*m_v[i3];
		dQdx.w() = -0.5*m_v[i]*sinc;
	}
}

// reParametize away from singularity, updating
// m_v and m_theta

	void
MT_ExpMap::
reParametrize(
){
	if (m_theta > MT_PI) {
		MT_Scalar scl = m_theta;
		if (m_theta > MT_2_PI){	/* first get theta into range 0..2PI */
			m_theta = MT_Scalar(fmod(m_theta, MT_2_PI));
			scl = m_theta/scl;
			m_v *= scl;
		}
		if (m_theta > MT_PI){
			scl = m_theta;
			m_theta = MT_2_PI - m_theta;
			scl = MT_Scalar(1.0) - MT_2_PI/scl;
			m_v *= scl;
		}
	}
}

// compute cached variables

	void
MT_ExpMap::
angleUpdated(
){
	m_theta = m_v.length();

	reParametrize();

	// compute quaternion, sinp and cosp

	if (m_theta < MT_EXPMAP_MINANGLE) {
		m_sinp = MT_Scalar(0.0);

		/* Taylor Series for sinc */
		MT_Vector3 temp = m_v * MT_Scalar(MT_Scalar(.5) - m_theta*m_theta/MT_Scalar(48.0));
		m_q.x() = temp.x();
		m_q.y() = temp.y();
		m_q.z() = temp.z();
		m_q.w() = MT_Scalar(1.0);
	} else {
		m_sinp = MT_Scalar(sin(.5*m_theta));

		/* Taylor Series for sinc */
		MT_Vector3 temp = m_v * (m_sinp/m_theta);
		m_q.x() = temp.x();
		m_q.y() = temp.y();
		m_q.z() = temp.z();
		m_q.w() = MT_Scalar(cos(.5*m_theta));
	}
}

