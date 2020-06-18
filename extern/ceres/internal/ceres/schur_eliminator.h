// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2019 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_SCHUR_ELIMINATOR_H_
#define CERES_INTERNAL_SCHUR_ELIMINATOR_H_

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "Eigen/Dense"
#include "ceres/block_random_access_matrix.h"
#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"
#include "ceres/linear_solver.h"

namespace ceres {
namespace internal {

// Classes implementing the SchurEliminatorBase interface implement
// variable elimination for linear least squares problems. Assuming
// that the input linear system Ax = b can be partitioned into
//
//  E y + F z = b
//
// Where x = [y;z] is a partition of the variables.  The partitioning
// of the variables is such that, E'E is a block diagonal matrix. Or
// in other words, the parameter blocks in E form an independent set
// of the of the graph implied by the block matrix A'A. Then, this
// class provides the functionality to compute the Schur complement
// system
//
//   S z = r
//
// where
//
//   S = F'F - F'E (E'E)^{-1} E'F and r = F'b - F'E(E'E)^(-1) E'b
//
// This is the Eliminate operation, i.e., construct the linear system
// obtained by eliminating the variables in E.
//
// The eliminator also provides the reverse functionality, i.e. given
// values for z it can back substitute for the values of y, by solving the
// linear system
//
//  Ey = b - F z
//
// which is done by observing that
//
//  y = (E'E)^(-1) [E'b - E'F z]
//
// The eliminator has a number of requirements.
//
// The rows of A are ordered so that for every variable block in y,
// all the rows containing that variable block occur as a vertically
// contiguous block. i.e the matrix A looks like
//
//              E                 F                   chunk
//  A = [ y1   0   0   0 |  z1    0    0   0    z5]     1
//      [ y1   0   0   0 |  z1   z2    0   0     0]     1
//      [  0  y2   0   0 |   0    0   z3   0     0]     2
//      [  0   0  y3   0 |  z1   z2   z3  z4    z5]     3
//      [  0   0  y3   0 |  z1    0    0   0    z5]     3
//      [  0   0   0  y4 |   0    0    0   0    z5]     4
//      [  0   0   0  y4 |   0   z2    0   0     0]     4
//      [  0   0   0  y4 |   0    0    0   0     0]     4
//      [  0   0   0   0 |  z1    0    0   0     0] non chunk blocks
//      [  0   0   0   0 |   0    0   z3  z4    z5] non chunk blocks
//
// This structure should be reflected in the corresponding
// CompressedRowBlockStructure object associated with A. The linear
// system Ax = b should either be well posed or the array D below
// should be non-null and the diagonal matrix corresponding to it
// should be non-singular. For simplicity of exposition only the case
// with a null D is described.
//
// The usual way to do the elimination is as follows. Starting with
//
//  E y + F z = b
//
// we can form the normal equations,
//
//  E'E y + E'F z = E'b
//  F'E y + F'F z = F'b
//
// multiplying both sides of the first equation by (E'E)^(-1) and then
// by F'E we get
//
//  F'E y + F'E (E'E)^(-1) E'F z =  F'E (E'E)^(-1) E'b
//  F'E y +                F'F z =  F'b
//
// now subtracting the two equations we get
//
// [FF' - F'E (E'E)^(-1) E'F] z = F'b - F'E(E'E)^(-1) E'b
//
// Instead of forming the normal equations and operating on them as
// general sparse matrices, the algorithm here deals with one
// parameter block in y at a time. The rows corresponding to a single
// parameter block yi are known as a chunk, and the algorithm operates
// on one chunk at a time. The mathematics remains the same since the
// reduced linear system can be shown to be the sum of the reduced
// linear systems for each chunk. This can be seen by observing two
// things.
//
//  1. E'E is a block diagonal matrix.
//
//  2. When E'F is computed, only the terms within a single chunk
//  interact, i.e for y1 column blocks when transposed and multiplied
//  with F, the only non-zero contribution comes from the blocks in
//  chunk1.
//
// Thus, the reduced linear system
//
//  FF' - F'E (E'E)^(-1) E'F
//
// can be re-written as
//
//  sum_k F_k F_k' - F_k'E_k (E_k'E_k)^(-1) E_k' F_k
//
// Where the sum is over chunks and E_k'E_k is dense matrix of size y1
// x y1.
//
// Advanced usage. Until now it has been assumed that the user would
// be interested in all of the Schur Complement S. However, it is also
// possible to use this eliminator to obtain an arbitrary submatrix of
// the full Schur complement. When the eliminator is generating the
// blocks of S, it asks the RandomAccessBlockMatrix instance passed to
// it if it has storage for that block. If it does, the eliminator
// computes/updates it, if not it is skipped. This is useful when one
// is interested in constructing a preconditioner based on the Schur
// Complement, e.g., computing the block diagonal of S so that it can
// be used as a preconditioner for an Iterative Substructuring based
// solver [See Agarwal et al, Bundle Adjustment in the Large, ECCV
// 2008 for an example of such use].
//
// Example usage: Please see schur_complement_solver.cc
class SchurEliminatorBase {
 public:
  virtual ~SchurEliminatorBase() {}

