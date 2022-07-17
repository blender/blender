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
//
// Author: mierle@gmail.com (Keir Mierle)
//         sameeragarwal@google.com (Sameer Agarwal)
//         thadh@gmail.com (Thad Hughes)
//         tbennun@gmail.com (Tal Ben-Nun)

#ifndef CERES_PUBLIC_DYNAMIC_NUMERIC_DIFF_COST_FUNCTION_H_
#define CERES_PUBLIC_DYNAMIC_NUMERIC_DIFF_COST_FUNCTION_H_

#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

#include "ceres/dynamic_cost_function.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/numeric_diff.h"
#include "ceres/internal/parameter_dims.h"
#include "ceres/numeric_diff_options.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {

// This numeric diff implementation differs from the one found in
// numeric_diff_cost_function.h by supporting numericdiff on cost
// functions with variable numbers of parameters with variable
// sizes. With the other implementation, all the sizes (both the
// number of parameter blocks and the size of each block) must be
// fixed at compile time.
//
// The functor API differs slightly from the API for fixed size
// numeric diff; the expected interface for the cost functors is:
//
//   struct MyCostFunctor {
//     bool operator()(double const*
//                     const* parameters,
//                     double* residuals) const {
//       // Use parameters[i] to access the i'th parameter block.
//     }
//   }
//
// Since the sizing of the parameters is done at runtime, you must
// also specify the sizes after creating the
// DynamicNumericDiffCostFunction. For example:
//
//   DynamicAutoDiffCostFunction<MyCostFunctor, CENTRAL> cost_function(
//       new MyCostFunctor());
//   cost_function.AddParameterBlock(5);
//   cost_function.AddParameterBlock(10);
//   cost_function.SetNumResiduals(21);
template <typename CostFunctor, NumericDiffMethodType method = CENTRAL>
class DynamicNumericDiffCostFunction final : public DynamicCostFunction {
 public:
  explicit DynamicNumericDiffCostFunction(
      const CostFunctor* functor,
      Ownership ownership = TAKE_OWNERSHIP,
      const NumericDiffOptions& options = NumericDiffOptions())
      : functor_(functor), ownership_(ownership), options_(options) {}

  DynamicNumericDiffCostFunction(DynamicNumericDiffCostFunction&& other)
      : functor_(std::move(other.functor_)), ownership_(other.ownership_) {}

  ~DynamicNumericDiffCostFunction() override {
    if (ownership_ != TAKE_OWNERSHIP) {
      functor_.release();
    }
  }

  bool Evaluate(double const* const* parameters,
                double* residuals,
                double** jacobians) const override {
    using internal::NumericDiff;
    CHECK_GT(num_residuals(), 0)
        << "You must call DynamicNumericDiffCostFunction::SetNumResiduals() "
        << "before DynamicNumericDiffCostFunction::Evaluate().";

    const std::vector<int32_t>& block_sizes = parameter_block_sizes();
    CHECK(!block_sizes.empty())
        << "You must call DynamicNumericDiffCostFunction::AddParameterBlock() "
        << "before DynamicNumericDiffCostFunction::Evaluate().";

    const bool status =
        internal::VariadicEvaluate<internal::DynamicParameterDims>(
            *functor_.get(), parameters, residuals);
    if (jacobians == nullptr || !status) {
      return status;
    }

    // Create local space for a copy of the parameters which will get mutated.
    int parameters_size = accumulate(block_sizes.begin(), block_sizes.end(), 0);
    std::vector<double> parameters_copy(parameters_size);
    std::vector<double*> parameters_references_copy(block_sizes.size());
    parameters_references_copy[0] = &parameters_copy[0];
    for (size_t block = 1; block < block_sizes.size(); ++block) {
      parameters_references_copy[block] =
          parameters_references_copy[block - 1] + block_sizes[block - 1];
    }

    // Copy the parameters into the local temp space.
    for (size_t block = 0; block < block_sizes.size(); ++block) {
      memcpy(parameters_references_copy[block],
             parameters[block],
             block_sizes[block] * sizeof(*parameters[block]));
    }

    for (size_t block = 0; block < block_sizes.size(); ++block) {
      if (jacobians[block] != nullptr &&
          !NumericDiff<CostFunctor,
                       method,
                       ceres::DYNAMIC,
                       internal::DynamicParameterDims,
                       ceres::DYNAMIC,
                       ceres::DYNAMIC>::
              EvaluateJacobianForParameterBlock(functor_.get(),
                                                residuals,
                                                options_,
                                                this->num_residuals(),
                                                block,
                                                block_sizes[block],
                                                &parameters_references_copy[0],
                                                jacobians[block])) {
        return false;
      }
    }
    return true;
  }

 private:
  std::unique_ptr<const CostFunctor> functor_;
  Ownership ownership_;
  NumericDiffOptions options_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_DYNAMIC_AUTODIFF_COST_FUNCTION_H_
