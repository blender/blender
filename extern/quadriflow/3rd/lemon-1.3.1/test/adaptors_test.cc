/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2013
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
#include <limits>

#include <lemon/list_graph.h>
#include <lemon/grid_graph.h>
#include <lemon/bfs.h>
#include <lemon/path.h>

#include <lemon/concepts/digraph.h>
#include <lemon/concepts/graph.h>
#include <lemon/concepts/graph_components.h>
#include <lemon/concepts/maps.h>
#include <lemon/concept_check.h>

#include <lemon/adaptors.h>

#include "test/test_tools.h"
#include "test/graph_test.h"

using namespace lemon;

void checkReverseDigraph() {
  // Check concepts
  checkConcept<concepts::Digraph, ReverseDigraph<concepts::Digraph> >();
  checkConcept<concepts::Digraph, ReverseDigraph<ListDigraph> >();
  checkConcept<concepts::AlterableDigraphComponent<>,
               ReverseDigraph<ListDigraph> >();
  checkConcept<concepts::ExtendableDigraphComponent<>,
               ReverseDigraph<ListDigraph> >();
  checkConcept<concepts::ErasableDigraphComponent<>,
               ReverseDigraph<ListDigraph> >();
  checkConcept<concepts::ClearableDigraphComponent<>,
               ReverseDigraph<ListDigraph> >();

  // Create a digraph and an adaptor
  typedef ListDigraph Digraph;
  typedef ReverseDigraph<Digraph> Adaptor;

  Digraph digraph;
  Adaptor adaptor(digraph);

  // Add nodes and arcs to the original digraph
  Digraph::Node n1 = digraph.addNode();
  Digraph::Node n2 = digraph.addNode();
  Digraph::Node n3 = digraph.addNode();

  Digraph::Arc a1 = digraph.addArc(n1, n2);
  Digraph::Arc a2 = digraph.addArc(n1, n3);
  Digraph::Arc a3 = digraph.addArc(n2, n3);
  ::lemon::ignore_unused_variable_warning(a3);

  // Check the adaptor
  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 3);
  checkGraphConArcList(adaptor, 3);

  checkGraphOutArcList(adaptor, n1, 0);
  checkGraphOutArcList(adaptor, n2, 1);
  checkGraphOutArcList(adaptor, n3, 2);

  checkGraphInArcList(adaptor, n1, 2);
  checkGraphInArcList(adaptor, n2, 1);
  checkGraphInArcList(adaptor, n3, 0);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Check the orientation of the arcs
  for (Adaptor::ArcIt a(adaptor); a != INVALID; ++a) {
    check(adaptor.source(a) == digraph.target(a), "Wrong reverse");
    check(adaptor.target(a) == digraph.source(a), "Wrong reverse");
  }

  // Add and erase nodes and arcs in the digraph through the adaptor
  Adaptor::Node n4 = adaptor.addNode();

  Adaptor::Arc a4 = adaptor.addArc(n4, n3);
  Adaptor::Arc a5 = adaptor.addArc(n2, n4);
  Adaptor::Arc a6 = adaptor.addArc(n2, n4);
  Adaptor::Arc a7 = adaptor.addArc(n1, n4);
  Adaptor::Arc a8 = adaptor.addArc(n1, n2);
  ::lemon::ignore_unused_variable_warning(a6,a7,a8);

  adaptor.erase(a1);
  adaptor.erase(n3);

  // Check the adaptor
  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 4);
  checkGraphConArcList(adaptor, 4);

  checkGraphOutArcList(adaptor, n1, 2);
  checkGraphOutArcList(adaptor, n2, 2);
  checkGraphOutArcList(adaptor, n4, 0);

  checkGraphInArcList(adaptor, n1, 0);
  checkGraphInArcList(adaptor, n2, 1);
  checkGraphInArcList(adaptor, n4, 3);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Check the digraph
  checkGraphNodeList(digraph, 3);
  checkGraphArcList(digraph, 4);
  checkGraphConArcList(digraph, 4);

  checkGraphOutArcList(digraph, n1, 0);
  checkGraphOutArcList(digraph, n2, 1);
  checkGraphOutArcList(digraph, n4, 3);

  checkGraphInArcList(digraph, n1, 2);
  checkGraphInArcList(digraph, n2, 2);
  checkGraphInArcList(digraph, n4, 0);

  checkNodeIds(digraph);
  checkArcIds(digraph);

  checkGraphNodeMap(digraph);
  checkGraphArcMap(digraph);

  // Check the conversion of nodes and arcs
  Digraph::Node nd = n4;
  nd = n4;
  Adaptor::Node na = n1;
  na = n2;
  Digraph::Arc ad = a4;
  ad = a5;
  Adaptor::Arc aa = a1;
  aa = a2;
}

