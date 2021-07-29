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

#include "ceres/detect_structure.h"
#include "ceres/internal/eigen.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

void DetectStructure(const CompressedRowBlockStructure& bs,
                     const int num_eliminate_blocks,
                     int* row_block_size,
                     int* e_block_size,
                     int* f_block_size) {
  const int num_row_blocks = bs.rows.size();
  *row_block_size = 0;
  *e_block_size = 0;
  *f_block_size = 0;

  // Iterate over row blocks of the matrix, checking if row_block,
  // e_block or f_block sizes remain constant.
  for (int r = 0; r < num_row_blocks; ++r) {
    const CompressedRow& row = bs.rows[r];
    // We do not care about the sizes of the blocks in rows which do
    // not contain e_blocks.
    if (row.cells.front().block_id >= num_eliminate_blocks) {
      break;
    }

    // Detect fixed or dynamic row block size.
    if (*row_block_size == 0) {
      *row_block_size = row.block.size;
    } else if (*row_block_size != Eigen::Dynamic &&
               *row_block_size != row.block.size) {
      VLOG(2) << "Dynamic row block size because the block size changed from "
              << *row_block_size << " to "
              << row.block.size;
      *row_block_size = Eigen::Dynamic;
    }

    // Detect fixed or dynamic e-block size.
    const int e_block_id = row.cells.front().block_id;
    if (*e_block_size == 0) {
      *e_block_size = bs.cols[e_block_id].size;
    } else if (*e_block_size != Eigen::Dynamic &&
               *e_block_size != bs.cols[e_block_id].size) {
      VLOG(2) << "Dynamic e block size because the block size changed from "
              << *e_block_size << " to "
              << bs.cols[e_block_id].size;
      *e_block_size = Eigen::Dynamic;
    }

    // Detect fixed or dynamic f-block size. We are only interested in
    // rows with e-blocks, and the e-block is always the first block,
    // so only rows of size greater than 1 are of interest.
    if (row.cells.size() > 1) {
      if (*f_block_size == 0) {
        const int f_block_id = row.cells[1].block_id;
        *f_block_size = bs.cols[f_block_id].size;
      }

      for (int c = 1;
           (c < row.cells.size()) && (*f_block_size != Eigen::Dynamic);
           ++c) {
        const int f_block_id = row.cells[c].block_id;
        if (*f_block_size != bs.cols[f_block_id].size) {
          VLOG(2) << "Dynamic f block size because the block size "
                  << "changed from " << *f_block_size << " to "
                  << bs.cols[f_block_id].size;
          *f_block_size = Eigen::Dynamic;
        }
      }
    }

    const bool is_everything_dynamic = (*row_block_size == Eigen::Dynamic &&
                                        *e_block_size == Eigen::Dynamic &&
                                        *f_block_size == Eigen::Dynamic);
    if (is_everything_dynamic) {
      break;
    }
  }

  CHECK_NE(*row_block_size, 0) << "No rows found";
  CHECK_NE(*e_block_size, 0) << "No e type blocks found";
  VLOG(1) << "Schur complement static structure <"
          << *row_block_size << ","
          << *e_block_size << ","
          << *f_block_size << ">.";
}

}  // namespace internal
}  // namespace ceres
