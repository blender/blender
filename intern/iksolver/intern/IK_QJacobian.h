
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

#ifndef NAN_INCLUDED_IK_QJacobian_h

#define NAN_INCLUDED_IK_QJacobian_h

#include "TNT/cmat.h"
#include <vector>
#include "MT_Vector3.h"

class IK_QJacobian
{
public:
	typedef TNT::Matrix<MT_Scalar> TMatrix;
	typedef TNT::Vector<MT_Scalar> TVector;

	IK_QJacobian();
	~IK_QJacobian();

	// Call once to initialize
	void ArmMatrices(int dof, int task_size);
	void SetDoFWeight(int dof, MT_Scalar weight);

	// Iteratively called
	void SetBetas(int id, int size, const MT_Vector3& v);
	void SetDerivatives(int id, int dof_id, const MT_Vector3& v);

	void Invert();

	MT_Scalar AngleUpdate(int dof_id) const;
	MT_Scalar AngleUpdateNorm() const;

	// DoF locking for inner clamping loop
	void Lock(int dof_id, MT_Scalar delta);

	// Secondary task
	bool ComputeNullProjection();

	void Restrict(TVector& d_theta, TMatrix& null);
	void SubTask(IK_QJacobian& jacobian);

private:
	
	void InvertSDLS();
	void InvertDLS();

	int m_dof, m_task_size;
	bool m_transpose;

	// the jacobian matrix and it's null space projector
	TMatrix m_jacobian, m_jacobian_tmp;
	TMatrix m_null;

	/// the vector of intermediate betas
	TVector m_beta;

	/// the vector of computed angle changes
	TVector m_d_theta;

	/// space required for SVD computation

	TVector m_svd_w;
	TMatrix m_svd_v;
	TMatrix m_svd_u;
    TVector m_work1;
    TVector m_work2;

	TMatrix m_svd_u_t;
	TVector m_svd_u_beta;

	// space required for SDLS

	bool m_sdls;
	TVector m_norm;
	TVector m_d_theta_tmp;
	MT_Scalar m_min_damp;

	// null space task vector
	TVector m_alpha;

	// dof weighting
	TVector m_weight;
	TVector m_weight_sqrt;
};

#endif