void checkSubDigraph() {
  // Check concepts
  checkConcept<concepts::Digraph, SubDigraph<concepts::Digraph> >();
  checkConcept<concepts::Digraph, SubDigraph<ListDigraph> >();
  checkConcept<concepts::AlterableDigraphComponent<>,
               SubDigraph<ListDigraph> >();
  checkConcept<concepts::ExtendableDigraphComponent<>,
               SubDigraph<ListDigraph> >();
  checkConcept<concepts::ErasableDigraphComponent<>,
               SubDigraph<ListDigraph> >();
  checkConcept<concepts::ClearableDigraphComponent<>,
               SubDigraph<ListDigraph> >();

  // Create a digraph and an adaptor
  typedef ListDigraph Digraph;
  typedef Digraph::NodeMap<bool> NodeFilter;
  typedef Digraph::ArcMap<bool> ArcFilter;
  typedef SubDigraph<Digraph, NodeFilter, ArcFilter> Adaptor;

  Digraph digraph;
  NodeFilter node_filter(digraph);
  ArcFilter arc_filter(digraph);
  Adaptor adaptor(digraph, node_filter, arc_filter);

  // Add nodes and arcs to the original digraph and the adaptor
  Digraph::Node n1 = digraph.addNode();
  Digraph::Node n2 = digraph.addNode();
  Adaptor::Node n3 = adaptor.addNode();

  node_filter[n1] = node_filter[n2] = node_filter[n3] = true;

  Digraph::Arc a1 = digraph.addArc(n1, n2);
  Digraph::Arc a2 = digraph.addArc(n1, n3);
  Adaptor::Arc a3 = adaptor.addArc(n2, n3);

  arc_filter[a1] = arc_filter[a2] = arc_filter[a3] = true;

  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 3);
  checkGraphConArcList(adaptor, 3);

  checkGraphOutArcList(adaptor, n1, 2);
  checkGraphOutArcList(adaptor, n2, 1);
  checkGraphOutArcList(adaptor, n3, 0);

  checkGraphInArcList(adaptor, n1, 0);
  checkGraphInArcList(adaptor, n2, 1);
  checkGraphInArcList(adaptor, n3, 2);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Hide an arc
  adaptor.status(a2, false);
  adaptor.disable(a3);
  if (!adaptor.status(a3)) adaptor.enable(a3);

  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 2);
  checkGraphConArcList(adaptor, 2);

  checkGraphOutArcList(adaptor, n1, 1);
  checkGraphOutArcList(adaptor, n2, 1);
  checkGraphOutArcList(adaptor, n3, 0);

  checkGraphInArcList(adaptor, n1, 0);
  checkGraphInArcList(adaptor, n2, 1);
  checkGraphInArcList(adaptor, n3, 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Hide a node
  adaptor.status(n1, false);
  adaptor.disable(n3);
  if (!adaptor.status(n3)) adaptor.enable(n3);

  checkGraphNodeList(adaptor, 2);
  checkGraphArcList(adaptor, 1);
  checkGraphConArcList(adaptor, 1);

  checkGraphOutArcList(adaptor, n2, 1);
  checkGraphOutArcList(adaptor, n3, 0);

  checkGraphInArcList(adaptor, n2, 0);
  checkGraphInArcList(adaptor, n3, 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Hide all nodes and arcs
  node_filter[n1] = node_filter[n2] = node_filter[n3] = false;
  arc_filter[a1] = arc_filter[a2] = arc_filter[a3] = false;

  checkGraphNodeList(adaptor, 0);
  checkGraphArcList(adaptor, 0);
  checkGraphConArcList(adaptor, 0);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Check the conversion of nodes and arcs
  Digraph::Node nd = n3;
  nd = n3;
  Adaptor::Node na = n1;
  na = n2;
  Digraph::Arc ad = a3;
  ad = a3;
  Adaptor::Arc aa = a1;
  aa = a2;
}

void checkFilterNodes1() {
  // Check concepts
  checkConcept<concepts::Digraph, FilterNodes<concepts::Digraph> >();
  checkConcept<concepts::Digraph, FilterNodes<ListDigraph> >();
  checkConcept<concepts::AlterableDigraphComponent<>,
               FilterNodes<ListDigraph> >();
  checkConcept<concepts::ExtendableDigraphComponent<>,
               FilterNodes<ListDigraph> >();
  checkConcept<concepts::ErasableDigraphComponent<>,
               FilterNodes<ListDigraph> >();
  checkConcept<concepts::ClearableDigraphComponent<>,
               FilterNodes<ListDigraph> >();

  // Create a digraph and an adaptor
  typedef ListDigraph Digraph;
  typedef Digraph::NodeMap<bool> NodeFilter;
  typedef FilterNodes<Digraph, NodeFilter> Adaptor;

  Digraph digraph;
  NodeFilter node_filter(digraph);
  Adaptor adaptor(digraph, node_filter);

  // Add nodes and arcs to the original digraph and the adaptor
  Digraph::Node n1 = digraph.addNode();
  Digraph::Node n2 = digraph.addNode();
  Adaptor::Node n3 = adaptor.addNode();

  node_filter[n1] = node_filter[n2] = node_filter[n3] = true;

  Digraph::Arc a1 = digraph.addArc(n1, n2);
  Digraph::Arc a2 = digraph.addArc(n1, n3);
  Adaptor::Arc a3 = adaptor.addArc(n2, n3);

  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 3);
  checkGraphConArcList(adaptor, 3);

  checkGraphOutArcList(adaptor, n1, 2);
  checkGraphOutArcList(adaptor, n2, 1);
  checkGraphOutArcList(adaptor, n3, 0);

  checkGraphInArcList(adaptor, n1, 0);
  checkGraphInArcList(adaptor, n2, 1);
  checkGraphInArcList(adaptor, n3, 2);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Hide a node
  adaptor.status(n1, false);
  adaptor.disable(n3);
  if (!adaptor.status(n3)) adaptor.enable(n3);

  checkGraphNodeList(adaptor, 2);
  checkGraphArcList(adaptor, 1);
  checkGraphConArcList(adaptor, 1);

  checkGraphOutArcList(adaptor, n2, 1);
  checkGraphOutArcList(adaptor, n3, 0);

  checkGraphInArcList(adaptor, n2, 0);
  checkGraphInArcList(adaptor, n3, 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Hide all nodes
  node_filter[n1] = node_filter[n2] = node_filter[n3] = false;

  checkGraphNodeList(adaptor, 0);
  checkGraphArcList(adaptor, 0);
  checkGraphConArcList(adaptor, 0);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Check the conversion of nodes and arcs
  Digraph::Node nd = n3;
  nd = n3;
  Adaptor::Node na = n1;
  na = n2;
  Digraph::Arc ad = a3;
  ad = a3;
  Adaptor::Arc aa = a1;
  aa = a2;
}

void checkFilterArcs() {
  // Check concepts
  checkConcept<concepts::Digraph, FilterArcs<concepts::Digraph> >();
  checkConcept<concepts::Digraph, FilterArcs<ListDigraph> >();
  checkConcept<concepts::AlterableDigraphComponent<>,
               FilterArcs<ListDigraph> >();
  checkConcept<concepts::ExtendableDigraphComponent<>,
               FilterArcs<ListDigraph> >();
  checkConcept<concepts::ErasableDigraphComponent<>,
               FilterArcs<ListDigraph> >();
  checkConcept<concepts::ClearableDigraphComponent<>,
               FilterArcs<ListDigraph> >();

  // Create a digraph and an adaptor
  typedef ListDigraph Digraph;
  typedef Digraph::ArcMap<bool> ArcFilter;
  typedef FilterArcs<Digraph, ArcFilter> Adaptor;

  Digraph digraph;
  ArcFilter arc_filter(digraph);
  Adaptor adaptor(digraph, arc_filter);

  // Add nodes and arcs to the original digraph and the adaptor
  Digraph::Node n1 = digraph.addNode();
  Digraph::Node n2 = digraph.addNode();
  Adaptor::Node n3 = adaptor.addNode();

  Digraph::Arc a1 = digraph.addArc(n1, n2);
  Digraph::Arc a2 = digraph.addArc(n1, n3);
  Adaptor::Arc a3 = adaptor.addArc(n2, n3);

  arc_filter[a1] = arc_filter[a2] = arc_filter[a3] = true;

  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 3);
  checkGraphConArcList(adaptor, 3);

  checkGraphOutArcList(adaptor, n1, 2);
  checkGraphOutArcList(adaptor, n2, 1);
  checkGraphOutArcList(adaptor, n3, 0);

  checkGraphInArcList(adaptor, n1, 0);
  checkGraphInArcList(adaptor, n2, 1);
  checkGraphInArcList(adaptor, n3, 2);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Hide an arc
  adaptor.status(a2, false);
  adaptor.disable(a3);
  if (!adaptor.status(a3)) adaptor.enable(a3);

  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 2);
  checkGraphConArcList(adaptor, 2);

  checkGraphOutArcList(adaptor, n1, 1);
  checkGraphOutArcList(adaptor, n2, 1);
  checkGraphOutArcList(adaptor, n3, 0);

  checkGraphInArcList(adaptor, n1, 0);
  checkGraphInArcList(adaptor, n2, 1);
  checkGraphInArcList(adaptor, n3, 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Hide all arcs
  arc_filter[a1] = arc_filter[a2] = arc_filter[a3] = false;

  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 0);
  checkGraphConArcList(adaptor, 0);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Check the conversion of nodes and arcs
  Digraph::Node nd = n3;
  nd = n3;
  Adaptor::Node na = n1;
  na = n2;
  Digraph::Arc ad = a3;
  ad = a3;
  Adaptor::Arc aa = a1;
  aa = a2;
}

