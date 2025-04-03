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
// Author: sameeragarwal@google.com (Sameer Agarwal)
//
// TODO(sameeragarwal): row_block_counter can perhaps be replaced by
// Chunk::start ?

#ifndef CERES_INTERNAL_SCHUR_ELIMINATOR_IMPL_H_
#define CERES_INTERNAL_SCHUR_ELIMINATOR_IMPL_H_

// Eigen has an internal threshold switching between different matrix
// multiplication algorithms. In particular for matrices larger than
// EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD it uses a cache friendly
// matrix matrix product algorithm that has a higher setup cost. For
// matrix sizes close to this threshold, especially when the matrices
// are thin and long, the default choice may not be optimal. This is
// the case for us, as the default choice causes a 30% performance
// regression when we moved from Eigen2 to Eigen3.

#define EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD 10

// This include must come before any #ifndef check on Ceres compile options.
// clang-format off
#include "ceres/internal/config.h"
// clang-format on

#include <algorithm>
#include <map>

#include "Eigen/Dense"
#include "ceres/block_random_access_matrix.h"
#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/invert_psd_matrix.h"
#include "ceres/map_util.h"
#include "ceres/parallel_for.h"
#include "ceres/schur_eliminator.h"
#include "ceres/scoped_thread_token.h"
#include "ceres/small_blas.h"
#include "ceres/stl_util.h"
#include "ceres/thread_token_provider.h"
#include "glog/logging.h"

