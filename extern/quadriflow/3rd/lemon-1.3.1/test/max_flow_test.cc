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
#include <lemon/preflow.h>
#include <lemon/edmonds_karp.h>
#include <lemon/concepts/digraph.h>
#include <lemon/concepts/maps.h>
#include <lemon/lgf_reader.h>
#include <lemon/elevator.h>

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
  "7\n"
  "8\n"
  "9\n"
  "@arcs\n"
  "    label capacity\n"
  "0 1 0     20\n"
  "0 2 1     0\n"
  "1 1 2     3\n"
  "1 2 3     8\n"
  "1 3 4     8\n"
  "2 5 5     5\n"
  "3 2 6     5\n"
  "3 5 7     5\n"
  "3 6 8     5\n"
  "4 3 9     3\n"
  "5 7 10    3\n"
  "5 6 11    10\n"
  "5 8 12    10\n"
  "6 8 13    8\n"
  "8 9 14    20\n"
  "8 1 15    5\n"
  "9 5 16    5\n"
  "@attributes\n"
  "source 1\n"
  "target 8\n";


// Checks the general interface of a max flow algorithm
template <typename GR, typename CAP>
struct MaxFlowClassConcept
{

  template <typename MF>
  struct Constraints {

    typedef typename GR::Node Node;
    typedef typename GR::Arc Arc;
    typedef typename CAP::Value Value;
    typedef concepts::ReadWriteMap<Arc, Value> FlowMap;
    typedef concepts::WriteMap<Node, bool> CutMap;

    GR g;
    Node n;
    Arc e;
    CAP cap;
    FlowMap flow;
    CutMap cut;
    Value v;
    bool b;

    void constraints() {
      checkConcept<concepts::Digraph, GR>();

      const Constraints& me = *this;

      typedef typename MF
          ::template SetFlowMap<FlowMap>
          ::Create MaxFlowType;
      typedef typename MF::Create MaxFlowType2;
      MaxFlowType max_flow(me.g, me.cap, me.n, me.n);
      const MaxFlowType& const_max_flow = max_flow;

      max_flow
          .capacityMap(cap)
          .flowMap(flow)
          .source(n)
          .target(n);

      typename MaxFlowType::Tolerance tol = const_max_flow.tolerance();
      max_flow.tolerance(tol);

      max_flow.init();
      max_flow.init(cap);
      max_flow.run();

      v = const_max_flow.flowValue();
      v = const_max_flow.flow(e);
      const FlowMap& fm = const_max_flow.flowMap();

      b = const_max_flow.minCut(n);
      const_max_flow.minCutMap(cut);

      ::lemon::ignore_unused_variable_warning(fm);
    }

  };

};

// Checks the specific parts of Preflow's interface
void checkPreflowCompile()
{
  typedef int Value;
  typedef concepts::Digraph Digraph;
  typedef concepts::ReadMap<Digraph::Arc, Value> CapMap;
  typedef Elevator<Digraph, Digraph::Node> Elev;
  typedef LinkedElevator<Digraph, Digraph::Node> LinkedElev;

  Digraph g;
  Digraph::Node n;
  CapMap cap;

  typedef Preflow<Digraph, CapMap>
      ::SetElevator<Elev>
      ::SetStandardElevator<LinkedElev>
      ::Create PreflowType;
  PreflowType preflow_test(g, cap, n, n);
  const PreflowType& const_preflow_test = preflow_test;

  const PreflowType::Elevator& elev = const_preflow_test.elevator();
  preflow_test.elevator(const_cast<PreflowType::Elevator&>(elev));

  bool b = preflow_test.init(cap);
  preflow_test.startFirstPhase();
  preflow_test.startSecondPhase();
  preflow_test.runMinCut();

  ::lemon::ignore_unused_variable_warning(b);
}

