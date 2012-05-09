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
//
// An implementation of the Canonical Views clustering algorithm from
// "Scene Summarization for Online Image Collections", Ian Simon, Noah
// Snavely, Steven M. Seitz, ICCV 2007.
//
// More details can be found at
// http://grail.cs.washington.edu/projects/canonview/
//
// Ceres uses this algorithm to perform view clustering for
// constructing visibility based preconditioners.

#ifndef CERES_INTERNAL_CANONICAL_VIEWS_CLUSTERING_H_
#define CERES_INTERNAL_CANONICAL_VIEWS_CLUSTERING_H_

#include <vector>

#include <glog/logging.h>
#include "ceres/collections_port.h"
#include "ceres/graph.h"
#include "ceres/map_util.h"
#include "ceres/internal/macros.h"

namespace ceres {
namespace internal {

class CanonicalViewsClusteringOptions;

// Compute a partitioning of the vertices of the graph using the
// canonical views clustering algorithm.
//
// In the following we will use the terms vertices and views
// interchangably.  Given a weighted Graph G(V,E), the canonical views
// of G are the the set of vertices that best "summarize" the content
// of the graph. If w_ij i s the weight connecting the vertex i to
// vertex j, and C is the set of canonical views. Then the objective
// of the canonical views algorithm is
//
//   E[C] = sum_[i in V] max_[j in C] w_ij
//          - size_penalty_weight * |C|
//          - similarity_penalty_weight * sum_[i in C, j in C, j > i] w_ij
//
// alpha is the size penalty that penalizes large number of canonical
// views.
//
// beta is the similarity penalty that penalizes canonical views that
// are too similar to other canonical views.
//
// Thus the canonical views algorithm tries to find a canonical view
// for each vertex in the graph which best explains it, while trying
// to minimize the number of canonical views and the overlap between
// them.
//
// We further augment the above objective function by allowing for per
// vertex weights, higher weights indicating a higher preference for
// being chosen as a canonical view. Thus if w_i is the vertex weight
// for vertex i, the objective function is then
//
//   E[C] = sum_[i in V] max_[j in C] w_ij
//          - size_penalty_weight * |C|
//          - similarity_penalty_weight * sum_[i in C, j in C, j > i] w_ij
//          + view_score_weight * sum_[i in C] w_i
//
// centers will contain the vertices that are the identified
// as the canonical views/cluster centers, and membership is a map
// from vertices to cluster_ids. The i^th cluster center corresponds
// to the i^th cluster.
//
// It is possible depending on the configuration of the clustering
// algorithm that some of the vertices may not be assigned to any
// cluster. In this case they are assigned to a cluster with id = -1;
void ComputeCanonicalViewsClustering(
    const Graph<int>& graph,
    const CanonicalViewsClusteringOptions& options,
    vector<int>* centers,
    HashMap<int, int>* membership);

struct CanonicalViewsClusteringOptions {
  CanonicalViewsClusteringOptions()
      : min_views(3),
        size_penalty_weight(5.75),
        similarity_penalty_weight(100.0),
        view_score_weight(0.0) {
  }
  // The minimum number of canonical views to compute.
  int min_views;

  // Penalty weight for the number of canonical views.  A higher
  // number will result in fewer canonical views.
  double size_penalty_weight;

  // Penalty weight for the diversity (orthogonality) of the
  // canonical views.  A higher number will encourage less similar
  // canonical views.
  double similarity_penalty_weight;

  // Weight for per-view scores.  Lower weight places less
  // confidence in the view scores.
  double view_score_weight;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_CANONICAL_VIEWS_CLUSTERING_H_
