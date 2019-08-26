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
#include <lemon/smart_graph.h>
#include <lemon/max_cardinality_search.h>
#include <lemon/concepts/digraph.h>
#include <lemon/concepts/maps.h>
#include <lemon/concepts/heap.h>
#include <lemon/lgf_reader.h>

using namespace lemon;
using namespace std;

char test_lgf[] =
  "@nodes\n"
  "label\n"
  "0\n"
  "1\n"
  "2\n"
  "3\n"
  "@arcs\n"
  "    label capacity\n"
  "0 1 0     2\n"
  "1 0 1     2\n"
  "2 1 2     1\n"
  "2 3 3     3\n"
  "3 2 4     3\n"
  "3 1 5     5\n"
  "@attributes\n"
  "s 0\n"
  "x 1\n"
  "y 2\n"
  "z 3\n";

void checkMaxCardSearchCompile() {

  typedef concepts::Digraph Digraph;
  typedef int Value;
  typedef Digraph::Node Node;
  typedef Digraph::Arc Arc;
  typedef concepts::ReadMap<Arc,Value> CapMap;
  typedef concepts::ReadWriteMap<Node,Value> CardMap;
  typedef concepts::ReadWriteMap<Node,bool> ProcMap;
  typedef Digraph::NodeMap<int> HeapCrossRef;

  Digraph g;
  Node n,s;
  CapMap cap;
  CardMap card;
  ProcMap proc;
  HeapCrossRef crossref(g);

  typedef MaxCardinalitySearch<Digraph,CapMap>
    ::SetCapacityMap<CapMap>
    ::SetCardinalityMap<CardMap>
    ::SetProcessedMap<ProcMap>
    ::SetStandardHeap<BinHeap<Value,HeapCrossRef> >
    ::Create MaxCardType;

  MaxCardType maxcard(g,cap);
  const MaxCardType& const_maxcard = maxcard;

  const MaxCardType::Heap& heap_const = const_maxcard.heap();
  MaxCardType::Heap& heap = const_cast<MaxCardType::Heap&>(heap_const);
  maxcard.heap(heap,crossref);

  maxcard.capacityMap(cap).cardinalityMap(card).processedMap(proc);

  maxcard.init();
  maxcard.addSource(s);
  n = maxcard.nextNode();
   maxcard.processNextNode();
   maxcard.start();
   maxcard.run(s);
   maxcard.run();
 }

 void checkWithIntMap( std::istringstream& input)
 {
   typedef SmartDigraph Digraph;
   typedef Digraph::Node Node;
   typedef Digraph::ArcMap<int> CapMap;

   Digraph g;
   Node s,x,y,z,a;
   CapMap cap(g);

   DigraphReader<Digraph>(g,input).
     arcMap("capacity", cap).
     node("s",s).
     node("x",x).
     node("y",y).
     node("z",z).
     run();

   MaxCardinalitySearch<Digraph,CapMap> maxcard(g,cap);

   maxcard.init();
   maxcard.addSource(s);
   maxcard.start(x);

   check(maxcard.processed(s) && !maxcard.processed(x) &&
         !maxcard.processed(y), "Wrong processed()!");

   a=maxcard.nextNode();
   check(maxcard.processNextNode()==a,
         "Wrong nextNode() or processNextNode() return value!");

   check(maxcard.processed(a), "Wrong processNextNode()!");

   maxcard.start();
   check(maxcard.cardinality(x)==2 && maxcard.cardinality(y)>=4,
         "Wrong cardinalities!");
 }

 void checkWithConst1Map(std::istringstream &input) {
   typedef SmartDigraph Digraph;
   typedef Digraph::Node Node;

   Digraph g;
   Node s,x,y,z;

  DigraphReader<Digraph>(g,input).
    node("s",s).
    node("x",x).
    node("y",y).
    node("z",z).
    run();

  MaxCardinalitySearch<Digraph> maxcard(g);
  maxcard.run(s);
  check(maxcard.cardinality(x)==1 &&
        maxcard.cardinality(y)+maxcard.cardinality(z)==3,
        "Wrong cardinalities!");
}

int main() {

  std::istringstream input1(test_lgf);
  checkWithIntMap(input1);

  std::istringstream input2(test_lgf);
  checkWithConst1Map(input2);
}
