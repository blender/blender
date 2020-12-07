// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
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
//         jodebo_beck@gmx.de (Johannes Beck)

#ifndef CERES_PUBLIC_INTERNAL_VARIADIC_EVALUATE_H_
#define CERES_PUBLIC_INTERNAL_VARIADIC_EVALUATE_H_

#include <stddef.h>

#include <type_traits>
#include <utility>

#include "ceres/cost_function.h"
#include "ceres/internal/parameter_dims.h"

namespace ceres {
namespace internal {

// For fixed size cost functors
template <typename Functor, typename T, int... Indices>
inline bool VariadicEvaluateImpl(const Functor& functor,
                                 T const* const* input,
                                 T* output,
                                 std::false_type /*is_dynamic*/,
                                 std::integer_sequence<int, Indices...>) {
  static_assert(sizeof...(Indices),
                "Invalid number of parameter blocks. At least one parameter "
                "block must be specified.");
  return functor(input[Indices]..., output);
}

// For dynamic sized cost functors
template <typename Functor, typename T>
inline bool VariadicEvaluateImpl(const Functor& functor,
                                 T const* const* input,
                                 T* output,
                                 std::true_type /*is_dynamic*/,
                                 std::integer_sequence<int>) {
  return functor(input, output);
}

// For ceres cost functors (not ceres::CostFunction)
template <typename ParameterDims, typename Functor, typename T>
inline bool VariadicEvaluateImpl(const Functor& functor,
                                 T const* const* input,
                                 T* output,
                                 const void* /* NOT USED */) {
  using ParameterBlockIndices =
      std::make_integer_sequence<int, ParameterDims::kNumParameterBlocks>;
  using IsDynamic = std::integral_constant<bool, ParameterDims::kIsDynamic>;
  return VariadicEvaluateImpl(
      functor, input, output, IsDynamic(), ParameterBlockIndices());
}

// For ceres::CostFunction
template <typename ParameterDims, typename Functor, typename T>
inline bool VariadicEvaluateImpl(const Functor& functor,
                                 T const* const* input,
                                 T* output,
                                 const CostFunction* /* NOT USED */) {
  return functor.Evaluate(input, output, nullptr);
}

// Variadic evaluate is a helper function to evaluate ceres cost function or
// functors using an input, output and the parameter dimensions. There are
// several ways different possibilities:
// 1) If the passed functor is a 'ceres::CostFunction' its evaluate method is
// called.
// 2) If the functor is not a 'ceres::CostFunction' and the specified parameter
// dims is dynamic, the functor must have the following signature
// 'bool(T const* const* input, T* output)'.
// 3) If the functor is not a 'ceres::CostFunction' and the specified parameter
// dims is not dynamic, the input is expanded by using the number of parameter
// blocks. The signature of the functor must have the following signature
// 'bool()(const T* i_1, const T* i_2, ... const T* i_n, T* output)'.
template <typename ParameterDims, typename Functor, typename T>
inline bool VariadicEvaluate(const Functor& functor,
                             T const* const* input,
                             T* output) {
  return VariadicEvaluateImpl<ParameterDims>(functor, input, output, &functor);
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_PUBLIC_INTERNAL_VARIADIC_EVALUATE_H_
