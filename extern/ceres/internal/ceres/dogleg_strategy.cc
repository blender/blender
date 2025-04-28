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

#include "ceres/dogleg_strategy.h"

#include <algorithm>
#include <cmath>

#include "Eigen/Dense"
#include "ceres/array_utils.h"
#include "ceres/internal/eigen.h"
#include "ceres/linear_least_squares_problems.h"
#include "ceres/linear_solver.h"
#include "ceres/polynomial.h"
#include "ceres/sparse_matrix.h"
#include "ceres/trust_region_strategy.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres::internal {
namespace {
const double kMaxMu = 1.0;
const double kMinMu = 1e-8;
}  // namespace

DoglegStrategy::DoglegStrategy(const TrustRegionStrategy::Options& options)
    : linear_solver_(options.linear_solver),
      radius_(options.initial_radius),
      max_radius_(options.max_radius),
      min_diagonal_(options.min_lm_diagonal),
      max_diagonal_(options.max_lm_diagonal),
      mu_(kMinMu),
      min_mu_(kMinMu),
      max_mu_(kMaxMu),
      mu_increase_factor_(10.0),
      increase_threshold_(0.75),
      decrease_threshold_(0.25),
      dogleg_step_norm_(0.0),
      reuse_(false),
      dogleg_type_(options.dogleg_type) {
  CHECK(linear_solver_ != nullptr);
  CHECK_GT(min_diagonal_, 0.0);
  CHECK_LE(min_diagonal_, max_diagonal_);
  CHECK_GT(max_radius_, 0.0);
}

// If the reuse_ flag is not set, then the Cauchy point (scaled
// gradient) and the new Gauss-Newton step are computed from
// scratch. The Dogleg step is then computed as interpolation of these
// two vectors.
TrustRegionStrategy::Summary DoglegStrategy::ComputeStep(
    const TrustRegionStrategy::PerSolveOptions& per_solve_options,
    SparseMatrix* jacobian,
    const double* residuals,
    double* step) {
  CHECK(jacobian != nullptr);
  CHECK(residuals != nullptr);
  CHECK(step != nullptr);

  const int n = jacobian->num_cols();
  if (reuse_) {
    // Gauss-Newton and gradient vectors are always available, only a
    // new interpolant need to be computed. For the subspace case,
    // the subspace and the two-dimensional model are also still valid.
    switch (dogleg_type_) {
      case TRADITIONAL_DOGLEG:
        ComputeTraditionalDoglegStep(step);
        break;

      case SUBSPACE_DOGLEG:
        ComputeSubspaceDoglegStep(step);
        break;
    }
    TrustRegionStrategy::Summary summary;
    summary.num_iterations = 0;
    summary.termination_type = LinearSolverTerminationType::SUCCESS;
    return summary;
  }

  reuse_ = true;
  // Check that we have the storage needed to hold the various
  // temporary vectors.
  if (diagonal_.rows() != n) {
    diagonal_.resize(n, 1);
    gradient_.resize(n, 1);
    gauss_newton_step_.resize(n, 1);
  }

  // Vector used to form the diagonal matrix that is used to
  // regularize the Gauss-Newton solve and that defines the
  // elliptical trust region
  //
  //   || D * step || <= radius_ .
  //
  jacobian->SquaredColumnNorm(diagonal_.data());
  for (int i = 0; i < n; ++i) {
    diagonal_[i] =
        std::min(std::max(diagonal_[i], min_diagonal_), max_diagonal_);
  }
  diagonal_ = diagonal_.array().sqrt();

  ComputeGradient(jacobian, residuals);
  ComputeCauchyPoint(jacobian);

  LinearSolver::Summary linear_solver_summary =
      ComputeGaussNewtonStep(per_solve_options, jacobian, residuals);

  TrustRegionStrategy::Summary summary;
  summary.residual_norm = linear_solver_summary.residual_norm;
  summary.num_iterations = linear_solver_summary.num_iterations;
  summary.termination_type = linear_solver_summary.termination_type;

  if (linear_solver_summary.termination_type ==
      LinearSolverTerminationType::FATAL_ERROR) {
    return summary;
  }

  if (linear_solver_summary.termination_type !=
      LinearSolverTerminationType::FAILURE) {
    switch (dogleg_type_) {
      // Interpolate the Cauchy point and the Gauss-Newton step.
      case TRADITIONAL_DOGLEG:
        ComputeTraditionalDoglegStep(step);
        break;

      // Find the minimum in the subspace defined by the
      // Cauchy point and the (Gauss-)Newton step.
      case SUBSPACE_DOGLEG:
        if (!ComputeSubspaceModel(jacobian)) {
          summary.termination_type = LinearSolverTerminationType::FAILURE;
          break;
        }
        ComputeSubspaceDoglegStep(step);
        break;
    }
  }

  return summary;
}

