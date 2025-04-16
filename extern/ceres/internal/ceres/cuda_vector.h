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

#ifndef CERES_INTERNAL_CUDA_VECTOR_H_
#define CERES_INTERNAL_CUDA_VECTOR_H_

// This include must come before any #ifndef check on Ceres compile options.
// clang-format off
#include "ceres/internal/config.h"
// clang-format on

#include <math.h>

#include <memory>
#include <string>

#include "ceres/context_impl.h"
#include "ceres/internal/export.h"
#include "ceres/types.h"

#ifndef CERES_NO_CUDA

#include "ceres/cuda_buffer.h"
#include "ceres/cuda_kernels_vector_ops.h"
#include "ceres/internal/eigen.h"
#include "cublas_v2.h"
#include "cusparse.h"

namespace ceres::internal {

// An Nx1 vector, denoted y hosted on the GPU, with CUDA-accelerated operations.
class CERES_NO_EXPORT CudaVector {
 public:
  // Create a pre-allocated vector of size N and return a pointer to it. The
  // caller must ensure that InitCuda() has already been successfully called on
  // context before calling this method.
  CudaVector(ContextImpl* context, int size);

  CudaVector(CudaVector&& other);

  ~CudaVector();

  void Resize(int size);

  // Perform a deep copy of the vector.
  CudaVector& operator=(const CudaVector&);

  // Return the inner product x' * y.
  double Dot(const CudaVector& x) const;

  // Return the L2 norm of the vector (||y||_2).
  double Norm() const;

  // Set all elements to zero.
  void SetZero();

  // Copy from Eigen vector.
  void CopyFromCpu(const Vector& x);

  // Copy from CPU memory array.
  void CopyFromCpu(const double* x);

  // Copy to Eigen vector.
  void CopyTo(Vector* x) const;

  // Copy to CPU memory array. It is the caller's responsibility to ensure
  // that the array is large enough.
  void CopyTo(double* x) const;

  // y = a * x + b * y.
  void Axpby(double a, const CudaVector& x, double b);

  // y = diag(d)' * diag(d) * x + y.
  void DtDxpy(const CudaVector& D, const CudaVector& x);

  // y = s * y.
  void Scale(double s);

  int num_rows() const { return num_rows_; }
  int num_cols() const { return 1; }

  const double* data() const { return data_.data(); }
  double* mutable_data() { return data_.data(); }

  const cusparseDnVecDescr_t& descr() const { return descr_; }

 private:
  CudaVector(const CudaVector&) = delete;
  void DestroyDescriptor();

  int num_rows_ = 0;
  ContextImpl* context_ = nullptr;
  CudaBuffer<double> data_;
  // CuSparse object that describes this dense vector.
  cusparseDnVecDescr_t descr_ = nullptr;
};

// Blas1 operations on Cuda vectors. These functions are needed as an
// abstraction layer so that we can use different versions of a vector style
// object in the conjugate gradients linear solver.
// Context and num_threads arguments are not used by CUDA implementation,
// context embedded into CudaVector is used instead.
inline double Norm(const CudaVector& x,
                   ContextImpl* context = nullptr,
                   int num_threads = 1) {
  (void)context;
  (void)num_threads;
  return x.Norm();
}
inline void SetZero(CudaVector& x,
                    ContextImpl* context = nullptr,
                    int num_threads = 1) {
  (void)context;
  (void)num_threads;
  x.SetZero();
}
inline void Axpby(double a,
                  const CudaVector& x,
                  double b,
                  const CudaVector& y,
                  CudaVector& z,
                  ContextImpl* context = nullptr,
                  int num_threads = 1) {
  (void)context;
  (void)num_threads;
  if (&x == &y && &y == &z) {
    // z = (a + b) * z;
    z.Scale(a + b);
  } else if (&x == &z) {
    // x is aliased to z.
    // z = x
    //   = b * y + a * x;
    z.Axpby(b, y, a);
  } else if (&y == &z) {
    // y is aliased to z.
    // z = y = a * x + b * y;
    z.Axpby(a, x, b);
  } else {
    // General case: all inputs and outputs are distinct.
    z = y;
    z.Axpby(a, x, b);
  }
}
inline double Dot(const CudaVector& x,
                  const CudaVector& y,
                  ContextImpl* context = nullptr,
                  int num_threads = 1) {
  (void)context;
  (void)num_threads;
  return x.Dot(y);
}
inline void Copy(const CudaVector& from,
                 CudaVector& to,
                 ContextImpl* context = nullptr,
                 int num_threads = 1) {
  (void)context;
  (void)num_threads;
  to = from;
}

}  // namespace ceres::internal

#endif  // CERES_NO_CUDA
#endif  // CERES_INTERNAL_CUDA_SPARSE_LINEAR_OPERATOR_H_
