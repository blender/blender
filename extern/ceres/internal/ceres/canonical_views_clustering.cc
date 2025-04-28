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
// Author: David Gallup (dgallup@google.com)
//         Sameer Agarwal (sameeragarwal@google.com)

#include "ceres/canonical_views_clustering.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ceres/graph.h"
#include "ceres/internal/export.h"
#include "ceres/map_util.h"
#include "glog/logging.h"

namespace ceres::internal {

using IntMap = std::unordered_map<int, int>;
using IntSet = std::unordered_set<int>;

class CERES_NO_EXPORT CanonicalViewsClustering {
 public:
  // Compute the canonical views clustering of the vertices of the
  // graph. centers will contain the vertices that are the identified
  // as the canonical views/cluster centers, and membership is a map
  // from vertices to cluster_ids. The i^th cluster center corresponds
  // to the i^th cluster. It is possible depending on the
  // configuration of the clustering algorithm that some of the
  // vertices may not be assigned to any cluster. In this case they
  // are assigned to a cluster with id = kInvalidClusterId.
  void ComputeClustering(const CanonicalViewsClusteringOptions& options,
                         const WeightedGraph<int>& graph,
                         std::vector<int>* centers,
                         IntMap* membership);

 private:
  void FindValidViews(IntSet* valid_views) const;
  double ComputeClusteringQualityDifference(
      int candidate, const std::vector<int>& centers) const;
  void UpdateCanonicalViewAssignments(const int canonical_view);
  void ComputeClusterMembership(const std::vector<int>& centers,
                                IntMap* membership) const;

  CanonicalViewsClusteringOptions options_;
  const WeightedGraph<int>* graph_;
  // Maps a view to its representative canonical view (its cluster
  // center).
  IntMap view_to_canonical_view_;
  // Maps a view to its similarity to its current cluster center.
  std::unordered_map<int, double> view_to_canonical_view_similarity_;
};

void ComputeCanonicalViewsClustering(
    const CanonicalViewsClusteringOptions& options,
    const WeightedGraph<int>& graph,
    std::vector<int>* centers,
    IntMap* membership) {
  time_t start_time = time(nullptr);
  CanonicalViewsClustering cv;
  cv.ComputeClustering(options, graph, centers, membership);
  VLOG(2) << "Canonical views clustering time (secs): "
          << time(nullptr) - start_time;
}

// Implementation of CanonicalViewsClustering
void CanonicalViewsClustering::ComputeClustering(
    const CanonicalViewsClusteringOptions& options,
    const WeightedGraph<int>& graph,
    std::vector<int>* centers,
    IntMap* membership) {
  options_ = options;
  CHECK(centers != nullptr);
  CHECK(membership != nullptr);
  centers->clear();
  membership->clear();
  graph_ = &graph;

  IntSet valid_views;
  FindValidViews(&valid_views);
  while (!valid_views.empty()) {
    // Find the next best canonical view.
    double best_difference = -std::numeric_limits<double>::max();
    int best_view = 0;

    // TODO(sameeragarwal): Make this loop multi-threaded.
    for (const auto& view : valid_views) {
      const double difference =
          ComputeClusteringQualityDifference(view, *centers);
      if (difference > best_difference) {
        best_difference = difference;
        best_view = view;
      }
    }

    CHECK_GT(best_difference, -std::numeric_limits<double>::max());

    // Add canonical view if quality improves, or if minimum is not
    // yet met, otherwise break.
    if ((best_difference <= 0) && (centers->size() >= options_.min_views)) {
      break;
    }

    centers->push_back(best_view);
    valid_views.erase(best_view);
    UpdateCanonicalViewAssignments(best_view);
  }

  ComputeClusterMembership(*centers, membership);
}

// Return the set of vertices of the graph which have valid vertex
// weights.
void CanonicalViewsClustering::FindValidViews(IntSet* valid_views) const {
  const IntSet& views = graph_->vertices();
  for (const auto& view : views) {
    if (graph_->VertexWeight(view) != WeightedGraph<int>::InvalidWeight()) {
      valid_views->insert(view);
    }
  }
}

// Computes the difference in the quality score if 'candidate' were
// added to the set of canonical views.
double CanonicalViewsClustering::ComputeClusteringQualityDifference(
    const int candidate, const std::vector<int>& centers) const {
  // View score.
  double difference =
      options_.view_score_weight * graph_->VertexWeight(candidate);

  // Compute how much the quality score changes if the candidate view
  // was added to the list of canonical views and its nearest
  // neighbors became members of its cluster.
  const IntSet& neighbors = graph_->Neighbors(candidate);
  for (const auto& neighbor : neighbors) {
    const double old_similarity =
        FindWithDefault(view_to_canonical_view_similarity_, neighbor, 0.0);
    const double new_similarity = graph_->EdgeWeight(neighbor, candidate);
    if (new_similarity > old_similarity) {
      difference += new_similarity - old_similarity;
    }
  }

  // Number of views penalty.
  difference -= options_.size_penalty_weight;

  // Orthogonality.
  for (int center : centers) {
    difference -= options_.similarity_penalty_weight *
                  graph_->EdgeWeight(center, candidate);
  }

  return difference;
}

// Reassign views if they're more similar to the new canonical view.
void CanonicalViewsClustering::UpdateCanonicalViewAssignments(
    const int canonical_view) {
  const IntSet& neighbors = graph_->Neighbors(canonical_view);
  for (const auto& neighbor : neighbors) {
    const double old_similarity =
        FindWithDefault(view_to_canonical_view_similarity_, neighbor, 0.0);
    const double new_similarity = graph_->EdgeWeight(neighbor, canonical_view);
    if (new_similarity > old_similarity) {
      view_to_canonical_view_[neighbor] = canonical_view;
      view_to_canonical_view_similarity_[neighbor] = new_similarity;
    }
  }
}

// Assign a cluster id to each view.
void CanonicalViewsClustering::ComputeClusterMembership(
    const std::vector<int>& centers, IntMap* membership) const {
  CHECK(membership != nullptr);
  membership->clear();

  // The i^th cluster has cluster id i.
  IntMap center_to_cluster_id;
  for (int i = 0; i < centers.size(); ++i) {
    center_to_cluster_id[centers[i]] = i;
  }

  static constexpr int kInvalidClusterId = -1;

  const IntSet& views = graph_->vertices();
  for (const auto& view : views) {
    auto it = view_to_canonical_view_.find(view);
    int cluster_id = kInvalidClusterId;
    if (it != view_to_canonical_view_.end()) {
      cluster_id = FindOrDie(center_to_cluster_id, it->second);
    }

    InsertOrDie(membership, view, cluster_id);
  }
}

}  // namespace ceres::internal
