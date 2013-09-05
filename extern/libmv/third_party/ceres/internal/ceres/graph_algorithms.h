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
// Various algorithms that operate on undirected graphs.

#ifndef CERES_INTERNAL_GRAPH_ALGORITHMS_H_
#define CERES_INTERNAL_GRAPH_ALGORITHMS_H_

#include <algorithm>
#include <vector>
#include <utility>
#include "ceres/collections_port.h"
#include "ceres/graph.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

// Compare two vertices of a graph by their degrees, if the degrees
// are equal then order them by their ids.
template <typename Vertex>
class VertexTotalOrdering {
 public:
  explicit VertexTotalOrdering(const Graph<Vertex>& graph)
      : graph_(graph) {}

  bool operator()(const Vertex& lhs, const Vertex& rhs) const {
    if (graph_.Neighbors(lhs).size() == graph_.Neighbors(rhs).size()) {
      return lhs < rhs;
    }
    return graph_.Neighbors(lhs).size() < graph_.Neighbors(rhs).size();
  }

 private:
  const Graph<Vertex>& graph_;
};

template <typename Vertex>
class VertexDegreeLessThan {
 public:
  explicit VertexDegreeLessThan(const Graph<Vertex>& graph)
      : graph_(graph) {}

  bool operator()(const Vertex& lhs, const Vertex& rhs) const {
    return graph_.Neighbors(lhs).size() < graph_.Neighbors(rhs).size();
  }

 private:
  const Graph<Vertex>& graph_;
};

// Order the vertices of a graph using its (approximately) largest
// independent set, where an independent set of a graph is a set of
// vertices that have no edges connecting them. The maximum
// independent set problem is NP-Hard, but there are effective
// approximation algorithms available. The implementation here uses a
// breadth first search that explores the vertices in order of
// increasing degree. The same idea is used by Saad & Li in "MIQR: A
// multilevel incomplete QR preconditioner for large sparse
// least-squares problems", SIMAX, 2007.
//
// Given a undirected graph G(V,E), the algorithm is a greedy BFS
// search where the vertices are explored in increasing order of their
// degree. The output vector ordering contains elements of S in
// increasing order of their degree, followed by elements of V - S in
// increasing order of degree. The return value of the function is the
// cardinality of S.
template <typename Vertex>
int IndependentSetOrdering(const Graph<Vertex>& graph,
                           vector<Vertex>* ordering) {
  const HashSet<Vertex>& vertices = graph.vertices();
  const int num_vertices = vertices.size();

  CHECK_NOTNULL(ordering);
  ordering->clear();
  ordering->reserve(num_vertices);

  // Colors for labeling the graph during the BFS.
  const char kWhite = 0;
  const char kGrey = 1;
  const char kBlack = 2;

  // Mark all vertices white.
  HashMap<Vertex, char> vertex_color;
  vector<Vertex> vertex_queue;
  for (typename HashSet<Vertex>::const_iterator it = vertices.begin();
       it != vertices.end();
       ++it) {
    vertex_color[*it] = kWhite;
    vertex_queue.push_back(*it);
  }


  sort(vertex_queue.begin(), vertex_queue.end(),
       VertexTotalOrdering<Vertex>(graph));

  // Iterate over vertex_queue. Pick the first white vertex, add it
  // to the independent set. Mark it black and its neighbors grey.
  for (int i = 0; i < vertex_queue.size(); ++i) {
    const Vertex& vertex = vertex_queue[i];
    if (vertex_color[vertex] != kWhite) {
      continue;
    }

    ordering->push_back(vertex);
    vertex_color[vertex] = kBlack;
    const HashSet<Vertex>& neighbors = graph.Neighbors(vertex);
    for (typename HashSet<Vertex>::const_iterator it = neighbors.begin();
         it != neighbors.end();
         ++it) {
      vertex_color[*it] = kGrey;
    }
  }

  int independent_set_size = ordering->size();

  // Iterate over the vertices and add all the grey vertices to the
  // ordering. At this stage there should only be black or grey
  // vertices in the graph.
  for (typename vector<Vertex>::const_iterator it = vertex_queue.begin();
       it != vertex_queue.end();
       ++it) {
    const Vertex vertex = *it;
    DCHECK(vertex_color[vertex] != kWhite);
    if (vertex_color[vertex] != kBlack) {
      ordering->push_back(vertex);
    }
  }

  CHECK_EQ(ordering->size(), num_vertices);
  return independent_set_size;
}

