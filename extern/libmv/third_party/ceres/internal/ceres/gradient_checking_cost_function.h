// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
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
// Author: keir@google.com (Keir Mierle)

#ifndef CERES_INTERNAL_GRADIENT_CHECKING_COST_FUNCTION_H_
#define CERES_INTERNAL_GRADIENT_CHECKING_COST_FUNCTION_H_

#include <string>

#include "ceres/cost_function.h"

namespace ceres {
namespace internal {

class ProblemImpl;

// Creates a CostFunction that checks the jacobians that cost_function computes
// with finite differences. Bad results are logged; required precision is
// controlled by relative_precision and the numeric differentiation step size is
// controlled with relative_step_size. See solver.h for a better explanation of
// relative_step_size. Caller owns result.
//
// The condition enforced is that
//
//    (J_actual(i, j) - J_numeric(i, j))
//   ------------------------------------  <  relative_precision
//   max(J_actual(i, j), J_numeric(i, j))
//
// where J_actual(i, j) is the jacobian as computed by the supplied cost
// function (by the user) and J_numeric is the jacobian as computed by finite
// differences.
//
// Note: This is quite inefficient and is intended only for debugging.
CostFunction* CreateGradientCheckingCostFunction(
    const CostFunction* cost_function,
    double relative_step_size,
    double relative_precision,
    const string& extra_info);

// Create a new ProblemImpl object from the input problem_impl, where
// each CostFunctions in problem_impl are wrapped inside a
// GradientCheckingCostFunctions. This gives us a ProblemImpl object
// which checks its derivatives against estimates from numeric
// differentiation everytime a ResidualBlock is evaluated.
//
// relative_step_size and relative_precision are parameters to control
// the numeric differentiation and the relative tolerance between the
// jacobian computed by the CostFunctions in problem_impl and
// jacobians obtained by numerically differentiating them. For more
// details see the documentation for
// CreateGradientCheckingCostFunction above.
ProblemImpl* CreateGradientCheckingProblemImpl(ProblemImpl* problem_impl,
                                               double relative_step_size,
                                               double relative_precision);

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_GRADIENT_CHECKING_COST_FUNCTION_H_
