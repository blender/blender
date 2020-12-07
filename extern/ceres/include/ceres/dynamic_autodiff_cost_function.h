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
// Author: sameeragarwal@google.com (Sameer Agarwal)
//         mierle@gmail.com (Keir Mierle)

#ifndef CERES_PUBLIC_DYNAMIC_AUTODIFF_COST_FUNCTION_H_
#define CERES_PUBLIC_DYNAMIC_AUTODIFF_COST_FUNCTION_H_

#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

#include "ceres/dynamic_cost_function.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/jet.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {

// This autodiff implementation differs from the one found in
// autodiff_cost_function.h by supporting autodiff on cost functions
// with variable numbers of parameters with variable sizes. With the
// other implementation, all the sizes (both the number of parameter
// blocks and the size of each block) must be fixed at compile time.
//
// The functor API differs slightly from the API for fixed size
// autodiff; the expected interface for the cost functors is:
//
//   struct MyCostFunctor {
//     template<typename T>
//     bool operator()(T const* const* parameters, T* residuals) const {
//       // Use parameters[i] to access the i'th parameter block.
//     }
//   };
//
// Since the sizing of the parameters is done at runtime, you must
// also specify the sizes after creating the dynamic autodiff cost
// function. For example:
//
//   DynamicAutoDiffCostFunction<MyCostFunctor, 3> cost_function(
//       new MyCostFunctor());
//   cost_function.AddParameterBlock(5);
//   cost_function.AddParameterBlock(10);
//   cost_function.SetNumResiduals(21);
//
// Under the hood, the implementation evaluates the cost function
// multiple times, computing a small set of the derivatives (four by
// default, controlled by the Stride template parameter) with each
// pass. There is a tradeoff with the size of the passes; you may want
// to experiment with the stride.
template <typename CostFunctor, int Stride = 4>
class DynamicAutoDiffCostFunction : public DynamicCostFunction {
 public:
  // Takes ownership by default.
  DynamicAutoDiffCostFunction(CostFunctor* functor,
                              Ownership ownership = TAKE_OWNERSHIP)
      : functor_(functor), ownership_(ownership) {}

  explicit DynamicAutoDiffCostFunction(DynamicAutoDiffCostFunction&& other)
      : functor_(std::move(other.functor_)), ownership_(other.ownership_) {}

  virtual ~DynamicAutoDiffCostFunction() {
    // Manually release pointer if configured to not take ownership
    // rather than deleting only if ownership is taken.  This is to
    // stay maximally compatible to old user code which may have
    // forgotten to implement a virtual destructor, from when the
    // AutoDiffCostFunction always took ownership.
    if (ownership_ == DO_NOT_TAKE_OWNERSHIP) {
      functor_.release();
    }
  }

