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

#ifndef LEMON_TEST_GRAPH_TEST_H
#define LEMON_TEST_GRAPH_TEST_H

#include <set>

#include <lemon/core.h>
#include <lemon/maps.h>

#include "test_tools.h"

namespace lemon {

  template<class Graph>
  void checkGraphNodeList(const Graph &G, int cnt)
  {
    typename Graph::NodeIt n(G);
    for(int i=0;i<cnt;i++) {
      check(n!=INVALID,"Wrong Node list linking.");
      ++n;
    }
    check(n==INVALID,"Wrong Node list linking.");
    check(countNodes(G)==cnt,"Wrong Node number.");
  }

  template<class Graph>
  void checkGraphRedNodeList(const Graph &G, int cnt)
  {
    typename Graph::RedNodeIt n(G);
    for(int i=0;i<cnt;i++) {
      check(n!=INVALID,"Wrong red Node list linking.");
      check(G.red(n),"Wrong node set check.");
      check(!G.blue(n),"Wrong node set check.");
      typename Graph::Node nn = n;
      check(G.asRedNodeUnsafe(nn) == n,"Wrong node conversion.");
      check(G.asRedNode(nn) == n,"Wrong node conversion.");
      check(G.asBlueNode(nn) == INVALID,"Wrong node conversion.");
      ++n;
    }
    check(n==INVALID,"Wrong red Node list linking.");
    check(countRedNodes(G)==cnt,"Wrong red Node number.");
  }

  template<class Graph>
  void checkGraphBlueNodeList(const Graph &G, int cnt)
  {
    typename Graph::BlueNodeIt n(G);
    for(int i=0;i<cnt;i++) {
      check(n!=INVALID,"Wrong blue Node list linking.");
      check(G.blue(n),"Wrong node set check.");
      check(!G.red(n),"Wrong node set check.");
      typename Graph::Node nn = n;
      check(G.asBlueNodeUnsafe(nn) == n,"Wrong node conversion.");
      check(G.asBlueNode(nn) == n,"Wrong node conversion.");
      check(G.asRedNode(nn) == INVALID,"Wrong node conversion.");
      ++n;
    }
    check(n==INVALID,"Wrong blue Node list linking.");
    check(countBlueNodes(G)==cnt,"Wrong blue Node number.");
  }

  template<class Graph>
  void checkGraphArcList(const Graph &G, int cnt)
  {
    typename Graph::ArcIt e(G);
    for(int i=0;i<cnt;i++) {
      check(e!=INVALID,"Wrong Arc list linking.");
      check(G.oppositeNode(G.source(e), e) == G.target(e),
            "Wrong opposite node");
      check(G.oppositeNode(G.target(e), e) == G.source(e),
            "Wrong opposite node");
      ++e;
    }
    check(e==INVALID,"Wrong Arc list linking.");
    check(countArcs(G)==cnt,"Wrong Arc number.");
  }

  template<class Graph>
  void checkGraphOutArcList(const Graph &G, typename Graph::Node n, int cnt)
  {
    typename Graph::OutArcIt e(G,n);
    for(int i=0;i<cnt;i++) {
      check(e!=INVALID,"Wrong OutArc list linking.");
      check(n==G.source(e),"Wrong OutArc list linking.");
      check(n==G.baseNode(e),"Wrong OutArc list linking.");
      check(G.target(e)==G.runningNode(e),"Wrong OutArc list linking.");
      ++e;
    }
    check(e==INVALID,"Wrong OutArc list linking.");
    check(countOutArcs(G,n)==cnt,"Wrong OutArc number.");
  }

  template<class Graph>
  void checkGraphInArcList(const Graph &G, typename Graph::Node n, int cnt)
  {
    typename Graph::InArcIt e(G,n);
    for(int i=0;i<cnt;i++) {
      check(e!=INVALID,"Wrong InArc list linking.");
      check(n==G.target(e),"Wrong InArc list linking.");
      check(n==G.baseNode(e),"Wrong OutArc list linking.");
      check(G.source(e)==G.runningNode(e),"Wrong OutArc list linking.");
      ++e;
    }
    check(e==INVALID,"Wrong InArc list linking.");
    check(countInArcs(G,n)==cnt,"Wrong InArc number.");
  }

