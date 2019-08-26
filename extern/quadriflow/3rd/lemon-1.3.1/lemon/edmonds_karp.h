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

#ifndef LEMON_EDMONDS_KARP_H
#define LEMON_EDMONDS_KARP_H

/// \file
/// \ingroup max_flow
/// \brief Implementation of the Edmonds-Karp algorithm.

#include <lemon/tolerance.h>
#include <vector>

namespace lemon {

  /// \brief Default traits class of EdmondsKarp class.
  ///
  /// Default traits class of EdmondsKarp class.
  /// \param GR Digraph type.
  /// \param CAP Type of capacity map.
  template <typename GR, typename CAP>
  struct EdmondsKarpDefaultTraits {

    /// \brief The digraph type the algorithm runs on.
    typedef GR Digraph;

    /// \brief The type of the map that stores the arc capacities.
    ///
    /// The type of the map that stores the arc capacities.
    /// It must meet the \ref concepts::ReadMap "ReadMap" concept.
    typedef CAP CapacityMap;

    /// \brief The type of the flow values.
    typedef typename CapacityMap::Value Value;

    /// \brief The type of the map that stores the flow values.
    ///
    /// The type of the map that stores the flow values.
    /// It must meet the \ref concepts::ReadWriteMap "ReadWriteMap" concept.
#ifdef DOXYGEN
    typedef GR::ArcMap<Value> FlowMap;
#else
    typedef typename Digraph::template ArcMap<Value> FlowMap;
#endif

    /// \brief Instantiates a FlowMap.
    ///
    /// This function instantiates a \ref FlowMap.
    /// \param digraph The digraph for which we would like to define
    /// the flow map.
    static FlowMap* createFlowMap(const Digraph& digraph) {
      return new FlowMap(digraph);
    }

    /// \brief The tolerance used by the algorithm
    ///
    /// The tolerance used by the algorithm to handle inexact computation.
    typedef lemon::Tolerance<Value> Tolerance;

  };

  /// \ingroup max_flow
  ///
  /// \brief Edmonds-Karp algorithms class.
  ///
  /// This class provides an implementation of the \e Edmonds-Karp \e
  /// algorithm producing a \ref max_flow "flow of maximum value" in a
  /// digraph \cite clrs01algorithms, \cite amo93networkflows,
  /// \cite edmondskarp72theoretical.
  /// The Edmonds-Karp algorithm is slower than the Preflow
  /// algorithm, but it has an advantage of the step-by-step execution
  /// control with feasible flow solutions. The \e source node, the \e
  /// target node, the \e capacity of the arcs and the \e starting \e
  /// flow value of the arcs should be passed to the algorithm
  /// through the constructor.
  ///
  /// The time complexity of the algorithm is \f$ O(nm^2) \f$ in
  /// worst case. Always try the Preflow algorithm instead of this if
  /// you just want to compute the optimal flow.
  ///
  /// \tparam GR The type of the digraph the algorithm runs on.
  /// \tparam CAP The type of the capacity map. The default map
  /// type is \ref concepts::Digraph::ArcMap "GR::ArcMap<int>".
  /// \tparam TR The traits class that defines various types used by the
  /// algorithm. By default, it is \ref EdmondsKarpDefaultTraits
  /// "EdmondsKarpDefaultTraits<GR, CAP>".
  /// In most cases, this parameter should not be set directly,
  /// consider to use the named template parameters instead.

#ifdef DOXYGEN
  template <typename GR, typename CAP, typename TR>
#else
  template <typename GR,
            typename CAP = typename GR::template ArcMap<int>,
            typename TR = EdmondsKarpDefaultTraits<GR, CAP> >
#endif
  class EdmondsKarp {
  public:

    /// \brief The \ref lemon::EdmondsKarpDefaultTraits "traits class"
    /// of the algorithm.
    typedef TR Traits;
    /// The type of the digraph the algorithm runs on.
    typedef typename Traits::Digraph Digraph;
    /// The type of the capacity map.
    typedef typename Traits::CapacityMap CapacityMap;
    /// The type of the flow values.
    typedef typename Traits::Value Value;

