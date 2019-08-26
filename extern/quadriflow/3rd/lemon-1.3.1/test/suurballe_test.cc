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

#include <lemon/list_graph.h>
#include <lemon/lgf_reader.h>
#include <lemon/path.h>
#include <lemon/suurballe.h>
#include <lemon/concepts/digraph.h>
#include <lemon/concepts/heap.h>

#include "test_tools.h"

using namespace lemon;

char test_lgf[] =
  "@nodes\n"
  "label\n"
  "1\n"
  "2\n"
  "3\n"
  "4\n"
  "5\n"
  "6\n"
  "7\n"
  "8\n"
  "9\n"
  "10\n"
  "11\n"
  "12\n"
  "@arcs\n"
  "      length\n"
  " 1  2  70\n"
  " 1  3 150\n"
  " 1  4  80\n"
  " 2  8  80\n"
  " 3  5 140\n"
  " 4  6  60\n"
  " 4  7  80\n"
  " 4  8 110\n"
  " 5  7  60\n"
  " 5 11 120\n"
  " 6  3   0\n"
  " 6  9 140\n"
  " 6 10  90\n"
  " 7  1  30\n"
  " 8 12  60\n"
  " 9 12  50\n"
  "10 12  70\n"
  "10  2 100\n"
  "10  7  60\n"
  "11 10  20\n"
  "12 11  30\n"
  "@attributes\n"
  "source  1\n"
  "target 12\n"
  "@end\n";

// Check the interface of Suurballe
void checkSuurballeCompile()
{
  typedef int VType;
  typedef concepts::Digraph Digraph;

  typedef Digraph::Node Node;
  typedef Digraph::Arc Arc;
  typedef concepts::ReadMap<Arc, VType> LengthMap;

  typedef Suurballe<Digraph, LengthMap> ST;
  typedef Suurballe<Digraph, LengthMap>
    ::SetFlowMap<ST::FlowMap>
    ::SetPotentialMap<ST::PotentialMap>
    ::SetPath<SimplePath<Digraph> >
    ::SetHeap<concepts::Heap<VType, Digraph::NodeMap<int> > >
    ::Create SuurballeType;

  Digraph g;
  Node n;
  Arc e;
  LengthMap len;
  SuurballeType::FlowMap flow(g);
  SuurballeType::PotentialMap pi(g);

  SuurballeType suurb_test(g, len);
  const SuurballeType& const_suurb_test = suurb_test;

  suurb_test
    .flowMap(flow)
    .potentialMap(pi);

  int k;
  k = suurb_test.run(n, n);
  k = suurb_test.run(n, n, k);
  suurb_test.init(n);
  suurb_test.fullInit(n);
  suurb_test.start(n);
  suurb_test.start(n, k);
  k = suurb_test.findFlow(n);
  k = suurb_test.findFlow(n, k);
  suurb_test.findPaths();

  int f;
  VType c;
  ::lemon::ignore_unused_variable_warning(f,c);

  c = const_suurb_test.totalLength();
  f = const_suurb_test.flow(e);
  const SuurballeType::FlowMap& fm =
    const_suurb_test.flowMap();
  c = const_suurb_test.potential(n);
  const SuurballeType::PotentialMap& pm =
    const_suurb_test.potentialMap();
  k = const_suurb_test.pathNum();
  Path<Digraph> p = const_suurb_test.path(k);

  ::lemon::ignore_unused_variable_warning(fm);
  ::lemon::ignore_unused_variable_warning(pm);
}

// Check the feasibility of the flow
template <typename Digraph, typename FlowMap>
bool checkFlow( const Digraph& gr, const FlowMap& flow,
                typename Digraph::Node s, typename Digraph::Node t,
                int value )
{
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);
  for (ArcIt e(gr); e != INVALID; ++e)
    if (!(flow[e] == 0 || flow[e] == 1)) return false;

  for (NodeIt n(gr); n != INVALID; ++n) {
    int sum = 0;
    for (OutArcIt e(gr, n); e != INVALID; ++e)
      sum += flow[e];
    for (InArcIt e(gr, n); e != INVALID; ++e)
      sum -= flow[e];
    if (n == s && sum != value) return false;
    if (n == t && sum != -value) return false;
    if (n != s && n != t && sum != 0) return false;
  }

  return true;
}

