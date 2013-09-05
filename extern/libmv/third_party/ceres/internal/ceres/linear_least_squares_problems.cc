// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
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
#include <string>
#include <vector>
#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/casts.h"
#include "ceres/file.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/stringprintf.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

LinearLeastSquaresProblem* CreateLinearLeastSquaresProblemFromId(int id) {
  switch (id) {
    case 0:
      return LinearLeastSquaresProblem0();
    case 1:
      return LinearLeastSquaresProblem1();
    case 2:
      return LinearLeastSquaresProblem2();
    case 3:
      return LinearLeastSquaresProblem3();
    default:
      LOG(FATAL) << "Unknown problem id requested " << id;
  }
  return NULL;
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
LinearLeastSquaresProblem* LinearLeastSquaresProblem0() {
  LinearLeastSquaresProblem* problem = new LinearLeastSquaresProblem;

  TripletSparseMatrix* A = new TripletSparseMatrix(3, 2, 6);
  problem->b.reset(new double[3]);
  problem->D.reset(new double[2]);

  problem->x.reset(new double[2]);
  problem->x_D.reset(new double[2]);

  int* Ai = A->mutable_rows();
  int* Aj = A->mutable_cols();
  double* Ax = A->mutable_values();

  int counter = 0;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j< 2; ++j) {
      Ai[counter]=i;
      Aj[counter]=j;
      ++counter;
    }
  };

  Ax[0] = 1.;
  Ax[1] = 2.;
  Ax[2] = 3.;
  Ax[3] = 4.;
  Ax[4] = 6;
  Ax[5] = -10;
  A->set_num_nonzeros(6);
  problem->A.reset(A);

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

      S = [ 42.3419  -1.4000  -11.5806
            -1.4000   2.6000    1.0000
            11.5806   1.0000   31.1935]

      r = [ 4.3032
            5.4000
            5.0323]

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
LinearLeastSquaresProblem* LinearLeastSquaresProblem1() {
  int num_rows = 6;
  int num_cols = 5;

  LinearLeastSquaresProblem* problem = new LinearLeastSquaresProblem;
  TripletSparseMatrix* A = new TripletSparseMatrix(num_rows,
                                                   num_cols,
                                                   num_rows * num_cols);
  problem->b.reset(new double[num_rows]);
  problem->D.reset(new double[num_cols]);
  problem->num_eliminate_blocks = 2;

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

  problem->A.reset(A);

  for (int i = 0; i < num_cols; ++i) {
    problem->D.get()[i] = 1;
  }

  for (int i = 0; i < num_rows; ++i) {
    problem->b.get()[i] = i;
  }

  return problem;
}