    /// The type of the flow map.
    typedef typename Traits::FlowMap FlowMap;
    /// The type of the tolerance.
    typedef typename Traits::Tolerance Tolerance;

  private:

    TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);
    typedef typename Digraph::template NodeMap<Arc> PredMap;

    const Digraph& _graph;
    const CapacityMap* _capacity;

    Node _source, _target;

    FlowMap* _flow;
    bool _local_flow;

    PredMap* _pred;
    std::vector<Node> _queue;

    Tolerance _tolerance;
    Value _flow_value;

    void createStructures() {
      if (!_flow) {
        _flow = Traits::createFlowMap(_graph);
        _local_flow = true;
      }
      if (!_pred) {
        _pred = new PredMap(_graph);
      }
      _queue.resize(countNodes(_graph));
    }

    void destroyStructures() {
      if (_local_flow) {
        delete _flow;
      }
      if (_pred) {
        delete _pred;
      }
    }

  public:

    typedef EdmondsKarp Create;

    ///\name Named template parameters

    ///@{

    template <typename T>
    struct SetFlowMapTraits : public Traits {
      typedef T FlowMap;
      static FlowMap *createFlowMap(const Digraph&) {
        LEMON_ASSERT(false, "FlowMap is not initialized");
        return 0;
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// FlowMap type
    ///
    /// \ref named-templ-param "Named parameter" for setting FlowMap
    /// type
    template <typename T>
    struct SetFlowMap
      : public EdmondsKarp<Digraph, CapacityMap, SetFlowMapTraits<T> > {
      typedef EdmondsKarp<Digraph, CapacityMap, SetFlowMapTraits<T> > Create;
    };

    /// @}

  protected:

    EdmondsKarp() {}

  public:

    /// \brief The constructor of the class.
    ///
    /// The constructor of the class.
    /// \param digraph The digraph the algorithm runs on.
    /// \param capacity The capacity of the arcs.
    /// \param source The source node.
    /// \param target The target node.
    EdmondsKarp(const Digraph& digraph, const CapacityMap& capacity,
                Node source, Node target)
      : _graph(digraph), _capacity(&capacity), _source(source), _target(target),
        _flow(0), _local_flow(false), _pred(0), _tolerance(), _flow_value()
    {
      LEMON_ASSERT(_source != _target,
                   "Flow source and target are the same nodes.");
    }

    /// \brief Destructor.
    ///
    /// Destructor.
    ~EdmondsKarp() {
      destroyStructures();
    }

    /// \brief Sets the capacity map.
    ///
    /// Sets the capacity map.
    /// \return <tt>(*this)</tt>
    EdmondsKarp& capacityMap(const CapacityMap& map) {
      _capacity = &map;
      return *this;
    }

    /// \brief Sets the flow map.
    ///
    /// Sets the flow map.
    /// If you don't use this function before calling \ref run() or
    /// \ref init(), an instance will be allocated automatically.
    /// The destructor deallocates this automatically allocated map,
    /// of course.
    /// \return <tt>(*this)</tt>
    EdmondsKarp& flowMap(FlowMap& map) {
      if (_local_flow) {
        delete _flow;
        _local_flow = false;
      }
      _flow = &map;
      return *this;
    }

    /// \brief Sets the source node.
    ///
    /// Sets the source node.
    /// \return <tt>(*this)</tt>
    EdmondsKarp& source(const Node& node) {
      _source = node;
      return *this;
    }

    /// \brief Sets the target node.
    ///
    /// Sets the target node.
    /// \return <tt>(*this)</tt>
    EdmondsKarp& target(const Node& node) {
      _target = node;
      return *this;
    }

    /// \brief Sets the tolerance used by algorithm.
    ///
    /// Sets the tolerance used by algorithm.
    /// \return <tt>(*this)</tt>
    EdmondsKarp& tolerance(const Tolerance& tolerance) {
      _tolerance = tolerance;
      return *this;
    }

    /// \brief Returns a const reference to the tolerance.
    ///
    /// Returns a const reference to the tolerance object used by
    /// the algorithm.
    const Tolerance& tolerance() const {
      return _tolerance;
    }

    /// \name Execution control
    /// The simplest way to execute the algorithm is to use \ref run().\n
    /// If you need better control on the initial solution or the execution,
    /// you have to call one of the \ref init() functions first, then
    /// \ref start() or multiple times the \ref augment() function.

    ///@{

    /// \brief Initializes the algorithm.
    ///
    /// Initializes the internal data structures and sets the initial
    /// flow to zero on each arc.
    void init() {
      createStructures();
      for (ArcIt it(_graph); it != INVALID; ++it) {
        _flow->set(it, 0);
      }
      _flow_value = 0;
    }

    /// \brief Initializes the algorithm using the given flow map.
    ///
    /// Initializes the internal data structures and sets the initial
    /// flow to the given \c flowMap. The \c flowMap should
    /// contain a feasible flow, i.e. at each node excluding the source
    /// and the target, the incoming flow should be equal to the
    /// outgoing flow.
    template <typename FlowMap>
    void init(const FlowMap& flowMap) {
      createStructures();
      for (ArcIt e(_graph); e != INVALID; ++e) {
        _flow->set(e, flowMap[e]);
      }
      _flow_value = 0;
      for (OutArcIt jt(_graph, _source); jt != INVALID; ++jt) {
        _flow_value += (*_flow)[jt];
      }
      for (InArcIt jt(_graph, _source); jt != INVALID; ++jt) {
        _flow_value -= (*_flow)[jt];
      }
    }

    /// \brief Initializes the algorithm using the given flow map.
    ///
    /// Initializes the internal data structures and sets the initial
    /// flow to the given \c flowMap. The \c flowMap should
    /// contain a feasible flow, i.e. at each node excluding the source
    /// and the target, the incoming flow should be equal to the
    /// outgoing flow.
    /// \return \c false when the given \c flowMap does not contain a
    /// feasible flow.
    template <typename FlowMap>
    bool checkedInit(const FlowMap& flowMap) {
      createStructures();
      for (ArcIt e(_graph); e != INVALID; ++e) {
        _flow->set(e, flowMap[e]);
      }
      for (NodeIt it(_graph); it != INVALID; ++it) {
        if (it == _source || it == _target) continue;
        Value outFlow = 0;
        for (OutArcIt jt(_graph, it); jt != INVALID; ++jt) {
          outFlow += (*_flow)[jt];
        }
        Value inFlow = 0;
        for (InArcIt jt(_graph, it); jt != INVALID; ++jt) {
          inFlow += (*_flow)[jt];
        }
        if (_tolerance.different(outFlow, inFlow)) {
          return false;
        }
      }
      for (ArcIt it(_graph); it != INVALID; ++it) {
        if (_tolerance.less((*_flow)[it], 0)) return false;
        if (_tolerance.less((*_capacity)[it], (*_flow)[it])) return false;
      }
      _flow_value = 0;
      for (OutArcIt jt(_graph, _source); jt != INVALID; ++jt) {
        _flow_value += (*_flow)[jt];
      }
      for (InArcIt jt(_graph, _source); jt != INVALID; ++jt) {
        _flow_value -= (*_flow)[jt];
      }
      return true;
    }

    /// \brief Augments the solution along a shortest path.
    ///
    /// Augments the solution along a shortest path. This function searches a
    /// shortest path between the source and the target
    /// in the residual digraph by the Bfs algoritm.
    /// Then it increases the flow on this path with the minimal residual
    /// capacity on the path. If there is no such path, it gives back
    /// false.
    /// \return \c false when the augmenting did not success, i.e. the
    /// current flow is a feasible and optimal solution.
    bool augment() {
      for (NodeIt n(_graph); n != INVALID; ++n) {
        _pred->set(n, INVALID);
      }

      int first = 0, last = 1;

      _queue[0] = _source;
      _pred->set(_source, OutArcIt(_graph, _source));

      while (first != last && (*_pred)[_target] == INVALID) {
        Node n = _queue[first++];

        for (OutArcIt e(_graph, n); e != INVALID; ++e) {
          Value rem = (*_capacity)[e] - (*_flow)[e];
          Node t = _graph.target(e);
          if (_tolerance.positive(rem) && (*_pred)[t] == INVALID) {
            _pred->set(t, e);
            _queue[last++] = t;
          }
        }
        for (InArcIt e(_graph, n); e != INVALID; ++e) {
          Value rem = (*_flow)[e];
          Node t = _graph.source(e);
          if (_tolerance.positive(rem) && (*_pred)[t] == INVALID) {
            _pred->set(t, e);
            _queue[last++] = t;
          }
        }
      }

      if ((*_pred)[_target] != INVALID) {
        Node n = _target;
        Arc e = (*_pred)[n];

        Value prem = (*_capacity)[e] - (*_flow)[e];
        n = _graph.source(e);
        while (n != _source) {
          e = (*_pred)[n];
          if (_graph.target(e) == n) {
            Value rem = (*_capacity)[e] - (*_flow)[e];
            if (rem < prem) prem = rem;
            n = _graph.source(e);
          } else {
            Value rem = (*_flow)[e];
            if (rem < prem) prem = rem;
            n = _graph.target(e);
          }
        }

        n = _target;
        e = (*_pred)[n];

        _flow->set(e, (*_flow)[e] + prem);
        n = _graph.source(e);
        while (n != _source) {
          e = (*_pred)[n];
          if (_graph.target(e) == n) {
            _flow->set(e, (*_flow)[e] + prem);
            n = _graph.source(e);
          } else {
            _flow->set(e, (*_flow)[e] - prem);
            n = _graph.target(e);
          }
        }

        _flow_value += prem;
        return true;
      } else {
        return false;
      }
    }

    /// \brief Executes the algorithm
    ///
    /// Executes the algorithm by performing augmenting phases until the
    /// optimal solution is reached.
    /// \pre One of the \ref init() functions must be called before
    /// using this function.
    void start() {
      while (augment()) {}
    }

    /// \brief Runs the algorithm.
    ///
    /// Runs the Edmonds-Karp algorithm.
    /// \note ek.run() is just a shortcut of the following code.
    ///\code
    /// ek.init();
    /// ek.start();
    ///\endcode
    void run() {
      init();
      start();
    }

    /// @}

    /// \name Query Functions
    /// The result of the Edmonds-Karp algorithm can be obtained using these
    /// functions.\n
    /// Either \ref run() or \ref start() should be called before using them.

    ///@{

    /// \brief Returns the value of the maximum flow.
    ///
    /// Returns the value of the maximum flow found by the algorithm.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    Value flowValue() const {
      return _flow_value;
    }

