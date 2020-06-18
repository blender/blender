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

#include "ceres/function_sample.h"
#include "ceres/stringprintf.h"

namespace ceres {
namespace internal {

FunctionSample::FunctionSample()
    : x(0.0),
      vector_x_is_valid(false),
      value(0.0),
      value_is_valid(false),
      vector_gradient_is_valid(false),
      gradient(0.0),
      gradient_is_valid(false) {}

FunctionSample::FunctionSample(const double x, const double value)
    : x(x),
      vector_x_is_valid(false),
      value(value),
      value_is_valid(true),
      vector_gradient_is_valid(false),
      gradient(0.0),
      gradient_is_valid(false) {}

FunctionSample::FunctionSample(const double x,
                               const double value,
                               const double gradient)
    : x(x),
      vector_x_is_valid(false),
      value(value),
      value_is_valid(true),
      vector_gradient_is_valid(false),
      gradient(gradient),
      gradient_is_valid(true) {}

std::string FunctionSample::ToDebugString() const {
  return StringPrintf("[x: %.8e, value: %.8e, gradient: %.8e, "
                      "value_is_valid: %d, gradient_is_valid: %d]",
                      x, value, gradient, value_is_valid, gradient_is_valid);
}

}  // namespace internal
}  // namespace ceres
