// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
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
// Author: keir@google.com (Keir Mierle)
//
// A convenience class for cost functions which are statically sized.
// Compared to the dynamically-sized base class, this reduces boilerplate.
//
// The kNumResiduals template parameter can be a constant such as 2 or 5, or it
// can be ceres::DYNAMIC. If kNumResiduals is ceres::DYNAMIC, then subclasses
// are responsible for calling set_num_residuals() at runtime.

#ifndef CERES_PUBLIC_SIZED_COST_FUNCTION_H_
#define CERES_PUBLIC_SIZED_COST_FUNCTION_H_

#include <glog/logging.h>
#include "ceres/types.h"
#include "ceres/cost_function.h"

namespace ceres {

template<int kNumResiduals,
         int N0 = 0, int N1 = 0, int N2 = 0, int N3 = 0, int N4 = 0, int N5 = 0>
class SizedCostFunction : public CostFunction {
 public:
  SizedCostFunction() {
    // Sanity checking.
    CHECK(kNumResiduals > 0 || kNumResiduals == DYNAMIC)
        << "Cost functions must have at least one residual block.";

    CHECK_GT(N0, 0)
        << "Cost functions must have at least one parameter block.";
    CHECK((!N1 && !N2 && !N3 && !N4 && !N5) ||
           ((N1 > 0) && !N2 && !N3 && !N4 && !N5) ||
           ((N1 > 0) && (N2 > 0) && !N3 && !N4 && !N5) ||
           ((N1 > 0) && (N2 > 0) && (N3 > 0) && !N4 && !N5) ||
           ((N1 > 0) && (N2 > 0) && (N3 > 0) && (N4 > 0) && !N5) ||
           ((N1 > 0) && (N2 > 0) && (N3 > 0) && (N4 > 0) && (N5 > 0)))
        << "Zero block cannot precede a non-zero block. Block sizes are "
        << "(ignore trailing 0s): " << N0 << ", " << N1 << ", " << N2 << ", "
        << N3 << ", " << N4 << ", " << N5;

    set_num_residuals(kNumResiduals);

#define ADD_PARAMETER_BLOCK(N) \
    if (N) mutable_parameter_block_sizes()->push_back(N);
    ADD_PARAMETER_BLOCK(N0);
    ADD_PARAMETER_BLOCK(N1);
    ADD_PARAMETER_BLOCK(N2);
    ADD_PARAMETER_BLOCK(N3);
    ADD_PARAMETER_BLOCK(N4);
    ADD_PARAMETER_BLOCK(N5);
#undef ADD_PARAMETER_BLOCK
  }

  virtual ~SizedCostFunction() { }

  // Subclasses must implement Evaluate().
};

}  // namespace ceres

#endif  // CERES_PUBLIC_SIZED_COST_FUNCTION_H_
