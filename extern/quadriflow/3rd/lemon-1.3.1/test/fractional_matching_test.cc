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
#include <sstream>
#include <vector>
#include <queue>
#include <cstdlib>

#include <lemon/fractional_matching.h>
#include <lemon/smart_graph.h>
#include <lemon/concepts/graph.h>
#include <lemon/concepts/maps.h>
#include <lemon/lgf_reader.h>
#include <lemon/math.h>

#include "test_tools.h"

using namespace std;
using namespace lemon;

GRAPH_TYPEDEFS(SmartGraph);


const int lgfn = 4;
const std::string lgf[lgfn] = {
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
  "@edges\n"
  "     label  weight\n"
  "7 4  0      984\n"
  "0 7  1      73\n"
  "7 1  2      204\n"
  "2 3  3      583\n"
  "2 7  4      565\n"
  "2 1  5      582\n"
  "0 4  6      551\n"
  "2 5  7      385\n"
  "1 5  8      561\n"
  "5 3  9      484\n"
  "7 5  10     904\n"
  "3 6  11     47\n"
  "7 6  12     888\n"
  "3 0  13     747\n"
  "6 1  14     310\n",

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
  "@edges\n"
  "     label  weight\n"
  "2 5  0      710\n"
  "0 5  1      241\n"
  "2 4  2      856\n"
  "2 6  3      762\n"
  "4 1  4      747\n"
  "6 1  5      962\n"
  "4 7  6      723\n"
  "1 7  7      661\n"
  "2 3  8      376\n"
  "1 0  9      416\n"
  "6 7  10     391\n",

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
  "@edges\n"
  "     label  weight\n"
  "6 2  0      553\n"
  "0 7  1      653\n"
  "6 3  2      22\n"
  "4 7  3      846\n"
  "7 2  4      981\n"
  "7 6  5      250\n"
  "5 2  6      539\n",

  "@nodes\n"
  "label\n"
  "0\n"
  "@edges\n"
  "     label  weight\n"
  "0 0  0      100\n"
};

void checkMaxFractionalMatchingCompile()
{
  typedef concepts::Graph Graph;
  typedef Graph::Node Node;
  typedef Graph::Edge Edge;

  Graph g;
  Node n;
  Edge e;

  MaxFractionalMatching<Graph> mat_test(g);
  const MaxFractionalMatching<Graph>&
    const_mat_test = mat_test;

  mat_test.init();
  mat_test.start();
  mat_test.start(true);
  mat_test.startPerfect();
  mat_test.startPerfect(true);
  mat_test.run();
  mat_test.run(true);
  mat_test.runPerfect();
  mat_test.runPerfect(true);

  const_mat_test.matchingSize();
  const_mat_test.matching(e);
  const_mat_test.matching(n);
  const MaxFractionalMatching<Graph>::MatchingMap& mmap =
    const_mat_test.matchingMap();
  e = mmap[n];

  const_mat_test.barrier(n);
}

void checkMaxWeightedFractionalMatchingCompile()
{
  typedef concepts::Graph Graph;
  typedef Graph::Node Node;
  typedef Graph::Edge Edge;
  typedef Graph::EdgeMap<int> WeightMap;

  Graph g;
  Node n;
  Edge e;
  WeightMap w(g);

  MaxWeightedFractionalMatching<Graph> mat_test(g, w);
  const MaxWeightedFractionalMatching<Graph>&
    const_mat_test = mat_test;

  mat_test.init();
  mat_test.start();
  mat_test.run();

  const_mat_test.matchingWeight();
  const_mat_test.matchingSize();
  const_mat_test.matching(e);
  const_mat_test.matching(n);
  const MaxWeightedFractionalMatching<Graph>::MatchingMap& mmap =
    const_mat_test.matchingMap();
  e = mmap[n];

  const_mat_test.dualValue();
  const_mat_test.nodeValue(n);
}

