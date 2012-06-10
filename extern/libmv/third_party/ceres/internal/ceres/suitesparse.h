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
// A simple C++ interface to the SuiteSparse and CHOLMOD libraries.

#ifndef CERES_INTERNAL_SUITESPARSE_H_
#define CERES_INTERNAL_SUITESPARSE_H_

#ifndef CERES_NO_SUITESPARSE

#include <cstring>
#include <string>

#include <glog/logging.h>
#include "cholmod.h"
#include "ceres/internal/port.h"

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
  SuiteSparse()  { cholmod_start(&cc_);  }
  ~SuiteSparse() { cholmod_finish(&cc_); }

  // Functions for building cholmod_sparse objects from sparse
  // matrices stored in triplet form. The matrix A is not
  // modifed. Called owns the result.
  cholmod_sparse* CreateSparseMatrix(TripletSparseMatrix* A);

  // This function works like CreateSparseMatrix, except that the
  // return value corresponds to A' rather than A.
  cholmod_sparse* CreateSparseMatrixTranspose(TripletSparseMatrix* A);

  // Create a cholmod_sparse wrapper around the contents of A. This is
  // a shallow object, which refers to the contents of A and does not
  // use the SuiteSparse machinery to allocate memory, this object
  // should be disposed off with a delete and not a call to Free as is
  // the case for objects returned by CreateSparseMatrixTranspose.
  cholmod_sparse* CreateSparseMatrixTransposeView(CompressedRowSparseMatrix* A);

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

  // Analyze the sparsity structure of the matrix A compute the
  // symbolic factorization of A. A is not modified, only the pattern
  // of non-zeros of A is used, the actual numerical values in A are
  // of no consequence. Caller owns the result.
  cholmod_factor* AnalyzeCholesky(cholmod_sparse* A);

  // Use the symbolic factorization in L, to find the numerical
  // factorization for the matrix A or AA^T. Return true if
  // successful, false otherwise. L contains the numeric factorization
  // on return.
  bool Cholesky(cholmod_sparse* A, cholmod_factor* L);

  // Given a Cholesky factorization of a matrix A = LL^T, solve the
  // linear system Ax = b, and return the result. If the Solve fails
  // NULL is returned. Caller owns the result.
  cholmod_dense* Solve(cholmod_factor* L, cholmod_dense* b);

  // Combine the calls to Cholesky and Solve into a single call. If
  // the cholesky factorization or the solve fails, return
  // NULL. Caller owns the result.
  cholmod_dense* SolveCholesky(cholmod_sparse* A,
                               cholmod_factor* L,
                               cholmod_dense* b);

  void Free(cholmod_sparse* m) { cholmod_free_sparse(&m, &cc_); }
  void Free(cholmod_dense* m)  { cholmod_free_dense(&m, &cc_);  }
  void Free(cholmod_factor* m) { cholmod_free_factor(&m, &cc_); }

  void Print(cholmod_sparse* m, const string& name) {
    cholmod_print_sparse(m, const_cast<char*>(name.c_str()), &cc_);
  }

  void Print(cholmod_dense* m, const string& name) {
    cholmod_print_dense(m, const_cast<char*>(name.c_str()), &cc_);
  }

  void Print(cholmod_triplet* m, const string& name) {
    cholmod_print_triplet(m, const_cast<char*>(name.c_str()), &cc_);
  }

  cholmod_common* mutable_cc() { return &cc_; }

 private:
  cholmod_common cc_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_SUITESPARSE

#endif  // CERES_INTERNAL_SUITESPARSE_H_
