// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
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

#include <list>

#include "ceres/internal/eigen.h"
#include "ceres/low_rank_inverse_hessian.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

using std::list;

// The (L)BFGS algorithm explicitly requires that the secant equation:
//
//   B_{k+1} * s_k = y_k
//
// Is satisfied at each iteration, where B_{k+1} is the approximated
// Hessian at the k+1-th iteration, s_k = (x_{k+1} - x_{k}) and
// y_k = (grad_{k+1} - grad_{k}). As the approximated Hessian must be
// positive definite, this is equivalent to the condition:
//
//   s_k^T * y_k > 0     [s_k^T * B_{k+1} * s_k = s_k^T * y_k > 0]
//
// This condition would always be satisfied if the function was strictly
// convex, alternatively, it is always satisfied provided that a Wolfe line
// search is used (even if the function is not strictly convex).  See [1]
// (p138) for a proof.
//
// Although Ceres will always use a Wolfe line search when using (L)BFGS,
// practical implementation considerations mean that the line search
// may return a point that satisfies only the Armijo condition, and thus
// could violate the Secant equation.  As such, we will only use a step
// to update the Hessian approximation if:
//
//   s_k^T * y_k > tolerance
//
// It is important that tolerance is very small (and >=0), as otherwise we
// might skip the update too often and fail to capture important curvature
// information in the Hessian.  For example going from 1e-10 -> 1e-14 improves
// the NIST benchmark score from 43/54 to 53/54.
//
// [1] Nocedal J., Wright S., Numerical Optimization, 2nd Ed. Springer, 1999.
//
// TODO(alexs.mac): Consider using Damped BFGS update instead of
// skipping update.
const double kLBFGSSecantConditionHessianUpdateTolerance = 1e-14;

LowRankInverseHessian::LowRankInverseHessian(
    int num_parameters,
    int max_num_corrections,
    bool use_approximate_eigenvalue_scaling)
    : num_parameters_(num_parameters),
      max_num_corrections_(max_num_corrections),
      use_approximate_eigenvalue_scaling_(use_approximate_eigenvalue_scaling),
      approximate_eigenvalue_scale_(1.0),
      delta_x_history_(num_parameters, max_num_corrections),
      delta_gradient_history_(num_parameters, max_num_corrections),
      delta_x_dot_delta_gradient_(max_num_corrections) {
}

bool LowRankInverseHessian::Update(const Vector& delta_x,
                                   const Vector& delta_gradient) {
  const double delta_x_dot_delta_gradient = delta_x.dot(delta_gradient);
  if (delta_x_dot_delta_gradient <=
      kLBFGSSecantConditionHessianUpdateTolerance) {
    VLOG(2) << "Skipping L-BFGS Update, delta_x_dot_delta_gradient too "
            << "small: " << delta_x_dot_delta_gradient << ", tolerance: "
            << kLBFGSSecantConditionHessianUpdateTolerance
            << " (Secant condition).";
    return false;
  }


  int next = indices_.size();
  // Once the size of the list reaches max_num_corrections_, simulate
  // a circular buffer by removing the first element of the list and
  // making it the next position where the LBFGS history is stored.
  if (next == max_num_corrections_) {
    next = indices_.front();
    indices_.pop_front();
  }

  indices_.push_back(next);
  delta_x_history_.col(next) = delta_x;
  delta_gradient_history_.col(next) = delta_gradient;
  delta_x_dot_delta_gradient_(next) = delta_x_dot_delta_gradient;
  approximate_eigenvalue_scale_ =
      delta_x_dot_delta_gradient / delta_gradient.squaredNorm();
  return true;
}

void LowRankInverseHessian::RightMultiply(const double* x_ptr,
                                          double* y_ptr) const {
  ConstVectorRef gradient(x_ptr, num_parameters_);
  VectorRef search_direction(y_ptr, num_parameters_);

  search_direction = gradient;

  const int num_corrections = indices_.size();
  Vector alpha(num_corrections);

  for (list<int>::const_reverse_iterator it = indices_.rbegin();
       it != indices_.rend();
       ++it) {
    const double alpha_i = delta_x_history_.col(*it).dot(search_direction) /
        delta_x_dot_delta_gradient_(*it);
    search_direction -= alpha_i * delta_gradient_history_.col(*it);
    alpha(*it) = alpha_i;
  }

  if (use_approximate_eigenvalue_scaling_) {
    // Rescale the initial inverse Hessian approximation (H_0) to be iteratively
    // updated so that it is of similar 'size' to the true inverse Hessian along
    // the most recent search direction.  As shown in [1]:
    //
    //   \gamma_k = (delta_gradient_{k-1}' * delta_x_{k-1}) /
    //              (delta_gradient_{k-1}' * delta_gradient_{k-1})
    //
    // Satisfies:
    //
    //   (1 / \lambda_m) <= \gamma_k <= (1 / \lambda_1)
    //
    // Where \lambda_1 & \lambda_m are the smallest and largest eigenvalues of
    // the true Hessian (not the inverse) along the most recent search direction
    // respectively.  Thus \gamma is an approximate eigenvalue of the true
    // inverse Hessian, and choosing: H_0 = I * \gamma will yield a starting
    // point that has a similar scale to the true inverse Hessian.  This
    // technique is widely reported to often improve convergence, however this
    // is not universally true, particularly if there are errors in the initial
    // jacobians, or if there are significant differences in the sensitivity
    // of the problem to the parameters (i.e. the range of the magnitudes of
    // the components of the gradient is large).
    //
    // The original origin of this rescaling trick is somewhat unclear, the
    // earliest reference appears to be Oren [1], however it is widely discussed
    // without specific attributation in various texts including [2] (p143/178).
    //
    // [1] Oren S.S., Self-scaling variable metric (SSVM) algorithms Part II:
    //     Implementation and experiments, Management Science,
    //     20(5), 863-874, 1974.
    // [2] Nocedal J., Wright S., Numerical Optimization, Springer, 1999.
    search_direction *= approximate_eigenvalue_scale_;

    VLOG(4) << "Applying approximate_eigenvalue_scale: "
            << approximate_eigenvalue_scale_ << " to initial inverse Hessian "
            << "approximation.";
  }

  for (const int i : indices_) {
    const double beta = delta_gradient_history_.col(i).dot(search_direction) /
        delta_x_dot_delta_gradient_(i);
    search_direction += delta_x_history_.col(i) * (alpha(i) - beta);
  }
}

}  // namespace internal
}  // namespace ceres
