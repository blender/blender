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

#ifndef CERES_INTERNAL_CUDA_KERNELS_BSM_TO_CRS_H_
#define CERES_INTERNAL_CUDA_KERNELS_BSM_TO_CRS_H_

#include "ceres/internal/config.h"

#ifndef CERES_NO_CUDA

#include "cuda_runtime.h"

namespace ceres {
namespace internal {
struct Block;
struct Cell;

// Compute structure of CRS matrix using block-sparse structure.
// Arrays corresponding to CRS matrix are to be allocated by caller
void FillCRSStructure(const int num_row_blocks,
                      const int num_rows,
                      const int* first_cell_in_row_block,
                      const Cell* cells,
                      const Block* row_blocks,
                      const Block* col_blocks,
                      int* rows,
                      int* cols,
                      cudaStream_t stream,
                      bool memory_pools_supported);

// Compute structure of partitioned CRS matrix using block-sparse structure.
// Arrays corresponding to CRS matrices are to be allocated by caller
void FillCRSStructurePartitioned(const int num_row_blocks,
                                 const int num_rows,
                                 const int num_row_blocks_e,
                                 const int num_col_blocks_e,
                                 const int num_nonzeros_e,
                                 const int* first_cell_in_row_block,
                                 const Cell* cells,
                                 const Block* row_blocks,
                                 const Block* col_blocks,
                                 int* rows_e,
                                 int* cols_e,
                                 int* rows_f,
                                 int* cols_f,
                                 cudaStream_t stream,
                                 bool memory_pools_supported);

// Permute segment of values from block-sparse matrix with sequential layout to
// CRS order. Segment starts at block_sparse_offset and has length of num_values
void PermuteToCRS(const int block_sparse_offset,
                  const int num_values,
                  const int num_row_blocks,
                  const int* first_cell_in_row_block,
                  const Cell* cells,
                  const Block* row_blocks,
                  const Block* col_blocks,
                  const int* crs_rows,
                  const double* block_sparse_values,
                  double* crs_values,
                  cudaStream_t stream);

// Permute segment of values from F sub-matrix of block-sparse partitioned
// matrix with sequential layout to CRS order. Segment starts at
// block_sparse_offset (including the offset induced by values of E submatrix)
// and has length of num_values
void PermuteToCRSPartitionedF(const int block_sparse_offset,
                              const int num_values,
                              const int num_row_blocks,
                              const int num_row_blocks_e,
                              const int* first_cell_in_row_block,
                              const int* value_offset_row_block_f,
                              const Cell* cells,
                              const Block* row_blocks,
                              const Block* col_blocks,
                              const int* crs_rows,
                              const double* block_sparse_values,
                              double* crs_values,
                              cudaStream_t stream);

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_CUDA

#endif  // CERES_INTERNAL_CUDA_KERNELS_BSM_TO_CRS_H_