  // Initialize the eliminator. It is the user's responsibilty to call
  // this function before calling Eliminate or BackSubstitute. It is
  // also the caller's responsibilty to ensure that the
  // CompressedRowBlockStructure object passed to this method is the
  // same one (or is equivalent to) the one associated with the
  // BlockSparseMatrix objects below.
  //
  // assume_full_rank_ete controls how the eliminator inverts with the
  // diagonal blocks corresponding to e blocks in A'A. If
  // assume_full_rank_ete is true, then a Cholesky factorization is
  // used to compute the inverse, otherwise a singular value
  // decomposition is used to compute the pseudo inverse.
  virtual void Init(int num_eliminate_blocks,
                    bool assume_full_rank_ete,
                    const CompressedRowBlockStructure* bs) = 0;

  // Compute the Schur complement system from the augmented linear
  // least squares problem [A;D] x = [b;0]. The left hand side and the
  // right hand side of the reduced linear system are returned in lhs
  // and rhs respectively.
  //
  // It is the caller's responsibility to construct and initialize
  // lhs. Depending upon the structure of the lhs object passed here,
  // the full or a submatrix of the Schur complement will be computed.
  //
  // Since the Schur complement is a symmetric matrix, only the upper
  // triangular part of the Schur complement is computed.
  virtual void Eliminate(const BlockSparseMatrixData& A,
                         const double* b,
                         const double* D,
                         BlockRandomAccessMatrix* lhs,
                         double* rhs) = 0;

  // Given values for the variables z in the F block of A, solve for
  // the optimal values of the variables y corresponding to the E
  // block in A.
  virtual void BackSubstitute(const BlockSparseMatrixData& A,
                              const double* b,
                              const double* D,
                              const double* z,
                              double* y) = 0;
  // Factory
  static SchurEliminatorBase* Create(const LinearSolver::Options& options);
};

// Templated implementation of the SchurEliminatorBase interface. The
// templating is on the sizes of the row, e and f blocks sizes in the
// input matrix. In many problems, the sizes of one or more of these
// blocks are constant, in that case, its worth passing these
// parameters as template arguments so that they are visible to the
// compiler and can be used for compile time optimization of the low
// level linear algebra routines.
template <int kRowBlockSize = Eigen::Dynamic,
          int kEBlockSize = Eigen::Dynamic,
          int kFBlockSize = Eigen::Dynamic>
class SchurEliminator : public SchurEliminatorBase {
 public:
  explicit SchurEliminator(const LinearSolver::Options& options)
      : num_threads_(options.num_threads), context_(options.context) {
    CHECK(context_ != nullptr);
  }

  // SchurEliminatorBase Interface
  virtual ~SchurEliminator();
  void Init(int num_eliminate_blocks,
            bool assume_full_rank_ete,
            const CompressedRowBlockStructure* bs) final;
  void Eliminate(const BlockSparseMatrixData& A,
                 const double* b,
                 const double* D,
                 BlockRandomAccessMatrix* lhs,
                 double* rhs) final;
  void BackSubstitute(const BlockSparseMatrixData& A,
                      const double* b,
                      const double* D,
                      const double* z,
                      double* y) final;

