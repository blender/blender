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

#ifndef CERES_INTERNAL_PRECONDITIONER_H_
#define CERES_INTERNAL_PRECONDITIONER_H_

#include <vector>
#include "ceres/casts.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/context_impl.h"
#include "ceres/linear_operator.h"
#include "ceres/sparse_matrix.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

class BlockSparseMatrix;
class SparseMatrix;

class Preconditioner : public LinearOperator {
 public:
  struct Options {
    PreconditionerType type = JACOBI;
    VisibilityClusteringType visibility_clustering_type = CANONICAL_VIEWS;
    SparseLinearAlgebraLibraryType sparse_linear_algebra_library_type = SUITE_SPARSE;

    // When using the subset preconditioner, all row blocks starting
    // from this row block are used to construct the preconditioner.
    //
    // i.e., the Jacobian matrix A is horizontally partitioned as
    //
    // A = [P]
    //     [Q]
    //
    // where P has subset_preconditioner_start_row_block row blocks,
    // and the preconditioner is the inverse of the matrix Q'Q.
    int subset_preconditioner_start_row_block = -1;

    // See solver.h for information about these flags.
    bool use_postordering = false;

    // If possible, how many threads the preconditioner can use.
    int num_threads = 1;

    // Hints about the order in which the parameter blocks should be
    // eliminated by the linear solver.
    //
    // For example if elimination_groups is a vector of size k, then
    // the linear solver is informed that it should eliminate the
    // parameter blocks 0 ... elimination_groups[0] - 1 first, and
    // then elimination_groups[0] ... elimination_groups[1] - 1 and so
    // on. Within each elimination group, the linear solver is free to
    // choose how the parameter blocks are ordered. Different linear
    // solvers have differing requirements on elimination_groups.
    //
    // The most common use is for Schur type solvers, where there
    // should be at least two elimination groups and the first
    // elimination group must form an independent set in the normal
    // equations. The first elimination group corresponds to the
    // num_eliminate_blocks in the Schur type solvers.
    std::vector<int> elimination_groups;

    // If the block sizes in a BlockSparseMatrix are fixed, then in
    // some cases the Schur complement based solvers can detect and
    // specialize on them.
    //
    // It is expected that these parameters are set programmatically
    // rather than manually.
    //
    // Please see schur_complement_solver.h and schur_eliminator.h for
    // more details.
    int row_block_size = Eigen::Dynamic;
    int e_block_size = Eigen::Dynamic;
    int f_block_size = Eigen::Dynamic;

    ContextImpl* context = nullptr;
  };

  // If the optimization problem is such that there are no remaining
  // e-blocks, ITERATIVE_SCHUR with a Schur type preconditioner cannot
  // be used. This function returns JACOBI if a preconditioner for
  // ITERATIVE_SCHUR is used. The input preconditioner_type is
  // returned otherwise.
  static PreconditionerType PreconditionerForZeroEBlocks(
      PreconditionerType preconditioner_type);

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
  virtual bool Update(const LinearOperator& A, const double* D) = 0;

  // LinearOperator interface. Since the operator is symmetric,
  // LeftMultiply and num_cols are just calls to RightMultiply and
  // num_rows respectively. Update() must be called before
  // RightMultiply can be called.
  void RightMultiply(const double* x, double* y) const override = 0;
  void LeftMultiply(const double* x, double* y) const override {
    return RightMultiply(x, y);
  }

  int num_rows() const override = 0;
  int num_cols() const override {
    return num_rows();
  }
};

// This templated subclass of Preconditioner serves as a base class for
// other preconditioners that depend on the particular matrix layout of
// the underlying linear operator.
template <typename MatrixType>
class TypedPreconditioner : public Preconditioner {
 public:
  virtual ~TypedPreconditioner() {}
  bool Update(const LinearOperator& A, const double* D) final {
    return UpdateImpl(*down_cast<const MatrixType*>(&A), D);
  }

 private:
  virtual bool UpdateImpl(const MatrixType& A, const double* D) = 0;
};

// Preconditioners that depend on access to the low level structure
// of a SparseMatrix.
typedef TypedPreconditioner<SparseMatrix>              SparseMatrixPreconditioner;               // NOLINT
typedef TypedPreconditioner<BlockSparseMatrix>         BlockSparseMatrixPreconditioner;          // NOLINT
typedef TypedPreconditioner<CompressedRowSparseMatrix> CompressedRowSparseMatrixPreconditioner;  // NOLINT

// Wrap a SparseMatrix object as a preconditioner.
class SparseMatrixPreconditionerWrapper : public SparseMatrixPreconditioner {
 public:
  // Wrapper does NOT take ownership of the matrix pointer.
  explicit SparseMatrixPreconditionerWrapper(const SparseMatrix* matrix);
  virtual ~SparseMatrixPreconditionerWrapper();

  // Preconditioner interface
  virtual void RightMultiply(const double* x, double* y) const;
  virtual int num_rows() const;

 private:
  virtual bool UpdateImpl(const SparseMatrix& A, const double* D);
  const SparseMatrix* matrix_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_PRECONDITIONER_H_
