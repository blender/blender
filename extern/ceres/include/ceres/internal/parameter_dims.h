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
// Author: jodebo_beck@gmx.de (Johannes Beck)

#ifndef CERES_PUBLIC_INTERNAL_PARAMETER_DIMS_H_
#define CERES_PUBLIC_INTERNAL_PARAMETER_DIMS_H_

#include <array>
#include <utility>

#include "ceres/internal/integer_sequence_algorithm.h"

namespace ceres::internal {

// Helper class that represents the parameter dimensions. The parameter
// dimensions are either dynamic or the sizes are known at compile time. It is
// used to pass parameter block dimensions around (e.g. between functions or
// classes).
//
// As an example if one have three parameter blocks with dimensions (2, 4, 1),
// one would use 'StaticParameterDims<2, 4, 1>' which is a synonym for
// 'ParameterDims<false, 2, 4, 1>'.
// For dynamic parameter dims, one would just use 'DynamicParameterDims', which
// is a synonym for 'ParameterDims<true>'.
template <bool IsDynamic, int... Ns>
class ParameterDims {
 public:
  using Parameters = std::integer_sequence<int, Ns...>;

  // The parameter dimensions are only valid if all parameter block dimensions
  // are greater than zero.
  static constexpr bool kIsValid = ((Ns > 0) && ...);
  static_assert(kIsValid,
                "Invalid parameter block dimension detected. Each parameter "
                "block dimension must be bigger than zero.");

  static constexpr bool kIsDynamic = IsDynamic;
  static constexpr int kNumParameterBlocks = sizeof...(Ns);
  static_assert(kIsDynamic || kNumParameterBlocks > 0,
                "At least one parameter block must be specified.");

  static constexpr int kNumParameters = (Ns + ... + 0);

  static constexpr int GetDim(int dim) { return params_[dim]; }

  // If one has all parameters packed into a single array this function unpacks
  // the parameters.
  template <typename T>
  static inline std::array<T*, kNumParameterBlocks> GetUnpackedParameters(
      T* ptr) {
    using Offsets = ExclusiveScan<Parameters>;
    return GetUnpackedParameters(ptr, Offsets());
  }

 private:
  template <typename T, int... Indices>
  static inline std::array<T*, kNumParameterBlocks> GetUnpackedParameters(
      T* ptr, std::integer_sequence<int, Indices...>) {
    return std::array<T*, kNumParameterBlocks>{{ptr + Indices...}};
  }

  static constexpr std::array<int, kNumParameterBlocks> params_{Ns...};
};

// Even static constexpr member variables needs to be defined (not only
// declared). As the ParameterDims class is tempalted this definition must
// be in the header file.
template <bool IsDynamic, int... Ns>
constexpr std::array<int, ParameterDims<IsDynamic, Ns...>::kNumParameterBlocks>
    ParameterDims<IsDynamic, Ns...>::params_;

// Using declarations for static and dynamic parameter dims. This makes client
// code easier to read.
template <int... Ns>
using StaticParameterDims = ParameterDims<false, Ns...>;
using DynamicParameterDims = ParameterDims<true>;

}  // namespace ceres::internal

#endif  // CERES_PUBLIC_INTERNAL_PARAMETER_DIMS_H_
