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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#include <cmath>
#include <limits>
#include <memory>

#include "ceres/dynamic_numeric_diff_cost_function.h"
#include "ceres/internal/eigen.h"
#include "ceres/manifold.h"
#include "ceres/numeric_diff_options.h"
#include "ceres/types.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ceres {

// Matchers and macros to simplify testing of custom Manifold objects using the
// gtest testing framework.
//
// Testing a Manifold has two parts.
//
// 1. Checking that Manifold::Plus() and Manifold::Minus() are correctly
//    defined. This requires per manifold tests.
//
// 2. The other methods of the manifold have mathematical properties that make
//    them compatible with Plus() and Minus(), as described in [1].
//
// To verify these general requirements for a custom Manifold, use the
// EXPECT_THAT_MANIFOLD_INVARIANTS_HOLD() macro from within a gtest test. Note
// that additional domain-specific tests may also be prudent, e.g to verify the
// behaviour of a Quaternion Manifold about pi.
//
// [1] "Integrating Generic Sensor Fusion Algorithms with Sound State
//     Representations through Encapsulation of Manifolds", C. Hertzberg,
//     R. Wagner, U. Frese and L. Schroder, https://arxiv.org/pdf/1107.1119.pdf

// Verifies the general requirements for a custom Manifold are satisfied to
// within the specified (numerical) tolerance.
//
// Example usage for a custom Manifold: ExampleManifold:
//
//    TEST(ExampleManifold, ManifoldInvariantsHold) {
//      constexpr double kTolerance = 1.0e-9;
//      ExampleManifold manifold;
//      ceres::Vector x = ceres::Vector::Zero(manifold.AmbientSize());
//      ceres::Vector y = ceres::Vector::Zero(manifold.AmbientSize());
//      ceres::Vector delta = ceres::Vector::Zero(manifold.TangentSize());
//      EXPECT_THAT_MANIFOLD_INVARIANTS_HOLD(manifold, x, delta, y, kTolerance);
//    }
#define EXPECT_THAT_MANIFOLD_INVARIANTS_HOLD(manifold, x, delta, y, tolerance) \
  ::ceres::Vector zero_tangent =                                               \
      ::ceres::Vector::Zero(manifold.TangentSize());                           \
  EXPECT_THAT(manifold, ::ceres::XPlusZeroIsXAt(x, tolerance));                \
  EXPECT_THAT(manifold, ::ceres::XMinusXIsZeroAt(x, tolerance));               \
  EXPECT_THAT(manifold, ::ceres::MinusPlusIsIdentityAt(x, delta, tolerance));  \
  EXPECT_THAT(manifold,                                                        \
              ::ceres::MinusPlusIsIdentityAt(x, zero_tangent, tolerance));     \
  EXPECT_THAT(manifold, ::ceres::PlusMinusIsIdentityAt(x, x, tolerance));      \
  EXPECT_THAT(manifold, ::ceres::PlusMinusIsIdentityAt(x, y, tolerance));      \
  EXPECT_THAT(manifold, ::ceres::HasCorrectPlusJacobianAt(x, tolerance));      \
  EXPECT_THAT(manifold, ::ceres::HasCorrectMinusJacobianAt(x, tolerance));     \
  EXPECT_THAT(manifold, ::ceres::MinusPlusJacobianIsIdentityAt(x, tolerance)); \
  EXPECT_THAT(manifold,                                                        \
              ::ceres::HasCorrectRightMultiplyByPlusJacobianAt(x, tolerance));

// Checks that the invariant Plus(x, 0) == x holds.
MATCHER_P2(XPlusZeroIsXAt, x, tolerance, "") {
  const int ambient_size = arg.AmbientSize();
  const int tangent_size = arg.TangentSize();

  Vector actual = Vector::Zero(ambient_size);
  Vector zero = Vector::Zero(tangent_size);
  EXPECT_TRUE(arg.Plus(x.data(), zero.data(), actual.data()));
  const double n = (actual - Vector{x}).norm();
  const double d = x.norm();
  const double diffnorm = (d == 0.0) ? n : (n / d);
  if (diffnorm > tolerance) {
    *result_listener << "\nexpected (x): " << x.transpose()
                     << "\nactual: " << actual.transpose()
                     << "\ndiffnorm: " << diffnorm;
    return false;
  }
  return true;
}

// Checks that the invariant Minus(x, x) == 0 holds.
MATCHER_P2(XMinusXIsZeroAt, x, tolerance, "") {
  const int tangent_size = arg.TangentSize();
  Vector actual = Vector::Zero(tangent_size);
  EXPECT_TRUE(arg.Minus(x.data(), x.data(), actual.data()));
  const double diffnorm = actual.norm();
  if (diffnorm > tolerance) {
    *result_listener << "\nx: " << x.transpose()  //
                     << "\nexpected: 0 0 0"
                     << "\nactual: " << actual.transpose()
                     << "\ndiffnorm: " << diffnorm;
    return false;
  }
  return true;
}

