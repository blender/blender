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

#ifndef CERES_INTERNAL_GRAPH_H_
#define CERES_INTERNAL_GRAPH_H_

#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "ceres/internal/export.h"
#include "ceres/map_util.h"
#include "ceres/pair_hash.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres::internal {

// A unweighted undirected graph templated over the vertex ids. Vertex
// should be hashable.
template <typename Vertex>
class CERES_NO_EXPORT Graph {
 public:
  // Add a vertex.
  void AddVertex(const Vertex& vertex) {
    if (vertices_.insert(vertex).second) {
      edges_[vertex] = std::unordered_set<Vertex>();
    }
  }

  bool RemoveVertex(const Vertex& vertex) {
    if (vertices_.find(vertex) == vertices_.end()) {
      return false;
    }

    vertices_.erase(vertex);
    const std::unordered_set<Vertex>& sinks = edges_[vertex];
    for (const Vertex& s : sinks) {
      edges_[s].erase(vertex);
    }

    edges_.erase(vertex);
    return true;
  }

  // Add an edge between the vertex1 and vertex2. Calling AddEdge on a
  // pair of vertices which do not exist in the graph yet will result
  // in undefined behavior.
  //
  // It is legal to call this method repeatedly for the same set of
  // vertices.
  void AddEdge(const Vertex& vertex1, const Vertex& vertex2) {
    DCHECK(vertices_.find(vertex1) != vertices_.end());
    DCHECK(vertices_.find(vertex2) != vertices_.end());

    if (edges_[vertex1].insert(vertex2).second) {
      edges_[vertex2].insert(vertex1);
    }
  }

  // Calling Neighbors on a vertex not in the graph will result in
  // undefined behaviour.
  const std::unordered_set<Vertex>& Neighbors(const Vertex& vertex) const {
    return FindOrDie(edges_, vertex);
  }

  const std::unordered_set<Vertex>& vertices() const { return vertices_; }

 private:
  std::unordered_set<Vertex> vertices_;
  std::unordered_map<Vertex, std::unordered_set<Vertex>> edges_;
};

// A weighted undirected graph templated over the vertex ids. Vertex
// should be hashable and comparable.
template <typename Vertex>
class WeightedGraph {
 public:
  // Add a weighted vertex. If the vertex already exists in the graph,
  // its weight is set to the new weight.
  void AddVertex(const Vertex& vertex, double weight) {
    if (vertices_.find(vertex) == vertices_.end()) {
      vertices_.insert(vertex);
      edges_[vertex] = std::unordered_set<Vertex>();
    }
    vertex_weights_[vertex] = weight;
  }

  // Uses weight = 1.0. If vertex already exists, its weight is set to
  // 1.0.
  void AddVertex(const Vertex& vertex) { AddVertex(vertex, 1.0); }

  bool RemoveVertex(const Vertex& vertex) {
    if (vertices_.find(vertex) == vertices_.end()) {
      return false;
    }

    vertices_.erase(vertex);
    vertex_weights_.erase(vertex);
    const std::unordered_set<Vertex>& sinks = edges_[vertex];
    for (const Vertex& s : sinks) {
      if (vertex < s) {
        edge_weights_.erase(std::make_pair(vertex, s));
      } else {
        edge_weights_.erase(std::make_pair(s, vertex));
      }
      edges_[s].erase(vertex);
    }

    edges_.erase(vertex);
    return true;
  }

  // Add a weighted edge between the vertex1 and vertex2. Calling
  // AddEdge on a pair of vertices which do not exist in the graph yet
  // will result in undefined behavior.
  //
  // It is legal to call this method repeatedly for the same set of
  // vertices.
  void AddEdge(const Vertex& vertex1, const Vertex& vertex2, double weight) {
    DCHECK(vertices_.find(vertex1) != vertices_.end());
    DCHECK(vertices_.find(vertex2) != vertices_.end());

    if (edges_[vertex1].insert(vertex2).second) {
      edges_[vertex2].insert(vertex1);
    }

    if (vertex1 < vertex2) {
      edge_weights_[std::make_pair(vertex1, vertex2)] = weight;
    } else {
      edge_weights_[std::make_pair(vertex2, vertex1)] = weight;
    }
  }

  // Uses weight = 1.0.
  void AddEdge(const Vertex& vertex1, const Vertex& vertex2) {
    AddEdge(vertex1, vertex2, 1.0);
  }

  // Calling VertexWeight on a vertex not in the graph will result in
  // undefined behavior.
  double VertexWeight(const Vertex& vertex) const {
    return FindOrDie(vertex_weights_, vertex);
  }

  // Calling EdgeWeight on a pair of vertices where either one of the
  // vertices is not present in the graph will result in undefined
  // behaviour. If there is no edge connecting vertex1 and vertex2,
  // the edge weight is zero.
  double EdgeWeight(const Vertex& vertex1, const Vertex& vertex2) const {
    if (vertex1 < vertex2) {
      return FindWithDefault(
          edge_weights_, std::make_pair(vertex1, vertex2), 0.0);
    } else {
      return FindWithDefault(
          edge_weights_, std::make_pair(vertex2, vertex1), 0.0);
    }
  }

  // Calling Neighbors on a vertex not in the graph will result in
  // undefined behaviour.
  const std::unordered_set<Vertex>& Neighbors(const Vertex& vertex) const {
    return FindOrDie(edges_, vertex);
  }

  const std::unordered_set<Vertex>& vertices() const { return vertices_; }

  static double InvalidWeight() {
    return std::numeric_limits<double>::quiet_NaN();
  }

 private:
  std::unordered_set<Vertex> vertices_;
  std::unordered_map<Vertex, double> vertex_weights_;
  std::unordered_map<Vertex, std::unordered_set<Vertex>> edges_;
  std::unordered_map<std::pair<Vertex, Vertex>, double, pair_hash>
      edge_weights_;
};

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_GRAPH_H_
