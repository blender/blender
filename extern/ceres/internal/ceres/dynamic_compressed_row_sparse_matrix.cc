// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
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
// Author: richie.stebbing@gmail.com (Richard Stebbing)

#include <cstring>
#include "ceres/dynamic_compressed_row_sparse_matrix.h"

namespace ceres {
namespace internal {

DynamicCompressedRowSparseMatrix::DynamicCompressedRowSparseMatrix(
  int num_rows,
  int num_cols,
  int initial_max_num_nonzeros)
    : CompressedRowSparseMatrix(num_rows,
                                num_cols,
                                initial_max_num_nonzeros) {
    dynamic_cols_.resize(num_rows);
    dynamic_values_.resize(num_rows);
  }

void DynamicCompressedRowSparseMatrix::InsertEntry(int row,
                                                   int col,
                                                   const double& value) {
  CHECK_GE(row, 0);
  CHECK_LT(row, num_rows());
  CHECK_GE(col, 0);
  CHECK_LT(col, num_cols());
  dynamic_cols_[row].push_back(col);
  dynamic_values_[row].push_back(value);
}

void DynamicCompressedRowSparseMatrix::ClearRows(int row_start,
                                                 int num_rows) {
  for (int r = 0; r < num_rows; ++r) {
    const int i = row_start + r;
    CHECK_GE(i, 0);
    CHECK_LT(i, this->num_rows());
    dynamic_cols_[i].resize(0);
    dynamic_values_[i].resize(0);
  }
}

void DynamicCompressedRowSparseMatrix::Finalize(int num_additional_elements) {
  // `num_additional_elements` is provided as an argument so that additional
  // storage can be reserved when it is known by the finalizer.
  CHECK_GE(num_additional_elements, 0);

  // Count the number of non-zeros and resize `cols_` and `values_`.
  int num_jacobian_nonzeros = 0;
  for (int i = 0; i < dynamic_cols_.size(); ++i) {
    num_jacobian_nonzeros += dynamic_cols_[i].size();
  }

  SetMaxNumNonZeros(num_jacobian_nonzeros + num_additional_elements);

  // Flatten `dynamic_cols_` into `cols_` and `dynamic_values_`
  // into `values_`.
  int index_into_values_and_cols = 0;
  for (int i = 0; i < num_rows(); ++i) {
    mutable_rows()[i] = index_into_values_and_cols;
    const int num_nonzero_columns = dynamic_cols_[i].size();
    if (num_nonzero_columns > 0) {
      memcpy(mutable_cols() + index_into_values_and_cols,
             &dynamic_cols_[i][0],
             dynamic_cols_[i].size() * sizeof(dynamic_cols_[0][0]));
      memcpy(mutable_values() + index_into_values_and_cols,
             &dynamic_values_[i][0],
             dynamic_values_[i].size() * sizeof(dynamic_values_[0][0]));
      index_into_values_and_cols += dynamic_cols_[i].size();
    }
  }
  mutable_rows()[num_rows()] = index_into_values_and_cols;

  CHECK_EQ(index_into_values_and_cols, num_jacobian_nonzeros)
    << "Ceres bug: final index into values_ and cols_ should be equal to "
    << "the number of jacobian nonzeros. Please contact the developers!";
}

}  // namespace internal
}  // namespace ceres
