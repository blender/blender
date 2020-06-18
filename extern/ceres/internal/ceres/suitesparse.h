// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2017 Google Inc. All rights reserved.
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
#include "ceres/internal/port.h"

#ifndef CERES_NO_SUITESPARSE

#include <cstring>
#include <string>
#include <vector>
#include "SuiteSparseQR.hpp"
#include "ceres/linear_solver.h"
#include "ceres/sparse_cholesky.h"
#include "cholmod.h"
#include "glog/logging.h"

// Before SuiteSparse version 4.2.0, cholmod_camd was only enabled
// if SuiteSparse was compiled with Metis support. This makes
// calling and linking into cholmod_camd problematic even though it
// has nothing to do with Metis. This has been fixed reliably in
// 4.2.0.
//
// The fix was actually committed in 4.1.0, but there is
// some confusion about a silent update to the tar ball, so we are
// being conservative and choosing the next minor version where
// things are stable.
#if (SUITESPARSE_VERSION < 4002)
#define CERES_NO_CAMD
#endif

// UF_long is deprecated but SuiteSparse_long is only available in
// newer versions of SuiteSparse. So for older versions of
// SuiteSparse, we define SuiteSparse_long to be the same as UF_long,
// which is what recent versions of SuiteSparse do anyways.
#ifndef SuiteSparse_long
#define SuiteSparse_long UF_long
#endif

namespace ceres {
namespace internal {

class CompressedRowSparseMatrix;
class TripletSparseMatrix;

// The raw CHOLMOD and SuiteSparseQR libraries have a slightly
// cumbersome c like calling format. This object abstracts it away and
// provides the user with a simpler interface. The methods here cannot
// be static as a cholmod_common object serves as a global variable
// for all cholmod function calls.
class SuiteSparse {
 public:
  SuiteSparse();
  ~SuiteSparse();

  // Functions for building cholmod_sparse objects from sparse
  // matrices stored in triplet form. The matrix A is not
  // modifed. Called owns the result.
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
  // with the first in_size entries copied from x. If x is NULL, then
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
    cholmod_sparse*m =  cholmod_aat(A, NULL, A->nrow, 1, &cc_);
    m->stype = 1;  // Pay attention to the upper triangular part.
    return m;
  }

  // y = alpha * A * x + beta * y. Only y is modified.
  void SparseDenseMultiply(cholmod_sparse* A, double alpha, double beta,
                           cholmod_dense* x, cholmod_dense* y) {
    double alpha_[2] = {alpha, 0};
    double beta_[2] = {beta, 0};
    cholmod_sdmult(A, 0, alpha_, beta_, x, y, &cc_);
  }

  // Find an ordering of A or AA' (if A is unsymmetric) that minimizes
  // the fill-in in the Cholesky factorization of the corresponding
  // matrix. This is done by using the AMD algorithm.
  //
  // Using this ordering, the symbolic Cholesky factorization of A (or
  // AA') is computed and returned.
  //
  // A is not modified, only the pattern of non-zeros of A is used,
  // the actual numerical values in A are of no consequence.
  //
  // message contains an explanation of the failures if any.
  //
  // Caller owns the result.
  cholmod_factor* AnalyzeCholesky(cholmod_sparse* A, std::string* message);

