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

#include <sstream>

#include <lemon/smart_graph.h>
#include <lemon/adaptors.h>
#include <lemon/concepts/digraph.h>
#include <lemon/concepts/maps.h>
#include <lemon/lgf_reader.h>
#include <lemon/hao_orlin.h>

#include "test_tools.h"

using namespace lemon;
using namespace std;

const std::string lgf =
  "@nodes\n"
  "label\n"
  "0\n"
  "1\n"
  "2\n"
  "3\n"
  "4\n"
  "5\n"
  "@edges\n"
  "     cap1 cap2 cap3\n"
  "0 1  1    1    1   \n"
  "0 2  2    2    4   \n"
  "1 2  4    4    4   \n"
  "3 4  1    1    1   \n"
  "3 5  2    2    4   \n"
  "4 5  4    4    4   \n"
  "5 4  4    4    4   \n"
  "2 3  1    6    6   \n"
  "4 0  1    6    6   \n";

void checkHaoOrlinCompile()
{
  typedef int Value;
  typedef concepts::Digraph Digraph;

  typedef Digraph::Node Node;
  typedef Digraph::Arc Arc;
  typedef concepts::ReadMap<Arc, Value> CapMap;
  typedef concepts::WriteMap<Node, bool> CutMap;

  Digraph g;
  Node n;
  CapMap cap;
  CutMap cut;
  Value v;
  ::lemon::ignore_unused_variable_warning(v);

  HaoOrlin<Digraph, CapMap> ho_test(g, cap);
  const HaoOrlin<Digraph, CapMap>&
    const_ho_test = ho_test;

  ho_test.init();
  ho_test.init(n);
  ho_test.calculateOut();
  ho_test.calculateIn();
  ho_test.run();
  ho_test.run(n);

  v = const_ho_test.minCutValue();
  v = const_ho_test.minCutMap(cut);
}

template <typename Graph, typename CapMap, typename CutMap>
typename CapMap::Value
  cutValue(const Graph& graph, const CapMap& cap, const CutMap& cut)
{
  typename CapMap::Value sum = 0;
  for (typename Graph::ArcIt a(graph); a != INVALID; ++a) {
    if (cut[graph.source(a)] && !cut[graph.target(a)])
      sum += cap[a];
  }
  return sum;
}

int main() {
  SmartDigraph graph;
  SmartDigraph::ArcMap<int> cap1(graph), cap2(graph), cap3(graph);
  SmartDigraph::NodeMap<bool> cut(graph);

  istringstream input(lgf);
  digraphReader(graph, input)
    .arcMap("cap1", cap1)
    .arcMap("cap2", cap2)
    .arcMap("cap3", cap3)
    .run();

  {
    HaoOrlin<SmartDigraph> ho(graph, cap1);
    ho.run();
    ho.minCutMap(cut);

    check(ho.minCutValue() == 1, "Wrong cut value");
    check(ho.minCutValue() == cutValue(graph, cap1, cut), "Wrong cut value");
  }
  {
    HaoOrlin<SmartDigraph> ho(graph, cap2);
    ho.run();
    ho.minCutMap(cut);

    check(ho.minCutValue() == 1, "Wrong cut value");
    check(ho.minCutValue() == cutValue(graph, cap2, cut), "Wrong cut value");
  }
  {
    HaoOrlin<SmartDigraph> ho(graph, cap3);
    ho.run();
    ho.minCutMap(cut);

    check(ho.minCutValue() == 1, "Wrong cut value");
    check(ho.minCutValue() == cutValue(graph, cap3, cut), "Wrong cut value");
  }

  typedef Undirector<SmartDigraph> UGraph;
  UGraph ugraph(graph);

  {
    HaoOrlin<UGraph, SmartDigraph::ArcMap<int> > ho(ugraph, cap1);
    ho.run();
    ho.minCutMap(cut);

    check(ho.minCutValue() == 2, "Wrong cut value");
    check(ho.minCutValue() == cutValue(ugraph, cap1, cut), "Wrong cut value");
  }
  {
    HaoOrlin<UGraph, SmartDigraph::ArcMap<int> > ho(ugraph, cap2);
    ho.run();
    ho.minCutMap(cut);

    check(ho.minCutValue() == 5, "Wrong cut value");
    check(ho.minCutValue() == cutValue(ugraph, cap2, cut), "Wrong cut value");
  }
  {
    HaoOrlin<UGraph, SmartDigraph::ArcMap<int> > ho(ugraph, cap3);
    ho.run();
    ho.minCutMap(cut);

    check(ho.minCutValue() == 5, "Wrong cut value");
    check(ho.minCutValue() == cutValue(ugraph, cap3, cut), "Wrong cut value");
  }

  return 0;
}
