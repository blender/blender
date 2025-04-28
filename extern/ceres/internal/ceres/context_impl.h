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
// Author: vitus@google.com (Michael Vitus)

#ifndef CERES_INTERNAL_CONTEXT_IMPL_H_
#define CERES_INTERNAL_CONTEXT_IMPL_H_

// This include must come before any #ifndef check on Ceres compile options.
// clang-format off
#include "ceres/internal/config.h"
// clang-format on

#include <string>

#include "ceres/context.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"

#ifndef CERES_NO_CUDA
#include "cublas_v2.h"
#include "cuda_runtime.h"
#include "cusolverDn.h"
#include "cusparse.h"
#endif  // CERES_NO_CUDA

#include "ceres/thread_pool.h"

namespace ceres::internal {

class CERES_NO_EXPORT ContextImpl final : public Context {
 public:
  ContextImpl();
  ~ContextImpl() override;
  ContextImpl(const ContextImpl&) = delete;
  void operator=(const ContextImpl&) = delete;

  // When compiled with C++ threading support, resize the thread pool to have
  // at min(num_thread, num_hardware_threads) where num_hardware_threads is
  // defined by the hardware.  Otherwise this call is a no-op.
  void EnsureMinimumThreads(int num_threads);

  ThreadPool thread_pool;

#ifndef CERES_NO_CUDA
  // Note on Ceres' use of CUDA Devices on multi-GPU systems:
  // 1. On a multi-GPU system, if nothing special is done, the "default" CUDA
  //    device will be used, which is device 0.
  // 2. If the user masks out GPUs using the  CUDA_VISIBLE_DEVICES  environment
  //    variable, Ceres will still use device 0 visible to the program, but
  //    device 0 will be the first GPU indicated in the environment variable.
  // 3. If the user explicitly selects a GPU in the host process before calling
  //    Ceres, Ceres will use that GPU.

  // Note on Ceres' use of CUDA Streams:
  // Most of operations on the GPU are performed using a single stream.  In
  // those cases DefaultStream() should be used. This ensures that operations
  // are stream-ordered, and might be concurrent with cpu processing with no
  // additional efforts.
  //
  // a. Single-stream workloads
  //  - Only use default stream
  //  - Return control to the callee without synchronization whenever possible
  //  - Stream synchronization occurs only after GPU to CPU transfers, and is
  //  handled by CudaBuffer
  //
  // b. Multi-stream workloads
  // Multi-stream workloads are more restricted in order to make it harder to
  // get a race-condition.
  //  - Should always synchronize the default stream on entry
  //  - Should always synchronize all utilized streams on exit
  //  - Should not make any assumptions on one of streams_[] being default
  //
  // With those rules in place
  //  - All single-stream asynchronous workloads are serialized using default
  //  stream
  //  - Multiple-stream workloads always wait single-stream workloads to finish
  //  and leave no running computations on exit.
  //  This slightly penalizes multi-stream workloads, but makes it easier to
  //  avoid race conditions when  multiple-stream workload depends on results of
  //  any preceeding gpu computations.

  // Initializes cuBLAS, cuSOLVER, and cuSPARSE contexts, creates an
  // asynchronous CUDA stream, and associates the stream with the contexts.
  // Returns true iff initialization was successful, else it returns false and a
  // human-readable error message is returned.
  bool InitCuda(std::string* message);
  void TearDown();
  inline bool IsCudaInitialized() const { return is_cuda_initialized_; }
  // Returns a human-readable string describing the capabilities of the current
  // CUDA device. CudaConfigAsString can only be called after InitCuda has been
  // called.
  std::string CudaConfigAsString() const;
  // Returns the number of bytes of available global memory on the current CUDA
  // device. If it is called before InitCuda, it returns 0.
  size_t GpuMemoryAvailable() const;

  cusolverDnHandle_t cusolver_handle_ = nullptr;
  cublasHandle_t cublas_handle_ = nullptr;

  // Default stream.
  // Kernel invocations and memory copies on this stream can be left without
  // synchronization.
  cudaStream_t DefaultStream() { return streams_[0]; }
  static constexpr int kNumCudaStreams = 2;
  cudaStream_t streams_[kNumCudaStreams] = {0};

  cusparseHandle_t cusparse_handle_ = nullptr;
  bool is_cuda_initialized_ = false;
  int gpu_device_id_in_use_ = -1;
  cudaDeviceProp gpu_device_properties_;
  bool is_cuda_memory_pools_supported_ = false;
  int cuda_version_major_ = 0;
  int cuda_version_minor_ = 0;
#endif  // CERES_NO_CUDA
};

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_CONTEXT_IMPL_H_
