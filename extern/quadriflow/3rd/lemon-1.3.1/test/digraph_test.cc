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

#include <lemon/concepts/digraph.h>
#include <lemon/list_graph.h>
#include <lemon/smart_graph.h>
#include <lemon/static_graph.h>
#include <lemon/full_graph.h>

#include "test_tools.h"
#include "graph_test.h"

using namespace lemon;
using namespace lemon::concepts;

template <class Digraph>
void checkDigraphBuild() {
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);
  Digraph G;

  checkGraphNodeList(G, 0);
  checkGraphArcList(G, 0);

  G.reserveNode(3);
  G.reserveArc(4);

  Node
    n1 = G.addNode(),
    n2 = G.addNode(),
    n3 = G.addNode();
  checkGraphNodeList(G, 3);
  checkGraphArcList(G, 0);

  Arc a1 = G.addArc(n1, n2);
  check(G.source(a1) == n1 && G.target(a1) == n2, "Wrong arc");
  checkGraphNodeList(G, 3);
  checkGraphArcList(G, 1);

  checkGraphOutArcList(G, n1, 1);
  checkGraphOutArcList(G, n2, 0);
  checkGraphOutArcList(G, n3, 0);

  checkGraphInArcList(G, n1, 0);
  checkGraphInArcList(G, n2, 1);
  checkGraphInArcList(G, n3, 0);

  checkGraphConArcList(G, 1);

  Arc a2 = G.addArc(n2, n1),
      a3 = G.addArc(n2, n3),
      a4 = G.addArc(n2, n3);
  ::lemon::ignore_unused_variable_warning(a2,a3,a4);

  checkGraphNodeList(G, 3);
  checkGraphArcList(G, 4);

  checkGraphOutArcList(G, n1, 1);
  checkGraphOutArcList(G, n2, 3);
  checkGraphOutArcList(G, n3, 0);

  checkGraphInArcList(G, n1, 1);
  checkGraphInArcList(G, n2, 1);
  checkGraphInArcList(G, n3, 2);

  checkGraphConArcList(G, 4);

  checkNodeIds(G);
  checkArcIds(G);
  checkGraphNodeMap(G);
  checkGraphArcMap(G);
}

template <class Digraph>
void checkDigraphSplit() {
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

  Digraph G;
  Node n1 = G.addNode(), n2 = G.addNode(), n3 = G.addNode();
  Arc a1 = G.addArc(n1, n2), a2 = G.addArc(n2, n1),
      a3 = G.addArc(n2, n3), a4 = G.addArc(n2, n3);
  ::lemon::ignore_unused_variable_warning(a1,a2,a3,a4);

  Node n4 = G.split(n2);

  check(G.target(OutArcIt(G, n2)) == n4 &&
        G.source(InArcIt(G, n4)) == n2,
        "Wrong split.");

  checkGraphNodeList(G, 4);
  checkGraphArcList(G, 5);

  checkGraphOutArcList(G, n1, 1);
  checkGraphOutArcList(G, n2, 1);
  checkGraphOutArcList(G, n3, 0);
  checkGraphOutArcList(G, n4, 3);

  checkGraphInArcList(G, n1, 1);
  checkGraphInArcList(G, n2, 1);
  checkGraphInArcList(G, n3, 2);
  checkGraphInArcList(G, n4, 1);

  checkGraphConArcList(G, 5);
}

