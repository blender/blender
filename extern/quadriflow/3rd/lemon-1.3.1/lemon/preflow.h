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

#ifndef LEMON_PREFLOW_H
#define LEMON_PREFLOW_H

#include <lemon/tolerance.h>
#include <lemon/elevator.h>

/// \file
/// \ingroup max_flow
/// \brief Implementation of the preflow algorithm.

namespace lemon {

  /// \brief Default traits class of Preflow class.
  ///
  /// Default traits class of Preflow class.
  /// \tparam GR Digraph type.
  /// \tparam CAP Capacity map type.
  template <typename GR, typename CAP>
  struct PreflowDefaultTraits {

    /// \brief The type of the digraph the algorithm runs on.
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

    /// \brief The elevator type used by Preflow algorithm.
    ///
    /// The elevator type used by Preflow algorithm.
    ///
    /// \sa Elevator, LinkedElevator
#ifdef DOXYGEN
    typedef lemon::Elevator<GR, GR::Node> Elevator;
#else
    typedef lemon::Elevator<Digraph, typename Digraph::Node> Elevator;
#endif

    /// \brief Instantiates an Elevator.
    ///
    /// This function instantiates an \ref Elevator.
    /// \param digraph The digraph for which we would like to define
    /// the elevator.
    /// \param max_level The maximum level of the elevator.
    static Elevator* createElevator(const Digraph& digraph, int max_level) {
      return new Elevator(digraph, max_level);
    }

    /// \brief The tolerance used by the algorithm
    ///
    /// The tolerance used by the algorithm to handle inexact computation.
    typedef lemon::Tolerance<Value> Tolerance;

  };


  /// \ingroup max_flow
  ///
  /// \brief %Preflow algorithm class.
  ///
  /// This class provides an implementation of Goldberg-Tarjan's \e preflow
  /// \e push-relabel algorithm producing a \ref max_flow
  /// "flow of maximum value" in a digraph \cite clrs01algorithms,
  /// \cite amo93networkflows, \cite goldberg88newapproach.
  /// The preflow algorithms are the fastest known maximum
  /// flow algorithms. The current implementation uses a mixture of the
  /// \e "highest label" and the \e "bound decrease" heuristics.
  /// The worst case time complexity of the algorithm is \f$O(n^2\sqrt{m})\f$.
  ///
  /// The algorithm consists of two phases. After the first phase
  /// the maximum flow value and the minimum cut is obtained. The
  /// second phase constructs a feasible maximum flow on each arc.
  ///
  /// \warning This implementation cannot handle infinite or very large
  /// capacities (e.g. the maximum value of \c CAP::Value).
  ///
  /// \tparam GR The type of the digraph the algorithm runs on.
  /// \tparam CAP The type of the capacity map. The default map
  /// type is \ref concepts::Digraph::ArcMap "GR::ArcMap<int>".
  /// \tparam TR The traits class that defines various types used by the
  /// algorithm. By default, it is \ref PreflowDefaultTraits
  /// "PreflowDefaultTraits<GR, CAP>".
  /// In most cases, this parameter should not be set directly,
  /// consider to use the named template parameters instead.
#ifdef DOXYGEN
  template <typename GR, typename CAP, typename TR>
#else
  template <typename GR,
            typename CAP = typename GR::template ArcMap<int>,
            typename TR = PreflowDefaultTraits<GR, CAP> >
#endif
  class Preflow {
  public:

    ///The \ref lemon::PreflowDefaultTraits "traits class" of the algorithm.
    typedef TR Traits;
    ///The type of the digraph the algorithm runs on.
    typedef typename Traits::Digraph Digraph;
    ///The type of the capacity map.
    typedef typename Traits::CapacityMap CapacityMap;
    ///The type of the flow values.
    typedef typename Traits::Value Value;

    ///The type of the flow map.
    typedef typename Traits::FlowMap FlowMap;
    ///The type of the elevator.
    typedef typename Traits::Elevator Elevator;
    ///The type of the tolerance.
    typedef typename Traits::Tolerance Tolerance;

  private:

    TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

    const Digraph& _graph;
    const CapacityMap* _capacity;

    int _node_num;

    Node _source, _target;

    FlowMap* _flow;
    bool _local_flow;

    Elevator* _level;
    bool _local_level;

    typedef typename Digraph::template NodeMap<Value> ExcessMap;
    ExcessMap* _excess;

    Tolerance _tolerance;

    bool _phase;


