/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __EIGEN3_EIGENVALUES_C_API_CC__
#define __EIGEN3_EIGENVALUES_C_API_CC__

/* Eigen gives annoying huge amount of warnings here, silence them! */
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wlogical-op"
#endif

#include <Eigen/Core>
#include <Eigen/Eigenvalues>

#include "eigenvalues.h"

using Eigen::SelfAdjointEigenSolver;

using Eigen::Map;
using Eigen::MatrixXf;
using Eigen::VectorXf;

using Eigen::Success;

bool EIG_self_adjoint_eigen_solve(const int size,
                                  const float *matrix,
                                  float *r_eigen_values,
                                  float *r_eigen_vectors)
{
  SelfAdjointEigenSolver<MatrixXf> eigen_solver;

  /* Blender and Eigen matrices are both column-major. */
  eigen_solver.compute(Map<MatrixXf>((float *)matrix, size, size));

  if (eigen_solver.info() != Success) {
    return false;
  }

  if (r_eigen_values) {
    Map<VectorXf>(r_eigen_values, size) = eigen_solver.eigenvalues().transpose();
  }

  if (r_eigen_vectors) {
    Map<MatrixXf>(r_eigen_vectors, size, size) = eigen_solver.eigenvectors();
  }

  return true;
}

#endif /* __EIGEN3_EIGENVALUES_C_API_CC__ */
