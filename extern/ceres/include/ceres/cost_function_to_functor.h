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
//
// CostFunctionToFunctor is an adapter class that allows users to use
// SizedCostFunction objects in templated functors which are to be used for
// automatic differentiation. This allows the user to seamlessly mix
// analytic, numeric and automatic differentiation.
//
// For example, let us assume that
//
//  class IntrinsicProjection : public SizedCostFunction<2, 5, 3> {
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
// jacobians either via analytic or numerical differentiation.
//
// Now we would like to compose the action of this CostFunction with
// the action of camera extrinsics, i.e., rotation and
// translation. Say we have a templated function
//
//   template<typename T>
//   void RotateAndTranslatePoint(const T* rotation,
//                                const T* translation,
//                                const T* point,
//                                T* result);
//
// Then we can now do the following,
//
// struct CameraProjection {
//   CameraProjection(const double* observation)
//       : intrinsic_projection_(new IntrinsicProjection(observation)) {
//   }
//   template <typename T>
//   bool operator()(const T* rotation,
//                   const T* translation,
//                   const T* intrinsics,
//                   const T* point,
//                   T* residual) const {
//     T transformed_point[3];
//     RotateAndTranslatePoint(rotation, translation, point, transformed_point);
//
//     // Note that we call intrinsic_projection_, just like it was
//     // any other templated functor.
//
//     return intrinsic_projection_(intrinsics, transformed_point, residual);
//   }
//
//  private:
//   CostFunctionToFunctor<2,5,3> intrinsic_projection_;
// };

#ifndef CERES_PUBLIC_COST_FUNCTION_TO_FUNCTOR_H_
#define CERES_PUBLIC_COST_FUNCTION_TO_FUNCTOR_H_

#include <cstdint>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>

#include "ceres/cost_function.h"
#include "ceres/dynamic_cost_function_to_functor.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/internal/parameter_dims.h"
#include "ceres/internal/port.h"
#include "ceres/types.h"

namespace ceres {

template <int kNumResiduals, int... Ns>
class CostFunctionToFunctor {
 public:
  // Takes ownership of cost_function.
  explicit CostFunctionToFunctor(CostFunction* cost_function)
      : cost_functor_(cost_function) {
    CHECK(cost_function != nullptr);
    CHECK(kNumResiduals > 0 || kNumResiduals == DYNAMIC);

    const std::vector<int32_t>& parameter_block_sizes =
        cost_function->parameter_block_sizes();
    const int num_parameter_blocks = ParameterDims::kNumParameterBlocks;
    CHECK_EQ(static_cast<int>(parameter_block_sizes.size()),
             num_parameter_blocks);

    if (parameter_block_sizes.size() == num_parameter_blocks) {
      for (int block = 0; block < num_parameter_blocks; ++block) {
        CHECK_EQ(ParameterDims::GetDim(block), parameter_block_sizes[block])
            << "Parameter block size missmatch. The specified static parameter "
               "block dimension does not match the one from the cost function.";
      }
    }

    CHECK_EQ(accumulate(
                 parameter_block_sizes.begin(), parameter_block_sizes.end(), 0),
             ParameterDims::kNumParameters);
  }

  template <typename T, typename... Ts>
  bool operator()(const T* p1, Ts*... ps) const {
    // Add one because of residual block.
    static_assert(sizeof...(Ts) + 1 == ParameterDims::kNumParameterBlocks + 1,
                  "Invalid number of parameter blocks specified.");

    auto params = std::make_tuple(p1, ps...);

    // Extract residual pointer from params. The residual pointer is the
    // last pointer.
    constexpr int kResidualIndex = ParameterDims::kNumParameterBlocks;
    T* residuals = std::get<kResidualIndex>(params);

    // Extract parameter block pointers from params.
    using Indices =
        std::make_integer_sequence<int,
                                   ParameterDims::kNumParameterBlocks>;
    std::array<const T*, ParameterDims::kNumParameterBlocks> parameter_blocks =
        GetParameterPointers<T>(params, Indices());

    return cost_functor_(parameter_blocks.data(), residuals);
  }

 private:
  using ParameterDims = internal::StaticParameterDims<Ns...>;

  template <typename T, typename Tuple, int... Indices>
  static std::array<const T*, ParameterDims::kNumParameterBlocks>
  GetParameterPointers(const Tuple& paramPointers,
                       std::integer_sequence<int, Indices...>) {
    return std::array<const T*, ParameterDims::kNumParameterBlocks>{
        {std::get<Indices>(paramPointers)...}};
  }

  DynamicCostFunctionToFunctor cost_functor_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_COST_FUNCTION_TO_FUNCTOR_H_
