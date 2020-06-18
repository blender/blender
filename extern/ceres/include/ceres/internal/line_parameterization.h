// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2020 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: jodebo_beck@gmx.de (Johannes Beck)
//

#ifndef CERES_PUBLIC_INTERNAL_LINE_PARAMETERIZATION_H_
#define CERES_PUBLIC_INTERNAL_LINE_PARAMETERIZATION_H_

#include "householder_vector.h"

namespace ceres {

template <int AmbientSpaceDimension>
bool LineParameterization<AmbientSpaceDimension>::Plus(
    const double* x_ptr,
    const double* delta_ptr,
    double* x_plus_delta_ptr) const {
  // We seek a box plus operator of the form
  //
  //   [o*, d*] = Plus([o, d], [delta_o, delta_d])
  //
  // where o is the origin point, d is the direction vector, delta_o is
  // the delta of the origin point and delta_d the delta of the direction and
  // o* and d* is the updated origin point and direction.
  //
  // We separate the Plus operator into the origin point and directional part
  //   d* = Plus_d(d, delta_d)
  //   o* = Plus_o(o, d, delta_o)
  //
  // The direction update function Plus_d is the same as for the homogeneous
  // vector parameterization:
  //
  //   d* = H_{v(d)} [0.5 sinc(0.5 |delta_d|) delta_d, cos(0.5 |delta_d|)]^T
  //
  // where H is the householder matrix
  //   H_{v} = I - (2 / |v|^2) v v^T
  // and
  //   v(d) = d - sign(d_n) |d| e_n.
  //
  // The origin point update function Plus_o is defined as
  //
  //   o* = o + H_{v(d)} [0.5 delta_o, 0]^T.

  static constexpr int kDim = AmbientSpaceDimension;
  using AmbientVector = Eigen::Matrix<double, kDim, 1>;
  using AmbientVectorRef = Eigen::Map<Eigen::Matrix<double, kDim, 1>>;
  using ConstAmbientVectorRef =
      Eigen::Map<const Eigen::Matrix<double, kDim, 1>>;
  using ConstTangentVectorRef =
      Eigen::Map<const Eigen::Matrix<double, kDim - 1, 1>>;

  ConstAmbientVectorRef o(x_ptr);
  ConstAmbientVectorRef d(x_ptr + kDim);

  ConstTangentVectorRef delta_o(delta_ptr);
  ConstTangentVectorRef delta_d(delta_ptr + kDim - 1);
  AmbientVectorRef o_plus_delta(x_plus_delta_ptr);
  AmbientVectorRef d_plus_delta(x_plus_delta_ptr + kDim);

  const double norm_delta_d = delta_d.norm();

  o_plus_delta = o;

  // Shortcut for zero delta direction.
  if (norm_delta_d == 0.0) {
    d_plus_delta = d;

    if (delta_o.isZero(0.0)) {
      return true;
    }
  }

  // Calculate the householder transformation which is needed for f_d and f_o.
  AmbientVector v;
  double beta;

  // NOTE: The explicit template arguments are needed here because
  // ComputeHouseholderVector is templated and some versions of MSVC
  // have trouble deducing the type of v automatically.
  internal::ComputeHouseholderVector<ConstAmbientVectorRef, double, kDim>(
      d, &v, &beta);

  if (norm_delta_d != 0.0) {
    // Map the delta from the minimum representation to the over parameterized
    // homogeneous vector. See section A6.9.2 on page 624 of Hartley & Zisserman
    // (2nd Edition) for a detailed description.  Note there is a typo on Page
    // 625, line 4 so check the book errata.
    const double norm_delta_div_2 = 0.5 * norm_delta_d;
    const double sin_delta_by_delta =
        std::sin(norm_delta_div_2) / norm_delta_div_2;

    // Apply the delta update to remain on the unit sphere. See section A6.9.3
    // on page 625 of Hartley & Zisserman (2nd Edition) for a detailed
    // description.
    AmbientVector y;
    y.template head<kDim - 1>() = 0.5 * sin_delta_by_delta * delta_d;
    y[kDim - 1] = std::cos(norm_delta_div_2);

    d_plus_delta = d.norm() * (y - v * (beta * (v.transpose() * y)));
  }

  // The null space is in the direction of the line, so the tangent space is
  // perpendicular to the line direction. This is achieved by using the
  // householder matrix of the direction and allow only movements
  // perpendicular to e_n.
  //
  // The factor of 0.5 is used to be consistent with the line direction
  // update.
  AmbientVector y;
  y << 0.5 * delta_o, 0;
  o_plus_delta += y - v * (beta * (v.transpose() * y));

  return true;
}

template <int AmbientSpaceDimension>
bool LineParameterization<AmbientSpaceDimension>::ComputeJacobian(
    const double* x_ptr, double* jacobian_ptr) const {
  static constexpr int kDim = AmbientSpaceDimension;
  using AmbientVector = Eigen::Matrix<double, kDim, 1>;
  using ConstAmbientVectorRef =
      Eigen::Map<const Eigen::Matrix<double, kDim, 1>>;
  using MatrixRef = Eigen::Map<
      Eigen::Matrix<double, 2 * kDim, 2 * (kDim - 1), Eigen::RowMajor>>;

  ConstAmbientVectorRef d(x_ptr + kDim);
  MatrixRef jacobian(jacobian_ptr);

  // Clear the Jacobian as only half of the matrix is not zero.
  jacobian.setZero();

  AmbientVector v;
  double beta;

  // NOTE: The explicit template arguments are needed here because
  // ComputeHouseholderVector is templated and some versions of MSVC
  // have trouble deducing the type of v automatically.
  internal::ComputeHouseholderVector<ConstAmbientVectorRef, double, kDim>(
      d, &v, &beta);

  // The Jacobian is equal to J = 0.5 * H.leftCols(kDim - 1) where H is
  // the Householder matrix (H = I - beta * v * v') for the origin point. For
  // the line direction part the Jacobian is scaled by the norm of the
  // direction.
  for (int i = 0; i < kDim - 1; ++i) {
    jacobian.block(0, i, kDim, 1) = -0.5 * beta * v(i) * v;
    jacobian.col(i)(i) += 0.5;
  }

  jacobian.template block<kDim, kDim - 1>(kDim, kDim - 1) =
      jacobian.template block<kDim, kDim - 1>(0, 0) * d.norm();
  return true;
}

}  // namespace ceres

#endif  // CERES_PUBLIC_INTERNAL_LINE_PARAMETERIZATION_H_
