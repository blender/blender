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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_PUBLIC_MANIFOLD_H_
#define CERES_PUBLIC_MANIFOLD_H_

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {

// In sensor fusion problems, often we have to model quantities that live in
// spaces known as Manifolds, for example the rotation/orientation of a sensor
// that is represented by a quaternion.
//
// Manifolds are spaces which locally look like Euclidean spaces. More
// precisely, at each point on the manifold there is a linear space that is
// tangent to the manifold. It has dimension equal to the intrinsic dimension of
// the manifold itself, which is less than or equal to the ambient space in
// which the manifold is embedded.
//
// For example, the tangent space to a point on a sphere in three dimensions is
// the two dimensional plane that is tangent to the sphere at that point. There
// are two reasons tangent spaces are interesting:
//
// 1. They are Eucliean spaces so the usual vector space operations apply there,
//    which makes numerical operations easy.
// 2. Movement in the tangent space translate into movements along the manifold.
//    Movements perpendicular to the tangent space do not translate into
//    movements on the manifold.
//
// Returning to our sphere example, moving in the 2 dimensional plane
// tangent to the sphere and projecting back onto the sphere will move you away
// from the point you started from but moving along the normal at the same point
// and the projecting back onto the sphere brings you back to the point.
//
// The Manifold interface defines two operations (and their derivatives)
// involving the tangent space, allowing filtering and optimization to be
// performed on said manifold:
//
// 1. x_plus_delta = Plus(x, delta)
// 2. delta = Minus(x_plus_delta, x)
//
// "Plus" computes the result of moving along delta in the tangent space at x,
// and then projecting back onto the manifold that x belongs to. In Differential
// Geometry this is known as a "Retraction". It is a generalization of vector
// addition in Euclidean spaces.
//
// Given two points on the manifold, "Minus" computes the change delta to x in
// the tangent space at x, that will take it to x_plus_delta.
//
// Let us now consider two examples.
//
// The Euclidean space R^n is the simplest example of a manifold. It has
// dimension n (and so does its tangent space) and Plus and Minus are the
// familiar vector sum and difference operations.
//
//  Plus(x, delta) = x + delta = y,
//  Minus(y, x) = y - x = delta.
//
// A more interesting case is SO(3), the special orthogonal group in three
// dimensions - the space of 3x3 rotation matrices. SO(3) is a three dimensional
// manifold embedded in R^9 or R^(3x3). So points on SO(3) are represented using
// 9 dimensional vectors or 3x3 matrices, and points in its tangent spaces are
// represented by 3 dimensional vectors.
//
// Defining Plus and Minus are defined in terms of the matrix Exp and Log
// operations as follows:
//
// Let Exp(p, q, r) = [cos(theta) + cp^2, -sr + cpq        ,  sq + cpr        ]
//                    [sr + cpq         , cos(theta) + cq^2, -sp + cqr        ]
//                    [-sq + cpr        , sp + cqr         , cos(theta) + cr^2]
//
// where: theta = sqrt(p^2 + q^2 + r^2)
//            s = sinc(theta)
//            c = (1 - cos(theta))/theta^2
//
// and Log(x) = 1/(2 sinc(theta))[x_32 - x_23, x_13 - x_31, x_21 - x_12]
//
// where: theta = acos((Trace(x) - 1)/2)
//
// Then,
//
// Plus(x, delta) = x Exp(delta)
// Minus(y, x) = Log(x^T y)
//
// For Plus and Minus to be mathematically consistent, the following identities
// must be satisfied at all points x on the manifold:
//
// 1.  Plus(x, 0) = x.
// 2.  For all y, Plus(x, Minus(y, x)) = y.
// 3.  For all delta, Minus(Plus(x, delta), x) = delta.
// 4.  For all delta_1, delta_2
//    |Minus(Plus(x, delta_1), Plus(x, delta_2)) <= |delta_1 - delta_2|
//
// Briefly:
// (1) Ensures that the tangent space is "centered" at x, and the zero vector is
//     the identity element.
// (2) Ensures that any y can be reached from x.
// (3) Ensures that Plus is an injective (one-to-one) map.
// (4) Allows us to define a metric on the manifold.
//
// Additionally we require that Plus and Minus be sufficiently smooth. In
// particular they need to be differentiable everywhere on the manifold.
//
// For more details, please see
//
// "Integrating Generic Sensor Fusion Algorithms with Sound State
// Representations through Encapsulation of Manifolds"
// By C. Hertzberg, R. Wagner, U. Frese and L. Schroder
// https://arxiv.org/pdf/1107.1119.pdf
class CERES_EXPORT Manifold {
 public:
  virtual ~Manifold();

  // Dimension of the ambient space in which the manifold is embedded.
  virtual int AmbientSize() const = 0;

  // Dimension of the manifold/tangent space.
  virtual int TangentSize() const = 0;

  //   x_plus_delta = Plus(x, delta),
  //
  // A generalization of vector addition in Euclidean space, Plus computes the
  // result of moving along delta in the tangent space at x, and then projecting
  // back onto the manifold that x belongs to.
  //
  // x and x_plus_delta are AmbientSize() vectors.
  // delta is a TangentSize() vector.
  //
  // Return value indicates if the operation was successful or not.
  virtual bool Plus(const double* x,
                    const double* delta,
                    double* x_plus_delta) const = 0;

  // Compute the derivative of Plus(x, delta) w.r.t delta at delta = 0, i.e.
  //
  // (D_2 Plus)(x, 0)
  //
  // jacobian is a row-major AmbientSize() x TangentSize() matrix.
  //
  // Return value indicates whether the operation was successful or not.
  virtual bool PlusJacobian(const double* x, double* jacobian) const = 0;

  // tangent_matrix = ambient_matrix * (D_2 Plus)(x, 0)
  //
  // ambient_matrix is a row-major num_rows x AmbientSize() matrix.
  // tangent_matrix is a row-major num_rows x TangentSize() matrix.
  //
  // Return value indicates whether the operation was successful or not.
  //
  // This function is only used by the GradientProblemSolver, where the
  // dimension of the parameter block can be large and it may be more efficient
  // to compute this product directly rather than first evaluating the Jacobian
  // into a matrix and then doing a matrix vector product.
  //
  // Because this is not an often used function, we provide a default
  // implementation for convenience. If performance becomes an issue then the
  // user should consider implementing a specialization.
  virtual bool RightMultiplyByPlusJacobian(const double* x,
                                           const int num_rows,
                                           const double* ambient_matrix,
                                           double* tangent_matrix) const;

  // y_minus_x = Minus(y, x)
  //
  // Given two points on the manifold, Minus computes the change to x in the
  // tangent space at x, that will take it to y.
  //
  // x and y are AmbientSize() vectors.
  // y_minus_x is a TangentSize() vector.
  //
  // Return value indicates if the operation was successful or not.
  virtual bool Minus(const double* y,
                     const double* x,
                     double* y_minus_x) const = 0;

  // Compute the derivative of Minus(y, x) w.r.t y at y = x, i.e
  //
  //   (D_1 Minus) (x, x)
  //
  // Jacobian is a row-major TangentSize() x AmbientSize() matrix.
  //
  // Return value indicates whether the operation was successful or not.
  virtual bool MinusJacobian(const double* x, double* jacobian) const = 0;
};

// The Euclidean manifold is another name for the ordinary vector space R^size,
// where the plus and minus operations are the usual vector addition and
// subtraction:
//   Plus(x, delta) = x + delta
//   Minus(y, x) = y - x.
//
// The class works with dynamic and static ambient space dimensions. If the
// ambient space dimensions is know at compile time use
//
//    EuclideanManifold<3> manifold;
//
// If the ambient space dimensions is not known at compile time the template
// parameter needs to be set to ceres::DYNAMIC and the actual dimension needs
// to be provided as a constructor argument:
//
//    EuclideanManifold<ceres::DYNAMIC> manifold(ambient_dim);
template <int Size>
class EuclideanManifold final : public Manifold {
 public:
  static_assert(Size == ceres::DYNAMIC || Size >= 0,
                "The size of the manifold needs to be non-negative.");
  static_assert(ceres::DYNAMIC == Eigen::Dynamic,
                "ceres::DYNAMIC needs to be the same as Eigen::Dynamic.");

  EuclideanManifold() : size_{Size} {
    static_assert(
        Size != ceres::DYNAMIC,
        "The size is set to dynamic. Please call the constructor with a size.");
  }

  explicit EuclideanManifold(int size) : size_(size) {
    if (Size != ceres::DYNAMIC) {
      CHECK_EQ(Size, size)
          << "Specified size by template parameter differs from the supplied "
             "one.";
    } else {
      CHECK_GE(size_, 0)
          << "The size of the manifold needs to be non-negative.";
    }
  }

  int AmbientSize() const override { return size_; }
  int TangentSize() const override { return size_; }

  bool Plus(const double* x_ptr,
            const double* delta_ptr,
            double* x_plus_delta_ptr) const override {
    Eigen::Map<const AmbientVector> x(x_ptr, size_);
    Eigen::Map<const AmbientVector> delta(delta_ptr, size_);
    Eigen::Map<AmbientVector> x_plus_delta(x_plus_delta_ptr, size_);
    x_plus_delta = x + delta;
    return true;
  }

  bool PlusJacobian(const double* x_ptr, double* jacobian_ptr) const override {
    Eigen::Map<MatrixJacobian> jacobian(jacobian_ptr, size_, size_);
    jacobian.setIdentity();
    return true;
  }

  bool RightMultiplyByPlusJacobian(const double* x,
                                   const int num_rows,
                                   const double* ambient_matrix,
                                   double* tangent_matrix) const override {
    std::copy_n(ambient_matrix, num_rows * size_, tangent_matrix);
    return true;
  }

  bool Minus(const double* y_ptr,
             const double* x_ptr,
             double* y_minus_x_ptr) const override {
    Eigen::Map<const AmbientVector> x(x_ptr, size_);
    Eigen::Map<const AmbientVector> y(y_ptr, size_);
    Eigen::Map<AmbientVector> y_minus_x(y_minus_x_ptr, size_);
    y_minus_x = y - x;
    return true;
  }

  bool MinusJacobian(const double* x_ptr, double* jacobian_ptr) const override {
    Eigen::Map<MatrixJacobian> jacobian(jacobian_ptr, size_, size_);
    jacobian.setIdentity();
    return true;
  }

 private:
  static constexpr bool IsDynamic = (Size == ceres::DYNAMIC);
  using AmbientVector = Eigen::Matrix<double, Size, 1>;
  using MatrixJacobian = Eigen::Matrix<double, Size, Size, Eigen::RowMajor>;

  int size_{};
};

// Hold a subset of the parameters inside a parameter block constant.
class CERES_EXPORT SubsetManifold final : public Manifold {
 public:
  SubsetManifold(int size, const std::vector<int>& constant_parameters);
  int AmbientSize() const override;
  int TangentSize() const override;

  bool Plus(const double* x,
            const double* delta,
            double* x_plus_delta) const override;
  bool PlusJacobian(const double* x, double* jacobian) const override;
  bool RightMultiplyByPlusJacobian(const double* x,
                                   const int num_rows,
                                   const double* ambient_matrix,
                                   double* tangent_matrix) const override;
  bool Minus(const double* y,
             const double* x,
             double* y_minus_x) const override;
  bool MinusJacobian(const double* x, double* jacobian) const override;

 private:
  const int tangent_size_ = 0;
  std::vector<bool> constancy_mask_;
};

// Implements the manifold for a Hamilton quaternion as defined in
// https://en.wikipedia.org/wiki/Quaternion. Quaternions are represented as
// unit norm 4-vectors, i.e.
//
// q = [q0; q1; q2; q3], |q| = 1
//
// is the ambient space representation.
//
//   q0  scalar part.
//   q1  coefficient of i.
//   q2  coefficient of j.
//   q3  coefficient of k.
//
// where: i*i = j*j = k*k = -1 and i*j = k, j*k = i, k*i = j.
//
// The tangent space is R^3, which relates to the ambient space through the
// Plus and Minus operations defined as:
//
// Plus(x, delta) = [cos(|delta|); sin(|delta|) * delta / |delta|] * x
//    Minus(y, x) = to_delta(y * x^{-1})
//
// where "*" is the quaternion product and because q is a unit quaternion
// (|q|=1), q^-1 = [q0; -q1; -q2; -q3]
//
// and to_delta( [q0; u_{3x1}] ) = u / |u| * atan2(|u|, q0)
class CERES_EXPORT QuaternionManifold final : public Manifold {
 public:
  int AmbientSize() const override { return 4; }
  int TangentSize() const override { return 3; }

  bool Plus(const double* x,
            const double* delta,
            double* x_plus_delta) const override;
  bool PlusJacobian(const double* x, double* jacobian) const override;
  bool Minus(const double* y,
             const double* x,
             double* y_minus_x) const override;
  bool MinusJacobian(const double* x, double* jacobian) const override;
};

// Implements the quaternion manifold for Eigen's representation of the
// Hamilton quaternion. Geometrically it is exactly the same as the
// QuaternionManifold defined above. However, Eigen uses a different internal
// memory layout for the elements of the quaternion than what is commonly
// used. It stores the quaternion in memory as [q1, q2, q3, q0] or
// [x, y, z, w] where the real (scalar) part is last.
//
// Since Ceres operates on parameter blocks which are raw double pointers this
// difference is important and requires a different manifold.
class CERES_EXPORT EigenQuaternionManifold final : public Manifold {
 public:
  int AmbientSize() const override { return 4; }
  int TangentSize() const override { return 3; }

  bool Plus(const double* x,
            const double* delta,
            double* x_plus_delta) const override;
  bool PlusJacobian(const double* x, double* jacobian) const override;
  bool Minus(const double* y,
             const double* x,
             double* y_minus_x) const override;
  bool MinusJacobian(const double* x, double* jacobian) const override;
};

}  // namespace ceres

// clang-format off
#include "ceres/internal/reenable_warnings.h"
// clang-format on

#endif  // CERES_PUBLIC_MANIFOLD_H_
