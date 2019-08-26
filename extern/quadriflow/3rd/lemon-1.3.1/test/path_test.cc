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

#include <string>
#include <iostream>

#include <lemon/concepts/path.h>
#include <lemon/concepts/digraph.h>
#include <lemon/concept_check.h>

#include <lemon/path.h>
#include <lemon/list_graph.h>

#include "test_tools.h"

using namespace std;
using namespace lemon;

template <typename GR>
void checkConcepts() {
  checkConcept<concepts::Path<GR>, concepts::Path<GR> >();
  checkConcept<concepts::Path<GR>, Path<GR> >();
  checkConcept<concepts::Path<GR>, SimplePath<GR> >();
  checkConcept<concepts::Path<GR>, StaticPath<GR> >();
  checkConcept<concepts::Path<GR>, ListPath<GR> >();
}

// Conecpt checking for path structures
void checkPathConcepts() {
  checkConcepts<concepts::Digraph>();
  checkConcepts<ListDigraph>();
}

// Check if proper copy consructor is called (use valgrind for testing)
template <typename GR, typename P1, typename P2>
void checkCopy(typename GR::Arc a) {
  P1 p;
  p.addBack(a);
  P1 q;
  q = p;
  P1 r(p);
  P2 q2;
  q2 = p;
  P2 r2(p);
}

// Tests for copy constructors and assignment operators of paths
void checkPathCopy() {
  ListDigraph g;
  ListDigraph::Arc a = g.addArc(g.addNode(), g.addNode());

  typedef Path<ListDigraph> Path1;
  typedef SimplePath<ListDigraph> Path2;
  typedef ListPath<ListDigraph> Path3;
  typedef StaticPath<ListDigraph> Path4;
  checkCopy<ListDigraph, Path1, Path2>(a);
  checkCopy<ListDigraph, Path1, Path3>(a);
  checkCopy<ListDigraph, Path1, Path4>(a);
  checkCopy<ListDigraph, Path2, Path1>(a);
  checkCopy<ListDigraph, Path2, Path3>(a);
  checkCopy<ListDigraph, Path2, Path4>(a);
  checkCopy<ListDigraph, Path3, Path1>(a);
  checkCopy<ListDigraph, Path3, Path2>(a);
  checkCopy<ListDigraph, Path3, Path4>(a);
}

// Class for testing path functions
class CheckPathFunctions {
  typedef ListDigraph GR;
  DIGRAPH_TYPEDEFS(GR);
  GR gr;
  const GR& cgr;
  Node n1, n2, n3, n4;
  Node tmp_n;
  Arc a1, a2, a3, a4;
  Arc tmp_a;

public:

  CheckPathFunctions() : cgr(gr) {
    n1 = gr.addNode();
    n2 = gr.addNode();
    n3 = gr.addNode();
    n4 = gr.addNode();
    a1 = gr.addArc(n1, n2);
    a2 = gr.addArc(n2, n3);
    a3 = gr.addArc(n3, n4);
    a4 = gr.addArc(n4, n1);
  }

  void run() {
    checkBackAndFrontInsertablePath<Path<GR> >();
    checkBackAndFrontInsertablePath<ListPath<GR> >();
    checkBackInsertablePath<SimplePath<GR> >();

    checkListPathSplitAndSplice();
  }

private:

