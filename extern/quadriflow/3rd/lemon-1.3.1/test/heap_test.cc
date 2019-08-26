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
#include <fstream>
#include <string>
#include <vector>

#include <lemon/concept_check.h>
#include <lemon/concepts/heap.h>

#include <lemon/smart_graph.h>
#include <lemon/lgf_reader.h>
#include <lemon/dijkstra.h>
#include <lemon/maps.h>

#include <lemon/bin_heap.h>
#include <lemon/quad_heap.h>
#include <lemon/dheap.h>
#include <lemon/fib_heap.h>
#include <lemon/pairing_heap.h>
#include <lemon/radix_heap.h>
#include <lemon/binomial_heap.h>
#include <lemon/bucket_heap.h>

#include "test_tools.h"

using namespace lemon;
using namespace lemon::concepts;

typedef ListDigraph Digraph;
DIGRAPH_TYPEDEFS(Digraph);

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
  "7\n"
  "8\n"
  "9\n"
  "@arcs\n"
  "                label   capacity\n"
  "0       5       0       94\n"
  "3       9       1       11\n"
  "8       7       2       83\n"
  "1       2       3       94\n"
  "5       7       4       35\n"
  "7       4       5       84\n"
  "9       5       6       38\n"
  "0       4       7       96\n"
  "6       7       8       6\n"
  "3       1       9       27\n"
  "5       2       10      77\n"
  "5       6       11      69\n"
  "6       5       12      41\n"
  "4       6       13      70\n"
  "3       2       14      45\n"
  "7       9       15      93\n"
  "5       9       16      50\n"
  "9       0       17      94\n"
  "9       6       18      67\n"
  "0       9       19      86\n"
  "@attributes\n"
  "source 3\n";

int test_seq[] = { 2, 28, 19, 27, 33, 25, 13, 41, 10, 26,  1,  9,  4, 34};
int test_inc[] = {20, 28, 34, 16,  0, 46, 44,  0, 42, 32, 14,  8,  6, 37};

int test_len = sizeof(test_seq) / sizeof(test_seq[0]);

template <typename Heap>
void heapSortTest() {
  RangeMap<int> map(test_len, -1);
  Heap heap(map);

  std::vector<int> v(test_len);
  for (int i = 0; i < test_len; ++i) {
    v[i] = test_seq[i];
    heap.push(i, v[i]);
  }
  std::sort(v.begin(), v.end());
  for (int i = 0; i < test_len; ++i) {
    check(v[i] == heap.prio(), "Wrong order in heap sort.");
    heap.pop();
  }
}

template <typename Heap>
void heapIncreaseTest() {
  RangeMap<int> map(test_len, -1);

  Heap heap(map);

  std::vector<int> v(test_len);
  for (int i = 0; i < test_len; ++i) {
    v[i] = test_seq[i];
    heap.push(i, v[i]);
  }
  for (int i = 0; i < test_len; ++i) {
    v[i] += test_inc[i];
    heap.increase(i, v[i]);
  }
  std::sort(v.begin(), v.end());
  for (int i = 0; i < test_len; ++i) {
    check(v[i] == heap.prio(), "Wrong order in heap increase test.");
    heap.pop();
  }
}

template <typename Heap>
void dijkstraHeapTest(const Digraph& digraph, const IntArcMap& length,
                      Node source) {

  typename Dijkstra<Digraph, IntArcMap>::template SetStandardHeap<Heap>::
    Create dijkstra(digraph, length);

  dijkstra.run(source);

  for(ArcIt a(digraph); a != INVALID; ++a) {
    Node s = digraph.source(a);
    Node t = digraph.target(a);
    if (dijkstra.reached(s)) {
      check( dijkstra.dist(t) - dijkstra.dist(s) <= length[a],
             "Error in shortest path tree.");
    }
  }

  for(NodeIt n(digraph); n != INVALID; ++n) {
    if ( dijkstra.reached(n) && dijkstra.predArc(n) != INVALID ) {
      Arc a = dijkstra.predArc(n);
      Node s = digraph.source(a);
      check( dijkstra.dist(n) - dijkstra.dist(s) == length[a],
             "Error in shortest path tree.");
    }
  }

}

