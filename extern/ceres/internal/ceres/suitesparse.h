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
// A simple C++ interface to the SuiteSparse and CHOLMOD libraries.

#ifndef CERES_INTERNAL_SUITESPARSE_H_
#define CERES_INTERNAL_SUITESPARSE_H_

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/config.h"

#ifndef CERES_NO_SUITESPARSE

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "SuiteSparseQR.hpp"
#include "ceres/block_structure.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/linear_solver.h"
#include "ceres/sparse_cholesky.h"
#include "cholmod.h"
#include "glog/logging.h"

namespace ceres::internal {

class CompressedRowSparseMatrix;
class TripletSparseMatrix;

// The raw CHOLMOD and SuiteSparseQR libraries have a slightly
// cumbersome c like calling format. This object abstracts it away and
// provides the user with a simpler interface. The methods here cannot
// be static as a cholmod_common object serves as a global variable
// for all cholmod function calls.
class CERES_NO_EXPORT SuiteSparse {
 public:
  SuiteSparse();
  ~SuiteSparse();

  // Functions for building cholmod_sparse objects from sparse
  // matrices stored in triplet form. The matrix A is not
  // modified. Called owns the result.
  cholmod_sparse* CreateSparseMatrix(TripletSparseMatrix* A);

  // This function works like CreateSparseMatrix, except that the
  // return value corresponds to A' rather than A.
  cholmod_sparse* CreateSparseMatrixTranspose(TripletSparseMatrix* A);

  // Create a cholmod_sparse wrapper around the contents of A. This is
  // a shallow object, which refers to the contents of A and does not
  // use the SuiteSparse machinery to allocate memory.
  cholmod_sparse CreateSparseMatrixTransposeView(CompressedRowSparseMatrix* A);

  // Create a cholmod_dense vector around the contents of the array x.
  // This is a shallow object, which refers to the contents of x and
  // does not use the SuiteSparse machinery to allocate memory.
  cholmod_dense CreateDenseVectorView(const double* x, int size);

  // Given a vector x, build a cholmod_dense vector of size out_size
  // with the first in_size entries copied from x. If x is nullptr, then
  // an all zeros vector is returned. Caller owns the result.
  cholmod_dense* CreateDenseVector(const double* x, int in_size, int out_size);

  // The matrix A is scaled using the matrix whose diagonal is the
  // vector scale. mode describes how scaling is applied. Possible
  // values are CHOLMOD_ROW for row scaling - diag(scale) * A,
  // CHOLMOD_COL for column scaling - A * diag(scale) and CHOLMOD_SYM
  // for symmetric scaling which scales both the rows and the columns
  // - diag(scale) * A * diag(scale).
  void Scale(cholmod_dense* scale, int mode, cholmod_sparse* A) {
    cholmod_scale(scale, mode, A, &cc_);
  }

  // Create and return a matrix m = A * A'. Caller owns the
  // result. The matrix A is not modified.
  cholmod_sparse* AATranspose(cholmod_sparse* A) {
    cholmod_sparse* m = cholmod_aat(A, nullptr, A->nrow, 1, &cc_);
    m->stype = 1;  // Pay attention to the upper triangular part.
    return m;
  }

  // y = alpha * A * x + beta * y. Only y is modified.
  void SparseDenseMultiply(cholmod_sparse* A,
                           double alpha,
                           double beta,
                           cholmod_dense* x,
                           cholmod_dense* y) {
    double alpha_[2] = {alpha, 0};
    double beta_[2] = {beta, 0};
    cholmod_sdmult(A, 0, alpha_, beta_, x, y, &cc_);
  }

  // Compute a symbolic factorization for A or AA' (if A is
  // unsymmetric). If ordering_type is NATURAL, then no fill reducing
  // ordering is computed, otherwise depending on the value of
  // ordering_type AMD or Nested Dissection is used to compute a fill
  // reducing ordering before the symbolic factorization is computed.
  //
  // A is not modified, only the pattern of non-zeros of A is used,
  // the actual numerical values in A are of no consequence.
  //
  // message contains an explanation of the failures if any.
  //
  // Caller owns the result.
  cholmod_factor* AnalyzeCholesky(cholmod_sparse* A,
                                  OrderingType ordering_type,
                                  std::string* message);

  // Block oriented version of AnalyzeCholesky.
  cholmod_factor* BlockAnalyzeCholesky(cholmod_sparse* A,
                                       OrderingType ordering_type,
                                       const std::vector<Block>& row_blocks,
                                       const std::vector<Block>& col_blocks,
                                       std::string* message);

  // If A is symmetric, then compute the symbolic Cholesky
  // factorization of A(ordering, ordering). If A is unsymmetric, then
  // compute the symbolic factorization of
  // A(ordering,:) A(ordering,:)'.
  //
  // A is not modified, only the pattern of non-zeros of A is used,
  // the actual numerical values in A are of no consequence.
  //
  // message contains an explanation of the failures if any.
  //
  // Caller owns the result.
  cholmod_factor* AnalyzeCholeskyWithGivenOrdering(
      cholmod_sparse* A,
      const std::vector<int>& ordering,
      std::string* message);