// Checks the specific parts of EdmondsKarp's interface
void checkEdmondsKarpCompile()
{
  typedef int Value;
  typedef concepts::Digraph Digraph;
  typedef concepts::ReadMap<Digraph::Arc, Value> CapMap;
  typedef Elevator<Digraph, Digraph::Node> Elev;
  typedef LinkedElevator<Digraph, Digraph::Node> LinkedElev;

  Digraph g;
  Digraph::Node n;
  CapMap cap;

  EdmondsKarp<Digraph, CapMap> ek_test(g, cap, n, n);

  ek_test.init(cap);
  bool b = ek_test.checkedInit(cap);
  b = ek_test.augment();
  ek_test.start();

  ::lemon::ignore_unused_variable_warning(b);
}


template <typename T>
T cutValue (const SmartDigraph& g,
              const SmartDigraph::NodeMap<bool>& cut,
              const SmartDigraph::ArcMap<T>& cap) {

  T c=0;
  for(SmartDigraph::ArcIt e(g); e!=INVALID; ++e) {
    if (cut[g.source(e)] && !cut[g.target(e)]) c+=cap[e];
  }
  return c;
}

template <typename T>
bool checkFlow(const SmartDigraph& g,
               const SmartDigraph::ArcMap<T>& flow,
               const SmartDigraph::ArcMap<T>& cap,
               SmartDigraph::Node s, SmartDigraph::Node t) {

  for (SmartDigraph::ArcIt e(g); e != INVALID; ++e) {
    if (flow[e] < 0 || flow[e] > cap[e]) return false;
  }

  for (SmartDigraph::NodeIt n(g); n != INVALID; ++n) {
    if (n == s || n == t) continue;
    T sum = 0;
    for (SmartDigraph::OutArcIt e(g, n); e != INVALID; ++e) {
      sum += flow[e];
    }
    for (SmartDigraph::InArcIt e(g, n); e != INVALID; ++e) {
      sum -= flow[e];
    }
    if (sum != 0) return false;
  }
  return true;
}

void initFlowTest()
{
  DIGRAPH_TYPEDEFS(SmartDigraph);

  SmartDigraph g;
  SmartDigraph::ArcMap<int> cap(g),iflow(g);
  Node s=g.addNode(); Node t=g.addNode();
  Node n1=g.addNode(); Node n2=g.addNode();
  Arc a;
  a=g.addArc(s,n1); cap[a]=20; iflow[a]=20;
  a=g.addArc(n1,n2); cap[a]=10; iflow[a]=0;
  a=g.addArc(n2,t); cap[a]=20; iflow[a]=0;

  Preflow<SmartDigraph> pre(g,cap,s,t);
  pre.init(iflow);
  pre.startFirstPhase();
  check(pre.flowValue() == 10, "The incorrect max flow value.");
  check(pre.minCut(s), "Wrong min cut (Node s).");
  check(pre.minCut(n1), "Wrong min cut (Node n1).");
  check(!pre.minCut(n2), "Wrong min cut (Node n2).");
  check(!pre.minCut(t), "Wrong min cut (Node t).");
}

