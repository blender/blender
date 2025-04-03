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

#include "ceres/linear_least_squares_problems.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/casts.h"
#include "ceres/file.h"
#include "ceres/stringprintf.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres::internal {

std::unique_ptr<LinearLeastSquaresProblem>
CreateLinearLeastSquaresProblemFromId(int id) {
  switch (id) {
    case 0:
      return LinearLeastSquaresProblem0();
    case 1:
      return LinearLeastSquaresProblem1();
    case 2:
      return LinearLeastSquaresProblem2();
    case 3:
      return LinearLeastSquaresProblem3();
    case 4:
      return LinearLeastSquaresProblem4();
    case 5:
      return LinearLeastSquaresProblem5();
    case 6:
      return LinearLeastSquaresProblem6();
    default:
      LOG(FATAL) << "Unknown problem id requested " << id;
  }
  return nullptr;
}

/*
A = [1   2]
    [3   4]
    [6 -10]

b = [  8
      18
     -18]

x = [2
     3]

D = [1
     2]

x_D = [1.78448275;
       2.82327586;]
 */
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem0() {
  auto problem = std::make_unique<LinearLeastSquaresProblem>();

  auto A = std::make_unique<TripletSparseMatrix>(3, 2, 6);
  problem->b = std::make_unique<double[]>(3);
  problem->D = std::make_unique<double[]>(2);

  problem->x = std::make_unique<double[]>(2);
  problem->x_D = std::make_unique<double[]>(2);

  int* Ai = A->mutable_rows();
  int* Aj = A->mutable_cols();
  double* Ax = A->mutable_values();

  int counter = 0;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 2; ++j) {
      Ai[counter] = i;
      Aj[counter] = j;
      ++counter;
    }
  }

  Ax[0] = 1.;
  Ax[1] = 2.;
  Ax[2] = 3.;
  Ax[3] = 4.;
  Ax[4] = 6;
  Ax[5] = -10;
  A->set_num_nonzeros(6);
  problem->A = std::move(A);

  problem->b[0] = 8;
  problem->b[1] = 18;
  problem->b[2] = -18;

  problem->x[0] = 2.0;
  problem->x[1] = 3.0;

  problem->D[0] = 1;
  problem->D[1] = 2;

  problem->x_D[0] = 1.78448275;
  problem->x_D[1] = 2.82327586;
  return problem;
}

/*
      A = [1 0  | 2 0 0
           3 0  | 0 4 0
           0 5  | 0 0 6
           0 7  | 8 0 0
           0 9  | 1 0 0
           0 0  | 1 1 1]

      b = [0
           1
           2
           3
           4
           5]

      c = A'* b = [ 3
                   67
                   33
                    9
                   17]

      A'A = [10    0    2   12   0
              0  155   65    0  30
              2   65   70    1   1
             12    0    1   17   1
              0   30    1    1  37]

      cond(A'A) = 200.36

      S = [ 42.3419  -1.4000  -11.5806
            -1.4000   2.6000    1.0000
           -11.5806   1.0000   31.1935]

      r = [ 4.3032
            5.4000
            4.0323]

      S\r = [ 0.2102
              2.1367
              0.1388]

      A\b = [-2.3061
              0.3172
              0.2102
              2.1367
              0.1388]
*/
// The following two functions create a TripletSparseMatrix and a
// BlockSparseMatrix version of this problem.

