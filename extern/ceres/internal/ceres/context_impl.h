// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2018 Google Inc. All rights reserved.
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
#endif  // CERES_NO_CUDA

#ifdef CERES_USE_CXX_THREADS
#include "ceres/thread_pool.h"
#endif  // CERES_USE_CXX_THREADS

namespace ceres {
namespace internal {

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

#ifdef CERES_USE_CXX_THREADS
  ThreadPool thread_pool;
#endif  // CERES_USE_CXX_THREADS

#ifndef CERES_NO_CUDA
  // Initializes the cuSolverDN context, creates an asynchronous stream, and
  // associates the stream with cuSolverDN. Returns true iff initialization was
  // successful, else it returns false and a human-readable error message is
  // returned.
  bool InitCUDA(std::string* message);

  // Handle to the cuSOLVER context.
  cusolverDnHandle_t cusolver_handle_ = nullptr;
  // Handle to cuBLAS context.
  cublasHandle_t cublas_handle_ = nullptr;
  // CUDA device stream.
  cudaStream_t stream_ = nullptr;
  // Indicates whether all the CUDA resources have been initialized.
  bool cuda_initialized_ = false;
#endif  // CERES_NO_CUDA
};

}  // namespace internal
}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_CONTEXT_IMPL_H_