  template <typename P>
  void checkBackInsertablePath() {

    // Create and check empty path
    P p;
    const P& cp = p;
    check(cp.empty(), "The path is not empty");
    check(cp.length() == 0, "The path is not empty");
//    check(cp.front() == INVALID, "Wrong front()");
//    check(cp.back() == INVALID, "Wrong back()");
    typename P::ArcIt ai(cp);
    check(ai == INVALID, "Wrong ArcIt");
    check(pathSource(cgr, cp) == INVALID, "Wrong pathSource()");
    check(pathTarget(cgr, cp) == INVALID, "Wrong pathTarget()");
    check(checkPath(cgr, cp), "Wrong checkPath()");
    PathNodeIt<P> ni(cgr, cp);
    check(ni == INVALID, "Wrong PathNodeIt");

    // Check single-arc path
    p.addBack(a1);
    check(!cp.empty(), "Wrong empty()");
    check(cp.length() == 1, "Wrong length");
    check(cp.front() == a1, "Wrong front()");
    check(cp.back() == a1, "Wrong back()");
    check(cp.nth(0) == a1, "Wrong nth()");
    ai = cp.nthIt(0);
    check((tmp_a = ai) == a1, "Wrong nthIt()");
    check(++ai == INVALID, "Wrong nthIt()");
    typename P::ArcIt ai2(cp);
    check((tmp_a = ai2) == a1, "Wrong ArcIt");
    check(++ai2 == INVALID, "Wrong ArcIt");
    check(pathSource(cgr, cp) == n1, "Wrong pathSource()");
    check(pathTarget(cgr, cp) == n2, "Wrong pathTarget()");
    check(checkPath(cgr, cp), "Wrong checkPath()");
    PathNodeIt<P> ni2(cgr, cp);
    check((tmp_n = ni2) == n1, "Wrong PathNodeIt");
    check((tmp_n = ++ni2) == n2, "Wrong PathNodeIt");
    check(++ni2 == INVALID, "Wrong PathNodeIt");

    // Check adding more arcs
    p.addBack(a2);
    p.addBack(a3);
    check(!cp.empty(), "Wrong empty()");
    check(cp.length() == 3, "Wrong length");
    check(cp.front() == a1, "Wrong front()");
    check(cp.back() == a3, "Wrong back()");
    check(cp.nth(0) == a1, "Wrong nth()");
    check(cp.nth(1) == a2, "Wrong nth()");
    check(cp.nth(2) == a3, "Wrong nth()");
    typename P::ArcIt ai3(cp);
    check((tmp_a = ai3) == a1, "Wrong ArcIt");
    check((tmp_a = ++ai3) == a2, "Wrong nthIt()");
    check((tmp_a = ++ai3) == a3, "Wrong nthIt()");
    check(++ai3 == INVALID, "Wrong nthIt()");
    ai = cp.nthIt(0);
    check((tmp_a = ai) == a1, "Wrong nthIt()");
    check((tmp_a = ++ai) == a2, "Wrong nthIt()");
    ai = cp.nthIt(2);
    check((tmp_a = ai) == a3, "Wrong nthIt()");
    check(++ai == INVALID, "Wrong nthIt()");
    check(pathSource(cgr, cp) == n1, "Wrong pathSource()");
    check(pathTarget(cgr, cp) == n4, "Wrong pathTarget()");
    check(checkPath(cgr, cp), "Wrong checkPath()");
    PathNodeIt<P> ni3(cgr, cp);
    check((tmp_n = ni3) == n1, "Wrong PathNodeIt");
    check((tmp_n = ++ni3) == n2, "Wrong PathNodeIt");
    check((tmp_n = ++ni3) == n3, "Wrong PathNodeIt");
    check((tmp_n = ++ni3) == n4, "Wrong PathNodeIt");
    check(++ni3 == INVALID, "Wrong PathNodeIt");

    // Check arc removal and addition
    p.eraseBack();
    p.eraseBack();
    p.addBack(a2);
    check(!cp.empty(), "Wrong empty()");
    check(cp.length() == 2, "Wrong length");
    check(cp.front() == a1, "Wrong front()");
    check(cp.back() == a2, "Wrong back()");
    check(pathSource(cgr, cp) == n1, "Wrong pathSource()");
    check(pathTarget(cgr, cp) == n3, "Wrong pathTarget()");
    check(checkPath(cgr, cp), "Wrong checkPath()");

    // Check clear()
    p.clear();
    check(cp.empty(), "The path is not empty");
    check(cp.length() == 0, "The path is not empty");

    // Check inconsistent path
    p.addBack(a4);
    p.addBack(a2);
    p.addBack(a1);
    check(!cp.empty(), "Wrong empty()");
    check(cp.length() == 3, "Wrong length");
    check(cp.front() == a4, "Wrong front()");
    check(cp.back() == a1, "Wrong back()");
    check(pathSource(cgr, cp) == n4, "Wrong pathSource()");
    check(pathTarget(cgr, cp) == n2, "Wrong pathTarget()");
    check(!checkPath(cgr, cp), "Wrong checkPath()");
  }

