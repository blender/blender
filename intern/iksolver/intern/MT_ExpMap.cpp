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

#include "MT_ExpMap.h"

/** 
 * Set the exponential map from a quaternion. The quaternion must be non-zero.
 */

	void
MT_ExpMap::
setRotation(
	const MT_Quaternion &q
) {
	// ok first normailize the quaternion
	// then compute theta the axis-angle and the normalized axis v
	// scale v by theta and that's it hopefully!

	MT_Quaternion qt = q.normalized();

	MT_Vector3 axis(qt.x(),qt.y(),qt.z());
	MT_Scalar cosp = qt.w();
	MT_Scalar sinp = axis.length();
	axis /= sinp;

	MT_Scalar theta = atan2(double(sinp),double(cosp));

	axis *= theta;	
	m_v = axis;
}
 	
/** 
 * Convert from an exponential map to a quaternion
 * representation
 */	

	MT_Quaternion 
MT_ExpMap::
getRotation(
) const {
    MT_Scalar  cosp, sinp, theta;

	MT_Quaternion q;

	theta = m_v.length();
    
    cosp = MT_Scalar(cos(.5*theta));
    sinp = MT_Scalar(sin(.5*theta));

    q.w() = cosp;

    if (theta < MT_EXPMAP_MINANGLE) {

		MT_Vector3 temp = m_v * MT_Scalar(MT_Scalar(.5) - theta*theta/MT_Scalar(48.0)); /* Taylor Series for sinc */
		q.x() = temp.x();
		q.y() = temp.y();
		q.z() = temp.z();
	} else {
		MT_Vector3 temp = m_v * (sinp/theta); /* Taylor Series for sinc */
		q.x() = temp.x();
		q.y() = temp.y();
		q.z() = temp.z();
	}

    return q;
}
	
/** 
 * Convert the exponential map to a 3x3 matrix
 */

	MT_Matrix3x3
MT_ExpMap::
getMatrix(
) const {

	MT_Quaternion q = getRotation();
	return MT_Matrix3x3(q);
}




/** 
 * Force a reparameterization of the exponential
 * map.
 */

	bool
MT_ExpMap::
reParameterize(
	MT_Scalar &theta
){
    bool rep(false);
    theta = m_v.length();

    if (theta > MT_PI){
		MT_Scalar scl = theta;
		if (theta > MT_2_PI){	/* first get theta into range 0..2PI */
			theta = MT_Scalar(fmod(theta, MT_2_PI));
			scl = theta/scl;
			m_v *= scl;
			rep = true;
		}
		if (theta > MT_PI){
			scl = theta;
			theta = MT_2_PI - theta;
			scl = MT_Scalar(1.0) - MT_2_PI/scl;
			m_v *= scl;
			rep = true;
		}
    }
	return rep;

}

/**
 * Compute the partial derivatives of the exponential
 * map  (dR/de - where R is a 4x4 rotation matrix formed 
 * from the map) and return them as a 4x4 matrix
 */

	MT_Matrix4x4
MT_ExpMap::
partialDerivatives(
	const int i
) const {
	
	MT_Quaternion q = getRotation();
	MT_Quaternion dQdx;

	MT_Matrix4x4 output;

	compute_dQdVi(i,dQdx);
	compute_dRdVi(q,dQdx,output);

	return output;
}

	void
MT_ExpMap::
compute_dRdVi(
	const MT_Quaternion &q,
	const MT_Quaternion &dQdvi,
	MT_Matrix4x4 & dRdvi
) const {

    MT_Scalar  prod[9];
    
    /* This efficient formulation is arrived at by writing out the
     * entire chain rule product dRdq * dqdv in terms of 'q' and 
     * noticing that all the entries are formed from sums of just
     * nine products of 'q' and 'dqdv' */

    prod[0] = -MT_Scalar(4)*q.x()*dQdvi.x();
    prod[1] = -MT_Scalar(4)*q.y()*dQdvi.y();
    prod[2] = -MT_Scalar(4)*q.z()*dQdvi.z();
    prod[3] = MT_Scalar(2)*(q.y()*dQdvi.x() + q.x()*dQdvi.y());
    prod[4] = MT_Scalar(2)*(q.w()*dQdvi.z() + q.z()*dQdvi.w());
    prod[5] = MT_Scalar(2)*(q.z()*dQdvi.x() + q.x()*dQdvi.z());
    prod[6] = MT_Scalar(2)*(q.w()*dQdvi.y() + q.y()*dQdvi.w());
    prod[7] = MT_Scalar(2)*(q.z()*dQdvi.y() + q.y()*dQdvi.z());
    prod[8] = MT_Scalar(2)*(q.w()*dQdvi.x() + q.x()*dQdvi.w());

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

    /* the 4th row and column are all zero */
    int       i;

    for (i=0; i<3; i++)	
      dRdvi[3][i] = dRdvi[i][3] = MT_Scalar(0);
    dRdvi[3][3] = 0;
}

// compute partial derivatives dQ/dVi

	void
MT_ExpMap::
compute_dQdVi(
	const int i,
	MT_Quaternion & dQdX
) const {

    MT_Scalar   theta = m_v.length();
    MT_Scalar   cosp(cos(MT_Scalar(.5)*theta)), sinp(sin(MT_Scalar(.5)*theta));
    
    MT_assert(i>=0 && i<3);

    /* This is an efficient implementation of the derivatives given
     * in Appendix A of the paper with common subexpressions factored out */
    if (theta < MT_EXPMAP_MINANGLE){
		const int i2 = (i+1)%3, i3 = (i+2)%3;
		MT_Scalar Tsinc = MT_Scalar(0.5) - theta*theta/MT_Scalar(48.0);
		MT_Scalar vTerm = m_v[i] * (theta*theta/MT_Scalar(40.0) - MT_Scalar(1.0)) / MT_Scalar(24.0);
		
		dQdX.w() = -.5*m_v[i]*Tsinc;
		dQdX[i]  = m_v[i]* vTerm + Tsinc;
		dQdX[i2] = m_v[i2]*vTerm;
		dQdX[i3] = m_v[i3]*vTerm;
    } else {
		const int i2 = (i+1)%3, i3 = (i+2)%3;
		const MT_Scalar  ang = 1.0/theta, ang2 = ang*ang*m_v[i], sang = sinp*ang;
		const MT_Scalar  cterm = ang2*(.5*cosp - sang);
		
		dQdX[i]  = cterm*m_v[i] + sang;
		dQdX[i2] = cterm*m_v[i2];
		dQdX[i3] = cterm*m_v[i3];
		dQdX.w() = MT_Scalar(-.5)*m_v[i]*sang;
    }
}

 








