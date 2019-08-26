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

#ifndef LEMON_GOMORY_HU_TREE_H
#define LEMON_GOMORY_HU_TREE_H

#include <limits>

#include <lemon/core.h>
#include <lemon/preflow.h>
#include <lemon/concept_check.h>
#include <lemon/concepts/maps.h>

/// \ingroup min_cut
/// \file
/// \brief Gomory-Hu cut tree in graphs.

namespace lemon {

  /// \ingroup min_cut
  ///
  /// \brief Gomory-Hu cut tree algorithm
  ///
  /// The Gomory-Hu tree is a tree on the node set of a given graph, but it
  /// may contain edges which are not in the original graph. It has the
  /// property that the minimum capacity edge of the path between two nodes
  /// in this tree has the same weight as the minimum cut in the graph
  /// between these nodes. Moreover the components obtained by removing
  /// this edge from the tree determine the corresponding minimum cut.
  /// Therefore once this tree is computed, the minimum cut between any pair
  /// of nodes can easily be obtained.
  ///
  /// The algorithm calculates \e n-1 distinct minimum cuts (currently with
  /// the \ref Preflow algorithm), thus it has \f$O(n^3\sqrt{m})\f$ overall
  /// time complexity. It calculates a rooted Gomory-Hu tree.
  /// The structure of the tree and the edge weights can be
  /// obtained using \c predNode(), \c predValue() and \c rootDist().
  /// The functions \c minCutMap() and \c minCutValue() calculate
  /// the minimum cut and the minimum cut value between any two nodes
  /// in the graph. You can also list (iterate on) the nodes and the
  /// edges of the cuts using \c MinCutNodeIt and \c MinCutEdgeIt.
  ///
  /// \tparam GR The type of the undirected graph the algorithm runs on.
  /// \tparam CAP The type of the edge map containing the capacities.
  /// The default map type is \ref concepts::Graph::EdgeMap "GR::EdgeMap<int>".
#ifdef DOXYGEN
  template <typename GR,
            typename CAP>
#else
  template <typename GR,
            typename CAP = typename GR::template EdgeMap<int> >
#endif
  class GomoryHu {
  public:

    /// The graph type of the algorithm
    typedef GR Graph;
    /// The capacity map type of the algorithm
    typedef CAP Capacity;
    /// The value type of capacities
    typedef typename Capacity::Value Value;

  private:

    TEMPLATE_GRAPH_TYPEDEFS(Graph);

    const Graph& _graph;
    const Capacity& _capacity;

    Node _root;
    typename Graph::template NodeMap<Node>* _pred;
    typename Graph::template NodeMap<Value>* _weight;
    typename Graph::template NodeMap<int>* _order;

    void createStructures() {
      if (!_pred) {
        _pred = new typename Graph::template NodeMap<Node>(_graph);
      }
      if (!_weight) {
        _weight = new typename Graph::template NodeMap<Value>(_graph);
      }
      if (!_order) {
        _order = new typename Graph::template NodeMap<int>(_graph);
      }
    }

    void destroyStructures() {
      if (_pred) {
        delete _pred;
      }
      if (_weight) {
        delete _weight;
      }
      if (_order) {
        delete _order;
      }
    }

  public:

    /// \brief Constructor
    ///
    /// Constructor.
    /// \param graph The undirected graph the algorithm runs on.
    /// \param capacity The edge capacity map.
    GomoryHu(const Graph& graph, const Capacity& capacity)
      : _graph(graph), _capacity(capacity),
        _pred(0), _weight(0), _order(0)
    {
      checkConcept<concepts::ReadMap<Edge, Value>, Capacity>();
    }


    /// \brief Destructor
    ///
    /// Destructor.
    ~GomoryHu() {
      destroyStructures();
    }

  private:

    // Initialize the internal data structures
    void init() {
      createStructures();

      _root = NodeIt(_graph);
      for (NodeIt n(_graph); n != INVALID; ++n) {
        (*_pred)[n] = _root;
        (*_order)[n] = -1;
      }
      (*_pred)[_root] = INVALID;
      (*_weight)[_root] = std::numeric_limits<Value>::max();
    }


