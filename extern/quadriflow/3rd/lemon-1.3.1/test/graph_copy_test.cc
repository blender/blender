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

#include <lemon/smart_graph.h>
#include <lemon/list_graph.h>
#include <lemon/static_graph.h>
#include <lemon/lgf_reader.h>
#include <lemon/error.h>

#include "test_tools.h"

using namespace std;
using namespace lemon;

template <typename GR>
void digraph_copy_test() {
  const int nn = 10;

  // Build a digraph
  SmartDigraph from;
  SmartDigraph::NodeMap<int> fnm(from);
  SmartDigraph::ArcMap<int> fam(from);
  SmartDigraph::Node fn = INVALID;
  SmartDigraph::Arc fa = INVALID;

  std::vector<SmartDigraph::Node> fnv;
  for (int i = 0; i < nn; ++i) {
    SmartDigraph::Node node = from.addNode();
    fnv.push_back(node);
    fnm[node] = i * i;
    if (i == 0) fn = node;
  }

  for (int i = 0; i < nn; ++i) {
    for (int j = 0; j < nn; ++j) {
      SmartDigraph::Arc arc = from.addArc(fnv[i], fnv[j]);
      fam[arc] = i + j * j;
      if (i == 0 && j == 0) fa = arc;
    }
  }

  // Test digraph copy
  GR to;
  typename GR::template NodeMap<int> tnm(to);
  typename GR::template ArcMap<int> tam(to);
  typename GR::Node tn;
  typename GR::Arc ta;

  SmartDigraph::NodeMap<typename GR::Node> nr(from);
  SmartDigraph::ArcMap<typename GR::Arc> er(from);

  typename GR::template NodeMap<SmartDigraph::Node> ncr(to);
  typename GR::template ArcMap<SmartDigraph::Arc> ecr(to);

  digraphCopy(from, to).
    nodeMap(fnm, tnm).arcMap(fam, tam).
    nodeRef(nr).arcRef(er).
    nodeCrossRef(ncr).arcCrossRef(ecr).
    node(fn, tn).arc(fa, ta).run();

  check(countNodes(from) == countNodes(to), "Wrong copy.");
  check(countArcs(from) == countArcs(to), "Wrong copy.");

  for (SmartDigraph::NodeIt it(from); it != INVALID; ++it) {
    check(ncr[nr[it]] == it, "Wrong copy.");
    check(fnm[it] == tnm[nr[it]], "Wrong copy.");
  }

  for (SmartDigraph::ArcIt it(from); it != INVALID; ++it) {
    check(ecr[er[it]] == it, "Wrong copy.");
    check(fam[it] == tam[er[it]], "Wrong copy.");
    check(nr[from.source(it)] == to.source(er[it]), "Wrong copy.");
    check(nr[from.target(it)] == to.target(er[it]), "Wrong copy.");
  }

  for (typename GR::NodeIt it(to); it != INVALID; ++it) {
    check(nr[ncr[it]] == it, "Wrong copy.");
  }

  for (typename GR::ArcIt it(to); it != INVALID; ++it) {
    check(er[ecr[it]] == it, "Wrong copy.");
  }
  check(tn == nr[fn], "Wrong copy.");
  check(ta == er[fa], "Wrong copy.");

  // Test repeated copy
  digraphCopy(from, to).run();

  check(countNodes(from) == countNodes(to), "Wrong copy.");
  check(countArcs(from) == countArcs(to), "Wrong copy.");
}

