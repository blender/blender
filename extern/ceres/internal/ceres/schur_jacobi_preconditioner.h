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
// Detailed descriptions of these preconditions beyond what is
// documented here can be found in
//
// Bundle Adjustment in the Large
// S. Agarwal, N. Snavely, S. Seitz & R. Szeliski, ECCV 2010
// http://www.cs.washington.edu/homes/sagarwal/bal.pdf

#ifndef CERES_INTERNAL_SCHUR_JACOBI_PRECONDITIONER_H_
#define CERES_INTERNAL_SCHUR_JACOBI_PRECONDITIONER_H_

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "ceres/preconditioner.h"

namespace ceres {
namespace internal {

class BlockRandomAccessDiagonalMatrix;
class BlockSparseMatrix;
struct CompressedRowBlockStructure;
class SchurEliminatorBase;

// This class implements the SCHUR_JACOBI preconditioner for Structure
// from Motion/Bundle Adjustment problems. Full mathematical details
// can be found in
//
// Bundle Adjustment in the Large
// S. Agarwal, N. Snavely, S. Seitz & R. Szeliski, ECCV 2010
// http://www.cs.washington.edu/homes/sagarwal/bal.pdf
//
// Example usage:
//
//   Preconditioner::Options options;
//   options.preconditioner_type = SCHUR_JACOBI;
//   options.elimination_groups.push_back(num_points);
//   options.elimination_groups.push_back(num_cameras);
//   SchurJacobiPreconditioner preconditioner(
//      *A.block_structure(), options);
//   preconditioner.Update(A, NULL);
//   preconditioner.RightMultiply(x, y);
//
class SchurJacobiPreconditioner : public BlockSparseMatrixPreconditioner {
 public:
  // Initialize the symbolic structure of the preconditioner. bs is
  // the block structure of the linear system to be solved. It is used
  // to determine the sparsity structure of the preconditioner matrix.
  //
  // It has the same structural requirement as other Schur complement
  // based solvers. Please see schur_eliminator.h for more details.
  SchurJacobiPreconditioner(const CompressedRowBlockStructure& bs,
                            const Preconditioner::Options& options);
  SchurJacobiPreconditioner(const SchurJacobiPreconditioner&) = delete;
  void operator=(const SchurJacobiPreconditioner&) = delete;

  virtual ~SchurJacobiPreconditioner();

  // Preconditioner interface.
  void RightMultiply(const double* x, double* y) const final;
  int num_rows() const final;

 private:
  void InitEliminator(const CompressedRowBlockStructure& bs);
  bool UpdateImpl(const BlockSparseMatrix& A, const double* D) final;

  Preconditioner::Options options_;
  std::unique_ptr<SchurEliminatorBase> eliminator_;
  // Preconditioner matrix.
  std::unique_ptr<BlockRandomAccessDiagonalMatrix> m_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_SCHUR_JACOBI_PRECONDITIONER_H_
