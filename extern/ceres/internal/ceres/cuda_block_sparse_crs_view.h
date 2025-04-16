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
// Authors: dmitriy.korchemkin@gmail.com (Dmitriy Korchemkin)
//

#ifndef CERES_INTERNAL_CUDA_BLOCK_SPARSE_CRS_VIEW_H_
#define CERES_INTERNAL_CUDA_BLOCK_SPARSE_CRS_VIEW_H_

#include "ceres/internal/config.h"

#ifndef CERES_NO_CUDA

#include <memory>

#include "ceres/block_sparse_matrix.h"
#include "ceres/cuda_block_structure.h"
#include "ceres/cuda_buffer.h"
#include "ceres/cuda_sparse_matrix.h"
#include "ceres/cuda_streamed_buffer.h"

namespace ceres::internal {
// We use cuSPARSE library for SpMV operations. However, it does not support
// block-sparse format with varying size of the blocks. Thus, we perform the
// following operations in order to compute products of block-sparse matrices
// and dense vectors on gpu:
//  - Once per block-sparse structure update:
//    - Compute CRS structure from block-sparse structure and check if values of
//    block-sparse matrix would have the same order as values of CRS matrix
//  - Once per block-sparse values update:
//    - Update values in CRS matrix with values of block-sparse matrix
//
// Only block-sparse matrices with sequential order of cells are supported.
//
// UpdateValues method updates values:
//  - In a single host-to-device copy for matrices with CRS-compatible value
//  layout
//  - Simultaneously transferring and permuting values using CudaStreamedBuffer
//  otherwise
class CERES_NO_EXPORT CudaBlockSparseCRSView {
 public:
  // Initializes internal CRS matrix using structure and values of block-sparse
  // matrix For block-sparse matrices that have value layout different from CRS
  // block-sparse structure will be stored/
  CudaBlockSparseCRSView(const BlockSparseMatrix& bsm, ContextImpl* context);

  const CudaSparseMatrix* crs_matrix() const { return crs_matrix_.get(); }
  CudaSparseMatrix* mutable_crs_matrix() { return crs_matrix_.get(); }

  // Update values of crs_matrix_ using values of block-sparse matrix.
  // Assumes that bsm has the same block-sparse structure as matrix that was
  // used for construction.
  void UpdateValues(const BlockSparseMatrix& bsm);

  // Returns true if block-sparse matrix had CRS-compatible value layout
  bool IsCrsCompatible() const { return is_crs_compatible_; }

  void LeftMultiplyAndAccumulate(const CudaVector& x, CudaVector* y) const {
    crs_matrix()->LeftMultiplyAndAccumulate(x, y);
  }

  void RightMultiplyAndAccumulate(const CudaVector& x, CudaVector* y) const {
    crs_matrix()->RightMultiplyAndAccumulate(x, y);
  }

 private:
  // Value permutation kernel performs a single element-wise operation per
  // thread, thus performing permutation in blocks of 8 megabytes of
  // block-sparse  values seems reasonable
  static constexpr int kMaxTemporaryArraySize = 1 * 1024 * 1024;
  std::unique_ptr<CudaSparseMatrix> crs_matrix_;
  // Only created if block-sparse matrix has non-CRS value layout
  std::unique_ptr<CudaStreamedBuffer<double>> streamed_buffer_;
  // Only stored if block-sparse matrix has non-CRS value layout
  std::unique_ptr<CudaBlockSparseStructure> block_structure_;
  bool is_crs_compatible_;
  ContextImpl* context_;
};

}  // namespace ceres::internal

#endif  // CERES_NO_CUDA
#endif  // CERES_INTERNAL_CUDA_BLOCK_SPARSE_CRS_VIEW_H_
