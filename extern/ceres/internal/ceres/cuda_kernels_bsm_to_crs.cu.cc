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

#include "ceres/cuda_kernels_bsm_to_crs.h"

#include <cuda_runtime.h>
#include <thrust/execution_policy.h>
#include <thrust/scan.h>

#include "ceres/block_structure.h"
#include "ceres/cuda_kernels_utils.h"

namespace ceres {
namespace internal {

namespace {
inline auto ThrustCudaStreamExecutionPolicy(cudaStream_t stream) {
  // par_nosync execution policy was added in Thrust 1.16
  // https://github.com/NVIDIA/thrust/blob/main/CHANGELOG.md#thrust-1160
#if THRUST_VERSION < 101700
  return thrust::cuda::par.on(stream);
#else
  return thrust::cuda::par_nosync.on(stream);
#endif
}

void* CudaMalloc(size_t size,
                 cudaStream_t stream,
                 bool memory_pools_supported) {
  void* data = nullptr;
  // Stream-ordered alloaction API is available since CUDA 11.2, but might be
  // not implemented by particular device
#if CUDART_VERSION < 11020
#warning \
    "Stream-ordered allocations are unavailable, consider updating CUDA toolkit to version 11.2+"
  cudaMalloc(&data, size);
#else
  if (memory_pools_supported) {
    cudaMallocAsync(&data, size, stream);
  } else {
    cudaMalloc(&data, size);
  }
#endif
  return data;
}

void CudaFree(void* data, cudaStream_t stream, bool memory_pools_supported) {
  // Stream-ordered alloaction API is available since CUDA 11.2, but might be
  // not implemented by particular device
#if CUDART_VERSION < 11020
#warning \
    "Stream-ordered allocations are unavailable, consider updating CUDA toolkit to version 11.2+"
  cudaSuccess, cudaFree(data);
#else
  if (memory_pools_supported) {
    cudaFreeAsync(data, stream);
  } else {
    cudaFree(data);
  }
#endif
}
template <typename T>
T* CudaAllocate(size_t num_elements,
                cudaStream_t stream,
                bool memory_pools_supported) {
  T* data = static_cast<T*>(
      CudaMalloc(num_elements * sizeof(T), stream, memory_pools_supported));
  return data;
}
}  // namespace

// Fill row block id and nnz for each row using block-sparse structure
// represented by a set of flat arrays.
// Inputs:
// - num_row_blocks: number of row-blocks in block-sparse structure
// - first_cell_in_row_block: index of the first cell of the row-block; size:
// num_row_blocks + 1
// - cells: cells of block-sparse structure as a continuous array
// - row_blocks: row blocks of block-sparse structure stored sequentially
// - col_blocks: column blocks of block-sparse structure stored sequentially
// Outputs:
// - rows: rows[i + 1] will contain number of non-zeros in i-th row, rows[0]
// will be set to 0; rows are filled with a shift by one element in order
// to obtain row-index array of CRS matrix with a inclusive scan afterwards
// - row_block_ids: row_block_ids[i] will be set to index of row-block that
// contains i-th row.
// Computation is perform row-block-wise
template <bool partitioned = false>
__global__ void RowBlockIdAndNNZ(
    const int num_row_blocks,
    const int num_col_blocks_e,
    const int num_row_blocks_e,
    const int* __restrict__ first_cell_in_row_block,
    const Cell* __restrict__ cells,
    const Block* __restrict__ row_blocks,
    const Block* __restrict__ col_blocks,
    int* __restrict__ rows_e,
    int* __restrict__ rows_f,
    int* __restrict__ row_block_ids) {
  const int row_block_id = blockIdx.x * blockDim.x + threadIdx.x;
  if (row_block_id > num_row_blocks) {
    // No synchronization is performed in this kernel, thus it is safe to return
    return;
  }
  if (row_block_id == num_row_blocks) {
    // one extra thread sets the first element
    rows_f[0] = 0;
    if constexpr (partitioned) {
      rows_e[0] = 0;
    }
    return;
  }
  const auto& row_block = row_blocks[row_block_id];
  auto first_cell = cells + first_cell_in_row_block[row_block_id];
  const auto last_cell = cells + first_cell_in_row_block[row_block_id + 1];
  int row_nnz_e = 0;
  if (partitioned && row_block_id < num_row_blocks_e) {
    // First cell is a cell from E
    row_nnz_e = col_blocks[first_cell->block_id].size;
    ++first_cell;
  }
  int row_nnz_f = 0;
  for (auto cell = first_cell; cell < last_cell; ++cell) {
    row_nnz_f += col_blocks[cell->block_id].size;
  }
  const int first_row = row_block.position;
  const int last_row = first_row + row_block.size;
  for (int i = first_row; i < last_row; ++i) {
    if constexpr (partitioned) {
      rows_e[i + 1] = row_nnz_e;
    }
    rows_f[i + 1] = row_nnz_f;
    row_block_ids[i] = row_block_id;
  }
}

// Row-wise creation of CRS structure
// Inputs:
// - num_rows: number of rows in matrix
// - first_cell_in_row_block: index of the first cell of the row-block; size:
// num_row_blocks + 1
// - cells: cells of block-sparse structure as a continuous array
// - row_blocks: row blocks of block-sparse structure stored sequentially
// - col_blocks: column blocks of block-sparse structure stored sequentially
// - row_block_ids: index of row-block that corresponds to row
// - rows: row-index array of CRS structure
// Outputs:
// - cols: column-index array of CRS structure
// Computaion is perform row-wise
template <bool partitioned>
__global__ void ComputeColumns(const int num_rows,
                               const int num_row_blocks_e,
                               const int num_col_blocks_e,
                               const int* __restrict__ first_cell_in_row_block,
                               const Cell* __restrict__ cells,
                               const Block* __restrict__ row_blocks,
                               const Block* __restrict__ col_blocks,
                               const int* __restrict__ row_block_ids,
                               const int* __restrict__ rows_e,
                               int* __restrict__ cols_e,
                               const int* __restrict__ rows_f,
                               int* __restrict__ cols_f) {
  const int row = blockIdx.x * blockDim.x + threadIdx.x;
  if (row >= num_rows) {
    // No synchronization is performed in this kernel, thus it is safe to return
    return;
  }
  const int row_block_id = row_block_ids[row];
  // position in crs matrix
  auto first_cell = cells + first_cell_in_row_block[row_block_id];
  const auto last_cell = cells + first_cell_in_row_block[row_block_id + 1];
  const int num_cols_e = col_blocks[num_col_blocks_e].position;
  // For reach cell of row-block only current row is being filled
  if (partitioned && row_block_id < num_row_blocks_e) {
    // The first cell is cell from E
    const auto& col_block = col_blocks[first_cell->block_id];
    const int col_block_size = col_block.size;
    int column_idx = col_block.position;
    int crs_position_e = rows_e[row];
    // Column indices for each element of row_in_block row of current cell
    for (int i = 0; i < col_block_size; ++i, ++crs_position_e) {
      cols_e[crs_position_e] = column_idx++;
    }
    ++first_cell;
  }
  int crs_position_f = rows_f[row];
  for (auto cell = first_cell; cell < last_cell; ++cell) {
    const auto& col_block = col_blocks[cell->block_id];
    const int col_block_size = col_block.size;
    int column_idx = col_block.position - num_cols_e;
    // Column indices for each element of row_in_block row of current cell
    for (int i = 0; i < col_block_size; ++i, ++crs_position_f) {
      cols_f[crs_position_f] = column_idx++;
    }
  }
}

void FillCRSStructure(const int num_row_blocks,
                      const int num_rows,
                      const int* first_cell_in_row_block,
                      const Cell* cells,
                      const Block* row_blocks,
                      const Block* col_blocks,
                      int* rows,
                      int* cols,
                      cudaStream_t stream,
                      bool memory_pools_supported) {
  // Set number of non-zeros per row in rows array and row to row-block map in
  // row_block_ids array
  int* row_block_ids =
      CudaAllocate<int>(num_rows, stream, memory_pools_supported);
  const int num_blocks_blockwise = NumBlocksInGrid(num_row_blocks + 1);
  RowBlockIdAndNNZ<false><<<num_blocks_blockwise, kCudaBlockSize, 0, stream>>>(
      num_row_blocks,
      0,
      0,
      first_cell_in_row_block,
      cells,
      row_blocks,
      col_blocks,
      nullptr,
      rows,
      row_block_ids);
  // Finalize row-index array of CRS strucure by computing prefix sum
  thrust::inclusive_scan(
      ThrustCudaStreamExecutionPolicy(stream), rows, rows + num_rows + 1, rows);

  // Fill cols array of CRS structure
  const int num_blocks_rowwise = NumBlocksInGrid(num_rows);
  ComputeColumns<false><<<num_blocks_rowwise, kCudaBlockSize, 0, stream>>>(
      num_rows,
      0,
      0,
      first_cell_in_row_block,
      cells,
      row_blocks,
      col_blocks,
      row_block_ids,
      nullptr,
      nullptr,
      rows,
      cols);
  CudaFree(row_block_ids, stream, memory_pools_supported);
}

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
                                 bool memory_pools_supported) {
  // Set number of non-zeros per row in rows array and row to row-block map in
  // row_block_ids array
  int* row_block_ids =
      CudaAllocate<int>(num_rows, stream, memory_pools_supported);
  const int num_blocks_blockwise = NumBlocksInGrid(num_row_blocks + 1);
  RowBlockIdAndNNZ<true><<<num_blocks_blockwise, kCudaBlockSize, 0, stream>>>(
      num_row_blocks,
      num_col_blocks_e,
      num_row_blocks_e,
      first_cell_in_row_block,
      cells,
      row_blocks,
      col_blocks,
      rows_e,
      rows_f,
      row_block_ids);
  // Finalize row-index array of CRS strucure by computing prefix sum
  thrust::inclusive_scan(ThrustCudaStreamExecutionPolicy(stream),
                         rows_e,
                         rows_e + num_rows + 1,
                         rows_e);
  thrust::inclusive_scan(ThrustCudaStreamExecutionPolicy(stream),
                         rows_f,
                         rows_f + num_rows + 1,
                         rows_f);

  // Fill cols array of CRS structure
  const int num_blocks_rowwise = NumBlocksInGrid(num_rows);
  ComputeColumns<true><<<num_blocks_rowwise, kCudaBlockSize, 0, stream>>>(
      num_rows,
      num_row_blocks_e,
      num_col_blocks_e,
      first_cell_in_row_block,
      cells,
      row_blocks,
      col_blocks,
      row_block_ids,
      rows_e,
      cols_e,
      rows_f,
      cols_f);
  CudaFree(row_block_ids, stream, memory_pools_supported);
}

template <typename T, typename Predicate>
__device__ int PartitionPoint(const T* data,
                              int first,
                              int last,
                              Predicate&& predicate) {
  if (!predicate(data[first])) {
    return first;
  }
  while (last - first > 1) {
    const auto midpoint = first + (last - first) / 2;
    if (predicate(data[midpoint])) {
      first = midpoint;
    } else {
      last = midpoint;
    }
  }
  return last;
}

// Element-wise reordering of block-sparse values
// - first_cell_in_row_block - position of the first cell of row-block
// - block_sparse_values - segment of block-sparse values starting from
// block_sparse_offset, containing num_values
template <bool partitioned>
__global__ void PermuteToCrsKernel(
    const int block_sparse_offset,
    const int num_values,
    const int num_row_blocks,
    const int num_row_blocks_e,
    const int* __restrict__ first_cell_in_row_block,
    const int* __restrict__ value_offset_row_block_f,
    const Cell* __restrict__ cells,
    const Block* __restrict__ row_blocks,
    const Block* __restrict__ col_blocks,
    const int* __restrict__ crs_rows,
    const double* __restrict__ block_sparse_values,
    double* __restrict__ crs_values) {
  const int value_id = blockIdx.x * blockDim.x + threadIdx.x;
  if (value_id >= num_values) {
    return;
  }
  const int block_sparse_value_id = value_id + block_sparse_offset;
  // Find the corresponding row-block with a binary search
  const int row_block_id =
      (partitioned
           ? PartitionPoint(value_offset_row_block_f,
                            0,
                            num_row_blocks,
                            [block_sparse_value_id] __device__(
                                const int row_block_offset) {
                              return row_block_offset <= block_sparse_value_id;
                            })
           : PartitionPoint(first_cell_in_row_block,
                            0,
                            num_row_blocks,
                            [cells, block_sparse_value_id] __device__(
                                const int row_block_offset) {
                              return cells[row_block_offset].position <=
                                     block_sparse_value_id;
                            })) -
      1;
  // Find cell and calculate offset within the row with a linear scan
  const auto& row_block = row_blocks[row_block_id];
  auto first_cell = cells + first_cell_in_row_block[row_block_id];
  const auto last_cell = cells + first_cell_in_row_block[row_block_id + 1];
  const int row_block_size = row_block.size;
  int num_cols_before = 0;
  if (partitioned && row_block_id < num_row_blocks_e) {
    ++first_cell;
  }
  for (const Cell* cell = first_cell; cell < last_cell; ++cell) {
    const auto& col_block = col_blocks[cell->block_id];
    const int col_block_size = col_block.size;
    const int cell_size = row_block_size * col_block_size;
    if (cell->position + cell_size > block_sparse_value_id) {
      const int pos_in_cell = block_sparse_value_id - cell->position;
      const int row_in_cell = pos_in_cell / col_block_size;
      const int col_in_cell = pos_in_cell % col_block_size;
      const int row = row_in_cell + row_block.position;
      crs_values[crs_rows[row] + num_cols_before + col_in_cell] =
          block_sparse_values[value_id];
      break;
    }
    num_cols_before += col_block_size;
  }
}

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
                  cudaStream_t stream) {
  const int num_blocks_valuewise = NumBlocksInGrid(num_values);
  PermuteToCrsKernel<false>
      <<<num_blocks_valuewise, kCudaBlockSize, 0, stream>>>(
          block_sparse_offset,
          num_values,
          num_row_blocks,
          0,
          first_cell_in_row_block,
          nullptr,
          cells,
          row_blocks,
          col_blocks,
          crs_rows,
          block_sparse_values,
          crs_values);
}

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
                              cudaStream_t stream) {
  const int num_blocks_valuewise = NumBlocksInGrid(num_values);
  PermuteToCrsKernel<true><<<num_blocks_valuewise, kCudaBlockSize, 0, stream>>>(
      block_sparse_offset,
      num_values,
      num_row_blocks,
      num_row_blocks_e,
      first_cell_in_row_block,
      value_offset_row_block_f,
      cells,
      row_blocks,
      col_blocks,
      crs_rows,
      block_sparse_values,
      crs_values);
}

}  // namespace internal
}  // namespace ceres
