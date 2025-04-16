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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_INTERNAL_FUNCTION_SAMPLE_H_
#define CERES_INTERNAL_FUNCTION_SAMPLE_H_

#include <string>

#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"

namespace ceres::internal {

// FunctionSample is used by the line search routines to store and
// communicate the value and (optionally) the gradient of the function
// being minimized.
//
// Since line search as the name implies happens along a certain
// line/direction. FunctionSample contains the information in two
// ways. Information in the ambient space and information along the
// direction of search.
struct CERES_NO_EXPORT FunctionSample {
  FunctionSample();
  FunctionSample(double x, double value);
  FunctionSample(double x, double value, double gradient);

  std::string ToDebugString() const;

  // x is the location of the sample along the search direction.
  double x;

  // Let p be a point and d be the search direction then
  //
  // vector_x = p + x * d;
  Vector vector_x;
  // True if vector_x has been assigned a valid value.
  bool vector_x_is_valid;

  // value = f(vector_x)
  double value;
  // True of the evaluation was successful and value is a finite
  // number.
  bool value_is_valid;

  // vector_gradient = Df(vector_position);
  //
  // D is the derivative operator.
  Vector vector_gradient;
  // True if the vector gradient was evaluated and the evaluation was
  // successful (the value is a finite number).
  bool vector_gradient_is_valid;

  // gradient = d.transpose() * vector_gradient
  //
  // where d is the search direction.
  double gradient;
  // True if the evaluation of the gradient was successful and the
  // value is a finite number.
  bool gradient_is_valid;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_FUNCTION_SAMPLE_H_
