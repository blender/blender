/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2009
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

#include <iostream>
#include <vector>

#include "test_tools.h"
#include <lemon/maps.h>
#include <lemon/kruskal.h>
#include <lemon/list_graph.h>

#include <lemon/concepts/maps.h>
#include <lemon/concepts/digraph.h>
#include <lemon/concepts/graph.h>

using namespace std;
using namespace lemon;

void checkCompileKruskal()
{
  concepts::WriteMap<concepts::Digraph::Arc,bool> w;
  concepts::WriteMap<concepts::Graph::Edge,bool> uw;

  concepts::ReadMap<concepts::Digraph::Arc,int> r;
  concepts::ReadMap<concepts::Graph::Edge,int> ur;

  concepts::Digraph g;
  concepts::Graph ug;

  kruskal(g, r, w);
  kruskal(ug, ur, uw);

  std::vector<std::pair<concepts::Digraph::Arc, int> > rs;
  std::vector<std::pair<concepts::Graph::Edge, int> > urs;

  kruskal(g, rs, w);
  kruskal(ug, urs, uw);

  std::vector<concepts::Digraph::Arc> ws;
  std::vector<concepts::Graph::Edge> uws;

  kruskal(g, r, ws.begin());
  kruskal(ug, ur, uws.begin());
}

int main() {

  typedef ListGraph::Node Node;
  typedef ListGraph::Edge Edge;
  typedef ListGraph::NodeIt NodeIt;
  typedef ListGraph::ArcIt ArcIt;

  ListGraph G;

  Node s=G.addNode();
  Node v1=G.addNode();
  Node v2=G.addNode();
  Node v3=G.addNode();
  Node v4=G.addNode();
  Node t=G.addNode();

  Edge e1 = G.addEdge(s, v1);
  Edge e2 = G.addEdge(s, v2);
  Edge e3 = G.addEdge(v1, v2);
  Edge e4 = G.addEdge(v2, v1);
  Edge e5 = G.addEdge(v1, v3);
  Edge e6 = G.addEdge(v3, v2);
  Edge e7 = G.addEdge(v2, v4);
  Edge e8 = G.addEdge(v4, v3);
  Edge e9 = G.addEdge(v3, t);
  Edge e10 = G.addEdge(v4, t);

  typedef ListGraph::EdgeMap<int> ECostMap;
  typedef ListGraph::EdgeMap<bool> EBoolMap;

  ECostMap edge_cost_map(G, 2);
  EBoolMap tree_map(G);


  //Test with const map.
  check(kruskal(G, ConstMap<ListGraph::Edge,int>(2), tree_map)==10,
        "Total cost should be 10");
  //Test with an edge map (filled with uniform costs).
  check(kruskal(G, edge_cost_map, tree_map)==10,
        "Total cost should be 10");

  edge_cost_map[e1] = -10;
  edge_cost_map[e2] = -9;
  edge_cost_map[e3] = -8;
  edge_cost_map[e4] = -7;
  edge_cost_map[e5] = -6;
  edge_cost_map[e6] = -5;
  edge_cost_map[e7] = -4;
  edge_cost_map[e8] = -3;
  edge_cost_map[e9] = -2;
  edge_cost_map[e10] = -1;

  vector<Edge> tree_edge_vec(5);

  //Test with a edge map and inserter.
  check(kruskal(G, edge_cost_map,
                 tree_edge_vec.begin())
        ==-31,
        "Total cost should be -31.");

  tree_edge_vec.clear();

  check(kruskal(G, edge_cost_map,
                back_inserter(tree_edge_vec))
        ==-31,
        "Total cost should be -31.");

//   tree_edge_vec.clear();

//   //The above test could also be coded like this:
//   check(kruskal(G,
//                 makeKruskalMapInput(G, edge_cost_map),
//                 makeKruskalSequenceOutput(back_inserter(tree_edge_vec)))
//         ==-31,
//         "Total cost should be -31.");

  check(tree_edge_vec.size()==5,"The tree should have 5 edges.");

  check(tree_edge_vec[0]==e1 &&
        tree_edge_vec[1]==e2 &&
        tree_edge_vec[2]==e5 &&
        tree_edge_vec[3]==e7 &&
        tree_edge_vec[4]==e9,
        "Wrong tree.");

  return 0;
}
