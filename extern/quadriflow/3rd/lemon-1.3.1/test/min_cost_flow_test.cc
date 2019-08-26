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
#include <limits>

#include <lemon/list_graph.h>
#include <lemon/lgf_reader.h>

#include <lemon/network_simplex.h>
#include <lemon/capacity_scaling.h>
#include <lemon/cost_scaling.h>
#include <lemon/cycle_canceling.h>

#include <lemon/concepts/digraph.h>
#include <lemon/concepts/heap.h>
#include <lemon/concept_check.h>

#include "test_tools.h"

using namespace lemon;

// Test networks
char test_lgf[] =
  "@nodes\n"
  "label  sup1 sup2 sup3 sup4 sup5 sup6\n"
  "    1    20   27    0   30   20   30\n"
  "    2    -4    0    0    0   -8   -3\n"
  "    3     0    0    0    0    0    0\n"
  "    4     0    0    0    0    0    0\n"
  "    5     9    0    0    0    6   11\n"
  "    6    -6    0    0    0   -5   -6\n"
  "    7     0    0    0    0    0    0\n"
  "    8     0    0    0    0    0    3\n"
  "    9     3    0    0    0    0    0\n"
  "   10    -2    0    0    0   -7   -2\n"
  "   11     0    0    0    0  -10    0\n"
  "   12   -20  -27    0  -30  -30  -20\n"
  "\n"
  "@arcs\n"
  "       cost  cap low1 low2 low3\n"
  " 1  2    70   11    0    8    8\n"
  " 1  3   150    3    0    1    0\n"
  " 1  4    80   15    0    2    2\n"
  " 2  8    80   12    0    0    0\n"
  " 3  5   140    5    0    3    1\n"
  " 4  6    60   10    0    1    0\n"
  " 4  7    80    2    0    0    0\n"
  " 4  8   110    3    0    0    0\n"
  " 5  7    60   14    0    0    0\n"
  " 5 11   120   12    0    0    0\n"
  " 6  3     0    3    0    0    0\n"
  " 6  9   140    4    0    0    0\n"
  " 6 10    90    8    0    0    0\n"
  " 7  1    30    5    0    0   -5\n"
  " 8 12    60   16    0    4    3\n"
  " 9 12    50    6    0    0    0\n"
  "10 12    70   13    0    5    2\n"
  "10  2   100    7    0    0    0\n"
  "10  7    60   10    0    0   -3\n"
  "11 10    20   14    0    6  -20\n"
  "12 11    30   10    0    0  -10\n"
  "\n"
  "@attributes\n"
  "source 1\n"
  "target 12\n";

char test_neg1_lgf[] =
  "@nodes\n"
  "label   sup\n"
  "    1   100\n"
  "    2     0\n"
  "    3     0\n"
  "    4  -100\n"
  "    5     0\n"
  "    6     0\n"
  "    7     0\n"
  "@arcs\n"
  "      cost   low1   low2\n"
  "1 2    100      0      0\n"
  "1 3     30      0      0\n"
  "2 4     20      0      0\n"
  "3 4     80      0      0\n"
  "3 2     50      0      0\n"
  "5 3     10      0      0\n"
  "5 6     80      0   1000\n"
  "6 7     30      0  -1000\n"
  "7 5   -120      0      0\n";

char test_neg2_lgf[] =
  "@nodes\n"
  "label   sup\n"
  "    1   100\n"
  "    2  -300\n"
  "@arcs\n"
  "      cost\n"
  "1 2     -1\n";


// Test data
typedef ListDigraph Digraph;
DIGRAPH_TYPEDEFS(ListDigraph);

Digraph gr;
Digraph::ArcMap<int> c(gr), l1(gr), l2(gr), l3(gr), u(gr);
Digraph::NodeMap<int> s1(gr), s2(gr), s3(gr), s4(gr), s5(gr), s6(gr);
ConstMap<Arc, int> cc(1), cu(std::numeric_limits<int>::max());
Node v, w;

Digraph neg1_gr;
Digraph::ArcMap<int> neg1_c(neg1_gr), neg1_l1(neg1_gr), neg1_l2(neg1_gr);
ConstMap<Arc, int> neg1_u1(std::numeric_limits<int>::max()), neg1_u2(5000);
Digraph::NodeMap<int> neg1_s(neg1_gr);

Digraph neg2_gr;
Digraph::ArcMap<int> neg2_c(neg2_gr);
ConstMap<Arc, int> neg2_l(0), neg2_u(1000);
Digraph::NodeMap<int> neg2_s(neg2_gr);