void checkUndirector() {
  // Check concepts
  checkConcept<concepts::Graph, Undirector<concepts::Digraph> >();
  checkConcept<concepts::Graph, Undirector<ListDigraph> >();
  checkConcept<concepts::AlterableGraphComponent<>,
               Undirector<ListDigraph> >();
  checkConcept<concepts::ExtendableGraphComponent<>,
               Undirector<ListDigraph> >();
  checkConcept<concepts::ErasableGraphComponent<>,
               Undirector<ListDigraph> >();
  checkConcept<concepts::ClearableGraphComponent<>,
               Undirector<ListDigraph> >();


  // Create a digraph and an adaptor
  typedef ListDigraph Digraph;
  typedef Undirector<Digraph> Adaptor;

  Digraph digraph;
  Adaptor adaptor(digraph);

  // Add nodes and arcs/edges to the original digraph and the adaptor
  Digraph::Node n1 = digraph.addNode();
  Digraph::Node n2 = digraph.addNode();
  Adaptor::Node n3 = adaptor.addNode();

  Digraph::Arc a1 = digraph.addArc(n1, n2);
  Digraph::Arc a2 = digraph.addArc(n1, n3);
  Adaptor::Edge e3 = adaptor.addEdge(n2, n3);

  // Check the original digraph
  checkGraphNodeList(digraph, 3);
  checkGraphArcList(digraph, 3);
  checkGraphConArcList(digraph, 3);

  checkGraphOutArcList(digraph, n1, 2);
  checkGraphOutArcList(digraph, n2, 1);
  checkGraphOutArcList(digraph, n3, 0);

  checkGraphInArcList(digraph, n1, 0);
  checkGraphInArcList(digraph, n2, 1);
  checkGraphInArcList(digraph, n3, 2);

  checkNodeIds(digraph);
  checkArcIds(digraph);

  checkGraphNodeMap(digraph);
  checkGraphArcMap(digraph);

  // Check the adaptor
  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 6);
  checkGraphEdgeList(adaptor, 3);
  checkGraphConArcList(adaptor, 6);
  checkGraphConEdgeList(adaptor, 3);

  checkGraphIncEdgeArcLists(adaptor, n1, 2);
  checkGraphIncEdgeArcLists(adaptor, n2, 2);
  checkGraphIncEdgeArcLists(adaptor, n3, 2);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);
  checkEdgeIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);
  checkGraphEdgeMap(adaptor);

  // Check the edges of the adaptor
  for (Adaptor::EdgeIt e(adaptor); e != INVALID; ++e) {
    check(adaptor.u(e) == digraph.source(e), "Wrong undir");
    check(adaptor.v(e) == digraph.target(e), "Wrong undir");
  }

  // Check CombinedArcMap
  typedef Adaptor::CombinedArcMap
    <Digraph::ArcMap<int>, Digraph::ArcMap<int> > IntCombinedMap;
  typedef Adaptor::CombinedArcMap
    <Digraph::ArcMap<bool>, Digraph::ArcMap<bool> > BoolCombinedMap;
  checkConcept<concepts::ReferenceMap<Adaptor::Arc, int, int&, const int&>,
    IntCombinedMap>();
  checkConcept<concepts::ReferenceMap<Adaptor::Arc, bool, bool&, const bool&>,
    BoolCombinedMap>();

  Digraph::ArcMap<int> fw_map(digraph), bk_map(digraph);
  for (Digraph::ArcIt a(digraph); a != INVALID; ++a) {
    fw_map[a] = digraph.id(a);
    bk_map[a] = -digraph.id(a);
  }

  Adaptor::CombinedArcMap<Digraph::ArcMap<int>, Digraph::ArcMap<int> >
    comb_map(fw_map, bk_map);
  for (Adaptor::ArcIt a(adaptor); a != INVALID; ++a) {
    if (adaptor.source(a) == digraph.source(a)) {
      check(comb_map[a] == fw_map[a], "Wrong combined map");
    } else {
      check(comb_map[a] == bk_map[a], "Wrong combined map");
    }
  }

  // Check the conversion of nodes and arcs/edges
  Digraph::Node nd = n3;
  nd = n3;
  Adaptor::Node na = n1;
  na = n2;
  Digraph::Arc ad = e3;
  ad = e3;
  Adaptor::Edge ea = a1;
  ea = a2;
}