template <class Digraph>
void checkDigraphAlter() {
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

  Digraph G;
  Node n1 = G.addNode(), n2 = G.addNode(),
       n3 = G.addNode(), n4 = G.addNode();
  Arc a1 = G.addArc(n1, n2), a2 = G.addArc(n4, n1),
      a3 = G.addArc(n4, n3), a4 = G.addArc(n4, n3),
      a5 = G.addArc(n2, n4);
  ::lemon::ignore_unused_variable_warning(a1,a2,a3,a5);

  checkGraphNodeList(G, 4);
  checkGraphArcList(G, 5);

  // Check changeSource() and changeTarget()
  G.changeTarget(a4, n1);

  checkGraphNodeList(G, 4);
  checkGraphArcList(G, 5);

  checkGraphOutArcList(G, n1, 1);
  checkGraphOutArcList(G, n2, 1);
  checkGraphOutArcList(G, n3, 0);
  checkGraphOutArcList(G, n4, 3);

  checkGraphInArcList(G, n1, 2);
  checkGraphInArcList(G, n2, 1);
  checkGraphInArcList(G, n3, 1);
  checkGraphInArcList(G, n4, 1);

  checkGraphConArcList(G, 5);

  G.changeSource(a4, n3);

  checkGraphNodeList(G, 4);
  checkGraphArcList(G, 5);

  checkGraphOutArcList(G, n1, 1);
  checkGraphOutArcList(G, n2, 1);
  checkGraphOutArcList(G, n3, 1);
  checkGraphOutArcList(G, n4, 2);

  checkGraphInArcList(G, n1, 2);
  checkGraphInArcList(G, n2, 1);
  checkGraphInArcList(G, n3, 1);
  checkGraphInArcList(G, n4, 1);

  checkGraphConArcList(G, 5);

  // Check contract()
  G.contract(n2, n4, false);

  checkGraphNodeList(G, 3);
  checkGraphArcList(G, 5);

  checkGraphOutArcList(G, n1, 1);
  checkGraphOutArcList(G, n2, 3);
  checkGraphOutArcList(G, n3, 1);

  checkGraphInArcList(G, n1, 2);
  checkGraphInArcList(G, n2, 2);
  checkGraphInArcList(G, n3, 1);

  checkGraphConArcList(G, 5);

  G.contract(n2, n1);

  checkGraphNodeList(G, 2);
  checkGraphArcList(G, 3);

  checkGraphOutArcList(G, n2, 2);
  checkGraphOutArcList(G, n3, 1);

  checkGraphInArcList(G, n2, 2);
  checkGraphInArcList(G, n3, 1);

  checkGraphConArcList(G, 3);
}

template <class Digraph>
void checkDigraphErase() {
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

  Digraph G;
  Node n1 = G.addNode(), n2 = G.addNode(),
       n3 = G.addNode(), n4 = G.addNode();
  Arc a1 = G.addArc(n1, n2), a2 = G.addArc(n4, n1),
      a3 = G.addArc(n4, n3), a4 = G.addArc(n3, n1),
      a5 = G.addArc(n2, n4);
  ::lemon::ignore_unused_variable_warning(a2,a3,a4,a5);

  // Check arc deletion
  G.erase(a1);

  checkGraphNodeList(G, 4);
  checkGraphArcList(G, 4);

  checkGraphOutArcList(G, n1, 0);
  checkGraphOutArcList(G, n2, 1);
  checkGraphOutArcList(G, n3, 1);
  checkGraphOutArcList(G, n4, 2);

  checkGraphInArcList(G, n1, 2);
  checkGraphInArcList(G, n2, 0);
  checkGraphInArcList(G, n3, 1);
  checkGraphInArcList(G, n4, 1);

  checkGraphConArcList(G, 4);

  // Check node deletion
  G.erase(n4);

  checkGraphNodeList(G, 3);
  checkGraphArcList(G, 1);

  checkGraphOutArcList(G, n1, 0);
  checkGraphOutArcList(G, n2, 0);
  checkGraphOutArcList(G, n3, 1);
  checkGraphOutArcList(G, n4, 0);

  checkGraphInArcList(G, n1, 1);
  checkGraphInArcList(G, n2, 0);
  checkGraphInArcList(G, n3, 0);
  checkGraphInArcList(G, n4, 0);

  checkGraphConArcList(G, 1);
}


template <class Digraph>
void checkDigraphSnapshot() {
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

  Digraph G;
  Node n1 = G.addNode(), n2 = G.addNode(), n3 = G.addNode();
  Arc a1 = G.addArc(n1, n2), a2 = G.addArc(n2, n1),
      a3 = G.addArc(n2, n3), a4 = G.addArc(n2, n3);
  ::lemon::ignore_unused_variable_warning(a1,a2,a3,a4);

  typename Digraph::Snapshot snapshot(G);

  Node n = G.addNode();
  G.addArc(n3, n);
  G.addArc(n, n3);

  checkGraphNodeList(G, 4);
  checkGraphArcList(G, 6);

  snapshot.restore();

  checkGraphNodeList(G, 3);
  checkGraphArcList(G, 4);

  checkGraphOutArcList(G, n1, 1);
  checkGraphOutArcList(G, n2, 3);
  checkGraphOutArcList(G, n3, 0);

  checkGraphInArcList(G, n1, 1);
  checkGraphInArcList(G, n2, 1);
  checkGraphInArcList(G, n3, 2);

  checkGraphConArcList(G, 4);

  checkNodeIds(G);
  checkArcIds(G);
  checkGraphNodeMap(G);
  checkGraphArcMap(G);

  G.addNode();
  snapshot.save(G);

  G.addArc(G.addNode(), G.addNode());

  snapshot.restore();
  snapshot.save(G);

  checkGraphNodeList(G, 4);
  checkGraphArcList(G, 4);

  G.addArc(G.addNode(), G.addNode());

  snapshot.restore();

  checkGraphNodeList(G, 4);
  checkGraphArcList(G, 4);
}

