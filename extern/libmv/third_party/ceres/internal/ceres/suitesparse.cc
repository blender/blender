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

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

#ifndef CERES_NO_SUITESPARSE
#include "ceres/suitesparse.h"

#include <vector>
#include "cholmod.h"
#include "ceres/compressed_col_sparse_matrix_utils.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/linear_solver.h"
#include "ceres/triplet_sparse_matrix.h"

namespace ceres {
namespace internal {

SuiteSparse::SuiteSparse() {
  cholmod_start(&cc_);
}

SuiteSparse::~SuiteSparse() {
  cholmod_finish(&cc_);
}

cholmod_sparse* SuiteSparse::CreateSparseMatrix(TripletSparseMatrix* A) {
  cholmod_triplet triplet;

  triplet.nrow = A->num_rows();
  triplet.ncol = A->num_cols();
  triplet.nzmax = A->max_num_nonzeros();
  triplet.nnz = A->num_nonzeros();
  triplet.i = reinterpret_cast<void*>(A->mutable_rows());
  triplet.j = reinterpret_cast<void*>(A->mutable_cols());
  triplet.x = reinterpret_cast<void*>(A->mutable_values());
  triplet.stype = 0;  // Matrix is not symmetric.
  triplet.itype = CHOLMOD_INT;
  triplet.xtype = CHOLMOD_REAL;
  triplet.dtype = CHOLMOD_DOUBLE;

  return cholmod_triplet_to_sparse(&triplet, triplet.nnz, &cc_);
}


cholmod_sparse* SuiteSparse::CreateSparseMatrixTranspose(
    TripletSparseMatrix* A) {
  cholmod_triplet triplet;

  triplet.ncol = A->num_rows();  // swap row and columns
  triplet.nrow = A->num_cols();
  triplet.nzmax = A->max_num_nonzeros();
  triplet.nnz = A->num_nonzeros();

  // swap rows and columns
  triplet.j = reinterpret_cast<void*>(A->mutable_rows());
  triplet.i = reinterpret_cast<void*>(A->mutable_cols());
  triplet.x = reinterpret_cast<void*>(A->mutable_values());
  triplet.stype = 0;  // Matrix is not symmetric.
  triplet.itype = CHOLMOD_INT;
  triplet.xtype = CHOLMOD_REAL;
  triplet.dtype = CHOLMOD_DOUBLE;

  return cholmod_triplet_to_sparse(&triplet, triplet.nnz, &cc_);
}

cholmod_sparse SuiteSparse::CreateSparseMatrixTransposeView(
    CompressedRowSparseMatrix* A) {
  cholmod_sparse m;
  m.nrow = A->num_cols();
  m.ncol = A->num_rows();
  m.nzmax = A->num_nonzeros();
  m.nz = NULL;
  m.p = reinterpret_cast<void*>(A->mutable_rows());
  m.i = reinterpret_cast<void*>(A->mutable_cols());
  m.x = reinterpret_cast<void*>(A->mutable_values());
  m.z = NULL;
  m.stype = 0;  // Matrix is not symmetric.
  m.itype = CHOLMOD_INT;
  m.xtype = CHOLMOD_REAL;
  m.dtype = CHOLMOD_DOUBLE;
  m.sorted = 1;
  m.packed = 1;

  return m;
}

cholmod_dense* SuiteSparse::CreateDenseVector(const double* x,
                                              int in_size,
                                              int out_size) {
    CHECK_LE(in_size, out_size);
    cholmod_dense* v = cholmod_zeros(out_size, 1, CHOLMOD_REAL, &cc_);
    if (x != NULL) {
      memcpy(v->x, x, in_size*sizeof(*x));
    }
    return v;
}

cholmod_factor* SuiteSparse::AnalyzeCholesky(cholmod_sparse* A,
                                             string* message) {
  // Cholmod can try multiple re-ordering strategies to find a fill
  // reducing ordering. Here we just tell it use AMD with automatic
  // matrix dependence choice of supernodal versus simplicial
  // factorization.
  cc_.nmethods = 1;
  cc_.method[0].ordering = CHOLMOD_AMD;
  cc_.supernodal = CHOLMOD_AUTO;

  cholmod_factor* factor = cholmod_analyze(A, &cc_);
  if (VLOG_IS_ON(2)) {
    cholmod_print_common(const_cast<char*>("Symbolic Analysis"), &cc_);
  }

  if (cc_.status != CHOLMOD_OK) {
    *message = StringPrintf("cholmod_analyze failed. error code: %d",
                            cc_.status);
    return NULL;
  }

  return CHECK_NOTNULL(factor);
}

cholmod_factor* SuiteSparse::BlockAnalyzeCholesky(
    cholmod_sparse* A,
    const vector<int>& row_blocks,
    const vector<int>& col_blocks,
    string* message) {
  vector<int> ordering;
  if (!BlockAMDOrdering(A, row_blocks, col_blocks, &ordering)) {
    return NULL;
  }
  return AnalyzeCholeskyWithUserOrdering(A, ordering, message);
}

cholmod_factor* SuiteSparse::AnalyzeCholeskyWithUserOrdering(
    cholmod_sparse* A,
    const vector<int>& ordering,
    string* message) {
  CHECK_EQ(ordering.size(), A->nrow);

  cc_.nmethods = 1;
  cc_.method[0].ordering = CHOLMOD_GIVEN;

  cholmod_factor* factor  =
      cholmod_analyze_p(A, const_cast<int*>(&ordering[0]), NULL, 0, &cc_);
  if (VLOG_IS_ON(2)) {
    cholmod_print_common(const_cast<char*>("Symbolic Analysis"), &cc_);
  }
  if (cc_.status != CHOLMOD_OK) {
    *message = StringPrintf("cholmod_analyze failed. error code: %d",
                            cc_.status);
    return NULL;
  }

  return CHECK_NOTNULL(factor);
}

cholmod_factor* SuiteSparse::AnalyzeCholeskyWithNaturalOrdering(
    cholmod_sparse* A,
    string* message) {
  cc_.nmethods = 1;
  cc_.method[0].ordering = CHOLMOD_NATURAL;
  cc_.postorder = 0;

  cholmod_factor* factor  = cholmod_analyze(A, &cc_);
  if (VLOG_IS_ON(2)) {
    cholmod_print_common(const_cast<char*>("Symbolic Analysis"), &cc_);
  }
  if (cc_.status != CHOLMOD_OK) {
    *message = StringPrintf("cholmod_analyze failed. error code: %d",
                            cc_.status);
    return NULL;
  }

  return CHECK_NOTNULL(factor);
}

bool SuiteSparse::BlockAMDOrdering(const cholmod_sparse* A,
                                   const vector<int>& row_blocks,
                                   const vector<int>& col_blocks,
                                   vector<int>* ordering) {
  const int num_row_blocks = row_blocks.size();
  const int num_col_blocks = col_blocks.size();

  // Arrays storing the compressed column structure of the matrix
  // incoding the block sparsity of A.
  vector<int> block_cols;
  vector<int> block_rows;

  CompressedColumnScalarMatrixToBlockMatrix(reinterpret_cast<const int*>(A->i),
                                            reinterpret_cast<const int*>(A->p),
                                            row_blocks,
                                            col_blocks,
                                            &block_rows,
                                            &block_cols);

  cholmod_sparse_struct block_matrix;
  block_matrix.nrow = num_row_blocks;
  block_matrix.ncol = num_col_blocks;
  block_matrix.nzmax = block_rows.size();
  block_matrix.p = reinterpret_cast<void*>(&block_cols[0]);
  block_matrix.i = reinterpret_cast<void*>(&block_rows[0]);
  block_matrix.x = NULL;
  block_matrix.stype = A->stype;
  block_matrix.itype = CHOLMOD_INT;
  block_matrix.xtype = CHOLMOD_PATTERN;
  block_matrix.dtype = CHOLMOD_DOUBLE;
  block_matrix.sorted = 1;
  block_matrix.packed = 1;

  vector<int> block_ordering(num_row_blocks);
  if (!cholmod_amd(&block_matrix, NULL, 0, &block_ordering[0], &cc_)) {
    return false;
  }

  BlockOrderingToScalarOrdering(row_blocks, block_ordering, ordering);
  return true;
}

LinearSolverTerminationType SuiteSparse::Cholesky(cholmod_sparse* A,
                                                  cholmod_factor* L,
                                                  string* message) {
  CHECK_NOTNULL(A);
  CHECK_NOTNULL(L);

  // Save the current print level and silence CHOLMOD, otherwise
  // CHOLMOD is prone to dumping stuff to stderr, which can be
  // distracting when the error (matrix is indefinite) is not a fatal
  // failure.
  const int old_print_level = cc_.print;
  cc_.print = 0;

  cc_.quick_return_if_not_posdef = 1;
  int cholmod_status = cholmod_factorize(A, L, &cc_);
  cc_.print = old_print_level;

  // TODO(sameeragarwal): This switch statement is not consistent. It
  // treats all kinds of CHOLMOD failures as warnings. Some of these
  // like out of memory are definitely not warnings. The problem is
  // that the return value Cholesky is two valued, but the state of
  // the linear solver is really three valued. SUCCESS,
  // NON_FATAL_FAILURE (e.g., indefinite matrix) and FATAL_FAILURE
  // (e.g. out of memory).
  switch (cc_.status) {
    case CHOLMOD_NOT_INSTALLED:
      *message = "CHOLMOD failure: Method not installed.";
      return LINEAR_SOLVER_FATAL_ERROR;
    case CHOLMOD_OUT_OF_MEMORY:
      *message = "CHOLMOD failure: Out of memory.";
      return LINEAR_SOLVER_FATAL_ERROR;
    case CHOLMOD_TOO_LARGE:
      *message = "CHOLMOD failure: Integer overflow occured.";
      return LINEAR_SOLVER_FATAL_ERROR;
    case CHOLMOD_INVALID:
      *message = "CHOLMOD failure: Invalid input.";
      return LINEAR_SOLVER_FATAL_ERROR;
    case CHOLMOD_NOT_POSDEF:
      *message = "CHOLMOD warning: Matrix not positive definite.";
      return LINEAR_SOLVER_FAILURE;
    case CHOLMOD_DSMALL:
      *message = "CHOLMOD warning: D for LDL' or diag(L) or "
                "LL' has tiny absolute value.";
      return LINEAR_SOLVER_FAILURE;
    case CHOLMOD_OK:
      if (cholmod_status != 0) {
        return LINEAR_SOLVER_SUCCESS;
      }

      *message = "CHOLMOD failure: cholmod_factorize returned false "
          "but cholmod_common::status is CHOLMOD_OK."
          "Please report this to ceres-solver@googlegroups.com.";
      return LINEAR_SOLVER_FATAL_ERROR;
    default:
      *message =
          StringPrintf("Unknown cholmod return code: %d. "
                       "Please report this to ceres-solver@googlegroups.com.",
                       cc_.status);
      return LINEAR_SOLVER_FATAL_ERROR;
  }

  return LINEAR_SOLVER_FATAL_ERROR;
}

cholmod_dense* SuiteSparse::Solve(cholmod_factor* L,
                                  cholmod_dense* b,
                                  string* message) {
  if (cc_.status != CHOLMOD_OK) {
    *message = "cholmod_solve failed. CHOLMOD status is not CHOLMOD_OK";
    return NULL;
  }

  return cholmod_solve(CHOLMOD_A, L, b, &cc_);
}

bool SuiteSparse::ApproximateMinimumDegreeOrdering(cholmod_sparse* matrix,
                                                   int* ordering) {
  return cholmod_amd(matrix, NULL, 0, ordering, &cc_);
}

bool SuiteSparse::ConstrainedApproximateMinimumDegreeOrdering(
    cholmod_sparse* matrix,
    int* constraints,
    int* ordering) {
#ifndef CERES_NO_CAMD
  return cholmod_camd(matrix, NULL, 0, constraints, ordering, &cc_);
#else
  LOG(FATAL) << "Congratulations you have found a bug in Ceres."
             << "Ceres Solver was compiled with SuiteSparse "
             << "version 4.1.0 or less. Calling this function "
             << "in that case is a bug. Please contact the"
             << "the Ceres Solver developers.";
  return false;
#endif
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_SUITESPARSE
