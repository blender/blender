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

#ifndef CERES_INTERNAL_CUDA_KERNELS_UTILS_H_
#define CERES_INTERNAL_CUDA_KERNELS_UTILS_H_

namespace ceres {
namespace internal {

// Parallel execution on CUDA device requires splitting job into blocks of a
// fixed size. We use block-size of kCudaBlockSize for all kernels that do not
// require any specific block size. As the CUDA Toolkit documentation says,
// "although arbitrary in this case, is a common choice". This is determined by
// the warp size, max block size, and multiprocessor sizes of recent GPUs. For
// complex kernels with significant register usage and unusual memory patterns,
// the occupancy calculator API might provide better performance. See "Occupancy
// Calculator" under the CUDA toolkit documentation.
constexpr int kCudaBlockSize = 256;

// Compute number of blocks of kCudaBlockSize that span over 1-d grid with
// dimension size. Note that 1-d grid dimension is limited by 2^31-1 in CUDA,
// thus a signed int is used as an argument.
inline int NumBlocksInGrid(int size) {
  return (size + kCudaBlockSize - 1) / kCudaBlockSize;
}
}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_CUDA_KERNELS_UTILS_H_