 private:
  // Chunk objects store combinatorial information needed to
  // efficiently eliminate a whole chunk out of the least squares
  // problem. Consider the first chunk in the example matrix above.
  //
  //      [ y1   0   0   0 |  z1    0    0   0    z5]
  //      [ y1   0   0   0 |  z1   z2    0   0     0]
  //
  // One of the intermediate quantities that needs to be calculated is
  // for each row the product of the y block transposed with the
  // non-zero z block, and the sum of these blocks across rows. A
  // temporary array "buffer_" is used for computing and storing them
  // and the buffer_layout maps the indices of the z-blocks to
  // position in the buffer_ array.  The size of the chunk is the
  // number of row blocks/residual blocks for the particular y block
  // being considered.
  //
  // For the example chunk shown above,
  //
  // size = 2
  //
  // The entries of buffer_layout will be filled in the following order.
  //
  // buffer_layout[z1] = 0
  // buffer_layout[z5] = y1 * z1
  // buffer_layout[z2] = y1 * z1 + y1 * z5
  typedef std::map<int, int> BufferLayoutType;
  struct Chunk {
    Chunk() : size(0) {}
    int size;
    int start;
    BufferLayoutType buffer_layout;
  };

  void ChunkDiagonalBlockAndGradient(
      const Chunk& chunk,
      const BlockSparseMatrixData& A,
      const double* b,
      int row_block_counter,
      typename EigenTypes<kEBlockSize, kEBlockSize>::Matrix* eet,
      double* g,
      double* buffer,
      BlockRandomAccessMatrix* lhs);

  void UpdateRhs(const Chunk& chunk,
                 const BlockSparseMatrixData& A,
                 const double* b,
                 int row_block_counter,
                 const double* inverse_ete_g,
                 double* rhs);

  void ChunkOuterProduct(int thread_id,
                         const CompressedRowBlockStructure* bs,
                         const Matrix& inverse_eet,
                         const double* buffer,
                         const BufferLayoutType& buffer_layout,
                         BlockRandomAccessMatrix* lhs);
  void EBlockRowOuterProduct(const BlockSparseMatrixData& A,
                             int row_block_index,
                             BlockRandomAccessMatrix* lhs);

  void NoEBlockRowsUpdate(const BlockSparseMatrixData& A,
                          const double* b,
                          int row_block_counter,
                          BlockRandomAccessMatrix* lhs,
                          double* rhs);

  void NoEBlockRowOuterProduct(const BlockSparseMatrixData& A,
                               int row_block_index,
                               BlockRandomAccessMatrix* lhs);

  int num_threads_;
  ContextImpl* context_;
  int num_eliminate_blocks_;
  bool assume_full_rank_ete_;

  // Block layout of the columns of the reduced linear system. Since
  // the f blocks can be of varying size, this vector stores the
  // position of each f block in the row/col of the reduced linear
  // system. Thus lhs_row_layout_[i] is the row/col position of the
  // i^th f block.
  std::vector<int> lhs_row_layout_;

  // Combinatorial structure of the chunks in A. For more information
  // see the documentation of the Chunk object above.
  std::vector<Chunk> chunks_;

  // TODO(sameeragarwal): The following two arrays contain per-thread
  // storage. They should be refactored into a per thread struct.

  // Buffer to store the products of the y and z blocks generated
  // during the elimination phase. buffer_ is of size num_threads *
  // buffer_size_. Each thread accesses the chunk
  //
  //   [thread_id * buffer_size_ , (thread_id + 1) * buffer_size_]
  //
  std::unique_ptr<double[]> buffer_;

  // Buffer to store per thread matrix matrix products used by
  // ChunkOuterProduct. Like buffer_ it is of size num_threads *
  // buffer_size_. Each thread accesses the chunk
  //
  //   [thread_id * buffer_size_ , (thread_id + 1) * buffer_size_ -1]
  //
  std::unique_ptr<double[]> chunk_outer_product_buffer_;

