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

#include <lemon/concepts/bpgraph.h>
#include <lemon/list_graph.h>
#include <lemon/smart_graph.h>
#include <lemon/full_graph.h>

#include "test_tools.h"
#include "graph_test.h"

using namespace lemon;
using namespace lemon::concepts;

template <class BpGraph>
void checkBpGraphBuild() {
  TEMPLATE_BPGRAPH_TYPEDEFS(BpGraph);

  BpGraph G;
  checkGraphNodeList(G, 0);
  checkGraphRedNodeList(G, 0);
  checkGraphBlueNodeList(G, 0);
  checkGraphEdgeList(G, 0);
  checkGraphArcList(G, 0);

  G.reserveNode(3);
  G.reserveEdge(3);

  RedNode
    rn1 = G.addRedNode();
  checkGraphNodeList(G, 1);
  checkGraphRedNodeList(G, 1);
  checkGraphBlueNodeList(G, 0);
  checkGraphEdgeList(G, 0);
  checkGraphArcList(G, 0);

  BlueNode
    bn1 = G.addBlueNode(),
    bn2 = G.addBlueNode();
  checkGraphNodeList(G, 3);
  checkGraphRedNodeList(G, 1);
  checkGraphBlueNodeList(G, 2);
  checkGraphEdgeList(G, 0);
  checkGraphArcList(G, 0);

  Edge e1 = G.addEdge(rn1, bn2);
  check(G.redNode(e1) == rn1 && G.blueNode(e1) == bn2, "Wrong edge");
  check(G.u(e1) == rn1 && G.v(e1) == bn2, "Wrong edge");

  checkGraphNodeList(G, 3);
  checkGraphRedNodeList(G, 1);
  checkGraphBlueNodeList(G, 2);
  checkGraphEdgeList(G, 1);
  checkGraphArcList(G, 2);

  checkGraphIncEdgeArcLists(G, rn1, 1);
  checkGraphIncEdgeArcLists(G, bn1, 0);
  checkGraphIncEdgeArcLists(G, bn2, 1);

  checkGraphConEdgeList(G, 1);
  checkGraphConArcList(G, 2);

  Edge
    e2 = G.addEdge(bn1, rn1),
    e3 = G.addEdge(rn1, bn2);
  ::lemon::ignore_unused_variable_warning(e2,e3);

  checkGraphNodeList(G, 3);
  checkGraphRedNodeList(G, 1);
  checkGraphBlueNodeList(G, 2);
  checkGraphEdgeList(G, 3);
  checkGraphArcList(G, 6);

  checkGraphIncEdgeArcLists(G, rn1, 3);
  checkGraphIncEdgeArcLists(G, bn1, 1);
  checkGraphIncEdgeArcLists(G, bn2, 2);

  checkGraphConEdgeList(G, 3);
  checkGraphConArcList(G, 6);

  checkArcDirections(G);

  checkNodeIds(G);
  checkRedNodeIds(G);
  checkBlueNodeIds(G);
  checkArcIds(G);
  checkEdgeIds(G);

  checkGraphNodeMap(G);
  checkGraphRedNodeMap(G);
  checkGraphBlueNodeMap(G);
  checkGraphArcMap(G);
  checkGraphEdgeMap(G);
}