// TripletSparseMatrix version.
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem1() {
  int num_rows = 6;
  int num_cols = 5;

  auto problem = std::make_unique<LinearLeastSquaresProblem>();

  auto A = std::make_unique<TripletSparseMatrix>(
      num_rows, num_cols, num_rows * num_cols);
  problem->b = std::make_unique<double[]>(num_rows);
  problem->D = std::make_unique<double[]>(num_cols);
  problem->num_eliminate_blocks = 2;

  problem->x = std::make_unique<double[]>(num_cols);
  problem->x[0] = -2.3061;
  problem->x[1] = 0.3172;
  problem->x[2] = 0.2102;
  problem->x[3] = 2.1367;
  problem->x[4] = 0.1388;

  int* rows = A->mutable_rows();
  int* cols = A->mutable_cols();
  double* values = A->mutable_values();

  int nnz = 0;

  // Row 1
  {
    rows[nnz] = 0;
    cols[nnz] = 0;
    values[nnz++] = 1;

    rows[nnz] = 0;
    cols[nnz] = 2;
    values[nnz++] = 2;
  }

  // Row 2
  {
    rows[nnz] = 1;
    cols[nnz] = 0;
    values[nnz++] = 3;

    rows[nnz] = 1;
    cols[nnz] = 3;
    values[nnz++] = 4;
  }

  // Row 3
  {
    rows[nnz] = 2;
    cols[nnz] = 1;
    values[nnz++] = 5;

    rows[nnz] = 2;
    cols[nnz] = 4;
    values[nnz++] = 6;
  }

  // Row 4
  {
    rows[nnz] = 3;
    cols[nnz] = 1;
    values[nnz++] = 7;

    rows[nnz] = 3;
    cols[nnz] = 2;
    values[nnz++] = 8;
  }

  // Row 5
  {
    rows[nnz] = 4;
    cols[nnz] = 1;
    values[nnz++] = 9;

    rows[nnz] = 4;
    cols[nnz] = 2;
    values[nnz++] = 1;
  }

  // Row 6
  {
    rows[nnz] = 5;
    cols[nnz] = 2;
    values[nnz++] = 1;

    rows[nnz] = 5;
    cols[nnz] = 3;
    values[nnz++] = 1;

    rows[nnz] = 5;
    cols[nnz] = 4;
    values[nnz++] = 1;
  }

  A->set_num_nonzeros(nnz);
  CHECK(A->IsValid());

  problem->A = std::move(A);

  for (int i = 0; i < num_cols; ++i) {
    problem->D.get()[i] = 1;
  }

  for (int i = 0; i < num_rows; ++i) {
    problem->b.get()[i] = i;
  }

  return problem;
}

// BlockSparseMatrix version
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem2() {
  int num_rows = 6;
  int num_cols = 5;

  auto problem = std::make_unique<LinearLeastSquaresProblem>();

  problem->b = std::make_unique<double[]>(num_rows);
  problem->D = std::make_unique<double[]>(num_cols);
  problem->num_eliminate_blocks = 2;

  problem->x = std::make_unique<double[]>(num_cols);
  problem->x[0] = -2.3061;
  problem->x[1] = 0.3172;
  problem->x[2] = 0.2102;
  problem->x[3] = 2.1367;
  problem->x[4] = 0.1388;

  auto* bs = new CompressedRowBlockStructure;
  auto values = std::make_unique<double[]>(num_rows * num_cols);

  for (int c = 0; c < num_cols; ++c) {
    bs->cols.emplace_back();
    bs->cols.back().size = 1;
    bs->cols.back().position = c;
  }

  int nnz = 0;

  // Row 1
  {
    values[nnz++] = 1;
    values[nnz++] = 2;

    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 0;
    row.cells.emplace_back(0, 0);
    row.cells.emplace_back(2, 1);
  }

  // Row 2
  {
    values[nnz++] = 3;
    values[nnz++] = 4;

    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 1;
    row.cells.emplace_back(0, 2);
    row.cells.emplace_back(3, 3);
  }

  // Row 3
  {
    values[nnz++] = 5;
    values[nnz++] = 6;

    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 2;
    row.cells.emplace_back(1, 4);
    row.cells.emplace_back(4, 5);
  }

  // Row 4
  {
    values[nnz++] = 7;
    values[nnz++] = 8;

    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 3;
    row.cells.emplace_back(1, 6);
    row.cells.emplace_back(2, 7);
  }

  // Row 5
  {
    values[nnz++] = 9;
    values[nnz++] = 1;

    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 4;
    row.cells.emplace_back(1, 8);
    row.cells.emplace_back(2, 9);
  }

  // Row 6
  {
    values[nnz++] = 1;
    values[nnz++] = 1;
    values[nnz++] = 1;

    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 5;
    row.cells.emplace_back(2, 10);
    row.cells.emplace_back(3, 11);
    row.cells.emplace_back(4, 12);
  }

  auto A = std::make_unique<BlockSparseMatrix>(bs);
  memcpy(A->mutable_values(), values.get(), nnz * sizeof(*A->values()));

  for (int i = 0; i < num_cols; ++i) {
    problem->D.get()[i] = 1;
  }

  for (int i = 0; i < num_rows; ++i) {
    problem->b.get()[i] = i;
  }

  problem->A = std::move(A);

  return problem;
}