// The trust region is assumed to be elliptical with the
// diagonal scaling matrix D defined by sqrt(diagonal_).
// It is implemented by substituting step' = D * step.
// The trust region for step' is spherical.
// The gradient, the Gauss-Newton step, the Cauchy point,
// and all calculations involving the Jacobian have to
// be adjusted accordingly.
void DoglegStrategy::ComputeGradient(SparseMatrix* jacobian,
                                     const double* residuals) {
  gradient_.setZero();
  jacobian->LeftMultiplyAndAccumulate(residuals, gradient_.data());
  gradient_.array() /= diagonal_.array();
}

// The Cauchy point is the global minimizer of the quadratic model
// along the one-dimensional subspace spanned by the gradient.
void DoglegStrategy::ComputeCauchyPoint(SparseMatrix* jacobian) {
  // alpha * -gradient is the Cauchy point.
  Vector Jg(jacobian->num_rows());
  Jg.setZero();
  // The Jacobian is scaled implicitly by computing J * (D^-1 * (D^-1 * g))
  // instead of (J * D^-1) * (D^-1 * g).
  Vector scaled_gradient = (gradient_.array() / diagonal_.array()).matrix();
  jacobian->RightMultiplyAndAccumulate(scaled_gradient.data(), Jg.data());
  alpha_ = gradient_.squaredNorm() / Jg.squaredNorm();
}

// The dogleg step is defined as the intersection of the trust region
// boundary with the piecewise linear path from the origin to the Cauchy
// point and then from there to the Gauss-Newton point (global minimizer
// of the model function). The Gauss-Newton point is taken if it lies
// within the trust region.
void DoglegStrategy::ComputeTraditionalDoglegStep(double* dogleg) {
  VectorRef dogleg_step(dogleg, gradient_.rows());

  // Case 1. The Gauss-Newton step lies inside the trust region, and
  // is therefore the optimal solution to the trust-region problem.
  const double gradient_norm = gradient_.norm();
  const double gauss_newton_norm = gauss_newton_step_.norm();
  if (gauss_newton_norm <= radius_) {
    dogleg_step = gauss_newton_step_;
    dogleg_step_norm_ = gauss_newton_norm;
    dogleg_step.array() /= diagonal_.array();
    VLOG(3) << "GaussNewton step size: " << dogleg_step_norm_
            << " radius: " << radius_;
    return;
  }

  // Case 2. The Cauchy point and the Gauss-Newton steps lie outside
  // the trust region. Rescale the Cauchy point to the trust region
  // and return.
  if (gradient_norm * alpha_ >= radius_) {
    dogleg_step = -(radius_ / gradient_norm) * gradient_;
    dogleg_step_norm_ = radius_;
    dogleg_step.array() /= diagonal_.array();
    VLOG(3) << "Cauchy step size: " << dogleg_step_norm_
            << " radius: " << radius_;
    return;
  }

  // Case 3. The Cauchy point is inside the trust region and the
  // Gauss-Newton step is outside. Compute the line joining the two
  // points and the point on it which intersects the trust region
  // boundary.

  // a = alpha * -gradient
  // b = gauss_newton_step
  const double b_dot_a = -alpha_ * gradient_.dot(gauss_newton_step_);
  const double a_squared_norm = pow(alpha_ * gradient_norm, 2.0);
  const double b_minus_a_squared_norm =
      a_squared_norm - 2 * b_dot_a + pow(gauss_newton_norm, 2);

  // c = a' (b - a)
  //   = alpha * -gradient' gauss_newton_step - alpha^2 |gradient|^2
  const double c = b_dot_a - a_squared_norm;
  const double d = sqrt(c * c + b_minus_a_squared_norm *
                                    (pow(radius_, 2.0) - a_squared_norm));

  double beta = (c <= 0) ? (d - c) / b_minus_a_squared_norm
                         : (radius_ * radius_ - a_squared_norm) / (d + c);
  dogleg_step =
      (-alpha_ * (1.0 - beta)) * gradient_ + beta * gauss_newton_step_;
  dogleg_step_norm_ = dogleg_step.norm();
  dogleg_step.array() /= diagonal_.array();
  VLOG(3) << "Dogleg step size: " << dogleg_step_norm_
          << " radius: " << radius_;
}

