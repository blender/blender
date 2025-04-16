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
// Author: wjr@google.com (William Rucklidge)
//
// This file contains a cost function that can apply a transformation to
// each residual value before they are square-summed.

#ifndef CERES_PUBLIC_CONDITIONED_COST_FUNCTION_H_
#define CERES_PUBLIC_CONDITIONED_COST_FUNCTION_H_

#include <memory>
#include <vector>

#include "ceres/cost_function.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/types.h"

namespace ceres {

// This class allows you to apply different conditioning to the residual
// values of a wrapped cost function. An example where this is useful is
// where you have an existing cost function that produces N values, but you
// want the total cost to be something other than just the sum of these
// squared values - maybe you want to apply a different scaling to some
// values, to change their contribution to the cost.
//
// Usage:
//
//   // my_cost_function produces N residuals
//   CostFunction* my_cost_function = ...
//   CHECK_EQ(N, my_cost_function->num_residuals());
//   vector<CostFunction*> conditioners;
//
//   // Make N 1x1 cost functions (1 parameter, 1 residual)
//   CostFunction* f_1 = ...
//   conditioners.push_back(f_1);
//   ...
//   CostFunction* f_N = ...
//   conditioners.push_back(f_N);
//   ConditionedCostFunction* ccf =
//     new ConditionedCostFunction(my_cost_function, conditioners);
//
// Now ccf's residual i (i=0..N-1) will be passed though the i'th conditioner.
//
//   ccf_residual[i] = f_i(my_cost_function_residual[i])
//
// and the Jacobian will be affected appropriately.
class CERES_EXPORT ConditionedCostFunction final : public CostFunction {
 public:
  // Builds a cost function based on a wrapped cost function, and a
  // per-residual conditioner. Takes ownership of all of the wrapped cost
  // functions, or not, depending on the ownership parameter. Conditioners
  // may be nullptr, in which case the corresponding residual is not modified.
  //
  // The conditioners can repeat.
  ConditionedCostFunction(CostFunction* wrapped_cost_function,
                          const std::vector<CostFunction*>& conditioners,
                          Ownership ownership);
  ~ConditionedCostFunction() override;

  bool Evaluate(double const* const* parameters,
                double* residuals,
                double** jacobians) const override;

 private:
  std::unique_ptr<CostFunction> wrapped_cost_function_;
  std::vector<CostFunction*> conditioners_;
  Ownership ownership_;
};

}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_PUBLIC_CONDITIONED_COST_FUNCTION_H_
