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
// Author: tbennun@gmail.com (Tal Ben-Nun)
//

#ifndef CERES_PUBLIC_NUMERIC_DIFF_OPTIONS_H_
#define CERES_PUBLIC_NUMERIC_DIFF_OPTIONS_H_

namespace ceres {

// Options pertaining to numeric differentiation (e.g., convergence criteria,
// step sizes).
struct CERES_EXPORT NumericDiffOptions {
  NumericDiffOptions() {
    relative_step_size = 1e-6;
    ridders_relative_initial_step_size = 1e-2;
    max_num_ridders_extrapolations = 10;
    ridders_epsilon = 1e-12;
    ridders_step_shrink_factor = 2.0;
  }

  // Numeric differentiation step size (multiplied by parameter block's
  // order of magnitude). If parameters are close to zero, the step size
  // is set to sqrt(machine_epsilon).
  double relative_step_size;

  // Initial step size for Ridders adaptive numeric differentiation (multiplied
  // by parameter block's order of magnitude).
  // If parameters are close to zero, Ridders' method sets the step size
  // directly to this value. This parameter is separate from
  // "relative_step_size" in order to set a different default value.
  //
  // Note: For Ridders' method to converge, the step size should be initialized
  // to a value that is large enough to produce a significant change in the
  // function. As the derivative is estimated, the step size decreases.
  double ridders_relative_initial_step_size;

  // Maximal number of adaptive extrapolations (sampling) in Ridders' method.
  int max_num_ridders_extrapolations;

  // Convergence criterion on extrapolation error for Ridders adaptive
  // differentiation. The available error estimation methods are defined in
  // NumericDiffErrorType and set in the "ridders_error_method" field.
  double ridders_epsilon;

  // The factor in which to shrink the step size with each extrapolation in
  // Ridders' method.
  double ridders_step_shrink_factor;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_NUMERIC_DIFF_OPTIONS_H_