// The subspace method finds the minimum of the two-dimensional problem
//
//   min. 1/2 x' B' H B x + g' B x
//   s.t. || B x ||^2 <= r^2
//
// where r is the trust region radius and B is the matrix with unit columns
// spanning the subspace defined by the steepest descent and Newton direction.
// This subspace by definition includes the Gauss-Newton point, which is
// therefore taken if it lies within the trust region.
void DoglegStrategy::ComputeSubspaceDoglegStep(double* dogleg) {
  VectorRef dogleg_step(dogleg, gradient_.rows());

  // The Gauss-Newton point is inside the trust region if |GN| <= radius_.
  // This test is valid even though radius_ is a length in the two-dimensional
  // subspace while gauss_newton_step_ is expressed in the (scaled)
  // higher dimensional original space. This is because
  //
  //   1. gauss_newton_step_ by definition lies in the subspace, and
  //   2. the subspace basis is orthonormal.
  //
  // As a consequence, the norm of the gauss_newton_step_ in the subspace is
  // the same as its norm in the original space.
  const double gauss_newton_norm = gauss_newton_step_.norm();
  if (gauss_newton_norm <= radius_) {
    dogleg_step = gauss_newton_step_;
    dogleg_step_norm_ = gauss_newton_norm;
    dogleg_step.array() /= diagonal_.array();
    VLOG(3) << "GaussNewton step size: " << dogleg_step_norm_
            << " radius: " << radius_;
    return;
  }

  // The optimum lies on the boundary of the trust region. The above problem
  // therefore becomes
  //
  //   min. 1/2 x^T B^T H B x + g^T B x
  //   s.t. || B x ||^2 = r^2
  //
  // Notice the equality in the constraint.
  //
  // This can be solved by forming the Lagrangian, solving for x(y), where
  // y is the Lagrange multiplier, using the gradient of the objective, and
  // putting x(y) back into the constraint. This results in a fourth order
  // polynomial in y, which can be solved using e.g. the companion matrix.
  // See the description of MakePolynomialForBoundaryConstrainedProblem for
  // details. The result is up to four real roots y*, not all of which
  // correspond to feasible points. The feasible points x(y*) have to be
  // tested for optimality.

  if (subspace_is_one_dimensional_) {
    // The subspace is one-dimensional, so both the gradient and
    // the Gauss-Newton step point towards the same direction.
    // In this case, we move along the gradient until we reach the trust
    // region boundary.
    dogleg_step = -(radius_ / gradient_.norm()) * gradient_;
    dogleg_step_norm_ = radius_;
    dogleg_step.array() /= diagonal_.array();
    VLOG(3) << "Dogleg subspace step size (1D): " << dogleg_step_norm_
            << " radius: " << radius_;
    return;
  }

  Vector2d minimum(0.0, 0.0);
  if (!FindMinimumOnTrustRegionBoundary(&minimum)) {
    // For the positive semi-definite case, a traditional dogleg step
    // is taken in this case.
    LOG(WARNING) << "Failed to compute polynomial roots. "
                 << "Taking traditional dogleg step instead.";
    ComputeTraditionalDoglegStep(dogleg);
    return;
  }

  // Test first order optimality at the minimum.
  // The first order KKT conditions state that the minimum x*
  // has to satisfy either || x* ||^2 < r^2 (i.e. has to lie within
  // the trust region), or
  //
  //   (B x* + g) + y x* = 0
  //
  // for some positive scalar y.
  // Here, as it is already known that the minimum lies on the boundary, the
  // latter condition is tested. To allow for small imprecisions, we test if
  // the angle between (B x* + g) and -x* is smaller than acos(0.99).
  // The exact value of the cosine is arbitrary but should be close to 1.
  //
  // This condition should not be violated. If it is, the minimum was not
  // correctly determined.
  const double kCosineThreshold = 0.99;
  const Vector2d grad_minimum = subspace_B_ * minimum + subspace_g_;
  const double cosine_angle =
      -minimum.dot(grad_minimum) / (minimum.norm() * grad_minimum.norm());
  if (cosine_angle < kCosineThreshold) {
    LOG(WARNING) << "First order optimality seems to be violated "
                 << "in the subspace method!\n"
                 << "Cosine of angle between x and B x + g is " << cosine_angle
                 << ".\n"
                 << "Taking a regular dogleg step instead.\n"
                 << "Please consider filing a bug report if this "
                 << "happens frequently or consistently.\n";
    ComputeTraditionalDoglegStep(dogleg);
    return;
  }

  // Create the full step from the optimal 2d solution.
  dogleg_step = subspace_basis_ * minimum;
  dogleg_step_norm_ = radius_;
  dogleg_step.array() /= diagonal_.array();
  VLOG(3) << "Dogleg subspace step size: " << dogleg_step_norm_
          << " radius: " << radius_;
}

