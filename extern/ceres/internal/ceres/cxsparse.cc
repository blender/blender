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
// Author: strandmark@google.com (Petter Strandmark)

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

#ifndef CERES_NO_CXSPARSE

#include "ceres/cxsparse.h"

#include <string>
#include <vector>

#include "ceres/compressed_col_sparse_matrix_utils.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/triplet_sparse_matrix.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

using std::vector;

CXSparse::CXSparse() : scratch_(NULL), scratch_size_(0) {}

CXSparse::~CXSparse() {
  if (scratch_size_ > 0) {
    cs_di_free(scratch_);
  }
}

csn* CXSparse::Cholesky(cs_di* A, cs_dis* symbolic_factor) {
  return cs_di_chol(A, symbolic_factor);
}

void CXSparse::Solve(cs_dis* symbolic_factor, csn* numeric_factor, double* b) {
  // Make sure we have enough scratch space available.
  const int num_cols = numeric_factor->L->n;
  if (scratch_size_ < num_cols) {
    if (scratch_size_ > 0) {
      cs_di_free(scratch_);
    }
    scratch_ =
        reinterpret_cast<CS_ENTRY*>(cs_di_malloc(num_cols, sizeof(CS_ENTRY)));
    scratch_size_ = num_cols;
  }

  // When the Cholesky factor succeeded, these methods are
  // guaranteed to succeeded as well. In the comments below, "x"
  // refers to the scratch space.
  //
  // Set x = P * b.
  CHECK(cs_di_ipvec(symbolic_factor->pinv, b, scratch_, num_cols));
  // Set x = L \ x.
  CHECK(cs_di_lsolve(numeric_factor->L, scratch_));
  // Set x = L' \ x.
  CHECK(cs_di_ltsolve(numeric_factor->L, scratch_));
  // Set b = P' * x.
  CHECK(cs_di_pvec(symbolic_factor->pinv, scratch_, b, num_cols));
}

bool CXSparse::SolveCholesky(cs_di* lhs, double* rhs_and_solution) {
  return cs_cholsol(1, lhs, rhs_and_solution);
}

cs_dis* CXSparse::AnalyzeCholesky(cs_di* A) {
  // order = 1 for Cholesky factor.
  return cs_schol(1, A);
}

cs_dis* CXSparse::AnalyzeCholeskyWithNaturalOrdering(cs_di* A) {
  // order = 0 for Natural ordering.
  return cs_schol(0, A);
}

cs_dis* CXSparse::BlockAnalyzeCholesky(cs_di* A,
                                       const vector<int>& row_blocks,
                                       const vector<int>& col_blocks) {
  const int num_row_blocks = row_blocks.size();
  const int num_col_blocks = col_blocks.size();

  vector<int> block_rows;
  vector<int> block_cols;
  CompressedColumnScalarMatrixToBlockMatrix(
      A->i, A->p, row_blocks, col_blocks, &block_rows, &block_cols);
  cs_di block_matrix;
  block_matrix.m = num_row_blocks;
  block_matrix.n = num_col_blocks;
  block_matrix.nz = -1;
  block_matrix.nzmax = block_rows.size();
  block_matrix.p = &block_cols[0];
  block_matrix.i = &block_rows[0];
  block_matrix.x = NULL;

  int* ordering = cs_amd(1, &block_matrix);
  vector<int> block_ordering(num_row_blocks, -1);
  std::copy(ordering, ordering + num_row_blocks, &block_ordering[0]);
  cs_free(ordering);

  vector<int> scalar_ordering;
  BlockOrderingToScalarOrdering(row_blocks, block_ordering, &scalar_ordering);

  cs_dis* symbolic_factor =
      reinterpret_cast<cs_dis*>(cs_calloc(1, sizeof(cs_dis)));
  symbolic_factor->pinv = cs_pinv(&scalar_ordering[0], A->n);
  cs* permuted_A = cs_symperm(A, symbolic_factor->pinv, 0);

  symbolic_factor->parent = cs_etree(permuted_A, 0);
  int* postordering = cs_post(symbolic_factor->parent, A->n);
  int* column_counts =
      cs_counts(permuted_A, symbolic_factor->parent, postordering, 0);
  cs_free(postordering);
  cs_spfree(permuted_A);

  symbolic_factor->cp = (int*)cs_malloc(A->n + 1, sizeof(int));
  symbolic_factor->lnz = cs_cumsum(symbolic_factor->cp, column_counts, A->n);
  symbolic_factor->unz = symbolic_factor->lnz;

  cs_free(column_counts);

  if (symbolic_factor->lnz < 0) {
    cs_sfree(symbolic_factor);
    symbolic_factor = NULL;
  }

  return symbolic_factor;
}