  bool Evaluate(double const* const* parameters,
                double* residuals,
                double** jacobians) const override {
    CHECK_GT(num_residuals(), 0)
        << "You must call DynamicAutoDiffCostFunction::SetNumResiduals() "
        << "before DynamicAutoDiffCostFunction::Evaluate().";

    if (jacobians == NULL) {
      return (*functor_)(parameters, residuals);
    }

    // The difficulty with Jets, as implemented in Ceres, is that they were
    // originally designed for strictly compile-sized use. At this point, there
    // is a large body of code that assumes inside a cost functor it is
    // acceptable to do e.g. T(1.5) and get an appropriately sized jet back.
    //
    // Unfortunately, it is impossible to communicate the expected size of a
    // dynamically sized jet to the static instantiations that existing code
    // depends on.
    //
    // To work around this issue, the solution here is to evaluate the
    // jacobians in a series of passes, each one computing Stride *
    // num_residuals() derivatives. This is done with small, fixed-size jets.
    const int num_parameter_blocks =
        static_cast<int>(parameter_block_sizes().size());
    const int num_parameters = std::accumulate(
        parameter_block_sizes().begin(), parameter_block_sizes().end(), 0);

    // Allocate scratch space for the strided evaluation.
    using JetT = Jet<double, Stride>;
    internal::FixedArray<JetT, (256 * 7) / sizeof(JetT)> input_jets(
        num_parameters);
    internal::FixedArray<JetT, (256 * 7) / sizeof(JetT)> output_jets(
        num_residuals());

    // Make the parameter pack that is sent to the functor (reused).
    internal::FixedArray<Jet<double, Stride>*> jet_parameters(
        num_parameter_blocks, nullptr);
    int num_active_parameters = 0;

    // To handle constant parameters between non-constant parameter blocks, the
    // start position --- a raw parameter index --- of each contiguous block of
    // non-constant parameters is recorded in start_derivative_section.
    std::vector<int> start_derivative_section;
    bool in_derivative_section = false;
    int parameter_cursor = 0;

    // Discover the derivative sections and set the parameter values.
    for (int i = 0; i < num_parameter_blocks; ++i) {
      jet_parameters[i] = &input_jets[parameter_cursor];

      const int parameter_block_size = parameter_block_sizes()[i];
      if (jacobians[i] != NULL) {
        if (!in_derivative_section) {
          start_derivative_section.push_back(parameter_cursor);
          in_derivative_section = true;
        }

        num_active_parameters += parameter_block_size;
      } else {
        in_derivative_section = false;
      }

      for (int j = 0; j < parameter_block_size; ++j, parameter_cursor++) {
        input_jets[parameter_cursor].a = parameters[i][j];
      }
    }

    if (num_active_parameters == 0) {
      return (*functor_)(parameters, residuals);
    }
    // When `num_active_parameters % Stride != 0` then it can be the case
    // that `active_parameter_count < Stride` while parameter_cursor is less
    // than the total number of parameters and with no remaining non-constant
    // parameter blocks. Pushing parameter_cursor (the total number of
    // parameters) as a final entry to start_derivative_section is required
    // because if a constant parameter block is encountered after the
    // last non-constant block then current_derivative_section is incremented
    // and would otherwise index an invalid position in
    // start_derivative_section. Setting the final element to the total number
    // of parameters means that this can only happen at most once in the loop
    // below.
    start_derivative_section.push_back(parameter_cursor);

    // Evaluate all of the strides. Each stride is a chunk of the derivative to
    // evaluate, typically some size proportional to the size of the SIMD
    // registers of the CPU.
    int num_strides = static_cast<int>(
        ceil(num_active_parameters / static_cast<float>(Stride)));

    int current_derivative_section = 0;
    int current_derivative_section_cursor = 0;

    for (int pass = 0; pass < num_strides; ++pass) {
      // Set most of the jet components to zero, except for
      // non-constant #Stride parameters.
      const int initial_derivative_section = current_derivative_section;
      const int initial_derivative_section_cursor =
          current_derivative_section_cursor;

      int active_parameter_count = 0;
      parameter_cursor = 0;

      for (int i = 0; i < num_parameter_blocks; ++i) {
        for (int j = 0; j < parameter_block_sizes()[i];
             ++j, parameter_cursor++) {
          input_jets[parameter_cursor].v.setZero();
          if (active_parameter_count < Stride &&
              parameter_cursor >=
                  (start_derivative_section[current_derivative_section] +
                   current_derivative_section_cursor)) {
            if (jacobians[i] != NULL) {
              input_jets[parameter_cursor].v[active_parameter_count] = 1.0;
              ++active_parameter_count;
              ++current_derivative_section_cursor;
            } else {
              ++current_derivative_section;
              current_derivative_section_cursor = 0;
            }
          }
        }
      }

      if (!(*functor_)(&jet_parameters[0], &output_jets[0])) {
        return false;
      }

      // Copy the pieces of the jacobians into their final place.
      active_parameter_count = 0;

      current_derivative_section = initial_derivative_section;
      current_derivative_section_cursor = initial_derivative_section_cursor;

      for (int i = 0, parameter_cursor = 0; i < num_parameter_blocks; ++i) {
        for (int j = 0; j < parameter_block_sizes()[i];
             ++j, parameter_cursor++) {
          if (active_parameter_count < Stride &&
              parameter_cursor >=
                  (start_derivative_section[current_derivative_section] +
                   current_derivative_section_cursor)) {
            if (jacobians[i] != NULL) {
              for (int k = 0; k < num_residuals(); ++k) {
                jacobians[i][k * parameter_block_sizes()[i] + j] =
                    output_jets[k].v[active_parameter_count];
              }
              ++active_parameter_count;
              ++current_derivative_section_cursor;
            } else {
              ++current_derivative_section;
              current_derivative_section_cursor = 0;
            }
          }
        }
      }

      // Only copy the residuals over once (even though we compute them on
      // every loop).
      if (pass == num_strides - 1) {
        for (int k = 0; k < num_residuals(); ++k) {
          residuals[k] = output_jets[k].a;
        }
      }
    }
    return true;
  }

 private:
  std::unique_ptr<CostFunctor> functor_;
  Ownership ownership_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_DYNAMIC_AUTODIFF_COST_FUNCTION_H_