// Build the polynomial that defines the optimal Lagrange multipliers.
// Let the Lagrangian be
//
//   L(x, y) = 0.5 x^T B x + x^T g + y (0.5 x^T x - 0.5 r^2).       (1)
//
// Stationary points of the Lagrangian are given by
//
//   0 = d L(x, y) / dx = Bx + g + y x                              (2)
//   0 = d L(x, y) / dy = 0.5 x^T x - 0.5 r^2                       (3)
//
// For any given y, we can solve (2) for x as
//
//   x(y) = -(B + y I)^-1 g .                                       (4)
//
// As B + y I is 2x2, we form the inverse explicitly:
//
//   (B + y I)^-1 = (1 / det(B + y I)) adj(B + y I)                 (5)
//
// where adj() denotes adjugation. This should be safe, as B is positive
// semi-definite and y is necessarily positive, so (B + y I) is indeed
// invertible.
// Plugging (5) into (4) and the result into (3), then dividing by 0.5 we
// obtain
//
//   0 = (1 / det(B + y I))^2 g^T adj(B + y I)^T adj(B + y I) g - r^2
//                                                                  (6)
//
// or
//
//   det(B + y I)^2 r^2 = g^T adj(B + y I)^T adj(B + y I) g         (7a)
//                      = g^T adj(B)^T adj(B) g
//                           + 2 y g^T adj(B)^T g + y^2 g^T g       (7b)
//
// as
//
//   adj(B + y I) = adj(B) + y I = adj(B)^T + y I .                 (8)
//
// The left hand side can be expressed explicitly using
//
//   det(B + y I) = det(B) + y tr(B) + y^2 .                        (9)
//
// So (7) is a polynomial in y of degree four.
// Bringing everything back to the left hand side, the coefficients can
// be read off as
//
//     y^4  r^2
//   + y^3  2 r^2 tr(B)
//   + y^2 (r^2 tr(B)^2 + 2 r^2 det(B) - g^T g)
//   + y^1 (2 r^2 det(B) tr(B) - 2 g^T adj(B)^T g)
//   + y^0 (r^2 det(B)^2 - g^T adj(B)^T adj(B) g)
//
Vector DoglegStrategy::MakePolynomialForBoundaryConstrainedProblem() const {
  const double detB = subspace_B_.determinant();
  const double trB = subspace_B_.trace();
  const double r2 = radius_ * radius_;
  Matrix2d B_adj;
  // clang-format off
  B_adj <<  subspace_B_(1, 1) , -subspace_B_(0, 1),
           -subspace_B_(1, 0) ,  subspace_B_(0, 0);
  // clang-format on

  Vector polynomial(5);
  polynomial(0) = r2;
  polynomial(1) = 2.0 * r2 * trB;
  polynomial(2) = r2 * (trB * trB + 2.0 * detB) - subspace_g_.squaredNorm();
  polynomial(3) =
      -2.0 * (subspace_g_.transpose() * B_adj * subspace_g_ - r2 * detB * trB);
  polynomial(4) = r2 * detB * detB - (B_adj * subspace_g_).squaredNorm();

  return polynomial;
}

