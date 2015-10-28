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

#ifndef CERES_INTERNAL_SINGLE_LINKAGE_CLUSTERING_H_
#define CERES_INTERNAL_SINGLE_LINKAGE_CLUSTERING_H_

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

#ifndef CERES_NO_SUITESPARSE

#include "ceres/collections_port.h"
#include "ceres/graph.h"

namespace ceres {
namespace internal {

struct SingleLinkageClusteringOptions {
  SingleLinkageClusteringOptions()
      : min_similarity(0.99) {
  }

  // Graph edges with edge weight less than min_similarity are ignored
  // during the clustering process.
  double min_similarity;
};

// Compute a partitioning of the vertices of the graph using the
// single linkage clustering algorithm. Edges with weight less than
// SingleLinkageClusteringOptions::min_similarity will be ignored.
//
// membership upon return will contain a mapping from the vertices of
// the graph to an integer indicating the identity of the cluster that
// it belongs to.
//
// The return value of this function is the number of clusters
// identified by the algorithm.
int ComputeSingleLinkageClustering(
    const SingleLinkageClusteringOptions& options,
    const WeightedGraph<int>& graph,
    HashMap<int, int>* membership);

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_SUITESPARSE
#endif  // CERES_INTERNAL_SINGLE_LINKAGE_CLUSTERING_H_