/*
      A = [1 0
           3 0
           0 5
           0 7
           0 9
           0 0]

      b = [0
           1
           2
           3
           4
           5]
*/
// BlockSparseMatrix version
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem3() {
  int num_rows = 5;
  int num_cols = 2;

  auto problem = std::make_unique<LinearLeastSquaresProblem>();

  problem->b = std::make_unique<double[]>(num_rows);
  problem->D = std::make_unique<double[]>(num_cols);
  problem->num_eliminate_blocks = 2;

  auto* bs = new CompressedRowBlockStructure;
  auto values = std::make_unique<double[]>(num_rows * num_cols);

  for (int c = 0; c < num_cols; ++c) {
    bs->cols.emplace_back();
    bs->cols.back().size = 1;
    bs->cols.back().position = c;
  }

  int nnz = 0;

  // Row 1
  {
    values[nnz++] = 1;
    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 0;
    row.cells.emplace_back(0, 0);
  }

  // Row 2
  {
    values[nnz++] = 3;
    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 1;
    row.cells.emplace_back(0, 1);
  }

  // Row 3
  {
    values[nnz++] = 5;
    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 2;
    row.cells.emplace_back(1, 2);
  }

  // Row 4
  {
    values[nnz++] = 7;
    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 3;
    row.cells.emplace_back(1, 3);
  }

  // Row 5
  {
    values[nnz++] = 9;
    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 4;
    row.cells.emplace_back(1, 4);
  }

  auto A = std::make_unique<BlockSparseMatrix>(bs);
  memcpy(A->mutable_values(), values.get(), nnz * sizeof(*A->values()));

  for (int i = 0; i < num_cols; ++i) {
    problem->D.get()[i] = 1;
  }

  for (int i = 0; i < num_rows; ++i) {
    problem->b.get()[i] = i;
  }

  problem->A = std::move(A);

  return problem;
}

/*
      A = [1 2 0 0 0 1 1
           1 4 0 0 0 5 6
           0 0 9 0 0 3 1]

      b = [0
           1
           2]
*/
// BlockSparseMatrix version
//
// This problem has the unique property that it has two different
// sized f-blocks, but only one of them occurs in the rows involving
// the one e-block. So performing Schur elimination on this problem
// tests the Schur Eliminator's ability to handle non-e-block rows
// correctly when their structure does not conform to the static
// structure determined by DetectStructure.
//
// NOTE: This problem is too small and rank deficient to be solved without
// the diagonal regularization.
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem4() {
  int num_rows = 3;
  int num_cols = 7;

  auto problem = std::make_unique<LinearLeastSquaresProblem>();

  problem->b = std::make_unique<double[]>(num_rows);
  problem->D = std::make_unique<double[]>(num_cols);
  problem->num_eliminate_blocks = 1;

  auto* bs = new CompressedRowBlockStructure;
  auto values = std::make_unique<double[]>(num_rows * num_cols);

  // Column block structure
  bs->cols.emplace_back();
  bs->cols.back().size = 2;
  bs->cols.back().position = 0;

  bs->cols.emplace_back();
  bs->cols.back().size = 3;
  bs->cols.back().position = 2;

  bs->cols.emplace_back();
  bs->cols.back().size = 2;
  bs->cols.back().position = 5;

  int nnz = 0;

  // Row 1 & 2
  {
    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 2;
    row.block.position = 0;

    row.cells.emplace_back(0, nnz);
    values[nnz++] = 1;
    values[nnz++] = 2;
    values[nnz++] = 1;
    values[nnz++] = 4;

    row.cells.emplace_back(2, nnz);
    values[nnz++] = 1;
    values[nnz++] = 1;
    values[nnz++] = 5;
    values[nnz++] = 6;
  }

  // Row 3
  {
    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 2;

    row.cells.emplace_back(1, nnz);
    values[nnz++] = 9;
    values[nnz++] = 0;
    values[nnz++] = 0;

    row.cells.emplace_back(2, nnz);
    values[nnz++] = 3;
    values[nnz++] = 1;
  }

  auto A = std::make_unique<BlockSparseMatrix>(bs);
  memcpy(A->mutable_values(), values.get(), nnz * sizeof(*A->values()));

  for (int i = 0; i < num_cols; ++i) {
    problem->D.get()[i] = (i + 1) * 100;
  }

  for (int i = 0; i < num_rows; ++i) {
    problem->b.get()[i] = i;
  }

  problem->A = std::move(A);
  return problem;
}

