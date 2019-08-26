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

#include <lemon/concepts/digraph.h>
#include <lemon/smart_graph.h>
#include <lemon/list_graph.h>
#include <lemon/lgf_reader.h>
#include <lemon/dfs.h>
#include <lemon/path.h>

#include "graph_test.h"
#include "test_tools.h"

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
  "6\n"
  "@arcs\n"
  "     label\n"
  "0 1  0\n"
  "1 2  1\n"
  "2 3  2\n"
  "1 4  3\n"
  "4 2  4\n"
  "4 5  5\n"
  "5 0  6\n"
  "6 3  7\n"
  "@attributes\n"
  "source 0\n"
  "target 5\n"
  "source1 6\n"
  "target1 3\n";


void checkDfsCompile()
{
  typedef concepts::Digraph Digraph;
  typedef Dfs<Digraph> DType;
  typedef Digraph::Node Node;
  typedef Digraph::Arc Arc;

  Digraph G;
  Node s, t;
  Arc e;
  int l, i;
  bool b;
  ::lemon::ignore_unused_variable_warning(l,i,b);

  DType::DistMap d(G);
  DType::PredMap p(G);
  Path<Digraph> pp;
  concepts::ReadMap<Arc,bool> am;

  {
    DType dfs_test(G);
    const DType& const_dfs_test = dfs_test;

    dfs_test.run(s);
    dfs_test.run(s,t);
    dfs_test.run();

    dfs_test.init();
    dfs_test.addSource(s);
    e = dfs_test.processNextArc();
    e = const_dfs_test.nextArc();
    b = const_dfs_test.emptyQueue();
    i = const_dfs_test.queueSize();

    dfs_test.start();
    dfs_test.start(t);
    dfs_test.start(am);

    l  = const_dfs_test.dist(t);
    e  = const_dfs_test.predArc(t);
    s  = const_dfs_test.predNode(t);
    b  = const_dfs_test.reached(t);
    d  = const_dfs_test.distMap();
    p  = const_dfs_test.predMap();
    pp = const_dfs_test.path(t);
  }
  {
    DType
      ::SetPredMap<concepts::ReadWriteMap<Node,Arc> >
      ::SetDistMap<concepts::ReadWriteMap<Node,int> >
      ::SetReachedMap<concepts::ReadWriteMap<Node,bool> >
      ::SetStandardProcessedMap
      ::SetProcessedMap<concepts::WriteMap<Node,bool> >
      ::Create dfs_test(G);

    concepts::ReadWriteMap<Node,Arc> pred_map;
    concepts::ReadWriteMap<Node,int> dist_map;
    concepts::ReadWriteMap<Node,bool> reached_map;
    concepts::WriteMap<Node,bool> processed_map;

    dfs_test
      .predMap(pred_map)
      .distMap(dist_map)
      .reachedMap(reached_map)
      .processedMap(processed_map);

    dfs_test.run(s);
    dfs_test.run(s,t);
    dfs_test.run();
    dfs_test.init();

    dfs_test.addSource(s);
    e = dfs_test.processNextArc();
    e = dfs_test.nextArc();
    b = dfs_test.emptyQueue();
    i = dfs_test.queueSize();

    dfs_test.start();
    dfs_test.start(t);
    dfs_test.start(am);

    l  = dfs_test.dist(t);
    e  = dfs_test.predArc(t);
    s  = dfs_test.predNode(t);
    b  = dfs_test.reached(t);
    pp = dfs_test.path(t);
  }
}

void checkDfsFunctionCompile()
{
  typedef int VType;
  typedef concepts::Digraph Digraph;
  typedef Digraph::Arc Arc;
  typedef Digraph::Node Node;

  Digraph g;
  bool b;
  ::lemon::ignore_unused_variable_warning(b);

  dfs(g).run(Node());
  b=dfs(g).run(Node(),Node());
  dfs(g).run();
  dfs(g)
    .predMap(concepts::ReadWriteMap<Node,Arc>())
    .distMap(concepts::ReadWriteMap<Node,VType>())
    .reachedMap(concepts::ReadWriteMap<Node,bool>())
    .processedMap(concepts::WriteMap<Node,bool>())
    .run(Node());
  b=dfs(g)
    .predMap(concepts::ReadWriteMap<Node,Arc>())
    .distMap(concepts::ReadWriteMap<Node,VType>())
    .reachedMap(concepts::ReadWriteMap<Node,bool>())
    .processedMap(concepts::WriteMap<Node,bool>())
    .path(concepts::Path<Digraph>())
    .dist(VType())
    .run(Node(),Node());
  dfs(g)
    .predMap(concepts::ReadWriteMap<Node,Arc>())
    .distMap(concepts::ReadWriteMap<Node,VType>())
    .reachedMap(concepts::ReadWriteMap<Node,bool>())
    .processedMap(concepts::WriteMap<Node,bool>())
    .run();
}

template <class Digraph>
void checkDfs() {
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

  Digraph G;
  Node s, t;
  Node s1, t1;

  std::istringstream input(test_lgf);
  digraphReader(G, input).
    node("source", s).
    node("target", t).
    node("source1", s1).
    node("target1", t1).
    run();

  Dfs<Digraph> dfs_test(G);
  dfs_test.run(s);

  Path<Digraph> p = dfs_test.path(t);
  check(p.length() == dfs_test.dist(t),"path() found a wrong path.");
  check(checkPath(G, p),"path() found a wrong path.");
  check(pathSource(G, p) == s,"path() found a wrong path.");
  check(pathTarget(G, p) == t,"path() found a wrong path.");

  for(NodeIt v(G); v!=INVALID; ++v) {
    if (dfs_test.reached(v)) {
      check(v==s || dfs_test.predArc(v)!=INVALID, "Wrong tree.");
      if (dfs_test.predArc(v)!=INVALID ) {
        Arc e=dfs_test.predArc(v);
        Node u=G.source(e);
        check(u==dfs_test.predNode(v),"Wrong tree.");
        check(dfs_test.dist(v) - dfs_test.dist(u) == 1,
              "Wrong distance. (" << dfs_test.dist(u) << "->"
              << dfs_test.dist(v) << ")");
      }
    }
  }

  {
  Dfs<Digraph> dfs(G);
  check(dfs.run(s1,t1) && dfs.reached(t1),"Node 3 is reachable from Node 6.");
  }

  {
    NullMap<Node,Arc> myPredMap;
    dfs(G).predMap(myPredMap).run(s);
  }
}

int main()
{
  checkDfs<ListDigraph>();
  checkDfs<SmartDigraph>();
  return 0;
}
