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
#include <lemon/dijkstra.h>
#include <lemon/path.h>
#include <lemon/bin_heap.h>

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
  "@arcs\n"
  "     label length\n"
  "0 1  0     1\n"
  "1 2  1     1\n"
  "2 3  2     1\n"
  "0 3  4     5\n"
  "0 3  5     10\n"
  "0 3  6     7\n"
  "4 2  7     1\n"
  "@attributes\n"
  "source 0\n"
  "target 3\n";

void checkDijkstraCompile()
{
  typedef int VType;
  typedef concepts::Digraph Digraph;
  typedef concepts::ReadMap<Digraph::Arc,VType> LengthMap;
  typedef Dijkstra<Digraph, LengthMap> DType;
  typedef Digraph::Node Node;
  typedef Digraph::Arc Arc;

  Digraph G;
  Node s, t, n;
  Arc e;
  VType l;
  int i;
  bool b;
  ::lemon::ignore_unused_variable_warning(l,i,b);

  DType::DistMap d(G);
  DType::PredMap p(G);
  LengthMap length;
  Path<Digraph> pp;
  concepts::ReadMap<Node,bool> nm;

  {
    DType dijkstra_test(G,length);
    const DType& const_dijkstra_test = dijkstra_test;

    dijkstra_test.run(s);
    dijkstra_test.run(s,t);

    dijkstra_test.init();
    dijkstra_test.addSource(s);
    dijkstra_test.addSource(s, 1);
    n = dijkstra_test.processNextNode();
    n = const_dijkstra_test.nextNode();
    b = const_dijkstra_test.emptyQueue();
    i = const_dijkstra_test.queueSize();

    dijkstra_test.start();
    dijkstra_test.start(t);
    dijkstra_test.start(nm);

    l  = const_dijkstra_test.dist(t);
    e  = const_dijkstra_test.predArc(t);
    s  = const_dijkstra_test.predNode(t);
    b  = const_dijkstra_test.reached(t);
    b  = const_dijkstra_test.processed(t);
    d  = const_dijkstra_test.distMap();
    p  = const_dijkstra_test.predMap();
    pp = const_dijkstra_test.path(t);
    l  = const_dijkstra_test.currentDist(t);
  }
  {
    DType
      ::SetPredMap<concepts::ReadWriteMap<Node,Arc> >
      ::SetDistMap<concepts::ReadWriteMap<Node,VType> >
      ::SetStandardProcessedMap
      ::SetProcessedMap<concepts::WriteMap<Node,bool> >
      ::SetOperationTraits<DijkstraDefaultOperationTraits<VType> >
      ::SetHeap<BinHeap<VType, concepts::ReadWriteMap<Node,int> > >
      ::SetStandardHeap<BinHeap<VType, concepts::ReadWriteMap<Node,int> > >
      ::SetHeap<BinHeap<VType, concepts::ReadWriteMap<Node,int> >,
                concepts::ReadWriteMap<Node,int> >
      ::Create dijkstra_test(G,length);

    LengthMap length_map;
    concepts::ReadWriteMap<Node,Arc> pred_map;
    concepts::ReadWriteMap<Node,VType> dist_map;
    concepts::WriteMap<Node,bool> processed_map;
    concepts::ReadWriteMap<Node,int> heap_cross_ref;
    BinHeap<VType, concepts::ReadWriteMap<Node,int> > heap(heap_cross_ref);

    dijkstra_test
      .lengthMap(length_map)
      .predMap(pred_map)
      .distMap(dist_map)
      .processedMap(processed_map)
      .heap(heap, heap_cross_ref);

    dijkstra_test.run(s);
    dijkstra_test.run(s,t);

    dijkstra_test.addSource(s);
    dijkstra_test.addSource(s, 1);
    n = dijkstra_test.processNextNode();
    n = dijkstra_test.nextNode();
    b = dijkstra_test.emptyQueue();
    i = dijkstra_test.queueSize();

    dijkstra_test.start();
    dijkstra_test.start(t);
    dijkstra_test.start(nm);

    l  = dijkstra_test.dist(t);
    e  = dijkstra_test.predArc(t);
    s  = dijkstra_test.predNode(t);
    b  = dijkstra_test.reached(t);
    b  = dijkstra_test.processed(t);
    pp = dijkstra_test.path(t);
    l  = dijkstra_test.currentDist(t);
  }

}

void checkDijkstraFunctionCompile()
{
  typedef int VType;
  typedef concepts::Digraph Digraph;
  typedef Digraph::Arc Arc;
  typedef Digraph::Node Node;
  typedef concepts::ReadMap<Digraph::Arc,VType> LengthMap;

  Digraph g;
  bool b;
  ::lemon::ignore_unused_variable_warning(b);

  dijkstra(g,LengthMap()).run(Node());
  b=dijkstra(g,LengthMap()).run(Node(),Node());
  dijkstra(g,LengthMap())
    .predMap(concepts::ReadWriteMap<Node,Arc>())
    .distMap(concepts::ReadWriteMap<Node,VType>())
    .processedMap(concepts::WriteMap<Node,bool>())
    .run(Node());
  b=dijkstra(g,LengthMap())
    .predMap(concepts::ReadWriteMap<Node,Arc>())
    .distMap(concepts::ReadWriteMap<Node,VType>())
    .processedMap(concepts::WriteMap<Node,bool>())
    .path(concepts::Path<Digraph>())
    .dist(VType())
    .run(Node(),Node());
}

template <class Digraph>
void checkDijkstra() {
  TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);
  typedef typename Digraph::template ArcMap<int> LengthMap;

  Digraph G;
  Node s, t;
  LengthMap length(G);

  std::istringstream input(test_lgf);
  digraphReader(G, input).
    arcMap("length", length).
    node("source", s).
    node("target", t).
    run();

  Dijkstra<Digraph, LengthMap>
        dijkstra_test(G, length);
  dijkstra_test.run(s);

  check(dijkstra_test.dist(t)==3,"Dijkstra found a wrong path.");

  Path<Digraph> p = dijkstra_test.path(t);
  check(p.length()==3,"path() found a wrong path.");
  check(checkPath(G, p),"path() found a wrong path.");
  check(pathSource(G, p) == s,"path() found a wrong path.");
  check(pathTarget(G, p) == t,"path() found a wrong path.");

  for(ArcIt e(G); e!=INVALID; ++e) {
    Node u=G.source(e);
    Node v=G.target(e);
    check( !dijkstra_test.reached(u) ||
           (dijkstra_test.dist(v) - dijkstra_test.dist(u) <= length[e]),
           "Wrong output. dist(target)-dist(source)-arc_length=" <<
           dijkstra_test.dist(v) - dijkstra_test.dist(u) - length[e]);
  }

  for(NodeIt v(G); v!=INVALID; ++v) {
    if (dijkstra_test.reached(v)) {
      check(v==s || dijkstra_test.predArc(v)!=INVALID, "Wrong tree.");
      if (dijkstra_test.predArc(v)!=INVALID ) {
        Arc e=dijkstra_test.predArc(v);
        Node u=G.source(e);
        check(u==dijkstra_test.predNode(v),"Wrong tree.");
        check(dijkstra_test.dist(v) - dijkstra_test.dist(u) == length[e],
              "Wrong distance! Difference: " <<
              std::abs(dijkstra_test.dist(v)-dijkstra_test.dist(u)-length[e]));
      }
    }
  }

  {
    NullMap<Node,Arc> myPredMap;
    dijkstra(G,length).predMap(myPredMap).run(s);
  }
}

int main() {
  checkDijkstra<ListDigraph>();
  checkDijkstra<SmartDigraph>();
  return 0;
}