  // Use the symbolic factorization in L, to find the numerical
  // factorization for the matrix A or AA^T. Return true if
  // successful, false otherwise. L contains the numeric factorization
  // on return.
  //
  // message contains an explanation of the failures if any.
  LinearSolverTerminationType Cholesky(cholmod_sparse* A,
                                       cholmod_factor* L,
                                       std::string* message);

  // Given a Cholesky factorization of a matrix A = LL^T, solve the
  // linear system Ax = b, and return the result. If the Solve fails
  // nullptr is returned. Caller owns the result.
  //
  // message contains an explanation of the failures if any.
  cholmod_dense* Solve(cholmod_factor* L,
                       cholmod_dense* b,
                       std::string* message);

  // Find a fill reducing ordering. ordering is expected to be large
  // enough to hold the ordering. ordering_type must be AMD or NESDIS.
  bool Ordering(cholmod_sparse* matrix,
                OrderingType ordering_type,
                int* ordering);

  // Find the block oriented fill reducing ordering of a matrix A,
  // whose row and column blocks are given by row_blocks, and
  // col_blocks respectively. The matrix may or may not be
  // symmetric. The entries of col_blocks do not need to sum to the
  // number of columns in A. If this is the case, only the first
  // sum(col_blocks) are used to compute the ordering.
  //
  // By virtue of the modeling layer in Ceres being block oriented,
  // all the matrices used by Ceres are also block oriented. When
  // doing sparse direct factorization of these matrices the
  // fill-reducing ordering algorithms can either be run on the block
  // or the scalar form of these matrices. But since the underlying
  // matrices are block oriented, it is worth running the fill
  // reducing ordering on just the block structure of these matrices
  // and then lifting these block orderings to a full scalar
  // ordering. This preserves the block structure of the permuted
  // matrix, and exposes more of the super-nodal structure of the
  // matrix to the numerical factorization routines.
  bool BlockOrdering(const cholmod_sparse* A,
                     OrderingType ordering_type,
                     const std::vector<Block>& row_blocks,
                     const std::vector<Block>& col_blocks,
                     std::vector<int>* ordering);

  // Nested dissection is only available if SuiteSparse is compiled
  // with Metis support.
  static bool IsNestedDissectionAvailable();

  // Find a fill reducing approximate minimum degree
  // ordering. constraints is an array which associates with each
  // column of the matrix an elimination group. i.e., all columns in
  // group 0 are eliminated first, all columns in group 1 are
  // eliminated next etc. This function finds a fill reducing ordering
  // that obeys these constraints.
  //
  // Calling ApproximateMinimumDegreeOrdering is equivalent to calling
  // ConstrainedApproximateMinimumDegreeOrdering with a constraint
  // array that puts all columns in the same elimination group.
  bool ConstrainedApproximateMinimumDegreeOrdering(cholmod_sparse* matrix,
                                                   int* constraints,
                                                   int* ordering);

  void Free(cholmod_sparse* m) { cholmod_free_sparse(&m, &cc_); }
  void Free(cholmod_dense* m) { cholmod_free_dense(&m, &cc_); }
  void Free(cholmod_factor* m) { cholmod_free_factor(&m, &cc_); }

  void Print(cholmod_sparse* m, const std::string& name) {
    cholmod_print_sparse(m, const_cast<char*>(name.c_str()), &cc_);
  }

  void Print(cholmod_dense* m, const std::string& name) {
    cholmod_print_dense(m, const_cast<char*>(name.c_str()), &cc_);
  }

  void Print(cholmod_triplet* m, const std::string& name) {
    cholmod_print_triplet(m, const_cast<char*>(name.c_str()), &cc_);
  }

  cholmod_common* mutable_cc() { return &cc_; }

 private:
  cholmod_common cc_;
};

class CERES_NO_EXPORT SuiteSparseCholesky final : public SparseCholesky {
 public:
  static std::unique_ptr<SparseCholesky> Create(OrderingType ordering_type);

  // SparseCholesky interface.
  ~SuiteSparseCholesky() override;
  CompressedRowSparseMatrix::StorageType StorageType() const final;
  LinearSolverTerminationType Factorize(CompressedRowSparseMatrix* lhs,
                                        std::string* message) final;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) final;

 private:
  explicit SuiteSparseCholesky(const OrderingType ordering_type);

  const OrderingType ordering_type_;
  SuiteSparse ss_;
  cholmod_factor* factor_;
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#else  // CERES_NO_SUITESPARSE

using cholmod_factor = void;

#include "ceres/internal/disable_warnings.h"

namespace ceres {
namespace internal {

class CERES_NO_EXPORT SuiteSparse {
 public:
  // Nested dissection is only available if SuiteSparse is compiled
  // with Metis support.
  static bool IsNestedDissectionAvailable() { return false; }
  void Free(void* /*arg*/) {}
};

}  // namespace internal
}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_NO_SUITESPARSE

#endif  // CERES_INTERNAL_SUITESPARSE_H_