void checkConcepts() {
  { // Checking digraph components
    checkConcept<BaseDigraphComponent, BaseDigraphComponent >();

    checkConcept<IDableDigraphComponent<>,
      IDableDigraphComponent<> >();

    checkConcept<IterableDigraphComponent<>,
      IterableDigraphComponent<> >();

    checkConcept<MappableDigraphComponent<>,
      MappableDigraphComponent<> >();
  }
  { // Checking skeleton digraph
    checkConcept<Digraph, Digraph>();
  }
  { // Checking ListDigraph
    checkConcept<Digraph, ListDigraph>();
    checkConcept<AlterableDigraphComponent<>, ListDigraph>();
    checkConcept<ExtendableDigraphComponent<>, ListDigraph>();
    checkConcept<ClearableDigraphComponent<>, ListDigraph>();
    checkConcept<ErasableDigraphComponent<>, ListDigraph>();
  }
  { // Checking SmartDigraph
    checkConcept<Digraph, SmartDigraph>();
    checkConcept<AlterableDigraphComponent<>, SmartDigraph>();
    checkConcept<ExtendableDigraphComponent<>, SmartDigraph>();
    checkConcept<ClearableDigraphComponent<>, SmartDigraph>();
  }
  { // Checking StaticDigraph
    checkConcept<Digraph, StaticDigraph>();
    checkConcept<ClearableDigraphComponent<>, StaticDigraph>();
  }
  { // Checking FullDigraph
    checkConcept<Digraph, FullDigraph>();
  }
}

template <typename Digraph>
void checkDigraphValidity() {
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);
  Digraph g;

  Node
    n1 = g.addNode(),
    n2 = g.addNode(),
    n3 = g.addNode();

  Arc
    e1 = g.addArc(n1, n2),
    e2 = g.addArc(n2, n3);
  ::lemon::ignore_unused_variable_warning(e2);

  check(g.valid(n1), "Wrong validity check");
  check(g.valid(e1), "Wrong validity check");

  check(!g.valid(g.nodeFromId(-1)), "Wrong validity check");
  check(!g.valid(g.arcFromId(-1)), "Wrong validity check");
}

template <typename Digraph>
void checkDigraphValidityErase() {
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);
  Digraph g;

  Node
    n1 = g.addNode(),
    n2 = g.addNode(),
    n3 = g.addNode();

  Arc
    e1 = g.addArc(n1, n2),
    e2 = g.addArc(n2, n3);

  check(g.valid(n1), "Wrong validity check");
  check(g.valid(e1), "Wrong validity check");

  g.erase(n1);

  check(!g.valid(n1), "Wrong validity check");
  check(g.valid(n2), "Wrong validity check");
  check(g.valid(n3), "Wrong validity check");
  check(!g.valid(e1), "Wrong validity check");
  check(g.valid(e2), "Wrong validity check");

  check(!g.valid(g.nodeFromId(-1)), "Wrong validity check");
  check(!g.valid(g.arcFromId(-1)), "Wrong validity check");
}