void checkMaxWeightedPerfectFractionalMatchingCompile()
{
  typedef concepts::Graph Graph;
  typedef Graph::Node Node;
  typedef Graph::Edge Edge;
  typedef Graph::EdgeMap<int> WeightMap;

  Graph g;
  Node n;
  Edge e;
  WeightMap w(g);

  MaxWeightedPerfectFractionalMatching<Graph> mat_test(g, w);
  const MaxWeightedPerfectFractionalMatching<Graph>&
    const_mat_test = mat_test;

  mat_test.init();
  mat_test.start();
  mat_test.run();

  const_mat_test.matchingWeight();
  const_mat_test.matching(e);
  const_mat_test.matching(n);
  const MaxWeightedPerfectFractionalMatching<Graph>::MatchingMap& mmap =
    const_mat_test.matchingMap();
  e = mmap[n];

  const_mat_test.dualValue();
  const_mat_test.nodeValue(n);
}

void checkFractionalMatching(const SmartGraph& graph,
                             const MaxFractionalMatching<SmartGraph>& mfm,
                             bool allow_loops = true) {
  int pv = 0;
  for (SmartGraph::NodeIt n(graph); n != INVALID; ++n) {
    int indeg = 0;
    for (InArcIt a(graph, n); a != INVALID; ++a) {
      if (mfm.matching(graph.source(a)) == a) {
        ++indeg;
      }
    }
    if (mfm.matching(n) != INVALID) {
      check(indeg == 1, "Invalid matching");
      ++pv;
    } else {
      check(indeg == 0, "Invalid matching");
    }
  }
  check(pv == mfm.matchingSize(), "Wrong matching size");

  for (SmartGraph::EdgeIt e(graph); e != INVALID; ++e) {
    check((e == mfm.matching(graph.u(e)) ? 1 : 0) +
          (e == mfm.matching(graph.v(e)) ? 1 : 0) ==
          mfm.matching(e), "Invalid matching");
  }

  SmartGraph::NodeMap<bool> processed(graph, false);
  for (SmartGraph::NodeIt n(graph); n != INVALID; ++n) {
    if (processed[n]) continue;
    processed[n] = true;
    if (mfm.matching(n) == INVALID) continue;
    int num = 1;
    Node v = graph.target(mfm.matching(n));
    while (v != n) {
      processed[v] = true;
      ++num;
      v = graph.target(mfm.matching(v));
    }
    check(num == 2 || num % 2 == 1, "Wrong cycle size");
    check(allow_loops || num != 1, "Wrong cycle size");
  }

  int anum = 0, bnum = 0;
  SmartGraph::NodeMap<bool> neighbours(graph, false);
  for (SmartGraph::NodeIt n(graph); n != INVALID; ++n) {
    if (!mfm.barrier(n)) continue;
    ++anum;
    for (SmartGraph::InArcIt a(graph, n); a != INVALID; ++a) {
      Node u = graph.source(a);
      if (!allow_loops && u == n) continue;
      if (!neighbours[u]) {
        neighbours[u] = true;
        ++bnum;
      }
    }
  }
  check(anum - bnum + mfm.matchingSize() == countNodes(graph),
        "Wrong barrier");
}

void checkPerfectFractionalMatching(const SmartGraph& graph,
                             const MaxFractionalMatching<SmartGraph>& mfm,
                             bool perfect, bool allow_loops = true) {
  if (perfect) {
    for (SmartGraph::NodeIt n(graph); n != INVALID; ++n) {
      int indeg = 0;
      for (InArcIt a(graph, n); a != INVALID; ++a) {
        if (mfm.matching(graph.source(a)) == a) {
          ++indeg;
        }
      }
      check(mfm.matching(n) != INVALID, "Invalid matching");
      check(indeg == 1, "Invalid matching");
    }
    for (SmartGraph::EdgeIt e(graph); e != INVALID; ++e) {
      check((e == mfm.matching(graph.u(e)) ? 1 : 0) +
            (e == mfm.matching(graph.v(e)) ? 1 : 0) ==
            mfm.matching(e), "Invalid matching");
    }
  } else {
    int anum = 0, bnum = 0;
    SmartGraph::NodeMap<bool> neighbours(graph, false);
    for (SmartGraph::NodeIt n(graph); n != INVALID; ++n) {
      if (!mfm.barrier(n)) continue;
      ++anum;
      for (SmartGraph::InArcIt a(graph, n); a != INVALID; ++a) {
        Node u = graph.source(a);
        if (!allow_loops && u == n) continue;
        if (!neighbours[u]) {
          neighbours[u] = true;
          ++bnum;
        }
      }
    }
    check(anum - bnum > 0, "Wrong barrier");
  }
}