void checkResidualDigraph() {
  // Check concepts
  checkConcept<concepts::Digraph, ResidualDigraph<concepts::Digraph> >();
  checkConcept<concepts::Digraph, ResidualDigraph<ListDigraph> >();

  // Create a digraph and an adaptor
  typedef ListDigraph Digraph;
  typedef Digraph::ArcMap<int> IntArcMap;
  typedef ResidualDigraph<Digraph, IntArcMap> Adaptor;

  Digraph digraph;
  IntArcMap capacity(digraph), flow(digraph);
  Adaptor adaptor(digraph, capacity, flow);

  Digraph::Node n1 = digraph.addNode();
  Digraph::Node n2 = digraph.addNode();
  Digraph::Node n3 = digraph.addNode();
  Digraph::Node n4 = digraph.addNode();

  Digraph::Arc a1 = digraph.addArc(n1, n2);
  Digraph::Arc a2 = digraph.addArc(n1, n3);
  Digraph::Arc a3 = digraph.addArc(n1, n4);
  Digraph::Arc a4 = digraph.addArc(n2, n3);
  Digraph::Arc a5 = digraph.addArc(n2, n4);
  Digraph::Arc a6 = digraph.addArc(n3, n4);

  capacity[a1] = 8;
  capacity[a2] = 6;
  capacity[a3] = 4;
  capacity[a4] = 4;
  capacity[a5] = 6;
  capacity[a6] = 10;

  // Check the adaptor with various flow values
  for (Digraph::ArcIt a(digraph); a != INVALID; ++a) {
    flow[a] = 0;
  }

  checkGraphNodeList(adaptor, 4);
  checkGraphArcList(adaptor, 6);
  checkGraphConArcList(adaptor, 6);

  checkGraphOutArcList(adaptor, n1, 3);
  checkGraphOutArcList(adaptor, n2, 2);
  checkGraphOutArcList(adaptor, n3, 1);
  checkGraphOutArcList(adaptor, n4, 0);

  checkGraphInArcList(adaptor, n1, 0);
  checkGraphInArcList(adaptor, n2, 1);
  checkGraphInArcList(adaptor, n3, 2);
  checkGraphInArcList(adaptor, n4, 3);

  for (Digraph::ArcIt a(digraph); a != INVALID; ++a) {
    flow[a] = capacity[a] / 2;
  }

  checkGraphNodeList(adaptor, 4);
  checkGraphArcList(adaptor, 12);
  checkGraphConArcList(adaptor, 12);

  checkGraphOutArcList(adaptor, n1, 3);
  checkGraphOutArcList(adaptor, n2, 3);
  checkGraphOutArcList(adaptor, n3, 3);
  checkGraphOutArcList(adaptor, n4, 3);

  checkGraphInArcList(adaptor, n1, 3);
  checkGraphInArcList(adaptor, n2, 3);
  checkGraphInArcList(adaptor, n3, 3);
  checkGraphInArcList(adaptor, n4, 3);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  for (Digraph::ArcIt a(digraph); a != INVALID; ++a) {
    flow[a] = capacity[a];
  }

  checkGraphNodeList(adaptor, 4);
  checkGraphArcList(adaptor, 6);
  checkGraphConArcList(adaptor, 6);

  checkGraphOutArcList(adaptor, n1, 0);
  checkGraphOutArcList(adaptor, n2, 1);
  checkGraphOutArcList(adaptor, n3, 2);
  checkGraphOutArcList(adaptor, n4, 3);

  checkGraphInArcList(adaptor, n1, 3);
  checkGraphInArcList(adaptor, n2, 2);
  checkGraphInArcList(adaptor, n3, 1);
  checkGraphInArcList(adaptor, n4, 0);

  // Saturate all backward arcs
  // (set the flow to zero on all forward arcs)
  for (Adaptor::ArcIt a(adaptor); a != INVALID; ++a) {
    if (adaptor.backward(a))
      adaptor.augment(a, adaptor.residualCapacity(a));
  }

  checkGraphNodeList(adaptor, 4);
  checkGraphArcList(adaptor, 6);
  checkGraphConArcList(adaptor, 6);

  checkGraphOutArcList(adaptor, n1, 3);
  checkGraphOutArcList(adaptor, n2, 2);
  checkGraphOutArcList(adaptor, n3, 1);
  checkGraphOutArcList(adaptor, n4, 0);

  checkGraphInArcList(adaptor, n1, 0);
  checkGraphInArcList(adaptor, n2, 1);
  checkGraphInArcList(adaptor, n3, 2);
  checkGraphInArcList(adaptor, n4, 3);

  // Find maximum flow by augmenting along shortest paths
  int flow_value = 0;
  Adaptor::ResidualCapacity res_cap(adaptor);
  while (true) {

    Bfs<Adaptor> bfs(adaptor);
    bfs.run(n1, n4);

    if (!bfs.reached(n4)) break;

    Path<Adaptor> p = bfs.path(n4);

    int min = std::numeric_limits<int>::max();
    for (Path<Adaptor>::ArcIt a(p); a != INVALID; ++a) {
      if (res_cap[a] < min) min = res_cap[a];
    }

    for (Path<Adaptor>::ArcIt a(p); a != INVALID; ++a) {
      adaptor.augment(a, min);
    }
    flow_value += min;
  }

  check(flow_value == 18, "Wrong flow with res graph adaptor");

  // Check forward() and backward()
  for (Adaptor::ArcIt a(adaptor); a != INVALID; ++a) {
    check(adaptor.forward(a) != adaptor.backward(a),
          "Wrong forward() or backward()");
    check((adaptor.forward(a) && adaptor.forward(Digraph::Arc(a)) == a) ||
          (adaptor.backward(a) && adaptor.backward(Digraph::Arc(a)) == a),
          "Wrong forward() or backward()");
  }

  // Check the conversion of nodes and arcs
  Digraph::Node nd = Adaptor::NodeIt(adaptor);
  nd = ++Adaptor::NodeIt(adaptor);
  Adaptor::Node na = n1;
  na = n2;
  Digraph::Arc ad = Adaptor::ArcIt(adaptor);
  ad = ++Adaptor::ArcIt(adaptor);
}

