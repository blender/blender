/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_eigen
 */

#ifndef __EIGEN3_MATRIX_C_API_CC__
#define __EIGEN3_MATRIX_C_API_CC__

/* Eigen gives annoying huge amount of warnings here, silence them! */
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wlogical-op"
#endif

#ifdef __EIGEN3_MATRIX_C_API_CC__ /* quiet warning */
#endif

#include <Eigen/Core>
#include <Eigen/Dense>

#include "matrix.h"

using Eigen::Map;
using Eigen::Matrix4f;

bool EIG_invert_m4_m4(float inverse[4][4], const float matrix[4][4])
{
  Map<Matrix4f> M = Map<Matrix4f>((float *)matrix);
  Matrix4f R;
  bool invertible = true;
  M.computeInverseWithCheck(R, invertible, 0.0f);
  if (!invertible) {
    R = R.Zero();
  }
  memcpy(inverse, R.data(), sizeof(float) * 4 * 4);
  return invertible;
}

#endif /* __EIGEN3_MATRIX_C_API_CC__ */
