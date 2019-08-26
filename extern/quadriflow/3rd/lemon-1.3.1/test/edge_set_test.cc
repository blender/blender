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
#include <vector>

#include <lemon/concepts/digraph.h>
#include <lemon/concepts/graph.h>
#include <lemon/concept_check.h>

#include <lemon/list_graph.h>

#include <lemon/edge_set.h>

#include "graph_test.h"
#include "test_tools.h"

using namespace lemon;

void checkSmartArcSet() {
  checkConcept<concepts::Digraph, SmartArcSet<ListDigraph> >();

  typedef ListDigraph Digraph;
  typedef SmartArcSet<Digraph> ArcSet;

  Digraph digraph;
  Digraph::Node
    n1 = digraph.addNode(),
    n2 = digraph.addNode();

  Digraph::Arc ga1 = digraph.addArc(n1, n2);
  ::lemon::ignore_unused_variable_warning(ga1);

  ArcSet arc_set(digraph);

  Digraph::Arc ga2 = digraph.addArc(n2, n1);
  ::lemon::ignore_unused_variable_warning(ga2);

  checkGraphNodeList(arc_set, 2);
  checkGraphArcList(arc_set, 0);

  Digraph::Node
    n3 = digraph.addNode();
  checkGraphNodeList(arc_set, 3);
  checkGraphArcList(arc_set, 0);

  ArcSet::Arc a1 = arc_set.addArc(n1, n2);
  check(arc_set.source(a1) == n1 && arc_set.target(a1) == n2, "Wrong arc");
  checkGraphNodeList(arc_set, 3);
  checkGraphArcList(arc_set, 1);

  checkGraphOutArcList(arc_set, n1, 1);
  checkGraphOutArcList(arc_set, n2, 0);
  checkGraphOutArcList(arc_set, n3, 0);

  checkGraphInArcList(arc_set, n1, 0);
  checkGraphInArcList(arc_set, n2, 1);
  checkGraphInArcList(arc_set, n3, 0);

  checkGraphConArcList(arc_set, 1);

  ArcSet::Arc a2 = arc_set.addArc(n2, n1),
    a3 = arc_set.addArc(n2, n3),
    a4 = arc_set.addArc(n2, n3);
  ::lemon::ignore_unused_variable_warning(a2,a3,a4);

  checkGraphNodeList(arc_set, 3);
  checkGraphArcList(arc_set, 4);

  checkGraphOutArcList(arc_set, n1, 1);
  checkGraphOutArcList(arc_set, n2, 3);
  checkGraphOutArcList(arc_set, n3, 0);

  checkGraphInArcList(arc_set, n1, 1);
  checkGraphInArcList(arc_set, n2, 1);
  checkGraphInArcList(arc_set, n3, 2);

  checkGraphConArcList(arc_set, 4);

  checkNodeIds(arc_set);
  checkArcIds(arc_set);
  checkGraphNodeMap(arc_set);
  checkGraphArcMap(arc_set);

  check(arc_set.valid(), "Wrong validity");
  digraph.erase(n1);
  check(!arc_set.valid(), "Wrong validity");
}

void checkListArcSet() {
  checkConcept<concepts::Digraph, SmartArcSet<ListDigraph> >();

  typedef ListDigraph Digraph;
  typedef ListArcSet<Digraph> ArcSet;

  Digraph digraph;
  Digraph::Node
    n1 = digraph.addNode(),
    n2 = digraph.addNode();

  Digraph::Arc ga1 = digraph.addArc(n1, n2);
  ::lemon::ignore_unused_variable_warning(ga1);

  ArcSet arc_set(digraph);

  Digraph::Arc ga2 = digraph.addArc(n2, n1);
  ::lemon::ignore_unused_variable_warning(ga2);

  checkGraphNodeList(arc_set, 2);
  checkGraphArcList(arc_set, 0);

  Digraph::Node
    n3 = digraph.addNode();
  checkGraphNodeList(arc_set, 3);
  checkGraphArcList(arc_set, 0);

  ArcSet::Arc a1 = arc_set.addArc(n1, n2);
  check(arc_set.source(a1) == n1 && arc_set.target(a1) == n2, "Wrong arc");
  checkGraphNodeList(arc_set, 3);
  checkGraphArcList(arc_set, 1);

  checkGraphOutArcList(arc_set, n1, 1);
  checkGraphOutArcList(arc_set, n2, 0);
  checkGraphOutArcList(arc_set, n3, 0);

  checkGraphInArcList(arc_set, n1, 0);
  checkGraphInArcList(arc_set, n2, 1);
  checkGraphInArcList(arc_set, n3, 0);

  checkGraphConArcList(arc_set, 1);

  ArcSet::Arc a2 = arc_set.addArc(n2, n1),
    a3 = arc_set.addArc(n2, n3),
    a4 = arc_set.addArc(n2, n3);
  ::lemon::ignore_unused_variable_warning(a2,a3,a4);

  checkGraphNodeList(arc_set, 3);
  checkGraphArcList(arc_set, 4);

  checkGraphOutArcList(arc_set, n1, 1);
  checkGraphOutArcList(arc_set, n2, 3);
  checkGraphOutArcList(arc_set, n3, 0);

  checkGraphInArcList(arc_set, n1, 1);
  checkGraphInArcList(arc_set, n2, 1);
  checkGraphInArcList(arc_set, n3, 2);

  checkGraphConArcList(arc_set, 4);

  checkNodeIds(arc_set);
  checkArcIds(arc_set);
  checkGraphNodeMap(arc_set);
  checkGraphArcMap(arc_set);

  digraph.erase(n1);

  checkGraphNodeList(arc_set, 2);
  checkGraphArcList(arc_set, 2);

  checkGraphOutArcList(arc_set, n2, 2);
  checkGraphOutArcList(arc_set, n3, 0);

  checkGraphInArcList(arc_set, n2, 0);
  checkGraphInArcList(arc_set, n3, 2);

  checkNodeIds(arc_set);
  checkArcIds(arc_set);
  checkGraphNodeMap(arc_set);
  checkGraphArcMap(arc_set);

  checkGraphConArcList(arc_set, 2);
}