    // Start the algorithm
    void start() {
      Preflow<Graph, Capacity> fa(_graph, _capacity, _root, INVALID);

      for (NodeIt n(_graph); n != INVALID; ++n) {
        if (n == _root) continue;

        Node pn = (*_pred)[n];
        fa.source(n);
        fa.target(pn);

        fa.runMinCut();

        (*_weight)[n] = fa.flowValue();

        for (NodeIt nn(_graph); nn != INVALID; ++nn) {
          if (nn != n && fa.minCut(nn) && (*_pred)[nn] == pn) {
            (*_pred)[nn] = n;
          }
        }
        if ((*_pred)[pn] != INVALID && fa.minCut((*_pred)[pn])) {
          (*_pred)[n] = (*_pred)[pn];
          (*_pred)[pn] = n;
          (*_weight)[n] = (*_weight)[pn];
          (*_weight)[pn] = fa.flowValue();
        }
      }

      (*_order)[_root] = 0;
      int index = 1;

      for (NodeIt n(_graph); n != INVALID; ++n) {
        std::vector<Node> st;
        Node nn = n;
        while ((*_order)[nn] == -1) {
          st.push_back(nn);
          nn = (*_pred)[nn];
        }
        while (!st.empty()) {
          (*_order)[st.back()] = index++;
          st.pop_back();
        }
      }
    }

  public:

    ///\name Execution Control

    ///@{

    /// \brief Run the Gomory-Hu algorithm.
    ///
    /// This function runs the Gomory-Hu algorithm.
    void run() {
      init();
      start();
    }

    /// @}

    ///\name Query Functions
    ///The results of the algorithm can be obtained using these
    ///functions.\n
    ///\ref run() should be called before using them.\n
    ///See also \ref MinCutNodeIt and \ref MinCutEdgeIt.

    ///@{

    /// \brief Return the predecessor node in the Gomory-Hu tree.
    ///
    /// This function returns the predecessor node of the given node
    /// in the Gomory-Hu tree.
    /// If \c node is the root of the tree, then it returns \c INVALID.
    ///
    /// \pre \ref run() must be called before using this function.
    Node predNode(const Node& node) const {
      return (*_pred)[node];
    }

    /// \brief Return the weight of the predecessor edge in the
    /// Gomory-Hu tree.
    ///
    /// This function returns the weight of the predecessor edge of the
    /// given node in the Gomory-Hu tree.
    /// If \c node is the root of the tree, the result is undefined.
    ///
    /// \pre \ref run() must be called before using this function.
    Value predValue(const Node& node) const {
      return (*_weight)[node];
    }

    /// \brief Return the distance from the root node in the Gomory-Hu tree.
    ///
    /// This function returns the distance of the given node from the root
    /// node in the Gomory-Hu tree.
    ///
    /// \pre \ref run() must be called before using this function.
    int rootDist(const Node& node) const {
      return (*_order)[node];
    }

    /// \brief Return the minimum cut value between two nodes
    ///
    /// This function returns the minimum cut value between the nodes
    /// \c s and \c t.
    /// It finds the nearest common ancestor of the given nodes in the
    /// Gomory-Hu tree and calculates the minimum weight edge on the
    /// paths to the ancestor.
    ///
    /// \pre \ref run() must be called before using this function.
    Value minCutValue(const Node& s, const Node& t) const {
      Node sn = s, tn = t;
      Value value = std::numeric_limits<Value>::max();

      while (sn != tn) {
        if ((*_order)[sn] < (*_order)[tn]) {
          if ((*_weight)[tn] <= value) value = (*_weight)[tn];
          tn = (*_pred)[tn];
        } else {
          if ((*_weight)[sn] <= value) value = (*_weight)[sn];
          sn = (*_pred)[sn];
        }
      }
      return value;
    }