// Same as above with one important difference. The ordering parameter
// is an input/output parameter which carries an initial ordering of
// the vertices of the graph. The greedy independent set algorithm
// starts by sorting the vertices in increasing order of their
// degree. The input ordering is used to stabilize this sort, i.e., if
// two vertices have the same degree then they are ordered in the same
// order in which they occur in "ordering".
//
// This is useful in eliminating non-determinism from the Schur
// ordering algorithm over all.
template <typename Vertex>
int StableIndependentSetOrdering(const Graph<Vertex>& graph,
                                 vector<Vertex>* ordering) {
  CHECK_NOTNULL(ordering);
  const HashSet<Vertex>& vertices = graph.vertices();
  const int num_vertices = vertices.size();
  CHECK_EQ(vertices.size(), ordering->size());

  // Colors for labeling the graph during the BFS.
  const char kWhite = 0;
  const char kGrey = 1;
  const char kBlack = 2;

  vector<Vertex> vertex_queue(*ordering);

  stable_sort(vertex_queue.begin(), vertex_queue.end(),
              VertexDegreeLessThan<Vertex>(graph));

  // Mark all vertices white.
  HashMap<Vertex, char> vertex_color;
  for (typename HashSet<Vertex>::const_iterator it = vertices.begin();
       it != vertices.end();
       ++it) {
    vertex_color[*it] = kWhite;
  }

  ordering->clear();
  ordering->reserve(num_vertices);
  // Iterate over vertex_queue. Pick the first white vertex, add it
  // to the independent set. Mark it black and its neighbors grey.
  for (int i = 0; i < vertex_queue.size(); ++i) {
    const Vertex& vertex = vertex_queue[i];
    if (vertex_color[vertex] != kWhite) {
      continue;
    }

    ordering->push_back(vertex);
    vertex_color[vertex] = kBlack;
    const HashSet<Vertex>& neighbors = graph.Neighbors(vertex);
    for (typename HashSet<Vertex>::const_iterator it = neighbors.begin();
         it != neighbors.end();
         ++it) {
      vertex_color[*it] = kGrey;
    }
  }

  int independent_set_size = ordering->size();

  // Iterate over the vertices and add all the grey vertices to the
  // ordering. At this stage there should only be black or grey
  // vertices in the graph.
  for (typename vector<Vertex>::const_iterator it = vertex_queue.begin();
       it != vertex_queue.end();
       ++it) {
    const Vertex vertex = *it;
    DCHECK(vertex_color[vertex] != kWhite);
    if (vertex_color[vertex] != kBlack) {
      ordering->push_back(vertex);
    }
  }

  CHECK_EQ(ordering->size(), num_vertices);
  return independent_set_size;
}

// Find the connected component for a vertex implemented using the
// find and update operation for disjoint-set. Recursively traverse
// the disjoint set structure till you reach a vertex whose connected
// component has the same id as the vertex itself. Along the way
// update the connected components of all the vertices. This updating
// is what gives this data structure its efficiency.
template <typename Vertex>
Vertex FindConnectedComponent(const Vertex& vertex,
                              HashMap<Vertex, Vertex>* union_find) {
  typename HashMap<Vertex, Vertex>::iterator it = union_find->find(vertex);
  DCHECK(it != union_find->end());
  if (it->second != vertex) {
    it->second = FindConnectedComponent(it->second, union_find);
  }

  return it->second;
}

// Compute a degree two constrained Maximum Spanning Tree/forest of
// the input graph. Caller owns the result.
//
// Finding degree 2 spanning tree of a graph is not always
// possible. For example a star graph, i.e. a graph with n-nodes
// where one node is connected to the other n-1 nodes does not have
// a any spanning trees of degree less than n-1.Even if such a tree
// exists, finding such a tree is NP-Hard.

