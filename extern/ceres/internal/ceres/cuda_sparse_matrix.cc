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
// Author: joydeepb@cs.utexas.edu (Joydeep Biswas)
//
// A CUDA sparse matrix linear operator.

// This include must come before any #ifndef check on Ceres compile options.
// clang-format off
#include "ceres/internal/config.h"
// clang-format on

#include "ceres/cuda_sparse_matrix.h"

#include <math.h>

#include <memory>

#include "ceres/block_sparse_matrix.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/context_impl.h"
#include "ceres/crs_matrix.h"
#include "ceres/internal/export.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"

#ifndef CERES_NO_CUDA

#include "ceres/cuda_buffer.h"
#include "ceres/cuda_kernels_vector_ops.h"
#include "ceres/cuda_vector.h"
#include "cuda_runtime_api.h"
#include "cusparse.h"

namespace ceres::internal {
namespace {
// Starting in CUDA 11.2.1, CUSPARSE_MV_ALG_DEFAULT was deprecated in favor of
// CUSPARSE_SPMV_ALG_DEFAULT.
#if CUDART_VERSION >= 11021
const auto kSpMVAlgorithm = CUSPARSE_SPMV_ALG_DEFAULT;
#else   // CUDART_VERSION >= 11021
const auto kSpMVAlgorithm = CUSPARSE_MV_ALG_DEFAULT;
#endif  // CUDART_VERSION >= 11021
size_t GetTempBufferSizeForOp(const cusparseHandle_t& handle,
                              const cusparseOperation_t op,
                              const cusparseDnVecDescr_t& x,
                              const cusparseDnVecDescr_t& y,
                              const cusparseSpMatDescr_t& A) {
  size_t buffer_size;
  const double alpha = 1.0;
  const double beta = 1.0;
  CHECK_NE(A, nullptr);
  CHECK_EQ(cusparseSpMV_bufferSize(handle,
                                   op,
                                   &alpha,
                                   A,
                                   x,
                                   &beta,
                                   y,
                                   CUDA_R_64F,
                                   kSpMVAlgorithm,
                                   &buffer_size),
           CUSPARSE_STATUS_SUCCESS);
  return buffer_size;
}

size_t GetTempBufferSize(const cusparseHandle_t& handle,
                         const cusparseDnVecDescr_t& left,
                         const cusparseDnVecDescr_t& right,
                         const cusparseSpMatDescr_t& A) {
  CHECK_NE(A, nullptr);
  return std::max(GetTempBufferSizeForOp(
                      handle, CUSPARSE_OPERATION_NON_TRANSPOSE, right, left, A),
                  GetTempBufferSizeForOp(
                      handle, CUSPARSE_OPERATION_TRANSPOSE, left, right, A));
}
}  // namespace

CudaSparseMatrix::CudaSparseMatrix(int num_cols,
                                   CudaBuffer<int32_t>&& rows,
                                   CudaBuffer<int32_t>&& cols,
                                   ContextImpl* context)
    : num_rows_(rows.size() - 1),
      num_cols_(num_cols),
      num_nonzeros_(cols.size()),
      context_(context),
      rows_(std::move(rows)),
      cols_(std::move(cols)),
      values_(context, num_nonzeros_),
      spmv_buffer_(context) {
  Initialize();
}

CudaSparseMatrix::CudaSparseMatrix(ContextImpl* context,
                                   const CompressedRowSparseMatrix& crs_matrix)
    : num_rows_(crs_matrix.num_rows()),
      num_cols_(crs_matrix.num_cols()),
      num_nonzeros_(crs_matrix.num_nonzeros()),
      context_(context),
      rows_(context, num_rows_ + 1),
      cols_(context, num_nonzeros_),
      values_(context, num_nonzeros_),
      spmv_buffer_(context) {
  rows_.CopyFromCpu(crs_matrix.rows(), num_rows_ + 1);
  cols_.CopyFromCpu(crs_matrix.cols(), num_nonzeros_);
  values_.CopyFromCpu(crs_matrix.values(), num_nonzeros_);
  Initialize();
}

CudaSparseMatrix::~CudaSparseMatrix() {
  CHECK_EQ(cusparseDestroySpMat(descr_), CUSPARSE_STATUS_SUCCESS);
  descr_ = nullptr;
  CHECK_EQ(CUSPARSE_STATUS_SUCCESS, cusparseDestroyDnVec(descr_vec_left_));
  CHECK_EQ(CUSPARSE_STATUS_SUCCESS, cusparseDestroyDnVec(descr_vec_right_));
}

void CudaSparseMatrix::CopyValuesFromCpu(
    const CompressedRowSparseMatrix& crs_matrix) {
  // There is no quick and easy way to verify that the structure is unchanged,
  // but at least we can check that the size of the matrix and the number of
  // nonzeros is unchanged.
  CHECK_EQ(num_rows_, crs_matrix.num_rows());
  CHECK_EQ(num_cols_, crs_matrix.num_cols());
  CHECK_EQ(num_nonzeros_, crs_matrix.num_nonzeros());
  values_.CopyFromCpu(crs_matrix.values(), num_nonzeros_);
}

void CudaSparseMatrix::Initialize() {
  CHECK(context_->IsCudaInitialized());
  CHECK_EQ(CUSPARSE_STATUS_SUCCESS,
           cusparseCreateCsr(&descr_,
                             num_rows_,
                             num_cols_,
                             num_nonzeros_,
                             rows_.data(),
                             cols_.data(),
                             values_.data(),
                             CUSPARSE_INDEX_32I,
                             CUSPARSE_INDEX_32I,
                             CUSPARSE_INDEX_BASE_ZERO,
                             CUDA_R_64F));

  // Note: values_.data() is used as non-zero pointer to device memory
  // When there is no non-zero values, data-pointer of values_ array will be a
  // nullptr; but in this case left/right products are trivial and temporary
  // buffer (and vector descriptors) is not required
  if (!num_nonzeros_) return;

  CHECK_EQ(CUSPARSE_STATUS_SUCCESS,
           cusparseCreateDnVec(
               &descr_vec_left_, num_rows_, values_.data(), CUDA_R_64F));
  CHECK_EQ(CUSPARSE_STATUS_SUCCESS,
           cusparseCreateDnVec(
               &descr_vec_right_, num_cols_, values_.data(), CUDA_R_64F));
  size_t buffer_size = GetTempBufferSize(
      context_->cusparse_handle_, descr_vec_left_, descr_vec_right_, descr_);
  spmv_buffer_.Reserve(buffer_size);
}

void CudaSparseMatrix::SpMv(cusparseOperation_t op,
                            const cusparseDnVecDescr_t& x,
                            const cusparseDnVecDescr_t& y) const {
  const double alpha = 1.0;
  const double beta = 1.0;

  CHECK_EQ(cusparseSpMV(context_->cusparse_handle_,
                        op,
                        &alpha,
                        descr_,
                        x,
                        &beta,
                        y,
                        CUDA_R_64F,
                        kSpMVAlgorithm,
                        spmv_buffer_.data()),
           CUSPARSE_STATUS_SUCCESS);
}

void CudaSparseMatrix::RightMultiplyAndAccumulate(const CudaVector& x,
                                                  CudaVector* y) const {
  DCHECK(GetTempBufferSize(
             context_->cusparse_handle_, y->descr(), x.descr(), descr_) <=
         spmv_buffer_.size());
  SpMv(CUSPARSE_OPERATION_NON_TRANSPOSE, x.descr(), y->descr());
}

void CudaSparseMatrix::LeftMultiplyAndAccumulate(const CudaVector& x,
                                                 CudaVector* y) const {
  // TODO(Joydeep Biswas): We should consider storing a transposed copy of the
  // matrix by converting CSR to CSC. From the cuSPARSE documentation:
  // "In general, opA == CUSPARSE_OPERATION_NON_TRANSPOSE is 3x faster than opA
  // != CUSPARSE_OPERATION_NON_TRANSPOSE"
  DCHECK(GetTempBufferSize(
             context_->cusparse_handle_, x.descr(), y->descr(), descr_) <=
         spmv_buffer_.size());
  SpMv(CUSPARSE_OPERATION_TRANSPOSE, x.descr(), y->descr());
}

}  // namespace ceres::internal

#endif  // CERES_NO_CUDA
