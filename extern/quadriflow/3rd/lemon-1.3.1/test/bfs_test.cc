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
#include <lemon/bfs.h>
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
  "@arcs\n"
  "     label\n"
  "0 1  0\n"
  "1 2  1\n"
  "2 3  2\n"
  "3 4  3\n"
  "0 3  4\n"
  "0 3  5\n"
  "5 2  6\n"
  "@attributes\n"
  "source 0\n"
  "target 4\n";

void checkBfsCompile()
{
  typedef concepts::Digraph Digraph;
  typedef Bfs<Digraph> BType;
  typedef Digraph::Node Node;
  typedef Digraph::Arc Arc;

  Digraph G;
  Node s, t, n;
  Arc e;
  int l, i;
  ::lemon::ignore_unused_variable_warning(l,i);
  bool b;
  BType::DistMap d(G);
  BType::PredMap p(G);
  Path<Digraph> pp;
  concepts::ReadMap<Node,bool> nm;

  {
    BType bfs_test(G);
    const BType& const_bfs_test = bfs_test;

    bfs_test.run(s);
    bfs_test.run(s,t);
    bfs_test.run();

    bfs_test.init();
    bfs_test.addSource(s);
    n = bfs_test.processNextNode();
    n = bfs_test.processNextNode(t, b);
    n = bfs_test.processNextNode(nm, n);
    n = const_bfs_test.nextNode();
    b = const_bfs_test.emptyQueue();
    i = const_bfs_test.queueSize();

    bfs_test.start();
    bfs_test.start(t);
    bfs_test.start(nm);

    l  = const_bfs_test.dist(t);
    e  = const_bfs_test.predArc(t);
    s  = const_bfs_test.predNode(t);
    b  = const_bfs_test.reached(t);
    d  = const_bfs_test.distMap();
    p  = const_bfs_test.predMap();
    pp = const_bfs_test.path(t);
  }
  {
    BType
      ::SetPredMap<concepts::ReadWriteMap<Node,Arc> >
      ::SetDistMap<concepts::ReadWriteMap<Node,int> >
      ::SetReachedMap<concepts::ReadWriteMap<Node,bool> >
      ::SetStandardProcessedMap
      ::SetProcessedMap<concepts::WriteMap<Node,bool> >
      ::Create bfs_test(G);

    concepts::ReadWriteMap<Node,Arc> pred_map;
    concepts::ReadWriteMap<Node,int> dist_map;
    concepts::ReadWriteMap<Node,bool> reached_map;
    concepts::WriteMap<Node,bool> processed_map;

    bfs_test
      .predMap(pred_map)
      .distMap(dist_map)
      .reachedMap(reached_map)
      .processedMap(processed_map);

    bfs_test.run(s);
    bfs_test.run(s,t);
    bfs_test.run();

    bfs_test.init();
    bfs_test.addSource(s);
    n = bfs_test.processNextNode();
    n = bfs_test.processNextNode(t, b);
    n = bfs_test.processNextNode(nm, n);
    n = bfs_test.nextNode();
    b = bfs_test.emptyQueue();
    i = bfs_test.queueSize();

    bfs_test.start();
    bfs_test.start(t);
    bfs_test.start(nm);

    l  = bfs_test.dist(t);
    e  = bfs_test.predArc(t);
    s  = bfs_test.predNode(t);
    b  = bfs_test.reached(t);
    pp = bfs_test.path(t);
  }
}

void checkBfsFunctionCompile()
{
  typedef int VType;
  typedef concepts::Digraph Digraph;
  typedef Digraph::Arc Arc;
  typedef Digraph::Node Node;

  Digraph g;
  bool b;
  ::lemon::ignore_unused_variable_warning(b);

  bfs(g).run(Node());
  b=bfs(g).run(Node(),Node());
  bfs(g).run();
  bfs(g)
    .predMap(concepts::ReadWriteMap<Node,Arc>())
    .distMap(concepts::ReadWriteMap<Node,VType>())
    .reachedMap(concepts::ReadWriteMap<Node,bool>())
    .processedMap(concepts::WriteMap<Node,bool>())
    .run(Node());
  b=bfs(g)
    .predMap(concepts::ReadWriteMap<Node,Arc>())
    .distMap(concepts::ReadWriteMap<Node,VType>())
    .reachedMap(concepts::ReadWriteMap<Node,bool>())
    .processedMap(concepts::WriteMap<Node,bool>())
    .path(concepts::Path<Digraph>())
    .dist(VType())
    .run(Node(),Node());
  bfs(g)
    .predMap(concepts::ReadWriteMap<Node,Arc>())
    .distMap(concepts::ReadWriteMap<Node,VType>())
    .reachedMap(concepts::ReadWriteMap<Node,bool>())
    .processedMap(concepts::WriteMap<Node,bool>())
    .run();
}

template <class Digraph>
void checkBfs() {
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

  Digraph G;
  Node s, t;

  std::istringstream input(test_lgf);
  digraphReader(G, input).
    node("source", s).
    node("target", t).
    run();

  Bfs<Digraph> bfs_test(G);
  bfs_test.run(s);

  check(bfs_test.dist(t)==2,"Bfs found a wrong path.");

  Path<Digraph> p = bfs_test.path(t);
  check(p.length()==2,"path() found a wrong path.");
  check(checkPath(G, p),"path() found a wrong path.");
  check(pathSource(G, p) == s,"path() found a wrong path.");
  check(pathTarget(G, p) == t,"path() found a wrong path.");


  for(ArcIt a(G); a!=INVALID; ++a) {
    Node u=G.source(a);
    Node v=G.target(a);
    check( !bfs_test.reached(u) ||
           (bfs_test.dist(v) <= bfs_test.dist(u)+1),
           "Wrong output. " << G.id(u) << "->" << G.id(v));
  }

  for(NodeIt v(G); v!=INVALID; ++v) {
    if (bfs_test.reached(v)) {
      check(v==s || bfs_test.predArc(v)!=INVALID, "Wrong tree.");
      if (bfs_test.predArc(v)!=INVALID ) {
        Arc a=bfs_test.predArc(v);
        Node u=G.source(a);
        check(u==bfs_test.predNode(v),"Wrong tree.");
        check(bfs_test.dist(v) - bfs_test.dist(u) == 1,
              "Wrong distance. Difference: "
              << std::abs(bfs_test.dist(v) - bfs_test.dist(u) - 1));
      }
    }
  }

  {
    NullMap<Node,Arc> myPredMap;
    bfs(G).predMap(myPredMap).run(s);
  }
}

int main()
{
  checkBfs<ListDigraph>();
  checkBfs<SmartDigraph>();
  return 0;
}
