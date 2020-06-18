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
#include "ceres/internal/eigen.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/local_parameterization.h"
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
  // This will not take ownership of the cost function or local
  // parameterizations.
  //
  // function: The cost function to probe.
  // local_parameterizations: A vector of local parameterizations for each
  // parameter. May be NULL or contain NULL pointers to indicate that the
  // respective parameter does not have a local parameterization.
  // options: Options to use for numerical differentiation.
  GradientChecker(
      const CostFunction* function,
      const std::vector<const LocalParameterization*>* local_parameterizations,
      const NumericDiffOptions& options);

  // Contains results from a call to Probe for later inspection.
  struct CERES_EXPORT ProbeResults {
    // The return value of the cost function.
    bool return_value;

    // Computed residual vector.
    Vector residuals;

    // The sizes of the Jacobians below are dictated by the cost function's
    // parameter block size and residual block sizes. If a parameter block
    // has a local parameterization associated with it, the size of the "local"
    // Jacobian will be determined by the local parameterization dimension and
    // residual block size, otherwise it will be identical to the regular
    // Jacobian.

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
  // differencing and compare results. If local parameterizations are given,
  // the Jacobians will be multiplied by the local parameterization Jacobians
  // before performing the check, which effectively means that all errors along
  // the null space of the local parameterization will be ignored.
  // Returns false if the Jacobians don't match, the cost function return false,
  // or if the cost function returns different residual when called with a
  // Jacobian output argument vs. calling it without. Otherwise returns true.
  //
  // parameters: The parameter values at which to probe.
  // relative_precision: A threshold for the relative difference between the
  // Jacobians. If the Jacobians differ by more than this amount, then the
  // probe fails.
  // results: On return, the Jacobians (and other information) will be stored
  // here. May be NULL.
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

  std::vector<const LocalParameterization*> local_parameterizations_;
  const CostFunction* function_;
  std::unique_ptr<CostFunction> finite_diff_cost_function_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_GRADIENT_CHECKER_H_