  int buffer_size_;
  int uneliminated_row_begins_;

  // Locks for the blocks in the right hand side of the reduced linear
  // system.
  std::vector<std::mutex*> rhs_locks_;
};

// SchurEliminatorForOneFBlock specializes the SchurEliminatorBase interface for
// the case where there is exactly one f-block and all three parameters
// kRowBlockSize, kEBlockSize and KFBlockSize are known at compile time. This is
// the case for some two view bundle adjustment problems which have very
// stringent latency requirements.
//
// Under these assumptions, we can simplify the more general algorithm
// implemented by SchurEliminatorImpl significantly. Two of the major
// contributors to the increased performance are:
//
// 1. Simpler loop structure and less use of dynamic memory. Almost everything
//    is on the stack and benefits from aligned memory as well as fixed sized
//    vectorization. We are also able to reason about temporaries and control
//    their lifetimes better.
// 2. Use of inverse() over llt().solve(Identity).
template <int kRowBlockSize = Eigen::Dynamic,
          int kEBlockSize = Eigen::Dynamic,
          int kFBlockSize = Eigen::Dynamic>
class SchurEliminatorForOneFBlock : public SchurEliminatorBase {
 public:
  virtual ~SchurEliminatorForOneFBlock() {}
  void Init(int num_eliminate_blocks,
            bool assume_full_rank_ete,
            const CompressedRowBlockStructure* bs) override {
    CHECK_GT(num_eliminate_blocks, 0)
        << "SchurComplementSolver cannot be initialized with "
        << "num_eliminate_blocks = 0.";
    CHECK_EQ(bs->cols.size() - num_eliminate_blocks, 1);

    num_eliminate_blocks_ = num_eliminate_blocks;
    const int num_row_blocks = bs->rows.size();
    chunks_.clear();
    int r = 0;
    // Iterate over the row blocks of A, and detect the chunks. The
    // matrix should already have been ordered so that all rows
    // containing the same y block are vertically contiguous.
    while (r < num_row_blocks) {
      const int e_block_id = bs->rows[r].cells.front().block_id;
      if (e_block_id >= num_eliminate_blocks_) {
        break;
      }

      chunks_.push_back(Chunk());
      Chunk& chunk = chunks_.back();
      chunk.num_rows = 0;
      chunk.start = r;
      // Add to the chunk until the first block in the row is
      // different than the one in the first row for the chunk.
      while (r + chunk.num_rows < num_row_blocks) {
        const CompressedRow& row = bs->rows[r + chunk.num_rows];
        if (row.cells.front().block_id != e_block_id) {
          break;
        }
        ++chunk.num_rows;
      }
      r += chunk.num_rows;
    }

    const Chunk& last_chunk = chunks_.back();
    uneliminated_row_begins_ = last_chunk.start + last_chunk.num_rows;
    e_t_e_inverse_matrices_.resize(kEBlockSize * kEBlockSize * chunks_.size());
    std::fill(
        e_t_e_inverse_matrices_.begin(), e_t_e_inverse_matrices_.end(), 0.0);
  }

