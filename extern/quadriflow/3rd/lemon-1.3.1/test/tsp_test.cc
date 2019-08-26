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

#include <lemon/full_graph.h>
#include <lemon/math.h>
#include <lemon/maps.h>
#include <lemon/random.h>
#include <lemon/dim2.h>

#include <lemon/nearest_neighbor_tsp.h>
#include <lemon/greedy_tsp.h>
#include <lemon/insertion_tsp.h>
#include <lemon/christofides_tsp.h>
#include <lemon/opt2_tsp.h>

#include "test_tools.h"

using namespace lemon;

// // Tests checkMetricCost() function
// void metricCostTest() {
//   GRAPH_TYPEDEFS(FullGraph);
//   FullGraph g(10);
//   check(checkMetricCost(g, constMap<Edge>(0)), "Wrong checkMetricCost()");
//   check(checkMetricCost(g, constMap<Edge>(1)), "Wrong checkMetricCost()");
//   check(!checkMetricCost(g, constMap<Edge>(-1)), "Wrong checkMetricCost()");
//
//   FullGraph::EdgeMap<float> cost(g);
//   for (NodeIt u(g); u != INVALID; ++u) {
//     for (NodeIt v(g); v != INVALID; ++v) {
//       if (u == v) continue;
//       float x1 = g.id(u), x2 = g.id(v);
//       float y1 = x1 * x1, y2 = x2 * x2;
//       cost[g.edge(u, v)] = std::sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
//     }
//   }
//   check(checkMetricCost(g, cost), "Wrong checkMetricCost()");
//   float eps = Tolerance<float>::defaultEpsilon();
//   cost[g.edge(g(0), g(9))] =
//     cost[g.edge(g(0), g(8))] + cost[g.edge(g(8), g(9))] + eps * 2;
//   check(!checkMetricCost(g, cost), "Wrong checkMetricCost()");
//   check(checkMetricCost(g, cost, Tolerance<float>(eps * 4)),
//     "Wrong checkMetricCost()");
// }

// Checks tour validity
template <typename Container>
bool checkTour(const FullGraph &gr, const Container &p) {
  FullGraph::NodeMap<bool> used(gr, false);

  int node_cnt = 0;
  for (typename Container::const_iterator it = p.begin(); it != p.end(); ++it)
    {
      FullGraph::Node node = *it;
      if (used[node]) return false;
      used[node] = true;
      ++node_cnt;
    }

  return (node_cnt == gr.nodeNum());
}

// Checks tour validity
bool checkTourPath(const FullGraph &gr, const Path<FullGraph> &p) {
  FullGraph::NodeMap<bool> used(gr, false);

  if (!checkPath(gr, p)) return false;
  if (gr.nodeNum() <= 1 && p.length() != 0) return false;
  if (gr.nodeNum() > 1 && p.length() != gr.nodeNum()) return false;

  for (int i = 0; i < p.length(); ++i) {
    if (used[gr.target(p.nth(i))]) return false;
    used[gr.target(p.nth(i))] = true;
  }
  return true;
}

// Checks tour cost
template <typename CostMap>
bool checkCost(const FullGraph &gr, const std::vector<FullGraph::Node> &p,
               const CostMap &cost, typename CostMap::Value total)
{
  typedef typename CostMap::Value Cost;

  Cost s = 0;
  for (int i = 0; i < int(p.size()) - 1; ++i)
    s += cost[gr.edge(p[i], p[i+1])];
  if (int(p.size()) >= 2)
    s += cost[gr.edge(p.back(), p.front())];

  return !Tolerance<Cost>().different(s, total);
}

// Checks tour cost
template <typename CostMap>
bool checkCost(const FullGraph &, const Path<FullGraph> &p,
               const CostMap &cost, typename CostMap::Value total)
{
  typedef typename CostMap::Value Cost;

  Cost s = 0;
  for (int i = 0; i < p.length(); ++i)
    s += cost[p.nth(i)];

  return !Tolerance<Cost>().different(s, total);
}

// Tests a TSP algorithm on small graphs
template <typename TSP>
void tspTestSmall(const std::string &alg_name) {
  GRAPH_TYPEDEFS(FullGraph);

  for (int n = 0; n <= 5; ++n) {
    FullGraph g(n);
    unsigned nsize = n;
    int esize = n <= 1 ? 0 : n;

    ConstMap<Edge, int> cost_map(1);
    TSP alg(g, cost_map);

    check(alg.run() == esize, alg_name + ": Wrong total cost");
    check(alg.tourCost() == esize, alg_name + ": Wrong total cost");

    std::list<Node> list1(nsize), list2;
    std::vector<Node> vec1(nsize), vec2;
    alg.tourNodes(list1.begin());
    alg.tourNodes(vec1.begin());
    alg.tourNodes(std::front_inserter(list2));
    alg.tourNodes(std::back_inserter(vec2));
    check(checkTour(g, alg.tourNodes()), alg_name + ": Wrong node sequence");
    check(checkTour(g, list1), alg_name + ": Wrong node sequence");
    check(checkTour(g, vec1), alg_name + ": Wrong node sequence");
    check(checkTour(g, list2), alg_name + ": Wrong node sequence");
    check(checkTour(g, vec2), alg_name + ": Wrong node sequence");
    check(checkCost(g, vec1, constMap<Edge, int>(1), esize),
      alg_name + ": Wrong tour cost");

    SimplePath<FullGraph> path;
    alg.tour(path);
    check(path.length() == esize, alg_name + ": Wrong tour");
    check(checkTourPath(g, path), alg_name + ": Wrong tour");
    check(checkCost(g, path, constMap<Edge, int>(1), esize),
      alg_name + ": Wrong tour cost");
  }
}

