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
// Author: mierle@gmail.com (Keir Mierle)
//         sameeragarwal@google.com (Sameer Agarwal)
//         thadh@gmail.com (Thad Hughes)
//
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
//     template<typename T>
//     bool operator()(double const* const* parameters, double* residuals) const {
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

#ifndef CERES_PUBLIC_DYNAMIC_NUMERIC_DIFF_COST_FUNCTION_H_
#define CERES_PUBLIC_DYNAMIC_NUMERIC_DIFF_COST_FUNCTION_H_

#include <cmath>
#include <numeric>
#include <vector>

#include "ceres/cost_function.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/numeric_diff.h"
#include "glog/logging.h"

namespace ceres {

template <typename CostFunctor, NumericDiffMethod method = CENTRAL>
class DynamicNumericDiffCostFunction : public CostFunction {
 public:
  explicit DynamicNumericDiffCostFunction(const CostFunctor* functor,
                                          Ownership ownership = TAKE_OWNERSHIP,
                                          double relative_step_size = 1e-6)
      : functor_(functor),
        ownership_(ownership),
        relative_step_size_(relative_step_size) {
  }

  virtual ~DynamicNumericDiffCostFunction() {
    if (ownership_ != TAKE_OWNERSHIP) {
      functor_.release();
    }
  }

  void AddParameterBlock(int size) {
    mutable_parameter_block_sizes()->push_back(size);
  }

  void SetNumResiduals(int num_residuals) {
    set_num_residuals(num_residuals);
  }

  virtual bool Evaluate(double const* const* parameters,
                        double* residuals,
                        double** jacobians) const {
    CHECK_GT(num_residuals(), 0)
        << "You must call DynamicNumericDiffCostFunction::SetNumResiduals() "
        << "before DynamicNumericDiffCostFunction::Evaluate().";

    const vector<int32>& block_sizes = parameter_block_sizes();
    CHECK(!block_sizes.empty())
        << "You must call DynamicNumericDiffCostFunction::AddParameterBlock() "
        << "before DynamicNumericDiffCostFunction::Evaluate().";

    const bool status = EvaluateCostFunctor(parameters, residuals);
    if (jacobians == NULL || !status) {
      return status;
    }

    // Create local space for a copy of the parameters which will get mutated.
    int parameters_size = accumulate(block_sizes.begin(), block_sizes.end(), 0);
    vector<double> parameters_copy(parameters_size);
    vector<double*> parameters_references_copy(block_sizes.size());
    parameters_references_copy[0] = &parameters_copy[0];
    for (int block = 1; block < block_sizes.size(); ++block) {
      parameters_references_copy[block] = parameters_references_copy[block - 1]
          + block_sizes[block - 1];
    }

    // Copy the parameters into the local temp space.
    for (int block = 0; block < block_sizes.size(); ++block) {
      memcpy(parameters_references_copy[block],
             parameters[block],
             block_sizes[block] * sizeof(*parameters[block]));
    }

    for (int block = 0; block < block_sizes.size(); ++block) {
      if (jacobians[block] != NULL &&
          !EvaluateJacobianForParameterBlock(block_sizes[block],
                                             block,
                                             relative_step_size_,
                                             residuals,
                                             &parameters_references_copy[0],
                                             jacobians)) {
        return false;
      }
    }
    return true;
  }

 private:
  bool EvaluateJacobianForParameterBlock(const int parameter_block_size,
                                         const int parameter_block,
                                         const double relative_step_size,
                                         double const* residuals_at_eval_point,
                                         double** parameters,
                                         double** jacobians) const {
    using Eigen::Map;
    using Eigen::Matrix;
    using Eigen::Dynamic;
    using Eigen::RowMajor;

    typedef Matrix<double, Dynamic, 1> ResidualVector;
    typedef Matrix<double, Dynamic, 1> ParameterVector;
    typedef Matrix<double, Dynamic, Dynamic, RowMajor> JacobianMatrix;

    int num_residuals = this->num_residuals();

    Map<JacobianMatrix> parameter_jacobian(jacobians[parameter_block],
                                           num_residuals,
                                           parameter_block_size);

    // Mutate one element at a time and then restore.
    Map<ParameterVector> x_plus_delta(parameters[parameter_block],
                                      parameter_block_size);
    ParameterVector x(x_plus_delta);
    ParameterVector step_size = x.array().abs() * relative_step_size;

    // To handle cases where a paremeter is exactly zero, instead use
    // the mean step_size for the other dimensions.
    double fallback_step_size = step_size.sum() / step_size.rows();
    if (fallback_step_size == 0.0) {
      // If all the parameters are zero, there's no good answer. Use the given
      // relative step_size as absolute step_size and hope for the best.
      fallback_step_size = relative_step_size;
    }

    // For each parameter in the parameter block, use finite
    // differences to compute the derivative for that parameter.
    for (int j = 0; j < parameter_block_size; ++j) {
      if (step_size(j) == 0.0) {
        // The parameter is exactly zero, so compromise and use the
        // mean step_size from the other parameters. This can break in
        // many cases, but it's hard to pick a good number without
        // problem specific knowledge.
        step_size(j) = fallback_step_size;
      }
      x_plus_delta(j) = x(j) + step_size(j);

      ResidualVector residuals(num_residuals);
      if (!EvaluateCostFunctor(parameters, &residuals[0])) {
        // Something went wrong; bail.
        return false;
      }

      // Compute this column of the jacobian in 3 steps:
      // 1. Store residuals for the forward part.
      // 2. Subtract residuals for the backward (or 0) part.
      // 3. Divide out the run.
      parameter_jacobian.col(j).matrix() = residuals;

      double one_over_h = 1 / step_size(j);
      if (method == CENTRAL) {
        // Compute the function on the other side of x(j).
        x_plus_delta(j) = x(j) - step_size(j);

        if (!EvaluateCostFunctor(parameters, &residuals[0])) {
          // Something went wrong; bail.
          return false;
        }

        parameter_jacobian.col(j) -= residuals;
        one_over_h /= 2;
      } else {
        // Forward difference only; reuse existing residuals evaluation.
        parameter_jacobian.col(j) -=
            Map<const ResidualVector>(residuals_at_eval_point, num_residuals);
      }
      x_plus_delta(j) = x(j);  // Restore x_plus_delta.

      // Divide out the run to get slope.
      parameter_jacobian.col(j) *= one_over_h;
    }
    return true;
  }

  bool EvaluateCostFunctor(double const* const* parameters,
                           double* residuals) const {
    return EvaluateCostFunctorImpl(functor_.get(),
                                   parameters,
                                   residuals,
                                   functor_.get());
  }

  // Helper templates to allow evaluation of a functor or a
  // CostFunction.
  bool EvaluateCostFunctorImpl(const CostFunctor* functor,
                               double const* const* parameters,
                               double* residuals,
                               const void* /* NOT USED */) const {
    return (*functor)(parameters, residuals);
  }

  bool EvaluateCostFunctorImpl(const CostFunctor* functor,
                               double const* const* parameters,
                               double* residuals,
                               const CostFunction* /* NOT USED */) const {
    return functor->Evaluate(parameters, residuals, NULL);
  }

  internal::scoped_ptr<const CostFunctor> functor_;
  Ownership ownership_;
  const double relative_step_size_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_DYNAMIC_AUTODIFF_COST_FUNCTION_H_
