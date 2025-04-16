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
// Author: mierle@gmail.com (Keir Mierle)

#ifndef CERES_PUBLIC_EVALUATION_CALLBACK_H_
#define CERES_PUBLIC_EVALUATION_CALLBACK_H_

#include "ceres/internal/export.h"

namespace ceres {

// Using this callback interface, Ceres can notify you when it is
// about to evaluate the residuals or jacobians. With the callback,
// you can share computation between residual blocks by doing the
// shared computation in PrepareForEvaluation() before Ceres calls
// CostFunction::Evaluate(). It also enables caching results between a
// pure residual evaluation and a residual & jacobian evaluation, via
// the new_evaluation_point argument.
//
// One use case for this callback is if the cost function compute is
// moved to the GPU. In that case, the prepare call does the actual
// cost function evaluation, and subsequent calls from Ceres to the
// actual cost functions merely copy the results from the GPU onto the
// corresponding blocks for Ceres to plug into the solver.
//
// NOTE: Ceres provides no mechanism to share data other than the
// notification from the callback. Users must provide access to
// pre-computed shared data to their cost functions behind the scenes;
// this all happens without Ceres knowing.
//
// One approach is to put a pointer to the shared data in each cost
// function (recommended) or to use a global shared variable
// (discouraged; bug-prone).  As far as Ceres is concerned, it is
// evaluating cost functions like any other; it just so happens that
// behind the scenes the cost functions reuse pre-computed data to
// execute faster.
class CERES_EXPORT EvaluationCallback {
 public:
  virtual ~EvaluationCallback();

  // Called before Ceres requests residuals or jacobians for a given setting of
  // the parameters. User parameters (the double* values provided to the cost
  // functions) are fixed until the next call to PrepareForEvaluation().
  //
  // If evaluate_jacobians == true, then the user provided CostFunctions will be
  // asked to evaluate one or more of their Jacobians.
  //
  // If new_evaluation_point == true, then this is a new point that is different
  // from the last evaluated point. Otherwise, it is the same point that was
  // evaluated previously (either jacobian or residual) and the user can use
  // cached results from previous evaluations.
  virtual void PrepareForEvaluation(bool evaluate_jacobians,
                                    bool new_evaluation_point) = 0;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_EVALUATION_CALLBACK_H_