// Tests a TSP algorithm on random graphs
template <typename TSP>
void tspTestRandom(const std::string &alg_name) {
  GRAPH_TYPEDEFS(FullGraph);

  FullGraph g(20);
  FullGraph::NodeMap<dim2::Point<double> > pos(g);
  DoubleEdgeMap cost(g);

  TSP alg(g, cost);
  Opt2Tsp<DoubleEdgeMap > opt2(g, cost);

  for (int i = 1; i <= 3; i++) {
    for (NodeIt u(g); u != INVALID; ++u) {
      pos[u] = dim2::Point<double>(rnd(), rnd());
    }
    for (NodeIt u(g); u != INVALID; ++u) {
      for (NodeIt v(g); v != INVALID; ++v) {
        if (u == v) continue;
        cost[g.edge(u, v)] = (pos[u] - pos[v]).normSquare();
      }
    }

    check(alg.run() > 0, alg_name + ": Wrong total cost");

    std::vector<Node> vec;
    alg.tourNodes(std::back_inserter(vec));
    check(checkTour(g, vec), alg_name + ": Wrong node sequence");
    check(checkCost(g, vec, cost, alg.tourCost()),
      alg_name + ": Wrong tour cost");

    SimplePath<FullGraph> path;
    alg.tour(path);
    check(checkTourPath(g, path), alg_name + ": Wrong tour");
    check(checkCost(g, path, cost, alg.tourCost()),
      alg_name + ": Wrong tour cost");

    check(!Tolerance<double>().less(alg.tourCost(), opt2.run(alg.tourNodes())),
      "2-opt improvement: Wrong total cost");
    check(checkTour(g, opt2.tourNodes()),
      "2-opt improvement: Wrong node sequence");
    check(checkCost(g, opt2.tourNodes(), cost, opt2.tourCost()),
      "2-opt improvement: Wrong tour cost");

    check(!Tolerance<double>().less(alg.tourCost(), opt2.run(path)),
      "2-opt improvement: Wrong total cost");
    check(checkTour(g, opt2.tourNodes()),
      "2-opt improvement: Wrong node sequence");
    check(checkCost(g, opt2.tourNodes(), cost, opt2.tourCost()),
      "2-opt improvement: Wrong tour cost");
  }
}

// Algorithm class for Nearest Insertion
template <typename CM>
class NearestInsertionTsp : public InsertionTsp<CM> {
public:
  NearestInsertionTsp(const FullGraph &gr, const CM &cost)
    : InsertionTsp<CM>(gr, cost) {}
  typename CM::Value run() {
    return InsertionTsp<CM>::run(InsertionTsp<CM>::NEAREST);
  }
};

// Algorithm class for Farthest Insertion
template <typename CM>
class FarthestInsertionTsp : public InsertionTsp<CM> {
public:
  FarthestInsertionTsp(const FullGraph &gr, const CM &cost)
    : InsertionTsp<CM>(gr, cost) {}
  typename CM::Value run() {
    return InsertionTsp<CM>::run(InsertionTsp<CM>::FARTHEST);
  }
};

// Algorithm class for Cheapest Insertion
template <typename CM>
class CheapestInsertionTsp : public InsertionTsp<CM> {
public:
  CheapestInsertionTsp(const FullGraph &gr, const CM &cost)
    : InsertionTsp<CM>(gr, cost) {}
  typename CM::Value run() {
    return InsertionTsp<CM>::run(InsertionTsp<CM>::CHEAPEST);
  }
};

// Algorithm class for Random Insertion
template <typename CM>
class RandomInsertionTsp : public InsertionTsp<CM> {
public:
  RandomInsertionTsp(const FullGraph &gr, const CM &cost)
    : InsertionTsp<CM>(gr, cost) {}
  typename CM::Value run() {
    return InsertionTsp<CM>::run(InsertionTsp<CM>::RANDOM);
  }
};

int main() {
  GRAPH_TYPEDEFS(FullGraph);

  // metricCostTest();

  tspTestSmall<NearestNeighborTsp<ConstMap<Edge, int> > >("Nearest Neighbor");
  tspTestSmall<GreedyTsp<ConstMap<Edge, int> > >("Greedy");
  tspTestSmall<NearestInsertionTsp<ConstMap<Edge, int> > >("Nearest Insertion");
  tspTestSmall<FarthestInsertionTsp<ConstMap<Edge, int> > >
    ("Farthest Insertion");
  tspTestSmall<CheapestInsertionTsp<ConstMap<Edge, int> > >
    ("Cheapest Insertion");
  tspTestSmall<RandomInsertionTsp<ConstMap<Edge, int> > >("Random Insertion");
  tspTestSmall<ChristofidesTsp<ConstMap<Edge, int> > >("Christofides");
  tspTestSmall<Opt2Tsp<ConstMap<Edge, int> > >("2-opt");

  tspTestRandom<NearestNeighborTsp<DoubleEdgeMap > >("Nearest Neighbor");
  tspTestRandom<GreedyTsp<DoubleEdgeMap > >("Greedy");
  tspTestRandom<NearestInsertionTsp<DoubleEdgeMap > >("Nearest Insertion");
  tspTestRandom<FarthestInsertionTsp<DoubleEdgeMap > >("Farthest Insertion");
  tspTestRandom<CheapestInsertionTsp<DoubleEdgeMap > >("Cheapest Insertion");
  tspTestRandom<RandomInsertionTsp<DoubleEdgeMap > >("Random Insertion");
  tspTestRandom<ChristofidesTsp<DoubleEdgeMap > >("Christofides");
  tspTestRandom<Opt2Tsp<DoubleEdgeMap > >("2-opt");

  return 0;
}