    /// \brief Returns the flow value on the given arc.
    ///
    /// Returns the flow value on the given arc.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    Value flow(const Arc& arc) const {
      return (*_flow)[arc];
    }

    /// \brief Returns a const reference to the flow map.
    ///
    /// Returns a const reference to the arc map storing the found flow.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    const FlowMap& flowMap() const {
      return *_flow;
    }

    /// \brief Returns \c true when the node is on the source side of the
    /// minimum cut.
    ///
    /// Returns true when the node is on the source side of the found
    /// minimum cut.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    bool minCut(const Node& node) const {
      return ((*_pred)[node] != INVALID) || node == _source;
    }

    /// \brief Gives back a minimum value cut.
    ///
    /// Sets \c cutMap to the characteristic vector of a minimum value
    /// cut. \c cutMap should be a \ref concepts::WriteMap "writable"
    /// node map with \c bool (or convertible) value type.
    ///
    /// \note This function calls \ref minCut() for each node, so it runs in
    /// O(n) time.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    template <typename CutMap>
    void minCutMap(CutMap& cutMap) const {
      for (NodeIt n(_graph); n != INVALID; ++n) {
        cutMap.set(n, (*_pred)[n] != INVALID);
      }
      cutMap.set(_source, true);
    }

    /// @}

  };

}

#endif
