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
// Author: keir@google.com (Keir Mierle)
//
// A evaluate preparer which puts jacobian the evaluated jacobian blocks
// directly into their final resting place in an overall block sparse matrix.
// The evaluator takes care to avoid evaluating the jacobian for fixed
// parameters.

#ifndef CERES_INTERNAL_BLOCK_EVALUATE_PREPARER_H_
#define CERES_INTERNAL_BLOCK_EVALUATE_PREPARER_H_

#include "ceres/internal/export.h"
#include "ceres/scratch_evaluate_preparer.h"

namespace ceres::internal {

class ResidualBlock;
class SparseMatrix;

class CERES_NO_EXPORT BlockEvaluatePreparer {
 public:
  // Using Init() instead of a constructor allows for allocating this structure
  // with new[]. This is because C++ doesn't allow passing arguments to objects
  // constructed with new[] (as opposed to plain 'new').
  void Init(int const* const* jacobian_layout,
            int max_derivatives_per_residual_block);

  // EvaluatePreparer interface

  // Point the jacobian blocks directly into the block sparse matrix, if
  // jacobian is non-null. Otherwise, uses an internal per-thread buffer to
  // store the jacobians temporarily.
  void Prepare(const ResidualBlock* residual_block,
               int residual_block_index,
               SparseMatrix* jacobian,
               double** jacobians);

 private:
  int const* const* jacobian_layout_;

  // For the case that the overall jacobian is not available, but the
  // individual jacobians are requested, use a pass-through scratch evaluate
  // preparer.
  ScratchEvaluatePreparer scratch_evaluate_preparer_;
};

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_BLOCK_EVALUATE_PREPARER_H_