enum SupplyType {
  EQ,
  GEQ,
  LEQ
};


// Check the interface of an MCF algorithm
template <typename GR, typename Value, typename Cost>
class McfClassConcept
{
public:

  template <typename MCF>
  struct Constraints {
    void constraints() {
      checkConcept<concepts::Digraph, GR>();

      const Constraints& me = *this;

      MCF mcf(me.g);
      const MCF& const_mcf = mcf;

      b = mcf.reset().resetParams()
             .lowerMap(me.lower)
             .upperMap(me.upper)
             .costMap(me.cost)
             .supplyMap(me.sup)
             .stSupply(me.n, me.n, me.k)
             .run();

      c = const_mcf.totalCost();
      x = const_mcf.template totalCost<double>();
      v = const_mcf.flow(me.a);
      c = const_mcf.potential(me.n);
      const_mcf.flowMap(fm);
      const_mcf.potentialMap(pm);
    }

    typedef typename GR::Node Node;
    typedef typename GR::Arc Arc;
    typedef concepts::ReadMap<Node, Value> NM;
    typedef concepts::ReadMap<Arc, Value> VAM;
    typedef concepts::ReadMap<Arc, Cost> CAM;
    typedef concepts::WriteMap<Arc, Value> FlowMap;
    typedef concepts::WriteMap<Node, Cost> PotMap;

    GR g;
    VAM lower;
    VAM upper;
    CAM cost;
    NM sup;
    Node n;
    Arc a;
    Value k;

    FlowMap fm;
    PotMap pm;
    bool b;
    double x;
    typename MCF::Value v;
    typename MCF::Cost c;
  };

};


// Check the feasibility of the given flow (primal soluiton)
template < typename GR, typename LM, typename UM,
           typename SM, typename FM >
bool checkFlow( const GR& gr, const LM& lower, const UM& upper,
                const SM& supply, const FM& flow,
                SupplyType type = EQ )
{
  TEMPLATE_DIGRAPH_TYPEDEFS(GR);

  for (ArcIt e(gr); e != INVALID; ++e) {
    if (flow[e] < lower[e] || flow[e] > upper[e]) return false;
  }

  for (NodeIt n(gr); n != INVALID; ++n) {
    typename SM::Value sum = 0;
    for (OutArcIt e(gr, n); e != INVALID; ++e)
      sum += flow[e];
    for (InArcIt e(gr, n); e != INVALID; ++e)
      sum -= flow[e];
    bool b = (type ==  EQ && sum == supply[n]) ||
             (type == GEQ && sum >= supply[n]) ||
             (type == LEQ && sum <= supply[n]);
    if (!b) return false;
  }

  return true;
}

// Check the feasibility of the given potentials (dual soluiton)
// using the "Complementary Slackness" optimality condition
template < typename GR, typename LM, typename UM,
           typename CM, typename SM, typename FM, typename PM >
bool checkPotential( const GR& gr, const LM& lower, const UM& upper,
                     const CM& cost, const SM& supply, const FM& flow,
                     const PM& pi, SupplyType type )
{
  TEMPLATE_DIGRAPH_TYPEDEFS(GR);

  bool opt = true;
  for (ArcIt e(gr); opt && e != INVALID; ++e) {
    typename CM::Value red_cost =
      cost[e] + pi[gr.source(e)] - pi[gr.target(e)];
    opt = red_cost == 0 ||
          (red_cost > 0 && flow[e] == lower[e]) ||
          (red_cost < 0 && flow[e] == upper[e]);
  }

  for (NodeIt n(gr); opt && n != INVALID; ++n) {
    typename SM::Value sum = 0;
    for (OutArcIt e(gr, n); e != INVALID; ++e)
      sum += flow[e];
    for (InArcIt e(gr, n); e != INVALID; ++e)
      sum -= flow[e];
    if (type != LEQ) {
      opt = (pi[n] <= 0) && (sum == supply[n] || pi[n] == 0);
    } else {
      opt = (pi[n] >= 0) && (sum == supply[n] || pi[n] == 0);
    }
  }

  return opt;
}

// Check whether the dual cost is equal to the primal cost
template < typename GR, typename LM, typename UM,
           typename CM, typename SM, typename PM >
