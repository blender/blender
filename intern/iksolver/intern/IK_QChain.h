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

#ifndef NAN_INCLUDED_IK_QChain_h
#define NAN_INCLUDED_IK_QChain_h


#include "IK_QSegment.h"
#include <vector>
#include "MT_Scalar.h"
#include "TNT/cmat.h"

/**
 * This class is a collection of ordered segments that are used
 * in an Inverse Kinematic solving routine. An IK solver operating
 * on the chain, will in general manipulate all the segments of the
 * chain in order to solve the IK problem.
 * 
 * To build a chain use the default constructor. Once built it's
 * then possible to add IK_Segments to the chain by inserting
 * them into the vector of IK_Segments. Note that segments will be
 * copied into the chain so chains cannot share instances of 
 * IK_Segments. 
 * 
 * You have full control of which segments form the chain via the 
 * the std::vector routines. 
 */

class IK_QChain{

public :

	/**
	 * Construct a IK_QChain with no segments.
	 */

	IK_QChain(
	);

	// IK_QChains also have the default copy constructors
	// available. 

	/** 
     * Const access to the array of segments
	 * comprising the IK_QChain. Used for rendering
	 * etc
     * @return a const reference to a vector of segments
	 */

	const 
		std::vector<IK_QSegment> &
	Segments(
	) const ;


	/**
	 * Full access to segments used to initialize
	 * the IK_QChain and manipulate the segments.
	 * Use the push_back() method of std::vector to add
     * segments in order to the chain
     * @return a reference to a vector of segments
	 */

		std::vector<IK_QSegment> &
	Segments(
	);


	/** 
	 * Force the IK_QChain to recompute all the local 
	 * segment transformations and composite them
	 * to calculate the global transformation for 
	 * each segment. Must be called before 
	 * ComputeJacobian()
     */

		void
	UpdateGlobalTransformations(
	);

	/** 
	 * Return the global position of the end
	 * of the last segment.
	 */

		MT_Vector3 
	EndEffector(
	) const;


	/** 
	 * Return the global pose of the end
	 * of the last segment.
	 */

		MT_Vector3
	EndPose(
	) const;


	/** 
	 * Calculate the jacobian matrix for
	 * the current end effector position.
	 * A jacobian is the set of column vectors
	 * of partial derivatives for each active angle.
	 * This method also computes the transposed jacobian.
	 * @pre You must have updated the global transformations
	 * of the chain's segments before a call to this method. Do this
	 * with UpdateGlobalTransformation()
	 */

		void
	ComputeJacobian(
	);
	

	/** 
	 * @return A const reference to the last computed jacobian matrix
	 */

	const 
		TNT::Matrix<MT_Scalar> &
	Jacobian(
	) const ;

	/** 
	 * @return A const reference to the last computed transposed jacobian matrix
	 */

	const 
		TNT::Matrix<MT_Scalar> &
	TransposedJacobian(
	) const ;

	/** 
	 * Count the degrees of freedom in the IK_QChain
	 * @warning store this value rather than using this function
	 * as the termination value of a for loop etc.
	 */

		int
	DoF(
	) const;

	/** 
	 * Compute the maximum extension of the chain from the
	 * root segment. This is the length of the root segments
	 * + the max extensions of all the other segments
	 */
	
	const 
		MT_Scalar
	MaxExtension(
	) const;


private :

	/// The vector of segments comprising the chain
	std::vector<IK_QSegment> m_segments;

	/// The jacobain of the IK_QChain
	TNT::Matrix<MT_Scalar> m_jacobian;

	/// It's transpose
	TNT::Matrix<MT_Scalar> m_t_jacobian;

	MT_Vector3 m_end_effector;		
	MT_Vector3 m_end_pose;


};


#endif