  template<class Graph>
  void checkGraphEdgeList(const Graph &G, int cnt)
  {
    typename Graph::EdgeIt e(G);
    for(int i=0;i<cnt;i++) {
      check(e!=INVALID,"Wrong Edge list linking.");
      check(G.oppositeNode(G.u(e), e) == G.v(e), "Wrong opposite node");
      check(G.oppositeNode(G.v(e), e) == G.u(e), "Wrong opposite node");
      ++e;
    }
    check(e==INVALID,"Wrong Edge list linking.");
    check(countEdges(G)==cnt,"Wrong Edge number.");
  }

  template<class Graph>
  void checkGraphIncEdgeList(const Graph &G, typename Graph::Node n, int cnt)
  {
    typename Graph::IncEdgeIt e(G,n);
    for(int i=0;i<cnt;i++) {
      check(e!=INVALID,"Wrong IncEdge list linking.");
      check(n==G.u(e) || n==G.v(e),"Wrong IncEdge list linking.");
      check(n==G.baseNode(e),"Wrong OutArc list linking.");
      check(G.u(e)==G.runningNode(e) || G.v(e)==G.runningNode(e),
            "Wrong OutArc list linking.");
      ++e;
    }
    check(e==INVALID,"Wrong IncEdge list linking.");
    check(countIncEdges(G,n)==cnt,"Wrong IncEdge number.");
  }

  template <class Graph>
  void checkGraphIncEdgeArcLists(const Graph &G, typename Graph::Node n,
                                 int cnt)
  {
    checkGraphIncEdgeList(G, n, cnt);
    checkGraphOutArcList(G, n, cnt);
    checkGraphInArcList(G, n, cnt);
  }

  template <class Graph>
  void checkGraphConArcList(const Graph &G, int cnt) {
    int i = 0;
    for (typename Graph::NodeIt u(G); u != INVALID; ++u) {
      for (typename Graph::NodeIt v(G); v != INVALID; ++v) {
        for (ConArcIt<Graph> a(G, u, v); a != INVALID; ++a) {
          check(G.source(a) == u, "Wrong iterator.");
          check(G.target(a) == v, "Wrong iterator.");
          ++i;
        }
      }
    }
    check(cnt == i, "Wrong iterator.");
  }

  template <class Graph>
  void checkGraphConEdgeList(const Graph &G, int cnt) {
    int i = 0;
    for (typename Graph::NodeIt u(G); u != INVALID; ++u) {
      for (typename Graph::NodeIt v(G); v != INVALID; ++v) {
        for (ConEdgeIt<Graph> e(G, u, v); e != INVALID; ++e) {
          check((G.u(e) == u && G.v(e) == v) ||
                (G.u(e) == v && G.v(e) == u), "Wrong iterator.");
          i += u == v ? 2 : 1;
        }
      }
    }
    check(2 * cnt == i, "Wrong iterator.");
  }

  template <typename Graph>
  void checkArcDirections(const Graph& G) {
    for (typename Graph::ArcIt a(G); a != INVALID; ++a) {
      check(G.source(a) == G.target(G.oppositeArc(a)), "Wrong direction");
      check(G.target(a) == G.source(G.oppositeArc(a)), "Wrong direction");
      check(G.direct(a, G.direction(a)) == a, "Wrong direction");
    }
  }

  template <typename Graph>
  void checkNodeIds(const Graph& G) {
    typedef typename Graph::Node Node;
    std::set<int> values;
    for (typename Graph::NodeIt n(G); n != INVALID; ++n) {
      check(G.nodeFromId(G.id(n)) == n, "Wrong id");
      check(values.find(G.id(n)) == values.end(), "Wrong id");
      check(G.id(n) <= G.maxNodeId(), "Wrong maximum id");
      values.insert(G.id(n));
    }
    check(G.maxId(Node()) <= G.maxNodeId(), "Wrong maximum id");
  }