bool checkDualCost( const GR& gr, const LM& lower, const UM& upper,
                    const CM& cost, const SM& supply, const PM& pi,
                    typename CM::Value total )
{
  TEMPLATE_DIGRAPH_TYPEDEFS(GR);

  typename CM::Value dual_cost = 0;
  SM red_supply(gr);
  for (NodeIt n(gr); n != INVALID; ++n) {
    red_supply[n] = supply[n];
  }
  for (ArcIt a(gr); a != INVALID; ++a) {
    if (lower[a] != 0) {
      dual_cost += lower[a] * cost[a];
      red_supply[gr.source(a)] -= lower[a];
      red_supply[gr.target(a)] += lower[a];
    }
  }

  for (NodeIt n(gr); n != INVALID; ++n) {
    dual_cost -= red_supply[n] * pi[n];
  }
  for (ArcIt a(gr); a != INVALID; ++a) {
    typename CM::Value red_cost =
      cost[a] + pi[gr.source(a)] - pi[gr.target(a)];
    dual_cost -= (upper[a] - lower[a]) * std::max(-red_cost, 0);
  }

  return dual_cost == total;
}

// Run a minimum cost flow algorithm and check the results
template < typename MCF, typename GR,
           typename LM, typename UM,
           typename CM, typename SM,
           typename PT >
void checkMcf( const MCF& mcf, PT mcf_result,
               const GR& gr, const LM& lower, const UM& upper,
               const CM& cost, const SM& supply,
               PT result, bool optimal, typename CM::Value total,
               const std::string &test_id = "",
               SupplyType type = EQ )
{
  check(mcf_result == result, "Wrong result " + test_id);
  if (optimal) {
    typename GR::template ArcMap<typename SM::Value> flow(gr);
    typename GR::template NodeMap<typename CM::Value> pi(gr);
    mcf.flowMap(flow);
    mcf.potentialMap(pi);
    check(checkFlow(gr, lower, upper, supply, flow, type),
          "The flow is not feasible " + test_id);
    check(mcf.totalCost() == total, "The flow is not optimal " + test_id);
    check(checkPotential(gr, lower, upper, cost, supply, flow, pi, type),
          "Wrong potentials " + test_id);
    check(checkDualCost(gr, lower, upper, cost, supply, pi, total),
          "Wrong dual cost " + test_id);
  }
}

