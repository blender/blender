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

#ifndef NAN_INCLUDED_IK_QJacobianSolver_h

#define NAN_INCLUDED_IK_QJacobianSolver_h

/**
 * @author Laurence Bourn
 * @date 28/6/2001
 */

#include "TNT/cmat.h"
#include <vector>
#include "MT_Vector3.h"
#include "IK_QChain.h"

class IK_QJacobianSolver {

public :

	/**
	 * Create a new IK_QJacobianSolver on the heap
	 * @return A newly created IK_QJacobianSolver you take ownership of the object
     * and responsibility for deleting it
	 */


	static 
		IK_QJacobianSolver *
	New(
	);

	/**
	 * Compute a solution for a chain.
     * @param chain Reference to the chain to modify
	 * @param g_position Reference to the goal position.
	 * @param g_pose -not used- Reference to the goal pose. 
	 * @param tolerance The maximum allowed distance between solution
     * and goal for termination.
	 * @param max_iterations should be in the range (50 - 500) 
     * @param max_angle_change The maximum change in the angle vector 
     * of the chain (0.1 is a good value)
	 *
     * @return True iff goal position reached.
     */

		bool
	Solve(
		IK_QChain &chain,
		const MT_Vector3 &g_position,
		const MT_Vector3 &g_pose,
		const MT_Scalar tolerance,
		const int max_iterations,
		const MT_Scalar max_angle_change
	);

	~IK_QJacobianSolver(
	);


private :

	/**
	 * Private constructor to force use of New()
	 */

	IK_QJacobianSolver(
	);


	/** 
	 * Compute the inverse jacobian matrix of the chain.
	 * Uses a damped least squares solution when the matrix is 
	 * is approaching singularity
     */

		int
	ComputeInverseJacobian(
		IK_QChain &chain,
		const MT_Scalar x_length,
		const MT_Scalar max_angle_change
	);

		void
	ComputeBetas(
		IK_QChain &chain,
		const MT_Vector3 d_pos
	);

	/** 
	 * Updates the angles of the chain with the newly
	 * computed values
     */

		void
	UpdateChain(
		IK_QChain &chain
	);
	
	/**
	 * Make sure all the matrices are of the correct size
     */

		void
	ArmMatrices(
		int dof
	);

	/**
	 * Quick check to see if all the segments are parallel
	 * to the goal direction.
	 */

		bool
	ParallelCheck(
		const IK_QChain &chain,
		const MT_Vector3 goal
	) const;
	


private :

	/// the vector of intermediate betas
	TNT::Vector<MT_Scalar> m_beta;

	/// the vector of computed angle changes
	TNT::Vector<MT_Scalar> m_d_theta;

	/// the constraint gradients for each angle.
	TNT::Vector<MT_Scalar> m_dh;

	/// Space required for SVD computation

	TNT::Vector<MT_Scalar> m_svd_w;
    TNT::Vector<MT_Scalar> m_svd_work_space;        
	TNT::Matrix<MT_Scalar> m_svd_v;
	TNT::Matrix<MT_Scalar> m_svd_u;

	TNT::Matrix<MT_Scalar> m_svd_w_diag;
	TNT::Matrix<MT_Scalar> m_svd_v_t;
	TNT::Matrix<MT_Scalar> m_svd_u_t;
	TNT::Matrix<MT_Scalar> m_svd_inverse;
	TNT::Matrix<MT_Scalar> m_svd_temp1;


};

#endif

