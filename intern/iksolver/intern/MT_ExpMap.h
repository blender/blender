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

#ifndef MT_ExpMap_H
#define MT_ExpMap_H

#include <MT_assert.h>

#include "MT_Vector3.h"
#include "MT_Quaternion.h"
#include "MT_Matrix4x4.h"

const MT_Scalar MT_EXPMAP_MINANGLE (1e-7);

/**
 * MT_ExpMap an exponential map parameterization of rotations
 * in 3D. This implementation is derived from the paper 
 * "F. Sebastian Grassia. Practical parameterization of 
 * rotations using the exponential map. Journal of Graphics Tools,
 *  3(3):29-48, 1998" Please go to http://www.acm.org/jgt/papers/Grassia98/
 * for a thorough description of the theory and sample code used 
 * to derive this class. 
 *
 * Basic overview of why this class is used.
 * In an IK system we need to paramterize the joint angles in some
 * way. Typically 2 parameterizations are used.
 * - Euler Angles
 * These suffer from singularities in the parameterization known
 * as gimbal lock. They also do not interpolate well. For every
 * set of euler angles there is exactly 1 corresponding 3d rotation.
 * - Quaternions.
 * Great for interpolating. Only unit quaternions are valid rotations
 * means that in a differential ik solver we often stray outside of
 * this manifold into invalid rotations. Means we have to do a lot
 * of nasty normalizations all the time. Does not suffer from 
 * gimbal lock problems. More expensive to compute partial derivatives
 * as there are 4 of them.
 * 
 * So exponential map is similar to a quaternion axis/angle 
 * representation but we store the angle as the length of the
 * axis. So require only 3 parameters. Means that all exponential
 * maps are valid rotations. Suffers from gimbal lock. But it's
 * possible to detect when gimbal lock is near and reparameterize
 * away from it. Also nice for interpolating.
 * Exponential maps are share some of the useful properties of
 * euler and quaternion parameterizations. And are very useful
 * for differential IK solvers.
 */

class MT_ExpMap {
public:

	/**
	 * Default constructor
	 * @warning there is no initialization in the
	 * default constructor
	 */ 

    MT_ExpMap() {}
    MT_ExpMap(const MT_Vector3& v) : m_v(v) {}

    MT_ExpMap(const float v[3]) : m_v(v) {}
    MT_ExpMap(const double v[3]) : m_v(v) {}

    MT_ExpMap(MT_Scalar x, MT_Scalar y, MT_Scalar z) :
        m_v(x, y, z) {}

	/** 
	 * Construct an exponential map from a quaternion
	 */

	MT_ExpMap(
		const MT_Quaternion &q
	) {
		setRotation(q);
	};

	/** 
	 * Accessors
	 * Decided not to inherit from MT_Vector3 but rather
	 * this class contains an MT_Vector3. This is because
	 * it is very dangerous to use MT_Vector3 functions
	 * on this class and some of them have no direct meaning.
	 */

		MT_Vector3 &
	vector(
	) {
		return m_v;
	};

	const 
		MT_Vector3 &
	vector(
	) const {
		return m_v;
	};

	/** 
	 * Set the exponential map from a quaternion
	 */

		void
	setRotation(
		const MT_Quaternion &q
	);

	/** 
	 * Convert from an exponential map to a quaternion
	 * representation
	 */	
	
		MT_Quaternion 
	getRotation(
	) const;

	/** 
	 * Convert the exponential map to a 3x3 matrix
	 */

		MT_Matrix3x3
	getMatrix(
	) const; 
	
	/** 
	 * Force a reparameterization check of the exponential
	 * map.
	 * @param theta returns the new axis-angle.
	 * @return true iff a reParameterization took place.
	 * Use this function whenever you adjust the  vector
	 * representing the exponential map.
	 */

		bool
	reParameterize(
		MT_Scalar &theta
	);

	/**
	 * Compute the partial derivatives of the exponential
	 * map  (dR/de - where R is a 4x4 matrix formed 
	 * from the map) and return them as a 4x4 matrix
	 */

		MT_Matrix4x4
	partialDerivatives(
		const int i
	) const ;
	
private :
	
	MT_Vector3 m_v;

	// private methods

	// Compute partial derivatives dR (3x3 rotation matrix) / dVi (EM vector)
	// given the partial derivative dQ (Quaternion) / dVi (ith element of EM vector)


		void
	compute_dRdVi(
		const MT_Quaternion &q,
		const MT_Quaternion &dQdV,
		MT_Matrix4x4 & dRdVi
	) const;

	// compute partial derivatives dQ/dVi

		void
	compute_dQdVi(
		int i,
		MT_Quaternion & dQdX
	) const ; 

		
};



#endif