template < typename MCF, typename Param >
void runMcfGeqTests( Param param,
                     const std::string &test_str = "",
                     bool full_neg_cost_support = false )
{
  MCF mcf1(gr), mcf2(neg1_gr), mcf3(neg2_gr);

  // Basic tests
  mcf1.upperMap(u).costMap(c).supplyMap(s1);
  checkMcf(mcf1, mcf1.run(param), gr, l1, u, c, s1,
           mcf1.OPTIMAL, true,     5240, test_str + "-1");
  mcf1.stSupply(v, w, 27);
  checkMcf(mcf1, mcf1.run(param), gr, l1, u, c, s2,
           mcf1.OPTIMAL, true,     7620, test_str + "-2");
  mcf1.lowerMap(l2).supplyMap(s1);
  checkMcf(mcf1, mcf1.run(param), gr, l2, u, c, s1,
           mcf1.OPTIMAL, true,     5970, test_str + "-3");
  mcf1.stSupply(v, w, 27);
  checkMcf(mcf1, mcf1.run(param), gr, l2, u, c, s2,
           mcf1.OPTIMAL, true,     8010, test_str + "-4");
  mcf1.resetParams().supplyMap(s1);
  checkMcf(mcf1, mcf1.run(param), gr, l1, cu, cc, s1,
           mcf1.OPTIMAL, true,       74, test_str + "-5");
  mcf1.lowerMap(l2).stSupply(v, w, 27);
  checkMcf(mcf1, mcf1.run(param), gr, l2, cu, cc, s2,
           mcf1.OPTIMAL, true,       94, test_str + "-6");
  mcf1.reset();
  checkMcf(mcf1, mcf1.run(param), gr, l1, cu, cc, s3,
           mcf1.OPTIMAL, true,        0, test_str + "-7");
  mcf1.lowerMap(l2).upperMap(u);
  checkMcf(mcf1, mcf1.run(param), gr, l2, u, cc, s3,
           mcf1.INFEASIBLE, false,    0, test_str + "-8");
  mcf1.lowerMap(l3).upperMap(u).costMap(c).supplyMap(s4);
  checkMcf(mcf1, mcf1.run(param), gr, l3, u, c, s4,
           mcf1.OPTIMAL, true,     6360, test_str + "-9");

  // Tests for the GEQ form
  mcf1.resetParams().upperMap(u).costMap(c).supplyMap(s5);
  checkMcf(mcf1, mcf1.run(param), gr, l1, u, c, s5,
           mcf1.OPTIMAL, true,     3530, test_str + "-10", GEQ);
  mcf1.lowerMap(l2);
  checkMcf(mcf1, mcf1.run(param), gr, l2, u, c, s5,
           mcf1.OPTIMAL, true,     4540, test_str + "-11", GEQ);
  mcf1.supplyMap(s6);
  checkMcf(mcf1, mcf1.run(param), gr, l2, u, c, s6,
           mcf1.INFEASIBLE, false,    0, test_str + "-12", GEQ);

  // Tests with negative costs
  mcf2.lowerMap(neg1_l1).costMap(neg1_c).supplyMap(neg1_s);
  checkMcf(mcf2, mcf2.run(param), neg1_gr, neg1_l1, neg1_u1, neg1_c, neg1_s,
           mcf2.UNBOUNDED, false,     0, test_str + "-13");
  mcf2.upperMap(neg1_u2);
  checkMcf(mcf2, mcf2.run(param), neg1_gr, neg1_l1, neg1_u2, neg1_c, neg1_s,
           mcf2.OPTIMAL, true,   -40000, test_str + "-14");
  mcf2.resetParams().lowerMap(neg1_l2).costMap(neg1_c).supplyMap(neg1_s);
  checkMcf(mcf2, mcf2.run(param), neg1_gr, neg1_l2, neg1_u1, neg1_c, neg1_s,
           mcf2.UNBOUNDED, false,     0, test_str + "-15");

  mcf3.costMap(neg2_c).supplyMap(neg2_s);
  if (full_neg_cost_support) {
    checkMcf(mcf3, mcf3.run(param), neg2_gr, neg2_l, neg2_u, neg2_c, neg2_s,
             mcf3.OPTIMAL, true,   -300, test_str + "-16", GEQ);
  } else {
    checkMcf(mcf3, mcf3.run(param), neg2_gr, neg2_l, neg2_u, neg2_c, neg2_s,
             mcf3.UNBOUNDED, false,   0, test_str + "-17", GEQ);
  }
  mcf3.upperMap(neg2_u);
  checkMcf(mcf3, mcf3.run(param), neg2_gr, neg2_l, neg2_u, neg2_c, neg2_s,
           mcf3.OPTIMAL, true,     -300, test_str + "-18", GEQ);

  // Tests for empty graph
  Digraph gr0;
  MCF mcf0(gr0);
  mcf0.run(param);
  check(mcf0.totalCost() == 0, "Wrong total cost");  
}

template < typename MCF, typename Param >
void runMcfLeqTests( Param param,
                     const std::string &test_str = "" )
{
  // Tests for the LEQ form
  MCF mcf1(gr);
  mcf1.supplyType(mcf1.LEQ);
  mcf1.upperMap(u).costMap(c).supplyMap(s6);
  checkMcf(mcf1, mcf1.run(param), gr, l1, u, c, s6,
           mcf1.OPTIMAL, true,   5080, test_str + "-19", LEQ);
  mcf1.lowerMap(l2);
  checkMcf(mcf1, mcf1.run(param), gr, l2, u, c, s6,
           mcf1.OPTIMAL, true,   5930, test_str + "-20", LEQ);
  mcf1.supplyMap(s5);
  checkMcf(mcf1, mcf1.run(param), gr, l2, u, c, s5,
           mcf1.INFEASIBLE, false,  0, test_str + "-21", LEQ);
}


