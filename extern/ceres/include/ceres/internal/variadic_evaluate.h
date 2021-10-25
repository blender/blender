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

#ifndef CERES_PUBLIC_INTERNAL_VARIADIC_EVALUATE_H_
#define CERES_PUBLIC_INTERNAL_VARIADIC_EVALUATE_H_

#include <stddef.h>

#include "ceres/jet.h"
#include "ceres/types.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/fixed_array.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

// This block of quasi-repeated code calls the user-supplied functor, which may
// take a variable number of arguments. This is accomplished by specializing the
// struct based on the size of the trailing parameters; parameters with 0 size
// are assumed missing.
template<typename Functor, typename T, int N0, int N1, int N2, int N3, int N4,
         int N5, int N6, int N7, int N8, int N9>
struct VariadicEvaluate {
  static bool Call(const Functor& functor, T const *const *input, T* output) {
    return functor(input[0],
                   input[1],
                   input[2],
                   input[3],
                   input[4],
                   input[5],
                   input[6],
                   input[7],
                   input[8],
                   input[9],
                   output);
  }
};

template<typename Functor, typename T, int N0, int N1, int N2, int N3, int N4,
         int N5, int N6, int N7, int N8>
struct VariadicEvaluate<Functor, T, N0, N1, N2, N3, N4, N5, N6, N7, N8, 0> {
  static bool Call(const Functor& functor, T const *const *input, T* output) {
    return functor(input[0],
                   input[1],
                   input[2],
                   input[3],
                   input[4],
                   input[5],
                   input[6],
                   input[7],
                   input[8],
                   output);
  }
};

template<typename Functor, typename T, int N0, int N1, int N2, int N3, int N4,
         int N5, int N6, int N7>
struct VariadicEvaluate<Functor, T, N0, N1, N2, N3, N4, N5, N6, N7, 0, 0> {
  static bool Call(const Functor& functor, T const *const *input, T* output) {
    return functor(input[0],
                   input[1],
                   input[2],
                   input[3],
                   input[4],
                   input[5],
                   input[6],
                   input[7],
                   output);
  }
};

template<typename Functor, typename T, int N0, int N1, int N2, int N3, int N4,
         int N5, int N6>
struct VariadicEvaluate<Functor, T, N0, N1, N2, N3, N4, N5, N6, 0, 0, 0> {
  static bool Call(const Functor& functor, T const *const *input, T* output) {
    return functor(input[0],
                   input[1],
                   input[2],
                   input[3],
                   input[4],
                   input[5],
                   input[6],
                   output);
  }
};

template<typename Functor, typename T, int N0, int N1, int N2, int N3, int N4,
         int N5>
struct VariadicEvaluate<Functor, T, N0, N1, N2, N3, N4, N5, 0, 0, 0, 0> {
  static bool Call(const Functor& functor, T const *const *input, T* output) {
    return functor(input[0],
                   input[1],
                   input[2],
                   input[3],
                   input[4],
                   input[5],
                   output);
  }
};

template<typename Functor, typename T, int N0, int N1, int N2, int N3, int N4>
struct VariadicEvaluate<Functor, T, N0, N1, N2, N3, N4, 0, 0, 0, 0, 0> {
  static bool Call(const Functor& functor, T const *const *input, T* output) {
    return functor(input[0],
                   input[1],
                   input[2],
                   input[3],
                   input[4],
                   output);
  }
};

template<typename Functor, typename T, int N0, int N1, int N2, int N3>
struct VariadicEvaluate<Functor, T, N0, N1, N2, N3, 0, 0, 0, 0, 0, 0> {
  static bool Call(const Functor& functor, T const *const *input, T* output) {
    return functor(input[0],
                   input[1],
                   input[2],
                   input[3],
                   output);
  }
};

template<typename Functor, typename T, int N0, int N1, int N2>
struct VariadicEvaluate<Functor, T, N0, N1, N2, 0, 0, 0, 0, 0, 0, 0> {
  static bool Call(const Functor& functor, T const *const *input, T* output) {
    return functor(input[0],
                   input[1],
                   input[2],
                   output);
  }
};

template<typename Functor, typename T, int N0, int N1>
struct VariadicEvaluate<Functor, T, N0, N1, 0, 0, 0, 0, 0, 0, 0, 0> {
  static bool Call(const Functor& functor, T const *const *input, T* output) {
    return functor(input[0],
                   input[1],
                   output);
  }
};

template<typename Functor, typename T, int N0>
struct VariadicEvaluate<Functor, T, N0, 0, 0, 0, 0, 0, 0, 0, 0, 0> {
  static bool Call(const Functor& functor, T const *const *input, T* output) {
    return functor(input[0],
                   output);
  }
};

// Template instantiation for dynamically-sized functors.
template<typename Functor, typename T>
struct VariadicEvaluate<Functor, T, ceres::DYNAMIC, ceres::DYNAMIC,
                        ceres::DYNAMIC, ceres::DYNAMIC, ceres::DYNAMIC,
                        ceres::DYNAMIC, ceres::DYNAMIC, ceres::DYNAMIC,
                        ceres::DYNAMIC, ceres::DYNAMIC> {
  static bool Call(const Functor& functor, T const *const *input, T* output) {
    return functor(input, output);
  }
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_PUBLIC_INTERNAL_VARIADIC_EVALUATE_H_