void checkSmartEdgeSet() {
  checkConcept<concepts::Digraph, SmartEdgeSet<ListDigraph> >();

  typedef ListDigraph Digraph;
  typedef SmartEdgeSet<Digraph> EdgeSet;

  Digraph digraph;
  Digraph::Node
    n1 = digraph.addNode(),
    n2 = digraph.addNode();

  Digraph::Arc ga1 = digraph.addArc(n1, n2);
  ::lemon::ignore_unused_variable_warning(ga1);

  EdgeSet edge_set(digraph);

  Digraph::Arc ga2 = digraph.addArc(n2, n1);
  ::lemon::ignore_unused_variable_warning(ga2);

  checkGraphNodeList(edge_set, 2);
  checkGraphArcList(edge_set, 0);
  checkGraphEdgeList(edge_set, 0);

  Digraph::Node
    n3 = digraph.addNode();
  checkGraphNodeList(edge_set, 3);
  checkGraphArcList(edge_set, 0);
  checkGraphEdgeList(edge_set, 0);

  EdgeSet::Edge e1 = edge_set.addEdge(n1, n2);
  check((edge_set.u(e1) == n1 && edge_set.v(e1) == n2) ||
        (edge_set.v(e1) == n1 && edge_set.u(e1) == n2), "Wrong edge");
  checkGraphNodeList(edge_set, 3);
  checkGraphArcList(edge_set, 2);
  checkGraphEdgeList(edge_set, 1);

  checkGraphOutArcList(edge_set, n1, 1);
  checkGraphOutArcList(edge_set, n2, 1);
  checkGraphOutArcList(edge_set, n3, 0);

  checkGraphInArcList(edge_set, n1, 1);
  checkGraphInArcList(edge_set, n2, 1);
  checkGraphInArcList(edge_set, n3, 0);

  checkGraphIncEdgeList(edge_set, n1, 1);
  checkGraphIncEdgeList(edge_set, n2, 1);
  checkGraphIncEdgeList(edge_set, n3, 0);

  checkGraphConEdgeList(edge_set, 1);
  checkGraphConArcList(edge_set, 2);

  EdgeSet::Edge e2 = edge_set.addEdge(n2, n1),
    e3 = edge_set.addEdge(n2, n3),
    e4 = edge_set.addEdge(n2, n3);
  ::lemon::ignore_unused_variable_warning(e2,e3,e4);

  checkGraphNodeList(edge_set, 3);
  checkGraphEdgeList(edge_set, 4);

  checkGraphOutArcList(edge_set, n1, 2);
  checkGraphOutArcList(edge_set, n2, 4);
  checkGraphOutArcList(edge_set, n3, 2);

  checkGraphInArcList(edge_set, n1, 2);
  checkGraphInArcList(edge_set, n2, 4);
  checkGraphInArcList(edge_set, n3, 2);

  checkGraphIncEdgeList(edge_set, n1, 2);
  checkGraphIncEdgeList(edge_set, n2, 4);
  checkGraphIncEdgeList(edge_set, n3, 2);

  checkGraphConEdgeList(edge_set, 4);
  checkGraphConArcList(edge_set, 8);

  checkArcDirections(edge_set);

  checkNodeIds(edge_set);
  checkArcIds(edge_set);
  checkEdgeIds(edge_set);
  checkGraphNodeMap(edge_set);
  checkGraphArcMap(edge_set);
  checkGraphEdgeMap(edge_set);

  check(edge_set.valid(), "Wrong validity");
  digraph.erase(n1);
  check(!edge_set.valid(), "Wrong validity");
}