// Given a Lagrange multiplier y that corresponds to a stationary point
// of the Lagrangian L(x, y), compute the corresponding x from the
// equation
//
//   0 = d L(x, y) / dx
//     = B * x + g + y * x
//     = (B + y * I) * x + g
//
DoglegStrategy::Vector2d DoglegStrategy::ComputeSubspaceStepFromRoot(
    double y) const {
  const Matrix2d B_i = subspace_B_ + y * Matrix2d::Identity();
  return -B_i.partialPivLu().solve(subspace_g_);
}

// This function evaluates the quadratic model at a point x in the
// subspace spanned by subspace_basis_.
double DoglegStrategy::EvaluateSubspaceModel(const Vector2d& x) const {
  return 0.5 * x.dot(subspace_B_ * x) + subspace_g_.dot(x);
}

// This function attempts to solve the boundary-constrained subspace problem
//
//   min. 1/2 x^T B^T H B x + g^T B x
//   s.t. || B x ||^2 = r^2
//
// where B is an orthonormal subspace basis and r is the trust-region radius.
//
// This is done by finding the roots of a fourth degree polynomial. If the
// root finding fails, the function returns false and minimum will be set
// to (0, 0). If it succeeds, true is returned.
//
// In the failure case, another step should be taken, such as the traditional
// dogleg step.
bool DoglegStrategy::FindMinimumOnTrustRegionBoundary(Vector2d* minimum) const {
  CHECK(minimum != nullptr);

  // Return (0, 0) in all error cases.
  minimum->setZero();

  // Create the fourth-degree polynomial that is a necessary condition for
  // optimality.
  const Vector polynomial = MakePolynomialForBoundaryConstrainedProblem();

  // Find the real parts y_i of its roots (not only the real roots).
  Vector roots_real;
  if (!FindPolynomialRoots(polynomial, &roots_real, nullptr)) {
    // Failed to find the roots of the polynomial, i.e. the candidate
    // solutions of the constrained problem. Report this back to the caller.
    return false;
  }

  // For each root y, compute B x(y) and check for feasibility.
  // Notice that there should always be four roots, as the leading term of
  // the polynomial is r^2 and therefore non-zero. However, as some roots
  // may be complex, the real parts are not necessarily unique.
  double minimum_value = std::numeric_limits<double>::max();
  bool valid_root_found = false;
  for (int i = 0; i < roots_real.size(); ++i) {
    const Vector2d x_i = ComputeSubspaceStepFromRoot(roots_real(i));

    // Not all roots correspond to points on the trust region boundary.
    // There are at most four candidate solutions. As we are interested
    // in the minimum, it is safe to consider all of them after projecting
    // them onto the trust region boundary.
    if (x_i.norm() > 0) {
      const double f_i = EvaluateSubspaceModel((radius_ / x_i.norm()) * x_i);
      valid_root_found = true;
      if (f_i < minimum_value) {
        minimum_value = f_i;
        *minimum = x_i;
      }
    }
  }

  return valid_root_found;
}

