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

#include <lemon/euler.h>
#include <lemon/list_graph.h>
#include <lemon/adaptors.h>
#include "test_tools.h"

using namespace lemon;

template <typename Digraph>
void checkDiEulerIt(const Digraph& g,
                    const typename Digraph::Node& start = INVALID)
{
  typename Digraph::template ArcMap<int> visitationNumber(g, 0);

  DiEulerIt<Digraph> e(g, start);
  if (e == INVALID) return;
  typename Digraph::Node firstNode = g.source(e);
  typename Digraph::Node lastNode = g.target(e);
  if (start != INVALID) {
    check(firstNode == start, "checkDiEulerIt: Wrong first node");
  }

  for (; e != INVALID; ++e) {
    if (e != INVALID) lastNode = g.target(e);
    ++visitationNumber[e];
  }

  check(firstNode == lastNode,
      "checkDiEulerIt: First and last nodes are not the same");

  for (typename Digraph::ArcIt a(g); a != INVALID; ++a)
  {
    check(visitationNumber[a] == 1,
        "checkDiEulerIt: Not visited or multiple times visited arc found");
  }
}

template <typename Graph>
void checkEulerIt(const Graph& g,
                  const typename Graph::Node& start = INVALID)
{
  typename Graph::template EdgeMap<int> visitationNumber(g, 0);

  EulerIt<Graph> e(g, start);
  if (e == INVALID) return;
  typename Graph::Node firstNode = g.source(typename Graph::Arc(e));
  typename Graph::Node lastNode = g.target(typename Graph::Arc(e));
  if (start != INVALID) {
    check(firstNode == start, "checkEulerIt: Wrong first node");
  }

  for (; e != INVALID; ++e) {
    if (e != INVALID) lastNode = g.target(typename Graph::Arc(e));
    ++visitationNumber[e];
  }

  check(firstNode == lastNode,
      "checkEulerIt: First and last nodes are not the same");

  for (typename Graph::EdgeIt e(g); e != INVALID; ++e)
  {
    check(visitationNumber[e] == 1,
        "checkEulerIt: Not visited or multiple times visited edge found");
  }
}

int main()
{
  typedef ListDigraph Digraph;
  typedef Undirector<Digraph> Graph;

  {
    Digraph d;
    Graph g(d);

    checkDiEulerIt(d);
    checkDiEulerIt(g);
    checkEulerIt(g);

    check(eulerian(d), "This graph is Eulerian");
    check(eulerian(g), "This graph is Eulerian");
  }
  {
    Digraph d;
    Graph g(d);
    Digraph::Node n = d.addNode();
    ::lemon::ignore_unused_variable_warning(n);

    checkDiEulerIt(d);
    checkDiEulerIt(g);
    checkEulerIt(g);

    check(eulerian(d), "This graph is Eulerian");
    check(eulerian(g), "This graph is Eulerian");
  }
  {
    Digraph d;
    Graph g(d);
    Digraph::Node n = d.addNode();
    d.addArc(n, n);

    checkDiEulerIt(d);
    checkDiEulerIt(g);
    checkEulerIt(g);

    check(eulerian(d), "This graph is Eulerian");
    check(eulerian(g), "This graph is Eulerian");
  }
  {
    Digraph d;
    Graph g(d);
    Digraph::Node n1 = d.addNode();
    Digraph::Node n2 = d.addNode();
    Digraph::Node n3 = d.addNode();

    d.addArc(n1, n2);
    d.addArc(n2, n1);
    d.addArc(n2, n3);
    d.addArc(n3, n2);

    checkDiEulerIt(d);
    checkDiEulerIt(d, n2);
    checkDiEulerIt(g);
    checkDiEulerIt(g, n2);
    checkEulerIt(g);
    checkEulerIt(g, n2);

    check(eulerian(d), "This graph is Eulerian");
    check(eulerian(g), "This graph is Eulerian");
  }
  {
    Digraph d;
    Graph g(d);
    Digraph::Node n1 = d.addNode();
    Digraph::Node n2 = d.addNode();
    Digraph::Node n3 = d.addNode();
    Digraph::Node n4 = d.addNode();
    Digraph::Node n5 = d.addNode();
    Digraph::Node n6 = d.addNode();

    d.addArc(n1, n2);
    d.addArc(n2, n4);
    d.addArc(n1, n3);
    d.addArc(n3, n4);
    d.addArc(n4, n1);
    d.addArc(n3, n5);
    d.addArc(n5, n2);
    d.addArc(n4, n6);
    d.addArc(n2, n6);
    d.addArc(n6, n1);
    d.addArc(n6, n3);

    checkDiEulerIt(d);
    checkDiEulerIt(d, n1);
    checkDiEulerIt(d, n5);

    checkDiEulerIt(g);
    checkDiEulerIt(g, n1);
    checkDiEulerIt(g, n5);
    checkEulerIt(g);
    checkEulerIt(g, n1);
    checkEulerIt(g, n5);

    check(eulerian(d), "This graph is Eulerian");
    check(eulerian(g), "This graph is Eulerian");
  }
  {
    Digraph d;
    Graph g(d);
    Digraph::Node n0 = d.addNode();
    Digraph::Node n1 = d.addNode();
    Digraph::Node n2 = d.addNode();
    Digraph::Node n3 = d.addNode();
    Digraph::Node n4 = d.addNode();
    Digraph::Node n5 = d.addNode();
    ::lemon::ignore_unused_variable_warning(n0,n4,n5);

    d.addArc(n1, n2);
    d.addArc(n2, n3);
    d.addArc(n3, n1);

    checkDiEulerIt(d);
    checkDiEulerIt(d, n2);

    checkDiEulerIt(g);
    checkDiEulerIt(g, n2);
    checkEulerIt(g);
    checkEulerIt(g, n2);

    check(!eulerian(d), "This graph is not Eulerian");
    check(!eulerian(g), "This graph is not Eulerian");
  }
  {
    Digraph d;
    Graph g(d);
    Digraph::Node n1 = d.addNode();
    Digraph::Node n2 = d.addNode();
    Digraph::Node n3 = d.addNode();

    d.addArc(n1, n2);
    d.addArc(n2, n3);

    check(!eulerian(d), "This graph is not Eulerian");
    check(!eulerian(g), "This graph is not Eulerian");
  }

  return 0;
}
