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
// Author: kushalav@google.com (Avanish Kushal)

#include "ceres/visibility.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ceres/block_structure.h"
#include "ceres/graph.h"
#include "ceres/pair_hash.h"
#include "glog/logging.h"

namespace ceres::internal {

void ComputeVisibility(const CompressedRowBlockStructure& block_structure,
                       const int num_eliminate_blocks,
                       std::vector<std::set<int>>* visibility) {
  CHECK(visibility != nullptr);

  // Clear the visibility vector and resize it to hold a
  // vector for each camera.
  visibility->resize(0);
  visibility->resize(block_structure.cols.size() - num_eliminate_blocks);

  for (const auto& row : block_structure.rows) {
    const std::vector<Cell>& cells = row.cells;
    int block_id = cells[0].block_id;
    // If the first block is not an e_block, then skip this row block.
    if (block_id >= num_eliminate_blocks) {
      continue;
    }

    for (int j = 1; j < cells.size(); ++j) {
      int camera_block_id = cells[j].block_id - num_eliminate_blocks;
      DCHECK_GE(camera_block_id, 0);
      DCHECK_LT(camera_block_id, visibility->size());
      (*visibility)[camera_block_id].insert(block_id);
    }
  }
}

std::unique_ptr<WeightedGraph<int>> CreateSchurComplementGraph(
    const std::vector<std::set<int>>& visibility) {
  const time_t start_time = time(nullptr);
  // Compute the number of e_blocks/point blocks. Since the visibility
  // set for each e_block/camera contains the set of e_blocks/points
  // visible to it, we find the maximum across all visibility sets.
  int num_points = 0;
  for (const auto& visible : visibility) {
    if (!visible.empty()) {
      num_points = std::max(num_points, (*visible.rbegin()) + 1);
    }
  }

  // Invert the visibility. The input is a camera->point mapping,
  // which tells us which points are visible in which
  // cameras. However, to compute the sparsity structure of the Schur
  // Complement efficiently, its better to have the point->camera
  // mapping.
  std::vector<std::set<int>> inverse_visibility(num_points);
  for (int i = 0; i < visibility.size(); i++) {
    const std::set<int>& visibility_set = visibility[i];
    for (int v : visibility_set) {
      inverse_visibility[v].insert(i);
    }
  }

  // Map from camera pairs to number of points visible to both cameras
  // in the pair.
  std::unordered_map<std::pair<int, int>, int, pair_hash> camera_pairs;

  // Count the number of points visible to each camera/f_block pair.
  for (const auto& inverse_visibility_set : inverse_visibility) {
    for (auto camera1 = inverse_visibility_set.begin();
         camera1 != inverse_visibility_set.end();
         ++camera1) {
      auto camera2 = camera1;
      for (++camera2; camera2 != inverse_visibility_set.end(); ++camera2) {
        ++(camera_pairs[std::make_pair(*camera1, *camera2)]);
      }
    }
  }

  auto graph = std::make_unique<WeightedGraph<int>>();

  // Add vertices and initialize the pairs for self edges so that self
  // edges are guaranteed. This is needed for the Canonical views
  // algorithm to work correctly.
  static constexpr double kSelfEdgeWeight = 1.0;
  for (int i = 0; i < visibility.size(); ++i) {
    graph->AddVertex(i);
    graph->AddEdge(i, i, kSelfEdgeWeight);
  }

  // Add an edge for each camera pair.
  for (const auto& camera_pair_count : camera_pairs) {
    const int camera1 = camera_pair_count.first.first;
    const int camera2 = camera_pair_count.first.second;
    const int count = camera_pair_count.second;
    DCHECK_NE(camera1, camera2);
    // Static cast necessary for Windows.
    const double weight =
        static_cast<double>(count) /
        (sqrt(static_cast<double>(visibility[camera1].size() *
                                  visibility[camera2].size())));
    graph->AddEdge(camera1, camera2, weight);
  }

  VLOG(2) << "Schur complement graph time: " << (time(nullptr) - start_time);
  return graph;
}

}  // namespace ceres::internal