void checkListEdgeSet() {
  checkConcept<concepts::Digraph, ListEdgeSet<ListDigraph> >();

  typedef ListDigraph Digraph;
  typedef ListEdgeSet<Digraph> EdgeSet;

  Digraph digraph;
  Digraph::Node
    n1 = digraph.addNode(),
    n2 = digraph.addNode();

  Digraph::Arc ga1 = digraph.addArc(n1, n2);
  ::lemon::ignore_unused_variable_warning(ga1);

  EdgeSet edge_set(digraph);

  Digraph::Arc ga2 = digraph.addArc(n2, n1);
  ::lemon::ignore_unused_variable_warning(ga2);

  checkGraphNodeList(edge_set, 2);
  checkGraphArcList(edge_set, 0);
  checkGraphEdgeList(edge_set, 0);

  Digraph::Node
    n3 = digraph.addNode();
  checkGraphNodeList(edge_set, 3);
  checkGraphArcList(edge_set, 0);
  checkGraphEdgeList(edge_set, 0);

  EdgeSet::Edge e1 = edge_set.addEdge(n1, n2);
  check((edge_set.u(e1) == n1 && edge_set.v(e1) == n2) ||
        (edge_set.v(e1) == n1 && edge_set.u(e1) == n2), "Wrong edge");
  checkGraphNodeList(edge_set, 3);
  checkGraphArcList(edge_set, 2);
  checkGraphEdgeList(edge_set, 1);

  checkGraphOutArcList(edge_set, n1, 1);
  checkGraphOutArcList(edge_set, n2, 1);
  checkGraphOutArcList(edge_set, n3, 0);

  checkGraphInArcList(edge_set, n1, 1);
  checkGraphInArcList(edge_set, n2, 1);
  checkGraphInArcList(edge_set, n3, 0);

  checkGraphIncEdgeList(edge_set, n1, 1);
  checkGraphIncEdgeList(edge_set, n2, 1);
  checkGraphIncEdgeList(edge_set, n3, 0);

  checkGraphConEdgeList(edge_set, 1);
  checkGraphConArcList(edge_set, 2);

  EdgeSet::Edge e2 = edge_set.addEdge(n2, n1),
    e3 = edge_set.addEdge(n2, n3),
    e4 = edge_set.addEdge(n2, n3);
  ::lemon::ignore_unused_variable_warning(e2,e3,e4);

  checkGraphNodeList(edge_set, 3);
  checkGraphEdgeList(edge_set, 4);

  checkGraphOutArcList(edge_set, n1, 2);
  checkGraphOutArcList(edge_set, n2, 4);
  checkGraphOutArcList(edge_set, n3, 2);

  checkGraphInArcList(edge_set, n1, 2);
  checkGraphInArcList(edge_set, n2, 4);
  checkGraphInArcList(edge_set, n3, 2);

  checkGraphIncEdgeList(edge_set, n1, 2);
  checkGraphIncEdgeList(edge_set, n2, 4);
  checkGraphIncEdgeList(edge_set, n3, 2);

  checkGraphConEdgeList(edge_set, 4);
  checkGraphConArcList(edge_set, 8);

  checkArcDirections(edge_set);

  checkNodeIds(edge_set);
  checkArcIds(edge_set);
  checkEdgeIds(edge_set);
  checkGraphNodeMap(edge_set);
  checkGraphArcMap(edge_set);
  checkGraphEdgeMap(edge_set);

  digraph.erase(n1);

  checkGraphNodeList(edge_set, 2);
  checkGraphArcList(edge_set, 4);
  checkGraphEdgeList(edge_set, 2);

  checkGraphOutArcList(edge_set, n2, 2);
  checkGraphOutArcList(edge_set, n3, 2);

  checkGraphInArcList(edge_set, n2, 2);
  checkGraphInArcList(edge_set, n3, 2);

  checkGraphIncEdgeList(edge_set, n2, 2);
  checkGraphIncEdgeList(edge_set, n3, 2);

  checkNodeIds(edge_set);
  checkArcIds(edge_set);
  checkEdgeIds(edge_set);
  checkGraphNodeMap(edge_set);
  checkGraphArcMap(edge_set);
  checkGraphEdgeMap(edge_set);

  checkGraphConEdgeList(edge_set, 2);
  checkGraphConArcList(edge_set, 4);

}


int main() {

  checkSmartArcSet();
  checkListArcSet();
  checkSmartEdgeSet();
  checkListEdgeSet();

  return 0;
}