/*
A problem with block-diagonal F'F.

      A = [1  0 | 0 0 2
           3  0 | 0 0 4
           0 -1 | 0 1 0
           0 -3 | 0 1 0
           0 -1 | 3 0 0
           0 -2 | 1 0 0]

      b = [0
           1
           2
           3
           4
           5]

      c = A'* b = [ 22
                   -25
                    17
                     7
                     4]

      A'A = [10    0    0    0   10
              0   15   -5   -4    0
              0   -5   10    0    0
              0   -4    0    2    0
             10    0    0    0   20]

      cond(A'A) = 41.402

      S = [ 8.3333   -1.3333         0
           -1.3333    0.9333         0
                 0         0   10.0000]

      r = [ 8.6667
           -1.6667
            1.0000]

      S\r = [  0.9778
              -0.3889
               0.1000]

      A\b = [  0.2
              -1.4444
               0.9777
              -0.3888
               0.1]
*/

std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem5() {
  int num_rows = 6;
  int num_cols = 5;

  auto problem = std::make_unique<LinearLeastSquaresProblem>();
  problem->b = std::make_unique<double[]>(num_rows);
  problem->D = std::make_unique<double[]>(num_cols);
  problem->num_eliminate_blocks = 2;

  // TODO: add x
  problem->x = std::make_unique<double[]>(num_cols);
  problem->x[0] = 0.2;
  problem->x[1] = -1.4444;
  problem->x[2] = 0.9777;
  problem->x[3] = -0.3888;
  problem->x[4] = 0.1;

  auto* bs = new CompressedRowBlockStructure;
  auto values = std::make_unique<double[]>(num_rows * num_cols);

  for (int c = 0; c < num_cols; ++c) {
    bs->cols.emplace_back();
    bs->cols.back().size = 1;
    bs->cols.back().position = c;
  }

  int nnz = 0;

  // Row 1
  {
    values[nnz++] = -1;
    values[nnz++] = 2;

    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 0;
    row.cells.emplace_back(0, 0);
    row.cells.emplace_back(4, 1);
  }

  // Row 2
  {
    values[nnz++] = 3;
    values[nnz++] = 4;

    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 1;
    row.cells.emplace_back(0, 2);
    row.cells.emplace_back(4, 3);
  }

  // Row 3
  {
    values[nnz++] = -1;
    values[nnz++] = 1;

    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 2;
    row.cells.emplace_back(1, 4);
    row.cells.emplace_back(3, 5);
  }

  // Row 4
  {
    values[nnz++] = -3;
    values[nnz++] = 1;

    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 3;
    row.cells.emplace_back(1, 6);
    row.cells.emplace_back(3, 7);
  }

  // Row 5
  {
    values[nnz++] = -1;
    values[nnz++] = 3;

    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 4;
    row.cells.emplace_back(1, 8);
    row.cells.emplace_back(2, 9);
  }

  // Row 6
  {
    // values[nnz++] = 2;
    values[nnz++] = -2;
    values[nnz++] = 1;

    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 5;
    // row.cells.emplace_back(0, 10);
    row.cells.emplace_back(1, 10);
    row.cells.emplace_back(2, 11);
  }

  auto A = std::make_unique<BlockSparseMatrix>(bs);
  memcpy(A->mutable_values(), values.get(), nnz * sizeof(*A->values()));

  for (int i = 0; i < num_cols; ++i) {
    problem->D.get()[i] = 1;
  }

  for (int i = 0; i < num_rows; ++i) {
    problem->b.get()[i] = i;
  }

  problem->A = std::move(A);

  return problem;
}