    /// \brief Return the minimum cut between two nodes
    ///
    /// This function returns the minimum cut between the nodes \c s and \c t
    /// in the \c cutMap parameter by setting the nodes in the component of
    /// \c s to \c true and the other nodes to \c false.
    ///
    /// For higher level interfaces see MinCutNodeIt and MinCutEdgeIt.
    ///
    /// \param s The base node.
    /// \param t The node you want to separate from node \c s.
    /// \param cutMap The cut will be returned in this map.
    /// It must be a \c bool (or convertible) \ref concepts::ReadWriteMap
    /// "ReadWriteMap" on the graph nodes.
    ///
    /// \return The value of the minimum cut between \c s and \c t.
    ///
    /// \pre \ref run() must be called before using this function.
    template <typename CutMap>
    Value minCutMap(const Node& s,
                    const Node& t,
                    CutMap& cutMap
                    ) const {
      Node sn = s, tn = t;
      bool s_root=false;
      Node rn = INVALID;
      Value value = std::numeric_limits<Value>::max();

      while (sn != tn) {
        if ((*_order)[sn] < (*_order)[tn]) {
          if ((*_weight)[tn] <= value) {
            rn = tn;
            s_root = false;
            value = (*_weight)[tn];
          }
          tn = (*_pred)[tn];
        } else {
          if ((*_weight)[sn] <= value) {
            rn = sn;
            s_root = true;
            value = (*_weight)[sn];
          }
          sn = (*_pred)[sn];
        }
      }

      typename Graph::template NodeMap<bool> reached(_graph, false);
      reached[_root] = true;
      cutMap.set(_root, !s_root);
      reached[rn] = true;
      cutMap.set(rn, s_root);

      std::vector<Node> st;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        st.clear();
        Node nn = n;
        while (!reached[nn]) {
          st.push_back(nn);
          nn = (*_pred)[nn];
        }
        while (!st.empty()) {
          cutMap.set(st.back(), cutMap[nn]);
          st.pop_back();
        }
      }

      return value;
    }

    ///@}

    friend class MinCutNodeIt;

    /// Iterate on the nodes of a minimum cut

    /// This iterator class lists the nodes of a minimum cut found by
    /// GomoryHu. Before using it, you must allocate a GomoryHu class
    /// and call its \ref GomoryHu::run() "run()" method.
    ///
    /// This example counts the nodes in the minimum cut separating \c s from
    /// \c t.
    /// \code
    /// GomoryHu<Graph> gom(g, capacities);
    /// gom.run();
    /// int cnt=0;
    /// for(GomoryHu<Graph>::MinCutNodeIt n(gom,s,t); n!=INVALID; ++n) ++cnt;
    /// \endcode
    class MinCutNodeIt
    {
      bool _side;
      typename Graph::NodeIt _node_it;
      typename Graph::template NodeMap<bool> _cut;
    public:
      /// Constructor

      /// Constructor.
      ///
      MinCutNodeIt(GomoryHu const &gomory,
                   ///< The GomoryHu class. You must call its
                   ///  run() method
                   ///  before initializing this iterator.
                   const Node& s, ///< The base node.
                   const Node& t,
                   ///< The node you want to separate from node \c s.
                   bool side=true
                   ///< If it is \c true (default) then the iterator lists
                   ///  the nodes of the component containing \c s,
                   ///  otherwise it lists the other component.
                   /// \note As the minimum cut is not always unique,
                   /// \code
                   /// MinCutNodeIt(gomory, s, t, true);
                   /// \endcode
                   /// and
                   /// \code
                   /// MinCutNodeIt(gomory, t, s, false);
                   /// \endcode
                   /// does not necessarily give the same set of nodes.
                   /// However, it is ensured that
                   /// \code
                   /// MinCutNodeIt(gomory, s, t, true);
                   /// \endcode
                   /// and
                   /// \code
                   /// MinCutNodeIt(gomory, s, t, false);
                   /// \endcode
                   /// together list each node exactly once.
                   )
        : _side(side), _cut(gomory._graph)
      {
        gomory.minCutMap(s,t,_cut);
        for(_node_it=typename Graph::NodeIt(gomory._graph);
            _node_it!=INVALID && _cut[_node_it]!=_side;
            ++_node_it) {}
      }
      /// Conversion to \c Node

      /// Conversion to \c Node.
      ///
      operator typename Graph::Node() const
      {
        return _node_it;
      }
      bool operator==(Invalid) { return _node_it==INVALID; }
      bool operator!=(Invalid) { return _node_it!=INVALID; }
      /// Next node

      /// Next node.
      ///
      MinCutNodeIt &operator++()
      {
        for(++_node_it;_node_it!=INVALID&&_cut[_node_it]!=_side;++_node_it) {}
        return *this;
      }
      /// Postfix incrementation