LinearSolver::Summary DoglegStrategy::ComputeGaussNewtonStep(
    const PerSolveOptions& per_solve_options,
    SparseMatrix* jacobian,
    const double* residuals) {
  const int n = jacobian->num_cols();
  LinearSolver::Summary linear_solver_summary;
  linear_solver_summary.termination_type = LinearSolverTerminationType::FAILURE;

  // The Jacobian matrix is often quite poorly conditioned. Thus it is
  // necessary to add a diagonal matrix at the bottom to prevent the
  // linear solver from failing.
  //
  // We do this by computing the same diagonal matrix as the one used
  // by Levenberg-Marquardt (other choices are possible), and scaling
  // it by a small constant (independent of the trust region radius).
  //
  // If the solve fails, the multiplier to the diagonal is increased
  // up to max_mu_ by a factor of mu_increase_factor_ every time. If
  // the linear solver is still not successful, the strategy returns
  // with LinearSolverTerminationType::FAILURE.
  //
  // Next time when a new Gauss-Newton step is requested, the
  // multiplier starts out from the last successful solve.
  //
  // When a step is declared successful, the multiplier is decreased
  // by half of mu_increase_factor_.

  while (mu_ < max_mu_) {
    // Dogleg, as far as I (sameeragarwal) understand it, requires a
    // reasonably good estimate of the Gauss-Newton step. This means
    // that we need to solve the normal equations more or less
    // exactly. This is reflected in the values of the tolerances set
    // below.
    //
    // For now, this strategy should only be used with exact
    // factorization based solvers, for which these tolerances are
    // automatically satisfied.
    //
    // The right way to combine inexact solves with trust region
    // methods is to use Stiehaug's method.
    LinearSolver::PerSolveOptions solve_options;
    solve_options.q_tolerance = 0.0;
    solve_options.r_tolerance = 0.0;

    lm_diagonal_ = diagonal_ * std::sqrt(mu_);
    solve_options.D = lm_diagonal_.data();

    // As in the LevenbergMarquardtStrategy, solve Jy = r instead
    // of Jx = -r and later set x = -y to avoid having to modify
    // either jacobian or residuals.
    InvalidateArray(n, gauss_newton_step_.data());
    linear_solver_summary = linear_solver_->Solve(
        jacobian, residuals, solve_options, gauss_newton_step_.data());

    if (per_solve_options.dump_format_type == CONSOLE ||
        (per_solve_options.dump_format_type != CONSOLE &&
         !per_solve_options.dump_filename_base.empty())) {
      if (!DumpLinearLeastSquaresProblem(per_solve_options.dump_filename_base,
                                         per_solve_options.dump_format_type,
                                         jacobian,
                                         solve_options.D,
                                         residuals,
                                         gauss_newton_step_.data(),
                                         0)) {
        LOG(ERROR) << "Unable to dump trust region problem."
                   << " Filename base: "
                   << per_solve_options.dump_filename_base;
      }
    }

    if (linear_solver_summary.termination_type ==
        LinearSolverTerminationType::FATAL_ERROR) {
      return linear_solver_summary;
    }

    if (linear_solver_summary.termination_type ==
            LinearSolverTerminationType::FAILURE ||
        !IsArrayValid(n, gauss_newton_step_.data())) {
      mu_ *= mu_increase_factor_;
      VLOG(2) << "Increasing mu " << mu_;
      linear_solver_summary.termination_type =
          LinearSolverTerminationType::FAILURE;
      continue;
    }
    break;
  }

  if (linear_solver_summary.termination_type !=
      LinearSolverTerminationType::FAILURE) {
    // The scaled Gauss-Newton step is D * GN:
    //
    //     - (D^-1 J^T J D^-1)^-1 (D^-1 g)
    //   = - D (J^T J)^-1 D D^-1 g
    //   = D -(J^T J)^-1 g
    //
    gauss_newton_step_.array() *= -diagonal_.array();
  }

  return linear_solver_summary;
}