void checkSplitNodes() {
  // Check concepts
  checkConcept<concepts::Digraph, SplitNodes<concepts::Digraph> >();
  checkConcept<concepts::Digraph, SplitNodes<ListDigraph> >();

  // Create a digraph and an adaptor
  typedef ListDigraph Digraph;
  typedef SplitNodes<Digraph> Adaptor;

  Digraph digraph;
  Adaptor adaptor(digraph);

  Digraph::Node n1 = digraph.addNode();
  Digraph::Node n2 = digraph.addNode();
  Digraph::Node n3 = digraph.addNode();

  Digraph::Arc a1 = digraph.addArc(n1, n2);
  Digraph::Arc a2 = digraph.addArc(n1, n3);
  Digraph::Arc a3 = digraph.addArc(n2, n3);
  ::lemon::ignore_unused_variable_warning(a1,a2,a3);

  checkGraphNodeList(adaptor, 6);
  checkGraphArcList(adaptor, 6);
  checkGraphConArcList(adaptor, 6);

  checkGraphOutArcList(adaptor, adaptor.inNode(n1), 1);
  checkGraphOutArcList(adaptor, adaptor.outNode(n1), 2);
  checkGraphOutArcList(adaptor, adaptor.inNode(n2), 1);
  checkGraphOutArcList(adaptor, adaptor.outNode(n2), 1);
  checkGraphOutArcList(adaptor, adaptor.inNode(n3), 1);
  checkGraphOutArcList(adaptor, adaptor.outNode(n3), 0);

  checkGraphInArcList(adaptor, adaptor.inNode(n1), 0);
  checkGraphInArcList(adaptor, adaptor.outNode(n1), 1);
  checkGraphInArcList(adaptor, adaptor.inNode(n2), 1);
  checkGraphInArcList(adaptor, adaptor.outNode(n2), 1);
  checkGraphInArcList(adaptor, adaptor.inNode(n3), 2);
  checkGraphInArcList(adaptor, adaptor.outNode(n3), 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Check split
  for (Adaptor::ArcIt a(adaptor); a != INVALID; ++a) {
    if (adaptor.origArc(a)) {
      Digraph::Arc oa = a;
      check(adaptor.source(a) == adaptor.outNode(digraph.source(oa)),
            "Wrong split");
      check(adaptor.target(a) == adaptor.inNode(digraph.target(oa)),
            "Wrong split");
    } else {
      Digraph::Node on = a;
      check(adaptor.source(a) == adaptor.inNode(on), "Wrong split");
      check(adaptor.target(a) == adaptor.outNode(on), "Wrong split");
    }
  }

  // Check combined node map
  typedef Adaptor::CombinedNodeMap
    <Digraph::NodeMap<int>, Digraph::NodeMap<int> > IntCombinedNodeMap;
  typedef Adaptor::CombinedNodeMap
    <Digraph::NodeMap<bool>, Digraph::NodeMap<bool> > BoolCombinedNodeMap;
  checkConcept<concepts::ReferenceMap<Adaptor::Node, int, int&, const int&>,
    IntCombinedNodeMap>();
//checkConcept<concepts::ReferenceMap<Adaptor::Node, bool, bool&, const bool&>,
//  BoolCombinedNodeMap>();
  checkConcept<concepts::ReadWriteMap<Adaptor::Node, bool>,
    BoolCombinedNodeMap>();

  Digraph::NodeMap<int> in_map(digraph), out_map(digraph);
  for (Digraph::NodeIt n(digraph); n != INVALID; ++n) {
    in_map[n] = digraph.id(n);
    out_map[n] = -digraph.id(n);
  }

  Adaptor::CombinedNodeMap<Digraph::NodeMap<int>, Digraph::NodeMap<int> >
    node_map(in_map, out_map);
  for (Adaptor::NodeIt n(adaptor); n != INVALID; ++n) {
    if (adaptor.inNode(n)) {
      check(node_map[n] == in_map[n], "Wrong combined node map");
    } else {
      check(node_map[n] == out_map[n], "Wrong combined node map");
    }
  }

  // Check combined arc map
  typedef Adaptor::CombinedArcMap
    <Digraph::ArcMap<int>, Digraph::NodeMap<int> > IntCombinedArcMap;
  typedef Adaptor::CombinedArcMap
    <Digraph::ArcMap<bool>, Digraph::NodeMap<bool> > BoolCombinedArcMap;
  checkConcept<concepts::ReferenceMap<Adaptor::Arc, int, int&, const int&>,
    IntCombinedArcMap>();
//checkConcept<concepts::ReferenceMap<Adaptor::Arc, bool, bool&, const bool&>,
//  BoolCombinedArcMap>();
  checkConcept<concepts::ReadWriteMap<Adaptor::Arc, bool>,
    BoolCombinedArcMap>();

  Digraph::ArcMap<int> a_map(digraph);
  for (Digraph::ArcIt a(digraph); a != INVALID; ++a) {
    a_map[a] = digraph.id(a);
  }

  Adaptor::CombinedArcMap<Digraph::ArcMap<int>, Digraph::NodeMap<int> >
    arc_map(a_map, out_map);
  for (Digraph::ArcIt a(digraph); a != INVALID; ++a) {
    check(arc_map[adaptor.arc(a)] == a_map[a],  "Wrong combined arc map");
  }
  for (Digraph::NodeIt n(digraph); n != INVALID; ++n) {
    check(arc_map[adaptor.arc(n)] == out_map[n],  "Wrong combined arc map");
  }

  // Check the conversion of nodes
  Digraph::Node nd = adaptor.inNode(n1);
  check (nd == n1, "Wrong node conversion");
  nd = adaptor.outNode(n2);
  check (nd == n2, "Wrong node conversion");
}

void checkSubGraph() {
  // Check concepts
  checkConcept<concepts::Graph, SubGraph<concepts::Graph> >();
  checkConcept<concepts::Graph, SubGraph<ListGraph> >();
  checkConcept<concepts::AlterableGraphComponent<>,
               SubGraph<ListGraph> >();
  checkConcept<concepts::ExtendableGraphComponent<>,
               SubGraph<ListGraph> >();
  checkConcept<concepts::ErasableGraphComponent<>,
               SubGraph<ListGraph> >();
  checkConcept<concepts::ClearableGraphComponent<>,
               SubGraph<ListGraph> >();

  // Create a graph and an adaptor
  typedef ListGraph Graph;
  typedef Graph::NodeMap<bool> NodeFilter;
  typedef Graph::EdgeMap<bool> EdgeFilter;
  typedef SubGraph<Graph, NodeFilter, EdgeFilter> Adaptor;

  Graph graph;
  NodeFilter node_filter(graph);
  EdgeFilter edge_filter(graph);
  Adaptor adaptor(graph, node_filter, edge_filter);

  // Add nodes and edges to the original graph and the adaptor
  Graph::Node n1 = graph.addNode();
  Graph::Node n2 = graph.addNode();
  Adaptor::Node n3 = adaptor.addNode();
  Adaptor::Node n4 = adaptor.addNode();

  node_filter[n1] = node_filter[n2] = node_filter[n3] = node_filter[n4] = true;

  Graph::Edge e1 = graph.addEdge(n1, n2);
  Graph::Edge e2 = graph.addEdge(n1, n3);
  Adaptor::Edge e3 = adaptor.addEdge(n2, n3);
  Adaptor::Edge e4 = adaptor.addEdge(n3, n4);

  edge_filter[e1] = edge_filter[e2] = edge_filter[e3] = edge_filter[e4] = true;

  checkGraphNodeList(adaptor, 4);
  checkGraphArcList(adaptor, 8);
  checkGraphEdgeList(adaptor, 4);
  checkGraphConArcList(adaptor, 8);
  checkGraphConEdgeList(adaptor, 4);

  checkGraphIncEdgeArcLists(adaptor, n1, 2);
  checkGraphIncEdgeArcLists(adaptor, n2, 2);
  checkGraphIncEdgeArcLists(adaptor, n3, 3);
  checkGraphIncEdgeArcLists(adaptor, n4, 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);
  checkEdgeIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);
  checkGraphEdgeMap(adaptor);

  // Hide an edge
  adaptor.status(e2, false);
  adaptor.disable(e3);
  if (!adaptor.status(e3)) adaptor.enable(e3);

  checkGraphNodeList(adaptor, 4);
  checkGraphArcList(adaptor, 6);
  checkGraphEdgeList(adaptor, 3);
  checkGraphConArcList(adaptor, 6);
  checkGraphConEdgeList(adaptor, 3);

  checkGraphIncEdgeArcLists(adaptor, n1, 1);
  checkGraphIncEdgeArcLists(adaptor, n2, 2);
  checkGraphIncEdgeArcLists(adaptor, n3, 2);
  checkGraphIncEdgeArcLists(adaptor, n4, 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);
  checkEdgeIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);
  checkGraphEdgeMap(adaptor);

  // Hide a node
  adaptor.status(n1, false);
  adaptor.disable(n3);
  if (!adaptor.status(n3)) adaptor.enable(n3);

  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 4);
  checkGraphEdgeList(adaptor, 2);
  checkGraphConArcList(adaptor, 4);
  checkGraphConEdgeList(adaptor, 2);

  checkGraphIncEdgeArcLists(adaptor, n2, 1);
  checkGraphIncEdgeArcLists(adaptor, n3, 2);
  checkGraphIncEdgeArcLists(adaptor, n4, 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);
  checkEdgeIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);
  checkGraphEdgeMap(adaptor);

  // Hide all nodes and edges
  node_filter[n1] = node_filter[n2] = node_filter[n3] = node_filter[n4] = false;
  edge_filter[e1] = edge_filter[e2] = edge_filter[e3] = edge_filter[e4] = false;

  checkGraphNodeList(adaptor, 0);
  checkGraphArcList(adaptor, 0);
  checkGraphEdgeList(adaptor, 0);
  checkGraphConArcList(adaptor, 0);
  checkGraphConEdgeList(adaptor, 0);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);
  checkEdgeIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);
  checkGraphEdgeMap(adaptor);

  // Check the conversion of nodes and edges
  Graph::Node ng = n3;
  ng = n4;
  Adaptor::Node na = n1;
  na = n2;
  Graph::Edge eg = e3;
  eg = e4;
  Adaptor::Edge ea = e1;
  ea = e2;
}

