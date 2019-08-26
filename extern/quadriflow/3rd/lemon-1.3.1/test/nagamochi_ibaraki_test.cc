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
#include <lemon/concepts/graph.h>
#include <lemon/concepts/maps.h>
#include <lemon/lgf_reader.h>
#include <lemon/nagamochi_ibaraki.h>

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
  "2 3  1    6    6   \n";

void checkNagamochiIbarakiCompile()
{
  typedef int Value;
  typedef concepts::Graph Graph;

  typedef Graph::Node Node;
  typedef Graph::Edge Edge;
  typedef concepts::ReadMap<Edge, Value> CapMap;
  typedef concepts::WriteMap<Node, bool> CutMap;

  Graph g;
  Node n;
  CapMap cap;
  CutMap cut;
  Value v;
  bool b;
  ::lemon::ignore_unused_variable_warning(v,b);

  NagamochiIbaraki<Graph, CapMap> ni_test(g, cap);
  const NagamochiIbaraki<Graph, CapMap>& const_ni_test = ni_test;

  ni_test.init();
  ni_test.start();
  b = ni_test.processNextPhase();
  ni_test.run();

  v = const_ni_test.minCutValue();
  v = const_ni_test.minCutMap(cut);
}

template <typename Graph, typename CapMap, typename CutMap>
typename CapMap::Value
  cutValue(const Graph& graph, const CapMap& cap, const CutMap& cut)
{
  typename CapMap::Value sum = 0;
  for (typename Graph::EdgeIt e(graph); e != INVALID; ++e) {
    if (cut[graph.u(e)] != cut[graph.v(e)]) {
      sum += cap[e];
    }
  }
  return sum;
}

int main() {
  SmartGraph graph;
  SmartGraph::EdgeMap<int> cap1(graph), cap2(graph), cap3(graph);
  SmartGraph::NodeMap<bool> cut(graph);

  istringstream input(lgf);
  graphReader(graph, input)
    .edgeMap("cap1", cap1)
    .edgeMap("cap2", cap2)
    .edgeMap("cap3", cap3)
    .run();

  {
    NagamochiIbaraki<SmartGraph> ni(graph, cap1);
    ni.run();
    ni.minCutMap(cut);

    check(ni.minCutValue() == 1, "Wrong cut value");
    check(ni.minCutValue() == cutValue(graph, cap1, cut), "Wrong cut value");
  }
  {
    NagamochiIbaraki<SmartGraph> ni(graph, cap2);
    ni.run();
    ni.minCutMap(cut);

    check(ni.minCutValue() == 3, "Wrong cut value");
    check(ni.minCutValue() == cutValue(graph, cap2, cut), "Wrong cut value");
  }
  {
    NagamochiIbaraki<SmartGraph> ni(graph, cap3);
    ni.run();
    ni.minCutMap(cut);

    check(ni.minCutValue() == 5, "Wrong cut value");
    check(ni.minCutValue() == cutValue(graph, cap3, cut), "Wrong cut value");
  }
  {
    NagamochiIbaraki<SmartGraph>::SetUnitCapacity::Create ni(graph);
    ni.run();
    ni.minCutMap(cut);

    ConstMap<SmartGraph::Edge, int> cap4(1);
    check(ni.minCutValue() == 1, "Wrong cut value");
    check(ni.minCutValue() == cutValue(graph, cap4, cut), "Wrong cut value");
  }

  return 0;
}
