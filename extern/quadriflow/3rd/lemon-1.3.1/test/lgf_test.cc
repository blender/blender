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

#include <lemon/list_graph.h>
#include <lemon/lgf_reader.h>
#include "test_tools.h"

using namespace lemon;

char test_lgf[] =
  "@nodes\n"
  "label\n"
  "0\n"
  "1\n"
  "@arcs\n"
  "     label\n"
  "0 1  0\n"
  "1 0  1\n"
  "@attributes\n"
  "source 0\n"
  "target 1\n";

char test_lgf_nomap[] =
  "@nodes\n"
  "label\n"
  "0\n"
  "1\n"
  "@arcs\n"
  "     -\n"
  "0 1\n";

char test_lgf_bad1[] =
  "@nodes\n"
  "label\n"
  "0\n"
  "1\n"
  "@arcs\n"
  "     - another\n"
  "0 1\n";

char test_lgf_bad2[] =
  "@nodes\n"
  "label\n"
  "0\n"
  "1\n"
  "@arcs\n"
  "     label -\n"
  "0 1\n";


int main()
{
  {
    ListDigraph d;
    ListDigraph::Node s,t;
    ListDigraph::ArcMap<int> label(d);
    std::istringstream input(test_lgf);
    digraphReader(d, input).
      node("source", s).
      node("target", t).
      arcMap("label", label).
      run();
    check(countNodes(d) == 2,"There should be 2 nodes");
    check(countArcs(d) == 2,"There should be 2 arcs");
  }
  {
    ListGraph g;
    ListGraph::Node s,t;
    ListGraph::EdgeMap<int> label(g);
    std::istringstream input(test_lgf);
    graphReader(g, input).
      node("source", s).
      node("target", t).
      edgeMap("label", label).
      run();
    check(countNodes(g) == 2,"There should be 2 nodes");
    check(countEdges(g) == 2,"There should be 2 arcs");
  }

  {
    ListDigraph d;
    std::istringstream input(test_lgf_nomap);
    digraphReader(d, input).
      run();
    check(countNodes(d) == 2,"There should be 2 nodes");
    check(countArcs(d) == 1,"There should be 1 arc");
  }
  {
    ListGraph g;
    std::istringstream input(test_lgf_nomap);
    graphReader(g, input).
      run();
    check(countNodes(g) == 2,"There should be 2 nodes");
    check(countEdges(g) == 1,"There should be 1 edge");
  }

  {
    ListDigraph d;
    std::istringstream input(test_lgf_bad1);
    bool ok=false;
    try {
      digraphReader(d, input).
        run();
    }
    catch (FormatError&)
      {
        ok = true;
      }
    check(ok,"FormatError exception should have occured");
  }
  {
    ListGraph g;
    std::istringstream input(test_lgf_bad1);
    bool ok=false;
    try {
      graphReader(g, input).
        run();
    }
    catch (FormatError&)
      {
        ok = true;
      }
    check(ok,"FormatError exception should have occured");
  }

  {
    ListDigraph d;
    std::istringstream input(test_lgf_bad2);
    bool ok=false;
    try {
      digraphReader(d, input).
        run();
    }
    catch (FormatError&)
      {
        ok = true;
      }
    check(ok,"FormatError exception should have occured");
  }
  {
    ListGraph g;
    std::istringstream input(test_lgf_bad2);
    bool ok=false;
    try {
      graphReader(g, input).
        run();
    }
    catch (FormatError&)
      {
        ok = true;
      }
    check(ok,"FormatError exception should have occured");
  }
}
