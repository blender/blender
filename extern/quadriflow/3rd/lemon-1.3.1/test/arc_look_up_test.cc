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
#include "lemon/list_graph.h"
#include "lemon/lgf_reader.h"

#include "test_tools.h"

using namespace lemon;

const std::string lgf =
  "@nodes\n"
"label\n"
"0\n"
"1\n"
"2\n"
"3\n"
"4\n"
"5\n"
"6\n"
"@arcs\n"
"label\n"
"5 6 0\n"
"5 4 1\n"
"4 6 2\n"
"3 4 3\n"
"3 4 4\n"
"3 2 5\n"
"3 5 6\n"
"3 5 7\n"
"3 5 8\n"
"3 5 9\n"
"2 4 10\n"
"2 4 11\n"
"2 4 12\n"
"2 4 13\n"
"1 2 14\n"
"1 2 15\n"
"1 0 16\n"
"1 3 17\n"
"1 3 18\n"
"1 3 19\n"
"1 3 20\n"
"0 2 21\n"
"0 2 22\n"
"0 2 23\n"
"0 2 24\n";


int main() {
  ListDigraph graph;
  std::istringstream lgfs(lgf);
  DigraphReader<ListDigraph>(graph, lgfs).run();

  AllArcLookUp<ListDigraph> lookup(graph);

  int numArcs = countArcs(graph);

  int arcCnt = 0;
  for(ListDigraph::NodeIt n1(graph); n1 != INVALID; ++n1)
    for(ListDigraph::NodeIt n2(graph); n2 != INVALID; ++n2)
      for(ListDigraph::Arc a = lookup(n1, n2); a != INVALID;
          a = lookup(n1, n2, a))
        ++arcCnt;
  check(arcCnt==numArcs, "Wrong total number of arcs");

  return 0;
}
