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

#include "IK_JacobianSolver.h"

#include "TNT/svd.h"

using namespace std;

	IK_JacobianSolver *
IK_JacobianSolver::
New(
){
	return new IK_JacobianSolver();
}

	bool
IK_JacobianSolver::
Solve(
	IK_Chain &chain,
	const MT_Vector3 &g_position,
	const MT_Vector3 &g_pose,
	const MT_Scalar tolerance,
	const int max_iterations,
	const MT_Scalar max_angle_change
){

	ArmMatrices(chain.DoF());

	for (int iterations = 0; iterations < max_iterations; iterations++) {
		

		MT_Vector3 end_effector = chain.EndEffector();

		MT_Vector3 d_pos = g_position - end_effector;
		const MT_Scalar x_length = d_pos.length();
		
		if (x_length < tolerance) {
			return true;
		}	

		chain.ComputeJacobian();
		ComputeInverseJacobian(chain,x_length,max_angle_change);
		
		ComputeBetas(chain,d_pos);
		UpdateChain(chain);
		chain.UpdateGlobalTransformations();
	}

	return false;
};

IK_JacobianSolver::
~IK_JacobianSolver(
){
	// nothing to do
}


IK_JacobianSolver::
IK_JacobianSolver(
){
	// nothing to do
};
 
	void
IK_JacobianSolver::
ComputeBetas(
	IK_Chain &chain,
	const MT_Vector3 d_pos
){	

	m_beta = 0;

	m_beta[0] = d_pos.x();
	m_beta[1] = d_pos.y();
	m_beta[2] = d_pos.z();
	
	TNT::matmult(m_d_theta,m_svd_inverse,m_beta);

};	


	int
IK_JacobianSolver::
ComputeInverseJacobian(
	IK_Chain &chain,
	const MT_Scalar x_length,
	const MT_Scalar max_angle_change
) {

	int dimension = 0;

	m_svd_u = 0;

	// copy first 3 rows of jacobian into m_svd_u

	int row, column;

	for (row = 0; row < 3; row ++) {
		for (column = 0; column < chain.Jacobian().num_cols(); column ++) {
			m_svd_u[row][column] = chain.Jacobian()[row][column];
		}
	} 

	m_svd_w = 0;
	m_svd_v = 0;

	TNT::SVD_a(m_svd_u,m_svd_w,m_svd_v);	

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

	MT_Scalar epsilon = 1e-10;

	for ( i = 0; i <m_svd_w.size() ; i++) {

		if (m_svd_w[i] > epsilon && m_svd_w[i] < w_min) {
			w_min = m_svd_w[i];
		}
	}
	MT_Scalar lambda = 0;

	MT_Scalar d = x_length/max_angle_change;

	if (w_min <= d/2) {
		lambda = d/2;
	} else 
	if (w_min < d) {
		lambda = sqrt(w_min*(d - w_min));
	} else {
		lambda = 0;
	}

	lambda *= lambda;

	for (i= 0; i < m_svd_w.size(); i++) {
		if (m_svd_w[i] < epsilon) {
			m_svd_w[i] = 0;
		} else {
			m_svd_w[i] = m_svd_w[i] / (m_svd_w[i] * m_svd_w[i] + lambda);
		}
	}

	m_svd_w_diag.diagonal(m_svd_w);

	// FIXME optimize these matrix multiplications
	// using the fact that m_svd_w_diag is diagonal!

	TNT::matmult(m_svd_temp1,m_svd_w_diag,m_svd_u_t);
	TNT::matmult(m_svd_inverse,m_svd_v,m_svd_temp1);
	return dimension;

}

	void
IK_JacobianSolver::
UpdateChain(
	IK_Chain &chain
){

	// iterate through the set of angles and 
	// update their values from the d_thetas

	int n;
	vector<IK_Segment> &segs = chain.Segments(); 
	
	int chain_dof = chain.DoF();
	int seg_ind = 0;
	for (n=0; n < chain_dof;seg_ind ++) {
		n += segs[seg_ind].IncrementAngles(m_d_theta.begin() + n);
	}
};
	
	void
IK_JacobianSolver::
ArmMatrices(
	int dof
){

	m_beta.newsize(dof);
	m_d_theta.newsize(dof);

	m_svd_u.newsize(dof,dof);
	m_svd_v.newsize(dof,dof);
	m_svd_w.newsize(dof);

	m_svd_u = 0;
	m_svd_v = 0;
	m_svd_w = 0;

	m_svd_u_t.newsize(dof,dof);
	m_svd_v_t.newsize(dof,dof);
	m_svd_w_diag.newsize(dof,dof);
	m_svd_inverse.newsize(dof,dof);
	m_svd_temp1.newsize(dof,dof);

};
















	
