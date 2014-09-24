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
// Author: kushalav@google.com (Avanish Kushal)

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

#ifndef CERES_NO_SUITESPARSE

#include "ceres/visibility.h"

#include <cmath>
#include <ctime>
#include <algorithm>
#include <set>
#include <vector>
#include <utility>
#include "ceres/block_structure.h"
#include "ceres/collections_port.h"
#include "ceres/graph.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

void ComputeVisibility(const CompressedRowBlockStructure& block_structure,
                       const int num_eliminate_blocks,
                       vector< set<int> >* visibility) {
  CHECK_NOTNULL(visibility);

  // Clear the visibility vector and resize it to hold a
  // vector for each camera.
  visibility->resize(0);
  visibility->resize(block_structure.cols.size() - num_eliminate_blocks);

  for (int i = 0; i < block_structure.rows.size(); ++i) {
    const vector<Cell>& cells = block_structure.rows[i].cells;
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

WeightedGraph<int>* CreateSchurComplementGraph(
    const vector<set<int> >& visibility) {
  const time_t start_time = time(NULL);
  // Compute the number of e_blocks/point blocks. Since the visibility
  // set for each e_block/camera contains the set of e_blocks/points
  // visible to it, we find the maximum across all visibility sets.
  int num_points = 0;
  for (int i = 0; i < visibility.size(); i++) {
    if (visibility[i].size() > 0) {
      num_points = max(num_points, (*visibility[i].rbegin()) + 1);
    }
  }

  // Invert the visibility. The input is a camera->point mapping,
  // which tells us which points are visible in which
  // cameras. However, to compute the sparsity structure of the Schur
  // Complement efficiently, its better to have the point->camera
  // mapping.
  vector<set<int> > inverse_visibility(num_points);
  for (int i = 0; i < visibility.size(); i++) {
    const set<int>& visibility_set = visibility[i];
    for (set<int>::const_iterator it = visibility_set.begin();
         it != visibility_set.end();
         ++it) {
      inverse_visibility[*it].insert(i);
    }
  }

  // Map from camera pairs to number of points visible to both cameras
  // in the pair.
  HashMap<pair<int, int>, int > camera_pairs;

  // Count the number of points visible to each camera/f_block pair.
  for (vector<set<int> >::const_iterator it = inverse_visibility.begin();
       it != inverse_visibility.end();
       ++it) {
    const set<int>& inverse_visibility_set = *it;
    for (set<int>::const_iterator camera1 = inverse_visibility_set.begin();
         camera1 != inverse_visibility_set.end();
         ++camera1) {
      set<int>::const_iterator camera2 = camera1;
      for (++camera2; camera2 != inverse_visibility_set.end(); ++camera2) {
        ++(camera_pairs[make_pair(*camera1, *camera2)]);
      }
    }
  }

  WeightedGraph<int>* graph = new WeightedGraph<int>;

  // Add vertices and initialize the pairs for self edges so that self
  // edges are guaranteed. This is needed for the Canonical views
  // algorithm to work correctly.
  static const double kSelfEdgeWeight = 1.0;
  for (int i = 0; i < visibility.size(); ++i) {
    graph->AddVertex(i);
    graph->AddEdge(i, i, kSelfEdgeWeight);
  }

  // Add an edge for each camera pair.
  for (HashMap<pair<int, int>, int>::const_iterator it = camera_pairs.begin();
       it != camera_pairs.end();
       ++it) {
    const int camera1 = it->first.first;
    const int camera2 = it->first.second;
    CHECK_NE(camera1, camera2);

    const int count = it->second;
    // Static cast necessary for Windows.
    const double weight = static_cast<double>(count) /
        (sqrt(static_cast<double>(
                  visibility[camera1].size() * visibility[camera2].size())));
    graph->AddEdge(camera1, camera2, weight);
  }

  VLOG(2) << "Schur complement graph time: " << (time(NULL) - start_time);
  return graph;
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_SUITESPARSE
