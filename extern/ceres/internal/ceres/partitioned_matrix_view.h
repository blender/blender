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
//
// For generalized bi-partite Jacobian matrices that arise in
// Structure from Motion related problems, it is sometimes useful to
// have access to the two parts of the matrix as linear operators
// themselves. This class provides that functionality.

#ifndef CERES_INTERNAL_PARTITIONED_MATRIX_VIEW_H_
#define CERES_INTERNAL_PARTITIONED_MATRIX_VIEW_H_

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#include "ceres/block_structure.h"
#include "ceres/internal/config.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/linear_solver.h"
#include "ceres/small_blas.h"
#include "glog/logging.h"

namespace ceres::internal {

class ContextImpl;

// Given generalized bi-partite matrix A = [E F], with the same block
// structure as required by the Schur complement based solver, found
// in schur_complement_solver.h, provide access to the
// matrices E and F and their outer products E'E and F'F with
// themselves.
//
// Lack of BlockStructure object will result in a crash and if the
// block structure of the matrix does not satisfy the requirements of
// the Schur complement solver it will result in unpredictable and
// wrong output.
class CERES_NO_EXPORT PartitionedMatrixViewBase {
 public:
  virtual ~PartitionedMatrixViewBase();

  // y += E'x
  virtual void LeftMultiplyAndAccumulateE(const double* x, double* y) const = 0;
  virtual void LeftMultiplyAndAccumulateESingleThreaded(const double* x,
                                                        double* y) const = 0;
  virtual void LeftMultiplyAndAccumulateEMultiThreaded(const double* x,
                                                       double* y) const = 0;

  // y += F'x
  virtual void LeftMultiplyAndAccumulateF(const double* x, double* y) const = 0;
  virtual void LeftMultiplyAndAccumulateFSingleThreaded(const double* x,
                                                        double* y) const = 0;
  virtual void LeftMultiplyAndAccumulateFMultiThreaded(const double* x,
                                                       double* y) const = 0;

  // y += Ex
  virtual void RightMultiplyAndAccumulateE(const double* x,
                                           double* y) const = 0;

  // y += Fx
  virtual void RightMultiplyAndAccumulateF(const double* x,
                                           double* y) const = 0;

  // Create and return the block diagonal of the matrix E'E.
  virtual std::unique_ptr<BlockSparseMatrix> CreateBlockDiagonalEtE() const = 0;

  // Create and return the block diagonal of the matrix F'F. Caller
  // owns the result.
  virtual std::unique_ptr<BlockSparseMatrix> CreateBlockDiagonalFtF() const = 0;

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

  // clang-format off
  virtual int num_col_blocks_e() const = 0;
  virtual int num_col_blocks_f() const = 0;
  virtual int num_cols_e()       const = 0;
  virtual int num_cols_f()       const = 0;
  virtual int num_rows()         const = 0;
  virtual int num_cols()         const = 0;
  virtual const std::vector<int>& e_cols_partition() const = 0;
  virtual const std::vector<int>& f_cols_partition() const = 0;
  // clang-format on

  static std::unique_ptr<PartitionedMatrixViewBase> Create(
      const LinearSolver::Options& options, const BlockSparseMatrix& matrix);
};

template <int kRowBlockSize = Eigen::Dynamic,
          int kEBlockSize = Eigen::Dynamic,
          int kFBlockSize = Eigen::Dynamic>
class CERES_NO_EXPORT PartitionedMatrixView final
    : public PartitionedMatrixViewBase {
 public:
  // matrix = [E F], where the matrix E contains the first
  // options.elimination_groups[0] column blocks.
  PartitionedMatrixView(const LinearSolver::Options& options,
                        const BlockSparseMatrix& matrix);

  // y += E'x
  virtual void LeftMultiplyAndAccumulateE(const double* x,
                                          double* y) const final;
  virtual void LeftMultiplyAndAccumulateESingleThreaded(const double* x,
                                                        double* y) const final;
  virtual void LeftMultiplyAndAccumulateEMultiThreaded(const double* x,
                                                       double* y) const final;

  // y += F'x
  virtual void LeftMultiplyAndAccumulateF(const double* x,
                                          double* y) const final;
  virtual void LeftMultiplyAndAccumulateFSingleThreaded(const double* x,
                                                        double* y) const final;
  virtual void LeftMultiplyAndAccumulateFMultiThreaded(const double* x,
                                                       double* y) const final;

  // y += Ex
  virtual void RightMultiplyAndAccumulateE(const double* x,
                                           double* y) const final;

  // y += Fx
  virtual void RightMultiplyAndAccumulateF(const double* x,
                                           double* y) const final;

  std::unique_ptr<BlockSparseMatrix> CreateBlockDiagonalEtE() const final;
  std::unique_ptr<BlockSparseMatrix> CreateBlockDiagonalFtF() const final;
  void UpdateBlockDiagonalEtE(BlockSparseMatrix* block_diagonal) const final;
  void UpdateBlockDiagonalEtESingleThreaded(
      BlockSparseMatrix* block_diagonal) const;
  void UpdateBlockDiagonalEtEMultiThreaded(
      BlockSparseMatrix* block_diagonal) const;
  void UpdateBlockDiagonalFtF(BlockSparseMatrix* block_diagonal) const final;
  void UpdateBlockDiagonalFtFSingleThreaded(
      BlockSparseMatrix* block_diagonal) const;
  void UpdateBlockDiagonalFtFMultiThreaded(
      BlockSparseMatrix* block_diagonal) const;
  // clang-format off
  int num_col_blocks_e() const final { return num_col_blocks_e_;  }
  int num_col_blocks_f() const final { return num_col_blocks_f_;  }
  int num_cols_e()       const final { return num_cols_e_;        }
  int num_cols_f()       const final { return num_cols_f_;        }
  int num_rows()         const final { return matrix_.num_rows(); }
  int num_cols()         const final { return matrix_.num_cols(); }
  // clang-format on
  const std::vector<int>& e_cols_partition() const final {
    return e_cols_partition_;
  }
  const std::vector<int>& f_cols_partition() const final {
    return f_cols_partition_;
  }

 private:
  std::unique_ptr<BlockSparseMatrix> CreateBlockDiagonalMatrixLayout(
      int start_col_block, int end_col_block) const;

  const LinearSolver::Options options_;
  const BlockSparseMatrix& matrix_;
  int num_row_blocks_e_;
  int num_col_blocks_e_;
  int num_col_blocks_f_;
  int num_cols_e_;
  int num_cols_f_;
  std::vector<int> e_cols_partition_;
  std::vector<int> f_cols_partition_;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_PARTITIONED_MATRIX_VIEW_H_