  void Eliminate(const BlockSparseMatrixData& A,
                 const double* b,
                 const double* D,
                 BlockRandomAccessMatrix* lhs_bram,
                 double* rhs_ptr) override {
    // Since there is only one f-block, we can call GetCell once, and cache its
    // output.
    int r, c, row_stride, col_stride;
    CellInfo* cell_info =
        lhs_bram->GetCell(0, 0, &r, &c, &row_stride, &col_stride);
    typename EigenTypes<kFBlockSize, kFBlockSize>::MatrixRef lhs(
        cell_info->values, kFBlockSize, kFBlockSize);
    typename EigenTypes<kFBlockSize>::VectorRef rhs(rhs_ptr, kFBlockSize);

    lhs.setZero();
    rhs.setZero();

    const CompressedRowBlockStructure* bs = A.block_structure();
    const double* values = A.values();

    // Add the diagonal to the schur complement.
    if (D != nullptr) {
      typename EigenTypes<kFBlockSize>::ConstVectorRef diag(
          D + bs->cols[num_eliminate_blocks_].position, kFBlockSize);
      lhs.diagonal() = diag.array().square().matrix();
    }

    Eigen::Matrix<double, kEBlockSize, kFBlockSize> tmp;
    Eigen::Matrix<double, kEBlockSize, 1> tmp2;

    // The following loop works on a block matrix which looks as follows
    // (number of rows can be anything):
    //
    // [e_1 | f_1] = [b1]
    // [e_2 | f_2] = [b2]
    // [e_3 | f_3] = [b3]
    // [e_4 | f_4] = [b4]
    //
    // and computes the following
    //
    // e_t_e = sum_i e_i^T * e_i
    // e_t_e_inverse = inverse(e_t_e)
    // e_t_f = sum_i e_i^T f_i
    // e_t_b = sum_i e_i^T b_i
    // f_t_b = sum_i f_i^T b_i
    //
    // lhs += sum_i f_i^T * f_i - e_t_f^T * e_t_e_inverse * e_t_f
    // rhs += f_t_b - e_t_f^T * e_t_e_inverse * e_t_b
    for (int i = 0; i < chunks_.size(); ++i) {
      const Chunk& chunk = chunks_[i];
      const int e_block_id = bs->rows[chunk.start].cells.front().block_id;

      // Naming covention, e_t_e = e_block.transpose() * e_block;
      Eigen::Matrix<double, kEBlockSize, kEBlockSize> e_t_e;
      Eigen::Matrix<double, kEBlockSize, kFBlockSize> e_t_f;
      Eigen::Matrix<double, kEBlockSize, 1> e_t_b;
      Eigen::Matrix<double, kFBlockSize, 1> f_t_b;

      // Add the square of the diagonal to e_t_e.
      if (D != NULL) {
        const typename EigenTypes<kEBlockSize>::ConstVectorRef diag(
            D + bs->cols[e_block_id].position, kEBlockSize);
        e_t_e = diag.array().square().matrix().asDiagonal();
      } else {
        e_t_e.setZero();
      }
      e_t_f.setZero();
      e_t_b.setZero();
      f_t_b.setZero();

      for (int j = 0; j < chunk.num_rows; ++j) {
        const int row_id = chunk.start + j;
        const auto& row = bs->rows[row_id];
        const typename EigenTypes<kRowBlockSize, kEBlockSize>::ConstMatrixRef
            e_block(values + row.cells[0].position, kRowBlockSize, kEBlockSize);
        const typename EigenTypes<kRowBlockSize>::ConstVectorRef b_block(
            b + row.block.position, kRowBlockSize);

        e_t_e.noalias() += e_block.transpose() * e_block;
        e_t_b.noalias() += e_block.transpose() * b_block;

        if (row.cells.size() == 1) {
          // There is no f block, so there is nothing more to do.
          continue;
        }

        const typename EigenTypes<kRowBlockSize, kFBlockSize>::ConstMatrixRef
            f_block(values + row.cells[1].position, kRowBlockSize, kFBlockSize);
        e_t_f.noalias() += e_block.transpose() * f_block;
        lhs.noalias() += f_block.transpose() * f_block;
        f_t_b.noalias() += f_block.transpose() * b_block;
      }

      // BackSubstitute computes the same inverse, and this is the key workload
      // there, so caching these inverses makes BackSubstitute essentially free.
      typename EigenTypes<kEBlockSize, kEBlockSize>::MatrixRef e_t_e_inverse(
          &e_t_e_inverse_matrices_[kEBlockSize * kEBlockSize * i],
          kEBlockSize,
          kEBlockSize);

      // e_t_e is a symmetric positive definite matrix, so the standard way to
      // compute its inverse is via the Cholesky factorization by calling
      // e_t_e.llt().solve(Identity()). However, the inverse() method even
      // though it is not optimized for symmetric matrices is significantly
      // faster for small fixed size (up to 4x4) matrices.
      //
      // https://eigen.tuxfamily.org/dox/group__TutorialLinearAlgebra.html#title3
      e_t_e_inverse.noalias() = e_t_e.inverse();

      // The use of these two pre-allocated tmp vectors saves temporaries in the
      // expressions for lhs and rhs updates below and has a significant impact
      // on the performance of this method.
      tmp.noalias() = e_t_e_inverse * e_t_f;
      tmp2.noalias() = e_t_e_inverse * e_t_b;

      lhs.noalias() -= e_t_f.transpose() * tmp;
      rhs.noalias() += f_t_b - e_t_f.transpose() * tmp2;
    }

    // The rows without any e-blocks can have arbitrary size but only contain
    // the f-block.
    //
    // lhs += f_i^T f_i
    // rhs += f_i^T b_i
    for (int row_id = uneliminated_row_begins_; row_id < bs->rows.size();
         ++row_id) {
      const auto& row = bs->rows[row_id];
      const auto& cell = row.cells[0];
      const typename EigenTypes<Eigen::Dynamic, kFBlockSize>::ConstMatrixRef
          f_block(values + cell.position, row.block.size, kFBlockSize);
      const typename EigenTypes<Eigen::Dynamic>::ConstVectorRef b_block(
          b + row.block.position, row.block.size);
      lhs.noalias() += f_block.transpose() * f_block;
      rhs.noalias() += f_block.transpose() * b_block;
    }
  }

