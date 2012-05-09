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
//
// Create CostFunctions as needed by the least squares framework with jacobians
// computed via numeric differentiation.
//
// To get a numerically differentiated cost function, define a subclass of
// CostFunction such that the Evaluate() function ignores the jacobian
// parameter. The numeric differentiation wrapper will fill in the jacobian
// parameter if nececssary by repeatedly calling the Evaluate() function with
// small changes to the appropriate parameters, and computing the slope. This
// implementation is not templated (hence the "Runtime" prefix), which is a bit
// slower than but is more convenient than the templated version in
// numeric_diff_cost_function.h
//
// The numerically differentiated version of a cost function for a cost function
// can be constructed as follows:
//
//   CostFunction* cost_function =
//     CreateRuntimeNumericDiffCostFunction(new MyCostFunction(...),
//                                          CENTRAL,
//                                          TAKE_OWNERSHIP);
//
// The central difference method is considerably more accurate; consider using
// to start and only after that works, trying forward difference.
//
// TODO(keir): Characterize accuracy; mention pitfalls; provide alternatives.

#ifndef CERES_INTERNAL_RUNTIME_NUMERIC_DIFF_COST_FUNCTION_H_
#define CERES_INTERNAL_RUNTIME_NUMERIC_DIFF_COST_FUNCTION_H_

#include "ceres/cost_function.h"

namespace ceres {
namespace internal {

enum RuntimeNumericDiffMethod {
  CENTRAL,
  FORWARD,
};

// Create a cost function that evaluates the derivative with finite differences.
// The base cost_function's implementation of Evaluate() only needs to fill in
// the "residuals" argument and not the "jacobians". Any data written to the
// jacobians by the base cost_function is overwritten.
//
// Forward difference or central difference is selected with CENTRAL or FORWARD.
// The relative eps, which determines the step size for forward and central
// differencing, is set with relative eps. Caller owns the resulting cost
// function, and the resulting cost function does not own the base cost
// function.
CostFunction *CreateRuntimeNumericDiffCostFunction(
    const CostFunction *cost_function,
    RuntimeNumericDiffMethod method,
    double relative_eps);

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_RUNTIME_NUMERIC_DIFF_COST_FUNCTION_H_