namespace ceres::internal {

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
SchurEliminator<kRowBlockSize, kEBlockSize, kFBlockSize>::~SchurEliminator() {
  STLDeleteElements(&rhs_locks_);
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void SchurEliminator<kRowBlockSize, kEBlockSize, kFBlockSize>::Init(
    int num_eliminate_blocks,
    bool assume_full_rank_ete,
    const CompressedRowBlockStructure* bs) {
  CHECK_GT(num_eliminate_blocks, 0)
      << "SchurComplementSolver cannot be initialized with "
      << "num_eliminate_blocks = 0.";

  num_eliminate_blocks_ = num_eliminate_blocks;
  assume_full_rank_ete_ = assume_full_rank_ete;

  const int num_col_blocks = bs->cols.size();
  const int num_row_blocks = bs->rows.size();

  buffer_size_ = 1;
  chunks_.clear();
  lhs_row_layout_.clear();

  int lhs_num_rows = 0;
  // Add a map object for each block in the reduced linear system
  // and build the row/column block structure of the reduced linear
  // system.
  lhs_row_layout_.resize(num_col_blocks - num_eliminate_blocks_);
  for (int i = num_eliminate_blocks_; i < num_col_blocks; ++i) {
    lhs_row_layout_[i - num_eliminate_blocks_] = lhs_num_rows;
    lhs_num_rows += bs->cols[i].size;
  }

  // TODO(sameeragarwal): Now that we may have subset block structure,
  // we need to make sure that we account for the fact that some
  // point blocks only have a "diagonal" row and nothing more.
  //
  // This likely requires a slightly different algorithm, which works
  // off of the number of elimination blocks.

  int r = 0;
  // Iterate over the row blocks of A, and detect the chunks. The
  // matrix should already have been ordered so that all rows
  // containing the same y block are vertically contiguous. Along
  // the way also compute the amount of space each chunk will need
  // to perform the elimination.
  while (r < num_row_blocks) {
    const int chunk_block_id = bs->rows[r].cells.front().block_id;
    if (chunk_block_id >= num_eliminate_blocks_) {
      break;
    }

    chunks_.push_back(Chunk(r));
    Chunk& chunk = chunks_.back();
    int buffer_size = 0;
    const int e_block_size = bs->cols[chunk_block_id].size;

    // Add to the chunk until the first block in the row is
    // different than the one in the first row for the chunk.
    while (r + chunk.size < num_row_blocks) {
      const CompressedRow& row = bs->rows[r + chunk.size];
      if (row.cells.front().block_id != chunk_block_id) {
        break;
      }

      // Iterate over the blocks in the row, ignoring the first
      // block since it is the one to be eliminated.
      for (int c = 1; c < row.cells.size(); ++c) {
        const Cell& cell = row.cells[c];
        if (InsertIfNotPresent(
                &(chunk.buffer_layout), cell.block_id, buffer_size)) {
          buffer_size += e_block_size * bs->cols[cell.block_id].size;
        }
      }

      buffer_size_ = std::max(buffer_size, buffer_size_);
      ++chunk.size;
    }

    CHECK_GT(chunk.size, 0);  // This check will need to be resolved.
    r += chunk.size;
  }
  const Chunk& chunk = chunks_.back();

  uneliminated_row_begins_ = chunk.start + chunk.size;

  buffer_ = std::make_unique<double[]>(buffer_size_ * num_threads_);

  // chunk_outer_product_buffer_ only needs to store e_block_size *
  // f_block_size, which is always less than buffer_size_, so we just
  // allocate buffer_size_ per thread.
  chunk_outer_product_buffer_ =
      std::make_unique<double[]>(buffer_size_ * num_threads_);

  STLDeleteElements(&rhs_locks_);
  rhs_locks_.resize(num_col_blocks - num_eliminate_blocks_);
  for (int i = 0; i < num_col_blocks - num_eliminate_blocks_; ++i) {
    rhs_locks_[i] = new std::mutex;
  }
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void SchurEliminator<kRowBlockSize, kEBlockSize, kFBlockSize>::Eliminate(
    const BlockSparseMatrixData& A,
    const double* b,
    const double* D,
    BlockRandomAccessMatrix* lhs,
    double* rhs) {
  if (lhs->num_rows() > 0) {
    lhs->SetZero();
    if (rhs) {
      VectorRef(rhs, lhs->num_rows()).setZero();
    }
  }

  const CompressedRowBlockStructure* bs = A.block_structure();
  const int num_col_blocks = bs->cols.size();

  // Add the diagonal to the schur complement.
  if (D != nullptr) {
    ParallelFor(context_,
                num_eliminate_blocks_,
                num_col_blocks,
                num_threads_,
                [&](int i) {
                  const int block_id = i - num_eliminate_blocks_;
                  int r, c, row_stride, col_stride;
                  CellInfo* cell_info = lhs->GetCell(
                      block_id, block_id, &r, &c, &row_stride, &col_stride);
                  if (cell_info != nullptr) {
                    const int block_size = bs->cols[i].size;
                    typename EigenTypes<Eigen::Dynamic>::ConstVectorRef diag(
                        D + bs->cols[i].position, block_size);
                    MatrixRef m(cell_info->values, row_stride, col_stride);
                    m.block(r, c, block_size, block_size).diagonal() +=
                        diag.array().square().matrix();
                  }
                });
  }

  // Eliminate y blocks one chunk at a time.  For each chunk, compute
  // the entries of the normal equations and the gradient vector block
  // corresponding to the y block and then apply Gaussian elimination
  // to them. The matrix ete stores the normal matrix corresponding to
  // the block being eliminated and array buffer_ contains the
  // non-zero blocks in the row corresponding to this y block in the
  // normal equations. This computation is done in
  // ChunkDiagonalBlockAndGradient. UpdateRhs then applies gaussian
  // elimination to the rhs of the normal equations, updating the rhs
  // of the reduced linear system by modifying rhs blocks for all the
  // z blocks that share a row block/residual term with the y
  // block. EliminateRowOuterProduct does the corresponding operation
  // for the lhs of the reduced linear system.
  ParallelFor(
      context_,
      0,
      int(chunks_.size()),
      num_threads_,
      [&](int thread_id, int i) {
        double* buffer = buffer_.get() + thread_id * buffer_size_;
        const Chunk& chunk = chunks_[i];
        const int e_block_id = bs->rows[chunk.start].cells.front().block_id;
        const int e_block_size = bs->cols[e_block_id].size;

        VectorRef(buffer, buffer_size_).setZero();

        typename EigenTypes<kEBlockSize, kEBlockSize>::Matrix ete(e_block_size,
                                                                  e_block_size);

        if (D != nullptr) {
          const typename EigenTypes<kEBlockSize>::ConstVectorRef diag(
              D + bs->cols[e_block_id].position, e_block_size);
          ete = diag.array().square().matrix().asDiagonal();
        } else {
          ete.setZero();
        }

        FixedArray<double, 8> g(e_block_size);
        typename EigenTypes<kEBlockSize>::VectorRef gref(g.data(),
                                                         e_block_size);
        gref.setZero();

        // We are going to be computing
        //
        //   S += F'F - F'E(E'E)^{-1}E'F
        //
        // for each Chunk. The computation is broken down into a number of
        // function calls as below.

        // Compute the outer product of the e_blocks with themselves (ete
        // = E'E). Compute the product of the e_blocks with the
        // corresponding f_blocks (buffer = E'F), the gradient of the terms
        // in this chunk (g) and add the outer product of the f_blocks to
        // Schur complement (S += F'F).
        ChunkDiagonalBlockAndGradient(
            chunk, A, b, chunk.start, &ete, g.data(), buffer, lhs);

        // Normally one wouldn't compute the inverse explicitly, but
        // e_block_size will typically be a small number like 3, in
        // which case its much faster to compute the inverse once and
        // use it to multiply other matrices/vectors instead of doing a
        // Solve call over and over again.
        typename EigenTypes<kEBlockSize, kEBlockSize>::Matrix inverse_ete =
            InvertPSDMatrix<kEBlockSize>(assume_full_rank_ete_, ete);

        // For the current chunk compute and update the rhs of the reduced
        // linear system.
        //
        //   rhs = F'b - F'E(E'E)^(-1) E'b

        if (rhs) {
          FixedArray<double, 8> inverse_ete_g(e_block_size);
          MatrixVectorMultiply<kEBlockSize, kEBlockSize, 0>(
              inverse_ete.data(),
              e_block_size,
              e_block_size,
              g.data(),
              inverse_ete_g.data());
          UpdateRhs(chunk, A, b, chunk.start, inverse_ete_g.data(), rhs);
        }

        // S -= F'E(E'E)^{-1}E'F
        ChunkOuterProduct(
            thread_id, bs, inverse_ete, buffer, chunk.buffer_layout, lhs);
      });

  // For rows with no e_blocks, the Schur complement update reduces to
  // S += F'F.
  NoEBlockRowsUpdate(A, b, uneliminated_row_begins_, lhs, rhs);
}

template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void SchurEliminator<kRowBlockSize, kEBlockSize, kFBlockSize>::BackSubstitute(
    const BlockSparseMatrixData& A,
    const double* b,
    const double* D,
    const double* z,
    double* y) {
  const CompressedRowBlockStructure* bs = A.block_structure();
  const double* values = A.values();

  ParallelFor(context_, 0, int(chunks_.size()), num_threads_, [&](int i) {
    const Chunk& chunk = chunks_[i];
    const int e_block_id = bs->rows[chunk.start].cells.front().block_id;
    const int e_block_size = bs->cols[e_block_id].size;

    double* y_ptr = y + bs->cols[e_block_id].position;
    typename EigenTypes<kEBlockSize>::VectorRef y_block(y_ptr, e_block_size);

    typename EigenTypes<kEBlockSize, kEBlockSize>::Matrix ete(e_block_size,
                                                              e_block_size);
    if (D != nullptr) {
      const typename EigenTypes<kEBlockSize>::ConstVectorRef diag(
          D + bs->cols[e_block_id].position, e_block_size);
      ete = diag.array().square().matrix().asDiagonal();
    } else {
      ete.setZero();
    }

    for (int j = 0; j < chunk.size; ++j) {
      const CompressedRow& row = bs->rows[chunk.start + j];
      const Cell& e_cell = row.cells.front();
      DCHECK_EQ(e_block_id, e_cell.block_id);

      FixedArray<double, 8> sj(row.block.size);

      typename EigenTypes<kRowBlockSize>::VectorRef(sj.data(), row.block.size) =
          typename EigenTypes<kRowBlockSize>::ConstVectorRef(
              b + bs->rows[chunk.start + j].block.position, row.block.size);

      for (int c = 1; c < row.cells.size(); ++c) {
        const int f_block_id = row.cells[c].block_id;
        const int f_block_size = bs->cols[f_block_id].size;
        const int r_block = f_block_id - num_eliminate_blocks_;

        // clang-format off
        MatrixVectorMultiply<kRowBlockSize, kFBlockSize, -1>(
            values + row.cells[c].position, row.block.size, f_block_size,
            z + lhs_row_layout_[r_block],
            sj.data());
      }

      MatrixTransposeVectorMultiply<kRowBlockSize, kEBlockSize, 1>(
          values + e_cell.position, row.block.size, e_block_size,
          sj.data(),
          y_ptr);

      MatrixTransposeMatrixMultiply
          <kRowBlockSize, kEBlockSize, kRowBlockSize, kEBlockSize, 1>(
          values + e_cell.position, row.block.size, e_block_size,
          values + e_cell.position, row.block.size, e_block_size,
          ete.data(), 0, 0, e_block_size, e_block_size);
      // clang-format on
    }

    y_block =
        InvertPSDMatrix<kEBlockSize>(assume_full_rank_ete_, ete) * y_block;
  });
}

// Update the rhs of the reduced linear system. Compute
//
//   F'b - F'E(E'E)^(-1) E'b
template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void SchurEliminator<kRowBlockSize, kEBlockSize, kFBlockSize>::UpdateRhs(
    const Chunk& chunk,
    const BlockSparseMatrixData& A,
    const double* b,
    int row_block_counter,
    const double* inverse_ete_g,
    double* rhs) {
  const CompressedRowBlockStructure* bs = A.block_structure();
  const double* values = A.values();

  const int e_block_id = bs->rows[chunk.start].cells.front().block_id;
  const int e_block_size = bs->cols[e_block_id].size;
  int b_pos = bs->rows[row_block_counter].block.position;
  for (int j = 0; j < chunk.size; ++j) {
    const CompressedRow& row = bs->rows[row_block_counter + j];
    const Cell& e_cell = row.cells.front();

    typename EigenTypes<kRowBlockSize>::Vector sj =
        typename EigenTypes<kRowBlockSize>::ConstVectorRef(b + b_pos,
                                                           row.block.size);

    // clang-format off
    MatrixVectorMultiply<kRowBlockSize, kEBlockSize, -1>(
        values + e_cell.position, row.block.size, e_block_size,
        inverse_ete_g, sj.data());
    // clang-format on

    for (int c = 1; c < row.cells.size(); ++c) {
      const int block_id = row.cells[c].block_id;
      const int block_size = bs->cols[block_id].size;
      const int block = block_id - num_eliminate_blocks_;
      auto lock = MakeConditionalLock(num_threads_, *rhs_locks_[block]);
      // clang-format off
      MatrixTransposeVectorMultiply<kRowBlockSize, kFBlockSize, 1>(
          values + row.cells[c].position,
          row.block.size, block_size,
          sj.data(), rhs + lhs_row_layout_[block]);
      // clang-format on
    }
    b_pos += row.block.size;
  }
}

// Given a Chunk - set of rows with the same e_block, e.g. in the
// following Chunk with two rows.
//
//                E                   F
//      [ y11   0   0   0 |  z11     0    0   0    z51]
//      [ y12   0   0   0 |  z12   z22    0   0      0]
//
// this function computes twp matrices. The diagonal block matrix
//
//   ete = y11 * y11' + y12 * y12'
//
// and the off diagonal blocks in the Gauss Newton Hessian.
//
//   buffer = [y11'(z11 + z12), y12' * z22, y11' * z51]
//
// which are zero compressed versions of the block sparse matrices E'E
// and E'F.
//
// and the gradient of the e_block, E'b.
template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void SchurEliminator<kRowBlockSize, kEBlockSize, kFBlockSize>::
    ChunkDiagonalBlockAndGradient(
        const Chunk& chunk,
        const BlockSparseMatrixData& A,
        const double* b,
        int row_block_counter,
        typename EigenTypes<kEBlockSize, kEBlockSize>::Matrix* ete,
        double* g,
        double* buffer,
        BlockRandomAccessMatrix* lhs) {
  const CompressedRowBlockStructure* bs = A.block_structure();
  const double* values = A.values();

  int b_pos = bs->rows[row_block_counter].block.position;
  const int e_block_size = ete->rows();

  // Iterate over the rows in this chunk, for each row, compute the
  // contribution of its F blocks to the Schur complement, the
  // contribution of its E block to the matrix EE' (ete), and the
  // corresponding block in the gradient vector.
  for (int j = 0; j < chunk.size; ++j) {
    const CompressedRow& row = bs->rows[row_block_counter + j];

    if (row.cells.size() > 1) {
      EBlockRowOuterProduct(A, row_block_counter + j, lhs);
    }

    // Extract the e_block, ETE += E_i' E_i
    const Cell& e_cell = row.cells.front();
    // clang-format off
    MatrixTransposeMatrixMultiply
        <kRowBlockSize, kEBlockSize, kRowBlockSize, kEBlockSize, 1>(
            values + e_cell.position, row.block.size, e_block_size,
            values + e_cell.position, row.block.size, e_block_size,
            ete->data(), 0, 0, e_block_size, e_block_size);
    // clang-format on

    if (b) {
      // g += E_i' b_i
      // clang-format off
      MatrixTransposeVectorMultiply<kRowBlockSize, kEBlockSize, 1>(
          values + e_cell.position, row.block.size, e_block_size,
          b + b_pos,
          g);
      // clang-format on
    }

    // buffer = E'F. This computation is done by iterating over the
    // f_blocks for each row in the chunk.
    for (int c = 1; c < row.cells.size(); ++c) {
      const int f_block_id = row.cells[c].block_id;
      const int f_block_size = bs->cols[f_block_id].size;
      double* buffer_ptr = buffer + FindOrDie(chunk.buffer_layout, f_block_id);
      // clang-format off
      MatrixTransposeMatrixMultiply
          <kRowBlockSize, kEBlockSize, kRowBlockSize, kFBlockSize, 1>(
          values + e_cell.position, row.block.size, e_block_size,
          values + row.cells[c].position, row.block.size, f_block_size,
          buffer_ptr, 0, 0, e_block_size, f_block_size);
      // clang-format on
    }
    b_pos += row.block.size;
  }
}

// Compute the outer product F'E(E'E)^{-1}E'F and subtract it from the
// Schur complement matrix, i.e
//
//  S -= F'E(E'E)^{-1}E'F.
template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void SchurEliminator<kRowBlockSize, kEBlockSize, kFBlockSize>::
    ChunkOuterProduct(int thread_id,
                      const CompressedRowBlockStructure* bs,
                      const Matrix& inverse_ete,
                      const double* buffer,
                      const BufferLayoutType& buffer_layout,
                      BlockRandomAccessMatrix* lhs) {
  // This is the most computationally expensive part of this
  // code. Profiling experiments reveal that the bottleneck is not the
  // computation of the right-hand matrix product, but memory
  // references to the left hand side.
  const int e_block_size = inverse_ete.rows();
  auto it1 = buffer_layout.begin();

  double* b1_transpose_inverse_ete =
      chunk_outer_product_buffer_.get() + thread_id * buffer_size_;

  // S(i,j) -= bi' * ete^{-1} b_j
  for (; it1 != buffer_layout.end(); ++it1) {
    const int block1 = it1->first - num_eliminate_blocks_;
    const int block1_size = bs->cols[it1->first].size;
    // clang-format off
    MatrixTransposeMatrixMultiply
        <kEBlockSize, kFBlockSize, kEBlockSize, kEBlockSize, 0>(
        buffer + it1->second, e_block_size, block1_size,
        inverse_ete.data(), e_block_size, e_block_size,
        b1_transpose_inverse_ete, 0, 0, block1_size, e_block_size);
    // clang-format on

    auto it2 = it1;
    for (; it2 != buffer_layout.end(); ++it2) {
      const int block2 = it2->first - num_eliminate_blocks_;

      int r, c, row_stride, col_stride;
      CellInfo* cell_info =
          lhs->GetCell(block1, block2, &r, &c, &row_stride, &col_stride);
      if (cell_info != nullptr) {
        const int block2_size = bs->cols[it2->first].size;
        auto lock = MakeConditionalLock(num_threads_, cell_info->m);
        // clang-format off
        MatrixMatrixMultiply
            <kFBlockSize, kEBlockSize, kEBlockSize, kFBlockSize, -1>(
                b1_transpose_inverse_ete, block1_size, e_block_size,
                buffer  + it2->second, e_block_size, block2_size,
                cell_info->values, r, c, row_stride, col_stride);
        // clang-format on
      }
    }
  }
}

// For rows with no e_blocks, the Schur complement update reduces to S
// += F'F. This function iterates over the rows of A with no e_block,
// and calls NoEBlockRowOuterProduct on each row.
template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void SchurEliminator<kRowBlockSize, kEBlockSize, kFBlockSize>::
    NoEBlockRowsUpdate(const BlockSparseMatrixData& A,
                       const double* b,
                       int row_block_counter,
                       BlockRandomAccessMatrix* lhs,
                       double* rhs) {
  const CompressedRowBlockStructure* bs = A.block_structure();
  const double* values = A.values();
  for (; row_block_counter < bs->rows.size(); ++row_block_counter) {
    NoEBlockRowOuterProduct(A, row_block_counter, lhs);
    if (!rhs) {
      continue;
    }
    const CompressedRow& row = bs->rows[row_block_counter];
    for (int c = 0; c < row.cells.size(); ++c) {
      const int block_id = row.cells[c].block_id;
      const int block_size = bs->cols[block_id].size;
      const int block = block_id - num_eliminate_blocks_;
      // clang-format off
      MatrixTransposeVectorMultiply<Eigen::Dynamic, Eigen::Dynamic, 1>(
          values + row.cells[c].position, row.block.size, block_size,
          b + row.block.position,
          rhs + lhs_row_layout_[block]);
      // clang-format on
    }
  }
}

// A row r of A, which has no e_blocks gets added to the Schur
// complement as S += r r'. This function is responsible for computing
// the contribution of a single row r to the Schur complement. It is
// very similar in structure to EBlockRowOuterProduct except for
// one difference. It does not use any of the template
// parameters. This is because the algorithm used for detecting the
// static structure of the matrix A only pays attention to rows with
// e_blocks. This is because rows without e_blocks are rare and
// typically arise from regularization terms in the original
// optimization problem, and have a very different structure than the
// rows with e_blocks. Including them in the static structure
// detection will lead to most template parameters being set to
// dynamic. Since the number of rows without e_blocks is small, the
// lack of templating is not an issue.
template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void SchurEliminator<kRowBlockSize, kEBlockSize, kFBlockSize>::
    NoEBlockRowOuterProduct(const BlockSparseMatrixData& A,
                            int row_block_index,
                            BlockRandomAccessMatrix* lhs) {
  const CompressedRowBlockStructure* bs = A.block_structure();
  const double* values = A.values();

  const CompressedRow& row = bs->rows[row_block_index];
  for (int i = 0; i < row.cells.size(); ++i) {
    const int block1 = row.cells[i].block_id - num_eliminate_blocks_;
    DCHECK_GE(block1, 0);

    const int block1_size = bs->cols[row.cells[i].block_id].size;
    int r, c, row_stride, col_stride;
    CellInfo* cell_info =
        lhs->GetCell(block1, block1, &r, &c, &row_stride, &col_stride);
    if (cell_info != nullptr) {
      auto lock = MakeConditionalLock(num_threads_, cell_info->m);
      // This multiply currently ignores the fact that this is a
      // symmetric outer product.
      // clang-format off
      MatrixTransposeMatrixMultiply
          <Eigen::Dynamic, Eigen::Dynamic, Eigen::Dynamic, Eigen::Dynamic, 1>(
              values + row.cells[i].position, row.block.size, block1_size,
              values + row.cells[i].position, row.block.size, block1_size,
              cell_info->values, r, c, row_stride, col_stride);
      // clang-format on
    }

    for (int j = i + 1; j < row.cells.size(); ++j) {
      const int block2 = row.cells[j].block_id - num_eliminate_blocks_;
      DCHECK_GE(block2, 0);
      DCHECK_LT(block1, block2);
      int r, c, row_stride, col_stride;
      CellInfo* cell_info =
          lhs->GetCell(block1, block2, &r, &c, &row_stride, &col_stride);
      if (cell_info != nullptr) {
        const int block2_size = bs->cols[row.cells[j].block_id].size;
        auto lock = MakeConditionalLock(num_threads_, cell_info->m);
        // clang-format off
        MatrixTransposeMatrixMultiply
            <Eigen::Dynamic, Eigen::Dynamic, Eigen::Dynamic, Eigen::Dynamic, 1>(
                values + row.cells[i].position, row.block.size, block1_size,
                values + row.cells[j].position, row.block.size, block2_size,
                cell_info->values, r, c, row_stride, col_stride);
        // clang-format on
      }
    }
  }
}

// For a row with an e_block, compute the contribution S += F'F. This
// function has the same structure as NoEBlockRowOuterProduct, except
// that this function uses the template parameters.
template <int kRowBlockSize, int kEBlockSize, int kFBlockSize>
void SchurEliminator<kRowBlockSize, kEBlockSize, kFBlockSize>::
    EBlockRowOuterProduct(const BlockSparseMatrixData& A,
                          int row_block_index,
                          BlockRandomAccessMatrix* lhs) {
  const CompressedRowBlockStructure* bs = A.block_structure();
  const double* values = A.values();

  const CompressedRow& row = bs->rows[row_block_index];
  for (int i = 1; i < row.cells.size(); ++i) {
    const int block1 = row.cells[i].block_id - num_eliminate_blocks_;
    DCHECK_GE(block1, 0);

    const int block1_size = bs->cols[row.cells[i].block_id].size;
    int r, c, row_stride, col_stride;
    CellInfo* cell_info =
        lhs->GetCell(block1, block1, &r, &c, &row_stride, &col_stride);
    if (cell_info != nullptr) {
      auto lock = MakeConditionalLock(num_threads_, cell_info->m);
      // block += b1.transpose() * b1;
      // clang-format off
      MatrixTransposeMatrixMultiply
          <kRowBlockSize, kFBlockSize, kRowBlockSize, kFBlockSize, 1>(
          values + row.cells[i].position, row.block.size, block1_size,
          values + row.cells[i].position, row.block.size, block1_size,
          cell_info->values, r, c, row_stride, col_stride);
      // clang-format on
    }

    for (int j = i + 1; j < row.cells.size(); ++j) {
      const int block2 = row.cells[j].block_id - num_eliminate_blocks_;
      DCHECK_GE(block2, 0);
      DCHECK_LT(block1, block2);
      const int block2_size = bs->cols[row.cells[j].block_id].size;
      int r, c, row_stride, col_stride;
      CellInfo* cell_info =
          lhs->GetCell(block1, block2, &r, &c, &row_stride, &col_stride);
      if (cell_info != nullptr) {
        // block += b1.transpose() * b2;
        auto lock = MakeConditionalLock(num_threads_, cell_info->m);
        // clang-format off
        MatrixTransposeMatrixMultiply
            <kRowBlockSize, kFBlockSize, kRowBlockSize, kFBlockSize, 1>(
                values + row.cells[i].position, row.block.size, block1_size,
                values + row.cells[j].position, row.block.size, block2_size,
                cell_info->values, r, c, row_stride, col_stride);
        // clang-format on
      }
    }
  }
}

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_SCHUR_ELIMINATOR_IMPL_H_