// Helper struct to curry Plus(x, .) so that it can be numerically
// differentiated.
struct PlusFunctor {
  PlusFunctor(const Manifold& manifold, const double* x)
      : manifold(manifold), x(x) {}
  bool operator()(double const* const* parameters, double* x_plus_delta) const {
    return manifold.Plus(x, parameters[0], x_plus_delta);
  }

  const Manifold& manifold;
  const double* x;
};

// Checks that the output of PlusJacobian matches the one obtained by
// numerically evaluating D_2 Plus(x,0).
MATCHER_P2(HasCorrectPlusJacobianAt, x, tolerance, "") {
  const int ambient_size = arg.AmbientSize();
  const int tangent_size = arg.TangentSize();

  NumericDiffOptions options;
  options.ridders_relative_initial_step_size = 1e-4;

  DynamicNumericDiffCostFunction<PlusFunctor, RIDDERS> cost_function(
      new PlusFunctor(arg, x.data()), TAKE_OWNERSHIP, options);
  cost_function.AddParameterBlock(tangent_size);
  cost_function.SetNumResiduals(ambient_size);

  Vector zero = Vector::Zero(tangent_size);
  double* parameters[1] = {zero.data()};

  Vector x_plus_zero = Vector::Zero(ambient_size);
  Matrix expected = Matrix::Zero(ambient_size, tangent_size);
  double* jacobians[1] = {expected.data()};

  EXPECT_TRUE(
      cost_function.Evaluate(parameters, x_plus_zero.data(), jacobians));

  Matrix actual = Matrix::Random(ambient_size, tangent_size);
  EXPECT_TRUE(arg.PlusJacobian(x.data(), actual.data()));

  const double n = (actual - expected).norm();
  const double d = expected.norm();
  const double diffnorm = (d == 0.0) ? n : n / d;
  if (diffnorm > tolerance) {
    *result_listener << "\nx: " << x.transpose() << "\nexpected: \n"
                     << expected << "\nactual:\n"
                     << actual << "\ndiff:\n"
                     << expected - actual << "\ndiffnorm : " << diffnorm;
    return false;
  }
  return true;
}

// Checks that the invariant Minus(Plus(x, delta), x) == delta holds.
MATCHER_P3(MinusPlusIsIdentityAt, x, delta, tolerance, "") {
  const int ambient_size = arg.AmbientSize();
  const int tangent_size = arg.TangentSize();
  Vector x_plus_delta = Vector::Zero(ambient_size);
  EXPECT_TRUE(arg.Plus(x.data(), delta.data(), x_plus_delta.data()));
  Vector actual = Vector::Zero(tangent_size);
  EXPECT_TRUE(arg.Minus(x_plus_delta.data(), x.data(), actual.data()));

  const double n = (actual - Vector{delta}).norm();
  const double d = delta.norm();
  const double diffnorm = (d == 0.0) ? n : (n / d);
  if (diffnorm > tolerance) {
    *result_listener << "\nx: " << x.transpose()
                     << "\nexpected: " << delta.transpose()
                     << "\nactual:" << actual.transpose()
                     << "\ndiff:" << (delta - actual).transpose()
                     << "\ndiffnorm: " << diffnorm;
    return false;
  }
  return true;
}

// Checks that the invariant Plus(Minus(y, x), x) == y holds.
MATCHER_P3(PlusMinusIsIdentityAt, x, y, tolerance, "") {
  const int ambient_size = arg.AmbientSize();
  const int tangent_size = arg.TangentSize();

  Vector y_minus_x = Vector::Zero(tangent_size);
  EXPECT_TRUE(arg.Minus(y.data(), x.data(), y_minus_x.data()));

  Vector actual = Vector::Zero(ambient_size);
  EXPECT_TRUE(arg.Plus(x.data(), y_minus_x.data(), actual.data()));

  const double n = (actual - Vector{y}).norm();
  const double d = y.norm();
  const double diffnorm = (d == 0.0) ? n : (n / d);
  if (diffnorm > tolerance) {
    *result_listener << "\nx: " << x.transpose()
                     << "\nexpected: " << y.transpose()
                     << "\nactual:" << actual.transpose()
                     << "\ndiff:" << (y - actual).transpose()
                     << "\ndiffnorm: " << diffnorm;
    return false;
  }
  return true;
}

// Helper struct to curry Minus(., x) so that it can be numerically
// differentiated.
struct MinusFunctor {
  MinusFunctor(const Manifold& manifold, const double* x)
      : manifold(manifold), x(x) {}
  bool operator()(double const* const* parameters, double* y_minus_x) const {
    return manifold.Minus(parameters[0], x, y_minus_x);
  }

