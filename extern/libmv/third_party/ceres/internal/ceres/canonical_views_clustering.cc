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
// Author: David Gallup (dgallup@google.com)
//         Sameer Agarwal (sameeragarwal@google.com)

#ifndef CERES_NO_SUITESPARSE

#include "ceres/canonical_views_clustering.h"

#include "ceres/collections_port.h"
#include "ceres/graph.h"
#include "ceres/internal/macros.h"
#include "ceres/map_util.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

typedef HashMap<int, int> IntMap;
typedef HashSet<int> IntSet;

class CanonicalViewsClustering {
 public:
  CanonicalViewsClustering() {}

  // Compute the canonical views clustering of the vertices of the
  // graph. centers will contain the vertices that are the identified
  // as the canonical views/cluster centers, and membership is a map
  // from vertices to cluster_ids. The i^th cluster center corresponds
  // to the i^th cluster. It is possible depending on the
  // configuration of the clustering algorithm that some of the
  // vertices may not be assigned to any cluster. In this case they
  // are assigned to a cluster with id = kInvalidClusterId.
  void ComputeClustering(const Graph<int>& graph,
                         const CanonicalViewsClusteringOptions& options,
                         vector<int>* centers,
                         IntMap* membership);

 private:
  void FindValidViews(IntSet* valid_views) const;
  double ComputeClusteringQualityDifference(const int candidate,
                                            const vector<int>& centers) const;
  void UpdateCanonicalViewAssignments(const int canonical_view);
  void ComputeClusterMembership(const vector<int>& centers,
                                IntMap* membership) const;

  CanonicalViewsClusteringOptions options_;
  const Graph<int>* graph_;
  // Maps a view to its representative canonical view (its cluster
  // center).
  IntMap view_to_canonical_view_;
  // Maps a view to its similarity to its current cluster center.
  HashMap<int, double> view_to_canonical_view_similarity_;
  CERES_DISALLOW_COPY_AND_ASSIGN(CanonicalViewsClustering);
};

void ComputeCanonicalViewsClustering(
    const Graph<int>& graph,
    const CanonicalViewsClusteringOptions& options,
    vector<int>* centers,
    IntMap* membership) {
  time_t start_time = time(NULL);
  CanonicalViewsClustering cv;
  cv.ComputeClustering(graph, options, centers, membership);
  VLOG(2) << "Canonical views clustering time (secs): "
          << time(NULL) - start_time;
}

// Implementation of CanonicalViewsClustering
void CanonicalViewsClustering::ComputeClustering(
    const Graph<int>& graph,
    const CanonicalViewsClusteringOptions& options,
    vector<int>* centers,
    IntMap* membership) {
  options_ = options;
  CHECK_NOTNULL(centers)->clear();
  CHECK_NOTNULL(membership)->clear();
  graph_ = &graph;

  IntSet valid_views;
  FindValidViews(&valid_views);
  while (valid_views.size() > 0) {
    // Find the next best canonical view.
    double best_difference = -std::numeric_limits<double>::max();
    int best_view = 0;

    // TODO(sameeragarwal): Make this loop multi-threaded.
    for (IntSet::const_iterator view = valid_views.begin();
         view != valid_views.end();
         ++view) {
      const double difference =
          ComputeClusteringQualityDifference(*view, *centers);
      if (difference > best_difference) {
        best_difference = difference;
        best_view = *view;
      }
    }

    CHECK_GT(best_difference, -std::numeric_limits<double>::max());

    // Add canonical view if quality improves, or if minimum is not
    // yet met, otherwise break.
    if ((best_difference <= 0) &&
        (centers->size() >= options_.min_views)) {
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
void CanonicalViewsClustering::FindValidViews(
    IntSet* valid_views) const {
  const IntSet& views = graph_->vertices();
  for (IntSet::const_iterator view = views.begin();
       view != views.end();
       ++view) {
    if (graph_->VertexWeight(*view) != Graph<int>::InvalidWeight()) {
      valid_views->insert(*view);
    }
  }
}

// Computes the difference in the quality score if 'candidate' were
// added to the set of canonical views.
double CanonicalViewsClustering::ComputeClusteringQualityDifference(
    const int candidate,
    const vector<int>& centers) const {
  // View score.
  double difference =
      options_.view_score_weight * graph_->VertexWeight(candidate);

  // Compute how much the quality score changes if the candidate view
  // was added to the list of canonical views and its nearest
  // neighbors became members of its cluster.
  const IntSet& neighbors = graph_->Neighbors(candidate);
  for (IntSet::const_iterator neighbor = neighbors.begin();
       neighbor != neighbors.end();
       ++neighbor) {
    const double old_similarity =
        FindWithDefault(view_to_canonical_view_similarity_, *neighbor, 0.0);
    const double new_similarity = graph_->EdgeWeight(*neighbor, candidate);
    if (new_similarity > old_similarity) {
      difference += new_similarity - old_similarity;
    }
  }

  // Number of views penalty.
  difference -= options_.size_penalty_weight;

  // Orthogonality.
  for (int i = 0; i < centers.size(); ++i) {
    difference -= options_.similarity_penalty_weight *
        graph_->EdgeWeight(centers[i], candidate);
  }

  return difference;
}

// Reassign views if they're more similar to the new canonical view.
void CanonicalViewsClustering::UpdateCanonicalViewAssignments(
    const int canonical_view) {
  const IntSet& neighbors = graph_->Neighbors(canonical_view);
  for (IntSet::const_iterator neighbor = neighbors.begin();
       neighbor != neighbors.end();
       ++neighbor) {
    const double old_similarity =
        FindWithDefault(view_to_canonical_view_similarity_, *neighbor, 0.0);
    const double new_similarity =
        graph_->EdgeWeight(*neighbor, canonical_view);
    if (new_similarity > old_similarity) {
      view_to_canonical_view_[*neighbor] = canonical_view;
      view_to_canonical_view_similarity_[*neighbor] = new_similarity;
    }
  }
}

// Assign a cluster id to each view.
void CanonicalViewsClustering::ComputeClusterMembership(
    const vector<int>& centers,
    IntMap* membership) const {
  CHECK_NOTNULL(membership)->clear();

  // The i^th cluster has cluster id i.
  IntMap center_to_cluster_id;
  for (int i = 0; i < centers.size(); ++i) {
    center_to_cluster_id[centers[i]] = i;
  }

  static const int kInvalidClusterId = -1;

  const IntSet& views = graph_->vertices();
  for (IntSet::const_iterator view = views.begin();
       view != views.end();
       ++view) {
    IntMap::const_iterator it =
        view_to_canonical_view_.find(*view);
    int cluster_id = kInvalidClusterId;
    if (it != view_to_canonical_view_.end()) {
      cluster_id = FindOrDie(center_to_cluster_id, it->second);
    }

    InsertOrDie(membership, *view, cluster_id);
  }
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_SUITESPARSE
