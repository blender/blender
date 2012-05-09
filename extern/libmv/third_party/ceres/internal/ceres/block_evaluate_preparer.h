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
// A evaluate preparer which puts jacobian the evaluated jacobian blocks
// directly into their final resting place in an overall block sparse matrix.
// The evaluator takes care to avoid evaluating the jacobian for fixed
// parameters.

#ifndef CERES_INTERNAL_BLOCK_EVALUATE_PREPARER_H_
#define CERES_INTERNAL_BLOCK_EVALUATE_PREPARER_H_

namespace ceres {
namespace internal {

class ResidualBlock;
class SparseMatrix;

class BlockEvaluatePreparer {
 public:
  // Using Init() instead of a constructor allows for allocating this structure
  // with new[]. This is because C++ doesn't allow passing arguments to objects
  // constructed with new[] (as opposed to plain 'new').
  void Init(int** jacobian_layout);

  // EvaluatePreparer interface

  // Point the jacobian blocks directly into the block sparse matrix.
  void Prepare(const ResidualBlock* residual_block,
               int residual_block_index,
               SparseMatrix* jacobian,
               double** jacobians) const;

 private:
  int const* const* jacobian_layout_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_BLOCK_EVALUATE_PREPARER_H_
