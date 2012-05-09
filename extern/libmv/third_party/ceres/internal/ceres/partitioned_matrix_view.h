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
// Author: sameeragarwal@google.com (Sameer Agarwal)
//
// For generalized bi-partite Jacobian matrices that arise in
// Structure from Motion related problems, it is sometimes useful to
// have access to the two parts of the matrix as linear operators
// themselves. This class provides that functionality.

#ifndef CERES_INTERNAL_PARTITIONED_MATRIX_VIEW_H_
#define CERES_INTERNAL_PARTITIONED_MATRIX_VIEW_H_

#include "ceres/block_sparse_matrix.h"

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
//
// This class lives in the internal name space as its a utility class
// to be used by the IterativeSchurComplementSolver class, found in
// iterative_schur_complement_solver.h, and is not meant for general
// consumption.
class PartitionedMatrixView {
 public:
  // matrix = [E F], where the matrix E contains the first
  // num_col_blocks_a column blocks.
  PartitionedMatrixView(const BlockSparseMatrixBase& matrix,
                        int num_col_blocks_a);
  ~PartitionedMatrixView();

  // y += E'x
  void LeftMultiplyE(const double* x, double* y) const;

  // y += F'x
  void LeftMultiplyF(const double* x, double* y) const;

  // y += Ex
  void RightMultiplyE(const double* x, double* y) const;

  // y += Fx
  void RightMultiplyF(const double* x, double* y) const;

  // Create and return the block diagonal of the matrix E'E.
  BlockSparseMatrix* CreateBlockDiagonalEtE() const;

  // Create and return the block diagonal of the matrix F'F.
  BlockSparseMatrix* CreateBlockDiagonalFtF() const;

  // Compute the block diagonal of the matrix E'E and store it in
  // block_diagonal. The matrix block_diagonal is expected to have a
  // BlockStructure (preferably created using
  // CreateBlockDiagonalMatrixEtE) which is has the same structure as
  // the block diagonal of E'E.
  void UpdateBlockDiagonalEtE(BlockSparseMatrix* block_diagonal) const;

  // Compute the block diagonal of the matrix F'F and store it in
  // block_diagonal. The matrix block_diagonal is expected to have a
  // BlockStructure (preferably created using
  // CreateBlockDiagonalMatrixFtF) which is has the same structure as
  // the block diagonal of F'F.
  void UpdateBlockDiagonalFtF(BlockSparseMatrix* block_diagonal) const;

  int num_col_blocks_e() const { return num_col_blocks_e_;  }
  int num_col_blocks_f() const { return num_col_blocks_f_;  }
  int num_cols_e()       const { return num_cols_e_;        }
  int num_cols_f()       const { return num_cols_f_;        }
  int num_rows()         const { return matrix_.num_rows(); }
  int num_cols()         const { return matrix_.num_cols(); }

 private:
  BlockSparseMatrix* CreateBlockDiagonalMatrixLayout(int start_col_block,
                                                     int end_col_block) const;

  const BlockSparseMatrixBase& matrix_;
  int num_row_blocks_e_;
  int num_col_blocks_e_;
  int num_col_blocks_f_;
  int num_cols_e_;
  int num_cols_f_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_PARTITIONED_MATRIX_VIEW_H_
