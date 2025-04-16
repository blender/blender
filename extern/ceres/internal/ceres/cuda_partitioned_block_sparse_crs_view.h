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

#ifndef CERES_INTERNAL_CUDA_PARTITIONED_BLOCK_SPARSE_CRS_VIEW_H_
#define CERES_INTERNAL_CUDA_PARTITIONED_BLOCK_SPARSE_CRS_VIEW_H_

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
// neither block-sparse format with varying size of the blocks nor
// submatrix-vector products. Thus, we perform the following operations in order
// to compute products of partitioned block-sparse matrices and dense vectors on
// gpu:
//  - Once per block-sparse structure update:
//    - Compute CRS structures of left and right submatrices from block-sparse
//    structure
//    - Check if values of F sub-matrix can be copied without permutation
//    matrices
//  - Once per block-sparse values update:
//    - Copy values of E sub-matrix
//    - Permute or copy values of F sub-matrix
//
// It is assumed that cells of block-sparse matrix are laid out sequentially in
// both of sub-matrices and there is exactly one cell in row-block of E
// sub-matrix in the first num_row_blocks_e_ row blocks, and no cells in E
// sub-matrix below num_row_blocks_e_ row blocks.
//
// This class avoids storing both CRS and block-sparse values in GPU memory.
// Instead, block-sparse values are transferred to gpu memory as a disjoint set
// of small continuous segments with simultaneous permutation of the values into
// correct order using block-structure.
class CERES_NO_EXPORT CudaPartitionedBlockSparseCRSView {
 public:
  // Initializes internal CRS matrix and block-sparse structure on GPU side
  // values. The following objects are stored in gpu memory for the whole
  // lifetime of the object
  //  - matrix_e_: left CRS submatrix
  //  - matrix_f_: right CRS submatrix
  //  - block_structure_: copy of block-sparse structure on GPU
  //  - streamed_buffer_: helper for value updating
  CudaPartitionedBlockSparseCRSView(const BlockSparseMatrix& bsm,
                                    const int num_col_blocks_e,
                                    ContextImpl* context);

  // Update values of CRS submatrices using values of block-sparse matrix.
  // Assumes that bsm has the same block-sparse structure as matrix that was
  // used for construction.
  void UpdateValues(const BlockSparseMatrix& bsm);

  const CudaSparseMatrix* matrix_e() const { return matrix_e_.get(); }
  const CudaSparseMatrix* matrix_f() const { return matrix_f_.get(); }
  CudaSparseMatrix* mutable_matrix_e() { return matrix_e_.get(); }
  CudaSparseMatrix* mutable_matrix_f() { return matrix_f_.get(); }

 private:
  // Value permutation kernel performs a single element-wise operation per
  // thread, thus performing permutation in blocks of 8 megabytes of
  // block-sparse  values seems reasonable
  static constexpr int kMaxTemporaryArraySize = 1 * 1024 * 1024;
  std::unique_ptr<CudaSparseMatrix> matrix_e_;
  std::unique_ptr<CudaSparseMatrix> matrix_f_;
  std::unique_ptr<CudaStreamedBuffer<double>> streamed_buffer_;
  std::unique_ptr<CudaBlockSparseStructure> block_structure_;
  bool f_is_crs_compatible_;
  int num_row_blocks_e_;
  ContextImpl* context_;
};

}  // namespace ceres::internal

#endif  // CERES_NO_CUDA
#endif  // CERES_INTERNAL_CUDA_PARTITIONED_BLOCK_SPARSE_CRS_VIEW_H_