/*
      A = [1 2 0 0 0 1 1
           1 4 0 0 0 5 6
           3 4 0 0 0 7 8
           5 6 0 0 0 9 0
           0 0 9 0 0 3 1]

      b = [0
           1
           2
           3
           4]
*/
// BlockSparseMatrix version
//
// This problem has the unique property that it has two different
// sized f-blocks, but only one of them occurs in the rows involving
// the one e-block. So performing Schur elimination on this problem
// tests the Schur Eliminator's ability to handle non-e-block rows
// correctly when their structure does not conform to the static
// structure determined by DetectStructure.
//
// Additionally, this problem has the first row of the last row block of E being
// larger than number of row blocks in E
//
// NOTE: This problem is too small and rank deficient to be solved without
// the diagonal regularization.
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem6() {
  int num_rows = 5;
  int num_cols = 7;

  auto problem = std::make_unique<LinearLeastSquaresProblem>();

  problem->b = std::make_unique<double[]>(num_rows);
  problem->D = std::make_unique<double[]>(num_cols);
  problem->num_eliminate_blocks = 1;

  auto* bs = new CompressedRowBlockStructure;
  auto values = std::make_unique<double[]>(num_rows * num_cols);

  // Column block structure
  bs->cols.emplace_back();
  bs->cols.back().size = 2;
  bs->cols.back().position = 0;

  bs->cols.emplace_back();
  bs->cols.back().size = 3;
  bs->cols.back().position = 2;

  bs->cols.emplace_back();
  bs->cols.back().size = 2;
  bs->cols.back().position = 5;

  int nnz = 0;

  // Row 1 & 2
  {
    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 2;
    row.block.position = 0;

    row.cells.emplace_back(0, nnz);
    values[nnz++] = 1;
    values[nnz++] = 2;
    values[nnz++] = 1;
    values[nnz++] = 4;

    row.cells.emplace_back(2, nnz);
    values[nnz++] = 1;
    values[nnz++] = 1;
    values[nnz++] = 5;
    values[nnz++] = 6;
  }

  // Row 3 & 4
  {
    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 2;
    row.block.position = 2;

    row.cells.emplace_back(0, nnz);
    values[nnz++] = 3;
    values[nnz++] = 4;
    values[nnz++] = 5;
    values[nnz++] = 6;

    row.cells.emplace_back(2, nnz);
    values[nnz++] = 7;
    values[nnz++] = 8;
    values[nnz++] = 9;
    values[nnz++] = 0;
  }

  // Row 5
  {
    bs->rows.emplace_back();
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 4;

    row.cells.emplace_back(1, nnz);
    values[nnz++] = 9;
    values[nnz++] = 0;
    values[nnz++] = 0;

    row.cells.emplace_back(2, nnz);
    values[nnz++] = 3;
    values[nnz++] = 1;
  }

  auto A = std::make_unique<BlockSparseMatrix>(bs);
  memcpy(A->mutable_values(), values.get(), nnz * sizeof(*A->values()));

  for (int i = 0; i < num_cols; ++i) {
    problem->D.get()[i] = (i + 1) * 100;
  }

  for (int i = 0; i < num_rows; ++i) {
    problem->b.get()[i] = i;
  }

  problem->A = std::move(A);
  return problem;
}