int main()
{
  // Read the test networks
  std::istringstream input(test_lgf);
  DigraphReader<Digraph>(gr, input)
    .arcMap("cost", c)
    .arcMap("cap", u)
    .arcMap("low1", l1)
    .arcMap("low2", l2)
    .arcMap("low3", l3)
    .nodeMap("sup1", s1)
    .nodeMap("sup2", s2)
    .nodeMap("sup3", s3)
    .nodeMap("sup4", s4)
    .nodeMap("sup5", s5)
    .nodeMap("sup6", s6)
    .node("source", v)
    .node("target", w)
    .run();

  std::istringstream neg_inp1(test_neg1_lgf);
  DigraphReader<Digraph>(neg1_gr, neg_inp1)
    .arcMap("cost", neg1_c)
    .arcMap("low1", neg1_l1)
    .arcMap("low2", neg1_l2)
    .nodeMap("sup", neg1_s)
    .run();

  std::istringstream neg_inp2(test_neg2_lgf);
  DigraphReader<Digraph>(neg2_gr, neg_inp2)
    .arcMap("cost", neg2_c)
    .nodeMap("sup", neg2_s)
    .run();

  // Check the interface of NetworkSimplex
  {
    typedef concepts::Digraph GR;
    checkConcept< McfClassConcept<GR, int, int>,
                  NetworkSimplex<GR> >();
    checkConcept< McfClassConcept<GR, double, double>,
                  NetworkSimplex<GR, double> >();
    checkConcept< McfClassConcept<GR, int, double>,
                  NetworkSimplex<GR, int, double> >();
  }

  // Check the interface of CapacityScaling
  {
    typedef concepts::Digraph GR;
    checkConcept< McfClassConcept<GR, int, int>,
                  CapacityScaling<GR> >();
    checkConcept< McfClassConcept<GR, double, double>,
                  CapacityScaling<GR, double> >();
    checkConcept< McfClassConcept<GR, int, double>,
                  CapacityScaling<GR, int, double> >();
    typedef CapacityScaling<GR>::
      SetHeap<concepts::Heap<int, RangeMap<int> > >::Create CAS;
    checkConcept< McfClassConcept<GR, int, int>, CAS >();
  }

  // Check the interface of CostScaling
  {
    typedef concepts::Digraph GR;
    checkConcept< McfClassConcept<GR, int, int>,
                  CostScaling<GR> >();
    checkConcept< McfClassConcept<GR, double, double>,
                  CostScaling<GR, double> >();
    checkConcept< McfClassConcept<GR, int, double>,
                  CostScaling<GR, int, double> >();
    typedef CostScaling<GR>::
      SetLargeCost<double>::Create COS;
    checkConcept< McfClassConcept<GR, int, int>, COS >();
  }

  // Check the interface of CycleCanceling
  {
    typedef concepts::Digraph GR;
    checkConcept< McfClassConcept<GR, int, int>,
                  CycleCanceling<GR> >();
    checkConcept< McfClassConcept<GR, double, double>,
                  CycleCanceling<GR, double> >();
    checkConcept< McfClassConcept<GR, int, double>,
                  CycleCanceling<GR, int, double> >();
  }

  // Test NetworkSimplex
  {
    typedef NetworkSimplex<Digraph> MCF;
    runMcfGeqTests<MCF>(MCF::FIRST_ELIGIBLE, "NS-FE", true);
    runMcfLeqTests<MCF>(MCF::FIRST_ELIGIBLE, "NS-FE");
    runMcfGeqTests<MCF>(MCF::BEST_ELIGIBLE,  "NS-BE", true);
    runMcfLeqTests<MCF>(MCF::BEST_ELIGIBLE,  "NS-BE");
    runMcfGeqTests<MCF>(MCF::BLOCK_SEARCH,   "NS-BS", true);
    runMcfLeqTests<MCF>(MCF::BLOCK_SEARCH,   "NS-BS");
    runMcfGeqTests<MCF>(MCF::CANDIDATE_LIST, "NS-CL", true);
    runMcfLeqTests<MCF>(MCF::CANDIDATE_LIST, "NS-CL");
    runMcfGeqTests<MCF>(MCF::ALTERING_LIST,  "NS-AL", true);
    runMcfLeqTests<MCF>(MCF::ALTERING_LIST,  "NS-AL");
  }

  // Test CapacityScaling
  {
    typedef CapacityScaling<Digraph> MCF;
    runMcfGeqTests<MCF>(0, "SSP");
    runMcfGeqTests<MCF>(2, "CAS");
  }

  // Test CostScaling
  {
    typedef CostScaling<Digraph> MCF;
    runMcfGeqTests<MCF>(MCF::PUSH, "COS-PR");
    runMcfGeqTests<MCF>(MCF::AUGMENT, "COS-AR");
    runMcfGeqTests<MCF>(MCF::PARTIAL_AUGMENT, "COS-PAR");
  }

  // Test CycleCanceling
  {
    typedef CycleCanceling<Digraph> MCF;
    runMcfGeqTests<MCF>(MCF::SIMPLE_CYCLE_CANCELING, "SCC");
    runMcfGeqTests<MCF>(MCF::MINIMUM_MEAN_CYCLE_CANCELING, "MMCC");
    runMcfGeqTests<MCF>(MCF::CANCEL_AND_TIGHTEN, "CAT");
  }

  return 0;
}
