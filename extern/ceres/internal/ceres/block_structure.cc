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

#include "ceres/block_structure.h"

#include <vector>

#include "glog/logging.h"

namespace ceres::internal {

bool CellLessThan(const Cell& lhs, const Cell& rhs) {
  if (lhs.block_id == rhs.block_id) {
    return (lhs.position < rhs.position);
  }
  return (lhs.block_id < rhs.block_id);
}

std::vector<Block> Tail(const std::vector<Block>& blocks, int n) {
  CHECK_LE(n, blocks.size());
  std::vector<Block> tail;
  const int num_blocks = blocks.size();
  const int start = num_blocks - n;

  int position = 0;
  tail.reserve(n);
  for (int i = start; i < num_blocks; ++i) {
    tail.emplace_back(blocks[i].size, position);
    position += blocks[i].size;
  }

  return tail;
}

int SumSquaredSizes(const std::vector<Block>& blocks) {
  int sum = 0;
  for (const auto& b : blocks) {
    sum += b.size * b.size;
  }
  return sum;
}

}  // namespace ceres::internal