cs_di CXSparse::CreateSparseMatrixTransposeView(CompressedRowSparseMatrix* A) {
  cs_di At;
  At.m = A->num_cols();
  At.n = A->num_rows();
  At.nz = -1;
  At.nzmax = A->num_nonzeros();
  At.p = A->mutable_rows();
  At.i = A->mutable_cols();
  At.x = A->mutable_values();
  return At;
}

cs_di* CXSparse::CreateSparseMatrix(TripletSparseMatrix* tsm) {
  cs_di_sparse tsm_wrapper;
  tsm_wrapper.nzmax = tsm->num_nonzeros();
  tsm_wrapper.nz = tsm->num_nonzeros();
  tsm_wrapper.m = tsm->num_rows();
  tsm_wrapper.n = tsm->num_cols();
  tsm_wrapper.p = tsm->mutable_cols();
  tsm_wrapper.i = tsm->mutable_rows();
  tsm_wrapper.x = tsm->mutable_values();

  return cs_compress(&tsm_wrapper);
}

void CXSparse::ApproximateMinimumDegreeOrdering(cs_di* A, int* ordering) {
  int* cs_ordering = cs_amd(1, A);
  std::copy(cs_ordering, cs_ordering + A->m, ordering);
  cs_free(cs_ordering);
}

cs_di* CXSparse::TransposeMatrix(cs_di* A) { return cs_di_transpose(A, 1); }

cs_di* CXSparse::MatrixMatrixMultiply(cs_di* A, cs_di* B) {
  return cs_di_multiply(A, B);
}

void CXSparse::Free(cs_di* sparse_matrix) { cs_di_spfree(sparse_matrix); }

void CXSparse::Free(cs_dis* symbolic_factor) { cs_di_sfree(symbolic_factor); }

void CXSparse::Free(csn* numeric_factor) { cs_di_nfree(numeric_factor); }

std::unique_ptr<SparseCholesky> CXSparseCholesky::Create(
    const OrderingType ordering_type) {
  return std::unique_ptr<SparseCholesky>(new CXSparseCholesky(ordering_type));
}

CompressedRowSparseMatrix::StorageType CXSparseCholesky::StorageType() const {
  return CompressedRowSparseMatrix::LOWER_TRIANGULAR;
}

CXSparseCholesky::CXSparseCholesky(const OrderingType ordering_type)
    : ordering_type_(ordering_type),
      symbolic_factor_(NULL),
      numeric_factor_(NULL) {}

CXSparseCholesky::~CXSparseCholesky() {
  FreeSymbolicFactorization();
  FreeNumericFactorization();
}

LinearSolverTerminationType CXSparseCholesky::Factorize(
    CompressedRowSparseMatrix* lhs, std::string* message) {
  CHECK_EQ(lhs->storage_type(), StorageType());
  if (lhs == NULL) {
    *message = "Failure: Input lhs is NULL.";
    return LINEAR_SOLVER_FATAL_ERROR;
  }

  cs_di cs_lhs = cs_.CreateSparseMatrixTransposeView(lhs);

  if (symbolic_factor_ == NULL) {
    if (ordering_type_ == NATURAL) {
      symbolic_factor_ = cs_.AnalyzeCholeskyWithNaturalOrdering(&cs_lhs);
    } else {
      if (!lhs->col_blocks().empty() && !(lhs->row_blocks().empty())) {
        symbolic_factor_ = cs_.BlockAnalyzeCholesky(
            &cs_lhs, lhs->col_blocks(), lhs->row_blocks());
      } else {
        symbolic_factor_ = cs_.AnalyzeCholesky(&cs_lhs);
      }
    }

    if (symbolic_factor_ == NULL) {
      *message = "CXSparse Failure : Symbolic factorization failed.";
      return LINEAR_SOLVER_FATAL_ERROR;
    }
  }

  FreeNumericFactorization();
  numeric_factor_ = cs_.Cholesky(&cs_lhs, symbolic_factor_);
  if (numeric_factor_ == NULL) {
    *message = "CXSparse Failure : Numeric factorization failed.";
    return LINEAR_SOLVER_FAILURE;
  }

  return LINEAR_SOLVER_SUCCESS;
}

LinearSolverTerminationType CXSparseCholesky::Solve(const double* rhs,
                                                    double* solution,
                                                    std::string* message) {
  CHECK(numeric_factor_ != NULL)
      << "Solve called without a call to Factorize first.";
  const int num_cols = numeric_factor_->L->n;
  memcpy(solution, rhs, num_cols * sizeof(*solution));
  cs_.Solve(symbolic_factor_, numeric_factor_, solution);
  return LINEAR_SOLVER_SUCCESS;
}

void CXSparseCholesky::FreeSymbolicFactorization() {
  if (symbolic_factor_ != NULL) {
    cs_.Free(symbolic_factor_);
    symbolic_factor_ = NULL;
  }
}

void CXSparseCholesky::FreeNumericFactorization() {
  if (numeric_factor_ != NULL) {
    cs_.Free(numeric_factor_);
    numeric_factor_ = NULL;
  }
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_CXSPARSE