template <class BpGraph>
void checkBpGraphErase() {
  TEMPLATE_BPGRAPH_TYPEDEFS(BpGraph);

  BpGraph G;
  RedNode
    n1 = G.addRedNode(), n4 = G.addRedNode();
  BlueNode
    n2 = G.addBlueNode(), n3 = G.addBlueNode();
  Edge
    e1 = G.addEdge(n1, n2), e2 = G.addEdge(n1, n3),
    e3 = G.addEdge(n4, n2), e4 = G.addEdge(n4, n3);
  ::lemon::ignore_unused_variable_warning(e1,e3,e4);

  // Check edge deletion
  G.erase(e2);

  checkGraphNodeList(G, 4);
  checkGraphRedNodeList(G, 2);
  checkGraphBlueNodeList(G, 2);
  checkGraphEdgeList(G, 3);
  checkGraphArcList(G, 6);

  checkGraphIncEdgeArcLists(G, n1, 1);
  checkGraphIncEdgeArcLists(G, n2, 2);
  checkGraphIncEdgeArcLists(G, n3, 1);
  checkGraphIncEdgeArcLists(G, n4, 2);

  checkGraphConEdgeList(G, 3);
  checkGraphConArcList(G, 6);

  // Check node deletion
  G.erase(n3);

  checkGraphNodeList(G, 3);
  checkGraphRedNodeList(G, 2);
  checkGraphBlueNodeList(G, 1);
  checkGraphEdgeList(G, 2);
  checkGraphArcList(G, 4);

  checkGraphIncEdgeArcLists(G, n1, 1);
  checkGraphIncEdgeArcLists(G, n2, 2);
  checkGraphIncEdgeArcLists(G, n4, 1);

  checkGraphConEdgeList(G, 2);
  checkGraphConArcList(G, 4);

}

template <class BpGraph>
void checkBpGraphAlter() {
  TEMPLATE_BPGRAPH_TYPEDEFS(BpGraph);

  BpGraph G;
  RedNode
    n1 = G.addRedNode(), n4 = G.addRedNode();
  BlueNode
    n2 = G.addBlueNode(), n3 = G.addBlueNode();
  Edge
    e1 = G.addEdge(n1, n2), e2 = G.addEdge(n1, n3),
    e3 = G.addEdge(n4, n2), e4 = G.addEdge(n4, n3);
  ::lemon::ignore_unused_variable_warning(e1,e3,e4);

  G.changeRed(e2, n4);
  check(G.redNode(e2) == n4, "Wrong red node");
  check(G.blueNode(e2) == n3, "Wrong blue node");

  checkGraphNodeList(G, 4);
  checkGraphRedNodeList(G, 2);
  checkGraphBlueNodeList(G, 2);
  checkGraphEdgeList(G, 4);
  checkGraphArcList(G, 8);

  checkGraphIncEdgeArcLists(G, n1, 1);
  checkGraphIncEdgeArcLists(G, n2, 2);
  checkGraphIncEdgeArcLists(G, n3, 2);
  checkGraphIncEdgeArcLists(G, n4, 3);

  checkGraphConEdgeList(G, 4);
  checkGraphConArcList(G, 8);

  G.changeBlue(e2, n2);
  check(G.redNode(e2) == n4, "Wrong red node");
  check(G.blueNode(e2) == n2, "Wrong blue node");

  checkGraphNodeList(G, 4);
  checkGraphRedNodeList(G, 2);
  checkGraphBlueNodeList(G, 2);
  checkGraphEdgeList(G, 4);
  checkGraphArcList(G, 8);

  checkGraphIncEdgeArcLists(G, n1, 1);
  checkGraphIncEdgeArcLists(G, n2, 3);
  checkGraphIncEdgeArcLists(G, n3, 1);
  checkGraphIncEdgeArcLists(G, n4, 3);

  checkGraphConEdgeList(G, 4);
  checkGraphConArcList(G, 8);
}


