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

#include "ceres/cuda_kernels_vector_ops.h"

#include <cuda_runtime.h>

#include "ceres/cuda_kernels_utils.h"

namespace ceres {
namespace internal {

template <typename SrcType, typename DstType>
__global__ void TypeConversionKernel(const SrcType* __restrict__ input,
                                     DstType* __restrict__ output,
                                     const int size) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < size) {
    output[i] = static_cast<DstType>(input[i]);
  }
}

void CudaFP64ToFP32(const double* input,
                    float* output,
                    const int size,
                    cudaStream_t stream) {
  const int num_blocks = NumBlocksInGrid(size);
  TypeConversionKernel<double, float>
      <<<num_blocks, kCudaBlockSize, 0, stream>>>(input, output, size);
}

void CudaFP32ToFP64(const float* input,
                    double* output,
                    const int size,
                    cudaStream_t stream) {
  const int num_blocks = NumBlocksInGrid(size);
  TypeConversionKernel<float, double>
      <<<num_blocks, kCudaBlockSize, 0, stream>>>(input, output, size);
}

template <typename T>
__global__ void SetZeroKernel(T* __restrict__ output, const int size) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < size) {
    output[i] = T(0.0);
  }
}

void CudaSetZeroFP32(float* output, const int size, cudaStream_t stream) {
  const int num_blocks = NumBlocksInGrid(size);
  SetZeroKernel<float><<<num_blocks, kCudaBlockSize, 0, stream>>>(output, size);
}

void CudaSetZeroFP64(double* output, const int size, cudaStream_t stream) {
  const int num_blocks = NumBlocksInGrid(size);
  SetZeroKernel<double>
      <<<num_blocks, kCudaBlockSize, 0, stream>>>(output, size);
}

template <typename SrcType, typename DstType>
__global__ void XPlusEqualsYKernel(DstType* __restrict__ x,
                                   const SrcType* __restrict__ y,
                                   const int size) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < size) {
    x[i] = x[i] + DstType(y[i]);
  }
}

void CudaDsxpy(double* x, float* y, const int size, cudaStream_t stream) {
  const int num_blocks = NumBlocksInGrid(size);
  XPlusEqualsYKernel<float, double>
      <<<num_blocks, kCudaBlockSize, 0, stream>>>(x, y, size);
}

__global__ void CudaDtDxpyKernel(double* __restrict__ y,
                                 const double* D,
                                 const double* __restrict__ x,
                                 const int size) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < size) {
    y[i] = y[i] + D[i] * D[i] * x[i];
  }
}

void CudaDtDxpy(double* y,
                const double* D,
                const double* x,
                const int size,
                cudaStream_t stream) {
  const int num_blocks = NumBlocksInGrid(size);
  CudaDtDxpyKernel<<<num_blocks, kCudaBlockSize, 0, stream>>>(y, D, x, size);
}

}  // namespace internal
}  // namespace ceres