int main() {

  typedef int Item;
  typedef int Prio;
  typedef RangeMap<int> ItemIntMap;

  Digraph digraph;
  IntArcMap length(digraph);
  Node source;

  std::istringstream input(test_lgf);
  digraphReader(digraph, input).
    arcMap("capacity", length).
    node("source", source).
    run();

  // BinHeap
  {
    typedef BinHeap<Prio, ItemIntMap> IntHeap;
    checkConcept<Heap<Prio, ItemIntMap>, IntHeap>();
    heapSortTest<IntHeap>();
    heapIncreaseTest<IntHeap>();

    typedef BinHeap<Prio, IntNodeMap > NodeHeap;
    checkConcept<Heap<Prio, IntNodeMap >, NodeHeap>();
    dijkstraHeapTest<NodeHeap>(digraph, length, source);
  }

  // QuadHeap
  {
    typedef QuadHeap<Prio, ItemIntMap> IntHeap;
    checkConcept<Heap<Prio, ItemIntMap>, IntHeap>();
    heapSortTest<IntHeap>();
    heapIncreaseTest<IntHeap>();

    typedef QuadHeap<Prio, IntNodeMap > NodeHeap;
    checkConcept<Heap<Prio, IntNodeMap >, NodeHeap>();
    dijkstraHeapTest<NodeHeap>(digraph, length, source);
  }

  // DHeap
  {
    typedef DHeap<Prio, ItemIntMap> IntHeap;
    checkConcept<Heap<Prio, ItemIntMap>, IntHeap>();
    heapSortTest<IntHeap>();
    heapIncreaseTest<IntHeap>();

    typedef DHeap<Prio, IntNodeMap > NodeHeap;
    checkConcept<Heap<Prio, IntNodeMap >, NodeHeap>();
    dijkstraHeapTest<NodeHeap>(digraph, length, source);
  }

  // FibHeap
  {
    typedef FibHeap<Prio, ItemIntMap> IntHeap;
    checkConcept<Heap<Prio, ItemIntMap>, IntHeap>();
    heapSortTest<IntHeap>();
    heapIncreaseTest<IntHeap>();

    typedef FibHeap<Prio, IntNodeMap > NodeHeap;
    checkConcept<Heap<Prio, IntNodeMap >, NodeHeap>();
    dijkstraHeapTest<NodeHeap>(digraph, length, source);
  }

  // PairingHeap
  {
    typedef PairingHeap<Prio, ItemIntMap> IntHeap;
    checkConcept<Heap<Prio, ItemIntMap>, IntHeap>();
    heapSortTest<IntHeap>();
    heapIncreaseTest<IntHeap>();

    typedef PairingHeap<Prio, IntNodeMap > NodeHeap;
    checkConcept<Heap<Prio, IntNodeMap >, NodeHeap>();
    dijkstraHeapTest<NodeHeap>(digraph, length, source);
  }

  // RadixHeap
  {
    typedef RadixHeap<ItemIntMap> IntHeap;
    checkConcept<Heap<Prio, ItemIntMap>, IntHeap>();
    heapSortTest<IntHeap>();
    heapIncreaseTest<IntHeap>();

    typedef RadixHeap<IntNodeMap > NodeHeap;
    checkConcept<Heap<Prio, IntNodeMap >, NodeHeap>();
    dijkstraHeapTest<NodeHeap>(digraph, length, source);
  }

  // BinomialHeap
  {
    typedef BinomialHeap<Prio, ItemIntMap> IntHeap;
    checkConcept<Heap<Prio, ItemIntMap>, IntHeap>();
    heapSortTest<IntHeap>();
    heapIncreaseTest<IntHeap>();

    typedef BinomialHeap<Prio, IntNodeMap > NodeHeap;
    checkConcept<Heap<Prio, IntNodeMap >, NodeHeap>();
    dijkstraHeapTest<NodeHeap>(digraph, length, source);
  }

  // BucketHeap, SimpleBucketHeap
  {
    typedef BucketHeap<ItemIntMap> IntHeap;
    checkConcept<Heap<Prio, ItemIntMap>, IntHeap>();
    heapSortTest<IntHeap>();
    heapIncreaseTest<IntHeap>();

    typedef BucketHeap<IntNodeMap > NodeHeap;
    checkConcept<Heap<Prio, IntNodeMap >, NodeHeap>();
    dijkstraHeapTest<NodeHeap>(digraph, length, source);

    typedef SimpleBucketHeap<ItemIntMap> SimpleIntHeap;
    heapSortTest<SimpleIntHeap>();
  }

  {
    typedef FibHeap<Prio, ItemIntMap> IntHeap;
    checkConcept<Heap<Prio, ItemIntMap>, IntHeap>();
    heapSortTest<IntHeap>();
    heapIncreaseTest<IntHeap>();

    typedef FibHeap<Prio, IntNodeMap > NodeHeap;
    checkConcept<Heap<Prio, IntNodeMap >, NodeHeap>();
    dijkstraHeapTest<NodeHeap>(digraph, length, source);
  }

  {
    typedef RadixHeap<ItemIntMap> IntHeap;
    checkConcept<Heap<Prio, ItemIntMap>, IntHeap>();
    heapSortTest<IntHeap>();
    heapIncreaseTest<IntHeap>();

    typedef RadixHeap<IntNodeMap > NodeHeap;
    checkConcept<Heap<Prio, IntNodeMap >, NodeHeap>();
    dijkstraHeapTest<NodeHeap>(digraph, length, source);
  }

  {
    typedef BucketHeap<ItemIntMap> IntHeap;
    checkConcept<Heap<Prio, ItemIntMap>, IntHeap>();
    heapSortTest<IntHeap>();
    heapIncreaseTest<IntHeap>();

    typedef BucketHeap<IntNodeMap > NodeHeap;
    checkConcept<Heap<Prio, IntNodeMap >, NodeHeap>();
    dijkstraHeapTest<NodeHeap>(digraph, length, source);
  }


  return 0;
}
