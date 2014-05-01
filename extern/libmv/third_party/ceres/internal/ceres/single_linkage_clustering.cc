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

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

#ifndef CERES_NO_SUITESPARSE

#include "ceres/single_linkage_clustering.h"

#include "ceres/graph.h"
#include "ceres/collections_port.h"
#include "ceres/graph_algorithms.h"

namespace ceres {
namespace internal {

int ComputeSingleLinkageClustering(
    const SingleLinkageClusteringOptions& options,
    const Graph<int>& graph,
    HashMap<int, int>* membership) {
  CHECK_NOTNULL(membership)->clear();

  // Initially each vertex is in its own cluster.
  const HashSet<int>& vertices = graph.vertices();
  for (HashSet<int>::const_iterator it = vertices.begin();
       it != vertices.end();
       ++it) {
    (*membership)[*it] = *it;
  }

  for (HashSet<int>::const_iterator it1 = vertices.begin();
       it1 != vertices.end();
       ++it1) {
    const int vertex1 = *it1;
    const HashSet<int>& neighbors = graph.Neighbors(vertex1);
    for (HashSet<int>::const_iterator it2 = neighbors.begin();
         it2 != neighbors.end();
         ++it2) {
      const int vertex2 = *it2;

      // Since the graph is undirected, only pay attention to one side
      // of the edge and ignore weak edges.
      if ((vertex1 > vertex2) ||
          (graph.EdgeWeight(vertex1, vertex2) < options.min_similarity)) {
        continue;
      }

      // Use a union-find algorithm to keep track of the clusters.
      const int c1 = FindConnectedComponent(vertex1, membership);
      const int c2 = FindConnectedComponent(vertex2, membership);

      if (c1 == c2) {
        continue;
      }

      if (c1 < c2) {
        (*membership)[c2] = c1;
      } else {
        (*membership)[c1] = c2;
      }
    }
  }

  // Make sure that every vertex is connected directly to the vertex
  // identifying the cluster.
  int num_clusters = 0;
  for (HashMap<int, int>::iterator it = membership->begin();
       it != membership->end();
       ++it) {
    it->second = FindConnectedComponent(it->first, membership);
    if (it->first == it->second) {
      ++num_clusters;
    }
  }

  return num_clusters;
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_SUITESPARSE