// Check the optimalitiy of the flow
template < typename Digraph, typename CostMap,
           typename FlowMap, typename PotentialMap >
bool checkOptimality( const Digraph& gr, const CostMap& cost,
                      const FlowMap& flow, const PotentialMap& pi )
{
  // Check the "Complementary Slackness" optimality condition
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);
  bool opt = true;
  for (ArcIt e(gr); e != INVALID; ++e) {
    typename CostMap::Value red_cost =
      cost[e] + pi[gr.source(e)] - pi[gr.target(e)];
    opt = (flow[e] == 0 && red_cost >= 0) ||
          (flow[e] == 1 && red_cost <= 0);
    if (!opt) break;
  }
  return opt;
}

// Check a path
template <typename Digraph, typename Path>
bool checkPath( const Digraph& gr, const Path& path,
                typename Digraph::Node s, typename Digraph::Node t)
{
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);
  Node n = s;
  for (int i = 0; i < path.length(); ++i) {
    if (gr.source(path.nth(i)) != n) return false;
    n = gr.target(path.nth(i));
  }
  return n == t;
}


int main()
{
  DIGRAPH_TYPEDEFS(ListDigraph);

  // Read the test digraph
  ListDigraph digraph;
  ListDigraph::ArcMap<int> length(digraph);
  Node s, t;

  std::istringstream input(test_lgf);
  DigraphReader<ListDigraph>(digraph, input).
    arcMap("length", length).
    node("source", s).
    node("target", t).
    run();

  // Check run()
  {
    Suurballe<ListDigraph> suurballe(digraph, length);

    // Find 2 paths
    check(suurballe.run(s, t) == 2, "Wrong number of paths");
    check(checkFlow(digraph, suurballe.flowMap(), s, t, 2),
          "The flow is not feasible");
    check(suurballe.totalLength() == 510, "The flow is not optimal");
    check(checkOptimality(digraph, length, suurballe.flowMap(),
                          suurballe.potentialMap()),
          "Wrong potentials");
    for (int i = 0; i < suurballe.pathNum(); ++i)
      check(checkPath(digraph, suurballe.path(i), s, t), "Wrong path");

    // Find 3 paths
    check(suurballe.run(s, t, 3) == 3, "Wrong number of paths");
    check(checkFlow(digraph, suurballe.flowMap(), s, t, 3),
          "The flow is not feasible");
    check(suurballe.totalLength() == 1040, "The flow is not optimal");
    check(checkOptimality(digraph, length, suurballe.flowMap(),
                          suurballe.potentialMap()),
          "Wrong potentials");
    for (int i = 0; i < suurballe.pathNum(); ++i)
      check(checkPath(digraph, suurballe.path(i), s, t), "Wrong path");

    // Find 5 paths (only 3 can be found)
    check(suurballe.run(s, t, 5) == 3, "Wrong number of paths");
    check(checkFlow(digraph, suurballe.flowMap(), s, t, 3),
          "The flow is not feasible");
    check(suurballe.totalLength() == 1040, "The flow is not optimal");
    check(checkOptimality(digraph, length, suurballe.flowMap(),
                          suurballe.potentialMap()),
          "Wrong potentials");
    for (int i = 0; i < suurballe.pathNum(); ++i)
      check(checkPath(digraph, suurballe.path(i), s, t), "Wrong path");
  }

  // Check fullInit() + start()
  {
    Suurballe<ListDigraph> suurballe(digraph, length);
    suurballe.fullInit(s);

    // Find 2 paths
    check(suurballe.start(t) == 2, "Wrong number of paths");
    check(suurballe.totalLength() == 510, "The flow is not optimal");

    // Find 3 paths
    check(suurballe.start(t, 3) == 3, "Wrong number of paths");
    check(suurballe.totalLength() == 1040, "The flow is not optimal");

    // Find 5 paths (only 3 can be found)
    check(suurballe.start(t, 5) == 3, "Wrong number of paths");
    check(suurballe.totalLength() == 1040, "The flow is not optimal");
  }

  return 0;
}
