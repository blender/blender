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

#include <lemon/connectivity.h>
#include <lemon/list_graph.h>
#include <lemon/adaptors.h>

#include "test_tools.h"

using namespace lemon;


int main()
{
  typedef ListDigraph Digraph;
  typedef Undirector<Digraph> Graph;

  {
    Digraph d;
    Digraph::NodeMap<int> order(d);
    Graph g(d);

    check(stronglyConnected(d), "The empty digraph is strongly connected");
    check(countStronglyConnectedComponents(d) == 0,
          "The empty digraph has 0 strongly connected component");
    check(connected(g), "The empty graph is connected");
    check(countConnectedComponents(g) == 0,
          "The empty graph has 0 connected component");

    check(biNodeConnected(g), "The empty graph is bi-node-connected");
    check(countBiNodeConnectedComponents(g) == 0,
          "The empty graph has 0 bi-node-connected component");
    check(biEdgeConnected(g), "The empty graph is bi-edge-connected");
    check(countBiEdgeConnectedComponents(g) == 0,
          "The empty graph has 0 bi-edge-connected component");

    check(dag(d), "The empty digraph is DAG.");
    check(checkedTopologicalSort(d, order), "The empty digraph is DAG.");
    check(loopFree(d), "The empty digraph is loop-free.");
    check(parallelFree(d), "The empty digraph is parallel-free.");
    check(simpleGraph(d), "The empty digraph is simple.");

    check(acyclic(g), "The empty graph is acyclic.");
    check(tree(g), "The empty graph is tree.");
    check(bipartite(g), "The empty graph is bipartite.");
    check(loopFree(g), "The empty graph is loop-free.");
    check(parallelFree(g), "The empty graph is parallel-free.");
    check(simpleGraph(g), "The empty graph is simple.");
  }

  {
    Digraph d;
    Digraph::NodeMap<int> order(d);
    Graph g(d);
    Digraph::Node n = d.addNode();
    ::lemon::ignore_unused_variable_warning(n);

    check(stronglyConnected(d), "This digraph is strongly connected");
    check(countStronglyConnectedComponents(d) == 1,
          "This digraph has 1 strongly connected component");
    check(connected(g), "This graph is connected");
    check(countConnectedComponents(g) == 1,
          "This graph has 1 connected component");

    check(biNodeConnected(g), "This graph is bi-node-connected");
    check(countBiNodeConnectedComponents(g) == 0,
          "This graph has 0 bi-node-connected component");
    check(biEdgeConnected(g), "This graph is bi-edge-connected");
    check(countBiEdgeConnectedComponents(g) == 1,
          "This graph has 1 bi-edge-connected component");

    check(dag(d), "This digraph is DAG.");
    check(checkedTopologicalSort(d, order), "This digraph is DAG.");
    check(loopFree(d), "This digraph is loop-free.");
    check(parallelFree(d), "This digraph is parallel-free.");
    check(simpleGraph(d), "This digraph is simple.");

    check(acyclic(g), "This graph is acyclic.");
    check(tree(g), "This graph is tree.");
    check(bipartite(g), "This graph is bipartite.");
    check(loopFree(g), "This graph is loop-free.");
    check(parallelFree(g), "This graph is parallel-free.");
    check(simpleGraph(g), "This graph is simple.");
  }

  {
    ListGraph g;
    ListGraph::NodeMap<bool> map(g);

    ListGraph::Node n1 = g.addNode();
    ListGraph::Node n2 = g.addNode();

    ListGraph::Edge e1 = g.addEdge(n1, n2);
    ::lemon::ignore_unused_variable_warning(e1);
    check(biNodeConnected(g), "Graph is bi-node-connected");

    ListGraph::Node n3 = g.addNode();
    ::lemon::ignore_unused_variable_warning(n3);
    check(!biNodeConnected(g), "Graph is not bi-node-connected");
  }


  {
    Digraph d;
    Digraph::NodeMap<int> order(d);
    Graph g(d);

    Digraph::Node n1 = d.addNode();
    Digraph::Node n2 = d.addNode();
    Digraph::Node n3 = d.addNode();
    Digraph::Node n4 = d.addNode();
    Digraph::Node n5 = d.addNode();
    Digraph::Node n6 = d.addNode();

    d.addArc(n1, n3);
    d.addArc(n3, n2);
    d.addArc(n2, n1);
    d.addArc(n4, n2);
    d.addArc(n4, n3);
    d.addArc(n5, n6);
    d.addArc(n6, n5);

    check(!stronglyConnected(d), "This digraph is not strongly connected");
    check(countStronglyConnectedComponents(d) == 3,
          "This digraph has 3 strongly connected components");
    check(!connected(g), "This graph is not connected");
    check(countConnectedComponents(g) == 2,
          "This graph has 2 connected components");

    check(!dag(d), "This digraph is not DAG.");
    check(!checkedTopologicalSort(d, order), "This digraph is not DAG.");
    check(loopFree(d), "This digraph is loop-free.");
    check(parallelFree(d), "This digraph is parallel-free.");
    check(simpleGraph(d), "This digraph is simple.");

    check(!acyclic(g), "This graph is not acyclic.");
    check(!tree(g), "This graph is not tree.");
    check(!bipartite(g), "This graph is not bipartite.");
    check(loopFree(g), "This graph is loop-free.");
    check(!parallelFree(g), "This graph is not parallel-free.");
    check(!simpleGraph(g), "This graph is not simple.");

    d.addArc(n3, n3);

    check(!loopFree(d), "This digraph is not loop-free.");
    check(!loopFree(g), "This graph is not loop-free.");
    check(!simpleGraph(d), "This digraph is not simple.");

    d.addArc(n3, n2);

    check(!parallelFree(d), "This digraph is not parallel-free.");
  }

  {
    Digraph d;
    Digraph::ArcMap<bool> cutarcs(d, false);
    Graph g(d);

    Digraph::Node n1 = d.addNode();
    Digraph::Node n2 = d.addNode();
    Digraph::Node n3 = d.addNode();
    Digraph::Node n4 = d.addNode();
    Digraph::Node n5 = d.addNode();
    Digraph::Node n6 = d.addNode();
    Digraph::Node n7 = d.addNode();
    Digraph::Node n8 = d.addNode();

    d.addArc(n1, n2);
    d.addArc(n5, n1);
    d.addArc(n2, n8);
    d.addArc(n8, n5);
    d.addArc(n6, n4);
    d.addArc(n4, n6);
    d.addArc(n2, n5);
    d.addArc(n1, n8);
    d.addArc(n6, n7);
    d.addArc(n7, n6);

    check(!stronglyConnected(d), "This digraph is not strongly connected");
    check(countStronglyConnectedComponents(d) == 3,
          "This digraph has 3 strongly connected components");
    Digraph::NodeMap<int> scomp1(d);
    check(stronglyConnectedComponents(d, scomp1) == 3,
          "This digraph has 3 strongly connected components");
    check(scomp1[n1] != scomp1[n3] && scomp1[n1] != scomp1[n4] &&
          scomp1[n3] != scomp1[n4], "Wrong stronglyConnectedComponents()");
    check(scomp1[n1] == scomp1[n2] && scomp1[n1] == scomp1[n5] &&
          scomp1[n1] == scomp1[n8], "Wrong stronglyConnectedComponents()");
    check(scomp1[n4] == scomp1[n6] && scomp1[n4] == scomp1[n7],
          "Wrong stronglyConnectedComponents()");
    Digraph::ArcMap<bool> scut1(d, false);
    check(stronglyConnectedCutArcs(d, scut1) == 0,
          "This digraph has 0 strongly connected cut arc.");
    for (Digraph::ArcIt a(d); a != INVALID; ++a) {
      check(!scut1[a], "Wrong stronglyConnectedCutArcs()");
    }

    check(!connected(g), "This graph is not connected");
    check(countConnectedComponents(g) == 3,
          "This graph has 3 connected components");
    Graph::NodeMap<int> comp(g);
    check(connectedComponents(g, comp) == 3,
          "This graph has 3 connected components");
    check(comp[n1] != comp[n3] && comp[n1] != comp[n4] &&
          comp[n3] != comp[n4], "Wrong connectedComponents()");
    check(comp[n1] == comp[n2] && comp[n1] == comp[n5] &&
          comp[n1] == comp[n8], "Wrong connectedComponents()");
    check(comp[n4] == comp[n6] && comp[n4] == comp[n7],
          "Wrong connectedComponents()");

    cutarcs[d.addArc(n3, n1)] = true;
    cutarcs[d.addArc(n3, n5)] = true;
    cutarcs[d.addArc(n3, n8)] = true;
    cutarcs[d.addArc(n8, n6)] = true;
    cutarcs[d.addArc(n8, n7)] = true;

    check(!stronglyConnected(d), "This digraph is not strongly connected");
    check(countStronglyConnectedComponents(d) == 3,
          "This digraph has 3 strongly connected components");
    Digraph::NodeMap<int> scomp2(d);
    check(stronglyConnectedComponents(d, scomp2) == 3,
          "This digraph has 3 strongly connected components");
    check(scomp2[n3] == 0, "Wrong stronglyConnectedComponents()");
    check(scomp2[n1] == 1 && scomp2[n2] == 1 && scomp2[n5] == 1 &&
          scomp2[n8] == 1, "Wrong stronglyConnectedComponents()");
    check(scomp2[n4] == 2 && scomp2[n6] == 2 && scomp2[n7] == 2,
          "Wrong stronglyConnectedComponents()");
    Digraph::ArcMap<bool> scut2(d, false);
    check(stronglyConnectedCutArcs(d, scut2) == 5,
          "This digraph has 5 strongly connected cut arcs.");
    for (Digraph::ArcIt a(d); a != INVALID; ++a) {
      check(scut2[a] == cutarcs[a], "Wrong stronglyConnectedCutArcs()");
    }
  }

  {
    // DAG example for topological sort from the book New Algorithms
    // (T. H. Cormen, C. E. Leiserson, R. L. Rivest, C. Stein)
    Digraph d;
    Digraph::NodeMap<int> order(d);

    Digraph::Node belt = d.addNode();
    Digraph::Node trousers = d.addNode();
    Digraph::Node necktie = d.addNode();
    Digraph::Node coat = d.addNode();
    Digraph::Node socks = d.addNode();
    Digraph::Node shirt = d.addNode();
    Digraph::Node shoe = d.addNode();
    Digraph::Node watch = d.addNode();
    Digraph::Node pants = d.addNode();
    ::lemon::ignore_unused_variable_warning(watch);

    d.addArc(socks, shoe);
    d.addArc(pants, shoe);
    d.addArc(pants, trousers);
    d.addArc(trousers, shoe);
    d.addArc(trousers, belt);
    d.addArc(belt, coat);
    d.addArc(shirt, belt);
    d.addArc(shirt, necktie);
    d.addArc(necktie, coat);

    check(dag(d), "This digraph is DAG.");
    topologicalSort(d, order);
    for (Digraph::ArcIt a(d); a != INVALID; ++a) {
      check(order[d.source(a)] < order[d.target(a)],
            "Wrong topologicalSort()");
    }
  }

  {
    ListGraph g;
    ListGraph::NodeMap<bool> map(g);

    ListGraph::Node n1 = g.addNode();
    ListGraph::Node n2 = g.addNode();
    ListGraph::Node n3 = g.addNode();
    ListGraph::Node n4 = g.addNode();
    ListGraph::Node n5 = g.addNode();
    ListGraph::Node n6 = g.addNode();
    ListGraph::Node n7 = g.addNode();

    g.addEdge(n1, n3);
    g.addEdge(n1, n4);
    g.addEdge(n2, n5);
    g.addEdge(n3, n6);
    g.addEdge(n4, n6);
    g.addEdge(n4, n7);
    g.addEdge(n5, n7);

    check(bipartite(g), "This graph is bipartite");
    check(bipartitePartitions(g, map), "This graph is bipartite");

    check(map[n1] == map[n2] && map[n1] == map[n6] && map[n1] == map[n7],
          "Wrong bipartitePartitions()");
    check(map[n3] == map[n4] && map[n3] == map[n5],
          "Wrong bipartitePartitions()");
  }

  return 0;
}
