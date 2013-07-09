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
// This autodiff implementation differs from the one found in
// autodiff_cost_function.h by supporting autodiff on cost functions with
// variable numbers of parameters with variable sizes. With the other
// implementation, all the sizes (both the number of parameter blocks and the
// size of each block) must be fixed at compile time.
//
// The functor API differs slightly from the API for fixed size autodiff; the
// expected interface for the cost functors is:
//
//   struct MyCostFunctor {
//     template<typename T>
//     bool operator()(T const* const* parameters, T* residuals) const {
//       // Use parameters[i] to access the i'th parameter block.
//     }
//   }
//
// Since the sizing of the parameters is done at runtime, you must also specify
// the sizes after creating the dynamic autodiff cost function. For example:
//
//   DynamicAutoDiffCostFunction<MyCostFunctor, 3> cost_function(
//       new MyCostFunctor());
//   cost_function.AddParameterBlock(5);
//   cost_function.AddParameterBlock(10);
//   cost_function.SetNumResiduals(21);
//
// Under the hood, the implementation evaluates the cost function multiple
// times, computing a small set of the derivatives (four by default, controlled
// by the Stride template parameter) with each pass. There is a tradeoff with
// the size of the passes; you may want to experiment with the stride.

#ifndef CERES_PUBLIC_DYNAMIC_AUTODIFF_COST_FUNCTION_H_
#define CERES_PUBLIC_DYNAMIC_AUTODIFF_COST_FUNCTION_H_

#include <cmath>
#include <numeric>
#include <vector>

#include "ceres/cost_function.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/jet.h"
#include "glog/logging.h"

namespace ceres {

template <typename CostFunctor, int Stride = 4>
class DynamicAutoDiffCostFunction : public CostFunction {
 public:
  explicit DynamicAutoDiffCostFunction(CostFunctor* functor)
    : functor_(functor) {}

  virtual ~DynamicAutoDiffCostFunction() {}

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
    // jacobians in a series of passes, each one computing Stripe *
    // num_residuals() derivatives. This is done with small, fixed-size jets.
    const int num_parameter_blocks = parameter_block_sizes().size();
    const int num_parameters = std::accumulate(parameter_block_sizes().begin(),
                                               parameter_block_sizes().end(),
                                               0);

    // Allocate scratch space for the strided evaluation.
    vector<Jet<double, Stride> > input_jets(num_parameters);
    vector<Jet<double, Stride> > output_jets(num_residuals());

    // Make the parameter pack that is sent to the functor (reused).
    vector<Jet<double, Stride>* > jet_parameters(num_parameter_blocks,
        static_cast<Jet<double, Stride>* >(NULL));
    int num_active_parameters = 0;
    int start_derivative_section = -1;
    for (int i = 0, parameter_cursor = 0; i < num_parameter_blocks; ++i) {
      jet_parameters[i] = &input_jets[parameter_cursor];

      const int parameter_block_size = parameter_block_sizes()[i];
      if (jacobians[i] != NULL) {
        start_derivative_section =
            (start_derivative_section == -1)
            ? parameter_cursor
            : start_derivative_section;
        num_active_parameters += parameter_block_size;
      }

      for (int j = 0; j < parameter_block_size; ++j, parameter_cursor++) {
        input_jets[parameter_cursor].a = parameters[i][j];
      }
    }

    // Evaluate all of the strides. Each stride is a chunk of the derivative to
    // evaluate, typically some size proportional to the size of the SIMD
    // registers of the CPU.
    int num_strides = static_cast<int>(ceil(num_active_parameters /
                                            static_cast<float>(Stride)));

    for (int pass = 0; pass < num_strides; ++pass) {
      // Set most of the jet components to zero, except for
      // non-constant #Stride parameters.
      int active_parameter_count = 0;
      int end_derivative_section = start_derivative_section;
      for (int i = 0, parameter_cursor = 0; i < num_parameter_blocks; ++i) {
        for (int j = 0; j < parameter_block_sizes()[i];
             ++j, parameter_cursor++) {
          input_jets[parameter_cursor].v.setZero();
          if (parameter_cursor >= start_derivative_section &&
              active_parameter_count < Stride) {
            if (jacobians[i] != NULL) {
              input_jets[parameter_cursor]
                  .v[parameter_cursor - start_derivative_section] = 1.0;
              ++active_parameter_count;
            }
            ++end_derivative_section;
          }
        }
      }

      if (!(*functor_)(&jet_parameters[0], &output_jets[0])) {
        return false;
      }

      // Copy the pieces of the jacobians into their final place.
      active_parameter_count = 0;
      for (int i = 0, parameter_cursor = 0; i < num_parameter_blocks; ++i) {
        for (int j = 0; j < parameter_block_sizes()[i];
             ++j, parameter_cursor++) {
          if (parameter_cursor >= start_derivative_section &&
              active_parameter_count < Stride) {
            if (jacobians[i] != NULL) {
              for (int k = 0; k < num_residuals(); ++k) {
                jacobians[i][k * parameter_block_sizes()[i] + j] =
                    output_jets[k].v[parameter_cursor -
                                     start_derivative_section];
              }
              ++active_parameter_count;
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

      start_derivative_section = end_derivative_section;
    }
    return true;
  }

 private:
  internal::scoped_ptr<CostFunctor> functor_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_DYNAMIC_AUTODIFF_COST_FUNCTION_H_
