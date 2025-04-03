// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
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

#ifndef CERES_PUBLIC_LINE_MANIFOLD_H_
#define CERES_PUBLIC_LINE_MANIFOLD_H_

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/internal/householder_vector.h"
#include "ceres/internal/sphere_manifold_functions.h"
#include "ceres/manifold.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {
// This provides a manifold for lines, where the line is
// over-parameterized by an origin point and a direction vector. So the
// parameter vector size needs to be two times the ambient space dimension,
// where the first half is interpreted as the origin point and the second half
// as the direction.
//
// The plus operator for the line direction is the same as for the
// SphereManifold. The update of the origin point is
// perpendicular to the line direction before the update.
//
// This manifold is a special case of the affine Grassmannian
// manifold (see https://en.wikipedia.org/wiki/Affine_Grassmannian_(manifold))
// for the case Graff_1(R^n).
//
// The class works with dynamic and static ambient space dimensions. If the
// ambient space dimensions is known at compile time use
//
//    LineManifold<3> manifold;
//
// If the ambient space dimensions is not known at compile time the template
// parameter needs to be set to ceres::DYNAMIC and the actual dimension needs
// to be provided as a constructor argument:
//
//    LineManifold<ceres::DYNAMIC> manifold(ambient_dim);
//
template <int AmbientSpaceDimension>
class LineManifold final : public Manifold {
 public:
  static_assert(AmbientSpaceDimension == DYNAMIC || AmbientSpaceDimension >= 2,
                "The ambient space must be at least 2.");
  static_assert(ceres::DYNAMIC == Eigen::Dynamic,
                "ceres::DYNAMIC needs to be the same as Eigen::Dynamic.");

  LineManifold();
  explicit LineManifold(int size);

  int AmbientSize() const override { return 2 * size_; }
  int TangentSize() const override { return 2 * (size_ - 1); }
  bool Plus(const double* x,
            const double* delta,
            double* x_plus_delta) const override;
  bool PlusJacobian(const double* x, double* jacobian) const override;
  bool Minus(const double* y,
             const double* x,
             double* y_minus_x) const override;
  bool MinusJacobian(const double* x, double* jacobian) const override;

 private:
  static constexpr bool IsDynamic = (AmbientSpaceDimension == ceres::DYNAMIC);
  static constexpr int TangentSpaceDimension =
      IsDynamic ? ceres::DYNAMIC : AmbientSpaceDimension - 1;

  static constexpr int DAmbientSpaceDimension =
      IsDynamic ? ceres::DYNAMIC : 2 * AmbientSpaceDimension;
  static constexpr int DTangentSpaceDimension =
      IsDynamic ? ceres::DYNAMIC : 2 * TangentSpaceDimension;

  using AmbientVector = Eigen::Matrix<double, AmbientSpaceDimension, 1>;
  using TangentVector = Eigen::Matrix<double, TangentSpaceDimension, 1>;
  using MatrixPlusJacobian = Eigen::Matrix<double,
                                           DAmbientSpaceDimension,
                                           DTangentSpaceDimension,
                                           Eigen::RowMajor>;
  using MatrixMinusJacobian = Eigen::Matrix<double,
                                            DTangentSpaceDimension,
                                            DAmbientSpaceDimension,
                                            Eigen::RowMajor>;

  const int size_{AmbientSpaceDimension};
};

template <int AmbientSpaceDimension>
LineManifold<AmbientSpaceDimension>::LineManifold()
    : size_{AmbientSpaceDimension} {
  static_assert(
      AmbientSpaceDimension != Eigen::Dynamic,
      "The size is set to dynamic. Please call the constructor with a size.");
}

template <int AmbientSpaceDimension>
LineManifold<AmbientSpaceDimension>::LineManifold(int size) : size_{size} {
  if (AmbientSpaceDimension != Eigen::Dynamic) {
    CHECK_EQ(AmbientSpaceDimension, size)
        << "Specified size by template parameter differs from the supplied "
           "one.";
  } else {
    CHECK_GT(size_, 1)
        << "The size of the manifold needs to be greater than 1.";
  }
}