void checkWeightedFractionalMatching(const SmartGraph& graph,
                   const SmartGraph::EdgeMap<int>& weight,
                   const MaxWeightedFractionalMatching<SmartGraph>& mwfm,
                   bool allow_loops = true) {
  for (SmartGraph::EdgeIt e(graph); e != INVALID; ++e) {
    if (graph.u(e) == graph.v(e) && !allow_loops) continue;
    int rw = mwfm.nodeValue(graph.u(e)) + mwfm.nodeValue(graph.v(e))
      - weight[e] * mwfm.dualScale;

    check(rw >= 0, "Negative reduced weight");
    check(rw == 0 || !mwfm.matching(e),
          "Non-zero reduced weight on matching edge");
  }

  int pv = 0;
  for (SmartGraph::NodeIt n(graph); n != INVALID; ++n) {
    int indeg = 0;
    for (InArcIt a(graph, n); a != INVALID; ++a) {
      if (mwfm.matching(graph.source(a)) == a) {
        ++indeg;
      }
    }
    check(indeg <= 1, "Invalid matching");
    if (mwfm.matching(n) != INVALID) {
      check(mwfm.nodeValue(n) >= 0, "Invalid node value");
      check(indeg == 1, "Invalid matching");
      pv += weight[mwfm.matching(n)];
      SmartGraph::Node o = graph.target(mwfm.matching(n));
      ::lemon::ignore_unused_variable_warning(o);
    } else {
      check(mwfm.nodeValue(n) == 0, "Invalid matching");
      check(indeg == 0, "Invalid matching");
    }
  }

  for (SmartGraph::EdgeIt e(graph); e != INVALID; ++e) {
    check((e == mwfm.matching(graph.u(e)) ? 1 : 0) +
          (e == mwfm.matching(graph.v(e)) ? 1 : 0) ==
          mwfm.matching(e), "Invalid matching");
  }

  int dv = 0;
  for (SmartGraph::NodeIt n(graph); n != INVALID; ++n) {
    dv += mwfm.nodeValue(n);
  }

  check(pv * mwfm.dualScale == dv * 2, "Wrong duality");

  SmartGraph::NodeMap<bool> processed(graph, false);
  for (SmartGraph::NodeIt n(graph); n != INVALID; ++n) {
    if (processed[n]) continue;
    processed[n] = true;
    if (mwfm.matching(n) == INVALID) continue;
    int num = 1;
    Node v = graph.target(mwfm.matching(n));
    while (v != n) {
      processed[v] = true;
      ++num;
      v = graph.target(mwfm.matching(v));
    }
    check(num == 2 || num % 2 == 1, "Wrong cycle size");
    check(allow_loops || num != 1, "Wrong cycle size");
  }

  return;
}