void checkFilterNodes2() {
  // Check concepts
  checkConcept<concepts::Graph, FilterNodes<concepts::Graph> >();
  checkConcept<concepts::Graph, FilterNodes<ListGraph> >();
  checkConcept<concepts::AlterableGraphComponent<>,
               FilterNodes<ListGraph> >();
  checkConcept<concepts::ExtendableGraphComponent<>,
               FilterNodes<ListGraph> >();
  checkConcept<concepts::ErasableGraphComponent<>,
               FilterNodes<ListGraph> >();
  checkConcept<concepts::ClearableGraphComponent<>,
               FilterNodes<ListGraph> >();

  // Create a graph and an adaptor
  typedef ListGraph Graph;
  typedef Graph::NodeMap<bool> NodeFilter;
  typedef FilterNodes<Graph, NodeFilter> Adaptor;

  // Add nodes and edges to the original graph and the adaptor
  Graph graph;
  NodeFilter node_filter(graph);
  Adaptor adaptor(graph, node_filter);

  Graph::Node n1 = graph.addNode();
  Graph::Node n2 = graph.addNode();
  Adaptor::Node n3 = adaptor.addNode();
  Adaptor::Node n4 = adaptor.addNode();

  node_filter[n1] = node_filter[n2] = node_filter[n3] = node_filter[n4] = true;

  Graph::Edge e1 = graph.addEdge(n1, n2);
  Graph::Edge e2 = graph.addEdge(n1, n3);
  Adaptor::Edge e3 = adaptor.addEdge(n2, n3);
  Adaptor::Edge e4 = adaptor.addEdge(n3, n4);

  checkGraphNodeList(adaptor, 4);
  checkGraphArcList(adaptor, 8);
  checkGraphEdgeList(adaptor, 4);
  checkGraphConArcList(adaptor, 8);
  checkGraphConEdgeList(adaptor, 4);

  checkGraphIncEdgeArcLists(adaptor, n1, 2);
  checkGraphIncEdgeArcLists(adaptor, n2, 2);
  checkGraphIncEdgeArcLists(adaptor, n3, 3);
  checkGraphIncEdgeArcLists(adaptor, n4, 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);
  checkEdgeIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);
  checkGraphEdgeMap(adaptor);

  // Hide a node
  adaptor.status(n1, false);
  adaptor.disable(n3);
  if (!adaptor.status(n3)) adaptor.enable(n3);

  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 4);
  checkGraphEdgeList(adaptor, 2);
  checkGraphConArcList(adaptor, 4);
  checkGraphConEdgeList(adaptor, 2);

  checkGraphIncEdgeArcLists(adaptor, n2, 1);
  checkGraphIncEdgeArcLists(adaptor, n3, 2);
  checkGraphIncEdgeArcLists(adaptor, n4, 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);
  checkEdgeIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);
  checkGraphEdgeMap(adaptor);

  // Hide all nodes
  node_filter[n1] = node_filter[n2] = node_filter[n3] = node_filter[n4] = false;

  checkGraphNodeList(adaptor, 0);
  checkGraphArcList(adaptor, 0);
  checkGraphEdgeList(adaptor, 0);
  checkGraphConArcList(adaptor, 0);
  checkGraphConEdgeList(adaptor, 0);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);
  checkEdgeIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);
  checkGraphEdgeMap(adaptor);

  // Check the conversion of nodes and edges
  Graph::Node ng = n3;
  ng = n4;
  Adaptor::Node na = n1;
  na = n2;
  Graph::Edge eg = e3;
  eg = e4;
  Adaptor::Edge ea = e1;
  ea = e2;
}

