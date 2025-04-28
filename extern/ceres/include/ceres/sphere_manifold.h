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
// Author: vitus@google.com (Mike Vitus)
//         jodebo_beck@gmx.de (Johannes Beck)

#ifndef CERES_PUBLIC_SPHERE_MANIFOLD_H_
#define CERES_PUBLIC_SPHERE_MANIFOLD_H_

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

// This provides a manifold on a sphere meaning that the norm of the vector
// stays the same. Such cases often arises in Structure for Motion
// problems. One example where they are used is in representing points whose
// triangulation is ill-conditioned. Here it is advantageous to use an
// over-parameterization since homogeneous vectors can represent points at
// infinity.
//
// The plus operator is defined as
//  Plus(x, delta) =
//    [sin(0.5 * |delta|) * delta / |delta|, cos(0.5 * |delta|)] * x
//
// The minus operator is defined as
//  Minus(x, y) = 2 atan2(nhy, y[-1]) / nhy * hy[0 : size_ - 1]
// with nhy = norm(hy[0 : size_ - 1])
//
// with * defined as an operator which applies the update orthogonal to x to
// remain on the sphere. The ambient space dimension is required to be greater
// than 1.
//
// The class works with dynamic and static ambient space dimensions. If the
// ambient space dimensions is known at compile time use
//
//    SphereManifold<3> manifold;
//
// If the ambient space dimensions is not known at compile time the template
// parameter needs to be set to ceres::DYNAMIC and the actual dimension needs
// to be provided as a constructor argument:
//
//    SphereManifold<ceres::DYNAMIC> manifold(ambient_dim);
//
// See  section B.2 (p.25) in "Integrating Generic Sensor Fusion Algorithms
// with Sound State Representations through Encapsulation of Manifolds" by C.
// Hertzberg, R. Wagner, U. Frese and L. Schroder for more details
// (https://arxiv.org/pdf/1107.1119.pdf)
template <int AmbientSpaceDimension>
class SphereManifold final : public Manifold {
 public:
  static_assert(
      AmbientSpaceDimension == ceres::DYNAMIC || AmbientSpaceDimension > 1,
      "The size of the homogeneous vector needs to be greater than 1.");
  static_assert(ceres::DYNAMIC == Eigen::Dynamic,
                "ceres::DYNAMIC needs to be the same as Eigen::Dynamic.");

  SphereManifold();
  explicit SphereManifold(int size);

  int AmbientSize() const override {
    return AmbientSpaceDimension == ceres::DYNAMIC ? size_
                                                   : AmbientSpaceDimension;
  }
  int TangentSize() const override { return AmbientSize() - 1; }

  bool Plus(const double* x,
            const double* delta,
            double* x_plus_delta) const override;
  bool PlusJacobian(const double* x, double* jacobian) const override;

  bool Minus(const double* y,
             const double* x,
             double* y_minus_x) const override;
  bool MinusJacobian(const double* x, double* jacobian) const override;

 private:
  static constexpr int TangentSpaceDimension =
      AmbientSpaceDimension > 0 ? AmbientSpaceDimension - 1 : Eigen::Dynamic;

  // NOTE: Eigen does not allow to have a RowMajor column vector.
  // In that case, change the storage order
  static constexpr int SafeRowMajor =
      TangentSpaceDimension == 1 ? Eigen::ColMajor : Eigen::RowMajor;

  using AmbientVector = Eigen::Matrix<double, AmbientSpaceDimension, 1>;
  using TangentVector = Eigen::Matrix<double, TangentSpaceDimension, 1>;
  using MatrixPlusJacobian = Eigen::Matrix<double,
                                           AmbientSpaceDimension,
                                           TangentSpaceDimension,
                                           SafeRowMajor>;
  using MatrixMinusJacobian = Eigen::Matrix<double,
                                            TangentSpaceDimension,
                                            AmbientSpaceDimension,
                                            Eigen::RowMajor>;

  const int size_{};
};

template <int AmbientSpaceDimension>
SphereManifold<AmbientSpaceDimension>::SphereManifold()
    : size_{AmbientSpaceDimension} {
  static_assert(
      AmbientSpaceDimension != Eigen::Dynamic,
      "The size is set to dynamic. Please call the constructor with a size.");
}

template <int AmbientSpaceDimension>
SphereManifold<AmbientSpaceDimension>::SphereManifold(int size) : size_{size} {
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
bool SphereManifold<AmbientSpaceDimension>::Plus(
    const double* x_ptr,
    const double* delta_ptr,
    double* x_plus_delta_ptr) const {
  Eigen::Map<const AmbientVector> x(x_ptr, size_);
  Eigen::Map<const TangentVector> delta(delta_ptr, size_ - 1);
  Eigen::Map<AmbientVector> x_plus_delta(x_plus_delta_ptr, size_);

  const double norm_delta = delta.norm();

  if (norm_delta == 0.0) {
    x_plus_delta = x;
    return true;
  }

  AmbientVector v(size_);
  double beta;

  // NOTE: The explicit template arguments are needed here because
  // ComputeHouseholderVector is templated and some versions of MSVC
  // have trouble deducing the type of v automatically.
  internal::ComputeHouseholderVector<Eigen::Map<const AmbientVector>,
                                     double,
                                     AmbientSpaceDimension>(x, &v, &beta);

  internal::ComputeSphereManifoldPlus(
      v, beta, x, delta, norm_delta, &x_plus_delta);

  return true;
}

template <int AmbientSpaceDimension>
bool SphereManifold<AmbientSpaceDimension>::PlusJacobian(
    const double* x_ptr, double* jacobian_ptr) const {
  Eigen::Map<const AmbientVector> x(x_ptr, size_);
  Eigen::Map<MatrixPlusJacobian> jacobian(jacobian_ptr, size_, size_ - 1);
  internal::ComputeSphereManifoldPlusJacobian(x, &jacobian);

  return true;
}

template <int AmbientSpaceDimension>
bool SphereManifold<AmbientSpaceDimension>::Minus(const double* y_ptr,
                                                  const double* x_ptr,
                                                  double* y_minus_x_ptr) const {
  AmbientVector y = Eigen::Map<const AmbientVector>(y_ptr, size_);
  Eigen::Map<const AmbientVector> x(x_ptr, size_);
  Eigen::Map<TangentVector> y_minus_x(y_minus_x_ptr, size_ - 1);

  // Apply hoseholder transformation.
  AmbientVector v(size_);
  double beta;

  // NOTE: The explicit template arguments are needed here because
  // ComputeHouseholderVector is templated and some versions of MSVC
  // have trouble deducing the type of v automatically.
  internal::ComputeHouseholderVector<Eigen::Map<const AmbientVector>,
                                     double,
                                     AmbientSpaceDimension>(x, &v, &beta);
  internal::ComputeSphereManifoldMinus(v, beta, x, y, &y_minus_x);
  return true;
}

template <int AmbientSpaceDimension>
bool SphereManifold<AmbientSpaceDimension>::MinusJacobian(
    const double* x_ptr, double* jacobian_ptr) const {
  Eigen::Map<const AmbientVector> x(x_ptr, size_);
  Eigen::Map<MatrixMinusJacobian> jacobian(jacobian_ptr, size_ - 1, size_);

  internal::ComputeSphereManifoldMinusJacobian(x, &jacobian);
  return true;
}

}  // namespace ceres

// clang-format off
#include "ceres/internal/reenable_warnings.h"
// clang-format on

#endif  // CERES_PUBLIC_SPHERE_MANIFOLD_H_