template <int AmbientSpaceDimension>
bool LineManifold<AmbientSpaceDimension>::Plus(const double* x_ptr,
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
  // The direction update function Plus_d is the same as as the SphereManifold:
  //
  //   d* = H_{v(d)} [sinc(|delta_d|) delta_d, cos(|delta_d|)]^T
  //
  // where H is the householder matrix
  //   H_{v} = I - (2 / |v|^2) v v^T
  // and
  //   v(d) = d - sign(d_n) |d| e_n.
  //
  // The origin point update function Plus_o is defined as
  //
  //   o* = o + H_{v(d)} [delta_o, 0]^T.

  Eigen::Map<const AmbientVector> o(x_ptr, size_);
  Eigen::Map<const AmbientVector> d(x_ptr + size_, size_);

  Eigen::Map<const TangentVector> delta_o(delta_ptr, size_ - 1);
  Eigen::Map<const TangentVector> delta_d(delta_ptr + size_ - 1, size_ - 1);
  Eigen::Map<AmbientVector> o_plus_delta(x_plus_delta_ptr, size_);
  Eigen::Map<AmbientVector> d_plus_delta(x_plus_delta_ptr + size_, size_);

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
  AmbientVector v(size_);
  double beta;

  // NOTE: The explicit template arguments are needed here because
  // ComputeHouseholderVector is templated and some versions of MSVC
  // have trouble deducing the type of v automatically.
  internal::ComputeHouseholderVector<Eigen::Map<const AmbientVector>,
                                     double,
                                     AmbientSpaceDimension>(d, &v, &beta);

  if (norm_delta_d != 0.0) {
    internal::ComputeSphereManifoldPlus(
        v, beta, d, delta_d, norm_delta_d, &d_plus_delta);
  }

  // The null space is in the direction of the line, so the tangent space is
  // perpendicular to the line direction. This is achieved by using the
  // householder matrix of the direction and allow only movements
  // perpendicular to e_n.
  AmbientVector y(size_);
  y << delta_o, 0;
  o_plus_delta += internal::ApplyHouseholderVector(y, v, beta);

  return true;
}

template <int AmbientSpaceDimension>
bool LineManifold<AmbientSpaceDimension>::PlusJacobian(
    const double* x_ptr, double* jacobian_ptr) const {
  Eigen::Map<const AmbientVector> d(x_ptr + size_, size_);
  Eigen::Map<MatrixPlusJacobian> jacobian(
      jacobian_ptr, 2 * size_, 2 * (size_ - 1));

  // Clear the Jacobian as only half of the matrix is not zero.
  jacobian.setZero();

  auto jacobian_d =
      jacobian
          .template topLeftCorner<AmbientSpaceDimension, TangentSpaceDimension>(
              size_, size_ - 1);
  auto jacobian_o = jacobian.template bottomRightCorner<AmbientSpaceDimension,
                                                        TangentSpaceDimension>(
      size_, size_ - 1);
  internal::ComputeSphereManifoldPlusJacobian(d, &jacobian_d);
  jacobian_o = jacobian_d;
  return true;
}

template <int AmbientSpaceDimension>
bool LineManifold<AmbientSpaceDimension>::Minus(const double* y_ptr,
                                                const double* x_ptr,
                                                double* y_minus_x) const {
  Eigen::Map<const AmbientVector> y_o(y_ptr, size_);
  Eigen::Map<const AmbientVector> y_d(y_ptr + size_, size_);
  Eigen::Map<const AmbientVector> x_o(x_ptr, size_);
  Eigen::Map<const AmbientVector> x_d(x_ptr + size_, size_);

  Eigen::Map<TangentVector> y_minus_x_o(y_minus_x, size_ - 1);
  Eigen::Map<TangentVector> y_minus_x_d(y_minus_x + size_ - 1, size_ - 1);

  AmbientVector v(size_);
  double beta;

  // NOTE: The explicit template arguments are needed here because
  // ComputeHouseholderVector is templated and some versions of MSVC
  // have trouble deducing the type of v automatically.
  internal::ComputeHouseholderVector<Eigen::Map<const AmbientVector>,
                                     double,
                                     AmbientSpaceDimension>(x_d, &v, &beta);

  internal::ComputeSphereManifoldMinus(v, beta, x_d, y_d, &y_minus_x_d);

  AmbientVector delta_o = y_o - x_o;
  const AmbientVector h_delta_o =
      internal::ApplyHouseholderVector(delta_o, v, beta);
  y_minus_x_o = h_delta_o.template head<TangentSpaceDimension>(size_ - 1);

  return true;
}

template <int AmbientSpaceDimension>
bool LineManifold<AmbientSpaceDimension>::MinusJacobian(
    const double* x_ptr, double* jacobian_ptr) const {
  Eigen::Map<const AmbientVector> d(x_ptr + size_, size_);
  Eigen::Map<MatrixMinusJacobian> jacobian(
      jacobian_ptr, 2 * (size_ - 1), 2 * size_);

  // Clear the Jacobian as only half of the matrix is not zero.
  jacobian.setZero();

  auto jacobian_d =
      jacobian
          .template topLeftCorner<TangentSpaceDimension, AmbientSpaceDimension>(
              size_ - 1, size_);
  auto jacobian_o = jacobian.template bottomRightCorner<TangentSpaceDimension,
                                                        AmbientSpaceDimension>(
      size_ - 1, size_);
  internal::ComputeSphereManifoldMinusJacobian(d, &jacobian_d);
  jacobian_o = jacobian_d;

  return true;
}

}  // namespace ceres

// clang-format off
#include "ceres/internal/reenable_warnings.h"
// clang-format on

#endif  // CERES_PUBLIC_LINE_MANIFOLD_H_