template <class BpGraph>
void checkBpGraphSnapshot() {
  TEMPLATE_BPGRAPH_TYPEDEFS(BpGraph);

  BpGraph G;
  RedNode
    n1 = G.addRedNode();
  BlueNode
    n2 = G.addBlueNode(),
    n3 = G.addBlueNode();
  Edge
    e1 = G.addEdge(n1, n2),
    e2 = G.addEdge(n1, n3);
  ::lemon::ignore_unused_variable_warning(e1,e2);

  checkGraphNodeList(G, 3);
  checkGraphRedNodeList(G, 1);
  checkGraphBlueNodeList(G, 2);
  checkGraphEdgeList(G, 2);
  checkGraphArcList(G, 4);

  typename BpGraph::Snapshot snapshot(G);

  RedNode n4 = G.addRedNode();
  G.addEdge(n4, n2);
  G.addEdge(n4, n3);

  checkGraphNodeList(G, 4);
  checkGraphRedNodeList(G, 2);
  checkGraphBlueNodeList(G, 2);
  checkGraphEdgeList(G, 4);
  checkGraphArcList(G, 8);

  snapshot.restore();

  checkGraphNodeList(G, 3);
  checkGraphRedNodeList(G, 1);
  checkGraphBlueNodeList(G, 2);
  checkGraphEdgeList(G, 2);
  checkGraphArcList(G, 4);

  checkGraphIncEdgeArcLists(G, n1, 2);
  checkGraphIncEdgeArcLists(G, n2, 1);
  checkGraphIncEdgeArcLists(G, n3, 1);

  checkGraphConEdgeList(G, 2);
  checkGraphConArcList(G, 4);

  checkNodeIds(G);
  checkRedNodeIds(G);
  checkBlueNodeIds(G);
  checkArcIds(G);
  checkEdgeIds(G);

  checkGraphNodeMap(G);
  checkGraphRedNodeMap(G);
  checkGraphBlueNodeMap(G);
  checkGraphArcMap(G);
  checkGraphEdgeMap(G);

  G.addRedNode();
  snapshot.save(G);

  G.addEdge(G.addRedNode(), G.addBlueNode());

  snapshot.restore();
  snapshot.save(G);

  checkGraphNodeList(G, 4);
  checkGraphRedNodeList(G, 2);
  checkGraphBlueNodeList(G, 2);
  checkGraphEdgeList(G, 2);
  checkGraphArcList(G, 4);

  G.addEdge(G.addRedNode(), G.addBlueNode());

  snapshot.restore();

  checkGraphNodeList(G, 4);
  checkGraphRedNodeList(G, 2);
  checkGraphBlueNodeList(G, 2);
  checkGraphEdgeList(G, 2);
  checkGraphArcList(G, 4);
}

template <typename BpGraph>
void checkBpGraphValidity() {
  TEMPLATE_BPGRAPH_TYPEDEFS(BpGraph);
  BpGraph g;

  RedNode
    n1 = g.addRedNode();
  BlueNode
    n2 = g.addBlueNode(),
    n3 = g.addBlueNode();

  Edge
    e1 = g.addEdge(n1, n2),
    e2 = g.addEdge(n1, n3);
  ::lemon::ignore_unused_variable_warning(e2);

  check(g.valid(n1), "Wrong validity check");
  check(g.valid(e1), "Wrong validity check");
  check(g.valid(g.direct(e1, true)), "Wrong validity check");

  check(!g.valid(g.nodeFromId(-1)), "Wrong validity check");
  check(!g.valid(g.edgeFromId(-1)), "Wrong validity check");
  check(!g.valid(g.arcFromId(-1)), "Wrong validity check");
}

void checkConcepts() {
  { // Checking graph components
    checkConcept<BaseBpGraphComponent, BaseBpGraphComponent >();

    checkConcept<IDableBpGraphComponent<>,
      IDableBpGraphComponent<> >();

    checkConcept<IterableBpGraphComponent<>,
      IterableBpGraphComponent<> >();

    checkConcept<AlterableBpGraphComponent<>,
      AlterableBpGraphComponent<> >();

    checkConcept<MappableBpGraphComponent<>,
      MappableBpGraphComponent<> >();

    checkConcept<ExtendableBpGraphComponent<>,
      ExtendableBpGraphComponent<> >();

    checkConcept<ErasableBpGraphComponent<>,
      ErasableBpGraphComponent<> >();

    checkConcept<ClearableBpGraphComponent<>,
      ClearableBpGraphComponent<> >();

  }
  { // Checking skeleton graph
    checkConcept<BpGraph, BpGraph>();
  }
  { // Checking SmartBpGraph
    checkConcept<BpGraph, SmartBpGraph>();
    checkConcept<AlterableBpGraphComponent<>, SmartBpGraph>();
    checkConcept<ExtendableBpGraphComponent<>, SmartBpGraph>();
    checkConcept<ClearableBpGraphComponent<>, SmartBpGraph>();
  }
}

