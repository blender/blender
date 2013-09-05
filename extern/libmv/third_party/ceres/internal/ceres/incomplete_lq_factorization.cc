// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2013 Google Inc. All rights reserved.
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

#include "ceres/incomplete_lq_factorization.h"

#include <vector>
#include <utility>
#include <cmath>
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

// Normalize a row and return it's norm.
inline double NormalizeRow(const int row, CompressedRowSparseMatrix* matrix) {
  const int row_begin =  matrix->rows()[row];
  const int row_end = matrix->rows()[row + 1];

  double* values = matrix->mutable_values();
  double norm = 0.0;
  for (int i =  row_begin; i < row_end; ++i) {
    norm += values[i] * values[i];
  }

  norm = sqrt(norm);
  const double inverse_norm = 1.0 / norm;
  for (int i = row_begin; i < row_end; ++i) {
    values[i] *= inverse_norm;
  }

  return norm;
}

// Compute a(row_a,:) * b(row_b, :)'
inline double RowDotProduct(const CompressedRowSparseMatrix& a,
                            const int row_a,
                            const CompressedRowSparseMatrix& b,
                            const int row_b) {
  const int* a_rows = a.rows();
  const int* a_cols = a.cols();
  const double* a_values = a.values();

  const int* b_rows = b.rows();
  const int* b_cols = b.cols();
  const double* b_values = b.values();

  const int row_a_end = a_rows[row_a + 1];
  const int row_b_end = b_rows[row_b + 1];

  int idx_a = a_rows[row_a];
  int idx_b = b_rows[row_b];
  double dot_product = 0.0;
  while (idx_a < row_a_end && idx_b < row_b_end) {
    if (a_cols[idx_a] == b_cols[idx_b]) {
      dot_product += a_values[idx_a++] * b_values[idx_b++];
    }

    while (a_cols[idx_a] < b_cols[idx_b] && idx_a < row_a_end) {
      ++idx_a;
    }

    while (a_cols[idx_a] > b_cols[idx_b] && idx_b < row_b_end) {
      ++idx_b;
    }
  }

  return dot_product;
}

struct SecondGreaterThan {
 public:
  bool operator()(const pair<int, double>& lhs,
                  const pair<int, double>& rhs) const {
    return (fabs(lhs.second) > fabs(rhs.second));
  }
};

// In the row vector dense_row(0:num_cols), drop values smaller than
// the max_value * drop_tolerance. Of the remaining non-zero values,
// choose at most level_of_fill values and then add the resulting row
// vector to matrix.

void DropEntriesAndAddRow(const Vector& dense_row,
                          const int num_entries,
                          const int level_of_fill,
                          const double drop_tolerance,
                          vector<pair<int, double> >* scratch,
                          CompressedRowSparseMatrix* matrix) {
  int* rows = matrix->mutable_rows();
  int* cols = matrix->mutable_cols();
  double* values = matrix->mutable_values();
  int num_nonzeros = rows[matrix->num_rows()];

  if (num_entries == 0) {
    matrix->set_num_rows(matrix->num_rows() + 1);
    rows[matrix->num_rows()] = num_nonzeros;
    return;
  }

  const double max_value = dense_row.head(num_entries).cwiseAbs().maxCoeff();
  const double threshold = drop_tolerance * max_value;

  int scratch_count = 0;
  for (int i = 0; i < num_entries; ++i) {
    if (fabs(dense_row[i]) > threshold) {
      pair<int, double>& entry = (*scratch)[scratch_count];
      entry.first = i;
      entry.second = dense_row[i];
      ++scratch_count;
    }
  }

  if (scratch_count > level_of_fill) {
    nth_element(scratch->begin(),
                scratch->begin() + level_of_fill,
                scratch->begin() + scratch_count,
                SecondGreaterThan());
    scratch_count = level_of_fill;
    sort(scratch->begin(), scratch->begin() + scratch_count);
  }

  for (int i = 0; i < scratch_count; ++i) {
    const pair<int, double>& entry = (*scratch)[i];
    cols[num_nonzeros] = entry.first;
    values[num_nonzeros] = entry.second;
    ++num_nonzeros;
  }

  matrix->set_num_rows(matrix->num_rows() + 1);
  rows[matrix->num_rows()] = num_nonzeros;
}

// Saad's Incomplete LQ factorization algorithm.
CompressedRowSparseMatrix* IncompleteLQFactorization(
    const CompressedRowSparseMatrix& matrix,
    const int l_level_of_fill,
    const double l_drop_tolerance,
    const int q_level_of_fill,
    const double q_drop_tolerance) {
  const int num_rows = matrix.num_rows();
  const int num_cols = matrix.num_cols();
  const int* rows = matrix.rows();
  const int* cols = matrix.cols();
  const double* values = matrix.values();

  CompressedRowSparseMatrix* l =
      new CompressedRowSparseMatrix(num_rows,
                                    num_rows,
                                    l_level_of_fill * num_rows);
  l->set_num_rows(0);

  CompressedRowSparseMatrix q(num_rows, num_cols, q_level_of_fill * num_rows);
  q.set_num_rows(0);

  int* l_rows = l->mutable_rows();
  int* l_cols = l->mutable_cols();
  double* l_values = l->mutable_values();

  int* q_rows = q.mutable_rows();
  int* q_cols = q.mutable_cols();
  double* q_values = q.mutable_values();

  Vector l_i(num_rows);
  Vector q_i(num_cols);
  vector<pair<int, double> > scratch(num_cols);
  for (int i = 0; i < num_rows; ++i) {
    // l_i = q * matrix(i,:)');
    l_i.setZero();
    for (int j = 0; j < i; ++j) {
      l_i(j) = RowDotProduct(matrix, i, q, j);
    }
    DropEntriesAndAddRow(l_i,
                         i,
                         l_level_of_fill,
                         l_drop_tolerance,
                         &scratch,
                         l);

    // q_i = matrix(i,:) - q(0:i-1,:) * l_i);
    q_i.setZero();
    for (int idx = rows[i]; idx < rows[i + 1]; ++idx) {
      q_i(cols[idx]) = values[idx];
    }

    for (int j = l_rows[i]; j < l_rows[i + 1]; ++j) {
      const int r = l_cols[j];
      const double lij = l_values[j];
      for (int idx = q_rows[r]; idx < q_rows[r + 1]; ++idx) {
        q_i(q_cols[idx]) -= lij * q_values[idx];
      }
    }
    DropEntriesAndAddRow(q_i,
                         num_cols,
                         q_level_of_fill,
                         q_drop_tolerance,
                         &scratch,
                         &q);

    // lii = |qi|
    l_cols[l->num_nonzeros()] = i;
    l_values[l->num_nonzeros()] = NormalizeRow(i, &q);
    l_rows[l->num_rows()] += 1;
  }

  return l;
}

}  // namespace internal
}  // namespace ceres
