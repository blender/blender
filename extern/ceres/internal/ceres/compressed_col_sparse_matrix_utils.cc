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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#include "ceres/compressed_col_sparse_matrix_utils.h"

#include <vector>
#include <algorithm>
#include "ceres/internal/port.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

using std::vector;

void CompressedColumnScalarMatrixToBlockMatrix(
    const int* scalar_rows,
    const int* scalar_cols,
    const vector<int>& row_blocks,
    const vector<int>& col_blocks,
    vector<int>* block_rows,
    vector<int>* block_cols) {
  CHECK_NOTNULL(block_rows)->clear();
  CHECK_NOTNULL(block_cols)->clear();
  const int num_row_blocks = row_blocks.size();
  const int num_col_blocks = col_blocks.size();

  vector<int> row_block_starts(num_row_blocks);
  for (int i = 0, cursor = 0; i < num_row_blocks; ++i) {
    row_block_starts[i] = cursor;
    cursor += row_blocks[i];
  }

  // This loop extracts the block sparsity of the scalar sparse matrix
  // It does so by iterating over the columns, but only considering
  // the columns corresponding to the first element of each column
  // block. Within each column, the inner loop iterates over the rows,
  // and detects the presence of a row block by checking for the
  // presence of a non-zero entry corresponding to its first element.
  block_cols->push_back(0);
  int c = 0;
  for (int col_block = 0; col_block < num_col_blocks; ++col_block) {
    int column_size = 0;
    for (int idx = scalar_cols[c]; idx < scalar_cols[c + 1]; ++idx) {
      vector<int>::const_iterator it =
          std::lower_bound(row_block_starts.begin(),
                           row_block_starts.end(),
                           scalar_rows[idx]);
      // Since we are using lower_bound, it will return the row id
      // where the row block starts. For everything but the first row
      // of the block, where these values will be the same, we can
      // skip, as we only need the first row to detect the presence of
      // the block.
      //
      // For rows all but the first row in the last row block,
      // lower_bound will return row_block_starts.end(), but those can
      // be skipped like the rows in other row blocks too.
      if (it == row_block_starts.end() || *it != scalar_rows[idx]) {
        continue;
      }

      block_rows->push_back(it - row_block_starts.begin());
      ++column_size;
    }
    block_cols->push_back(block_cols->back() + column_size);
    c += col_blocks[col_block];
  }
}

void BlockOrderingToScalarOrdering(const vector<int>& blocks,
                                   const vector<int>& block_ordering,
                                   vector<int>* scalar_ordering) {
  CHECK_EQ(blocks.size(), block_ordering.size());
  const int num_blocks = blocks.size();

  // block_starts = [0, block1, block1 + block2 ..]
  vector<int> block_starts(num_blocks);
  for (int i = 0, cursor = 0; i < num_blocks ; ++i) {
    block_starts[i] = cursor;
    cursor += blocks[i];
  }

  scalar_ordering->resize(block_starts.back() + blocks.back());
  int cursor = 0;
  for (int i = 0; i < num_blocks; ++i) {
    const int block_id = block_ordering[i];
    const int block_size = blocks[block_id];
    int block_position = block_starts[block_id];
    for (int j = 0; j < block_size; ++j) {
      (*scalar_ordering)[cursor++] = block_position++;
    }
  }
}
}  // namespace internal
}  // namespace ceres