// BlockSparseMatrix version
LinearLeastSquaresProblem* LinearLeastSquaresProblem2() {
  int num_rows = 6;
  int num_cols = 5;

  LinearLeastSquaresProblem* problem = new LinearLeastSquaresProblem;

  problem->b.reset(new double[num_rows]);
  problem->D.reset(new double[num_cols]);
  problem->num_eliminate_blocks = 2;

  CompressedRowBlockStructure* bs = new CompressedRowBlockStructure;
  scoped_array<double> values(new double[num_rows * num_cols]);

  for (int c = 0; c < num_cols; ++c) {
    bs->cols.push_back(Block());
    bs->cols.back().size = 1;
    bs->cols.back().position = c;
  }

  int nnz = 0;

  // Row 1
  {
    values[nnz++] = 1;
    values[nnz++] = 2;

    bs->rows.push_back(CompressedRow());
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 0;
    row.cells.push_back(Cell(0, 0));
    row.cells.push_back(Cell(2, 1));
  }

  // Row 2
  {
    values[nnz++] = 3;
    values[nnz++] = 4;

    bs->rows.push_back(CompressedRow());
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 1;
    row.cells.push_back(Cell(0, 2));
    row.cells.push_back(Cell(3, 3));
  }

  // Row 3
  {
    values[nnz++] = 5;
    values[nnz++] = 6;

    bs->rows.push_back(CompressedRow());
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 2;
    row.cells.push_back(Cell(1, 4));
    row.cells.push_back(Cell(4, 5));
  }

  // Row 4
  {
    values[nnz++] = 7;
    values[nnz++] = 8;

    bs->rows.push_back(CompressedRow());
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 3;
    row.cells.push_back(Cell(1, 6));
    row.cells.push_back(Cell(2, 7));
  }

  // Row 5
  {
    values[nnz++] = 9;
    values[nnz++] = 1;

    bs->rows.push_back(CompressedRow());
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 4;
    row.cells.push_back(Cell(1, 8));
    row.cells.push_back(Cell(2, 9));
  }

  // Row 6
  {
    values[nnz++] = 1;
    values[nnz++] = 1;
    values[nnz++] = 1;

    bs->rows.push_back(CompressedRow());
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 5;
    row.cells.push_back(Cell(2, 10));
    row.cells.push_back(Cell(3, 11));
    row.cells.push_back(Cell(4, 12));
  }

  BlockSparseMatrix* A = new BlockSparseMatrix(bs);
  memcpy(A->mutable_values(), values.get(), nnz * sizeof(*A->values()));

  for (int i = 0; i < num_cols; ++i) {
    problem->D.get()[i] = 1;
  }

  for (int i = 0; i < num_rows; ++i) {
    problem->b.get()[i] = i;
  }

  problem->A.reset(A);

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
LinearLeastSquaresProblem* LinearLeastSquaresProblem3() {
  int num_rows = 5;
  int num_cols = 2;

  LinearLeastSquaresProblem* problem = new LinearLeastSquaresProblem;

  problem->b.reset(new double[num_rows]);
  problem->D.reset(new double[num_cols]);
  problem->num_eliminate_blocks = 2;

  CompressedRowBlockStructure* bs = new CompressedRowBlockStructure;
  scoped_array<double> values(new double[num_rows * num_cols]);

  for (int c = 0; c < num_cols; ++c) {
    bs->cols.push_back(Block());
    bs->cols.back().size = 1;
    bs->cols.back().position = c;
  }

  int nnz = 0;

  // Row 1
  {
    values[nnz++] = 1;
    bs->rows.push_back(CompressedRow());
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 0;
    row.cells.push_back(Cell(0, 0));
  }

  // Row 2
  {
    values[nnz++] = 3;
    bs->rows.push_back(CompressedRow());
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 1;
    row.cells.push_back(Cell(0, 1));
  }

  // Row 3
  {
    values[nnz++] = 5;
    bs->rows.push_back(CompressedRow());
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 2;
    row.cells.push_back(Cell(1, 2));
  }

  // Row 4
  {
    values[nnz++] = 7;
    bs->rows.push_back(CompressedRow());
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 3;
    row.cells.push_back(Cell(1, 3));
  }

  // Row 5
  {
    values[nnz++] = 9;
    bs->rows.push_back(CompressedRow());
    CompressedRow& row = bs->rows.back();
    row.block.size = 1;
    row.block.position = 4;
    row.cells.push_back(Cell(1, 4));
  }

  BlockSparseMatrix* A = new BlockSparseMatrix(bs);
  memcpy(A->mutable_values(), values.get(), nnz * sizeof(*A->values()));

  for (int i = 0; i < num_cols; ++i) {
    problem->D.get()[i] = 1;
  }

  for (int i = 0; i < num_rows; ++i) {
    problem->b.get()[i] = i;
  }

  problem->A.reset(A);

  return problem;
}

namespace {
bool DumpLinearLeastSquaresProblemToConsole(const SparseMatrix* A,
                                            const double* D,
                                            const double* b,
                                            const double* x,
                                            int num_eliminate_blocks) {
  CHECK_NOTNULL(A);
  Matrix AA;
  A->ToDenseMatrix(&AA);
  LOG(INFO) << "A^T: \n" << AA.transpose();

  if (D != NULL) {
    LOG(INFO) << "A's appended diagonal:\n"
              << ConstVectorRef(D, A->num_cols());
  }

  if (b != NULL) {
    LOG(INFO) << "b: \n" << ConstVectorRef(b, A->num_rows());
  }

  if (x != NULL) {
    LOG(INFO) << "x: \n" << ConstVectorRef(x, A->num_cols());
  }
  return true;
};

void WriteArrayToFileOrDie(const string& filename,
                           const double* x,
                           const int size) {
  CHECK_NOTNULL(x);
  VLOG(2) << "Writing array to: " << filename;
  FILE* fptr = fopen(filename.c_str(), "w");
  CHECK_NOTNULL(fptr);
  for (int i = 0; i < size; ++i) {
    fprintf(fptr, "%17f\n", x[i]);
  }
  fclose(fptr);
}

bool DumpLinearLeastSquaresProblemToTextFile(const string& filename_base,
                                             const SparseMatrix* A,
                                             const double* D,
                                             const double* b,
                                             const double* x,
                                             int num_eliminate_blocks) {
  CHECK_NOTNULL(A);
  LOG(INFO) << "writing to: " << filename_base << "*";

  string matlab_script;
  StringAppendF(&matlab_script,
                "function lsqp = load_trust_region_problem()\n");
  StringAppendF(&matlab_script,
                "lsqp.num_rows = %d;\n", A->num_rows());
  StringAppendF(&matlab_script,
                "lsqp.num_cols = %d;\n", A->num_cols());

  {
    string filename = filename_base + "_A.txt";
    FILE* fptr = fopen(filename.c_str(), "w");
    CHECK_NOTNULL(fptr);
    A->ToTextFile(fptr);
    fclose(fptr);
    StringAppendF(&matlab_script,
                  "tmp = load('%s', '-ascii');\n", filename.c_str());
    StringAppendF(
        &matlab_script,
        "lsqp.A = sparse(tmp(:, 1) + 1, tmp(:, 2) + 1, tmp(:, 3), %d, %d);\n",
        A->num_rows(),
        A->num_cols());
  }


  if (D != NULL) {
    string filename = filename_base + "_D.txt";
    WriteArrayToFileOrDie(filename, D, A->num_cols());
    StringAppendF(&matlab_script,
                  "lsqp.D = load('%s', '-ascii');\n", filename.c_str());
  }

  if (b != NULL) {
    string filename = filename_base + "_b.txt";
    WriteArrayToFileOrDie(filename, b, A->num_rows());
    StringAppendF(&matlab_script,
                  "lsqp.b = load('%s', '-ascii');\n", filename.c_str());
  }

  if (x != NULL) {
    string filename = filename_base + "_x.txt";
    WriteArrayToFileOrDie(filename, x, A->num_cols());
    StringAppendF(&matlab_script,
                  "lsqp.x = load('%s', '-ascii');\n", filename.c_str());
  }

  string matlab_filename = filename_base + ".m";
  WriteStringToFileOrDie(matlab_script, matlab_filename);
  return true;
}
}  // namespace

bool DumpLinearLeastSquaresProblem(const string& filename_base,
                                   DumpFormatType dump_format_type,
                                   const SparseMatrix* A,
                                   const double* D,
                                   const double* b,
                                   const double* x,
                                   int num_eliminate_blocks) {
  switch (dump_format_type) {
    case CONSOLE:
      return DumpLinearLeastSquaresProblemToConsole(A, D, b, x,
                                                    num_eliminate_blocks);
    case TEXTFILE:
      return DumpLinearLeastSquaresProblemToTextFile(filename_base,
                                                     A, D, b, x,
                                                     num_eliminate_blocks);
    default:
      LOG(FATAL) << "Unknown DumpFormatType " << dump_format_type;
  };

  return true;
}

}  // namespace internal
}  // namespace ceres
