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
// Author: sameeragarwal@google.com (Sameer Agarwal)
//
// Utility routines for validating arrays.
//
// These are useful for detecting two common class of errors.
//
// 1. Uninitialized memory - where the user for some reason did not
// compute part of an array, but the code expects it.
//
// 2. Numerical failure while computing the cost/residual/jacobian,
// e.g. NaN, infinities etc. This is particularly useful since the
// automatic differentiation code does computations that are not
// evident to the user and can silently generate hard to debug errors.

#ifndef CERES_INTERNAL_ARRAY_UTILS_H_
#define CERES_INTERNAL_ARRAY_UTILS_H_

#include "ceres/internal/port.h"

namespace ceres {
namespace internal {

// Fill the array x with an impossible value that the user code is
// never expected to compute.
void InvalidateArray(int size, double* x);

// Check if all the entries of the array x are valid, i.e. all the
// values in the array should be finite and none of them should be
// equal to the "impossible" value used by InvalidateArray.
bool IsArrayValid(int size, const double* x);

extern const double kImpossibleValue;

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_ARRAY_UTILS_H_
