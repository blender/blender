// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2019 Google Inc. All rights reserved.
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
// Copyright 2007 Google Inc. All Rights Reserved.
//
// Authors: wjr@google.com (William Rucklidge),
//          keir@google.com (Keir Mierle),
//          dgossow@google.com (David Gossow)

#ifndef CERES_PUBLIC_GRADIENT_CHECKER_H_
#define CERES_PUBLIC_GRADIENT_CHECKER_H_

#include <memory>
#include <string>
#include <vector>

#include "ceres/cost_function.h"
#include "ceres/dynamic_numeric_diff_cost_function.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/local_parameterization.h"
#include "ceres/manifold.h"
#include "glog/logging.h"

namespace ceres {

// GradientChecker compares the Jacobians returned by a cost function against
// derivatives estimated using finite differencing.
//
// The condition enforced is that
//
//    (J_actual(i, j) - J_numeric(i, j))
//   ------------------------------------  <  relative_precision
//   max(J_actual(i, j), J_numeric(i, j))
//
// where J_actual(i, j) is the jacobian as computed by the supplied cost
// function (by the user) multiplied by the local parameterization Jacobian
// and J_numeric is the jacobian as computed by finite differences, multiplied
// by the local parameterization Jacobian as well.
//
// How to use: Fill in an array of pointers to parameter blocks for your
// CostFunction, and then call Probe(). Check that the return value is 'true'.
class CERES_EXPORT GradientChecker {
 public:
  // This constructor will not take ownership of the cost function or local
  // parameterizations.
  //
  // function: The cost function to probe.
  //
  // local_parameterizations: A vector of local parameterizations, one for each
  // parameter block. May be nullptr or contain nullptrs to indicate that the
  // respective parameter does not have a local parameterization.
  //
  // options: Options to use for numerical differentiation.
  //
  // NOTE: This constructor is deprecated and will be removed in the next public
  // release of Ceres Solver. Please transition to using the Manifold based
  // version.
  CERES_DEPRECATED_WITH_MSG(
      "Local Parameterizations are deprecated. Use the constructor that uses "
      "Manifolds instead.")
  GradientChecker(
      const CostFunction* function,
      const std::vector<const LocalParameterization*>* local_parameterizations,
      const NumericDiffOptions& options);

  // This will not take ownership of the cost function or manifolds.
  //
  // function: The cost function to probe.
  //
  // manifolds: A vector of manifolds for each parameter. May be nullptr or
  // contain nullptrs to indicate that the respective parameter blocks are
  // Euclidean.
  //
  // options: Options to use for numerical differentiation.
  GradientChecker(const CostFunction* function,
                  const std::vector<const Manifold*>* manifolds,
                  const NumericDiffOptions& options);
  ~GradientChecker();

  // Contains results from a call to Probe for later inspection.
  struct CERES_EXPORT ProbeResults {
    // The return value of the cost function.
    bool return_value;

    // Computed residual vector.
    Vector residuals;

    // The sizes of the Jacobians below are dictated by the cost function's
    // parameter block size and residual block sizes. If a parameter block has a
    // manifold associated with it, the size of the "local" Jacobian will be
    // determined by the dimension of the manifold (which is the same as the
    // dimension of the tangent space) and residual block size, otherwise it
    // will be identical to the regular Jacobian.

    // Derivatives as computed by the cost function.
    std::vector<Matrix> jacobians;

    // Derivatives as computed by the cost function in local space.
    std::vector<Matrix> local_jacobians;

    // Derivatives as computed by numerical differentiation in local space.
    std::vector<Matrix> numeric_jacobians;

    // Derivatives as computed by numerical differentiation in local space.
    std::vector<Matrix> local_numeric_jacobians;

    // Contains the maximum relative error found in the local Jacobians.
    double maximum_relative_error;

    // If an error was detected, this will contain a detailed description of
    // that error.
    std::string error_log;
  };

  // Call the cost function, compute alternative Jacobians using finite
  // differencing and compare results. If manifolds are given, the Jacobians
  // will be multiplied by the manifold Jacobians before performing the check,
  // which effectively means that all errors along the null space of the
  // manifold will be ignored.  Returns false if the Jacobians don't match, the
  // cost function return false, or if a cost function returns a different
  // residual when called with a Jacobian output argument vs. calling it
  // without. Otherwise returns true.
  //
  // parameters: The parameter values at which to probe.
  // relative_precision: A threshold for the relative difference between the
  // Jacobians. If the Jacobians differ by more than this amount, then the
  // probe fails.
  // results: On return, the Jacobians (and other information) will be stored
  // here. May be nullptr.
  //
  // Returns true if no problems are detected and the difference between the
  // Jacobians is less than error_tolerance.
  bool Probe(double const* const* parameters,
             double relative_precision,
             ProbeResults* results) const;

 private:
  GradientChecker() = delete;
  GradientChecker(const GradientChecker&) = delete;
  void operator=(const GradientChecker&) = delete;

  // This bool is used to determine whether the constructor with the
  // LocalParameterizations is called or the one with Manifolds is called. If
  // the former, then the vector of manifolds is a vector of ManifoldAdapter
  // objects which we own and should be deleted. If the latter then they are
  // real Manifold objects owned by the caller and will not be deleted.
  //
  // This bool is only needed during the LocalParameterization to Manifold
  // transition, once this transition is complete the LocalParameterization
  // based constructor and this bool will be removed.
  const bool delete_manifolds_ = false;

  std::vector<const Manifold*> manifolds_;
  const CostFunction* function_;
  std::unique_ptr<CostFunction> finite_diff_cost_function_;
};

}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_PUBLIC_GRADIENT_CHECKER_H_
