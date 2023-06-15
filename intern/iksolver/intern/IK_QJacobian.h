/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_iksolver
 */

#pragma once

#include "IK_Math.h"

class IK_QJacobian {
 public:
  IK_QJacobian();
  ~IK_QJacobian();

  // Call once to initialize
  void ArmMatrices(int dof, int task_size);
  void SetDoFWeight(int dof, double weight);

  // Iteratively called
  void SetBetas(int id, int size, const Vector3d &v);
  void SetDerivatives(int id, int dof_id, const Vector3d &v, double norm_weight);

  void Invert();

  double AngleUpdate(int dof_id) const;
  double AngleUpdateNorm() const;

  // DoF locking for inner clamping loop
  void Lock(int dof_id, double delta);

  // Secondary task
  bool ComputeNullProjection();

  void Restrict(VectorXd &d_theta, MatrixXd &nullspace);
  void SubTask(IK_QJacobian &jacobian);

 private:
  void InvertSDLS();
  void InvertDLS();

  int m_dof, m_task_size;
  bool m_transpose;

  // the jacobian matrix and its null space projector
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
