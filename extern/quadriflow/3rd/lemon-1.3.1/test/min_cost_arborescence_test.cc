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
#include <set>
#include <vector>
#include <iterator>

#include <lemon/smart_graph.h>
#include <lemon/min_cost_arborescence.h>
#include <lemon/lgf_reader.h>
#include <lemon/concepts/digraph.h>

#include "test_tools.h"

using namespace lemon;
using namespace std;

const char test_lgf[] =
  "@nodes\n"
  "label\n"
  "0\n"
  "1\n"
  "2\n"
  "3\n"
  "4\n"
  "5\n"
  "6\n"
  "7\n"
  "8\n"
  "9\n"
  "@arcs\n"
  "     label  cost\n"
  "1 8  0      107\n"
  "0 3  1      70\n"
  "2 1  2      46\n"
  "4 1  3      28\n"
  "4 4  4      91\n"
  "3 9  5      76\n"
  "9 8  6      61\n"
  "8 1  7      39\n"
  "9 8  8      74\n"
  "8 0  9      39\n"
  "4 3  10     45\n"
  "2 2  11     34\n"
  "0 1  12     100\n"
  "6 3  13     95\n"
  "4 1  14     22\n"
  "1 1  15     31\n"
  "7 2  16     51\n"
  "2 6  17     29\n"
  "8 3  18     115\n"
  "6 9  19     32\n"
  "1 1  20     60\n"
  "0 3  21     40\n"
  "@attributes\n"
  "source 0\n";


void checkMinCostArborescenceCompile()
{
  typedef double VType;
  typedef concepts::Digraph Digraph;
  typedef concepts::ReadMap<Digraph::Arc, VType> CostMap;
  typedef Digraph::Node Node;
  typedef Digraph::Arc Arc;
  typedef concepts::WriteMap<Digraph::Arc, bool> ArbMap;
  typedef concepts::ReadWriteMap<Digraph::Node, Digraph::Arc> PredMap;

  typedef MinCostArborescence<Digraph, CostMap>::
            SetArborescenceMap<ArbMap>::
            SetPredMap<PredMap>::Create MinCostArbType;

  Digraph g;
  Node s, n;
  Arc e;
  VType c;
  bool b;
  ::lemon::ignore_unused_variable_warning(c,b);
  int i;
  CostMap cost;
  ArbMap arb;
  PredMap pred;

  MinCostArbType mcarb_test(g, cost);
  const MinCostArbType& const_mcarb_test = mcarb_test;

  mcarb_test
    .arborescenceMap(arb)
    .predMap(pred)
    .run(s);

  mcarb_test.init();
  mcarb_test.addSource(s);
  mcarb_test.start();
  n = mcarb_test.processNextNode();
  b = const_mcarb_test.emptyQueue();
  i = const_mcarb_test.queueSize();

  c = const_mcarb_test.arborescenceCost();
  b = const_mcarb_test.arborescence(e);
  e = const_mcarb_test.pred(n);
  const MinCostArbType::ArborescenceMap &am =
    const_mcarb_test.arborescenceMap();
  const MinCostArbType::PredMap &pm =
    const_mcarb_test.predMap();
  b = const_mcarb_test.reached(n);
  b = const_mcarb_test.processed(n);

  i = const_mcarb_test.dualNum();
  c = const_mcarb_test.dualValue();
  i = const_mcarb_test.dualSize(i);
  c = const_mcarb_test.dualValue(i);

  ::lemon::ignore_unused_variable_warning(am);
  ::lemon::ignore_unused_variable_warning(pm);
}

int main() {
  typedef SmartDigraph Digraph;
  DIGRAPH_TYPEDEFS(Digraph);

  typedef Digraph::ArcMap<double> CostMap;

  Digraph digraph;
  CostMap cost(digraph);
  Node source;

  std::istringstream is(test_lgf);
  digraphReader(digraph, is).
    arcMap("cost", cost).
    node("source", source).run();

  MinCostArborescence<Digraph, CostMap> mca(digraph, cost);
  mca.run(source);

  vector<pair<double, set<Node> > > dualSolution(mca.dualNum());

  for (int i = 0; i < mca.dualNum(); ++i) {
    dualSolution[i].first = mca.dualValue(i);
    for (MinCostArborescence<Digraph, CostMap>::DualIt it(mca, i);
         it != INVALID; ++it) {
      dualSolution[i].second.insert(it);
    }
  }

  for (ArcIt it(digraph); it != INVALID; ++it) {
    if (mca.reached(digraph.source(it))) {
      double sum = 0.0;
      for (int i = 0; i < int(dualSolution.size()); ++i) {
        if (dualSolution[i].second.find(digraph.target(it))
            != dualSolution[i].second.end() &&
            dualSolution[i].second.find(digraph.source(it))
            == dualSolution[i].second.end()) {
          sum += dualSolution[i].first;
        }
      }
      if (mca.arborescence(it)) {
        check(sum == cost[it], "Invalid dual solution");
      }
      check(sum <= cost[it], "Invalid dual solution");
    }
  }


  check(mca.dualValue() == mca.arborescenceCost(), "Invalid dual solution");

  check(mca.reached(source), "Invalid arborescence");
  for (ArcIt a(digraph); a != INVALID; ++a) {
    check(!mca.reached(digraph.source(a)) ||
          mca.reached(digraph.target(a)), "Invalid arborescence");
  }

  for (NodeIt n(digraph); n != INVALID; ++n) {
    if (!mca.reached(n)) continue;
    int cnt = 0;
    for (InArcIt a(digraph, n); a != INVALID; ++a) {
      if (mca.arborescence(a)) {
        check(mca.pred(n) == a, "Invalid arborescence");
        ++cnt;
      }
    }
    check((n == source ? cnt == 0 : cnt == 1), "Invalid arborescence");
  }

  Digraph::ArcMap<bool> arborescence(digraph);
  check(mca.arborescenceCost() ==
        minCostArborescence(digraph, cost, source, arborescence),
        "Wrong result of the function interface");

  return 0;
}
