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

#ifndef NAN_INCLUDED_Segment_h

#define NAN_INCLUDED_Segment_h

/**
 * @author Laurence Bourn
 * @date 28/6/2001
 */


#include "MT_Vector3.h"
#include "MT_Transform.h"
#include <vector>

class IK_Segment {

public :

	/**
	 * Constructor.
     * @warning This class uses axis angles for it's parameterization.
     * Axis angles are a poor representation for joints of more than 1 DOF
     * because they suffer from Gimbal lock. This becomes noticeable in 
	 * IK solutions. A better solution is to do use a quaternion to represent
     * angles with 3 DOF 
     */	

	IK_Segment(
		const MT_Point3 tr1,
		const MT_Matrix3x3 A,
		const MT_Scalar length,
		const bool pitch_on,
		const bool yaw_on,
		const bool role_on
	);


	IK_Segment(
	);


	/**
     * @return The length of the segment
     */

	const
		MT_Scalar
	Length(
	) const ;

	/**
     * @return The transform from adjacent
	 * coordinate systems in the chain.
     */

	const 
		MT_Transform &
	LocalTransform(
	) const ;

	
	/** 
     * Get the segment to compute it's 
	 * global transform given the global transform
	 * of the parent. This method also updtes the
	 * global segment start
	 */

		void
	UpdateGlobal(
		const MT_Transform & global
	);
	
	/**
	 * @return A const reference to the global trnasformation 
	 */

	const 
		MT_Transform &
	GlobalTransform(
	) const;
		
	/**
	 * @return A const Reference to start of segment in global 
     * coordinates
	 */
	
	const 	
		MT_Vector3 &
	GlobalSegmentStart(
	) const;

	/**  
	 * Computes the number of degrees of freedom of this segment
	 */

		int
	DoF(
	) const;


	/**
	 * Increment the active angles (at most DoF()) by 
	 * d_theta. Which angles are incremented depends 
	 * on which are active. 
     * @return DoF()
	 * @warning Bad interface
     */

		int
	IncrementAngles(
		MT_Scalar *d_theta
	);

	
	// FIXME - interface bloat

	/**
     * @return the vectors about which the active
	 * angles operate
	 */

	const
		std::vector<MT_Vector3> &
	AngleVectors(
	) const;

	/**
	 * @return the ith active angle
	 */

		MT_Scalar
	ActiveAngle(
		int i
	) const;

	/**
	 * @return the ith angle
	 */
		MT_Scalar
	Angle(
		int i
	) const;


	/**
	 * Set the active angles from the array
	 * @return the number of active angles
	 */

		int
	SetAngles(
		const MT_Scalar *angles
	);


private :
	
		void
	UpdateLocalTransform(
	);



private :

	/** The user defined transformation, composition of the 
	 * translation and rotation from constructor.
     */

	MT_Transform m_transform;
	MT_Scalar m_angles[3];
	MT_Scalar m_length;

	MT_Transform m_local_transform;
	MT_Transform m_global_transform;

	bool m_active_angles[3];

	MT_Vector3 m_seg_start;

	std::vector<MT_Vector3> m_angle_vectors;

};

#endif