// We get around both of these problems by using a greedy, degree
// constrained variant of Kruskal's algorithm. We start with a graph
// G_T with the same vertex set V as the input graph G(V,E) but an
// empty edge set. We then iterate over the edges of G in decreasing
// order of weight, adding them to G_T if doing so does not create a
// cycle in G_T} and the degree of all the vertices in G_T remains
// bounded by two. This O(|E|) algorithm results in a degree-2
// spanning forest, or a collection of linear paths that span the
// graph G.
template <typename Vertex>
Graph<Vertex>*
Degree2MaximumSpanningForest(const Graph<Vertex>& graph) {
  // Array of edges sorted in decreasing order of their weights.
  vector<pair<double, pair<Vertex, Vertex> > > weighted_edges;
  Graph<Vertex>* forest = new Graph<Vertex>();

  // Disjoint-set to keep track of the connected components in the
  // maximum spanning tree.
  HashMap<Vertex, Vertex> disjoint_set;

  // Sort of the edges in the graph in decreasing order of their
  // weight. Also add the vertices of the graph to the Maximum
  // Spanning Tree graph and set each vertex to be its own connected
  // component in the disjoint_set structure.
  const HashSet<Vertex>& vertices = graph.vertices();
  for (typename HashSet<Vertex>::const_iterator it = vertices.begin();
       it != vertices.end();
       ++it) {
    const Vertex vertex1 = *it;
    forest->AddVertex(vertex1, graph.VertexWeight(vertex1));
    disjoint_set[vertex1] = vertex1;

    const HashSet<Vertex>& neighbors = graph.Neighbors(vertex1);
    for (typename HashSet<Vertex>::const_iterator it2 = neighbors.begin();
         it2 != neighbors.end();
         ++it2) {
      const Vertex vertex2 = *it2;
      if (vertex1 >= vertex2) {
        continue;
      }
      const double weight = graph.EdgeWeight(vertex1, vertex2);
      weighted_edges.push_back(make_pair(weight, make_pair(vertex1, vertex2)));
    }
  }

  // The elements of this vector, are pairs<edge_weight,
  // edge>. Sorting it using the reverse iterators gives us the edges
  // in decreasing order of edges.
  sort(weighted_edges.rbegin(), weighted_edges.rend());

  // Greedily add edges to the spanning tree/forest as long as they do
  // not violate the degree/cycle constraint.
  for (int i =0; i < weighted_edges.size(); ++i) {
    const pair<Vertex, Vertex>& edge = weighted_edges[i].second;
    const Vertex vertex1 = edge.first;
    const Vertex vertex2 = edge.second;

    // Check if either of the vertices are of degree 2 already, in
    // which case adding this edge will violate the degree 2
    // constraint.
    if ((forest->Neighbors(vertex1).size() == 2) ||
        (forest->Neighbors(vertex2).size() == 2)) {
      continue;
    }

    // Find the id of the connected component to which the two
    // vertices belong to. If the id is the same, it means that the
    // two of them are already connected to each other via some other
    // vertex, and adding this edge will create a cycle.
    Vertex root1 = FindConnectedComponent(vertex1, &disjoint_set);
    Vertex root2 = FindConnectedComponent(vertex2, &disjoint_set);

    if (root1 == root2) {
      continue;
    }

    // This edge can be added, add an edge in either direction with
    // the same weight as the original graph.
    const double edge_weight = graph.EdgeWeight(vertex1, vertex2);
    forest->AddEdge(vertex1, vertex2, edge_weight);
    forest->AddEdge(vertex2, vertex1, edge_weight);

    // Connected the two connected components by updating the
    // disjoint_set structure. Always connect the connected component
    // with the greater index with the connected component with the
    // smaller index. This should ensure shallower trees, for quicker
    // lookup.
    if (root2 < root1) {
      std::swap(root1, root2);
    };

    disjoint_set[root2] = root1;
  }
  return forest;
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_GRAPH_ALGORITHMS_H_
