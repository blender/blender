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

#include "IK_QJacobianSolver.h"

#include "TNT/svd.h"

using namespace std;

	IK_QJacobianSolver *
IK_QJacobianSolver::
New(
){
	return new IK_QJacobianSolver();
}

	bool
IK_QJacobianSolver::
Solve(
	IK_QChain &chain,
	const MT_Vector3 &g_position,
	const MT_Vector3 &g_pose,
	const MT_Scalar tolerance,
	const int max_iterations,
	const MT_Scalar max_angle_change
){

	const vector<IK_QSegment> & segs = chain.Segments();
	if (segs.size() == 0) return false;
	
	// compute the goal direction from the base of the chain
	// in global coordinates

	MT_Vector3 goal_dir = g_position - segs[0].GlobalSegmentStart();
	

	const MT_Scalar chain_max_extension = chain.MaxExtension();

	bool do_parallel_check(false);

	if (chain_max_extension < goal_dir.length()) {
		do_parallel_check = true;
	}

	goal_dir.normalize();


	ArmMatrices(chain.DoF());

	for (int iterations = 0; iterations < max_iterations; iterations++) {
		
		// check to see if the chain is pointing in the right direction

		if (iterations%32 && do_parallel_check && ParallelCheck(chain,goal_dir)) {

			return false;
		}
		
		MT_Vector3 end_effector = chain.EndEffector();
		MT_Vector3 d_pos = g_position - end_effector;
		const MT_Scalar x_length = d_pos.length();
		
		if (x_length < tolerance) {
			return true;
		}	

		chain.ComputeJacobian();

        try {
		    ComputeInverseJacobian(chain,x_length,max_angle_change);
        }
        catch(...) {
            return false;
        }
		
		ComputeBetas(chain,d_pos);
		UpdateChain(chain);
		chain.UpdateGlobalTransformations();
	}


	return false;
};

IK_QJacobianSolver::
~IK_QJacobianSolver(
){
	// nothing to do
}


IK_QJacobianSolver::
IK_QJacobianSolver(
){
	// nothing to do
};
 
	void
IK_QJacobianSolver::
ComputeBetas(
	IK_QChain &chain,
	const MT_Vector3 d_pos
){	

	m_beta = 0;

	m_beta[0] = d_pos.x();
	m_beta[1] = d_pos.y();
	m_beta[2] = d_pos.z();
	
	TNT::matmult(m_d_theta,m_svd_inverse,m_beta);

};	


	int
IK_QJacobianSolver::
ComputeInverseJacobian(
	IK_QChain &chain,
	const MT_Scalar x_length,
	const MT_Scalar max_angle_change
) {

	int dimension = 0;

	m_svd_u = MT_Scalar(0);

	// copy first 3 rows of jacobian into m_svd_u

	int row, column;

	for (row = 0; row < 3; row ++) {
		for (column = 0; column < chain.Jacobian().num_cols(); column ++) {
			m_svd_u[row][column] = chain.Jacobian()[row][column];
		}
	}

	m_svd_w = MT_Scalar(0);
	m_svd_v = MT_Scalar(0);

    m_svd_work_space = MT_Scalar(0);

	TNT::SVD(m_svd_u,m_svd_w,m_svd_v,m_svd_work_space);

	// invert the SVD and compute inverse

	TNT::transpose(m_svd_v,m_svd_v_t);
	TNT::transpose(m_svd_u,m_svd_u_t);

	// Compute damped least squares inverse of pseudo inverse
	// Compute damping term lambda

	// Note when lambda is zero this is equivalent to the
	// least squares solution. This is fine when the m_jjt is
	// of full rank. When m_jjt is near to singular the least squares
	// inverse tries to minimize |J(dtheta) - dX)| and doesn't 
	// try to minimize  dTheta. This results in eratic changes in angle.
	// Damped least squares minimizes |dtheta| to try and reduce this
	// erratic behaviour.

	// We don't want to use the damped solution everywhere so we
	// only increase lamda from zero as we approach a singularity.

	// find the smallest non-zero m_svd_w value

	int i = 0;

	MT_Scalar w_min = MT_INFINITY;

	// anything below epsilon is treated as zero

	MT_Scalar epsilon = MT_Scalar(1e-10);

	for ( i = 0; i <m_svd_w.size() ; i++) {

		if (m_svd_w[i] > epsilon && m_svd_w[i] < w_min) {
			w_min = m_svd_w[i];
		}
	}

	MT_Scalar lambda(0);

	MT_Scalar d = x_length/max_angle_change;

	if (w_min <= d/2) {
		lambda = d/2;
	} else
	if (w_min < d) {
		lambda = sqrt(w_min*(d - w_min));
	} else {
		lambda = MT_Scalar(0);
	}


	lambda *= lambda;

	for (i= 0; i < m_svd_w.size(); i++) {
		if (m_svd_w[i] < epsilon) {
			m_svd_w[i] = MT_Scalar(0);
		} else {
			m_svd_w[i] = m_svd_w[i] / (m_svd_w[i] * m_svd_w[i] + lambda);
		}
	}


	TNT::matmultdiag(m_svd_temp1,m_svd_w,m_svd_u_t);
	TNT::matmult(m_svd_inverse,m_svd_v,m_svd_temp1);

	return dimension;

}

	void
IK_QJacobianSolver::
UpdateChain(
	IK_QChain &chain
){

	// iterate through the set of angles and 
	// update their values from the d_thetas

	vector<IK_QSegment> &segs = chain.Segments(); 
	
	int seg_ind = 0;
	for (seg_ind = 0;seg_ind < segs.size(); seg_ind++) {
	
		MT_Vector3 dq;
		dq[0] = m_d_theta[3*seg_ind];
		dq[1] = m_d_theta[3*seg_ind + 1];
		dq[2] = m_d_theta[3*seg_ind + 2];
		segs[seg_ind].IncrementAngle(dq);	
	}

};
	
	void
IK_QJacobianSolver::
ArmMatrices(
	int dof
){

	m_beta.newsize(dof);
	m_d_theta.newsize(dof);

	m_svd_u.newsize(dof,dof);
	m_svd_v.newsize(dof,dof);
	m_svd_w.newsize(dof);

    m_svd_work_space.newsize(dof);

	m_svd_u = MT_Scalar(0);
	m_svd_v = MT_Scalar(0);
	m_svd_w = MT_Scalar(0);

	m_svd_u_t.newsize(dof,dof);
	m_svd_v_t.newsize(dof,dof);
	m_svd_w_diag.newsize(dof,dof);
	m_svd_inverse.newsize(dof,dof);
	m_svd_temp1.newsize(dof,dof);

};

	bool
IK_QJacobianSolver::
ParallelCheck(
	const IK_QChain &chain,
	const MT_Vector3 goal_dir
) const {
	
	// compute the start of the chain in global coordinates
	const vector<IK_QSegment> &segs = chain.Segments(); 

	int num_segs = segs.size();
	
	if (num_segs == 0) {
		return false;
	}

	MT_Scalar crossp_sum = 0;

	int i;
	for (i = 0; i < num_segs; i++) {
		MT_Vector3 global_seg_direction = segs[i].GlobalSegmentEnd() - 
			segs[i].GlobalSegmentStart();

		global_seg_direction.normalize();	

		MT_Scalar crossp = (global_seg_direction.cross(goal_dir)).length();
		crossp_sum += MT_Scalar(fabs(crossp));
	}

	if (crossp_sum < MT_Scalar(0.01)) {
		return true;
	} else {
		return false;
	}
}