  cholmod_factor* BlockAnalyzeCholesky(cholmod_sparse* A,
                                       const std::vector<int>& row_blocks,
                                       const std::vector<int>& col_blocks,
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
  cholmod_factor* AnalyzeCholeskyWithUserOrdering(
      cholmod_sparse* A,
      const std::vector<int>& ordering,
      std::string* message);

  // Perform a symbolic factorization of A without re-ordering A. No
  // postordering of the elimination tree is performed. This ensures
  // that the symbolic factor does not introduce an extra permutation
  // on the matrix. See the documentation for CHOLMOD for more details.
  //
  // message contains an explanation of the failures if any.
  cholmod_factor* AnalyzeCholeskyWithNaturalOrdering(cholmod_sparse* A,
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
  // NULL is returned. Caller owns the result.
  //
  // message contains an explanation of the failures if any.
  cholmod_dense* Solve(cholmod_factor* L, cholmod_dense* b, std::string* message);

  // By virtue of the modeling layer in Ceres being block oriented,
  // all the matrices used by Ceres are also block oriented. When
  // doing sparse direct factorization of these matrices the
  // fill-reducing ordering algorithms (in particular AMD) can either
  // be run on the block or the scalar form of these matrices. The two
  // SuiteSparse::AnalyzeCholesky methods allows the client to
  // compute the symbolic factorization of a matrix by either using
  // AMD on the matrix or a user provided ordering of the rows.
  //
  // But since the underlying matrices are block oriented, it is worth
  // running AMD on just the block structure of these matrices and then
  // lifting these block orderings to a full scalar ordering. This
  // preserves the block structure of the permuted matrix, and exposes
  // more of the super-nodal structure of the matrix to the numerical
  // factorization routines.
  //
  // Find the block oriented AMD ordering of a matrix A, whose row and
  // column blocks are given by row_blocks, and col_blocks
  // respectively. The matrix may or may not be symmetric. The entries
  // of col_blocks do not need to sum to the number of columns in
  // A. If this is the case, only the first sum(col_blocks) are used
  // to compute the ordering.
  bool BlockAMDOrdering(const cholmod_sparse* A,
                        const std::vector<int>& row_blocks,
                        const std::vector<int>& col_blocks,
                        std::vector<int>* ordering);

  // Find a fill reducing approximate minimum degree
  // ordering. ordering is expected to be large enough to hold the
  // ordering.
  bool ApproximateMinimumDegreeOrdering(cholmod_sparse* matrix, int* ordering);


  // Before SuiteSparse version 4.2.0, cholmod_camd was only enabled
  // if SuiteSparse was compiled with Metis support. This makes
  // calling and linking into cholmod_camd problematic even though it
  // has nothing to do with Metis. This has been fixed reliably in
  // 4.2.0.
  //
  // The fix was actually committed in 4.1.0, but there is
  // some confusion about a silent update to the tar ball, so we are
  // being conservative and choosing the next minor version where
  // things are stable.
  static bool IsConstrainedApproximateMinimumDegreeOrderingAvailable() {
    return (SUITESPARSE_VERSION > 4001);
  }

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
  //
  // If CERES_NO_CAMD is defined then calling this function will
  // result in a crash.
  bool ConstrainedApproximateMinimumDegreeOrdering(cholmod_sparse* matrix,
                                                   int* constraints,
                                                   int* ordering);

  void Free(cholmod_sparse* m) { cholmod_free_sparse(&m, &cc_); }
  void Free(cholmod_dense* m)  { cholmod_free_dense(&m, &cc_);  }
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

class SuiteSparseCholesky : public SparseCholesky {
 public:
  static std::unique_ptr<SparseCholesky> Create(
      OrderingType ordering_type);

  // SparseCholesky interface.
  virtual ~SuiteSparseCholesky();
  CompressedRowSparseMatrix::StorageType StorageType() const final;
  LinearSolverTerminationType Factorize(
      CompressedRowSparseMatrix* lhs, std::string* message) final;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) final;
 private:
  SuiteSparseCholesky(const OrderingType ordering_type);

  const OrderingType ordering_type_;
  SuiteSparse ss_;
  cholmod_factor* factor_;
};

}  // namespace internal
}  // namespace ceres

#else  // CERES_NO_SUITESPARSE

typedef void cholmod_factor;

namespace ceres {
namespace internal {

class SuiteSparse {
 public:
  // Defining this static function even when SuiteSparse is not
  // available, allows client code to check for the presence of CAMD
  // without checking for the absence of the CERES_NO_CAMD symbol.
  //
  // This is safer because the symbol maybe missing due to a user
  // accidentally not including suitesparse.h in their code when
  // checking for the symbol.
  static bool IsConstrainedApproximateMinimumDegreeOrderingAvailable() {
    return false;
  }

  void Free(void* arg) {}
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_SUITESPARSE

#endif  // CERES_INTERNAL_SUITESPARSE_H_
