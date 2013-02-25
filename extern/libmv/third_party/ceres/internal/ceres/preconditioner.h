// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2013 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_PRECONDITIONER_H_
#define CERES_INTERNAL_PRECONDITIONER_H_

#include <vector>
#include "ceres/linear_operator.h"
#include "ceres/sparse_matrix.h"

namespace ceres {
namespace internal {

class BlockSparseMatrixBase;
class SparseMatrix;

class Preconditioner : public LinearOperator {
 public:
  struct Options {
    Options()
        : type(JACOBI),
          sparse_linear_algebra_library(SUITE_SPARSE),
          use_block_amd(true),
          num_threads(1),
          row_block_size(Dynamic),
          e_block_size(Dynamic),
          f_block_size(Dynamic) {
    }

    PreconditionerType type;

    SparseLinearAlgebraLibraryType sparse_linear_algebra_library;

    // See solver.h for explanation of this option.
    bool use_block_amd;

    // If possible, how many threads the preconditioner can use.
    int num_threads;

    // Hints about the order in which the parameter blocks should be
    // eliminated by the linear solver.
    //
    // For example if elimination_groups is a vector of size k, then
    // the linear solver is informed that it should eliminate the
    // parameter blocks 0 ... elimination_groups[0] - 1 first, and
    // then elimination_groups[0] ... elimination_groups[1] and so
    // on. Within each elimination group, the linear solver is free to
    // choose how the parameter blocks are ordered. Different linear
    // solvers have differing requirements on elimination_groups.
    //
    // The most common use is for Schur type solvers, where there
    // should be at least two elimination groups and the first
    // elimination group must form an independent set in the normal
    // equations. The first elimination group corresponds to the
    // num_eliminate_blocks in the Schur type solvers.
    vector<int> elimination_groups;

    // If the block sizes in a BlockSparseMatrix are fixed, then in
    // some cases the Schur complement based solvers can detect and
    // specialize on them.
    //
    // It is expected that these parameters are set programmatically
    // rather than manually.
    //
    // Please see schur_complement_solver.h and schur_eliminator.h for
    // more details.
    int row_block_size;
    int e_block_size;
    int f_block_size;
  };

  virtual ~Preconditioner();

  // Update the numerical value of the preconditioner for the linear
  // system:
  //
  //  |   A   | x = |b|
  //  |diag(D)|     |0|
  //
  // for some vector b. It is important that the matrix A have the
  // same block structure as the one used to construct this object.
  //
  // D can be NULL, in which case its interpreted as a diagonal matrix
  // of size zero.
  virtual bool Update(const BlockSparseMatrixBase& A, const double* D) = 0;

  // LinearOperator interface. Since the operator is symmetric,
  // LeftMultiply and num_cols are just calls to RightMultiply and
  // num_rows respectively. Update() must be called before
  // RightMultiply can be called.
  virtual void RightMultiply(const double* x, double* y) const = 0;
  virtual void LeftMultiply(const double* x, double* y) const {
    return RightMultiply(x, y);
  }

  virtual int num_rows() const = 0;
  virtual int num_cols() const {
    return num_rows();
  }
};

// Wrap a SparseMatrix object as a preconditioner.
class SparseMatrixPreconditionerWrapper : public Preconditioner {
 public:
  // Wrapper does NOT take ownership of the matrix pointer.
  explicit SparseMatrixPreconditionerWrapper(const SparseMatrix* matrix);
  virtual ~SparseMatrixPreconditionerWrapper();

  // Preconditioner interface
  virtual bool Update(const BlockSparseMatrixBase& A, const double* D);
  virtual void RightMultiply(const double* x, double* y) const;
  virtual int num_rows() const;

 private:
  const SparseMatrix* matrix_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_PRECONDITIONER_H_
