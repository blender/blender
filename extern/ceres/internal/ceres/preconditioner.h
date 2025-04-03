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

#ifndef CERES_INTERNAL_PRECONDITIONER_H_
#define CERES_INTERNAL_PRECONDITIONER_H_

#include <vector>

#include "ceres/casts.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/context_impl.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/linear_operator.h"
#include "ceres/linear_solver.h"
#include "ceres/sparse_matrix.h"
#include "ceres/types.h"

namespace ceres::internal {

class BlockSparseMatrix;
class SparseMatrix;

class CERES_NO_EXPORT Preconditioner : public LinearOperator {
 public:
  struct Options {
    Options() = default;
    Options(const LinearSolver::Options& linear_solver_options)
        : type(linear_solver_options.preconditioner_type),
          visibility_clustering_type(
              linear_solver_options.visibility_clustering_type),
          sparse_linear_algebra_library_type(
              linear_solver_options.sparse_linear_algebra_library_type),
          num_threads(linear_solver_options.num_threads),
          row_block_size(linear_solver_options.row_block_size),
          e_block_size(linear_solver_options.e_block_size),
          f_block_size(linear_solver_options.f_block_size),
          elimination_groups(linear_solver_options.elimination_groups),
          context(linear_solver_options.context) {}

    PreconditionerType type = JACOBI;
    VisibilityClusteringType visibility_clustering_type = CANONICAL_VIEWS;
    SparseLinearAlgebraLibraryType sparse_linear_algebra_library_type =
        SUITE_SPARSE;
    OrderingType ordering_type = OrderingType::NATURAL;

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

  ~Preconditioner() override;

  // Update the numerical value of the preconditioner for the linear
  // system:
  //
  //  |   A   | x = |b|
  //  |diag(D)|     |0|
  //
  // for some vector b. It is important that the matrix A have the
  // same block structure as the one used to construct this object.
  //
  // D can be nullptr, in which case its interpreted as a diagonal matrix
  // of size zero.
  virtual bool Update(const LinearOperator& A, const double* D) = 0;

  // LinearOperator interface. Since the operator is symmetric,
  // LeftMultiplyAndAccumulate and num_cols are just calls to
  // RightMultiplyAndAccumulate and num_rows respectively. Update() must be
  // called before RightMultiplyAndAccumulate can be called.
  void RightMultiplyAndAccumulate(const double* x,
                                  double* y) const override = 0;
  void LeftMultiplyAndAccumulate(const double* x, double* y) const override {
    return RightMultiplyAndAccumulate(x, y);
  }

  int num_rows() const override = 0;
  int num_cols() const override { return num_rows(); }
};

class CERES_NO_EXPORT IdentityPreconditioner : public Preconditioner {
 public:
  IdentityPreconditioner(int num_rows) : num_rows_(num_rows) {}

  bool Update(const LinearOperator& /*A*/, const double* /*D*/) final {
    return true;
  }

  void RightMultiplyAndAccumulate(const double* x, double* y) const final {
    VectorRef(y, num_rows_) += ConstVectorRef(x, num_rows_);
  }

  int num_rows() const final { return num_rows_; }

 private:
  int num_rows_ = -1;
};

// This templated subclass of Preconditioner serves as a base class for
// other preconditioners that depend on the particular matrix layout of
// the underlying linear operator.
template <typename MatrixType>
class CERES_NO_EXPORT TypedPreconditioner : public Preconditioner {
 public:
  bool Update(const LinearOperator& A, const double* D) final {
    return UpdateImpl(*down_cast<const MatrixType*>(&A), D);
  }

 private:
  virtual bool UpdateImpl(const MatrixType& A, const double* D) = 0;
};

// Preconditioners that depend on access to the low level structure
// of a SparseMatrix.
// clang-format off
using SparseMatrixPreconditioner = TypedPreconditioner<SparseMatrix>;
using BlockSparseMatrixPreconditioner = TypedPreconditioner<BlockSparseMatrix>;
using CompressedRowSparseMatrixPreconditioner = TypedPreconditioner<CompressedRowSparseMatrix>;
// clang-format on

// Wrap a SparseMatrix object as a preconditioner.
class CERES_NO_EXPORT SparseMatrixPreconditionerWrapper final
    : public SparseMatrixPreconditioner {
 public:
  // Wrapper does NOT take ownership of the matrix pointer.
  explicit SparseMatrixPreconditionerWrapper(
      const SparseMatrix* matrix, const Preconditioner::Options& options);
  ~SparseMatrixPreconditionerWrapper() override;

  // Preconditioner interface
  void RightMultiplyAndAccumulate(const double* x, double* y) const override;
  int num_rows() const override;

 private:
  bool UpdateImpl(const SparseMatrix& A, const double* D) override;
  const SparseMatrix* matrix_;
  const Preconditioner::Options options_;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_PRECONDITIONER_H_