  const Manifold& manifold;
  const double* x;
};

// Checks that the output of MinusJacobian matches the one obtained by
// numerically evaluating D_1 Minus(x,x).
MATCHER_P2(HasCorrectMinusJacobianAt, x, tolerance, "") {
  const int ambient_size = arg.AmbientSize();
  const int tangent_size = arg.TangentSize();

  Vector y = x;
  Vector y_minus_x = Vector::Zero(tangent_size);

  NumericDiffOptions options;
  options.ridders_relative_initial_step_size = 1e-4;
  DynamicNumericDiffCostFunction<MinusFunctor, RIDDERS> cost_function(
      new MinusFunctor(arg, x.data()), TAKE_OWNERSHIP, options);
  cost_function.AddParameterBlock(ambient_size);
  cost_function.SetNumResiduals(tangent_size);

  double* parameters[1] = {y.data()};

  Matrix expected = Matrix::Zero(tangent_size, ambient_size);
  double* jacobians[1] = {expected.data()};

  EXPECT_TRUE(cost_function.Evaluate(parameters, y_minus_x.data(), jacobians));

  Matrix actual = Matrix::Random(tangent_size, ambient_size);
  EXPECT_TRUE(arg.MinusJacobian(x.data(), actual.data()));

  const double n = (actual - expected).norm();
  const double d = expected.norm();
  const double diffnorm = (d == 0.0) ? n : (n / d);
  if (diffnorm > tolerance) {
    *result_listener << "\nx: " << x.transpose() << "\nexpected: \n"
                     << expected << "\nactual:\n"
                     << actual << "\ndiff:\n"
                     << expected - actual << "\ndiffnorm: " << diffnorm;
    return false;
  }
  return true;
}

// Checks that D_delta Minus(Plus(x, delta), x) at delta = 0 is an identity
// matrix.
MATCHER_P2(MinusPlusJacobianIsIdentityAt, x, tolerance, "") {
  const int ambient_size = arg.AmbientSize();
  const int tangent_size = arg.TangentSize();

  Matrix plus_jacobian(ambient_size, tangent_size);
  EXPECT_TRUE(arg.PlusJacobian(x.data(), plus_jacobian.data()));
  Matrix minus_jacobian(tangent_size, ambient_size);
  EXPECT_TRUE(arg.MinusJacobian(x.data(), minus_jacobian.data()));

  const Matrix actual = minus_jacobian * plus_jacobian;
  const Matrix expected = Matrix::Identity(tangent_size, tangent_size);

  const double n = (actual - expected).norm();
  const double d = expected.norm();
  const double diffnorm = n / d;
  if (diffnorm > tolerance) {
    *result_listener << "\nx: " << x.transpose() << "\nexpected: \n"
                     << expected << "\nactual:\n"
                     << actual << "\ndiff:\n"
                     << expected - actual << "\ndiffnorm: " << diffnorm;

    return false;
  }
  return true;
}

// Verify that the output of RightMultiplyByPlusJacobian is ambient_matrix *
// plus_jacobian.
MATCHER_P2(HasCorrectRightMultiplyByPlusJacobianAt, x, tolerance, "") {
  const int ambient_size = arg.AmbientSize();
  const int tangent_size = arg.TangentSize();

  constexpr int kMinNumRows = 0;
  constexpr int kMaxNumRows = 3;
  for (int num_rows = kMinNumRows; num_rows <= kMaxNumRows; ++num_rows) {
    Matrix plus_jacobian = Matrix::Random(ambient_size, tangent_size);
    EXPECT_TRUE(arg.PlusJacobian(x.data(), plus_jacobian.data()));

    Matrix ambient_matrix = Matrix::Random(num_rows, ambient_size);
    Matrix expected = ambient_matrix * plus_jacobian;

    Matrix actual = Matrix::Random(num_rows, tangent_size);
    EXPECT_TRUE(arg.RightMultiplyByPlusJacobian(
        x.data(), num_rows, ambient_matrix.data(), actual.data()));
    const double n = (actual - expected).norm();
    const double d = expected.norm();
    const double diffnorm = (d == 0.0) ? n : (n / d);
    if (diffnorm > tolerance) {
      *result_listener << "\nx: " << x.transpose() << "\nambient_matrix : \n"
                       << ambient_matrix << "\nplus_jacobian : \n"
                       << plus_jacobian << "\nexpected: \n"
                       << expected << "\nactual:\n"
                       << actual << "\ndiff:\n"
                       << expected - actual << "\ndiffnorm : " << diffnorm;
      return false;
    }
  }
  return true;
}

}  // namespace ceres