  template <typename Graph>
  void checkRedNodeIds(const Graph& G) {
    typedef typename Graph::RedNode RedNode;
    std::set<int> values;
    for (typename Graph::RedNodeIt n(G); n != INVALID; ++n) {
      check(G.red(n), "Wrong partition");
      check(values.find(G.id(n)) == values.end(), "Wrong id");
      check(G.id(n) <= G.maxRedId(), "Wrong maximum id");
      values.insert(G.id(n));
    }
    check(G.maxId(RedNode()) == G.maxRedId(), "Wrong maximum id");
  }

  template <typename Graph>
  void checkBlueNodeIds(const Graph& G) {
    typedef typename Graph::BlueNode BlueNode;
    std::set<int> values;
    for (typename Graph::BlueNodeIt n(G); n != INVALID; ++n) {
      check(G.blue(n), "Wrong partition");
      check(values.find(G.id(n)) == values.end(), "Wrong id");
      check(G.id(n) <= G.maxBlueId(), "Wrong maximum id");
      values.insert(G.id(n));
    }
    check(G.maxId(BlueNode()) == G.maxBlueId(), "Wrong maximum id");
  }

  template <typename Graph>
  void checkArcIds(const Graph& G) {
    typedef typename Graph::Arc Arc;
    std::set<int> values;
    for (typename Graph::ArcIt a(G); a != INVALID; ++a) {
      check(G.arcFromId(G.id(a)) == a, "Wrong id");
      check(values.find(G.id(a)) == values.end(), "Wrong id");
      check(G.id(a) <= G.maxArcId(), "Wrong maximum id");
      values.insert(G.id(a));
    }
    check(G.maxId(Arc()) <= G.maxArcId(), "Wrong maximum id");
  }

  template <typename Graph>
  void checkEdgeIds(const Graph& G) {
    typedef typename Graph::Edge Edge;
    std::set<int> values;
    for (typename Graph::EdgeIt e(G); e != INVALID; ++e) {
      check(G.edgeFromId(G.id(e)) == e, "Wrong id");
      check(values.find(G.id(e)) == values.end(), "Wrong id");
      check(G.id(e) <= G.maxEdgeId(), "Wrong maximum id");
      values.insert(G.id(e));
    }
    check(G.maxId(Edge()) <= G.maxEdgeId(), "Wrong maximum id");
  }

  template <typename Graph>
  void checkGraphNodeMap(const Graph& G) {
    typedef typename Graph::Node Node;
    typedef typename Graph::NodeIt NodeIt;

    typedef typename Graph::template NodeMap<int> IntNodeMap;
    IntNodeMap map(G, 42);
    for (NodeIt it(G); it != INVALID; ++it) {
      check(map[it] == 42, "Wrong map constructor.");
    }
    int s = 0;
    for (NodeIt it(G); it != INVALID; ++it) {
      map[it] = 0;
      check(map[it] == 0, "Wrong operator[].");
      map.set(it, s);
      check(map[it] == s, "Wrong set.");
      ++s;
    }
    s = s * (s - 1) / 2;
    for (NodeIt it(G); it != INVALID; ++it) {
      s -= map[it];
    }
    check(s == 0, "Wrong sum.");

    // map = constMap<Node>(12);
    // for (NodeIt it(G); it != INVALID; ++it) {
    //   check(map[it] == 12, "Wrong operator[].");
    // }
  }

