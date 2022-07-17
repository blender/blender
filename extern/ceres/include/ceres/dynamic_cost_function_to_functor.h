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
//         dgossow@google.com (David Gossow)

#ifndef CERES_PUBLIC_DYNAMIC_COST_FUNCTION_TO_FUNCTOR_H_
#define CERES_PUBLIC_DYNAMIC_COST_FUNCTION_TO_FUNCTOR_H_

#include <memory>
#include <numeric>
#include <vector>

#include "ceres/dynamic_cost_function.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/internal/fixed_array.h"
#include "glog/logging.h"

namespace ceres {

// DynamicCostFunctionToFunctor allows users to use CostFunction
// objects in templated functors which are to be used for automatic
// differentiation. It works similar to CostFunctionToFunctor, with the
// difference that it allows you to wrap a cost function with dynamic numbers
// of parameters and residuals.
//
// For example, let us assume that
//
//  class IntrinsicProjection : public CostFunction {
//    public:
//      IntrinsicProjection(const double* observation);
//      bool Evaluate(double const* const* parameters,
//                    double* residuals,
//                    double** jacobians) const override;
//  };
//
// is a cost function that implements the projection of a point in its
// local coordinate system onto its image plane and subtracts it from
// the observed point projection. It can compute its residual and
// either via analytic or numerical differentiation can compute its
// jacobians. The intrinsics are passed in as parameters[0] and the point as
// parameters[1].
//
// Now we would like to compose the action of this CostFunction with
// the action of camera extrinsics, i.e., rotation and
// translation. Say we have a templated function
//
//   template<typename T>
//   void RotateAndTranslatePoint(double const* const* parameters,
//                                double* residuals);
//
// Then we can now do the following,
//
// struct CameraProjection {
//   CameraProjection(const double* observation)
//       : intrinsic_projection_.(new IntrinsicProjection(observation)) {
//   }
//   template <typename T>
//   bool operator()(T const* const* parameters,
//                   T* residual) const {
//     const T* rotation = parameters[0];
//     const T* translation = parameters[1];
//     const T* intrinsics = parameters[2];
//     const T* point = parameters[3];
//     T transformed_point[3];
//     RotateAndTranslatePoint(rotation, translation, point, transformed_point);
//
//     // Note that we call intrinsic_projection_, just like it was
//     // any other templated functor.
//     const T* projection_parameters[2];
//     projection_parameters[0] = intrinsics;
//     projection_parameters[1] = transformed_point;
//     return intrinsic_projection_(projection_parameters, residual);
//   }
//
//  private:
//   DynamicCostFunctionToFunctor intrinsic_projection_;
// };
class CERES_EXPORT DynamicCostFunctionToFunctor {
 public:
  // Takes ownership of cost_function.
  explicit DynamicCostFunctionToFunctor(CostFunction* cost_function)
      : cost_function_(cost_function) {
    CHECK(cost_function != nullptr);
  }

  bool operator()(double const* const* parameters, double* residuals) const {
    return cost_function_->Evaluate(parameters, residuals, nullptr);
  }

  template <typename JetT>
  bool operator()(JetT const* const* inputs, JetT* output) const {
    const std::vector<int32_t>& parameter_block_sizes =
        cost_function_->parameter_block_sizes();
    const int num_parameter_blocks =
        static_cast<int>(parameter_block_sizes.size());
    const int num_residuals = cost_function_->num_residuals();
    const int num_parameters = std::accumulate(
        parameter_block_sizes.begin(), parameter_block_sizes.end(), 0);

    internal::FixedArray<double> parameters(num_parameters);
    internal::FixedArray<double*> parameter_blocks(num_parameter_blocks);
    internal::FixedArray<double> jacobians(num_residuals * num_parameters);
    internal::FixedArray<double*> jacobian_blocks(num_parameter_blocks);
    internal::FixedArray<double> residuals(num_residuals);

    // Build a set of arrays to get the residuals and jacobians from
    // the CostFunction wrapped by this functor.
    double* parameter_ptr = parameters.data();
    double* jacobian_ptr = jacobians.data();
    for (int i = 0; i < num_parameter_blocks; ++i) {
      parameter_blocks[i] = parameter_ptr;
      jacobian_blocks[i] = jacobian_ptr;
      for (int j = 0; j < parameter_block_sizes[i]; ++j) {
        *parameter_ptr++ = inputs[i][j].a;
      }
      jacobian_ptr += num_residuals * parameter_block_sizes[i];
    }

    if (!cost_function_->Evaluate(parameter_blocks.data(),
                                  residuals.data(),
                                  jacobian_blocks.data())) {
      return false;
    }

    // Now that we have the incoming Jets, which are carrying the
    // partial derivatives of each of the inputs w.r.t to some other
    // underlying parameters. The derivative of the outputs of the
    // cost function w.r.t to the same underlying parameters can now
    // be computed by applying the chain rule.
    //
    //  d output[i]               d output[i]   d input[j]
    //  --------------  = sum_j   ----------- * ------------
    //  d parameter[k]            d input[j]    d parameter[k]
    //
    // d input[j]
    // --------------  = inputs[j], so
    // d parameter[k]
    //
    //  outputJet[i]  = sum_k jacobian[i][k] * inputJet[k]
    //
    // The following loop, iterates over the residuals, computing one
    // output jet at a time.
    for (int i = 0; i < num_residuals; ++i) {
      output[i].a = residuals[i];
      output[i].v.setZero();

      for (int j = 0; j < num_parameter_blocks; ++j) {
        const int32_t block_size = parameter_block_sizes[j];
        for (int k = 0; k < parameter_block_sizes[j]; ++k) {
          output[i].v +=
              jacobian_blocks[j][i * block_size + k] * inputs[j][k].v;
        }
      }
    }

    return true;
  }

 private:
  std::unique_ptr<CostFunction> cost_function_;
};

}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_PUBLIC_DYNAMIC_COST_FUNCTION_TO_FUNCTOR_H_