void checkFilterEdges() {
  // Check concepts
  checkConcept<concepts::Graph, FilterEdges<concepts::Graph> >();
  checkConcept<concepts::Graph, FilterEdges<ListGraph> >();
  checkConcept<concepts::AlterableGraphComponent<>,
               FilterEdges<ListGraph> >();
  checkConcept<concepts::ExtendableGraphComponent<>,
               FilterEdges<ListGraph> >();
  checkConcept<concepts::ErasableGraphComponent<>,
               FilterEdges<ListGraph> >();
  checkConcept<concepts::ClearableGraphComponent<>,
               FilterEdges<ListGraph> >();

  // Create a graph and an adaptor
  typedef ListGraph Graph;
  typedef Graph::EdgeMap<bool> EdgeFilter;
  typedef FilterEdges<Graph, EdgeFilter> Adaptor;

  Graph graph;
  EdgeFilter edge_filter(graph);
  Adaptor adaptor(graph, edge_filter);

  // Add nodes and edges to the original graph and the adaptor
  Graph::Node n1 = graph.addNode();
  Graph::Node n2 = graph.addNode();
  Adaptor::Node n3 = adaptor.addNode();
  Adaptor::Node n4 = adaptor.addNode();

  Graph::Edge e1 = graph.addEdge(n1, n2);
  Graph::Edge e2 = graph.addEdge(n1, n3);
  Adaptor::Edge e3 = adaptor.addEdge(n2, n3);
  Adaptor::Edge e4 = adaptor.addEdge(n3, n4);

  edge_filter[e1] = edge_filter[e2] = edge_filter[e3] = edge_filter[e4] = true;

  checkGraphNodeList(adaptor, 4);
  checkGraphArcList(adaptor, 8);
  checkGraphEdgeList(adaptor, 4);
  checkGraphConArcList(adaptor, 8);
  checkGraphConEdgeList(adaptor, 4);

  checkGraphIncEdgeArcLists(adaptor, n1, 2);
  checkGraphIncEdgeArcLists(adaptor, n2, 2);
  checkGraphIncEdgeArcLists(adaptor, n3, 3);
  checkGraphIncEdgeArcLists(adaptor, n4, 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);
  checkEdgeIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);
  checkGraphEdgeMap(adaptor);

  // Hide an edge
  adaptor.status(e2, false);
  adaptor.disable(e3);
  if (!adaptor.status(e3)) adaptor.enable(e3);

  checkGraphNodeList(adaptor, 4);
  checkGraphArcList(adaptor, 6);
  checkGraphEdgeList(adaptor, 3);
  checkGraphConArcList(adaptor, 6);
  checkGraphConEdgeList(adaptor, 3);

  checkGraphIncEdgeArcLists(adaptor, n1, 1);
  checkGraphIncEdgeArcLists(adaptor, n2, 2);
  checkGraphIncEdgeArcLists(adaptor, n3, 2);
  checkGraphIncEdgeArcLists(adaptor, n4, 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);
  checkEdgeIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);
  checkGraphEdgeMap(adaptor);

  // Hide all edges
  edge_filter[e1] = edge_filter[e2] = edge_filter[e3] = edge_filter[e4] = false;

  checkGraphNodeList(adaptor, 4);
  checkGraphArcList(adaptor, 0);
  checkGraphEdgeList(adaptor, 0);
  checkGraphConArcList(adaptor, 0);
  checkGraphConEdgeList(adaptor, 0);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);
  checkEdgeIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);
  checkGraphEdgeMap(adaptor);

  // Check the conversion of nodes and edges
  Graph::Node ng = n3;
  ng = n4;
  Adaptor::Node na = n1;
  na = n2;
  Graph::Edge eg = e3;
  eg = e4;
  Adaptor::Edge ea = e1;
  ea = e2;
}

void checkOrienter() {
  // Check concepts
  checkConcept<concepts::Digraph, Orienter<concepts::Graph> >();
  checkConcept<concepts::Digraph, Orienter<ListGraph> >();
  checkConcept<concepts::AlterableDigraphComponent<>,
               Orienter<ListGraph> >();
  checkConcept<concepts::ExtendableDigraphComponent<>,
               Orienter<ListGraph> >();
  checkConcept<concepts::ErasableDigraphComponent<>,
               Orienter<ListGraph> >();
  checkConcept<concepts::ClearableDigraphComponent<>,
               Orienter<ListGraph> >();

  // Create a graph and an adaptor
  typedef ListGraph Graph;
  typedef ListGraph::EdgeMap<bool> DirMap;
  typedef Orienter<Graph> Adaptor;

  Graph graph;
  DirMap dir(graph);
  Adaptor adaptor(graph, dir);

  // Add nodes and edges to the original graph and the adaptor
  Graph::Node n1 = graph.addNode();
  Graph::Node n2 = graph.addNode();
  Adaptor::Node n3 = adaptor.addNode();

  Graph::Edge e1 = graph.addEdge(n1, n2);
  Graph::Edge e2 = graph.addEdge(n1, n3);
  Adaptor::Arc e3 = adaptor.addArc(n2, n3);

  dir[e1] = dir[e2] = dir[e3] = true;

  // Check the original graph
  checkGraphNodeList(graph, 3);
  checkGraphArcList(graph, 6);
  checkGraphConArcList(graph, 6);
  checkGraphEdgeList(graph, 3);
  checkGraphConEdgeList(graph, 3);

  checkGraphIncEdgeArcLists(graph, n1, 2);
  checkGraphIncEdgeArcLists(graph, n2, 2);
  checkGraphIncEdgeArcLists(graph, n3, 2);

  checkNodeIds(graph);
  checkArcIds(graph);
  checkEdgeIds(graph);

  checkGraphNodeMap(graph);
  checkGraphArcMap(graph);
  checkGraphEdgeMap(graph);

  // Check the adaptor
  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 3);
  checkGraphConArcList(adaptor, 3);

  checkGraphOutArcList(adaptor, n1, 2);
  checkGraphOutArcList(adaptor, n2, 1);
  checkGraphOutArcList(adaptor, n3, 0);

  checkGraphInArcList(adaptor, n1, 0);
  checkGraphInArcList(adaptor, n2, 1);
  checkGraphInArcList(adaptor, n3, 2);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Check direction changing
  {
    dir[e1] = true;
    Adaptor::Node u = adaptor.source(e1);
    Adaptor::Node v = adaptor.target(e1);

    dir[e1] = false;
    check (u == adaptor.target(e1), "Wrong dir");
    check (v == adaptor.source(e1), "Wrong dir");

    check ((u == n1 && v == n2) || (u == n2 && v == n1), "Wrong dir");
    dir[e1] = n1 == u;
  }

  {
    dir[e2] = true;
    Adaptor::Node u = adaptor.source(e2);
    Adaptor::Node v = adaptor.target(e2);

    dir[e2] = false;
    check (u == adaptor.target(e2), "Wrong dir");
    check (v == adaptor.source(e2), "Wrong dir");

    check ((u == n1 && v == n3) || (u == n3 && v == n1), "Wrong dir");
    dir[e2] = n3 == u;
  }

  {
    dir[e3] = true;
    Adaptor::Node u = adaptor.source(e3);
    Adaptor::Node v = adaptor.target(e3);

    dir[e3] = false;
    check (u == adaptor.target(e3), "Wrong dir");
    check (v == adaptor.source(e3), "Wrong dir");

    check ((u == n2 && v == n3) || (u == n3 && v == n2), "Wrong dir");
    dir[e3] = n2 == u;
  }

  // Check the adaptor again
  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 3);
  checkGraphConArcList(adaptor, 3);

  checkGraphOutArcList(adaptor, n1, 1);
  checkGraphOutArcList(adaptor, n2, 1);
  checkGraphOutArcList(adaptor, n3, 1);

  checkGraphInArcList(adaptor, n1, 1);
  checkGraphInArcList(adaptor, n2, 1);
  checkGraphInArcList(adaptor, n3, 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Check reverseArc()
  adaptor.reverseArc(e2);
  adaptor.reverseArc(e3);
  adaptor.reverseArc(e2);

  checkGraphNodeList(adaptor, 3);
  checkGraphArcList(adaptor, 3);
  checkGraphConArcList(adaptor, 3);

  checkGraphOutArcList(adaptor, n1, 1);
  checkGraphOutArcList(adaptor, n2, 0);
  checkGraphOutArcList(adaptor, n3, 2);

  checkGraphInArcList(adaptor, n1, 1);
  checkGraphInArcList(adaptor, n2, 2);
  checkGraphInArcList(adaptor, n3, 0);

  // Check the conversion of nodes and arcs/edges
  Graph::Node ng = n3;
  ng = n3;
  Adaptor::Node na = n1;
  na = n2;
  Graph::Edge eg = e3;
  eg = e3;
  Adaptor::Arc aa = e1;
  aa = e2;
}