template <typename MF, typename SF>
void checkMaxFlowAlg() {
  typedef SmartDigraph Digraph;
  DIGRAPH_TYPEDEFS(Digraph);

  typedef typename MF::Value Value;
  typedef Digraph::ArcMap<Value> CapMap;
  typedef CapMap FlowMap;
  typedef BoolNodeMap CutMap;

  Digraph g;
  Node s, t;
  CapMap cap(g);
  std::istringstream input(test_lgf);
  DigraphReader<Digraph>(g,input)
      .arcMap("capacity", cap)
      .node("source",s)
      .node("target",t)
      .run();

  MF max_flow(g, cap, s, t);
  max_flow.run();

  check(checkFlow(g, max_flow.flowMap(), cap, s, t),
        "The flow is not feasible.");

  CutMap min_cut(g);
  max_flow.minCutMap(min_cut);
  Value min_cut_value = cutValue(g, min_cut, cap);

  check(max_flow.flowValue() == min_cut_value,
        "The max flow value is not equal to the min cut value.");

  FlowMap flow(g);
  for (ArcIt e(g); e != INVALID; ++e) flow[e] = max_flow.flowMap()[e];

  Value flow_value = max_flow.flowValue();

  for (ArcIt e(g); e != INVALID; ++e) cap[e] = 2 * cap[e];
  max_flow.init(flow);

  SF::startFirstPhase(max_flow);       // start first phase of the algorithm

  CutMap min_cut1(g);
  max_flow.minCutMap(min_cut1);
  min_cut_value = cutValue(g, min_cut1, cap);

  check(max_flow.flowValue() == min_cut_value &&
        min_cut_value == 2 * flow_value,
        "The max flow value or the min cut value is wrong.");

  SF::startSecondPhase(max_flow);       // start second phase of the algorithm

  check(checkFlow(g, max_flow.flowMap(), cap, s, t),
        "The flow is not feasible.");

  CutMap min_cut2(g);
  max_flow.minCutMap(min_cut2);
  min_cut_value = cutValue(g, min_cut2, cap);

  check(max_flow.flowValue() == min_cut_value &&
        min_cut_value == 2 * flow_value,
        "The max flow value or the min cut value was not doubled");


  max_flow.flowMap(flow);

  NodeIt tmp1(g, s);
  ++tmp1;
  if (tmp1 != INVALID) s = tmp1;

  NodeIt tmp2(g, t);
  ++tmp2;
  if (tmp2 != INVALID) t = tmp2;

  max_flow.source(s);
  max_flow.target(t);

  max_flow.run();

  CutMap min_cut3(g);
  max_flow.minCutMap(min_cut3);
  min_cut_value=cutValue(g, min_cut3, cap);

  check(max_flow.flowValue() == min_cut_value,
        "The max flow value or the min cut value is wrong.");
}

// Struct for calling start functions of a general max flow algorithm
template <typename MF>
struct GeneralStartFunctions {

  static void startFirstPhase(MF& mf) {
    mf.start();
  }

  static void startSecondPhase(MF& mf) {
    ::lemon::ignore_unused_variable_warning(mf);
  }

};

// Struct for calling start functions of Preflow
template <typename MF>
struct PreflowStartFunctions {

  static void startFirstPhase(MF& mf) {
    mf.startFirstPhase();
  }

  static void startSecondPhase(MF& mf) {
    mf.startSecondPhase();
  }

};

int main() {

  typedef concepts::Digraph GR;
  typedef concepts::ReadMap<GR::Arc, int> CM1;
  typedef concepts::ReadMap<GR::Arc, double> CM2;

  // Check the interface of Preflow
  checkConcept< MaxFlowClassConcept<GR, CM1>,
                Preflow<GR, CM1> >();
  checkConcept< MaxFlowClassConcept<GR, CM2>,
                Preflow<GR, CM2> >();

  // Check the interface of EdmondsKarp
  checkConcept< MaxFlowClassConcept<GR, CM1>,
                EdmondsKarp<GR, CM1> >();
  checkConcept< MaxFlowClassConcept<GR, CM2>,
                EdmondsKarp<GR, CM2> >();

  // Check Preflow
  typedef Preflow<SmartDigraph, SmartDigraph::ArcMap<int> > PType1;
  typedef Preflow<SmartDigraph, SmartDigraph::ArcMap<float> > PType2;
  checkMaxFlowAlg<PType1, PreflowStartFunctions<PType1> >();
  checkMaxFlowAlg<PType2, PreflowStartFunctions<PType2> >();
  initFlowTest();

  // Check EdmondsKarp
  typedef EdmondsKarp<SmartDigraph, SmartDigraph::ArcMap<int> > EKType1;
  typedef EdmondsKarp<SmartDigraph, SmartDigraph::ArcMap<float> > EKType2;
  checkMaxFlowAlg<EKType1, GeneralStartFunctions<EKType1> >();
  checkMaxFlowAlg<EKType2, GeneralStartFunctions<EKType2> >();

  initFlowTest();

  return 0;
}
