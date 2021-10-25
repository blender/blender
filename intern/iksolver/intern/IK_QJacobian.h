
/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file iksolver/intern/IK_QJacobian.h
 *  \ingroup iksolver
 */

#pragma once

#include "IK_Math.h"

class IK_QJacobian
{
public:
	IK_QJacobian();
	~IK_QJacobian();

	// Call once to initialize
	void ArmMatrices(int dof, int task_size);
	void SetDoFWeight(int dof, double weight);

	// Iteratively called
	void SetBetas(int id, int size, const Vector3d& v);
	void SetDerivatives(int id, int dof_id, const Vector3d& v, double norm_weight);

	void Invert();

	double AngleUpdate(int dof_id) const;
	double AngleUpdateNorm() const;

	// DoF locking for inner clamping loop
	void Lock(int dof_id, double delta);

	// Secondary task
	bool ComputeNullProjection();

	void Restrict(VectorXd& d_theta, MatrixXd& nullspace);
	void SubTask(IK_QJacobian& jacobian);

private:
	
	void InvertSDLS();
	void InvertDLS();

	int m_dof, m_task_size;
	bool m_transpose;

	// the jacobian matrix and it's null space projector
	MatrixXd m_jacobian, m_jacobian_tmp;
	MatrixXd m_nullspace;

	/// the vector of intermediate betas
	VectorXd m_beta;

	/// the vector of computed angle changes
	VectorXd m_d_theta;
	VectorXd m_d_norm_weight;

	/// space required for SVD computation
	VectorXd m_svd_w;
	MatrixXd m_svd_v;
	MatrixXd m_svd_u;

	VectorXd m_svd_u_beta;

	// space required for SDLS

	bool m_sdls;
	VectorXd m_norm;
	VectorXd m_d_theta_tmp;
	double m_min_damp;

	// null space task vector
	VectorXd m_alpha;

	// dof weighting
	VectorXd m_weight;
	VectorXd m_weight_sqrt;
};

