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
//
// For generalized bi-partite Jacobian matrices that arise in
// Structure from Motion related problems, it is sometimes useful to
// have access to the two parts of the matrix as linear operators
// themselves. This class provides that functionality.

#ifndef CERES_INTERNAL_PARTITIONED_MATRIX_VIEW_H_
#define CERES_INTERNAL_PARTITIONED_MATRIX_VIEW_H_

#include <algorithm>
#include <cstring>
#include <vector>

#include "ceres/block_structure.h"
#include "ceres/internal/eigen.h"
#include "ceres/linear_solver.h"
#include "ceres/small_blas.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

// Given generalized bi-partite matrix A = [E F], with the same block
// structure as required by the Schur complement based solver, found
// in explicit_schur_complement_solver.h, provide access to the
// matrices E and F and their outer products E'E and F'F with
// themselves.
//
// Lack of BlockStructure object will result in a crash and if the
// block structure of the matrix does not satisfy the requirements of
// the Schur complement solver it will result in unpredictable and
// wrong output.
class PartitionedMatrixViewBase {
 public:
  virtual ~PartitionedMatrixViewBase() {}

  // y += E'x
  virtual void LeftMultiplyE(const double* x, double* y) const = 0;

  // y += F'x
  virtual void LeftMultiplyF(const double* x, double* y) const = 0;

  // y += Ex
  virtual void RightMultiplyE(const double* x, double* y) const = 0;

  // y += Fx
  virtual void RightMultiplyF(const double* x, double* y) const = 0;

  // Create and return the block diagonal of the matrix E'E.
  virtual BlockSparseMatrix* CreateBlockDiagonalEtE() const = 0;

  // Create and return the block diagonal of the matrix F'F. Caller
  // owns the result.
  virtual BlockSparseMatrix* CreateBlockDiagonalFtF() const = 0;

  // Compute the block diagonal of the matrix E'E and store it in
  // block_diagonal. The matrix block_diagonal is expected to have a
  // BlockStructure (preferably created using
  // CreateBlockDiagonalMatrixEtE) which is has the same structure as
  // the block diagonal of E'E.
  virtual void UpdateBlockDiagonalEtE(
      BlockSparseMatrix* block_diagonal) const = 0;

  // Compute the block diagonal of the matrix F'F and store it in
  // block_diagonal. The matrix block_diagonal is expected to have a
  // BlockStructure (preferably created using
  // CreateBlockDiagonalMatrixFtF) which is has the same structure as
  // the block diagonal of F'F.
  virtual void UpdateBlockDiagonalFtF(
      BlockSparseMatrix* block_diagonal) const = 0;

  virtual int num_col_blocks_e() const = 0;
  virtual int num_col_blocks_f() const = 0;
  virtual int num_cols_e()       const = 0;
  virtual int num_cols_f()       const = 0;
  virtual int num_rows()         const = 0;
  virtual int num_cols()         const = 0;

  static PartitionedMatrixViewBase* Create(const LinearSolver::Options& options,
                                           const BlockSparseMatrix& matrix);
};

template <int kRowBlockSize = Eigen::Dynamic,
          int kEBlockSize = Eigen::Dynamic,
          int kFBlockSize = Eigen::Dynamic >
class PartitionedMatrixView : public PartitionedMatrixViewBase {
 public:
  // matrix = [E F], where the matrix E contains the first
  // num_col_blocks_a column blocks.
  PartitionedMatrixView(const BlockSparseMatrix& matrix, int num_col_blocks_e);

  virtual ~PartitionedMatrixView();
  virtual void LeftMultiplyE(const double* x, double* y) const;
  virtual void LeftMultiplyF(const double* x, double* y) const;
  virtual void RightMultiplyE(const double* x, double* y) const;
  virtual void RightMultiplyF(const double* x, double* y) const;
  virtual BlockSparseMatrix* CreateBlockDiagonalEtE() const;
  virtual BlockSparseMatrix* CreateBlockDiagonalFtF() const;
  virtual void UpdateBlockDiagonalEtE(BlockSparseMatrix* block_diagonal) const;
  virtual void UpdateBlockDiagonalFtF(BlockSparseMatrix* block_diagonal) const;
  virtual int num_col_blocks_e() const { return num_col_blocks_e_;  }
  virtual int num_col_blocks_f() const { return num_col_blocks_f_;  }
  virtual int num_cols_e()       const { return num_cols_e_;        }
  virtual int num_cols_f()       const { return num_cols_f_;        }
  virtual int num_rows()         const { return matrix_.num_rows(); }
  virtual int num_cols()         const { return matrix_.num_cols(); }

 private:
  BlockSparseMatrix* CreateBlockDiagonalMatrixLayout(int start_col_block,
                                                     int end_col_block) const;

  const BlockSparseMatrix& matrix_;
  int num_row_blocks_e_;
  int num_col_blocks_e_;
  int num_col_blocks_f_;
  int num_cols_e_;
  int num_cols_f_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_PARTITIONED_MATRIX_VIEW_H_
