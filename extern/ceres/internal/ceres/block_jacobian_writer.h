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
// A jacobian writer that writes to block sparse matrices. The "writer" name is
// misleading, since the Write() operation on the block jacobian writer does not
// write anything. Instead, the Prepare() method on the BlockEvaluatePreparers
// makes a jacobians array which has direct pointers into the block sparse
// jacobian. When the cost function is evaluated, the jacobian blocks get placed
// directly in their final location.

#ifndef CERES_INTERNAL_BLOCK_JACOBIAN_WRITER_H_
#define CERES_INTERNAL_BLOCK_JACOBIAN_WRITER_H_

#include <memory>
#include <vector>

#include "ceres/evaluator.h"
#include "ceres/internal/export.h"

namespace ceres::internal {

class BlockEvaluatePreparer;
class Program;
class SparseMatrix;

// TODO(sameeragarwal): This class needs documentation.
class CERES_NO_EXPORT BlockJacobianWriter {
 public:
  // Pre-computes positions of cells in block-sparse jacobian.
  // Two possible memory layouts are implemented:
  //  - Non-partitioned case
  //  - Partitioned case (for Schur type linear solver)
  //
  // In non-partitioned case, cells are stored sequentially in the
  // lexicographic order of (row block id, column block id).
  //
  // In the case of partitoned matrix, cells of each sub-matrix (E and F) are
  // stored sequentially in the lexicographic order of (row block id, column
  // block id) and cells from E sub-matrix precede cells from F sub-matrix.
  BlockJacobianWriter(const Evaluator::Options& options, Program* program);

  // JacobianWriter interface.

  // Create evaluate prepareres that point directly into the final jacobian.
  // This makes the final Write() a nop.
  std::unique_ptr<BlockEvaluatePreparer[]> CreateEvaluatePreparers(
      unsigned num_threads);

  std::unique_ptr<SparseMatrix> CreateJacobian() const;

  void Write(int /* residual_id */,
             int /* residual_offset */,
             double** /* jacobians */,
             SparseMatrix* /* jacobian */) {
    // This is a noop since the blocks were written directly into their final
    // position by the outside evaluate call, thanks to the jacobians array
    // prepared by the BlockEvaluatePreparers.
  }

 private:
  Evaluator::Options options_;
  Program* program_;

  // Stores the position of each residual / parameter jacobian.
  //
  // The block sparse matrix that this writer writes to is stored as a set of
  // contiguous dense blocks, one after each other; see BlockSparseMatrix. The
  // "double* values_" member of the block sparse matrix contains all of these
  // blocks. Given a pointer to the first element of a block and the size of
  // that block, it's possible to write to it.
  //
  // In the case of a block sparse jacobian, the jacobian writer needs a way to
  // find the offset in the values_ array of each residual/parameter jacobian
  // block.
  //
  // That is the purpose of jacobian_layout_.
  //
  // In particular, jacobian_layout_[i][j] is the offset in the values_ array of
  // the derivative of residual block i with respect to the parameter block at
  // active argument position j.
  //
  // The active qualifier means that non-active parameters do not count. Care
  // must be taken when indexing into jacobian_layout_ to account for this.
  // Consider a single residual example:
  //
  //   r(x, y, z)
  //
  // with r in R^3, x in R^4, y in R^2, and z in R^5.
  // Take y as a constant (non-active) parameter.
  // Take r as residual number 0.
  //
  // In this case, the active arguments are only (x, z), so the active argument
  // position for x is 0, and the active argument position for z is 1. This is
  // similar to thinking of r as taking only 2 parameters:
  //
  //   r(x, z)
  //
  // There are only 2 jacobian blocks: dr/dx and dr/dz. jacobian_layout_ would
  // have the following contents:
  //
  //   jacobian_layout_[0] = { 0, 12 }
  //
  // which indicates that dr/dx is located at values_[0], and dr/dz is at
  // values_[12]. See BlockEvaluatePreparer::Prepare()'s comments about 'j'.
  std::vector<int*> jacobian_layout_;

  // The pointers in jacobian_layout_ point directly into this vector.
  std::vector<int> jacobian_layout_storage_;

  // The constructor computes the layout of the Jacobian, and this bool keeps
  // track of whether the computation of the layout completed successfully or
  // not, if it is false, then jacobian_layout and jacobian_layout_storage are
  // both in an invalid state.
  bool jacobian_layout_is_valid_ = false;
};

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_BLOCK_JACOBIAN_WRITER_H_
