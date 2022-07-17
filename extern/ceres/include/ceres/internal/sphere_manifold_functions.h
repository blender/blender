// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2022 Google Inc. All rights reserved.
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
// Author: vitus@google.com (Mike Vitus)
//         jodebo_beck@gmx.de (Johannes Beck)

#ifndef CERES_PUBLIC_INTERNAL_SPHERE_MANIFOLD_HELPERS_H_
#define CERES_PUBLIC_INTERNAL_SPHERE_MANIFOLD_HELPERS_H_

#include "ceres/internal/householder_vector.h"

// This module contains functions to compute the SphereManifold plus and minus
// operator and their Jacobians.
//
// As the parameters to these functions are shared between them, they are
// described here: The following variable names are used:
//  Plus(x, delta) = x + delta = x_plus_delta,
//  Minus(y, x) = y - x = y_minus_x.
//
// The remaining ones are v and beta which describe the Householder
// transformation of x, and norm_delta which is the norm of delta.
//
// The types of x, y, x_plus_delta and y_minus_x need to be equivalent to
// Eigen::Matrix<double, AmbientSpaceDimension, 1> and the type of delta needs
// to be equivalent to Eigen::Matrix<double, TangentSpaceDimension, 1>.
//
// The type of Jacobian plus needs to be equivalent to Eigen::Matrix<double,
// AmbientSpaceDimension, TangentSpaceDimension, Eigen::RowMajor> and for
// Jacobian minus Eigen::Matrix<double, TangentSpaceDimension,
// AmbientSpaceDimension, Eigen::RowMajor>.
//
// For all vector / matrix inputs and outputs, template parameters are
// used in order to allow also Eigen::Ref and Eigen block expressions to
// be passed to the function.

namespace ceres {
namespace internal {

template <typename VT, typename XT, typename DeltaT, typename XPlusDeltaT>
inline void ComputeSphereManifoldPlus(const VT& v,
                                      double beta,
                                      const XT& x,
                                      const DeltaT& delta,
                                      double norm_delta,
                                      XPlusDeltaT* x_plus_delta) {
  constexpr int AmbientDim = VT::RowsAtCompileTime;

  // Map the delta from the minimum representation to the over parameterized
  // homogeneous vector. See B.2 p.25 equation (106) - (107) for more details.
  const double norm_delta_div_2 = 0.5 * norm_delta;
  const double sin_delta_by_delta =
      std::sin(norm_delta_div_2) / norm_delta_div_2;

  Eigen::Matrix<double, AmbientDim, 1> y(v.size());
  y << 0.5 * sin_delta_by_delta * delta, std::cos(norm_delta_div_2);

  // Apply the delta update to remain on the sphere.
  *x_plus_delta = x.norm() * ApplyHouseholderVector(y, v, beta);
}

template <typename VT, typename JacobianT>
inline void ComputeSphereManifoldPlusJacobian(const VT& x,
                                              JacobianT* jacobian) {
  constexpr int AmbientSpaceDim = VT::RowsAtCompileTime;
  using AmbientVector = Eigen::Matrix<double, AmbientSpaceDim, 1>;
  const int ambient_size = x.size();
  const int tangent_size = x.size() - 1;

  AmbientVector v(ambient_size);
  double beta;

  // NOTE: The explicit template arguments are needed here because
  // ComputeHouseholderVector is templated and some versions of MSVC
  // have trouble deducing the type of v automatically.
  ComputeHouseholderVector<VT, double, AmbientSpaceDim>(x, &v, &beta);

  // The Jacobian is equal to J = 0.5 * H.leftCols(size_ - 1) where H is the
  // Householder matrix (H = I - beta * v * v').
  for (int i = 0; i < tangent_size; ++i) {
    (*jacobian).col(i) = -0.5 * beta * v(i) * v;
    (*jacobian)(i, i) += 0.5;
  }
  (*jacobian) *= x.norm();
}

template <typename VT, typename XT, typename YT, typename YMinusXT>
inline void ComputeSphereManifoldMinus(
    const VT& v, double beta, const XT& x, const YT& y, YMinusXT* y_minus_x) {
  constexpr int AmbientSpaceDim = VT::RowsAtCompileTime;
  constexpr int TangentSpaceDim =
      AmbientSpaceDim == Eigen::Dynamic ? Eigen::Dynamic : AmbientSpaceDim - 1;
  using AmbientVector = Eigen::Matrix<double, AmbientSpaceDim, 1>;

  const int tanget_size = v.size() - 1;

  const AmbientVector hy = ApplyHouseholderVector(y, v, beta) / x.norm();

  // Calculate y - x. See B.2 p.25 equation (108).
  double y_last = hy[tanget_size];
  double hy_norm = hy.template head<TangentSpaceDim>(tanget_size).norm();
  if (hy_norm == 0.0) {
    y_minus_x->setZero();
  } else {
    *y_minus_x = 2.0 * std::atan2(hy_norm, y_last) / hy_norm *
                 hy.template head<TangentSpaceDim>(tanget_size);
  }
}

template <typename VT, typename JacobianT>
inline void ComputeSphereManifoldMinusJacobian(const VT& x,
                                               JacobianT* jacobian) {
  constexpr int AmbientSpaceDim = VT::RowsAtCompileTime;
  using AmbientVector = Eigen::Matrix<double, AmbientSpaceDim, 1>;
  const int ambient_size = x.size();
  const int tangent_size = x.size() - 1;

  AmbientVector v(ambient_size);
  double beta;

  // NOTE: The explicit template arguments are needed here because
  // ComputeHouseholderVector is templated and some versions of MSVC
  // have trouble deducing the type of v automatically.
  ComputeHouseholderVector<VT, double, AmbientSpaceDim>(x, &v, &beta);

  // The Jacobian is equal to J = 2.0 * H.leftCols(size_ - 1) where H is the
  // Householder matrix (H = I - beta * v * v').
  for (int i = 0; i < tangent_size; ++i) {
    (*jacobian).row(i) = -2.0 * beta * v(i) * v;
    (*jacobian)(i, i) += 2.0;
  }
  (*jacobian) /= x.norm();
}

}  // namespace internal
}  // namespace ceres

#endif
