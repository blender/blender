/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2015 Blender Foundation */

/** \file
 * \ingroup intern_eigen
 */

#ifndef __EIGEN3_SVD_C_API_CC__
#define __EIGEN3_SVD_C_API_CC__

/* Eigen gives annoying huge amount of warnings here, silence them! */
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wlogical-op"
#endif

#ifdef __EIGEN3_SVD_C_API_CC__ /* quiet warning */
#endif

#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/SVD>

#include "svd.h"

using Eigen::JacobiSVD;

using Eigen::NoQRPreconditioner;

using Eigen::ComputeThinU;
using Eigen::ComputeThinV;

using Eigen::Map;
using Eigen::MatrixXf;
using Eigen::VectorXf;

using Eigen::Matrix4f;

void EIG_svd_square_matrix(const int size, const float *matrix, float *r_U, float *r_S, float *r_V)
{
  /* Since our matrix is squared, we can use thinU/V. */
  unsigned int flags = (r_U ? ComputeThinU : 0) | (r_V ? ComputeThinV : 0);

  /* Blender and Eigen matrices are both column-major. */
  JacobiSVD<MatrixXf, NoQRPreconditioner> svd(Map<MatrixXf>((float *)matrix, size, size), flags);

  if (r_U) {
    Map<MatrixXf>(r_U, size, size) = svd.matrixU();
  }

  if (r_S) {
    Map<VectorXf>(r_S, size) = svd.singularValues();
  }

  if (r_V) {
    Map<MatrixXf>(r_V, size, size) = svd.matrixV();
  }
}

#endif /* __EIGEN3_SVD_C_API_CC__ */