template <typename GR>
void graph_copy_test() {
  const int nn = 10;

  // Build a graph
  SmartGraph from;
  SmartGraph::NodeMap<int> fnm(from);
  SmartGraph::ArcMap<int> fam(from);
  SmartGraph::EdgeMap<int> fem(from);
  SmartGraph::Node fn = INVALID;
  SmartGraph::Arc fa = INVALID;
  SmartGraph::Edge fe = INVALID;

  std::vector<SmartGraph::Node> fnv;
  for (int i = 0; i < nn; ++i) {
    SmartGraph::Node node = from.addNode();
    fnv.push_back(node);
    fnm[node] = i * i;
    if (i == 0) fn = node;
  }

  for (int i = 0; i < nn; ++i) {
    for (int j = 0; j < nn; ++j) {
      SmartGraph::Edge edge = from.addEdge(fnv[i], fnv[j]);
      fem[edge] = i * i + j * j;
      fam[from.direct(edge, true)] = i + j * j;
      fam[from.direct(edge, false)] = i * i + j;
      if (i == 0 && j == 0) fa = from.direct(edge, true);
      if (i == 0 && j == 0) fe = edge;
    }
  }

  // Test graph copy
  GR to;
  typename GR::template NodeMap<int> tnm(to);
  typename GR::template ArcMap<int> tam(to);
  typename GR::template EdgeMap<int> tem(to);
  typename GR::Node tn;
  typename GR::Arc ta;
  typename GR::Edge te;

  SmartGraph::NodeMap<typename GR::Node> nr(from);
  SmartGraph::ArcMap<typename GR::Arc> ar(from);
  SmartGraph::EdgeMap<typename GR::Edge> er(from);

  typename GR::template NodeMap<SmartGraph::Node> ncr(to);
  typename GR::template ArcMap<SmartGraph::Arc> acr(to);
  typename GR::template EdgeMap<SmartGraph::Edge> ecr(to);

  graphCopy(from, to).
    nodeMap(fnm, tnm).arcMap(fam, tam).edgeMap(fem, tem).
    nodeRef(nr).arcRef(ar).edgeRef(er).
    nodeCrossRef(ncr).arcCrossRef(acr).edgeCrossRef(ecr).
    node(fn, tn).arc(fa, ta).edge(fe, te).run();

  check(countNodes(from) == countNodes(to), "Wrong copy.");
  check(countEdges(from) == countEdges(to), "Wrong copy.");
  check(countArcs(from) == countArcs(to), "Wrong copy.");

  for (SmartGraph::NodeIt it(from); it != INVALID; ++it) {
    check(ncr[nr[it]] == it, "Wrong copy.");
    check(fnm[it] == tnm[nr[it]], "Wrong copy.");
  }

  for (SmartGraph::ArcIt it(from); it != INVALID; ++it) {
    check(acr[ar[it]] == it, "Wrong copy.");
    check(fam[it] == tam[ar[it]], "Wrong copy.");
    check(nr[from.source(it)] == to.source(ar[it]), "Wrong copy.");
    check(nr[from.target(it)] == to.target(ar[it]), "Wrong copy.");
  }

  for (SmartGraph::EdgeIt it(from); it != INVALID; ++it) {
    check(ecr[er[it]] == it, "Wrong copy.");
    check(fem[it] == tem[er[it]], "Wrong copy.");
    check(nr[from.u(it)] == to.u(er[it]) || nr[from.u(it)] == to.v(er[it]),
          "Wrong copy.");
    check(nr[from.v(it)] == to.u(er[it]) || nr[from.v(it)] == to.v(er[it]),
          "Wrong copy.");
    check((from.u(it) != from.v(it)) == (to.u(er[it]) != to.v(er[it])),
          "Wrong copy.");
  }

  for (typename GR::NodeIt it(to); it != INVALID; ++it) {
    check(nr[ncr[it]] == it, "Wrong copy.");
  }

  for (typename GR::ArcIt it(to); it != INVALID; ++it) {
    check(ar[acr[it]] == it, "Wrong copy.");
  }
  for (typename GR::EdgeIt it(to); it != INVALID; ++it) {
    check(er[ecr[it]] == it, "Wrong copy.");
  }
  check(tn == nr[fn], "Wrong copy.");
  check(ta == ar[fa], "Wrong copy.");
  check(te == er[fe], "Wrong copy.");

  // Test repeated copy
  graphCopy(from, to).run();

  check(countNodes(from) == countNodes(to), "Wrong copy.");
  check(countEdges(from) == countEdges(to), "Wrong copy.");
  check(countArcs(from) == countArcs(to), "Wrong copy.");
}