  template <typename Graph>
  void checkGraphRedNodeMap(const Graph& G) {
    typedef typename Graph::Node Node;
    typedef typename Graph::RedNodeIt RedNodeIt;

    typedef typename Graph::template RedNodeMap<int> IntRedNodeMap;
    IntRedNodeMap map(G, 42);
    for (RedNodeIt it(G); it != INVALID; ++it) {
      check(map[it] == 42, "Wrong map constructor.");
    }
    int s = 0;
    for (RedNodeIt it(G); it != INVALID; ++it) {
      map[it] = 0;
      check(map[it] == 0, "Wrong operator[].");
      map.set(it, s);
      check(map[it] == s, "Wrong set.");
      ++s;
    }
    s = s * (s - 1) / 2;
    for (RedNodeIt it(G); it != INVALID; ++it) {
      s -= map[it];
    }
    check(s == 0, "Wrong sum.");

    // map = constMap<Node>(12);
    // for (NodeIt it(G); it != INVALID; ++it) {
    //   check(map[it] == 12, "Wrong operator[].");
    // }
  }

  template <typename Graph>
  void checkGraphBlueNodeMap(const Graph& G) {
    typedef typename Graph::Node Node;
    typedef typename Graph::BlueNodeIt BlueNodeIt;

    typedef typename Graph::template BlueNodeMap<int> IntBlueNodeMap;
    IntBlueNodeMap map(G, 42);
    for (BlueNodeIt it(G); it != INVALID; ++it) {
      check(map[it] == 42, "Wrong map constructor.");
    }
    int s = 0;
    for (BlueNodeIt it(G); it != INVALID; ++it) {
      map[it] = 0;
      check(map[it] == 0, "Wrong operator[].");
      map.set(it, s);
      check(map[it] == s, "Wrong set.");
      ++s;
    }
    s = s * (s - 1) / 2;
    for (BlueNodeIt it(G); it != INVALID; ++it) {
      s -= map[it];
    }
    check(s == 0, "Wrong sum.");

    // map = constMap<Node>(12);
    // for (NodeIt it(G); it != INVALID; ++it) {
    //   check(map[it] == 12, "Wrong operator[].");
    // }
  }

  template <typename Graph>
  void checkGraphArcMap(const Graph& G) {
    typedef typename Graph::Arc Arc;
    typedef typename Graph::ArcIt ArcIt;

    typedef typename Graph::template ArcMap<int> IntArcMap;
    IntArcMap map(G, 42);
    for (ArcIt it(G); it != INVALID; ++it) {
      check(map[it] == 42, "Wrong map constructor.");
    }
    int s = 0;
    for (ArcIt it(G); it != INVALID; ++it) {
      map[it] = 0;
      check(map[it] == 0, "Wrong operator[].");
      map.set(it, s);
      check(map[it] == s, "Wrong set.");
      ++s;
    }
    s = s * (s - 1) / 2;
    for (ArcIt it(G); it != INVALID; ++it) {
      s -= map[it];
    }
    check(s == 0, "Wrong sum.");

    // map = constMap<Arc>(12);
    // for (ArcIt it(G); it != INVALID; ++it) {
    //   check(map[it] == 12, "Wrong operator[].");
    // }
  }

  template <typename Graph>
  void checkGraphEdgeMap(const Graph& G) {
    typedef typename Graph::Edge Edge;
    typedef typename Graph::EdgeIt EdgeIt;

    typedef typename Graph::template EdgeMap<int> IntEdgeMap;
    IntEdgeMap map(G, 42);
    for (EdgeIt it(G); it != INVALID; ++it) {
      check(map[it] == 42, "Wrong map constructor.");
    }
    int s = 0;
    for (EdgeIt it(G); it != INVALID; ++it) {
      map[it] = 0;
      check(map[it] == 0, "Wrong operator[].");
      map.set(it, s);
      check(map[it] == s, "Wrong set.");
      ++s;
    }
    s = s * (s - 1) / 2;
    for (EdgeIt it(G); it != INVALID; ++it) {
      s -= map[it];
    }
    check(s == 0, "Wrong sum.");

    // map = constMap<Edge>(12);
    // for (EdgeIt it(G); it != INVALID; ++it) {
    //   check(map[it] == 12, "Wrong operator[].");
    // }
  }


} //namespace lemon

#endif
