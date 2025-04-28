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
// A simple CUDA vector class.

// This include must come before any #ifndef check on Ceres compile options.
// clang-format off
#include "ceres/internal/config.h"
// clang-format on

#include <math.h>

#include "ceres/context_impl.h"
#include "ceres/internal/export.h"
#include "ceres/types.h"

#ifndef CERES_NO_CUDA

#include "ceres/cuda_buffer.h"
#include "ceres/cuda_kernels_vector_ops.h"
#include "ceres/cuda_vector.h"
#include "cublas_v2.h"

namespace ceres::internal {

CudaVector::CudaVector(ContextImpl* context, int size)
    : context_(context), data_(context, size) {
  DCHECK_NE(context, nullptr);
  DCHECK(context->IsCudaInitialized());
  Resize(size);
}

CudaVector::CudaVector(CudaVector&& other)
    : num_rows_(other.num_rows_),
      context_(other.context_),
      data_(std::move(other.data_)),
      descr_(other.descr_) {
  other.num_rows_ = 0;
  other.descr_ = nullptr;
}

CudaVector& CudaVector::operator=(const CudaVector& other) {
  if (this != &other) {
    Resize(other.num_rows());
    data_.CopyFromGPUArray(other.data_.data(), num_rows_);
  }
  return *this;
}

void CudaVector::DestroyDescriptor() {
  if (descr_ != nullptr) {
    CHECK_EQ(cusparseDestroyDnVec(descr_), CUSPARSE_STATUS_SUCCESS);
    descr_ = nullptr;
  }
}

CudaVector::~CudaVector() { DestroyDescriptor(); }

void CudaVector::Resize(int size) {
  data_.Reserve(size);
  num_rows_ = size;
  DestroyDescriptor();
  CHECK_EQ(cusparseCreateDnVec(&descr_, num_rows_, data_.data(), CUDA_R_64F),
           CUSPARSE_STATUS_SUCCESS);
}

double CudaVector::Dot(const CudaVector& x) const {
  double result = 0;
  CHECK_EQ(cublasDdot(context_->cublas_handle_,
                      num_rows_,
                      data_.data(),
                      1,
                      x.data(),
                      1,
                      &result),
           CUBLAS_STATUS_SUCCESS)
      << "CuBLAS cublasDdot failed.";
  return result;
}

double CudaVector::Norm() const {
  double result = 0;
  CHECK_EQ(cublasDnrm2(
               context_->cublas_handle_, num_rows_, data_.data(), 1, &result),
           CUBLAS_STATUS_SUCCESS)
      << "CuBLAS cublasDnrm2 failed.";
  return result;
}

void CudaVector::CopyFromCpu(const double* x) {
  data_.CopyFromCpu(x, num_rows_);
}

void CudaVector::CopyFromCpu(const Vector& x) {
  if (x.rows() != num_rows_) {
    Resize(x.rows());
  }
  CopyFromCpu(x.data());
}

void CudaVector::CopyTo(Vector* x) const {
  CHECK(x != nullptr);
  x->resize(num_rows_);
  data_.CopyToCpu(x->data(), num_rows_);
}

void CudaVector::CopyTo(double* x) const {
  CHECK(x != nullptr);
  data_.CopyToCpu(x, num_rows_);
}

void CudaVector::SetZero() {
  // Allow empty vector to be zeroed
  if (num_rows_ == 0) return;
  CHECK(data_.data() != nullptr);
  CudaSetZeroFP64(data_.data(), num_rows_, context_->DefaultStream());
}

void CudaVector::Axpby(double a, const CudaVector& x, double b) {
  if (&x == this) {
    Scale(a + b);
    return;
  }
  CHECK_EQ(num_rows_, x.num_rows_);
  if (b != 1.0) {
    // First scale y by b.
    CHECK_EQ(
        cublasDscal(context_->cublas_handle_, num_rows_, &b, data_.data(), 1),
        CUBLAS_STATUS_SUCCESS)
        << "CuBLAS cublasDscal failed.";
  }
  // Then add a * x to y.
  CHECK_EQ(cublasDaxpy(context_->cublas_handle_,
                       num_rows_,
                       &a,
                       x.data(),
                       1,
                       data_.data(),
                       1),
           CUBLAS_STATUS_SUCCESS)
      << "CuBLAS cublasDaxpy failed.";
}

void CudaVector::DtDxpy(const CudaVector& D, const CudaVector& x) {
  CudaDtDxpy(
      data_.data(), D.data(), x.data(), num_rows_, context_->DefaultStream());
}

void CudaVector::Scale(double s) {
  CHECK_EQ(
      cublasDscal(context_->cublas_handle_, num_rows_, &s, data_.data(), 1),
      CUBLAS_STATUS_SUCCESS)
      << "CuBLAS cublasDscal failed.";
}

}  // namespace ceres::internal

#endif  // CERES_NO_CUDA