void DoglegStrategy::StepAccepted(double step_quality) {
  CHECK_GT(step_quality, 0.0);

  if (step_quality < decrease_threshold_) {
    radius_ *= 0.5;
  }

  if (step_quality > increase_threshold_) {
    radius_ = std::max(radius_, 3.0 * dogleg_step_norm_);
  }

  // Reduce the regularization multiplier, in the hope that whatever
  // was causing the rank deficiency has gone away and we can return
  // to doing a pure Gauss-Newton solve.
  mu_ = std::max(min_mu_, 2.0 * mu_ / mu_increase_factor_);
  reuse_ = false;
}

void DoglegStrategy::StepRejected(double /*step_quality*/) {
  radius_ *= 0.5;
  reuse_ = true;
}

void DoglegStrategy::StepIsInvalid() {
  mu_ *= mu_increase_factor_;
  reuse_ = false;
}

double DoglegStrategy::Radius() const { return radius_; }

bool DoglegStrategy::ComputeSubspaceModel(SparseMatrix* jacobian) {
  // Compute an orthogonal basis for the subspace using QR decomposition.
  Matrix basis_vectors(jacobian->num_cols(), 2);
  basis_vectors.col(0) = gradient_;
  basis_vectors.col(1) = gauss_newton_step_;
  Eigen::ColPivHouseholderQR<Matrix> basis_qr(basis_vectors);

  switch (basis_qr.rank()) {
    case 0:
      // This should never happen, as it implies that both the gradient
      // and the Gauss-Newton step are zero. In this case, the minimizer should
      // have stopped due to the gradient being too small.
      LOG(ERROR) << "Rank of subspace basis is 0. "
                 << "This means that the gradient at the current iterate is "
                 << "zero but the optimization has not been terminated. "
                 << "You may have found a bug in Ceres.";
      return false;

    case 1:
      // Gradient and Gauss-Newton step coincide, so we lie on one of the
      // major axes of the quadratic problem. In this case, we simply move
      // along the gradient until we reach the trust region boundary.
      subspace_is_one_dimensional_ = true;
      return true;

    case 2:
      subspace_is_one_dimensional_ = false;
      break;

    default:
      LOG(ERROR) << "Rank of the subspace basis matrix is reported to be "
                 << "greater than 2. As the matrix contains only two "
                 << "columns this cannot be true and is indicative of "
                 << "a bug.";
      return false;
  }

  // The subspace is two-dimensional, so compute the subspace model.
  // Given the basis U, this is
  //
  //   subspace_g_ = g_scaled^T U
  //
  // and
  //
  //   subspace_B_ = U^T (J_scaled^T J_scaled) U
  //
  // As J_scaled = J * D^-1, the latter becomes
  //
  //   subspace_B_ = ((U^T D^-1) J^T) (J (D^-1 U))
  //               = (J (D^-1 U))^T (J (D^-1 U))

  subspace_basis_ =
      basis_qr.householderQ() * Matrix::Identity(jacobian->num_cols(), 2);

  subspace_g_ = subspace_basis_.transpose() * gradient_;

  Eigen::Matrix<double, 2, Eigen::Dynamic, Eigen::RowMajor> Jb(
      2, jacobian->num_rows());
  Jb.setZero();

  Vector tmp;
  tmp = (subspace_basis_.col(0).array() / diagonal_.array()).matrix();
  jacobian->RightMultiplyAndAccumulate(tmp.data(), Jb.row(0).data());
  tmp = (subspace_basis_.col(1).array() / diagonal_.array()).matrix();
  jacobian->RightMultiplyAndAccumulate(tmp.data(), Jb.row(1).data());

  subspace_B_ = Jb * Jb.transpose();

  return true;
}

}  // namespace ceres::internal
