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
#ifndef NAN_INCLUDED_IK_QSegment_h
#define NAN_INCLUDED_IK_QSegment_h


#include "MT_Vector3.h"
#include "MT_Transform.h"
#include "MT_Matrix4x4.h"
#include "MT_ExpMap.h"

#include <vector>

/**
 * An IK_Qsegment encodes information about a segments
 * local coordinate system.
 * In these segments exponential maps are used to parameterize
 * the 3 DOF joints. Please see the file MT_ExpMap.h for more
 * information on this parameterization.
 * 
 * These segments always point along the y-axis.
 * 
 * Here wee define the local coordinates of a joint as
 * local_transform = 
 * translate(tr1) * rotation(A) * rotation(q) * translate(0,length,0)
 * We use the standard moto column ordered matrices. You can read
 * this as:
 * - first translate by (0,length,0)
 * - multiply by the rotation matrix derived from the current
 * angle parameterization q.
 * - multiply by the user defined matrix representing the rest
 * position of the bone.
 * - translate by the used defined translation (tr1)
 * The ordering of these transformations is vital, you must
 * use exactly the same transformations when displaying the segments
 */

class IK_QSegment {

public :

	/**
	 * Constructor.
	 * @param tr1 a user defined translation 
	 * @param a used defined rotation matrix representin
	 * the rest position of the bone.
	 * @param the length of the bone.
	 * @param an exponential map can also be used to 
	 * define the bone rest position.
	 */

	IK_QSegment(
		const MT_Point3 tr1,
		const MT_Matrix3x3 A,
		const MT_Scalar length,
		const MT_ExpMap q
	);

	/** 
	 * Default constructor
	 * Defines identity local coordinate system,
	 * with a bone length of 1.
	 */
	

	IK_QSegment(
	);


	/**
     * @return The length of the segment
     */

	const
		MT_Scalar
	Length(
	) const ;

	/**
	 * @return the max distance of the end of this
	 * bone from the local origin.
	 */

	const 
		MT_Scalar
	MaxExtension(
	) const ;

	/**
     * @return The transform from adjacent
	 * coordinate systems in the chain.
     */

	const 
		MT_Transform &
	LocalTransform(
	) const ;

	const
		MT_ExpMap &
	LocalJointParameter(
	) const;

	/**
	 * Update the global coordinates of this segment.
	 * @param global the global coordinates of the
	 * previous bone in the chain
	 * @return the global coordinates of this segment.
	 */

		MT_Transform
	UpdateGlobal(
		const MT_Transform & global
	);

	/**
	 * @return The global transformation 
	 */

		MT_Transform
	GlobalTransform(
	) const;
 
	
	/** 
	 * Update the accumulated local transform of this segment
	 * The accumulated local transform is the end effector
	 * transform in the local coordinates of this segment.
	 * @param acc_local the accumulated local transform of 
	 * the child of this bone.
	 * @return the accumulated local transorm of this segment
	 */
	
		MT_Transform
	UpdateAccumulatedLocal(
		const MT_Transform & acc_local
	);

	/**
	 * @return A const reference to accumulated local 
	 * transform of this segment.
	 */

	const 
		MT_Transform &
	AccumulatedLocal(
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
	 * @return A const Reference to end of segment in global 
     * coordinates
	 */

	const 
		MT_Vector3 &
	GlobalSegmentEnd(
	) const;


	/**
	 * @return the partial derivative of the end effector 
	 * with respect to one of the degrees of freedom of this 
	 * segment.
	 * @param angle the angle parameter you want to compute
	 * the partial derivatives for. For these segments this
	 * must be in the range [0,2]
	 */

		MT_Vector3 	
	ComputeJacobianColumn(
		int angle
	) const ;
		
	/**
	 * Explicitly set the angle parameterization value. 
	 */

		void
	SetAngle(
		const MT_ExpMap &q
	);

	/**
	 * Increment the angle parameterization value. 
	 */

		void
	IncrementAngle(
		const MT_Vector3 &dq
	);	


	/**
	 * Return the parameterization of this angle
	 */

	const 
		MT_ExpMap &
	ExpMap(
	) const {
		return m_q;
	};


private :
	
		void
	UpdateLocalTransform(
	);


private :

	/** 
	 * m_transform The user defined transformation, composition of the 
	 * translation and rotation from constructor.
     */
	MT_Transform m_transform;

	/**
	 * The exponential map parameterization of this joint.
	 */

	MT_Scalar m_length;
	MT_ExpMap m_q;

	/**
	 * The maximum extension of this segment
	 * This is the magnitude of the user offset + the length of the 
	 * chain 
	 */

	MT_Scalar m_max_extension;

	MT_Transform m_local_transform;
	MT_Transform m_global_transform;
	MT_Transform m_accum_local;

	MT_Vector3 m_seg_start;
	MT_Vector3 m_seg_end;

};

#endif