template <typename GR>
void bpgraph_copy_test() {
  const int nn = 10;

  // Build a graph
  SmartBpGraph from;
  SmartBpGraph::NodeMap<int> fnm(from);
  SmartBpGraph::RedNodeMap<int> frnm(from);
  SmartBpGraph::BlueNodeMap<int> fbnm(from);
  SmartBpGraph::ArcMap<int> fam(from);
  SmartBpGraph::EdgeMap<int> fem(from);
  SmartBpGraph::Node fn = INVALID;
  SmartBpGraph::RedNode frn = INVALID;
  SmartBpGraph::BlueNode fbn = INVALID;
  SmartBpGraph::Arc fa = INVALID;
  SmartBpGraph::Edge fe = INVALID;

  std::vector<SmartBpGraph::RedNode> frnv;
  for (int i = 0; i < nn; ++i) {
    SmartBpGraph::RedNode node = from.addRedNode();
    frnv.push_back(node);
    fnm[node] = i * i;
    frnm[node] = i + i;
    if (i == 0) {
      fn = node;
      frn = node;
    }
  }

  std::vector<SmartBpGraph::BlueNode> fbnv;
  for (int i = 0; i < nn; ++i) {
    SmartBpGraph::BlueNode node = from.addBlueNode();
    fbnv.push_back(node);
    fnm[node] = i * i;
    fbnm[node] = i + i;
    if (i == 0) fbn = node;
  }

  for (int i = 0; i < nn; ++i) {
    for (int j = 0; j < nn; ++j) {
      SmartBpGraph::Edge edge = from.addEdge(frnv[i], fbnv[j]);
      fem[edge] = i * i + j * j;
      fam[from.direct(edge, true)] = i + j * j;
      fam[from.direct(edge, false)] = i * i + j;
      if (i == 0 && j == 0) fa = from.direct(edge, true);
      if (i == 0 && j == 0) fe = edge;
    }
  }

  // Test graph copy
  GR to;
  typename GR::template NodeMap<int> tnm(to);
  typename GR::template RedNodeMap<int> trnm(to);
  typename GR::template BlueNodeMap<int> tbnm(to);
  typename GR::template ArcMap<int> tam(to);
  typename GR::template EdgeMap<int> tem(to);
  typename GR::Node tn;
  typename GR::RedNode trn;
  typename GR::BlueNode tbn;
  typename GR::Arc ta;
  typename GR::Edge te;

  SmartBpGraph::NodeMap<typename GR::Node> nr(from);
  SmartBpGraph::RedNodeMap<typename GR::RedNode> rnr(from);
  SmartBpGraph::BlueNodeMap<typename GR::BlueNode> bnr(from);
  SmartBpGraph::ArcMap<typename GR::Arc> ar(from);
  SmartBpGraph::EdgeMap<typename GR::Edge> er(from);

  typename GR::template NodeMap<SmartBpGraph::Node> ncr(to);
  typename GR::template RedNodeMap<SmartBpGraph::RedNode> rncr(to);
  typename GR::template BlueNodeMap<SmartBpGraph::BlueNode> bncr(to);
  typename GR::template ArcMap<SmartBpGraph::Arc> acr(to);
  typename GR::template EdgeMap<SmartBpGraph::Edge> ecr(to);

  bpGraphCopy(from, to).
    nodeMap(fnm, tnm).
    redNodeMap(frnm, trnm).blueNodeMap(fbnm, tbnm).
    arcMap(fam, tam).edgeMap(fem, tem).
    nodeRef(nr).redRef(rnr).blueRef(bnr).
    arcRef(ar).edgeRef(er).
    nodeCrossRef(ncr).redCrossRef(rncr).blueCrossRef(bncr).
    arcCrossRef(acr).edgeCrossRef(ecr).
    node(fn, tn).redNode(frn, trn).blueNode(fbn, tbn).
    arc(fa, ta).edge(fe, te).run();

  check(countNodes(from) == countNodes(to), "Wrong copy.");
  check(countRedNodes(from) == countRedNodes(to), "Wrong copy.");
  check(countBlueNodes(from) == countBlueNodes(to), "Wrong copy.");
  check(countEdges(from) == countEdges(to), "Wrong copy.");
  check(countArcs(from) == countArcs(to), "Wrong copy.");

  for (SmartBpGraph::NodeIt it(from); it != INVALID; ++it) {
    check(ncr[nr[it]] == it, "Wrong copy.");
    check(fnm[it] == tnm[nr[it]], "Wrong copy.");
  }

  for (SmartBpGraph::RedNodeIt it(from); it != INVALID; ++it) {
    check(ncr[nr[it]] == it, "Wrong copy.");
    check(fnm[it] == tnm[nr[it]], "Wrong copy.");
    check(rnr[it] == nr[it], "Wrong copy.");
    check(rncr[rnr[it]] == it, "Wrong copy.");
    check(frnm[it] == trnm[rnr[it]], "Wrong copy.");
    check(to.red(rnr[it]), "Wrong copy.");
  }

  for (SmartBpGraph::BlueNodeIt it(from); it != INVALID; ++it) {
    check(ncr[nr[it]] == it, "Wrong copy.");
    check(fnm[it] == tnm[nr[it]], "Wrong copy.");
    check(bnr[it] == nr[it], "Wrong copy.");
    check(bncr[bnr[it]] == it, "Wrong copy.");
    check(fbnm[it] == tbnm[bnr[it]], "Wrong copy.");
    check(to.blue(bnr[it]), "Wrong copy.");
  }

  for (SmartBpGraph::ArcIt it(from); it != INVALID; ++it) {
    check(acr[ar[it]] == it, "Wrong copy.");
    check(fam[it] == tam[ar[it]], "Wrong copy.");
    check(nr[from.source(it)] == to.source(ar[it]), "Wrong copy.");
    check(nr[from.target(it)] == to.target(ar[it]), "Wrong copy.");
  }

  for (SmartBpGraph::EdgeIt it(from); it != INVALID; ++it) {
    check(ecr[er[it]] == it, "Wrong copy.");
    check(fem[it] == tem[er[it]], "Wrong copy.");
    check(nr[from.u(it)] == to.u(er[it]) || nr[from.u(it)] == to.v(er[it]),
          "Wrong copy.");
    check(nr[from.v(it)] == to.u(er[it]) || nr[from.v(it)] == to.v(er[it]),
          "Wrong copy.");
    check((from.u(it) != from.v(it)) == (to.u(er[it]) != to.v(er[it])),
          "Wrong copy.");
  }

  for (typename GR::NodeIt it(to); it != INVALID; ++it) {
    check(nr[ncr[it]] == it, "Wrong copy.");
  }
  for (typename GR::RedNodeIt it(to); it != INVALID; ++it) {
    check(rncr[it] == ncr[it], "Wrong copy.");
    check(rnr[rncr[it]] == it, "Wrong copy.");
  }
  for (typename GR::BlueNodeIt it(to); it != INVALID; ++it) {
    check(bncr[it] == ncr[it], "Wrong copy.");
    check(bnr[bncr[it]] == it, "Wrong copy.");
  }
  for (typename GR::ArcIt it(to); it != INVALID; ++it) {
    check(ar[acr[it]] == it, "Wrong copy.");
  }
  for (typename GR::EdgeIt it(to); it != INVALID; ++it) {
    check(er[ecr[it]] == it, "Wrong copy.");
  }
  check(tn == nr[fn], "Wrong copy.");
  check(trn == rnr[frn], "Wrong copy.");
  check(tbn == bnr[fbn], "Wrong copy.");
  check(ta == ar[fa], "Wrong copy.");
  check(te == er[fe], "Wrong copy.");

  // Test repeated copy
  bpGraphCopy(from, to).run();

  check(countNodes(from) == countNodes(to), "Wrong copy.");
  check(countRedNodes(from) == countRedNodes(to), "Wrong copy.");
  check(countBlueNodes(from) == countBlueNodes(to), "Wrong copy.");
  check(countEdges(from) == countEdges(to), "Wrong copy.");
  check(countArcs(from) == countArcs(to), "Wrong copy.");
}


int main() {
  digraph_copy_test<SmartDigraph>();
  digraph_copy_test<ListDigraph>();
  digraph_copy_test<StaticDigraph>();
  graph_copy_test<SmartGraph>();
  graph_copy_test<ListGraph>();
  bpgraph_copy_test<SmartBpGraph>();
  bpgraph_copy_test<ListBpGraph>();

  return 0;
}
