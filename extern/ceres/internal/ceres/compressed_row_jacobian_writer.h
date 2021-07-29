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
// Author: keir@google.com (Keir Mierle)
//
// A jacobian writer that directly writes to compressed row sparse matrices.

#ifndef CERES_INTERNAL_COMPRESSED_ROW_JACOBIAN_WRITER_H_
#define CERES_INTERNAL_COMPRESSED_ROW_JACOBIAN_WRITER_H_

#include <utility>
#include <vector>

#include "ceres/evaluator.h"
#include "ceres/scratch_evaluate_preparer.h"

namespace ceres {
namespace internal {

class CompressedRowSparseMatrix;
class Program;
class SparseMatrix;

class CompressedRowJacobianWriter {
 public:
  CompressedRowJacobianWriter(Evaluator::Options /* ignored */,
                              Program* program)
    : program_(program) {
  }

  // PopulateJacobianRowAndColumnBlockVectors sets col_blocks and
  // row_blocks for a CompressedRowSparseMatrix, based on the
  // parameter block sizes and residual sizes respectively from the
  // program. This is useful when Solver::Options::use_block_amd =
  // true;
  //
  // This function is static so that it is available to other jacobian
  // writers which use CompressedRowSparseMatrix (or derived types).
  // (Jacobian writers do not fall under any type hierarchy; they only
  // have to provide an interface as specified in program_evaluator.h).
  static void PopulateJacobianRowAndColumnBlockVectors(
      const Program* program,
      CompressedRowSparseMatrix* jacobian);

  // It is necessary to determine the order of the jacobian blocks
  // before copying them into a CompressedRowSparseMatrix (or derived
  // type).  Just because a cost function uses parameter blocks 1
  // after 2 in its arguments does not mean that the block 1 occurs
  // before block 2 in the column layout of the jacobian. Thus,
  // GetOrderedParameterBlocks determines the order by sorting the
  // jacobian blocks by their position in the state vector.
  //
  // This function is static so that it is available to other jacobian
  // writers which use CompressedRowSparseMatrix (or derived types).
  // (Jacobian writers do not fall under any type hierarchy; they only
  // have to provide an interface as specified in
  // program_evaluator.h).
  static void GetOrderedParameterBlocks(
      const Program* program,
      int residual_id,
      std::vector<std::pair<int, int> >* evaluated_jacobian_blocks);

  // JacobianWriter interface.

  // Since the compressed row matrix has different layout than that
  // assumed by the cost functions, use scratch space to store the
  // jacobians temporarily then copy them over to the larger jacobian
  // in the Write() function.
  ScratchEvaluatePreparer* CreateEvaluatePreparers(int num_threads) {
    return ScratchEvaluatePreparer::Create(*program_, num_threads);
  }

  SparseMatrix* CreateJacobian() const;

  void Write(int residual_id,
             int residual_offset,
             double **jacobians,
             SparseMatrix* base_jacobian);

 private:
  Program* program_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_COMPRESSED_ROW_JACOBIAN_WRITER_H_