  // This implementation of BackSubstitute depends on Eliminate being called
  // before this. SchurComplementSolver always does this.
  //
  // y_i = e_t_e_inverse * sum_i e_i^T * (b_i - f_i * z);
  void BackSubstitute(const BlockSparseMatrixData& A,
                      const double* b,
                      const double* D,
                      const double* z_ptr,
                      double* y) override {
    typename EigenTypes<kFBlockSize>::ConstVectorRef z(z_ptr, kFBlockSize);
    const CompressedRowBlockStructure* bs = A.block_structure();
    const double* values = A.values();
    Eigen::Matrix<double, kEBlockSize, 1> tmp;
    for (int i = 0; i < chunks_.size(); ++i) {
      const Chunk& chunk = chunks_[i];
      const int e_block_id = bs->rows[chunk.start].cells.front().block_id;
      tmp.setZero();
      for (int j = 0; j < chunk.num_rows; ++j) {
        const int row_id = chunk.start + j;
        const auto& row = bs->rows[row_id];
        const typename EigenTypes<kRowBlockSize, kEBlockSize>::ConstMatrixRef
            e_block(values + row.cells[0].position, kRowBlockSize, kEBlockSize);
        const typename EigenTypes<kRowBlockSize>::ConstVectorRef b_block(
            b + row.block.position, kRowBlockSize);

        if (row.cells.size() == 1) {
          // There is no f block.
          tmp += e_block.transpose() * b_block;
        } else {
          typename EigenTypes<kRowBlockSize, kFBlockSize>::ConstMatrixRef
              f_block(
                  values + row.cells[1].position, kRowBlockSize, kFBlockSize);
          tmp += e_block.transpose() * (b_block - f_block * z);
        }
      }

      typename EigenTypes<kEBlockSize, kEBlockSize>::MatrixRef e_t_e_inverse(
          &e_t_e_inverse_matrices_[kEBlockSize * kEBlockSize * i],
          kEBlockSize,
          kEBlockSize);

      typename EigenTypes<kEBlockSize>::VectorRef y_block(
          y + bs->cols[e_block_id].position, kEBlockSize);
      y_block.noalias() = e_t_e_inverse * tmp;
    }
  }

 private:
  struct Chunk {
    int start = 0;
    int num_rows = 0;
  };

  std::vector<Chunk> chunks_;
  int num_eliminate_blocks_;
  int uneliminated_row_begins_;
  std::vector<double> e_t_e_inverse_matrices_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_SCHUR_ELIMINATOR_H_