void checkCombiningAdaptors() {
  // Create a grid graph
  GridGraph graph(2,2);
  GridGraph::Node n1 = graph(0,0);
  GridGraph::Node n2 = graph(0,1);
  GridGraph::Node n3 = graph(1,0);
  GridGraph::Node n4 = graph(1,1);

  GridGraph::EdgeMap<bool> dir_map(graph);
  dir_map[graph.right(n1)] = graph.u(graph.right(n1)) != n1;
  dir_map[graph.up(n1)] = graph.u(graph.up(n1)) == n1;
  dir_map[graph.left(n4)] = graph.u(graph.left(n4)) == n4;
  dir_map[graph.down(n4)] = graph.u(graph.down(n4)) == n4;

  // Apply several adaptors on the grid graph
  typedef Orienter< const GridGraph, GridGraph::EdgeMap<bool> >
    OrientedGridGraph;
  typedef SplitNodes<OrientedGridGraph> SplitGridGraph;
  typedef Undirector<const SplitGridGraph> USplitGridGraph;
  checkConcept<concepts::Digraph, SplitGridGraph>();
  checkConcept<concepts::Graph, USplitGridGraph>();

  OrientedGridGraph oadaptor = orienter(graph, dir_map);
  SplitGridGraph adaptor = splitNodes(oadaptor);
  USplitGridGraph uadaptor = undirector(adaptor);

  // Check adaptor
  checkGraphNodeList(adaptor, 8);
  checkGraphArcList(adaptor, 8);
  checkGraphConArcList(adaptor, 8);

  checkGraphOutArcList(adaptor, adaptor.inNode(n1), 1);
  checkGraphOutArcList(adaptor, adaptor.outNode(n1), 1);
  checkGraphOutArcList(adaptor, adaptor.inNode(n2), 1);
  checkGraphOutArcList(adaptor, adaptor.outNode(n2), 0);
  checkGraphOutArcList(adaptor, adaptor.inNode(n3), 1);
  checkGraphOutArcList(adaptor, adaptor.outNode(n3), 1);
  checkGraphOutArcList(adaptor, adaptor.inNode(n4), 1);
  checkGraphOutArcList(adaptor, adaptor.outNode(n4), 2);

  checkGraphInArcList(adaptor, adaptor.inNode(n1), 1);
  checkGraphInArcList(adaptor, adaptor.outNode(n1), 1);
  checkGraphInArcList(adaptor, adaptor.inNode(n2), 2);
  checkGraphInArcList(adaptor, adaptor.outNode(n2), 1);
  checkGraphInArcList(adaptor, adaptor.inNode(n3), 1);
  checkGraphInArcList(adaptor, adaptor.outNode(n3), 1);
  checkGraphInArcList(adaptor, adaptor.inNode(n4), 0);
  checkGraphInArcList(adaptor, adaptor.outNode(n4), 1);

  checkNodeIds(adaptor);
  checkArcIds(adaptor);

  checkGraphNodeMap(adaptor);
  checkGraphArcMap(adaptor);

  // Check uadaptor
  checkGraphNodeList(uadaptor, 8);
  checkGraphEdgeList(uadaptor, 8);
  checkGraphArcList(uadaptor, 16);
  checkGraphConEdgeList(uadaptor, 8);
  checkGraphConArcList(uadaptor, 16);

  checkNodeIds(uadaptor);
  checkEdgeIds(uadaptor);
  checkArcIds(uadaptor);

  checkGraphNodeMap(uadaptor);
  checkGraphEdgeMap(uadaptor);
  checkGraphArcMap(uadaptor);

  checkGraphIncEdgeArcLists(uadaptor, adaptor.inNode(n1), 2);
  checkGraphIncEdgeArcLists(uadaptor, adaptor.outNode(n1), 2);
  checkGraphIncEdgeArcLists(uadaptor, adaptor.inNode(n2), 3);
  checkGraphIncEdgeArcLists(uadaptor, adaptor.outNode(n2), 1);
  checkGraphIncEdgeArcLists(uadaptor, adaptor.inNode(n3), 2);
  checkGraphIncEdgeArcLists(uadaptor, adaptor.outNode(n3), 2);
  checkGraphIncEdgeArcLists(uadaptor, adaptor.inNode(n4), 1);
  checkGraphIncEdgeArcLists(uadaptor, adaptor.outNode(n4), 3);
}

int main(int, const char **) {
  // Check the digraph adaptors (using ListDigraph)
  checkReverseDigraph();
  checkSubDigraph();
  checkFilterNodes1();
  checkFilterArcs();
  checkUndirector();
  checkResidualDigraph();
  checkSplitNodes();

  // Check the graph adaptors (using ListGraph)
  checkSubGraph();
  checkFilterNodes2();
  checkFilterEdges();
  checkOrienter();

  // Combine adaptors (using GridGraph)
  checkCombiningAdaptors();

  return 0;
}