namespace {
bool DumpLinearLeastSquaresProblemToConsole(const SparseMatrix* A,
                                            const double* D,
                                            const double* b,
                                            const double* x,
                                            int /*num_eliminate_blocks*/) {
  CHECK(A != nullptr);
  Matrix AA;
  A->ToDenseMatrix(&AA);
  LOG(INFO) << "A^T: \n" << AA.transpose();

  if (D != nullptr) {
    LOG(INFO) << "A's appended diagonal:\n" << ConstVectorRef(D, A->num_cols());
  }

  if (b != nullptr) {
    LOG(INFO) << "b: \n" << ConstVectorRef(b, A->num_rows());
  }

  if (x != nullptr) {
    LOG(INFO) << "x: \n" << ConstVectorRef(x, A->num_cols());
  }
  return true;
}

void WriteArrayToFileOrDie(const std::string& filename,
                           const double* x,
                           const int size) {
  CHECK(x != nullptr);
  VLOG(2) << "Writing array to: " << filename;
  FILE* fptr = fopen(filename.c_str(), "w");
  CHECK(fptr != nullptr);
  for (int i = 0; i < size; ++i) {
    fprintf(fptr, "%17f\n", x[i]);
  }
  fclose(fptr);
}

bool DumpLinearLeastSquaresProblemToTextFile(const std::string& filename_base,
                                             const SparseMatrix* A,
                                             const double* D,
                                             const double* b,
                                             const double* x,
                                             int /*num_eliminate_blocks*/) {
  CHECK(A != nullptr);
  LOG(INFO) << "writing to: " << filename_base << "*";

  std::string matlab_script;
  StringAppendF(&matlab_script,
                "function lsqp = load_trust_region_problem()\n");
  StringAppendF(&matlab_script, "lsqp.num_rows = %d;\n", A->num_rows());
  StringAppendF(&matlab_script, "lsqp.num_cols = %d;\n", A->num_cols());

  {
    std::string filename = filename_base + "_A.txt";
    FILE* fptr = fopen(filename.c_str(), "w");
    CHECK(fptr != nullptr);
    A->ToTextFile(fptr);
    fclose(fptr);
    StringAppendF(
        &matlab_script, "tmp = load('%s', '-ascii');\n", filename.c_str());
    StringAppendF(
        &matlab_script,
        "lsqp.A = sparse(tmp(:, 1) + 1, tmp(:, 2) + 1, tmp(:, 3), %d, %d);\n",
        A->num_rows(),
        A->num_cols());
  }

  if (D != nullptr) {
    std::string filename = filename_base + "_D.txt";
    WriteArrayToFileOrDie(filename, D, A->num_cols());
    StringAppendF(
        &matlab_script, "lsqp.D = load('%s', '-ascii');\n", filename.c_str());
  }

  if (b != nullptr) {
    std::string filename = filename_base + "_b.txt";
    WriteArrayToFileOrDie(filename, b, A->num_rows());
    StringAppendF(
        &matlab_script, "lsqp.b = load('%s', '-ascii');\n", filename.c_str());
  }

  if (x != nullptr) {
    std::string filename = filename_base + "_x.txt";
    WriteArrayToFileOrDie(filename, x, A->num_cols());
    StringAppendF(
        &matlab_script, "lsqp.x = load('%s', '-ascii');\n", filename.c_str());
  }

  std::string matlab_filename = filename_base + ".m";
  WriteStringToFileOrDie(matlab_script, matlab_filename);
  return true;
}
}  // namespace

bool DumpLinearLeastSquaresProblem(const std::string& filename_base,
                                   DumpFormatType dump_format_type,
                                   const SparseMatrix* A,
                                   const double* D,
                                   const double* b,
                                   const double* x,
                                   int num_eliminate_blocks) {
  switch (dump_format_type) {
    case CONSOLE:
      return DumpLinearLeastSquaresProblemToConsole(
          A, D, b, x, num_eliminate_blocks);
    case TEXTFILE:
      return DumpLinearLeastSquaresProblemToTextFile(
          filename_base, A, D, b, x, num_eliminate_blocks);
    default:
      LOG(FATAL) << "Unknown DumpFormatType " << dump_format_type;
  }

  return true;
}

}  // namespace ceres::internal