    void createStructures() {
      _node_num = countNodes(_graph);

      if (!_flow) {
        _flow = Traits::createFlowMap(_graph);
        _local_flow = true;
      }
      if (!_level) {
        _level = Traits::createElevator(_graph, _node_num);
        _local_level = true;
      }
      if (!_excess) {
        _excess = new ExcessMap(_graph);
      }
    }

    void destroyStructures() {
      if (_local_flow) {
        delete _flow;
      }
      if (_local_level) {
        delete _level;
      }
      if (_excess) {
        delete _excess;
      }
    }

  public:

    typedef Preflow Create;

    ///\name Named Template Parameters

    ///@{

    template <typename T>
    struct SetFlowMapTraits : public Traits {
      typedef T FlowMap;
      static FlowMap *createFlowMap(const Digraph&) {
        LEMON_ASSERT(false, "FlowMap is not initialized");
        return 0; // ignore warnings
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// FlowMap type
    ///
    /// \ref named-templ-param "Named parameter" for setting FlowMap
    /// type.
    template <typename T>
    struct SetFlowMap
      : public Preflow<Digraph, CapacityMap, SetFlowMapTraits<T> > {
      typedef Preflow<Digraph, CapacityMap,
                      SetFlowMapTraits<T> > Create;
    };

    template <typename T>
    struct SetElevatorTraits : public Traits {
      typedef T Elevator;
      static Elevator *createElevator(const Digraph&, int) {
        LEMON_ASSERT(false, "Elevator is not initialized");
        return 0; // ignore warnings
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// Elevator type
    ///
    /// \ref named-templ-param "Named parameter" for setting Elevator
    /// type. If this named parameter is used, then an external
    /// elevator object must be passed to the algorithm using the
    /// \ref elevator(Elevator&) "elevator()" function before calling
    /// \ref run() or \ref init().
    /// \sa SetStandardElevator
    template <typename T>
    struct SetElevator
      : public Preflow<Digraph, CapacityMap, SetElevatorTraits<T> > {
      typedef Preflow<Digraph, CapacityMap,
                      SetElevatorTraits<T> > Create;
    };

    template <typename T>
    struct SetStandardElevatorTraits : public Traits {
      typedef T Elevator;
      static Elevator *createElevator(const Digraph& digraph, int max_level) {
        return new Elevator(digraph, max_level);
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// Elevator type with automatic allocation
    ///
    /// \ref named-templ-param "Named parameter" for setting Elevator
    /// type with automatic allocation.
    /// The Elevator should have standard constructor interface to be
    /// able to automatically created by the algorithm (i.e. the
    /// digraph and the maximum level should be passed to it).
    /// However, an external elevator object could also be passed to the
    /// algorithm with the \ref elevator(Elevator&) "elevator()" function
    /// before calling \ref run() or \ref init().
    /// \sa SetElevator
    template <typename T>
    struct SetStandardElevator
      : public Preflow<Digraph, CapacityMap,
                       SetStandardElevatorTraits<T> > {
      typedef Preflow<Digraph, CapacityMap,
                      SetStandardElevatorTraits<T> > Create;
    };

    /// @}

  protected:

    Preflow() {}

  public:


    /// \brief The constructor of the class.
    ///
    /// The constructor of the class.
    /// \param digraph The digraph the algorithm runs on.
    /// \param capacity The capacity of the arcs.
    /// \param source The source node.
    /// \param target The target node.
    Preflow(const Digraph& digraph, const CapacityMap& capacity,
            Node source, Node target)
      : _graph(digraph), _capacity(&capacity),
        _node_num(0), _source(source), _target(target),
        _flow(0), _local_flow(false),
        _level(0), _local_level(false),
        _excess(0), _tolerance(), _phase() {}

    /// \brief Destructor.
    ///
    /// Destructor.
    ~Preflow() {
      destroyStructures();
    }

    /// \brief Sets the capacity map.
    ///
    /// Sets the capacity map.
    /// \return <tt>(*this)</tt>
    Preflow& capacityMap(const CapacityMap& map) {
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
    Preflow& flowMap(FlowMap& map) {
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
    Preflow& source(const Node& node) {
      _source = node;
      return *this;
    }

    /// \brief Sets the target node.
    ///
    /// Sets the target node.
    /// \return <tt>(*this)</tt>
    Preflow& target(const Node& node) {
      _target = node;
      return *this;
    }

    /// \brief Sets the elevator used by algorithm.
    ///
    /// Sets the elevator used by algorithm.
    /// If you don't use this function before calling \ref run() or
    /// \ref init(), an instance will be allocated automatically.
    /// The destructor deallocates this automatically allocated elevator,
    /// of course.
    /// \return <tt>(*this)</tt>
    Preflow& elevator(Elevator& elevator) {
      if (_local_level) {
        delete _level;
        _local_level = false;
      }
      _level = &elevator;
      return *this;
    }

    /// \brief Returns a const reference to the elevator.
    ///
    /// Returns a const reference to the elevator.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    const Elevator& elevator() const {
      return *_level;
    }

    /// \brief Sets the tolerance used by the algorithm.
    ///
    /// Sets the tolerance object used by the algorithm.
    /// \return <tt>(*this)</tt>
    Preflow& tolerance(const Tolerance& tolerance) {
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

    /// \name Execution Control
    /// The simplest way to execute the preflow algorithm is to use
    /// \ref run() or \ref runMinCut().\n
    /// If you need better control on the initial solution or the execution,
    /// you have to call one of the \ref init() functions first, then
    /// \ref startFirstPhase() and if you need it \ref startSecondPhase().

    ///@{

    /// \brief Initializes the internal data structures.
    ///
    /// Initializes the internal data structures and sets the initial
    /// flow to zero on each arc.
    void init() {
      createStructures();

      _phase = true;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        (*_excess)[n] = 0;
      }

      for (ArcIt e(_graph); e != INVALID; ++e) {
        _flow->set(e, 0);
      }

      typename Digraph::template NodeMap<bool> reached(_graph, false);

      _level->initStart();
      _level->initAddItem(_target);

      std::vector<Node> queue;
      reached[_source] = true;

      queue.push_back(_target);
      reached[_target] = true;
      while (!queue.empty()) {
        _level->initNewLevel();
        std::vector<Node> nqueue;
        for (int i = 0; i < int(queue.size()); ++i) {
          Node n = queue[i];
          for (InArcIt e(_graph, n); e != INVALID; ++e) {
            Node u = _graph.source(e);
            if (!reached[u] && _tolerance.positive((*_capacity)[e])) {
              reached[u] = true;
              _level->initAddItem(u);
              nqueue.push_back(u);
            }
          }
        }
        queue.swap(nqueue);
      }
      _level->initFinish();

      for (OutArcIt e(_graph, _source); e != INVALID; ++e) {
        if (_tolerance.positive((*_capacity)[e])) {
          Node u = _graph.target(e);
          if ((*_level)[u] == _level->maxLevel()) continue;
          _flow->set(e, (*_capacity)[e]);
          (*_excess)[u] += (*_capacity)[e];
          if (u != _target && !_level->active(u)) {
            _level->activate(u);
          }
        }
      }
    }

    /// \brief Initializes the internal data structures using the
    /// given flow map.
    ///
    /// Initializes the internal data structures and sets the initial
    /// flow to the given \c flowMap. The \c flowMap should contain a
    /// flow or at least a preflow, i.e. at each node excluding the
    /// source node the incoming flow should greater or equal to the
    /// outgoing flow.
    /// \return \c false if the given \c flowMap is not a preflow.
    template <typename FlowMap>
    bool init(const FlowMap& flowMap) {
      createStructures();

      for (ArcIt e(_graph); e != INVALID; ++e) {
        _flow->set(e, flowMap[e]);
      }

      for (NodeIt n(_graph); n != INVALID; ++n) {
        Value excess = 0;
        for (InArcIt e(_graph, n); e != INVALID; ++e) {
          excess += (*_flow)[e];
        }
        for (OutArcIt e(_graph, n); e != INVALID; ++e) {
          excess -= (*_flow)[e];
        }
        if (excess < 0 && n != _source) return false;
        (*_excess)[n] = excess;
      }

      typename Digraph::template NodeMap<bool> reached(_graph, false);

      _level->initStart();
      _level->initAddItem(_target);

      std::vector<Node> queue;
      reached[_source] = true;

      queue.push_back(_target);
      reached[_target] = true;
      while (!queue.empty()) {
        _level->initNewLevel();
        std::vector<Node> nqueue;
        for (int i = 0; i < int(queue.size()); ++i) {
          Node n = queue[i];
          for (InArcIt e(_graph, n); e != INVALID; ++e) {
            Node u = _graph.source(e);
            if (!reached[u] &&
                _tolerance.positive((*_capacity)[e] - (*_flow)[e])) {
              reached[u] = true;
              _level->initAddItem(u);
              nqueue.push_back(u);
            }
          }
          for (OutArcIt e(_graph, n); e != INVALID; ++e) {
            Node v = _graph.target(e);
            if (!reached[v] && _tolerance.positive((*_flow)[e])) {
              reached[v] = true;
              _level->initAddItem(v);
              nqueue.push_back(v);
            }
          }
        }
        queue.swap(nqueue);
      }
      _level->initFinish();

      for (OutArcIt e(_graph, _source); e != INVALID; ++e) {
        Value rem = (*_capacity)[e] - (*_flow)[e];
        if (_tolerance.positive(rem)) {
          Node u = _graph.target(e);
          if ((*_level)[u] == _level->maxLevel()) continue;
          _flow->set(e, (*_capacity)[e]);
          (*_excess)[u] += rem;
        }
      }
      for (InArcIt e(_graph, _source); e != INVALID; ++e) {
        Value rem = (*_flow)[e];
        if (_tolerance.positive(rem)) {
          Node v = _graph.source(e);
          if ((*_level)[v] == _level->maxLevel()) continue;
          _flow->set(e, 0);
          (*_excess)[v] += rem;
        }
      }
      for (NodeIt n(_graph); n != INVALID; ++n)
        if(n!=_source && n!=_target && _tolerance.positive((*_excess)[n]))
          _level->activate(n);

      return true;
    }

    /// \brief Starts the first phase of the preflow algorithm.
    ///
    /// The preflow algorithm consists of two phases, this method runs
    /// the first phase. After the first phase the maximum flow value
    /// and a minimum value cut can already be computed, although a
    /// maximum flow is not yet obtained. So after calling this method
    /// \ref flowValue() returns the value of a maximum flow and \ref
    /// minCut() returns a minimum cut.
    /// \pre One of the \ref init() functions must be called before
    /// using this function.
    void startFirstPhase() {
      _phase = true;

      while (true) {
        int num = _node_num;

        Node n = INVALID;
        int level = -1;

        while (num > 0) {
          n = _level->highestActive();
          if (n == INVALID) goto first_phase_done;
          level = _level->highestActiveLevel();
          --num;

          Value excess = (*_excess)[n];
          int new_level = _level->maxLevel();

          for (OutArcIt e(_graph, n); e != INVALID; ++e) {
            Value rem = (*_capacity)[e] - (*_flow)[e];
            if (!_tolerance.positive(rem)) continue;
            Node v = _graph.target(e);
            if ((*_level)[v] < level) {
              if (!_level->active(v) && v != _target) {
                _level->activate(v);
              }
              if (!_tolerance.less(rem, excess)) {
                _flow->set(e, (*_flow)[e] + excess);
                (*_excess)[v] += excess;
                excess = 0;
                goto no_more_push_1;
              } else {
                excess -= rem;
                (*_excess)[v] += rem;
                _flow->set(e, (*_capacity)[e]);
              }
            } else if (new_level > (*_level)[v]) {
              new_level = (*_level)[v];
            }
          }

          for (InArcIt e(_graph, n); e != INVALID; ++e) {
            Value rem = (*_flow)[e];
            if (!_tolerance.positive(rem)) continue;
            Node v = _graph.source(e);
            if ((*_level)[v] < level) {
              if (!_level->active(v) && v != _target) {
                _level->activate(v);
              }
              if (!_tolerance.less(rem, excess)) {
                _flow->set(e, (*_flow)[e] - excess);
                (*_excess)[v] += excess;
                excess = 0;
                goto no_more_push_1;
              } else {
                excess -= rem;
                (*_excess)[v] += rem;
                _flow->set(e, 0);
              }
            } else if (new_level > (*_level)[v]) {
              new_level = (*_level)[v];
            }
          }

        no_more_push_1:

          (*_excess)[n] = excess;

          if (excess != 0) {
            if (new_level + 1 < _level->maxLevel()) {
              _level->liftHighestActive(new_level + 1);
            } else {
              _level->liftHighestActiveToTop();
            }
            if (_level->emptyLevel(level)) {
              _level->liftToTop(level);
            }
          } else {
            _level->deactivate(n);
          }
        }

        num = _node_num * 20;
        while (num > 0) {
          while (level >= 0 && _level->activeFree(level)) {
            --level;
          }
          if (level == -1) {
            n = _level->highestActive();
            level = _level->highestActiveLevel();
            if (n == INVALID) goto first_phase_done;
          } else {
            n = _level->activeOn(level);
          }
          --num;

          Value excess = (*_excess)[n];
          int new_level = _level->maxLevel();

          for (OutArcIt e(_graph, n); e != INVALID; ++e) {
            Value rem = (*_capacity)[e] - (*_flow)[e];
            if (!_tolerance.positive(rem)) continue;
            Node v = _graph.target(e);
            if ((*_level)[v] < level) {
              if (!_level->active(v) && v != _target) {
                _level->activate(v);
              }
              if (!_tolerance.less(rem, excess)) {
                _flow->set(e, (*_flow)[e] + excess);
                (*_excess)[v] += excess;
                excess = 0;
                goto no_more_push_2;
              } else {
                excess -= rem;
                (*_excess)[v] += rem;
                _flow->set(e, (*_capacity)[e]);
              }
            } else if (new_level > (*_level)[v]) {
              new_level = (*_level)[v];
            }
          }

          for (InArcIt e(_graph, n); e != INVALID; ++e) {
            Value rem = (*_flow)[e];
            if (!_tolerance.positive(rem)) continue;
            Node v = _graph.source(e);
            if ((*_level)[v] < level) {
              if (!_level->active(v) && v != _target) {
                _level->activate(v);
              }
              if (!_tolerance.less(rem, excess)) {
                _flow->set(e, (*_flow)[e] - excess);
                (*_excess)[v] += excess;
                excess = 0;
                goto no_more_push_2;
              } else {
                excess -= rem;
                (*_excess)[v] += rem;
                _flow->set(e, 0);
              }
            } else if (new_level > (*_level)[v]) {
              new_level = (*_level)[v];
            }
          }

        no_more_push_2:

          (*_excess)[n] = excess;

          if (excess != 0) {
            if (new_level + 1 < _level->maxLevel()) {
              _level->liftActiveOn(level, new_level + 1);
            } else {
              _level->liftActiveToTop(level);
            }
            if (_level->emptyLevel(level)) {
              _level->liftToTop(level);
            }
          } else {
            _level->deactivate(n);
          }
        }
      }
    first_phase_done:;
    }

    /// \brief Starts the second phase of the preflow algorithm.
    ///
    /// The preflow algorithm consists of two phases, this method runs
    /// the second phase. After calling one of the \ref init() functions
    /// and \ref startFirstPhase() and then \ref startSecondPhase(),
    /// \ref flowMap() returns a maximum flow, \ref flowValue() returns the
    /// value of a maximum flow, \ref minCut() returns a minimum cut
    /// \pre One of the \ref init() functions and \ref startFirstPhase()
    /// must be called before using this function.
    void startSecondPhase() {
      _phase = false;

      typename Digraph::template NodeMap<bool> reached(_graph);
      for (NodeIt n(_graph); n != INVALID; ++n) {
        reached[n] = (*_level)[n] < _level->maxLevel();
      }

      _level->initStart();
      _level->initAddItem(_source);

      std::vector<Node> queue;
      queue.push_back(_source);
      reached[_source] = true;

      while (!queue.empty()) {
        _level->initNewLevel();
        std::vector<Node> nqueue;
        for (int i = 0; i < int(queue.size()); ++i) {
          Node n = queue[i];
          for (OutArcIt e(_graph, n); e != INVALID; ++e) {
            Node v = _graph.target(e);
            if (!reached[v] && _tolerance.positive((*_flow)[e])) {
              reached[v] = true;
              _level->initAddItem(v);
              nqueue.push_back(v);
            }
          }
          for (InArcIt e(_graph, n); e != INVALID; ++e) {
            Node u = _graph.source(e);
            if (!reached[u] &&
                _tolerance.positive((*_capacity)[e] - (*_flow)[e])) {
              reached[u] = true;
              _level->initAddItem(u);
              nqueue.push_back(u);
            }
          }
        }
        queue.swap(nqueue);
      }
      _level->initFinish();

      for (NodeIt n(_graph); n != INVALID; ++n) {
        if (!reached[n]) {
          _level->dirtyTopButOne(n);
        } else if ((*_excess)[n] > 0 && _target != n) {
          _level->activate(n);
        }
      }

      Node n;
      while ((n = _level->highestActive()) != INVALID) {
        Value excess = (*_excess)[n];
        int level = _level->highestActiveLevel();
        int new_level = _level->maxLevel();

        for (OutArcIt e(_graph, n); e != INVALID; ++e) {
          Value rem = (*_capacity)[e] - (*_flow)[e];
          if (!_tolerance.positive(rem)) continue;
          Node v = _graph.target(e);
          if ((*_level)[v] < level) {
            if (!_level->active(v) && v != _source) {
              _level->activate(v);
            }
            if (!_tolerance.less(rem, excess)) {
              _flow->set(e, (*_flow)[e] + excess);
              (*_excess)[v] += excess;
              excess = 0;
              goto no_more_push;
            } else {
              excess -= rem;
              (*_excess)[v] += rem;
              _flow->set(e, (*_capacity)[e]);
            }
          } else if (new_level > (*_level)[v]) {
            new_level = (*_level)[v];
          }
        }

        for (InArcIt e(_graph, n); e != INVALID; ++e) {
          Value rem = (*_flow)[e];
          if (!_tolerance.positive(rem)) continue;
          Node v = _graph.source(e);
          if ((*_level)[v] < level) {
            if (!_level->active(v) && v != _source) {
              _level->activate(v);
            }
            if (!_tolerance.less(rem, excess)) {
              _flow->set(e, (*_flow)[e] - excess);
              (*_excess)[v] += excess;
              excess = 0;
              goto no_more_push;
            } else {
              excess -= rem;
              (*_excess)[v] += rem;
              _flow->set(e, 0);
            }
          } else if (new_level > (*_level)[v]) {
            new_level = (*_level)[v];
          }
        }

      no_more_push:

        (*_excess)[n] = excess;

        if (excess != 0) {
          if (new_level + 1 < _level->maxLevel()) {
            _level->liftHighestActive(new_level + 1);
          } else {
            // Calculation error
            _level->liftHighestActiveToTop();
          }
          if (_level->emptyLevel(level)) {
            // Calculation error
            _level->liftToTop(level);
          }
        } else {
          _level->deactivate(n);
        }

      }
    }

    /// \brief Runs the preflow algorithm.
    ///
    /// Runs the preflow algorithm.
    /// \note pf.run() is just a shortcut of the following code.
    /// \code
    ///   pf.init();
    ///   pf.startFirstPhase();
    ///   pf.startSecondPhase();
    /// \endcode
    void run() {
      init();
      startFirstPhase();
      startSecondPhase();
    }

    /// \brief Runs the preflow algorithm to compute the minimum cut.
    ///
    /// Runs the preflow algorithm to compute the minimum cut.
    /// \note pf.runMinCut() is just a shortcut of the following code.
    /// \code
    ///   pf.init();
    ///   pf.startFirstPhase();
    /// \endcode
    void runMinCut() {
      init();
      startFirstPhase();
    }

    /// @}

    /// \name Query Functions
    /// The results of the preflow algorithm can be obtained using these
    /// functions.\n
    /// Either one of the \ref run() "run*()" functions or one of the
    /// \ref startFirstPhase() "start*()" functions should be called
    /// before using them.

    ///@{

    /// \brief Returns the value of the maximum flow.
    ///
    /// Returns the value of the maximum flow by returning the excess
    /// of the target node. This value equals to the value of
    /// the maximum flow already after the first phase of the algorithm.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    Value flowValue() const {
      return (*_excess)[_target];
    }

    /// \brief Returns the flow value on the given arc.
    ///
    /// Returns the flow value on the given arc. This method can
    /// be called after the second phase of the algorithm.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    Value flow(const Arc& arc) const {
      return (*_flow)[arc];
    }

    /// \brief Returns a const reference to the flow map.
    ///
    /// Returns a const reference to the arc map storing the found flow.
    /// This method can be called after the second phase of the algorithm.
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
    /// minimum cut. This method can be called both after running \ref
    /// startFirstPhase() and \ref startSecondPhase().
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    bool minCut(const Node& node) const {
      return ((*_level)[node] == _level->maxLevel()) == _phase;
    }

    /// \brief Gives back a minimum value cut.
    ///
    /// Sets \c cutMap to the characteristic vector of a minimum value
    /// cut. \c cutMap should be a \ref concepts::WriteMap "writable"
    /// node map with \c bool (or convertible) value type.
    ///
    /// This method can be called both after running \ref startFirstPhase()
    /// and \ref startSecondPhase(). The result after the second phase
    /// could be slightly different if inexact computation is used.
    ///
    /// \note This function calls \ref minCut() for each node, so it runs in
    /// O(n) time.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    template <typename CutMap>
    void minCutMap(CutMap& cutMap) const {
      for (NodeIt n(_graph); n != INVALID; ++n) {
        cutMap.set(n, minCut(n));
      }
    }

    /// @}
  };
}

#endif
