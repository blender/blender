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

#include "test_tools.h"
#include <lemon/list_graph.h>
#include <lemon/circulation.h>
#include <lemon/lgf_reader.h>
#include <lemon/concepts/digraph.h>
#include <lemon/concepts/maps.h>

using namespace lemon;

char test_lgf[] =
  "@nodes\n"
  "label\n"
  "0\n"
  "1\n"
  "2\n"
  "3\n"
  "4\n"
  "5\n"
  "@arcs\n"
  "     lcap  ucap\n"
  "0 1  2  10\n"
  "0 2  2  6\n"
  "1 3  4  7\n"
  "1 4  0  5\n"
  "2 4  1  3\n"
  "3 5  3  8\n"
  "4 5  3  7\n"
  "@attributes\n"
  "source 0\n"
  "sink   5\n";

void checkCirculationCompile()
{
  typedef int VType;
  typedef concepts::Digraph Digraph;

  typedef Digraph::Node Node;
  typedef Digraph::Arc Arc;
  typedef concepts::ReadMap<Arc,VType> CapMap;
  typedef concepts::ReadMap<Node,VType> SupplyMap;
  typedef concepts::ReadWriteMap<Arc,VType> FlowMap;
  typedef concepts::WriteMap<Node,bool> BarrierMap;

  typedef Elevator<Digraph, Digraph::Node> Elev;
  typedef LinkedElevator<Digraph, Digraph::Node> LinkedElev;

  Digraph g;
  Node n;
  Arc a;
  CapMap lcap, ucap;
  SupplyMap supply;
  FlowMap flow;
  BarrierMap bar;
  VType v;
  bool b;
  ::lemon::ignore_unused_variable_warning(v,b);

  typedef Circulation<Digraph, CapMap, CapMap, SupplyMap>
            ::SetFlowMap<FlowMap>
            ::SetElevator<Elev>
            ::SetStandardElevator<LinkedElev>
            ::Create CirculationType;
  CirculationType circ_test(g, lcap, ucap, supply);
  const CirculationType& const_circ_test = circ_test;

  circ_test
    .lowerMap(lcap)
    .upperMap(ucap)
    .supplyMap(supply)
    .flowMap(flow);

  const CirculationType::Elevator& elev = const_circ_test.elevator();
  circ_test.elevator(const_cast<CirculationType::Elevator&>(elev));
  CirculationType::Tolerance tol = const_circ_test.tolerance();
  circ_test.tolerance(tol);

  circ_test.init();
  circ_test.greedyInit();
  circ_test.start();
  circ_test.run();

  v = const_circ_test.flow(a);
  const FlowMap& fm = const_circ_test.flowMap();
  b = const_circ_test.barrier(n);
  const_circ_test.barrierMap(bar);

  ::lemon::ignore_unused_variable_warning(fm);
}

template <class G, class LM, class UM, class DM>
void checkCirculation(const G& g, const LM& lm, const UM& um,
                      const DM& dm, bool find)
{
  Circulation<G, LM, UM, DM> circ(g, lm, um, dm);
  bool ret = circ.run();
  if (find) {
    check(ret, "A feasible solution should have been found.");
    check(circ.checkFlow(), "The found flow is corrupt.");
    check(!circ.checkBarrier(), "A barrier should not have been found.");
  } else {
    check(!ret, "A feasible solution should not have been found.");
    check(circ.checkBarrier(), "The found barrier is corrupt.");
  }
}

int main (int, char*[])
{
  typedef ListDigraph Digraph;
  DIGRAPH_TYPEDEFS(Digraph);

  Digraph g;
  IntArcMap lo(g), up(g);
  IntNodeMap delta(g, 0);
  Node s, t;

  std::istringstream input(test_lgf);
  DigraphReader<Digraph>(g,input).
    arcMap("lcap", lo).
    arcMap("ucap", up).
    node("source",s).
    node("sink",t).
    run();

  delta[s] = 7; delta[t] = -7;
  checkCirculation(g, lo, up, delta, true);

  delta[s] = 13; delta[t] = -13;
  checkCirculation(g, lo, up, delta, true);

  delta[s] = 6; delta[t] = -6;
  checkCirculation(g, lo, up, delta, false);

  delta[s] = 14; delta[t] = -14;
  checkCirculation(g, lo, up, delta, false);

  delta[s] = 7; delta[t] = -13;
  checkCirculation(g, lo, up, delta, true);

  delta[s] = 5; delta[t] = -15;
  checkCirculation(g, lo, up, delta, true);

  delta[s] = 10; delta[t] = -11;
  checkCirculation(g, lo, up, delta, true);

  delta[s] = 11; delta[t] = -10;
  checkCirculation(g, lo, up, delta, false);

  return 0;
}
