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

#ifndef LEMON_CONNECTIVITY_H
#define LEMON_CONNECTIVITY_H

#include <lemon/dfs.h>
#include <lemon/bfs.h>
#include <lemon/core.h>
#include <lemon/maps.h>
#include <lemon/adaptors.h>

#include <lemon/concepts/digraph.h>
#include <lemon/concepts/graph.h>
#include <lemon/concept_check.h>

#include <stack>
#include <functional>

/// \ingroup graph_properties
/// \file
/// \brief Connectivity algorithms
///
/// Connectivity algorithms

namespace lemon {

  /// \ingroup graph_properties
  ///
  /// \brief Check whether an undirected graph is connected.
  ///
  /// This function checks whether the given undirected graph is connected,
  /// i.e. there is a path between any two nodes in the graph.
  ///
  /// \return \c true if the graph is connected.
  /// \note By definition, the empty graph is connected.
  ///
  /// \see countConnectedComponents(), connectedComponents()
  /// \see stronglyConnected()
  template <typename Graph>
  bool connected(const Graph& graph) {
    checkConcept<concepts::Graph, Graph>();
    typedef typename Graph::NodeIt NodeIt;
    if (NodeIt(graph) == INVALID) return true;
    Dfs<Graph> dfs(graph);
    dfs.run(NodeIt(graph));
    for (NodeIt it(graph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        return false;
      }
    }
    return true;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Count the number of connected components of an undirected graph
  ///
  /// This function counts the number of connected components of the given
  /// undirected graph.
  ///
  /// The connected components are the classes of an equivalence relation
  /// on the nodes of an undirected graph. Two nodes are in the same class
  /// if they are connected with a path.
  ///
  /// \return The number of connected components.
  /// \note By definition, the empty graph consists
  /// of zero connected components.
  ///
  /// \see connected(), connectedComponents()
  template <typename Graph>
  int countConnectedComponents(const Graph &graph) {
    checkConcept<concepts::Graph, Graph>();
    typedef typename Graph::Node Node;
    typedef typename Graph::Arc Arc;

    typedef NullMap<Node, Arc> PredMap;
    typedef NullMap<Node, int> DistMap;

    int compNum = 0;
    typename Bfs<Graph>::
      template SetPredMap<PredMap>::
      template SetDistMap<DistMap>::
      Create bfs(graph);

    PredMap predMap;
    bfs.predMap(predMap);

    DistMap distMap;
    bfs.distMap(distMap);

    bfs.init();
    for(typename Graph::NodeIt n(graph); n != INVALID; ++n) {
      if (!bfs.reached(n)) {
        bfs.addSource(n);
        bfs.start();
        ++compNum;
      }
    }
    return compNum;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Find the connected components of an undirected graph
  ///
  /// This function finds the connected components of the given undirected
  /// graph.
  ///
  /// The connected components are the classes of an equivalence relation
  /// on the nodes of an undirected graph. Two nodes are in the same class
  /// if they are connected with a path.
  ///
  /// \image html connected_components.png
  /// \image latex connected_components.eps "Connected components" width=\textwidth
  ///
  /// \param graph The undirected graph.
  /// \retval compMap A writable node map. The values will be set from 0 to
  /// the number of the connected components minus one. Each value of the map
  /// will be set exactly once, and the values of a certain component will be
  /// set continuously.
  /// \return The number of connected components.
  /// \note By definition, the empty graph consists
  /// of zero connected components.
  ///
  /// \see connected(), countConnectedComponents()
  template <class Graph, class NodeMap>
  int connectedComponents(const Graph &graph, NodeMap &compMap) {
    checkConcept<concepts::Graph, Graph>();
    typedef typename Graph::Node Node;
    typedef typename Graph::Arc Arc;
    checkConcept<concepts::WriteMap<Node, int>, NodeMap>();

    typedef NullMap<Node, Arc> PredMap;
    typedef NullMap<Node, int> DistMap;

    int compNum = 0;
    typename Bfs<Graph>::
      template SetPredMap<PredMap>::
      template SetDistMap<DistMap>::
      Create bfs(graph);

    PredMap predMap;
    bfs.predMap(predMap);

    DistMap distMap;
    bfs.distMap(distMap);

    bfs.init();
    for(typename Graph::NodeIt n(graph); n != INVALID; ++n) {
      if(!bfs.reached(n)) {
        bfs.addSource(n);
        while (!bfs.emptyQueue()) {
          compMap.set(bfs.nextNode(), compNum);
          bfs.processNextNode();
        }
        ++compNum;
      }
    }
    return compNum;
  }

  namespace _connectivity_bits {

    template <typename Digraph, typename Iterator >
    struct LeaveOrderVisitor : public DfsVisitor<Digraph> {
    public:
      typedef typename Digraph::Node Node;
      LeaveOrderVisitor(Iterator it) : _it(it) {}

      void leave(const Node& node) {
        *(_it++) = node;
      }

    private:
      Iterator _it;
    };

    template <typename Digraph, typename Map>
    struct FillMapVisitor : public DfsVisitor<Digraph> {
    public:
      typedef typename Digraph::Node Node;
      typedef typename Map::Value Value;

      FillMapVisitor(Map& map, Value& value)
        : _map(map), _value(value) {}

      void reach(const Node& node) {
        _map.set(node, _value);
      }
    private:
      Map& _map;
      Value& _value;
    };

    template <typename Digraph, typename ArcMap>
    struct StronglyConnectedCutArcsVisitor : public DfsVisitor<Digraph> {
    public:
      typedef typename Digraph::Node Node;
      typedef typename Digraph::Arc Arc;

      StronglyConnectedCutArcsVisitor(const Digraph& digraph,
                                      ArcMap& cutMap,
                                      int& cutNum)
        : _digraph(digraph), _cutMap(cutMap), _cutNum(cutNum),
          _compMap(digraph, -1), _num(-1) {
      }

      void start(const Node&) {
        ++_num;
      }

      void reach(const Node& node) {
        _compMap.set(node, _num);
      }

      void examine(const Arc& arc) {
         if (_compMap[_digraph.source(arc)] !=
             _compMap[_digraph.target(arc)]) {
           _cutMap.set(arc, true);
           ++_cutNum;
         }
      }
    private:
      const Digraph& _digraph;
      ArcMap& _cutMap;
      int& _cutNum;

      typename Digraph::template NodeMap<int> _compMap;
      int _num;
    };

  }


  /// \ingroup graph_properties
  ///
  /// \brief Check whether a directed graph is strongly connected.
  ///
  /// This function checks whether the given directed graph is strongly
  /// connected, i.e. any two nodes of the digraph are
  /// connected with directed paths in both direction.
  ///
  /// \return \c true if the digraph is strongly connected.
  /// \note By definition, the empty digraph is strongly connected.
  ///
  /// \see countStronglyConnectedComponents(), stronglyConnectedComponents()
  /// \see connected()
  template <typename Digraph>
  bool stronglyConnected(const Digraph& digraph) {
    checkConcept<concepts::Digraph, Digraph>();

    typedef typename Digraph::Node Node;
    typedef typename Digraph::NodeIt NodeIt;

    typename Digraph::Node source = NodeIt(digraph);
    if (source == INVALID) return true;

    using namespace _connectivity_bits;

    typedef DfsVisitor<Digraph> Visitor;
    Visitor visitor;

    DfsVisit<Digraph, Visitor> dfs(digraph, visitor);
    dfs.init();
    dfs.addSource(source);
    dfs.start();

    for (NodeIt it(digraph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        return false;
      }
    }

    typedef ReverseDigraph<const Digraph> RDigraph;
    typedef typename RDigraph::NodeIt RNodeIt;
    RDigraph rdigraph(digraph);

    typedef DfsVisitor<RDigraph> RVisitor;
    RVisitor rvisitor;

    DfsVisit<RDigraph, RVisitor> rdfs(rdigraph, rvisitor);
    rdfs.init();
    rdfs.addSource(source);
    rdfs.start();

    for (RNodeIt it(rdigraph); it != INVALID; ++it) {
      if (!rdfs.reached(it)) {
        return false;
      }
    }

    return true;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Count the number of strongly connected components of a
  /// directed graph
  ///
  /// This function counts the number of strongly connected components of
  /// the given directed graph.
  ///
  /// The strongly connected components are the classes of an
  /// equivalence relation on the nodes of a digraph. Two nodes are in
  /// the same class if they are connected with directed paths in both
  /// direction.
  ///
  /// \return The number of strongly connected components.
  /// \note By definition, the empty digraph has zero
  /// strongly connected components.
  ///
  /// \see stronglyConnected(), stronglyConnectedComponents()
  template <typename Digraph>
  int countStronglyConnectedComponents(const Digraph& digraph) {
    checkConcept<concepts::Digraph, Digraph>();

    using namespace _connectivity_bits;

    typedef typename Digraph::Node Node;
    typedef typename Digraph::Arc Arc;
    typedef typename Digraph::NodeIt NodeIt;
    typedef typename Digraph::ArcIt ArcIt;

    typedef std::vector<Node> Container;
    typedef typename Container::iterator Iterator;

    Container nodes(countNodes(digraph));
    typedef LeaveOrderVisitor<Digraph, Iterator> Visitor;
    Visitor visitor(nodes.begin());

    DfsVisit<Digraph, Visitor> dfs(digraph, visitor);
    dfs.init();
    for (NodeIt it(digraph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        dfs.start();
      }
    }

    typedef typename Container::reverse_iterator RIterator;
    typedef ReverseDigraph<const Digraph> RDigraph;

    RDigraph rdigraph(digraph);

    typedef DfsVisitor<Digraph> RVisitor;
    RVisitor rvisitor;

    DfsVisit<RDigraph, RVisitor> rdfs(rdigraph, rvisitor);

    int compNum = 0;

    rdfs.init();
    for (RIterator it = nodes.rbegin(); it != nodes.rend(); ++it) {
      if (!rdfs.reached(*it)) {
        rdfs.addSource(*it);
        rdfs.start();
        ++compNum;
      }
    }
    return compNum;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Find the strongly connected components of a directed graph
  ///
  /// This function finds the strongly connected components of the given
  /// directed graph. In addition, the numbering of the components will
  /// satisfy that there is no arc going from a higher numbered component
  /// to a lower one (i.e. it provides a topological order of the components).
  ///
  /// The strongly connected components are the classes of an
  /// equivalence relation on the nodes of a digraph. Two nodes are in
  /// the same class if they are connected with directed paths in both
  /// direction.
  ///
  /// \image html strongly_connected_components.png
  /// \image latex strongly_connected_components.eps "Strongly connected components" width=\textwidth
  ///
  /// \param digraph The digraph.
  /// \retval compMap A writable node map. The values will be set from 0 to
  /// the number of the strongly connected components minus one. Each value
  /// of the map will be set exactly once, and the values of a certain
  /// component will be set continuously.
  /// \return The number of strongly connected components.
  /// \note By definition, the empty digraph has zero
  /// strongly connected components.
  ///
  /// \see stronglyConnected(), countStronglyConnectedComponents()
  template <typename Digraph, typename NodeMap>
  int stronglyConnectedComponents(const Digraph& digraph, NodeMap& compMap) {
    checkConcept<concepts::Digraph, Digraph>();
    typedef typename Digraph::Node Node;
    typedef typename Digraph::NodeIt NodeIt;
    checkConcept<concepts::WriteMap<Node, int>, NodeMap>();

    using namespace _connectivity_bits;

    typedef std::vector<Node> Container;
    typedef typename Container::iterator Iterator;

    Container nodes(countNodes(digraph));
    typedef LeaveOrderVisitor<Digraph, Iterator> Visitor;
    Visitor visitor(nodes.begin());

    DfsVisit<Digraph, Visitor> dfs(digraph, visitor);
    dfs.init();
    for (NodeIt it(digraph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        dfs.start();
      }
    }

    typedef typename Container::reverse_iterator RIterator;
    typedef ReverseDigraph<const Digraph> RDigraph;

    RDigraph rdigraph(digraph);

    int compNum = 0;

    typedef FillMapVisitor<RDigraph, NodeMap> RVisitor;
    RVisitor rvisitor(compMap, compNum);

    DfsVisit<RDigraph, RVisitor> rdfs(rdigraph, rvisitor);

    rdfs.init();
    for (RIterator it = nodes.rbegin(); it != nodes.rend(); ++it) {
      if (!rdfs.reached(*it)) {
        rdfs.addSource(*it);
        rdfs.start();
        ++compNum;
      }
    }
    return compNum;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Find the cut arcs of the strongly connected components.
  ///
  /// This function finds the cut arcs of the strongly connected components
  /// of the given digraph.
  ///
  /// The strongly connected components are the classes of an
  /// equivalence relation on the nodes of a digraph. Two nodes are in
  /// the same class if they are connected with directed paths in both
  /// direction.
  /// The strongly connected components are separated by the cut arcs.
  ///
  /// \param digraph The digraph.
  /// \retval cutMap A writable arc map. The values will be set to \c true
  /// for the cut arcs (exactly once for each cut arc), and will not be
  /// changed for other arcs.
  /// \return The number of cut arcs.
  ///
  /// \see stronglyConnected(), stronglyConnectedComponents()
  template <typename Digraph, typename ArcMap>
  int stronglyConnectedCutArcs(const Digraph& digraph, ArcMap& cutMap) {
    checkConcept<concepts::Digraph, Digraph>();
    typedef typename Digraph::Node Node;
    typedef typename Digraph::Arc Arc;
    typedef typename Digraph::NodeIt NodeIt;
    checkConcept<concepts::WriteMap<Arc, bool>, ArcMap>();

    using namespace _connectivity_bits;

    typedef std::vector<Node> Container;
    typedef typename Container::iterator Iterator;

    Container nodes(countNodes(digraph));
    typedef LeaveOrderVisitor<Digraph, Iterator> Visitor;
    Visitor visitor(nodes.begin());

    DfsVisit<Digraph, Visitor> dfs(digraph, visitor);
    dfs.init();
    for (NodeIt it(digraph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        dfs.start();
      }
    }

    typedef typename Container::reverse_iterator RIterator;
    typedef ReverseDigraph<const Digraph> RDigraph;

    RDigraph rdigraph(digraph);

    int cutNum = 0;

    typedef StronglyConnectedCutArcsVisitor<RDigraph, ArcMap> RVisitor;
    RVisitor rvisitor(rdigraph, cutMap, cutNum);

    DfsVisit<RDigraph, RVisitor> rdfs(rdigraph, rvisitor);

    rdfs.init();
    for (RIterator it = nodes.rbegin(); it != nodes.rend(); ++it) {
      if (!rdfs.reached(*it)) {
        rdfs.addSource(*it);
        rdfs.start();
      }
    }
    return cutNum;
  }

  namespace _connectivity_bits {

    template <typename Digraph>
    class CountBiNodeConnectedComponentsVisitor : public DfsVisitor<Digraph> {
    public:
      typedef typename Digraph::Node Node;
      typedef typename Digraph::Arc Arc;
      typedef typename Digraph::Edge Edge;

      CountBiNodeConnectedComponentsVisitor(const Digraph& graph, int &compNum)
        : _graph(graph), _compNum(compNum),
          _numMap(graph), _retMap(graph), _predMap(graph), _num(0) {}

      void start(const Node& node) {
        _predMap.set(node, INVALID);
      }

      void reach(const Node& node) {
        _numMap.set(node, _num);
        _retMap.set(node, _num);
        ++_num;
      }

      void discover(const Arc& edge) {
        _predMap.set(_graph.target(edge), _graph.source(edge));
      }

      void examine(const Arc& edge) {
        if (_graph.source(edge) == _graph.target(edge) &&
            _graph.direction(edge)) {
          ++_compNum;
          return;
        }
        if (_predMap[_graph.source(edge)] == _graph.target(edge)) {
          return;
        }
        if (_retMap[_graph.source(edge)] > _numMap[_graph.target(edge)]) {
          _retMap.set(_graph.source(edge), _numMap[_graph.target(edge)]);
        }
      }

      void backtrack(const Arc& edge) {
        if (_retMap[_graph.source(edge)] > _retMap[_graph.target(edge)]) {
          _retMap.set(_graph.source(edge), _retMap[_graph.target(edge)]);
        }
        if (_numMap[_graph.source(edge)] <= _retMap[_graph.target(edge)]) {
          ++_compNum;
        }
      }

    private:
      const Digraph& _graph;
      int& _compNum;

      typename Digraph::template NodeMap<int> _numMap;
      typename Digraph::template NodeMap<int> _retMap;
      typename Digraph::template NodeMap<Node> _predMap;
      int _num;
    };

    template <typename Digraph, typename ArcMap>
    class BiNodeConnectedComponentsVisitor : public DfsVisitor<Digraph> {
    public:
      typedef typename Digraph::Node Node;
      typedef typename Digraph::Arc Arc;
      typedef typename Digraph::Edge Edge;

      BiNodeConnectedComponentsVisitor(const Digraph& graph,
                                       ArcMap& compMap, int &compNum)
        : _graph(graph), _compMap(compMap), _compNum(compNum),
          _numMap(graph), _retMap(graph), _predMap(graph), _num(0) {}

      void start(const Node& node) {
        _predMap.set(node, INVALID);
      }

      void reach(const Node& node) {
        _numMap.set(node, _num);
        _retMap.set(node, _num);
        ++_num;
      }

      void discover(const Arc& edge) {
        Node target = _graph.target(edge);
        _predMap.set(target, edge);
        _edgeStack.push(edge);
      }

      void examine(const Arc& edge) {
        Node source = _graph.source(edge);
        Node target = _graph.target(edge);
        if (source == target && _graph.direction(edge)) {
          _compMap.set(edge, _compNum);
          ++_compNum;
          return;
        }
        if (_numMap[target] < _numMap[source]) {
          if (_predMap[source] != _graph.oppositeArc(edge)) {
            _edgeStack.push(edge);
          }
        }
        if (_predMap[source] != INVALID &&
            target == _graph.source(_predMap[source])) {
          return;
        }
        if (_retMap[source] > _numMap[target]) {
          _retMap.set(source, _numMap[target]);
        }
      }

      void backtrack(const Arc& edge) {
        Node source = _graph.source(edge);
        Node target = _graph.target(edge);
        if (_retMap[source] > _retMap[target]) {
          _retMap.set(source, _retMap[target]);
        }
        if (_numMap[source] <= _retMap[target]) {
          while (_edgeStack.top() != edge) {
            _compMap.set(_edgeStack.top(), _compNum);
            _edgeStack.pop();
          }
          _compMap.set(edge, _compNum);
          _edgeStack.pop();
          ++_compNum;
        }
      }

    private:
      const Digraph& _graph;
      ArcMap& _compMap;
      int& _compNum;

      typename Digraph::template NodeMap<int> _numMap;
      typename Digraph::template NodeMap<int> _retMap;
      typename Digraph::template NodeMap<Arc> _predMap;
      std::stack<Edge> _edgeStack;
      int _num;
    };


    template <typename Digraph, typename NodeMap>
    class BiNodeConnectedCutNodesVisitor : public DfsVisitor<Digraph> {
    public:
      typedef typename Digraph::Node Node;
      typedef typename Digraph::Arc Arc;
      typedef typename Digraph::Edge Edge;

      BiNodeConnectedCutNodesVisitor(const Digraph& graph, NodeMap& cutMap,
                                     int& cutNum)
        : _graph(graph), _cutMap(cutMap), _cutNum(cutNum),
          _numMap(graph), _retMap(graph), _predMap(graph), _num(0) {}

      void start(const Node& node) {
        _predMap.set(node, INVALID);
        rootCut = false;
      }

      void reach(const Node& node) {
        _numMap.set(node, _num);
        _retMap.set(node, _num);
        ++_num;
      }

      void discover(const Arc& edge) {
        _predMap.set(_graph.target(edge), _graph.source(edge));
      }

      void examine(const Arc& edge) {
        if (_graph.source(edge) == _graph.target(edge) &&
            _graph.direction(edge)) {
          if (!_cutMap[_graph.source(edge)]) {
            _cutMap.set(_graph.source(edge), true);
            ++_cutNum;
          }
          return;
        }
        if (_predMap[_graph.source(edge)] == _graph.target(edge)) return;
        if (_retMap[_graph.source(edge)] > _numMap[_graph.target(edge)]) {
          _retMap.set(_graph.source(edge), _numMap[_graph.target(edge)]);
        }
      }

      void backtrack(const Arc& edge) {
        if (_retMap[_graph.source(edge)] > _retMap[_graph.target(edge)]) {
          _retMap.set(_graph.source(edge), _retMap[_graph.target(edge)]);
        }
        if (_numMap[_graph.source(edge)] <= _retMap[_graph.target(edge)]) {
          if (_predMap[_graph.source(edge)] != INVALID) {
            if (!_cutMap[_graph.source(edge)]) {
              _cutMap.set(_graph.source(edge), true);
              ++_cutNum;
            }
          } else if (rootCut) {
            if (!_cutMap[_graph.source(edge)]) {
              _cutMap.set(_graph.source(edge), true);
              ++_cutNum;
            }
          } else {
            rootCut = true;
          }
        }
      }

    private:
      const Digraph& _graph;
      NodeMap& _cutMap;
      int& _cutNum;

      typename Digraph::template NodeMap<int> _numMap;
      typename Digraph::template NodeMap<int> _retMap;
      typename Digraph::template NodeMap<Node> _predMap;
      std::stack<Edge> _edgeStack;
      int _num;
      bool rootCut;
    };

  }

  template <typename Graph>
  int countBiNodeConnectedComponents(const Graph& graph);

  /// \ingroup graph_properties
  ///
  /// \brief Check whether an undirected graph is bi-node-connected.
  ///
  /// This function checks whether the given undirected graph is
  /// bi-node-connected, i.e. a connected graph without articulation
  /// node.
  ///
  /// \return \c true if the graph bi-node-connected.
  ///
  /// \note By definition,
  /// \li a graph consisting of zero or one node is bi-node-connected,
  /// \li a graph consisting of two isolated nodes
  /// is \e not bi-node-connected and
  /// \li a graph consisting of two nodes connected by an edge
  /// is bi-node-connected.
  ///
  /// \see countBiNodeConnectedComponents(), biNodeConnectedComponents()
  template <typename Graph>
  bool biNodeConnected(const Graph& graph) {
    bool hasNonIsolated = false, hasIsolated = false;
    for (typename Graph::NodeIt n(graph); n != INVALID; ++n) {
      if (typename Graph::OutArcIt(graph, n) == INVALID) {
        if (hasIsolated || hasNonIsolated) {
          return false;
        } else {
          hasIsolated = true;
        }
      } else {
        if (hasIsolated) {
          return false;
        } else {
          hasNonIsolated = true;
        }
      }
    }
    return countBiNodeConnectedComponents(graph) <= 1;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Count the number of bi-node-connected components of an
  /// undirected graph.
  ///
  /// This function counts the number of bi-node-connected components of
  /// the given undirected graph.
  ///
  /// The bi-node-connected components are the classes of an equivalence
  /// relation on the edges of a undirected graph. Two edges are in the
  /// same class if they are on same circle.
  ///
  /// \return The number of bi-node-connected components.
  ///
  /// \see biNodeConnected(), biNodeConnectedComponents()
  template <typename Graph>
  int countBiNodeConnectedComponents(const Graph& graph) {
    checkConcept<concepts::Graph, Graph>();
    typedef typename Graph::NodeIt NodeIt;

    using namespace _connectivity_bits;

    typedef CountBiNodeConnectedComponentsVisitor<Graph> Visitor;

    int compNum = 0;
    Visitor visitor(graph, compNum);

    DfsVisit<Graph, Visitor> dfs(graph, visitor);
    dfs.init();

    for (NodeIt it(graph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        dfs.start();
      }
    }
    return compNum;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Find the bi-node-connected components of an undirected graph.
  ///
  /// This function finds the bi-node-connected components of the given
  /// undirected graph.
  ///
  /// The bi-node-connected components are the classes of an equivalence
  /// relation on the edges of a undirected graph. Two edges are in the
  /// same class if they are on same circle.
  ///
  /// \image html node_biconnected_components.png
  /// \image latex node_biconnected_components.eps "bi-node-connected components" width=\textwidth
  ///
  /// \param graph The undirected graph.
  /// \retval compMap A writable edge map. The values will be set from 0
  /// to the number of the bi-node-connected components minus one. Each
  /// value of the map will be set exactly once, and the values of a
  /// certain component will be set continuously.
  /// \return The number of bi-node-connected components.
  ///
  /// \see biNodeConnected(), countBiNodeConnectedComponents()
  template <typename Graph, typename EdgeMap>
  int biNodeConnectedComponents(const Graph& graph,
                                EdgeMap& compMap) {
    checkConcept<concepts::Graph, Graph>();
    typedef typename Graph::NodeIt NodeIt;
    typedef typename Graph::Edge Edge;
    checkConcept<concepts::WriteMap<Edge, int>, EdgeMap>();

    using namespace _connectivity_bits;

    typedef BiNodeConnectedComponentsVisitor<Graph, EdgeMap> Visitor;

    int compNum = 0;
    Visitor visitor(graph, compMap, compNum);

    DfsVisit<Graph, Visitor> dfs(graph, visitor);
    dfs.init();

    for (NodeIt it(graph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        dfs.start();
      }
    }
    return compNum;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Find the bi-node-connected cut nodes in an undirected graph.
  ///
  /// This function finds the bi-node-connected cut nodes in the given
  /// undirected graph.
  ///
  /// The bi-node-connected components are the classes of an equivalence
  /// relation on the edges of a undirected graph. Two edges are in the
  /// same class if they are on same circle.
  /// The bi-node-connected components are separted by the cut nodes of
  /// the components.
  ///
  /// \param graph The undirected graph.
  /// \retval cutMap A writable node map. The values will be set to
  /// \c true for the nodes that separate two or more components
  /// (exactly once for each cut node), and will not be changed for
  /// other nodes.
  /// \return The number of the cut nodes.
  ///
  /// \see biNodeConnected(), biNodeConnectedComponents()
  template <typename Graph, typename NodeMap>
  int biNodeConnectedCutNodes(const Graph& graph, NodeMap& cutMap) {
    checkConcept<concepts::Graph, Graph>();
    typedef typename Graph::Node Node;
    typedef typename Graph::NodeIt NodeIt;
    checkConcept<concepts::WriteMap<Node, bool>, NodeMap>();

    using namespace _connectivity_bits;

    typedef BiNodeConnectedCutNodesVisitor<Graph, NodeMap> Visitor;

    int cutNum = 0;
    Visitor visitor(graph, cutMap, cutNum);

    DfsVisit<Graph, Visitor> dfs(graph, visitor);
    dfs.init();

    for (NodeIt it(graph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        dfs.start();
      }
    }
    return cutNum;
  }

  namespace _connectivity_bits {

    template <typename Digraph>
    class CountBiEdgeConnectedComponentsVisitor : public DfsVisitor<Digraph> {
    public:
      typedef typename Digraph::Node Node;
      typedef typename Digraph::Arc Arc;
      typedef typename Digraph::Edge Edge;

      CountBiEdgeConnectedComponentsVisitor(const Digraph& graph, int &compNum)
        : _graph(graph), _compNum(compNum),
          _numMap(graph), _retMap(graph), _predMap(graph), _num(0) {}

      void start(const Node& node) {
        _predMap.set(node, INVALID);
      }

      void reach(const Node& node) {
        _numMap.set(node, _num);
        _retMap.set(node, _num);
        ++_num;
      }

      void leave(const Node& node) {
        if (_numMap[node] <= _retMap[node]) {
          ++_compNum;
        }
      }

      void discover(const Arc& edge) {
        _predMap.set(_graph.target(edge), edge);
      }

      void examine(const Arc& edge) {
        if (_predMap[_graph.source(edge)] == _graph.oppositeArc(edge)) {
          return;
        }
        if (_retMap[_graph.source(edge)] > _retMap[_graph.target(edge)]) {
          _retMap.set(_graph.source(edge), _retMap[_graph.target(edge)]);
        }
      }

      void backtrack(const Arc& edge) {
        if (_retMap[_graph.source(edge)] > _retMap[_graph.target(edge)]) {
          _retMap.set(_graph.source(edge), _retMap[_graph.target(edge)]);
        }
      }

    private:
      const Digraph& _graph;
      int& _compNum;

      typename Digraph::template NodeMap<int> _numMap;
      typename Digraph::template NodeMap<int> _retMap;
      typename Digraph::template NodeMap<Arc> _predMap;
      int _num;
    };

    template <typename Digraph, typename NodeMap>
    class BiEdgeConnectedComponentsVisitor : public DfsVisitor<Digraph> {
    public:
      typedef typename Digraph::Node Node;
      typedef typename Digraph::Arc Arc;
      typedef typename Digraph::Edge Edge;

      BiEdgeConnectedComponentsVisitor(const Digraph& graph,
                                       NodeMap& compMap, int &compNum)
        : _graph(graph), _compMap(compMap), _compNum(compNum),
          _numMap(graph), _retMap(graph), _predMap(graph), _num(0) {}

      void start(const Node& node) {
        _predMap.set(node, INVALID);
      }

      void reach(const Node& node) {
        _numMap.set(node, _num);
        _retMap.set(node, _num);
        _nodeStack.push(node);
        ++_num;
      }

      void leave(const Node& node) {
        if (_numMap[node] <= _retMap[node]) {
          while (_nodeStack.top() != node) {
            _compMap.set(_nodeStack.top(), _compNum);
            _nodeStack.pop();
          }
          _compMap.set(node, _compNum);
          _nodeStack.pop();
          ++_compNum;
        }
      }

      void discover(const Arc& edge) {
        _predMap.set(_graph.target(edge), edge);
      }

      void examine(const Arc& edge) {
        if (_predMap[_graph.source(edge)] == _graph.oppositeArc(edge)) {
          return;
        }
        if (_retMap[_graph.source(edge)] > _retMap[_graph.target(edge)]) {
          _retMap.set(_graph.source(edge), _retMap[_graph.target(edge)]);
        }
      }

      void backtrack(const Arc& edge) {
        if (_retMap[_graph.source(edge)] > _retMap[_graph.target(edge)]) {
          _retMap.set(_graph.source(edge), _retMap[_graph.target(edge)]);
        }
      }

    private:
      const Digraph& _graph;
      NodeMap& _compMap;
      int& _compNum;

      typename Digraph::template NodeMap<int> _numMap;
      typename Digraph::template NodeMap<int> _retMap;
      typename Digraph::template NodeMap<Arc> _predMap;
      std::stack<Node> _nodeStack;
      int _num;
    };


    template <typename Digraph, typename ArcMap>
    class BiEdgeConnectedCutEdgesVisitor : public DfsVisitor<Digraph> {
    public:
      typedef typename Digraph::Node Node;
      typedef typename Digraph::Arc Arc;
      typedef typename Digraph::Edge Edge;

      BiEdgeConnectedCutEdgesVisitor(const Digraph& graph,
                                     ArcMap& cutMap, int &cutNum)
        : _graph(graph), _cutMap(cutMap), _cutNum(cutNum),
          _numMap(graph), _retMap(graph), _predMap(graph), _num(0) {}

      void start(const Node& node) {
        _predMap[node] = INVALID;
      }

      void reach(const Node& node) {
        _numMap.set(node, _num);
        _retMap.set(node, _num);
        ++_num;
      }

      void leave(const Node& node) {
        if (_numMap[node] <= _retMap[node]) {
          if (_predMap[node] != INVALID) {
            _cutMap.set(_predMap[node], true);
            ++_cutNum;
          }
        }
      }

      void discover(const Arc& edge) {
        _predMap.set(_graph.target(edge), edge);
      }

      void examine(const Arc& edge) {
        if (_predMap[_graph.source(edge)] == _graph.oppositeArc(edge)) {
          return;
        }
        if (_retMap[_graph.source(edge)] > _retMap[_graph.target(edge)]) {
          _retMap.set(_graph.source(edge), _retMap[_graph.target(edge)]);
        }
      }

      void backtrack(const Arc& edge) {
        if (_retMap[_graph.source(edge)] > _retMap[_graph.target(edge)]) {
          _retMap.set(_graph.source(edge), _retMap[_graph.target(edge)]);
        }
      }

    private:
      const Digraph& _graph;
      ArcMap& _cutMap;
      int& _cutNum;

      typename Digraph::template NodeMap<int> _numMap;
      typename Digraph::template NodeMap<int> _retMap;
      typename Digraph::template NodeMap<Arc> _predMap;
      int _num;
    };
  }

  template <typename Graph>
  int countBiEdgeConnectedComponents(const Graph& graph);

  /// \ingroup graph_properties
  ///
  /// \brief Check whether an undirected graph is bi-edge-connected.
  ///
  /// This function checks whether the given undirected graph is
  /// bi-edge-connected, i.e. any two nodes are connected with at least
  /// two edge-disjoint paths.
  ///
  /// \return \c true if the graph is bi-edge-connected.
  /// \note By definition, the empty graph is bi-edge-connected.
  ///
  /// \see countBiEdgeConnectedComponents(), biEdgeConnectedComponents()
  template <typename Graph>
  bool biEdgeConnected(const Graph& graph) {
    return countBiEdgeConnectedComponents(graph) <= 1;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Count the number of bi-edge-connected components of an
  /// undirected graph.
  ///
  /// This function counts the number of bi-edge-connected components of
  /// the given undirected graph.
  ///
  /// The bi-edge-connected components are the classes of an equivalence
  /// relation on the nodes of an undirected graph. Two nodes are in the
  /// same class if they are connected with at least two edge-disjoint
  /// paths.
  ///
  /// \return The number of bi-edge-connected components.
  ///
  /// \see biEdgeConnected(), biEdgeConnectedComponents()
  template <typename Graph>
  int countBiEdgeConnectedComponents(const Graph& graph) {
    checkConcept<concepts::Graph, Graph>();
    typedef typename Graph::NodeIt NodeIt;

    using namespace _connectivity_bits;

    typedef CountBiEdgeConnectedComponentsVisitor<Graph> Visitor;

    int compNum = 0;
    Visitor visitor(graph, compNum);

    DfsVisit<Graph, Visitor> dfs(graph, visitor);
    dfs.init();

    for (NodeIt it(graph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        dfs.start();
      }
    }
    return compNum;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Find the bi-edge-connected components of an undirected graph.
  ///
  /// This function finds the bi-edge-connected components of the given
  /// undirected graph.
  ///
  /// The bi-edge-connected components are the classes of an equivalence
  /// relation on the nodes of an undirected graph. Two nodes are in the
  /// same class if they are connected with at least two edge-disjoint
  /// paths.
  ///
  /// \image html edge_biconnected_components.png
  /// \image latex edge_biconnected_components.eps "bi-edge-connected components" width=\textwidth
  ///
  /// \param graph The undirected graph.
  /// \retval compMap A writable node map. The values will be set from 0 to
  /// the number of the bi-edge-connected components minus one. Each value
  /// of the map will be set exactly once, and the values of a certain
  /// component will be set continuously.
  /// \return The number of bi-edge-connected components.
  ///
  /// \see biEdgeConnected(), countBiEdgeConnectedComponents()
  template <typename Graph, typename NodeMap>
  int biEdgeConnectedComponents(const Graph& graph, NodeMap& compMap) {
    checkConcept<concepts::Graph, Graph>();
    typedef typename Graph::NodeIt NodeIt;
    typedef typename Graph::Node Node;
    checkConcept<concepts::WriteMap<Node, int>, NodeMap>();

    using namespace _connectivity_bits;

    typedef BiEdgeConnectedComponentsVisitor<Graph, NodeMap> Visitor;

    int compNum = 0;
    Visitor visitor(graph, compMap, compNum);

    DfsVisit<Graph, Visitor> dfs(graph, visitor);
    dfs.init();

    for (NodeIt it(graph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        dfs.start();
      }
    }
    return compNum;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Find the bi-edge-connected cut edges in an undirected graph.
  ///
  /// This function finds the bi-edge-connected cut edges in the given
  /// undirected graph.
  ///
  /// The bi-edge-connected components are the classes of an equivalence
  /// relation on the nodes of an undirected graph. Two nodes are in the
  /// same class if they are connected with at least two edge-disjoint
  /// paths.
  /// The bi-edge-connected components are separted by the cut edges of
  /// the components.
  ///
  /// \param graph The undirected graph.
  /// \retval cutMap A writable edge map. The values will be set to \c true
  /// for the cut edges (exactly once for each cut edge), and will not be
  /// changed for other edges.
  /// \return The number of cut edges.
  ///
  /// \see biEdgeConnected(), biEdgeConnectedComponents()
  template <typename Graph, typename EdgeMap>
  int biEdgeConnectedCutEdges(const Graph& graph, EdgeMap& cutMap) {
    checkConcept<concepts::Graph, Graph>();
    typedef typename Graph::NodeIt NodeIt;
    typedef typename Graph::Edge Edge;
    checkConcept<concepts::WriteMap<Edge, bool>, EdgeMap>();

    using namespace _connectivity_bits;

    typedef BiEdgeConnectedCutEdgesVisitor<Graph, EdgeMap> Visitor;

    int cutNum = 0;
    Visitor visitor(graph, cutMap, cutNum);

    DfsVisit<Graph, Visitor> dfs(graph, visitor);
    dfs.init();

    for (NodeIt it(graph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        dfs.start();
      }
    }
    return cutNum;
  }


  namespace _connectivity_bits {

    template <typename Digraph, typename IntNodeMap>
    class TopologicalSortVisitor : public DfsVisitor<Digraph> {
    public:
      typedef typename Digraph::Node Node;
      typedef typename Digraph::Arc edge;

      TopologicalSortVisitor(IntNodeMap& order, int num)
        : _order(order), _num(num) {}

      void leave(const Node& node) {
        _order.set(node, --_num);
      }

    private:
      IntNodeMap& _order;
      int _num;
    };

  }

  /// \ingroup graph_properties
  ///
  /// \brief Check whether a digraph is DAG.
  ///
  /// This function checks whether the given digraph is DAG, i.e.
  /// \e Directed \e Acyclic \e Graph.
  /// \return \c true if there is no directed cycle in the digraph.
  /// \see acyclic()
  template <typename Digraph>
  bool dag(const Digraph& digraph) {

    checkConcept<concepts::Digraph, Digraph>();

    typedef typename Digraph::Node Node;
    typedef typename Digraph::NodeIt NodeIt;
    typedef typename Digraph::Arc Arc;

    typedef typename Digraph::template NodeMap<bool> ProcessedMap;

    typename Dfs<Digraph>::template SetProcessedMap<ProcessedMap>::
      Create dfs(digraph);

    ProcessedMap processed(digraph);
    dfs.processedMap(processed);

    dfs.init();
    for (NodeIt it(digraph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        while (!dfs.emptyQueue()) {
          Arc arc = dfs.nextArc();
          Node target = digraph.target(arc);
          if (dfs.reached(target) && !processed[target]) {
            return false;
          }
          dfs.processNextArc();
        }
      }
    }
    return true;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Sort the nodes of a DAG into topolgical order.
  ///
  /// This function sorts the nodes of the given acyclic digraph (DAG)
  /// into topolgical order.
  ///
  /// \param digraph The digraph, which must be DAG.
  /// \retval order A writable node map. The values will be set from 0 to
  /// the number of the nodes in the digraph minus one. Each value of the
  /// map will be set exactly once, and the values will be set descending
  /// order.
  ///
  /// \see dag(), checkedTopologicalSort()
  template <typename Digraph, typename NodeMap>
  void topologicalSort(const Digraph& digraph, NodeMap& order) {
    using namespace _connectivity_bits;

    checkConcept<concepts::Digraph, Digraph>();
    checkConcept<concepts::WriteMap<typename Digraph::Node, int>, NodeMap>();

    typedef typename Digraph::Node Node;
    typedef typename Digraph::NodeIt NodeIt;
    typedef typename Digraph::Arc Arc;

    TopologicalSortVisitor<Digraph, NodeMap>
      visitor(order, countNodes(digraph));

    DfsVisit<Digraph, TopologicalSortVisitor<Digraph, NodeMap> >
      dfs(digraph, visitor);

    dfs.init();
    for (NodeIt it(digraph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        dfs.start();
      }
    }
  }

  /// \ingroup graph_properties
  ///
  /// \brief Sort the nodes of a DAG into topolgical order.
  ///
  /// This function sorts the nodes of the given acyclic digraph (DAG)
  /// into topolgical order and also checks whether the given digraph
  /// is DAG.
  ///
  /// \param digraph The digraph.
  /// \retval order A readable and writable node map. The values will be
  /// set from 0 to the number of the nodes in the digraph minus one.
  /// Each value of the map will be set exactly once, and the values will
  /// be set descending order.
  /// \return \c false if the digraph is not DAG.
  ///
  /// \see dag(), topologicalSort()
  template <typename Digraph, typename NodeMap>
  bool checkedTopologicalSort(const Digraph& digraph, NodeMap& order) {
    using namespace _connectivity_bits;

    checkConcept<concepts::Digraph, Digraph>();
    checkConcept<concepts::ReadWriteMap<typename Digraph::Node, int>,
      NodeMap>();

    typedef typename Digraph::Node Node;
    typedef typename Digraph::NodeIt NodeIt;
    typedef typename Digraph::Arc Arc;

    for (NodeIt it(digraph); it != INVALID; ++it) {
      order.set(it, -1);
    }

    TopologicalSortVisitor<Digraph, NodeMap>
      visitor(order, countNodes(digraph));

    DfsVisit<Digraph, TopologicalSortVisitor<Digraph, NodeMap> >
      dfs(digraph, visitor);

    dfs.init();
    for (NodeIt it(digraph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        while (!dfs.emptyQueue()) {
           Arc arc = dfs.nextArc();
           Node target = digraph.target(arc);
           if (dfs.reached(target) && order[target] == -1) {
             return false;
           }
           dfs.processNextArc();
         }
      }
    }
    return true;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Check whether an undirected graph is acyclic.
  ///
  /// This function checks whether the given undirected graph is acyclic.
  /// \return \c true if there is no cycle in the graph.
  /// \see dag()
  template <typename Graph>
  bool acyclic(const Graph& graph) {
    checkConcept<concepts::Graph, Graph>();
    typedef typename Graph::Node Node;
    typedef typename Graph::NodeIt NodeIt;
    typedef typename Graph::Arc Arc;
    Dfs<Graph> dfs(graph);
    dfs.init();
    for (NodeIt it(graph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        dfs.addSource(it);
        while (!dfs.emptyQueue()) {
          Arc arc = dfs.nextArc();
          Node source = graph.source(arc);
          Node target = graph.target(arc);
          if (dfs.reached(target) &&
              dfs.predArc(source) != graph.oppositeArc(arc)) {
            return false;
          }
          dfs.processNextArc();
        }
      }
    }
    return true;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Check whether an undirected graph is tree.
  ///
  /// This function checks whether the given undirected graph is tree.
  /// \return \c true if the graph is acyclic and connected.
  /// \see acyclic(), connected()
  template <typename Graph>
  bool tree(const Graph& graph) {
    checkConcept<concepts::Graph, Graph>();
    typedef typename Graph::Node Node;
    typedef typename Graph::NodeIt NodeIt;
    typedef typename Graph::Arc Arc;
    if (NodeIt(graph) == INVALID) return true;
    Dfs<Graph> dfs(graph);
    dfs.init();
    dfs.addSource(NodeIt(graph));
    while (!dfs.emptyQueue()) {
      Arc arc = dfs.nextArc();
      Node source = graph.source(arc);
      Node target = graph.target(arc);
      if (dfs.reached(target) &&
          dfs.predArc(source) != graph.oppositeArc(arc)) {
        return false;
      }
      dfs.processNextArc();
    }
    for (NodeIt it(graph); it != INVALID; ++it) {
      if (!dfs.reached(it)) {
        return false;
      }
    }
    return true;
  }

  namespace _connectivity_bits {

    template <typename Digraph>
    class BipartiteVisitor : public BfsVisitor<Digraph> {
    public:
      typedef typename Digraph::Arc Arc;
      typedef typename Digraph::Node Node;

      BipartiteVisitor(const Digraph& graph, bool& bipartite)
        : _graph(graph), _part(graph), _bipartite(bipartite) {}

      void start(const Node& node) {
        _part[node] = true;
      }
      void discover(const Arc& edge) {
        _part.set(_graph.target(edge), !_part[_graph.source(edge)]);
      }
      void examine(const Arc& edge) {
        _bipartite = _bipartite &&
          _part[_graph.target(edge)] != _part[_graph.source(edge)];
      }

    private:

      const Digraph& _graph;
      typename Digraph::template NodeMap<bool> _part;
      bool& _bipartite;
    };

    template <typename Digraph, typename PartMap>
    class BipartitePartitionsVisitor : public BfsVisitor<Digraph> {
    public:
      typedef typename Digraph::Arc Arc;
      typedef typename Digraph::Node Node;

      BipartitePartitionsVisitor(const Digraph& graph,
                                 PartMap& part, bool& bipartite)
        : _graph(graph), _part(part), _bipartite(bipartite) {}

      void start(const Node& node) {
        _part.set(node, true);
      }
      void discover(const Arc& edge) {
        _part.set(_graph.target(edge), !_part[_graph.source(edge)]);
      }
      void examine(const Arc& edge) {
        _bipartite = _bipartite &&
          _part[_graph.target(edge)] != _part[_graph.source(edge)];
      }

    private:

      const Digraph& _graph;
      PartMap& _part;
      bool& _bipartite;
    };
  }

  /// \ingroup graph_properties
  ///
  /// \brief Check whether an undirected graph is bipartite.
  ///
  /// The function checks whether the given undirected graph is bipartite.
  /// \return \c true if the graph is bipartite.
  ///
  /// \see bipartitePartitions()
  template<typename Graph>
  bool bipartite(const Graph &graph){
    using namespace _connectivity_bits;

    checkConcept<concepts::Graph, Graph>();

    typedef typename Graph::NodeIt NodeIt;
    typedef typename Graph::ArcIt ArcIt;

    bool bipartite = true;

    BipartiteVisitor<Graph>
      visitor(graph, bipartite);
    BfsVisit<Graph, BipartiteVisitor<Graph> >
      bfs(graph, visitor);
    bfs.init();
    for(NodeIt it(graph); it != INVALID; ++it) {
      if(!bfs.reached(it)){
        bfs.addSource(it);
        while (!bfs.emptyQueue()) {
          bfs.processNextNode();
          if (!bipartite) return false;
        }
      }
    }
    return true;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Find the bipartite partitions of an undirected graph.
  ///
  /// This function checks whether the given undirected graph is bipartite
  /// and gives back the bipartite partitions.
  ///
  /// \image html bipartite_partitions.png
  /// \image latex bipartite_partitions.eps "Bipartite partititions" width=\textwidth
  ///
  /// \param graph The undirected graph.
  /// \retval partMap A writable node map of \c bool (or convertible) value
  /// type. The values will be set to \c true for one component and
  /// \c false for the other one.
  /// \return \c true if the graph is bipartite, \c false otherwise.
  ///
  /// \see bipartite()
  template<typename Graph, typename NodeMap>
  bool bipartitePartitions(const Graph &graph, NodeMap &partMap){
    using namespace _connectivity_bits;

    checkConcept<concepts::Graph, Graph>();
    checkConcept<concepts::WriteMap<typename Graph::Node, bool>, NodeMap>();

    typedef typename Graph::Node Node;
    typedef typename Graph::NodeIt NodeIt;
    typedef typename Graph::ArcIt ArcIt;

    bool bipartite = true;

    BipartitePartitionsVisitor<Graph, NodeMap>
      visitor(graph, partMap, bipartite);
    BfsVisit<Graph, BipartitePartitionsVisitor<Graph, NodeMap> >
      bfs(graph, visitor);
    bfs.init();
    for(NodeIt it(graph); it != INVALID; ++it) {
      if(!bfs.reached(it)){
        bfs.addSource(it);
        while (!bfs.emptyQueue()) {
          bfs.processNextNode();
          if (!bipartite) return false;
        }
      }
    }
    return true;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Check whether the given graph contains no loop arcs/edges.
  ///
  /// This function returns \c true if there are no loop arcs/edges in
  /// the given graph. It works for both directed and undirected graphs.
  template <typename Graph>
  bool loopFree(const Graph& graph) {
    for (typename Graph::ArcIt it(graph); it != INVALID; ++it) {
      if (graph.source(it) == graph.target(it)) return false;
    }
    return true;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Check whether the given graph contains no parallel arcs/edges.
  ///
  /// This function returns \c true if there are no parallel arcs/edges in
  /// the given graph. It works for both directed and undirected graphs.
  template <typename Graph>
  bool parallelFree(const Graph& graph) {
    typename Graph::template NodeMap<int> reached(graph, 0);
    int cnt = 1;
    for (typename Graph::NodeIt n(graph); n != INVALID; ++n) {
      for (typename Graph::OutArcIt a(graph, n); a != INVALID; ++a) {
        if (reached[graph.target(a)] == cnt) return false;
        reached[graph.target(a)] = cnt;
      }
      ++cnt;
    }
    return true;
  }

  /// \ingroup graph_properties
  ///
  /// \brief Check whether the given graph is simple.
  ///
  /// This function returns \c true if the given graph is simple, i.e.
  /// it contains no loop arcs/edges and no parallel arcs/edges.
  /// The function works for both directed and undirected graphs.
  /// \see loopFree(), parallelFree()
  template <typename Graph>
  bool simpleGraph(const Graph& graph) {
    typename Graph::template NodeMap<int> reached(graph, 0);
    int cnt = 1;
    for (typename Graph::NodeIt n(graph); n != INVALID; ++n) {
      reached[n] = cnt;
      for (typename Graph::OutArcIt a(graph, n); a != INVALID; ++a) {
        if (reached[graph.target(a)] == cnt) return false;
        reached[graph.target(a)] = cnt;
      }
      ++cnt;
    }
    return true;
  }

} //namespace lemon

#endif //LEMON_CONNECTIVITY_H