  template <typename P>
  void checkBackAndFrontInsertablePath() {

    // Include back insertable test cases
    checkBackInsertablePath<P>();

    // Check front and back insertion
    P p;
    const P& cp = p;
    p.addFront(a4);
    p.addBack(a1);
    p.addFront(a3);
    check(!cp.empty(), "Wrong empty()");
    check(cp.length() == 3, "Wrong length");
    check(cp.front() == a3, "Wrong front()");
    check(cp.back() == a1, "Wrong back()");
    check(cp.nth(0) == a3, "Wrong nth()");
    check(cp.nth(1) == a4, "Wrong nth()");
    check(cp.nth(2) == a1, "Wrong nth()");
    typename P::ArcIt ai(cp);
    check((tmp_a = ai) == a3, "Wrong ArcIt");
    check((tmp_a = ++ai) == a4, "Wrong nthIt()");
    check((tmp_a = ++ai) == a1, "Wrong nthIt()");
    check(++ai == INVALID, "Wrong nthIt()");
    ai = cp.nthIt(0);
    check((tmp_a = ai) == a3, "Wrong nthIt()");
    check((tmp_a = ++ai) == a4, "Wrong nthIt()");
    ai = cp.nthIt(2);
    check((tmp_a = ai) == a1, "Wrong nthIt()");
    check(++ai == INVALID, "Wrong nthIt()");
    check(pathSource(cgr, cp) == n3, "Wrong pathSource()");
    check(pathTarget(cgr, cp) == n2, "Wrong pathTarget()");
    check(checkPath(cgr, cp), "Wrong checkPath()");

    // Check eraseFront()
    p.eraseFront();
    p.addBack(a2);
    check(!cp.empty(), "Wrong empty()");
    check(cp.length() == 3, "Wrong length");
    check(cp.front() == a4, "Wrong front()");
    check(cp.back() == a2, "Wrong back()");
    check(cp.nth(0) == a4, "Wrong nth()");
    check(cp.nth(1) == a1, "Wrong nth()");
    check(cp.nth(2) == a2, "Wrong nth()");
    typename P::ArcIt ai2(cp);
    check((tmp_a = ai2) == a4, "Wrong ArcIt");
    check((tmp_a = ++ai2) == a1, "Wrong nthIt()");
    check((tmp_a = ++ai2) == a2, "Wrong nthIt()");
    check(++ai2 == INVALID, "Wrong nthIt()");
    ai = cp.nthIt(0);
    check((tmp_a = ai) == a4, "Wrong nthIt()");
    check((tmp_a = ++ai) == a1, "Wrong nthIt()");
    ai = cp.nthIt(2);
    check((tmp_a = ai) == a2, "Wrong nthIt()");
    check(++ai == INVALID, "Wrong nthIt()");
    check(pathSource(cgr, cp) == n4, "Wrong pathSource()");
    check(pathTarget(cgr, cp) == n3, "Wrong pathTarget()");
    check(checkPath(cgr, cp), "Wrong checkPath()");
  }

  void checkListPathSplitAndSplice() {

    // Build a path with spliceFront() and spliceBack()
    ListPath<GR> p1, p2, p3, p4;
    p1.addBack(a3);
    p1.addFront(a2);
    p2.addBack(a1);
    p1.spliceFront(p2);
    p3.addFront(a4);
    p1.spliceBack(p3);
    check(p1.length() == 4, "Wrong length");
    check(p1.front() == a1, "Wrong front()");
    check(p1.back() == a4, "Wrong back()");
    ListPath<GR>::ArcIt ai(p1);
    check((tmp_a = ai) == a1, "Wrong ArcIt");
    check((tmp_a = ++ai) == a2, "Wrong nthIt()");
    check((tmp_a = ++ai) == a3, "Wrong nthIt()");
    check((tmp_a = ++ai) == a4, "Wrong nthIt()");
    check(++ai == INVALID, "Wrong nthIt()");
    check(checkPath(cgr, p1), "Wrong checkPath()");

    // Check split()
    p1.split(p1.nthIt(2), p2);
    check(p1.length() == 2, "Wrong length");
    ai = p1.nthIt(0);
    check((tmp_a = ai) == a1, "Wrong ArcIt");
    check((tmp_a = ++ai) == a2, "Wrong nthIt()");
    check(++ai == INVALID, "Wrong nthIt()");
    check(checkPath(cgr, p1), "Wrong checkPath()");
    check(p2.length() == 2, "Wrong length");
    ai = p2.nthIt(0);
    check((tmp_a = ai) == a3, "Wrong ArcIt");
    check((tmp_a = ++ai) == a4, "Wrong nthIt()");
    check(++ai == INVALID, "Wrong nthIt()");
    check(checkPath(cgr, p2), "Wrong checkPath()");

    // Check split() and splice()
    p1.spliceFront(p2);
    p1.split(p1.nthIt(2), p2);
    p2.split(p2.nthIt(1), p3);
    p2.spliceBack(p1);
    p2.splice(p2.nthIt(1), p3);
    check(p2.length() == 4, "Wrong length");
    check(p2.front() == a1, "Wrong front()");
    check(p2.back() == a4, "Wrong back()");
    ai = p2.nthIt(0);
    check((tmp_a = ai) == a1, "Wrong ArcIt");
    check((tmp_a = ++ai) == a2, "Wrong nthIt()");
    check((tmp_a = ++ai) == a3, "Wrong nthIt()");
    check((tmp_a = ++ai) == a4, "Wrong nthIt()");
    check(++ai == INVALID, "Wrong nthIt()");
    check(checkPath(cgr, p2), "Wrong checkPath()");
  }

};

int main() {
  checkPathConcepts();
  checkPathCopy();
  CheckPathFunctions cpf;
  cpf.run();

  return 0;
}