void checkWeightedPerfectFractionalMatching(const SmartGraph& graph,
                const SmartGraph::EdgeMap<int>& weight,
                const MaxWeightedPerfectFractionalMatching<SmartGraph>& mwpfm,
                bool allow_loops = true) {
  for (SmartGraph::EdgeIt e(graph); e != INVALID; ++e) {
    if (graph.u(e) == graph.v(e) && !allow_loops) continue;
    int rw = mwpfm.nodeValue(graph.u(e)) + mwpfm.nodeValue(graph.v(e))
      - weight[e] * mwpfm.dualScale;

    check(rw >= 0, "Negative reduced weight");
    check(rw == 0 || !mwpfm.matching(e),
          "Non-zero reduced weight on matching edge");
  }

  int pv = 0;
  for (SmartGraph::NodeIt n(graph); n != INVALID; ++n) {
    int indeg = 0;
    for (InArcIt a(graph, n); a != INVALID; ++a) {
      if (mwpfm.matching(graph.source(a)) == a) {
        ++indeg;
      }
    }
    check(mwpfm.matching(n) != INVALID, "Invalid perfect matching");
    check(indeg == 1, "Invalid perfect matching");
    pv += weight[mwpfm.matching(n)];
    SmartGraph::Node o = graph.target(mwpfm.matching(n));
    ::lemon::ignore_unused_variable_warning(o);
  }

  for (SmartGraph::EdgeIt e(graph); e != INVALID; ++e) {
    check((e == mwpfm.matching(graph.u(e)) ? 1 : 0) +
          (e == mwpfm.matching(graph.v(e)) ? 1 : 0) ==
          mwpfm.matching(e), "Invalid matching");
  }

  int dv = 0;
  for (SmartGraph::NodeIt n(graph); n != INVALID; ++n) {
    dv += mwpfm.nodeValue(n);
  }

  check(pv * mwpfm.dualScale == dv * 2, "Wrong duality");

  SmartGraph::NodeMap<bool> processed(graph, false);
  for (SmartGraph::NodeIt n(graph); n != INVALID; ++n) {
    if (processed[n]) continue;
    processed[n] = true;
    if (mwpfm.matching(n) == INVALID) continue;
    int num = 1;
    Node v = graph.target(mwpfm.matching(n));
    while (v != n) {
      processed[v] = true;
      ++num;
      v = graph.target(mwpfm.matching(v));
    }
    check(num == 2 || num % 2 == 1, "Wrong cycle size");
    check(allow_loops || num != 1, "Wrong cycle size");
  }

  return;
}


int main() {

  for (int i = 0; i < lgfn; ++i) {
    SmartGraph graph;
    SmartGraph::EdgeMap<int> weight(graph);

    istringstream lgfs(lgf[i]);
    graphReader(graph, lgfs).
      edgeMap("weight", weight).run();

    bool perfect_with_loops;
    {
      MaxFractionalMatching<SmartGraph> mfm(graph, true);
      mfm.run();
      checkFractionalMatching(graph, mfm, true);
      perfect_with_loops = mfm.matchingSize() == countNodes(graph);
    }

    bool perfect_without_loops;
    {
      MaxFractionalMatching<SmartGraph> mfm(graph, false);
      mfm.run();
      checkFractionalMatching(graph, mfm, false);
      perfect_without_loops = mfm.matchingSize() == countNodes(graph);
    }

    {
      MaxFractionalMatching<SmartGraph> mfm(graph, true);
      bool result = mfm.runPerfect();
      checkPerfectFractionalMatching(graph, mfm, result, true);
      check(result == perfect_with_loops, "Wrong perfect matching");
    }

    {
      MaxFractionalMatching<SmartGraph> mfm(graph, false);
      bool result = mfm.runPerfect();
      checkPerfectFractionalMatching(graph, mfm, result, false);
      check(result == perfect_without_loops, "Wrong perfect matching");
    }

    {
      MaxWeightedFractionalMatching<SmartGraph> mwfm(graph, weight, true);
      mwfm.run();
      checkWeightedFractionalMatching(graph, weight, mwfm, true);
    }

    {
      MaxWeightedFractionalMatching<SmartGraph> mwfm(graph, weight, false);
      mwfm.run();
      checkWeightedFractionalMatching(graph, weight, mwfm, false);
    }

    {
      MaxWeightedPerfectFractionalMatching<SmartGraph> mwpfm(graph, weight,
                                                             true);
      bool perfect = mwpfm.run();
      check(perfect == (mwpfm.matchingSize() == countNodes(graph)),
            "Perfect matching found");
      check(perfect == perfect_with_loops, "Wrong perfect matching");

      if (perfect) {
        checkWeightedPerfectFractionalMatching(graph, weight, mwpfm, true);
      }
    }

    {
      MaxWeightedPerfectFractionalMatching<SmartGraph> mwpfm(graph, weight,
                                                             false);
      bool perfect = mwpfm.run();
      check(perfect == (mwpfm.matchingSize() == countNodes(graph)),
            "Perfect matching found");
      check(perfect == perfect_without_loops, "Wrong perfect matching");

      if (perfect) {
        checkWeightedPerfectFractionalMatching(graph, weight, mwpfm, false);
      }
    }

  }

  return 0;
}
