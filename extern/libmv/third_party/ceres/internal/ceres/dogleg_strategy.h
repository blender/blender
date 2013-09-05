// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2012 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
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

#ifndef CERES_INTERNAL_DOGLEG_STRATEGY_H_
#define CERES_INTERNAL_DOGLEG_STRATEGY_H_

#include "ceres/linear_solver.h"
#include "ceres/trust_region_strategy.h"

namespace ceres {
namespace internal {

// Dogleg step computation and trust region sizing strategy based on
// on "Methods for Nonlinear Least Squares" by K. Madsen, H.B. Nielsen
// and O. Tingleff. Available to download from
//
// http://www2.imm.dtu.dk/pubdb/views/edoc_download.php/3215/pdf/imm3215.pdf
//
// One minor modification is that instead of computing the pure
// Gauss-Newton step, we compute a regularized version of it. This is
// because the Jacobian is often rank-deficient and in such cases
// using a direct solver leads to numerical failure.
//
// If SUBSPACE is passed as the type argument to the constructor, the
// DoglegStrategy follows the approach by Shultz, Schnabel, Byrd.
// This finds the exact optimum over the two-dimensional subspace
// spanned by the two Dogleg vectors.
class DoglegStrategy : public TrustRegionStrategy {
 public:
  explicit DoglegStrategy(const TrustRegionStrategy::Options& options);
  virtual ~DoglegStrategy() {}

  // TrustRegionStrategy interface
  virtual Summary ComputeStep(const PerSolveOptions& per_solve_options,
                              SparseMatrix* jacobian,
                              const double* residuals,
                              double* step);
  virtual void StepAccepted(double step_quality);
  virtual void StepRejected(double step_quality);
  virtual void StepIsInvalid();

  virtual double Radius() const;

  // These functions are predominantly for testing.
  Vector gradient() const { return gradient_; }
  Vector gauss_newton_step() const { return gauss_newton_step_; }
  Matrix subspace_basis() const { return subspace_basis_; }
  Vector subspace_g() const { return subspace_g_; }
  Matrix subspace_B() const { return subspace_B_; }

 private:
  typedef Eigen::Matrix<double, 2, 1, Eigen::DontAlign> Vector2d;
  typedef Eigen::Matrix<double, 2, 2, Eigen::DontAlign> Matrix2d;

  LinearSolver::Summary ComputeGaussNewtonStep(
      const PerSolveOptions& per_solve_options,
      SparseMatrix* jacobian,
      const double* residuals);
  void ComputeCauchyPoint(SparseMatrix* jacobian);
  void ComputeGradient(SparseMatrix* jacobian, const double* residuals);
  void ComputeTraditionalDoglegStep(double* step);
  bool ComputeSubspaceModel(SparseMatrix* jacobian);
  void ComputeSubspaceDoglegStep(double* step);

  bool FindMinimumOnTrustRegionBoundary(Vector2d* minimum) const;
  Vector MakePolynomialForBoundaryConstrainedProblem() const;
  Vector2d ComputeSubspaceStepFromRoot(double lambda) const;
  double EvaluateSubspaceModel(const Vector2d& x) const;

  LinearSolver* linear_solver_;
  double radius_;
  const double max_radius_;

  const double min_diagonal_;
  const double max_diagonal_;

  // mu is used to scale the diagonal matrix used to make the
  // Gauss-Newton solve full rank. In each solve, the strategy starts
  // out with mu = min_mu, and tries values upto max_mu. If the user
  // reports an invalid step, the value of mu_ is increased so that
  // the next solve starts with a stronger regularization.
  //
  // If a successful step is reported, then the value of mu_ is
  // decreased with a lower bound of min_mu_.
  double mu_;
  const double min_mu_;
  const double max_mu_;
  const double mu_increase_factor_;
  const double increase_threshold_;
  const double decrease_threshold_;

  Vector diagonal_;  // sqrt(diag(J^T J))
  Vector lm_diagonal_;

  Vector gradient_;
  Vector gauss_newton_step_;

  // cauchy_step = alpha * gradient
  double alpha_;
  double dogleg_step_norm_;

  // When, ComputeStep is called, reuse_ indicates whether the
  // Gauss-Newton and Cauchy steps from the last call to ComputeStep
  // can be reused or not.
  //
  // If the user called StepAccepted, then it is expected that the
  // user has recomputed the Jacobian matrix and new Gauss-Newton
  // solve is needed and reuse is set to false.
  //
  // If the user called StepRejected, then it is expected that the
  // user wants to solve the trust region problem with the same matrix
  // but a different trust region radius and the Gauss-Newton and
  // Cauchy steps can be reused to compute the Dogleg, thus reuse is
  // set to true.
  //
  // If the user called StepIsInvalid, then there was a numerical
  // problem with the step computed in the last call to ComputeStep,
  // and the regularization used to do the Gauss-Newton solve is
  // increased and a new solve should be done when ComputeStep is
  // called again, thus reuse is set to false.
  bool reuse_;

  // The dogleg type determines how the minimum of the local
  // quadratic model is found.
  DoglegType dogleg_type_;

  // If the type is SUBSPACE_DOGLEG, the two-dimensional
  // model 1/2 x^T B x + g^T x has to be computed and stored.
  bool subspace_is_one_dimensional_;
  Matrix subspace_basis_;
  Vector2d subspace_g_;
  Matrix2d subspace_B_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_DOGLEG_STRATEGY_H_
