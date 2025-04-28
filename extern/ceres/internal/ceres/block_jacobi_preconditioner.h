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

#ifndef CERES_INTERNAL_BLOCK_JACOBI_PRECONDITIONER_H_
#define CERES_INTERNAL_BLOCK_JACOBI_PRECONDITIONER_H_

#include <memory>

#include "ceres/block_random_access_diagonal_matrix.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/preconditioner.h"

namespace ceres::internal {

class BlockSparseMatrix;
class CompressedRowSparseMatrix;

// A block Jacobi preconditioner. This is intended for use with
// conjugate gradients, or other iterative symmetric solvers.

// This version of the preconditioner is for use with BlockSparseMatrix
// Jacobians.
//
// TODO(https://github.com/ceres-solver/ceres-solver/issues/936):
// BlockSparseJacobiPreconditioner::RightMultiply will benefit from
// multithreading
class CERES_NO_EXPORT BlockSparseJacobiPreconditioner
    : public BlockSparseMatrixPreconditioner {
 public:
  // A must remain valid while the BlockJacobiPreconditioner is.
  explicit BlockSparseJacobiPreconditioner(Preconditioner::Options,
                                           const BlockSparseMatrix& A);
  ~BlockSparseJacobiPreconditioner() override;
  void RightMultiplyAndAccumulate(const double* x, double* y) const final {
    return m_->RightMultiplyAndAccumulate(x, y);
  }
  int num_rows() const final { return m_->num_rows(); }
  int num_cols() const final { return m_->num_rows(); }
  const BlockRandomAccessDiagonalMatrix& matrix() const { return *m_; }

 private:
  bool UpdateImpl(const BlockSparseMatrix& A, const double* D) final;

  Preconditioner::Options options_;
  std::unique_ptr<BlockRandomAccessDiagonalMatrix> m_;
};

// This version of the preconditioner is for use with CompressedRowSparseMatrix
// Jacobians.
class CERES_NO_EXPORT BlockCRSJacobiPreconditioner
    : public CompressedRowSparseMatrixPreconditioner {
 public:
  // A must remain valid while the BlockJacobiPreconditioner is.
  explicit BlockCRSJacobiPreconditioner(Preconditioner::Options options,
                                        const CompressedRowSparseMatrix& A);
  ~BlockCRSJacobiPreconditioner() override;
  void RightMultiplyAndAccumulate(const double* x, double* y) const final {
    m_->RightMultiplyAndAccumulate(x, y);
  }
  int num_rows() const final { return m_->num_rows(); }
  int num_cols() const final { return m_->num_rows(); }
  const CompressedRowSparseMatrix& matrix() const { return *m_; }

 private:
  bool UpdateImpl(const CompressedRowSparseMatrix& A, const double* D) final;

  Preconditioner::Options options_;
  std::vector<std::mutex> locks_;
  std::unique_ptr<CompressedRowSparseMatrix> m_;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_BLOCK_JACOBI_PRECONDITIONER_H_
