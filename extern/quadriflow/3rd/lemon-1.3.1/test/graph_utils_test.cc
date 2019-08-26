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

#include <cstdlib>
#include <ctime>

#include <lemon/random.h>
#include <lemon/list_graph.h>
#include <lemon/smart_graph.h>
#include <lemon/maps.h>

#include "graph_test.h"
#include "test_tools.h"

using namespace lemon;

template <typename Digraph>
void checkFindArcs() {
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

  {
    Digraph digraph;
    for (int i = 0; i < 10; ++i) {
      digraph.addNode();
    }
    RangeIdMap<Digraph, Node> nodes(digraph);
    typename RangeIdMap<Digraph, Node>::InverseMap invNodes(nodes);
    for (int i = 0; i < 100; ++i) {
      int src = rnd[invNodes.size()];
      int trg = rnd[invNodes.size()];
      digraph.addArc(invNodes[src], invNodes[trg]);
    }
    typename Digraph::template ArcMap<bool> found(digraph, false);
    RangeIdMap<Digraph, Arc> arcs(digraph);
    for (NodeIt src(digraph); src != INVALID; ++src) {
      for (NodeIt trg(digraph); trg != INVALID; ++trg) {
        for (ConArcIt<Digraph> con(digraph, src, trg); con != INVALID; ++con) {
          check(digraph.source(con) == src, "Wrong source.");
          check(digraph.target(con) == trg, "Wrong target.");
          check(found[con] == false, "The arc found already.");
          found[con] = true;
        }
      }
    }
    for (ArcIt it(digraph); it != INVALID; ++it) {
      check(found[it] == true, "The arc is not found.");
    }
  }

  {
    int num = 5;
    Digraph fg;
    std::vector<Node> nodes;
    for (int i = 0; i < num; ++i) {
      nodes.push_back(fg.addNode());
    }
    for (int i = 0; i < num * num; ++i) {
      fg.addArc(nodes[i / num], nodes[i % num]);
    }
    check(countNodes(fg) == num, "Wrong node number.");
    check(countArcs(fg) == num*num, "Wrong arc number.");
    for (NodeIt src(fg); src != INVALID; ++src) {
      for (NodeIt trg(fg); trg != INVALID; ++trg) {
        ConArcIt<Digraph> con(fg, src, trg);
        check(con != INVALID, "There is no connecting arc.");
        check(fg.source(con) == src, "Wrong source.");
        check(fg.target(con) == trg, "Wrong target.");
        check(++con == INVALID, "There is more connecting arc.");
      }
    }
    ArcLookUp<Digraph> al1(fg);
    DynArcLookUp<Digraph> al2(fg);
    AllArcLookUp<Digraph> al3(fg);
    for (NodeIt src(fg); src != INVALID; ++src) {
      for (NodeIt trg(fg); trg != INVALID; ++trg) {
        Arc con1 = al1(src, trg);
        Arc con2 = al2(src, trg);
        Arc con3 = al3(src, trg);
        Arc con4 = findArc(fg, src, trg);
        check(con1 == con2 && con2 == con3 && con3 == con4,
              "Different results.")
        check(con1 != INVALID, "There is no connecting arc.");
        check(fg.source(con1) == src, "Wrong source.");
        check(fg.target(con1) == trg, "Wrong target.");
        check(al3(src, trg, con3) == INVALID,
              "There is more connecting arc.");
        check(findArc(fg, src, trg, con4) == INVALID,
              "There is more connecting arc.");
      }
    }
  }
}

template <typename Graph>
void checkFindEdges() {
  TEMPLATE_GRAPH_TYPEDEFS(Graph);
  Graph graph;
  for (int i = 0; i < 10; ++i) {
    graph.addNode();
  }
  RangeIdMap<Graph, Node> nodes(graph);
  typename RangeIdMap<Graph, Node>::InverseMap invNodes(nodes);
  for (int i = 0; i < 100; ++i) {
    int src = rnd[invNodes.size()];
    int trg = rnd[invNodes.size()];
    graph.addEdge(invNodes[src], invNodes[trg]);
  }
  typename Graph::template EdgeMap<int> found(graph, 0);
  RangeIdMap<Graph, Edge> edges(graph);
  for (NodeIt src(graph); src != INVALID; ++src) {
    for (NodeIt trg(graph); trg != INVALID; ++trg) {
      for (ConEdgeIt<Graph> con(graph, src, trg); con != INVALID; ++con) {
        check( (graph.u(con) == src && graph.v(con) == trg) ||
               (graph.v(con) == src && graph.u(con) == trg),
               "Wrong end nodes.");
        ++found[con];
        check(found[con] <= 2, "The edge found more than twice.");
      }
    }
  }
  for (EdgeIt it(graph); it != INVALID; ++it) {
    check( (graph.u(it) != graph.v(it) && found[it] == 2) ||
           (graph.u(it) == graph.v(it) && found[it] == 1),
           "The edge is not found correctly.");
  }
}

template <class Digraph>
void checkDeg()
{
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

  const int nodeNum = 10;
  const int arcNum = 100;
  Digraph digraph;
  InDegMap<Digraph> inDeg(digraph);
  OutDegMap<Digraph> outDeg(digraph);
  std::vector<Node> nodes(nodeNum);
  for (int i = 0; i < nodeNum; ++i) {
    nodes[i] = digraph.addNode();
  }
  std::vector<Arc> arcs(arcNum);
  for (int i = 0; i < arcNum; ++i) {
    arcs[i] = digraph.addArc(nodes[rnd[nodeNum]], nodes[rnd[nodeNum]]);
  }
  for (int i = 0; i < nodeNum; ++i) {
    check(inDeg[nodes[i]] == countInArcs(digraph, nodes[i]),
          "Wrong in degree map");
  }
  for (int i = 0; i < nodeNum; ++i) {
    check(outDeg[nodes[i]] == countOutArcs(digraph, nodes[i]),
          "Wrong out degree map");
  }
}

template <class Digraph>
void checkSnapDeg()
{
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

  Digraph g;
  Node n1=g.addNode();
  Node n2=g.addNode();

  InDegMap<Digraph> ind(g);

  g.addArc(n1,n2);

  typename Digraph::Snapshot snap(g);

  OutDegMap<Digraph> outd(g);

  check(ind[n1]==0 && ind[n2]==1, "Wrong InDegMap value.");
  check(outd[n1]==1 && outd[n2]==0, "Wrong OutDegMap value.");

  g.addArc(n1,n2);
  g.addArc(n2,n1);

  check(ind[n1]==1 && ind[n2]==2, "Wrong InDegMap value.");
  check(outd[n1]==2 && outd[n2]==1, "Wrong OutDegMap value.");

  snap.restore();

  check(ind[n1]==0 && ind[n2]==1, "Wrong InDegMap value.");
  check(outd[n1]==1 && outd[n2]==0, "Wrong OutDegMap value.");
}

int main() {
  // Checking ConArcIt, ConEdgeIt, ArcLookUp, AllArcLookUp, and DynArcLookUp
  checkFindArcs<ListDigraph>();
  checkFindArcs<SmartDigraph>();
  checkFindEdges<ListGraph>();
  checkFindEdges<SmartGraph>();

  // Checking In/OutDegMap (and Snapshot feature)
  checkDeg<ListDigraph>();
  checkDeg<SmartDigraph>();
  checkSnapDeg<ListDigraph>();
  checkSnapDeg<SmartDigraph>();

  return 0;
}
