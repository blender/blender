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
// Author: alexs.mac@gmail.com (Alex Stewart)

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/config.h"

#ifndef CERES_NO_ACCELERATE_SPARSE

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ceres/accelerate_sparse.h"
#include "ceres/compressed_col_sparse_matrix_utils.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/triplet_sparse_matrix.h"
#include "glog/logging.h"

#define CASESTR(x) \
  case x:          \
    return #x

namespace ceres {
namespace internal {

namespace {
const char* SparseStatusToString(SparseStatus_t status) {
  switch (status) {
    CASESTR(SparseStatusOK);
    CASESTR(SparseFactorizationFailed);
    CASESTR(SparseMatrixIsSingular);
    CASESTR(SparseInternalError);
    CASESTR(SparseParameterError);
    CASESTR(SparseStatusReleased);
    default:
      return "UNKNOWN";
  }
}
}  // namespace.

// Resizes workspace as required to contain at least required_size bytes
// aligned to kAccelerateRequiredAlignment and returns a pointer to the
// aligned start.
void* ResizeForAccelerateAlignment(const size_t required_size,
                                   std::vector<uint8_t>* workspace) {
  // As per the Accelerate documentation, all workspace memory passed to the
  // sparse solver functions must be 16-byte aligned.
  constexpr int kAccelerateRequiredAlignment = 16;
  // Although malloc() on macOS should always be 16-byte aligned, it is unclear
  // if this holds for new(), or on other Apple OSs (phoneOS, watchOS etc).
  // As such we assume it is not and use std::align() to create a (potentially
  // offset) 16-byte aligned sub-buffer of the specified size within workspace.
  workspace->resize(required_size + kAccelerateRequiredAlignment);
  size_t size_from_aligned_start = workspace->size();
  void* aligned_solve_workspace_start =
      reinterpret_cast<void*>(workspace->data());
  aligned_solve_workspace_start = std::align(kAccelerateRequiredAlignment,
                                             required_size,
                                             aligned_solve_workspace_start,
                                             size_from_aligned_start);
  CHECK(aligned_solve_workspace_start != nullptr)
      << "required_size: " << required_size
      << ", workspace size: " << workspace->size();
  return aligned_solve_workspace_start;
}

template <typename Scalar>
void AccelerateSparse<Scalar>::Solve(NumericFactorization* numeric_factor,
                                     DenseVector* rhs_and_solution) {
  // From SparseSolve() documentation in Solve.h
  const int required_size = numeric_factor->solveWorkspaceRequiredStatic +
                            numeric_factor->solveWorkspaceRequiredPerRHS;
  SparseSolve(*numeric_factor,
              *rhs_and_solution,
              ResizeForAccelerateAlignment(required_size, &solve_workspace_));
}

template <typename Scalar>
typename AccelerateSparse<Scalar>::ASSparseMatrix
AccelerateSparse<Scalar>::CreateSparseMatrixTransposeView(
    CompressedRowSparseMatrix* A) {
  // Accelerate uses CSC as its sparse storage format whereas Ceres uses CSR.
  // As this method returns the transpose view we can flip rows/cols to map
  // from CSR to CSC^T.
  //
  // Accelerate's columnStarts is a long*, not an int*.  These types might be
  // different (e.g. ARM on iOS) so always make a copy.
  column_starts_.resize(A->num_rows() + 1);  // +1 for final column length.
  std::copy_n(A->rows(), column_starts_.size(), column_starts_.data());

  ASSparseMatrix At;
  At.structure.rowCount = A->num_cols();
  At.structure.columnCount = A->num_rows();
  At.structure.columnStarts = column_starts_.data();
  At.structure.rowIndices = A->mutable_cols();
  At.structure.attributes.transpose = false;
  At.structure.attributes.triangle = SparseUpperTriangle;
  At.structure.attributes.kind = SparseSymmetric;
  At.structure.attributes._reserved = 0;
  At.structure.attributes._allocatedBySparse = 0;
  At.structure.blockSize = 1;
  if constexpr (std::is_same_v<Scalar, double>) {
    At.data = A->mutable_values();
  } else {
    values_ =
        ConstVectorRef(A->values(), A->num_nonzeros()).template cast<Scalar>();
    At.data = values_.data();
  }
  return At;
}

template <typename Scalar>
typename AccelerateSparse<Scalar>::SymbolicFactorization
AccelerateSparse<Scalar>::AnalyzeCholesky(OrderingType ordering_type,
                                          ASSparseMatrix* A) {
  SparseSymbolicFactorOptions sfoption;
  sfoption.control = SparseDefaultControl;
  sfoption.orderMethod = SparseOrderDefault;
  sfoption.order = nullptr;
  sfoption.ignoreRowsAndColumns = nullptr;
  sfoption.malloc = malloc;
  sfoption.free = free;
  sfoption.reportError = nullptr;

  if (ordering_type == OrderingType::AMD) {
    sfoption.orderMethod = SparseOrderAMD;
  } else if (ordering_type == OrderingType::NESDIS) {
    sfoption.orderMethod = SparseOrderMetis;
  }
  return SparseFactor(SparseFactorizationCholesky, A->structure, sfoption);
}

template <typename Scalar>
typename AccelerateSparse<Scalar>::NumericFactorization
AccelerateSparse<Scalar>::Cholesky(ASSparseMatrix* A,
                                   SymbolicFactorization* symbolic_factor) {
  return SparseFactor(*symbolic_factor, *A);
}

template <typename Scalar>
void AccelerateSparse<Scalar>::Cholesky(ASSparseMatrix* A,
                                        NumericFactorization* numeric_factor) {
  // From SparseRefactor() documentation in Solve.h
  const int required_size =
      std::is_same<Scalar, double>::value
          ? numeric_factor->symbolicFactorization.workspaceSize_Double
          : numeric_factor->symbolicFactorization.workspaceSize_Float;
  return SparseRefactor(
      *A,
      numeric_factor,
      ResizeForAccelerateAlignment(required_size, &factorization_workspace_));
}

// Instantiate only for the specific template types required/supported s/t the
// definition can be in the .cc file.
template class AccelerateSparse<double>;
template class AccelerateSparse<float>;

template <typename Scalar>
std::unique_ptr<SparseCholesky> AppleAccelerateCholesky<Scalar>::Create(
    OrderingType ordering_type) {
  return std::unique_ptr<SparseCholesky>(
      new AppleAccelerateCholesky<Scalar>(ordering_type));
}

template <typename Scalar>
AppleAccelerateCholesky<Scalar>::AppleAccelerateCholesky(
    const OrderingType ordering_type)
    : ordering_type_(ordering_type) {}

template <typename Scalar>
AppleAccelerateCholesky<Scalar>::~AppleAccelerateCholesky() {
  FreeSymbolicFactorization();
  FreeNumericFactorization();
}

template <typename Scalar>
CompressedRowSparseMatrix::StorageType
AppleAccelerateCholesky<Scalar>::StorageType() const {
  return CompressedRowSparseMatrix::StorageType::LOWER_TRIANGULAR;
}

template <typename Scalar>
LinearSolverTerminationType AppleAccelerateCholesky<Scalar>::Factorize(
    CompressedRowSparseMatrix* lhs, std::string* message) {
  CHECK_EQ(lhs->storage_type(), StorageType());
  if (lhs == nullptr) {
    *message = "Failure: Input lhs is nullptr.";
    return LinearSolverTerminationType::FATAL_ERROR;
  }
  typename SparseTypesTrait<Scalar>::SparseMatrix as_lhs =
      as_.CreateSparseMatrixTransposeView(lhs);

  if (!symbolic_factor_) {
    symbolic_factor_ = std::make_unique<
        typename SparseTypesTrait<Scalar>::SymbolicFactorization>(
        as_.AnalyzeCholesky(ordering_type_, &as_lhs));

    if (symbolic_factor_->status != SparseStatusOK) {
      *message = StringPrintf(
          "Apple Accelerate Failure : Symbolic factorisation failed: %s",
          SparseStatusToString(symbolic_factor_->status));
      FreeSymbolicFactorization();
      return LinearSolverTerminationType::FATAL_ERROR;
    }
  }

  if (!numeric_factor_) {
    numeric_factor_ = std::make_unique<
        typename SparseTypesTrait<Scalar>::NumericFactorization>(
        as_.Cholesky(&as_lhs, symbolic_factor_.get()));
  } else {
    // Recycle memory from previous numeric factorization.
    as_.Cholesky(&as_lhs, numeric_factor_.get());
  }
  if (numeric_factor_->status != SparseStatusOK) {
    *message = StringPrintf(
        "Apple Accelerate Failure : Numeric factorisation failed: %s",
        SparseStatusToString(numeric_factor_->status));
    FreeNumericFactorization();
    return LinearSolverTerminationType::FAILURE;
  }

  return LinearSolverTerminationType::SUCCESS;
}

template <typename Scalar>
LinearSolverTerminationType AppleAccelerateCholesky<Scalar>::Solve(
    const double* rhs, double* solution, std::string* message) {
  CHECK_EQ(numeric_factor_->status, SparseStatusOK)
      << "Solve called without a call to Factorize first ("
      << SparseStatusToString(numeric_factor_->status) << ").";
  const int num_cols = numeric_factor_->symbolicFactorization.columnCount;

  typename SparseTypesTrait<Scalar>::DenseVector as_rhs_and_solution;
  as_rhs_and_solution.count = num_cols;
  if constexpr (std::is_same_v<Scalar, double>) {
    as_rhs_and_solution.data = solution;
    std::copy_n(rhs, num_cols, solution);
  } else {
    scalar_rhs_and_solution_ =
        ConstVectorRef(rhs, num_cols).template cast<Scalar>();
    as_rhs_and_solution.data = scalar_rhs_and_solution_.data();
  }
  as_.Solve(numeric_factor_.get(), &as_rhs_and_solution);
  if (!std::is_same<Scalar, double>::value) {
    VectorRef(solution, num_cols) =
        scalar_rhs_and_solution_.template cast<double>();
  }
  return LinearSolverTerminationType::SUCCESS;
}

template <typename Scalar>
void AppleAccelerateCholesky<Scalar>::FreeSymbolicFactorization() {
  if (symbolic_factor_) {
    SparseCleanup(*symbolic_factor_);
    symbolic_factor_ = nullptr;
  }
}

template <typename Scalar>
void AppleAccelerateCholesky<Scalar>::FreeNumericFactorization() {
  if (numeric_factor_) {
    SparseCleanup(*numeric_factor_);
    numeric_factor_ = nullptr;
  }
}

// Instantiate only for the specific template types required/supported s/t the
// definition can be in the .cc file.
template class AppleAccelerateCholesky<double>;
template class AppleAccelerateCholesky<float>;

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_ACCELERATE_SPARSE
