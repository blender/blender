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

#include "ceres/cuda_block_sparse_crs_view.h"

#ifndef CERES_NO_CUDA

#include "ceres/cuda_kernels_bsm_to_crs.h"

namespace ceres::internal {

CudaBlockSparseCRSView::CudaBlockSparseCRSView(const BlockSparseMatrix& bsm,
                                               ContextImpl* context)
    : context_(context) {
  block_structure_ = std::make_unique<CudaBlockSparseStructure>(
      *bsm.block_structure(), context);
  CudaBuffer<int32_t> rows(context, bsm.num_rows() + 1);
  CudaBuffer<int32_t> cols(context, bsm.num_nonzeros());
  FillCRSStructure(block_structure_->num_row_blocks(),
                   bsm.num_rows(),
                   block_structure_->first_cell_in_row_block(),
                   block_structure_->cells(),
                   block_structure_->row_blocks(),
                   block_structure_->col_blocks(),
                   rows.data(),
                   cols.data(),
                   context->DefaultStream(),
                   context->is_cuda_memory_pools_supported_);
  is_crs_compatible_ = block_structure_->IsCrsCompatible();
  // if matrix is crs-compatible - we can drop block-structure and don't need
  // streamed_buffer_
  if (is_crs_compatible_) {
    VLOG(3) << "Block-sparse matrix is compatible with CRS, discarding "
               "block-structure";
    block_structure_ = nullptr;
  } else {
    streamed_buffer_ = std::make_unique<CudaStreamedBuffer<double>>(
        context_, kMaxTemporaryArraySize);
  }
  crs_matrix_ = std::make_unique<CudaSparseMatrix>(
      bsm.num_cols(), std::move(rows), std::move(cols), context);
  UpdateValues(bsm);
}

void CudaBlockSparseCRSView::UpdateValues(const BlockSparseMatrix& bsm) {
  if (is_crs_compatible_) {
    // Values of CRS-compatible matrices can be copied as-is
    CHECK_EQ(cudaSuccess,
             cudaMemcpyAsync(crs_matrix_->mutable_values(),
                             bsm.values(),
                             bsm.num_nonzeros() * sizeof(double),
                             cudaMemcpyHostToDevice,
                             context_->DefaultStream()));
    return;
  }
  streamed_buffer_->CopyToGpu(
      bsm.values(),
      bsm.num_nonzeros(),
      [bs = block_structure_.get(), crs = crs_matrix_.get()](
          const double* values, int num_values, int offset, auto stream) {
        PermuteToCRS(offset,
                     num_values,
                     bs->num_row_blocks(),
                     bs->first_cell_in_row_block(),
                     bs->cells(),
                     bs->row_blocks(),
                     bs->col_blocks(),
                     crs->rows(),
                     values,
                     crs->mutable_values(),
                     stream);
      });
}

}  // namespace ceres::internal
#endif  // CERES_NO_CUDA