void checkFullBpGraph(int redNum, int blueNum) {
  typedef FullBpGraph BpGraph;
  BPGRAPH_TYPEDEFS(BpGraph);

  BpGraph G(redNum, blueNum);
  checkGraphNodeList(G, redNum + blueNum);
  checkGraphRedNodeList(G, redNum);
  checkGraphBlueNodeList(G, blueNum);
  checkGraphEdgeList(G, redNum * blueNum);
  checkGraphArcList(G, 2 * redNum * blueNum);

  G.resize(redNum, blueNum);
  checkGraphNodeList(G, redNum + blueNum);
  checkGraphRedNodeList(G, redNum);
  checkGraphBlueNodeList(G, blueNum);
  checkGraphEdgeList(G, redNum * blueNum);
  checkGraphArcList(G, 2 * redNum * blueNum);

  for (RedNodeIt n(G); n != INVALID; ++n) {
    checkGraphOutArcList(G, n, blueNum);
    checkGraphInArcList(G, n, blueNum);
    checkGraphIncEdgeList(G, n, blueNum);
  }

  for (BlueNodeIt n(G); n != INVALID; ++n) {
    checkGraphOutArcList(G, n, redNum);
    checkGraphInArcList(G, n, redNum);
    checkGraphIncEdgeList(G, n, redNum);
  }

  checkGraphConArcList(G, 2 * redNum * blueNum);
  checkGraphConEdgeList(G, redNum * blueNum);

  checkArcDirections(G);

  checkNodeIds(G);
  checkRedNodeIds(G);
  checkBlueNodeIds(G);
  checkArcIds(G);
  checkEdgeIds(G);

  checkGraphNodeMap(G);
  checkGraphRedNodeMap(G);
  checkGraphBlueNodeMap(G);
  checkGraphArcMap(G);
  checkGraphEdgeMap(G);

  for (int i = 0; i < G.redNum(); ++i) {
    check(G.red(G.redNode(i)), "Wrong node");
    check(G.index(G.redNode(i)) == i, "Wrong index");
  }

  for (int i = 0; i < G.blueNum(); ++i) {
    check(G.blue(G.blueNode(i)), "Wrong node");
    check(G.index(G.blueNode(i)) == i, "Wrong index");
  }

  for (NodeIt u(G); u != INVALID; ++u) {
    for (NodeIt v(G); v != INVALID; ++v) {
      Edge e = G.edge(u, v);
      Arc a = G.arc(u, v);
      if (G.red(u) == G.red(v)) {
        check(e == INVALID, "Wrong edge lookup");
        check(a == INVALID, "Wrong arc lookup");
      } else {
        check((G.u(e) == u && G.v(e) == v) ||
              (G.u(e) == v && G.v(e) == u), "Wrong edge lookup");
        check(G.source(a) == u && G.target(a) == v, "Wrong arc lookup");
      }
    }
  }

}

void checkGraphs() {
  { // Checking ListGraph
    checkBpGraphBuild<ListBpGraph>();
    checkBpGraphErase<ListBpGraph>();
    checkBpGraphAlter<ListBpGraph>();
    checkBpGraphSnapshot<ListBpGraph>();
    checkBpGraphValidity<ListBpGraph>();
  }
  { // Checking SmartGraph
    checkBpGraphBuild<SmartBpGraph>();
    checkBpGraphSnapshot<SmartBpGraph>();
    checkBpGraphValidity<SmartBpGraph>();
  }
  { // Checking FullBpGraph
    checkFullBpGraph(6, 8);
    checkFullBpGraph(7, 4);
  }
}

int main() {
  checkConcepts();
  checkGraphs();
  return 0;
}
