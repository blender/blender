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

#include "ceres/cuda_partitioned_block_sparse_crs_view.h"

#ifndef CERES_NO_CUDA

#include "ceres/cuda_block_structure.h"
#include "ceres/cuda_kernels_bsm_to_crs.h"

namespace ceres::internal {

CudaPartitionedBlockSparseCRSView::CudaPartitionedBlockSparseCRSView(
    const BlockSparseMatrix& bsm,
    const int num_col_blocks_e,
    ContextImpl* context)
    :

      context_(context) {
  const auto& bs = *bsm.block_structure();
  block_structure_ =
      std::make_unique<CudaBlockSparseStructure>(bs, num_col_blocks_e, context);
  // Determine number of non-zeros in left submatrix
  // Row-blocks are at least 1 row high, thus we can use a temporary array of
  // num_rows for ComputeNonZerosInColumnBlockSubMatrix; and later reuse it for
  // FillCRSStructurePartitioned
  const int num_rows = bsm.num_rows();
  const int num_nonzeros_e = block_structure_->num_nonzeros_e();
  const int num_nonzeros_f = bsm.num_nonzeros() - num_nonzeros_e;

  const int num_cols_e = num_col_blocks_e < bs.cols.size()
                             ? bs.cols[num_col_blocks_e].position
                             : bsm.num_cols();
  const int num_cols_f = bsm.num_cols() - num_cols_e;

  CudaBuffer<int32_t> rows_e(context, num_rows + 1);
  CudaBuffer<int32_t> cols_e(context, num_nonzeros_e);
  CudaBuffer<int32_t> rows_f(context, num_rows + 1);
  CudaBuffer<int32_t> cols_f(context, num_nonzeros_f);

  num_row_blocks_e_ = block_structure_->num_row_blocks_e();
  FillCRSStructurePartitioned(block_structure_->num_row_blocks(),
                              num_rows,
                              num_row_blocks_e_,
                              num_col_blocks_e,
                              num_nonzeros_e,
                              block_structure_->first_cell_in_row_block(),
                              block_structure_->cells(),
                              block_structure_->row_blocks(),
                              block_structure_->col_blocks(),
                              rows_e.data(),
                              cols_e.data(),
                              rows_f.data(),
                              cols_f.data(),
                              context->DefaultStream(),
                              context->is_cuda_memory_pools_supported_);
  f_is_crs_compatible_ = block_structure_->IsCrsCompatible();
  if (f_is_crs_compatible_) {
    block_structure_ = nullptr;
  } else {
    streamed_buffer_ = std::make_unique<CudaStreamedBuffer<double>>(
        context, kMaxTemporaryArraySize);
  }
  matrix_e_ = std::make_unique<CudaSparseMatrix>(
      num_cols_e, std::move(rows_e), std::move(cols_e), context);
  matrix_f_ = std::make_unique<CudaSparseMatrix>(
      num_cols_f, std::move(rows_f), std::move(cols_f), context);

  CHECK_EQ(bsm.num_nonzeros(),
           matrix_e_->num_nonzeros() + matrix_f_->num_nonzeros());

  UpdateValues(bsm);
}

void CudaPartitionedBlockSparseCRSView::UpdateValues(
    const BlockSparseMatrix& bsm) {
  if (f_is_crs_compatible_) {
    CHECK_EQ(cudaSuccess,
             cudaMemcpyAsync(matrix_e_->mutable_values(),
                             bsm.values(),
                             matrix_e_->num_nonzeros() * sizeof(double),
                             cudaMemcpyHostToDevice,
                             context_->DefaultStream()));

    CHECK_EQ(cudaSuccess,
             cudaMemcpyAsync(matrix_f_->mutable_values(),
                             bsm.values() + matrix_e_->num_nonzeros(),
                             matrix_f_->num_nonzeros() * sizeof(double),
                             cudaMemcpyHostToDevice,
                             context_->DefaultStream()));
    return;
  }
  streamed_buffer_->CopyToGpu(
      bsm.values(),
      bsm.num_nonzeros(),
      [block_structure = block_structure_.get(),
       num_nonzeros_e = matrix_e_->num_nonzeros(),
       num_row_blocks_e = num_row_blocks_e_,
       values_f = matrix_f_->mutable_values(),
       rows_f = matrix_f_->rows()](
          const double* values, int num_values, int offset, auto stream) {
        PermuteToCRSPartitionedF(num_nonzeros_e + offset,
                                 num_values,
                                 block_structure->num_row_blocks(),
                                 num_row_blocks_e,
                                 block_structure->first_cell_in_row_block(),
                                 block_structure->value_offset_row_block_f(),
                                 block_structure->cells(),
                                 block_structure->row_blocks(),
                                 block_structure->col_blocks(),
                                 rows_f,
                                 values,
                                 values_f,
                                 stream);
      });
  CHECK_EQ(cudaSuccess,
           cudaMemcpyAsync(matrix_e_->mutable_values(),
                           bsm.values(),
                           matrix_e_->num_nonzeros() * sizeof(double),
                           cudaMemcpyHostToDevice,
                           context_->DefaultStream()));
}

}  // namespace ceres::internal
#endif  // CERES_NO_CUDA