void checkStaticDigraph() {
  SmartDigraph g;
  SmartDigraph::NodeMap<StaticDigraph::Node> nref(g);
  SmartDigraph::ArcMap<StaticDigraph::Arc> aref(g);

  StaticDigraph G;

  checkGraphNodeList(G, 0);
  checkGraphArcList(G, 0);

  G.build(g, nref, aref);

  checkGraphNodeList(G, 0);
  checkGraphArcList(G, 0);

  SmartDigraph::Node
    n1 = g.addNode(),
    n2 = g.addNode(),
    n3 = g.addNode();

  G.build(g, nref, aref);

  checkGraphNodeList(G, 3);
  checkGraphArcList(G, 0);

  SmartDigraph::Arc a1 = g.addArc(n1, n2);

  G.build(g, nref, aref);

  check(G.source(aref[a1]) == nref[n1] && G.target(aref[a1]) == nref[n2],
        "Wrong arc or wrong references");
  checkGraphNodeList(G, 3);
  checkGraphArcList(G, 1);

  checkGraphOutArcList(G, nref[n1], 1);
  checkGraphOutArcList(G, nref[n2], 0);
  checkGraphOutArcList(G, nref[n3], 0);

  checkGraphInArcList(G, nref[n1], 0);
  checkGraphInArcList(G, nref[n2], 1);
  checkGraphInArcList(G, nref[n3], 0);

  checkGraphConArcList(G, 1);

  SmartDigraph::Arc
    a2 = g.addArc(n2, n1),
    a3 = g.addArc(n2, n3),
    a4 = g.addArc(n2, n3);
  ::lemon::ignore_unused_variable_warning(a2,a3,a4);

  digraphCopy(g, G).nodeRef(nref).run();

  checkGraphNodeList(G, 3);
  checkGraphArcList(G, 4);

  checkGraphOutArcList(G, nref[n1], 1);
  checkGraphOutArcList(G, nref[n2], 3);
  checkGraphOutArcList(G, nref[n3], 0);

  checkGraphInArcList(G, nref[n1], 1);
  checkGraphInArcList(G, nref[n2], 1);
  checkGraphInArcList(G, nref[n3], 2);

  checkGraphConArcList(G, 4);

  std::vector<std::pair<int,int> > arcs;
  arcs.push_back(std::make_pair(0,1));
  arcs.push_back(std::make_pair(0,2));
  arcs.push_back(std::make_pair(1,3));
  arcs.push_back(std::make_pair(1,2));
  arcs.push_back(std::make_pair(3,0));
  arcs.push_back(std::make_pair(3,3));
  arcs.push_back(std::make_pair(4,2));
  arcs.push_back(std::make_pair(4,3));
  arcs.push_back(std::make_pair(4,1));

  G.build(6, arcs.begin(), arcs.end());

  checkGraphNodeList(G, 6);
  checkGraphArcList(G, 9);

  checkGraphOutArcList(G, G.node(0), 2);
  checkGraphOutArcList(G, G.node(1), 2);
  checkGraphOutArcList(G, G.node(2), 0);
  checkGraphOutArcList(G, G.node(3), 2);
  checkGraphOutArcList(G, G.node(4), 3);
  checkGraphOutArcList(G, G.node(5), 0);

  checkGraphInArcList(G, G.node(0), 1);
  checkGraphInArcList(G, G.node(1), 2);
  checkGraphInArcList(G, G.node(2), 3);
  checkGraphInArcList(G, G.node(3), 3);
  checkGraphInArcList(G, G.node(4), 0);
  checkGraphInArcList(G, G.node(5), 0);

  checkGraphConArcList(G, 9);

  checkNodeIds(G);
  checkArcIds(G);
  checkGraphNodeMap(G);
  checkGraphArcMap(G);

  int n = G.nodeNum();
  int m = G.arcNum();
  check(G.index(G.node(n-1)) == n-1, "Wrong index.");
  check(G.index(G.arc(m-1)) == m-1, "Wrong index.");
}

void checkFullDigraph(int num) {
  typedef FullDigraph Digraph;
  DIGRAPH_TYPEDEFS(Digraph);

  Digraph G(num);
  check(G.nodeNum() == num && G.arcNum() == num * num, "Wrong size");

  G.resize(num);
  check(G.nodeNum() == num && G.arcNum() == num * num, "Wrong size");

  checkGraphNodeList(G, num);
  checkGraphArcList(G, num * num);

  for (NodeIt n(G); n != INVALID; ++n) {
    checkGraphOutArcList(G, n, num);
    checkGraphInArcList(G, n, num);
  }

  checkGraphConArcList(G, num * num);

  checkNodeIds(G);
  checkArcIds(G);
  checkGraphNodeMap(G);
  checkGraphArcMap(G);

  for (int i = 0; i < G.nodeNum(); ++i) {
    check(G.index(G(i)) == i, "Wrong index");
  }

  for (NodeIt s(G); s != INVALID; ++s) {
    for (NodeIt t(G); t != INVALID; ++t) {
      Arc a = G.arc(s, t);
      check(G.source(a) == s && G.target(a) == t, "Wrong arc lookup");
    }
  }
}

void checkDigraphs() {
  { // Checking ListDigraph
    checkDigraphBuild<ListDigraph>();
    checkDigraphSplit<ListDigraph>();
    checkDigraphAlter<ListDigraph>();
    checkDigraphErase<ListDigraph>();
    checkDigraphSnapshot<ListDigraph>();
    checkDigraphValidityErase<ListDigraph>();
  }
  { // Checking SmartDigraph
    checkDigraphBuild<SmartDigraph>();
    checkDigraphSplit<SmartDigraph>();
    checkDigraphSnapshot<SmartDigraph>();
    checkDigraphValidity<SmartDigraph>();
  }
  { // Checking StaticDigraph
    checkStaticDigraph();
  }
  { // Checking FullDigraph
    checkFullDigraph(8);
  }
}

int main() {
  checkDigraphs();
  checkConcepts();
  return 0;
}