      /// Postfix incrementation.
      ///
      /// \warning This incrementation
      /// returns a \c Node, not a \c MinCutNodeIt, as one may
      /// expect.
      typename Graph::Node operator++(int)
      {
        typename Graph::Node n=*this;
        ++(*this);
        return n;
      }
    };

    friend class MinCutEdgeIt;

    /// Iterate on the edges of a minimum cut

    /// This iterator class lists the edges of a minimum cut found by
    /// GomoryHu. Before using it, you must allocate a GomoryHu class
    /// and call its \ref GomoryHu::run() "run()" method.
    ///
    /// This example computes the value of the minimum cut separating \c s from
    /// \c t.
    /// \code
    /// GomoryHu<Graph> gom(g, capacities);
    /// gom.run();
    /// int value=0;
    /// for(GomoryHu<Graph>::MinCutEdgeIt e(gom,s,t); e!=INVALID; ++e)
    ///   value+=capacities[e];
    /// \endcode
    /// The result will be the same as the value returned by
    /// \ref GomoryHu::minCutValue() "gom.minCutValue(s,t)".
    class MinCutEdgeIt
    {
      bool _side;
      const Graph &_graph;
      typename Graph::NodeIt _node_it;
      typename Graph::OutArcIt _arc_it;
      typename Graph::template NodeMap<bool> _cut;
      void step()
      {
        ++_arc_it;
        while(_node_it!=INVALID && _arc_it==INVALID)
          {
            for(++_node_it;_node_it!=INVALID&&!_cut[_node_it];++_node_it) {}
            if(_node_it!=INVALID)
              _arc_it=typename Graph::OutArcIt(_graph,_node_it);
          }
      }

    public:
      /// Constructor

      /// Constructor.
      ///
      MinCutEdgeIt(GomoryHu const &gomory,
                   ///< The GomoryHu class. You must call its
                   ///  run() method
                   ///  before initializing this iterator.
                   const Node& s,  ///< The base node.
                   const Node& t,
                   ///< The node you want to separate from node \c s.
                   bool side=true
                   ///< If it is \c true (default) then the listed arcs
                   ///  will be oriented from the
                   ///  nodes of the component containing \c s,
                   ///  otherwise they will be oriented in the opposite
                   ///  direction.
                   )
        : _graph(gomory._graph), _cut(_graph)
      {
        gomory.minCutMap(s,t,_cut);
        if(!side)
          for(typename Graph::NodeIt n(_graph);n!=INVALID;++n)
            _cut[n]=!_cut[n];

        for(_node_it=typename Graph::NodeIt(_graph);
            _node_it!=INVALID && !_cut[_node_it];
            ++_node_it) {}
        _arc_it = _node_it!=INVALID ?
          typename Graph::OutArcIt(_graph,_node_it) : INVALID;
        while(_node_it!=INVALID && _arc_it == INVALID)
          {
            for(++_node_it; _node_it!=INVALID&&!_cut[_node_it]; ++_node_it) {}
            if(_node_it!=INVALID)
              _arc_it= typename Graph::OutArcIt(_graph,_node_it);
          }
        while(_arc_it!=INVALID && _cut[_graph.target(_arc_it)]) step();
      }
      /// Conversion to \c Arc

      /// Conversion to \c Arc.
      ///
      operator typename Graph::Arc() const
      {
        return _arc_it;
      }
      /// Conversion to \c Edge

      /// Conversion to \c Edge.
      ///
      operator typename Graph::Edge() const
      {
        return _arc_it;
      }
      bool operator==(Invalid) { return _node_it==INVALID; }
      bool operator!=(Invalid) { return _node_it!=INVALID; }
      /// Next edge

      /// Next edge.
      ///
      MinCutEdgeIt &operator++()
      {
        step();
        while(_arc_it!=INVALID && _cut[_graph.target(_arc_it)]) step();
        return *this;
      }
      /// Postfix incrementation

      /// Postfix incrementation.
      ///
      /// \warning This incrementation
      /// returns an \c Arc, not a \c MinCutEdgeIt, as one may expect.
      typename Graph::Arc operator++(int)
      {
        typename Graph::Arc e=*this;
        ++(*this);
        return e;
      }
    };

  };

}

#endif
