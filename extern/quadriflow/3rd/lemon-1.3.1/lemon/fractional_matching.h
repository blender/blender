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

#ifndef LEMON_FRACTIONAL_MATCHING_H
#define LEMON_FRACTIONAL_MATCHING_H

#include <vector>
#include <queue>
#include <set>
#include <limits>

#include <lemon/core.h>
#include <lemon/unionfind.h>
#include <lemon/bin_heap.h>
#include <lemon/maps.h>
#include <lemon/assert.h>
#include <lemon/elevator.h>

///\ingroup matching
///\file
///\brief Fractional matching algorithms in general graphs.

namespace lemon {

  /// \brief Default traits class of MaxFractionalMatching class.
  ///
  /// Default traits class of MaxFractionalMatching class.
  /// \tparam GR Graph type.
  template <typename GR>
  struct MaxFractionalMatchingDefaultTraits {

    /// \brief The type of the graph the algorithm runs on.
    typedef GR Graph;

    /// \brief The type of the map that stores the matching.
    ///
    /// The type of the map that stores the matching arcs.
    /// It must meet the \ref concepts::ReadWriteMap "ReadWriteMap" concept.
    typedef typename Graph::template NodeMap<typename GR::Arc> MatchingMap;

    /// \brief Instantiates a MatchingMap.
    ///
    /// This function instantiates a \ref MatchingMap.
    /// \param graph The graph for which we would like to define
    /// the matching map.
    static MatchingMap* createMatchingMap(const Graph& graph) {
      return new MatchingMap(graph);
    }

    /// \brief The elevator type used by MaxFractionalMatching algorithm.
    ///
    /// The elevator type used by MaxFractionalMatching algorithm.
    ///
    /// \sa Elevator
    /// \sa LinkedElevator
    typedef LinkedElevator<Graph, typename Graph::Node> Elevator;

    /// \brief Instantiates an Elevator.
    ///
    /// This function instantiates an \ref Elevator.
    /// \param graph The graph for which we would like to define
    /// the elevator.
    /// \param max_level The maximum level of the elevator.
    static Elevator* createElevator(const Graph& graph, int max_level) {
      return new Elevator(graph, max_level);
    }
  };

  /// \ingroup matching
  ///
  /// \brief Max cardinality fractional matching
  ///
  /// This class provides an implementation of fractional matching
  /// algorithm based on push-relabel principle.
  ///
  /// The maximum cardinality fractional matching is a relaxation of the
  /// maximum cardinality matching problem where the odd set constraints
  /// are omitted.
  /// It can be formulated with the following linear program.
  /// \f[ \sum_{e \in \delta(u)}x_e \le 1 \quad \forall u\in V\f]
  /// \f[x_e \ge 0\quad \forall e\in E\f]
  /// \f[\max \sum_{e\in E}x_e\f]
  /// where \f$\delta(X)\f$ is the set of edges incident to a node in
  /// \f$X\f$. The result can be represented as the union of a
  /// matching with one value edges and a set of odd length cycles
  /// with half value edges.
  ///
  /// The algorithm calculates an optimal fractional matching and a
  /// barrier. The number of adjacents of any node set minus the size
  /// of node set is a lower bound on the uncovered nodes in the
  /// graph. For maximum matching a barrier is computed which
  /// maximizes this difference.
  ///
  /// The algorithm can be executed with the run() function.  After it
  /// the matching (the primal solution) and the barrier (the dual
  /// solution) can be obtained using the query functions.
  ///
  /// The primal solution is multiplied by
  /// \ref MaxFractionalMatching::primalScale "2".
  ///
  /// \tparam GR The undirected graph type the algorithm runs on.
#ifdef DOXYGEN
  template <typename GR, typename TR>
#else
  template <typename GR,
            typename TR = MaxFractionalMatchingDefaultTraits<GR> >
#endif
  class MaxFractionalMatching {
  public:

    /// \brief The \ref lemon::MaxFractionalMatchingDefaultTraits
    /// "traits class" of the algorithm.
    typedef TR Traits;
    /// The type of the graph the algorithm runs on.
    typedef typename TR::Graph Graph;
    /// The type of the matching map.
    typedef typename TR::MatchingMap MatchingMap;
    /// The type of the elevator.
    typedef typename TR::Elevator Elevator;

    /// \brief Scaling factor for primal solution
    ///
    /// Scaling factor for primal solution.
    static const int primalScale = 2;

  private:

    const Graph &_graph;
    int _node_num;
    bool _allow_loops;
    int _empty_level;

    TEMPLATE_GRAPH_TYPEDEFS(Graph);

    bool _local_matching;
    MatchingMap *_matching;

    bool _local_level;
    Elevator *_level;

    typedef typename Graph::template NodeMap<int> InDegMap;
    InDegMap *_indeg;

    void createStructures() {
      _node_num = countNodes(_graph);

      if (!_matching) {
        _local_matching = true;
        _matching = Traits::createMatchingMap(_graph);
      }
      if (!_level) {
        _local_level = true;
        _level = Traits::createElevator(_graph, _node_num);
      }
      if (!_indeg) {
        _indeg = new InDegMap(_graph);
      }
    }

    void destroyStructures() {
      if (_local_matching) {
        delete _matching;
      }
      if (_local_level) {
        delete _level;
      }
      if (_indeg) {
        delete _indeg;
      }
    }

    void postprocessing() {
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_indeg)[n] != 0) continue;
        _indeg->set(n, -1);
        Node u = n;
        while ((*_matching)[u] != INVALID) {
          Node v = _graph.target((*_matching)[u]);
          _indeg->set(v, -1);
          Arc a = _graph.oppositeArc((*_matching)[u]);
          u = _graph.target((*_matching)[v]);
          _indeg->set(u, -1);
          _matching->set(v, a);
        }
      }

      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_indeg)[n] != 1) continue;
        _indeg->set(n, -1);

        int num = 1;
        Node u = _graph.target((*_matching)[n]);
        while (u != n) {
          _indeg->set(u, -1);
          u = _graph.target((*_matching)[u]);
          ++num;
        }
        if (num % 2 == 0 && num > 2) {
          Arc prev = _graph.oppositeArc((*_matching)[n]);
          Node v = _graph.target((*_matching)[n]);
          u = _graph.target((*_matching)[v]);
          _matching->set(v, prev);
          while (u != n) {
            prev = _graph.oppositeArc((*_matching)[u]);
            v = _graph.target((*_matching)[u]);
            u = _graph.target((*_matching)[v]);
            _matching->set(v, prev);
          }
        }
      }
    }

  public:

    typedef MaxFractionalMatching Create;

    ///\name Named Template Parameters

    ///@{

    template <typename T>
    struct SetMatchingMapTraits : public Traits {
      typedef T MatchingMap;
      static MatchingMap *createMatchingMap(const Graph&) {
        LEMON_ASSERT(false, "MatchingMap is not initialized");
        return 0; // ignore warnings
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// MatchingMap type
    ///
    /// \ref named-templ-param "Named parameter" for setting MatchingMap
    /// type.
    template <typename T>
    struct SetMatchingMap
      : public MaxFractionalMatching<Graph, SetMatchingMapTraits<T> > {
      typedef MaxFractionalMatching<Graph, SetMatchingMapTraits<T> > Create;
    };

    template <typename T>
    struct SetElevatorTraits : public Traits {
      typedef T Elevator;
      static Elevator *createElevator(const Graph&, int) {
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
      : public MaxFractionalMatching<Graph, SetElevatorTraits<T> > {
      typedef MaxFractionalMatching<Graph, SetElevatorTraits<T> > Create;
    };

    template <typename T>
    struct SetStandardElevatorTraits : public Traits {
      typedef T Elevator;
      static Elevator *createElevator(const Graph& graph, int max_level) {
        return new Elevator(graph, max_level);
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// Elevator type with automatic allocation
    ///
    /// \ref named-templ-param "Named parameter" for setting Elevator
    /// type with automatic allocation.
    /// The Elevator should have standard constructor interface to be
    /// able to automatically created by the algorithm (i.e. the
    /// graph and the maximum level should be passed to it).
    /// However an external elevator object could also be passed to the
    /// algorithm with the \ref elevator(Elevator&) "elevator()" function
    /// before calling \ref run() or \ref init().
    /// \sa SetElevator
    template <typename T>
    struct SetStandardElevator
      : public MaxFractionalMatching<Graph, SetStandardElevatorTraits<T> > {
      typedef MaxFractionalMatching<Graph,
                                    SetStandardElevatorTraits<T> > Create;
    };

    /// @}

  protected:

    MaxFractionalMatching() {}

  public:

    /// \brief Constructor
    ///
    /// Constructor.
    ///
    MaxFractionalMatching(const Graph &graph, bool allow_loops = true)
      : _graph(graph), _allow_loops(allow_loops),
        _local_matching(false), _matching(0),
        _local_level(false), _level(0),  _indeg(0)
    {}

    ~MaxFractionalMatching() {
      destroyStructures();
    }

    /// \brief Sets the matching map.
    ///
    /// Sets the matching map.
    /// If you don't use this function before calling \ref run() or
    /// \ref init(), an instance will be allocated automatically.
    /// The destructor deallocates this automatically allocated map,
    /// of course.
    /// \return <tt>(*this)</tt>
    MaxFractionalMatching& matchingMap(MatchingMap& map) {
      if (_local_matching) {
        delete _matching;
        _local_matching = false;
      }
      _matching = &map;
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
    MaxFractionalMatching& elevator(Elevator& elevator) {
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

    /// \name Execution control
    /// The simplest way to execute the algorithm is to use one of the
    /// member functions called \c run(). \n
    /// If you need more control on the execution, first
    /// you must call \ref init() and then one variant of the start()
    /// member.

    /// @{

    /// \brief Initializes the internal data structures.
    ///
    /// Initializes the internal data structures and sets the initial
    /// matching.
    void init() {
      createStructures();

      _level->initStart();
      for (NodeIt n(_graph); n != INVALID; ++n) {
        _indeg->set(n, 0);
        _matching->set(n, INVALID);
        _level->initAddItem(n);
      }
      _level->initFinish();

      _empty_level = _node_num;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        for (OutArcIt a(_graph, n); a != INVALID; ++a) {
          if (_graph.target(a) == n && !_allow_loops) continue;
          _matching->set(n, a);
          Node v = _graph.target((*_matching)[n]);
          _indeg->set(v, (*_indeg)[v] + 1);
          break;
        }
      }

      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_indeg)[n] == 0) {
          _level->activate(n);
        }
      }
    }

    /// \brief Starts the algorithm and computes a fractional matching
    ///
    /// The algorithm computes a maximum fractional matching.
    ///
    /// \param postprocess The algorithm computes first a matching
    /// which is a union of a matching with one value edges, cycles
    /// with half value edges and even length paths with half value
    /// edges. If the parameter is true, then after the push-relabel
    /// algorithm it postprocesses the matching to contain only
    /// matching edges and half value odd cycles.
    void start(bool postprocess = true) {
      Node n;
      while ((n = _level->highestActive()) != INVALID) {
        int level = _level->highestActiveLevel();
        int new_level = _level->maxLevel();
        for (InArcIt a(_graph, n); a != INVALID; ++a) {
          Node u = _graph.source(a);
          if (n == u && !_allow_loops) continue;
          Node v = _graph.target((*_matching)[u]);
          if ((*_level)[v] < level) {
            _indeg->set(v, (*_indeg)[v] - 1);
            if ((*_indeg)[v] == 0) {
              _level->activate(v);
            }
            _matching->set(u, a);
            _indeg->set(n, (*_indeg)[n] + 1);
            _level->deactivate(n);
            goto no_more_push;
          } else if (new_level > (*_level)[v]) {
            new_level = (*_level)[v];
          }
        }

        if (new_level + 1 < _level->maxLevel()) {
          _level->liftHighestActive(new_level + 1);
        } else {
          _level->liftHighestActiveToTop();
        }
        if (_level->emptyLevel(level)) {
          _level->liftToTop(level);
        }
      no_more_push:
        ;
      }
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_matching)[n] == INVALID) continue;
        Node u = _graph.target((*_matching)[n]);
        if ((*_indeg)[u] > 1) {
          _indeg->set(u, (*_indeg)[u] - 1);
          _matching->set(n, INVALID);
        }
      }
      if (postprocess) {
        postprocessing();
      }
    }

    /// \brief Starts the algorithm and computes a perfect fractional
    /// matching
    ///
    /// The algorithm computes a perfect fractional matching. If it
    /// does not exists, then the algorithm returns false and the
    /// matching is undefined and the barrier.
    ///
    /// \param postprocess The algorithm computes first a matching
    /// which is a union of a matching with one value edges, cycles
    /// with half value edges and even length paths with half value
    /// edges. If the parameter is true, then after the push-relabel
    /// algorithm it postprocesses the matching to contain only
    /// matching edges and half value odd cycles.
    bool startPerfect(bool postprocess = true) {
      Node n;
      while ((n = _level->highestActive()) != INVALID) {
        int level = _level->highestActiveLevel();
        int new_level = _level->maxLevel();
        for (InArcIt a(_graph, n); a != INVALID; ++a) {
          Node u = _graph.source(a);
          if (n == u && !_allow_loops) continue;
          Node v = _graph.target((*_matching)[u]);
          if ((*_level)[v] < level) {
            _indeg->set(v, (*_indeg)[v] - 1);
            if ((*_indeg)[v] == 0) {
              _level->activate(v);
            }
            _matching->set(u, a);
            _indeg->set(n, (*_indeg)[n] + 1);
            _level->deactivate(n);
            goto no_more_push;
          } else if (new_level > (*_level)[v]) {
            new_level = (*_level)[v];
          }
        }

        if (new_level + 1 < _level->maxLevel()) {
          _level->liftHighestActive(new_level + 1);
        } else {
          _level->liftHighestActiveToTop();
          _empty_level = _level->maxLevel() - 1;
          return false;
        }
        if (_level->emptyLevel(level)) {
          _level->liftToTop(level);
          _empty_level = level;
          return false;
        }
      no_more_push:
        ;
      }
      if (postprocess) {
        postprocessing();
      }
      return true;
    }

    /// \brief Runs the algorithm
    ///
    /// Just a shortcut for the next code:
    ///\code
    /// init();
    /// start();
    ///\endcode
    void run(bool postprocess = true) {
      init();
      start(postprocess);
    }

    /// \brief Runs the algorithm to find a perfect fractional matching
    ///
    /// Just a shortcut for the next code:
    ///\code
    /// init();
    /// startPerfect();
    ///\endcode
    bool runPerfect(bool postprocess = true) {
      init();
      return startPerfect(postprocess);
    }

    ///@}

    /// \name Query Functions
    /// The result of the %Matching algorithm can be obtained using these
    /// functions.\n
    /// Before the use of these functions,
    /// either run() or start() must be called.
    ///@{


    /// \brief Return the number of covered nodes in the matching.
    ///
    /// This function returns the number of covered nodes in the matching.
    ///
    /// \pre Either run() or start() must be called before using this function.
    int matchingSize() const {
      int num = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_matching)[n] != INVALID) {
          ++num;
        }
      }
      return num;
    }

    /// \brief Returns a const reference to the matching map.
    ///
    /// Returns a const reference to the node map storing the found
    /// fractional matching. This method can be called after
    /// running the algorithm.
    ///
    /// \pre Either \ref run() or \ref init() must be called before
    /// using this function.
    const MatchingMap& matchingMap() const {
      return *_matching;
    }

    /// \brief Return \c true if the given edge is in the matching.
    ///
    /// This function returns \c true if the given edge is in the
    /// found matching. The result is scaled by \ref primalScale
    /// "primal scale".
    ///
    /// \pre Either run() or start() must be called before using this function.
    int matching(const Edge& edge) const {
      return (edge == (*_matching)[_graph.u(edge)] ? 1 : 0) +
        (edge == (*_matching)[_graph.v(edge)] ? 1 : 0);
    }

    /// \brief Return the fractional matching arc (or edge) incident
    /// to the given node.
    ///
    /// This function returns one of the fractional matching arc (or
    /// edge) incident to the given node in the found matching or \c
    /// INVALID if the node is not covered by the matching or if the
    /// node is on an odd length cycle then it is the successor edge
    /// on the cycle.
    ///
    /// \pre Either run() or start() must be called before using this function.
    Arc matching(const Node& node) const {
      return (*_matching)[node];
    }

    /// \brief Returns true if the node is in the barrier
    ///
    /// The barrier is a subset of the nodes. If the nodes in the
    /// barrier have less adjacent nodes than the size of the barrier,
    /// then at least as much nodes cannot be covered as the
    /// difference of the two subsets.
    bool barrier(const Node& node) const {
      return (*_level)[node] >= _empty_level;
    }

    /// @}

  };

  /// \ingroup matching
  ///
  /// \brief Weighted fractional matching in general graphs
  ///
  /// This class provides an efficient implementation of fractional
  /// matching algorithm. The implementation uses priority queues and
  /// provides \f$O(nm\log n)\f$ time complexity.
  ///
  /// The maximum weighted fractional matching is a relaxation of the
  /// maximum weighted matching problem where the odd set constraints
  /// are omitted.
  /// It can be formulated with the following linear program.
  /// \f[ \sum_{e \in \delta(u)}x_e \le 1 \quad \forall u\in V\f]
  /// \f[x_e \ge 0\quad \forall e\in E\f]
  /// \f[\max \sum_{e\in E}x_ew_e\f]
  /// where \f$\delta(X)\f$ is the set of edges incident to a node in
  /// \f$X\f$. The result must be the union of a matching with one
  /// value edges and a set of odd length cycles with half value edges.
  ///
  /// The algorithm calculates an optimal fractional matching and a
  /// proof of the optimality. The solution of the dual problem can be
  /// used to check the result of the algorithm. The dual linear
  /// problem is the following.
  /// \f[ y_u + y_v \ge w_{uv} \quad \forall uv\in E\f]
  /// \f[y_u \ge 0 \quad \forall u \in V\f]
  /// \f[\min \sum_{u \in V}y_u \f]
  ///
  /// The algorithm can be executed with the run() function.
  /// After it the matching (the primal solution) and the dual solution
  /// can be obtained using the query functions.
  ///
  /// The primal solution is multiplied by
  /// \ref MaxWeightedFractionalMatching::primalScale "2".
  /// If the value type is integer, then the dual
  /// solution is scaled by
  /// \ref MaxWeightedFractionalMatching::dualScale "4".
  ///
  /// \tparam GR The undirected graph type the algorithm runs on.
  /// \tparam WM The type edge weight map. The default type is
  /// \ref concepts::Graph::EdgeMap "GR::EdgeMap<int>".
#ifdef DOXYGEN
  template <typename GR, typename WM>
#else
  template <typename GR,
            typename WM = typename GR::template EdgeMap<int> >
#endif
  class MaxWeightedFractionalMatching {
  public:

    /// The graph type of the algorithm
    typedef GR Graph;
    /// The type of the edge weight map
    typedef WM WeightMap;
    /// The value type of the edge weights
    typedef typename WeightMap::Value Value;

    /// The type of the matching map
    typedef typename Graph::template NodeMap<typename Graph::Arc>
    MatchingMap;

    /// \brief Scaling factor for primal solution
    ///
    /// Scaling factor for primal solution.
    static const int primalScale = 2;

    /// \brief Scaling factor for dual solution
    ///
    /// Scaling factor for dual solution. It is equal to 4 or 1
    /// according to the value type.
    static const int dualScale =
      std::numeric_limits<Value>::is_integer ? 4 : 1;

  private:

    TEMPLATE_GRAPH_TYPEDEFS(Graph);

    typedef typename Graph::template NodeMap<Value> NodePotential;

    const Graph& _graph;
    const WeightMap& _weight;

    MatchingMap* _matching;
    NodePotential* _node_potential;

    int _node_num;
    bool _allow_loops;

    enum Status {
      EVEN = -1, MATCHED = 0, ODD = 1
    };

    typedef typename Graph::template NodeMap<Status> StatusMap;
    StatusMap* _status;

    typedef typename Graph::template NodeMap<Arc> PredMap;
    PredMap* _pred;

    typedef ExtendFindEnum<IntNodeMap> TreeSet;

    IntNodeMap *_tree_set_index;
    TreeSet *_tree_set;

    IntNodeMap *_delta1_index;
    BinHeap<Value, IntNodeMap> *_delta1;

    IntNodeMap *_delta2_index;
    BinHeap<Value, IntNodeMap> *_delta2;

    IntEdgeMap *_delta3_index;
    BinHeap<Value, IntEdgeMap> *_delta3;

    Value _delta_sum;

    void createStructures() {
      _node_num = countNodes(_graph);

      if (!_matching) {
        _matching = new MatchingMap(_graph);
      }
      if (!_node_potential) {
        _node_potential = new NodePotential(_graph);
      }
      if (!_status) {
        _status = new StatusMap(_graph);
      }
      if (!_pred) {
        _pred = new PredMap(_graph);
      }
      if (!_tree_set) {
        _tree_set_index = new IntNodeMap(_graph);
        _tree_set = new TreeSet(*_tree_set_index);
      }
      if (!_delta1) {
        _delta1_index = new IntNodeMap(_graph);
        _delta1 = new BinHeap<Value, IntNodeMap>(*_delta1_index);
      }
      if (!_delta2) {
        _delta2_index = new IntNodeMap(_graph);
        _delta2 = new BinHeap<Value, IntNodeMap>(*_delta2_index);
      }
      if (!_delta3) {
        _delta3_index = new IntEdgeMap(_graph);
        _delta3 = new BinHeap<Value, IntEdgeMap>(*_delta3_index);
      }
    }

    void destroyStructures() {
      if (_matching) {
        delete _matching;
      }
      if (_node_potential) {
        delete _node_potential;
      }
      if (_status) {
        delete _status;
      }
      if (_pred) {
        delete _pred;
      }
      if (_tree_set) {
        delete _tree_set_index;
        delete _tree_set;
      }
      if (_delta1) {
        delete _delta1_index;
        delete _delta1;
      }
      if (_delta2) {
        delete _delta2_index;
        delete _delta2;
      }
      if (_delta3) {
        delete _delta3_index;
        delete _delta3;
      }
    }

    void matchedToEven(Node node, int tree) {
      _tree_set->insert(node, tree);
      _node_potential->set(node, (*_node_potential)[node] + _delta_sum);
      _delta1->push(node, (*_node_potential)[node]);

      if (_delta2->state(node) == _delta2->IN_HEAP) {
        _delta2->erase(node);
      }

      for (InArcIt a(_graph, node); a != INVALID; ++a) {
        Node v = _graph.source(a);
        Value rw = (*_node_potential)[node] + (*_node_potential)[v] -
          dualScale * _weight[a];
        if (node == v) {
          if (_allow_loops && _graph.direction(a)) {
            _delta3->push(a, rw / 2);
          }
        } else if ((*_status)[v] == EVEN) {
          _delta3->push(a, rw / 2);
        } else if ((*_status)[v] == MATCHED) {
          if (_delta2->state(v) != _delta2->IN_HEAP) {
            _pred->set(v, a);
            _delta2->push(v, rw);
          } else if ((*_delta2)[v] > rw) {
            _pred->set(v, a);
            _delta2->decrease(v, rw);
          }
        }
      }
    }

    void matchedToOdd(Node node, int tree) {
      _tree_set->insert(node, tree);
      _node_potential->set(node, (*_node_potential)[node] - _delta_sum);

      if (_delta2->state(node) == _delta2->IN_HEAP) {
        _delta2->erase(node);
      }
    }

    void evenToMatched(Node node, int tree) {
      _delta1->erase(node);
      _node_potential->set(node, (*_node_potential)[node] - _delta_sum);
      Arc min = INVALID;
      Value minrw = std::numeric_limits<Value>::max();
      for (InArcIt a(_graph, node); a != INVALID; ++a) {
        Node v = _graph.source(a);
        Value rw = (*_node_potential)[node] + (*_node_potential)[v] -
          dualScale * _weight[a];

        if (node == v) {
          if (_allow_loops && _graph.direction(a)) {
            _delta3->erase(a);
          }
        } else if ((*_status)[v] == EVEN) {
          _delta3->erase(a);
          if (minrw > rw) {
            min = _graph.oppositeArc(a);
            minrw = rw;
          }
        } else if ((*_status)[v]  == MATCHED) {
          if ((*_pred)[v] == a) {
            Arc mina = INVALID;
            Value minrwa = std::numeric_limits<Value>::max();
            for (OutArcIt aa(_graph, v); aa != INVALID; ++aa) {
              Node va = _graph.target(aa);
              if ((*_status)[va] != EVEN ||
                  _tree_set->find(va) == tree) continue;
              Value rwa = (*_node_potential)[v] + (*_node_potential)[va] -
                dualScale * _weight[aa];
              if (minrwa > rwa) {
                minrwa = rwa;
                mina = aa;
              }
            }
            if (mina != INVALID) {
              _pred->set(v, mina);
              _delta2->increase(v, minrwa);
            } else {
              _pred->set(v, INVALID);
              _delta2->erase(v);
            }
          }
        }
      }
      if (min != INVALID) {
        _pred->set(node, min);
        _delta2->push(node, minrw);
      } else {
        _pred->set(node, INVALID);
      }
    }

    void oddToMatched(Node node) {
      _node_potential->set(node, (*_node_potential)[node] + _delta_sum);
      Arc min = INVALID;
      Value minrw = std::numeric_limits<Value>::max();
      for (InArcIt a(_graph, node); a != INVALID; ++a) {
        Node v = _graph.source(a);
        if ((*_status)[v] != EVEN) continue;
        Value rw = (*_node_potential)[node] + (*_node_potential)[v] -
          dualScale * _weight[a];

        if (minrw > rw) {
          min = _graph.oppositeArc(a);
          minrw = rw;
        }
      }
      if (min != INVALID) {
        _pred->set(node, min);
        _delta2->push(node, minrw);
      } else {
        _pred->set(node, INVALID);
      }
    }

    void alternatePath(Node even, int tree) {
      Node odd;

      _status->set(even, MATCHED);
      evenToMatched(even, tree);

      Arc prev = (*_matching)[even];
      while (prev != INVALID) {
        odd = _graph.target(prev);
        even = _graph.target((*_pred)[odd]);
        _matching->set(odd, (*_pred)[odd]);
        _status->set(odd, MATCHED);
        oddToMatched(odd);

        prev = (*_matching)[even];
        _status->set(even, MATCHED);
        _matching->set(even, _graph.oppositeArc((*_matching)[odd]));
        evenToMatched(even, tree);
      }
    }

    void destroyTree(int tree) {
      for (typename TreeSet::ItemIt n(*_tree_set, tree); n != INVALID; ++n) {
        if ((*_status)[n] == EVEN) {
          _status->set(n, MATCHED);
          evenToMatched(n, tree);
        } else if ((*_status)[n] == ODD) {
          _status->set(n, MATCHED);
          oddToMatched(n);
        }
      }
      _tree_set->eraseClass(tree);
    }


    void unmatchNode(const Node& node) {
      int tree = _tree_set->find(node);

      alternatePath(node, tree);
      destroyTree(tree);

      _matching->set(node, INVALID);
    }


    void augmentOnEdge(const Edge& edge) {
      Node left = _graph.u(edge);
      int left_tree = _tree_set->find(left);

      alternatePath(left, left_tree);
      destroyTree(left_tree);
      _matching->set(left, _graph.direct(edge, true));

      Node right = _graph.v(edge);
      int right_tree = _tree_set->find(right);

      alternatePath(right, right_tree);
      destroyTree(right_tree);
      _matching->set(right, _graph.direct(edge, false));
    }

    void augmentOnArc(const Arc& arc) {
      Node left = _graph.source(arc);
      _status->set(left, MATCHED);
      _matching->set(left, arc);
      _pred->set(left, arc);

      Node right = _graph.target(arc);
      int right_tree = _tree_set->find(right);

      alternatePath(right, right_tree);
      destroyTree(right_tree);
      _matching->set(right, _graph.oppositeArc(arc));
    }

    void extendOnArc(const Arc& arc) {
      Node base = _graph.target(arc);
      int tree = _tree_set->find(base);

      Node odd = _graph.source(arc);
      _tree_set->insert(odd, tree);
      _status->set(odd, ODD);
      matchedToOdd(odd, tree);
      _pred->set(odd, arc);

      Node even = _graph.target((*_matching)[odd]);
      _tree_set->insert(even, tree);
      _status->set(even, EVEN);
      matchedToEven(even, tree);
    }

    void cycleOnEdge(const Edge& edge, int tree) {
      Node nca = INVALID;
      std::vector<Node> left_path, right_path;

      {
        std::set<Node> left_set, right_set;
        Node left = _graph.u(edge);
        left_path.push_back(left);
        left_set.insert(left);

        Node right = _graph.v(edge);
        right_path.push_back(right);
        right_set.insert(right);

        while (true) {

          if (left_set.find(right) != left_set.end()) {
            nca = right;
            break;
          }

          if ((*_matching)[left] == INVALID) break;

          left = _graph.target((*_matching)[left]);
          left_path.push_back(left);
          left = _graph.target((*_pred)[left]);
          left_path.push_back(left);

          left_set.insert(left);

          if (right_set.find(left) != right_set.end()) {
            nca = left;
            break;
          }

          if ((*_matching)[right] == INVALID) break;

          right = _graph.target((*_matching)[right]);
          right_path.push_back(right);
          right = _graph.target((*_pred)[right]);
          right_path.push_back(right);

          right_set.insert(right);

        }

        if (nca == INVALID) {
          if ((*_matching)[left] == INVALID) {
            nca = right;
            while (left_set.find(nca) == left_set.end()) {
              nca = _graph.target((*_matching)[nca]);
              right_path.push_back(nca);
              nca = _graph.target((*_pred)[nca]);
              right_path.push_back(nca);
            }
          } else {
            nca = left;
            while (right_set.find(nca) == right_set.end()) {
              nca = _graph.target((*_matching)[nca]);
              left_path.push_back(nca);
              nca = _graph.target((*_pred)[nca]);
              left_path.push_back(nca);
            }
          }
        }
      }

      alternatePath(nca, tree);
      Arc prev;

      prev = _graph.direct(edge, true);
      for (int i = 0; left_path[i] != nca; i += 2) {
        _matching->set(left_path[i], prev);
        _status->set(left_path[i], MATCHED);
        evenToMatched(left_path[i], tree);

        prev = _graph.oppositeArc((*_pred)[left_path[i + 1]]);
        _status->set(left_path[i + 1], MATCHED);
        oddToMatched(left_path[i + 1]);
      }
      _matching->set(nca, prev);

      for (int i = 0; right_path[i] != nca; i += 2) {
        _status->set(right_path[i], MATCHED);
        evenToMatched(right_path[i], tree);

        _matching->set(right_path[i + 1], (*_pred)[right_path[i + 1]]);
        _status->set(right_path[i + 1], MATCHED);
        oddToMatched(right_path[i + 1]);
      }

      destroyTree(tree);
    }

    void extractCycle(const Arc &arc) {
      Node left = _graph.source(arc);
      Node odd = _graph.target((*_matching)[left]);
      Arc prev;
      while (odd != left) {
        Node even = _graph.target((*_matching)[odd]);
        prev = (*_matching)[odd];
        odd = _graph.target((*_matching)[even]);
        _matching->set(even, _graph.oppositeArc(prev));
      }
      _matching->set(left, arc);

      Node right = _graph.target(arc);
      int right_tree = _tree_set->find(right);
      alternatePath(right, right_tree);
      destroyTree(right_tree);
      _matching->set(right, _graph.oppositeArc(arc));
    }

  public:

    /// \brief Constructor
    ///
    /// Constructor.
    MaxWeightedFractionalMatching(const Graph& graph, const WeightMap& weight,
                                  bool allow_loops = true)
      : _graph(graph), _weight(weight), _matching(0),
      _node_potential(0), _node_num(0), _allow_loops(allow_loops),
      _status(0),  _pred(0),
      _tree_set_index(0), _tree_set(0),

      _delta1_index(0), _delta1(0),
      _delta2_index(0), _delta2(0),
      _delta3_index(0), _delta3(0),

      _delta_sum() {}

    ~MaxWeightedFractionalMatching() {
      destroyStructures();
    }

    /// \name Execution Control
    /// The simplest way to execute the algorithm is to use the
    /// \ref run() member function.

    ///@{

    /// \brief Initialize the algorithm
    ///
    /// This function initializes the algorithm.
    void init() {
      createStructures();

      for (NodeIt n(_graph); n != INVALID; ++n) {
        (*_delta1_index)[n] = _delta1->PRE_HEAP;
        (*_delta2_index)[n] = _delta2->PRE_HEAP;
      }
      for (EdgeIt e(_graph); e != INVALID; ++e) {
        (*_delta3_index)[e] = _delta3->PRE_HEAP;
      }

      _delta1->clear();
      _delta2->clear();
      _delta3->clear();
      _tree_set->clear();

      for (NodeIt n(_graph); n != INVALID; ++n) {
        Value max = 0;
        for (OutArcIt e(_graph, n); e != INVALID; ++e) {
          if (_graph.target(e) == n && !_allow_loops) continue;
          if ((dualScale * _weight[e]) / 2 > max) {
            max = (dualScale * _weight[e]) / 2;
          }
        }
        _node_potential->set(n, max);
        _delta1->push(n, max);

        _tree_set->insert(n);

        _matching->set(n, INVALID);
        _status->set(n, EVEN);
      }

      for (EdgeIt e(_graph); e != INVALID; ++e) {
        Node left = _graph.u(e);
        Node right = _graph.v(e);
        if (left == right && !_allow_loops) continue;
        _delta3->push(e, ((*_node_potential)[left] +
                          (*_node_potential)[right] -
                          dualScale * _weight[e]) / 2);
      }
    }

    /// \brief Start the algorithm
    ///
    /// This function starts the algorithm.
    ///
    /// \pre \ref init() must be called before using this function.
    void start() {
      enum OpType {
        D1, D2, D3
      };

      int unmatched = _node_num;
      while (unmatched > 0) {
        Value d1 = !_delta1->empty() ?
          _delta1->prio() : std::numeric_limits<Value>::max();

        Value d2 = !_delta2->empty() ?
          _delta2->prio() : std::numeric_limits<Value>::max();

        Value d3 = !_delta3->empty() ?
          _delta3->prio() : std::numeric_limits<Value>::max();

        _delta_sum = d3; OpType ot = D3;
        if (d1 < _delta_sum) { _delta_sum = d1; ot = D1; }
        if (d2 < _delta_sum) { _delta_sum = d2; ot = D2; }

        switch (ot) {
        case D1:
          {
            Node n = _delta1->top();
            unmatchNode(n);
            --unmatched;
          }
          break;
        case D2:
          {
            Node n = _delta2->top();
            Arc a = (*_pred)[n];
            if ((*_matching)[n] == INVALID) {
              augmentOnArc(a);
              --unmatched;
            } else {
              Node v = _graph.target((*_matching)[n]);
              if ((*_matching)[n] !=
                  _graph.oppositeArc((*_matching)[v])) {
                extractCycle(a);
                --unmatched;
              } else {
                extendOnArc(a);
              }
            }
          } break;
        case D3:
          {
            Edge e = _delta3->top();

            Node left = _graph.u(e);
            Node right = _graph.v(e);

            int left_tree = _tree_set->find(left);
            int right_tree = _tree_set->find(right);

            if (left_tree == right_tree) {
              cycleOnEdge(e, left_tree);
              --unmatched;
            } else {
              augmentOnEdge(e);
              unmatched -= 2;
            }
          } break;
        }
      }
    }

    /// \brief Run the algorithm.
    ///
    /// This method runs the \c %MaxWeightedFractionalMatching algorithm.
    ///
    /// \note mwfm.run() is just a shortcut of the following code.
    /// \code
    ///   mwfm.init();
    ///   mwfm.start();
    /// \endcode
    void run() {
      init();
      start();
    }

    /// @}

    /// \name Primal Solution
    /// Functions to get the primal solution, i.e. the maximum weighted
    /// matching.\n
    /// Either \ref run() or \ref start() function should be called before
    /// using them.

    /// @{

    /// \brief Return the weight of the matching.
    ///
    /// This function returns the weight of the found matching. This
    /// value is scaled by \ref primalScale "primal scale".
    ///
    /// \pre Either run() or start() must be called before using this function.
    Value matchingWeight() const {
      Value sum = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_matching)[n] != INVALID) {
          sum += _weight[(*_matching)[n]];
        }
      }
      return sum * primalScale / 2;
    }

    /// \brief Return the number of covered nodes in the matching.
    ///
    /// This function returns the number of covered nodes in the matching.
    ///
    /// \pre Either run() or start() must be called before using this function.
    int matchingSize() const {
      int num = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_matching)[n] != INVALID) {
          ++num;
        }
      }
      return num;
    }

    /// \brief Return \c true if the given edge is in the matching.
    ///
    /// This function returns \c true if the given edge is in the
    /// found matching. The result is scaled by \ref primalScale
    /// "primal scale".
    ///
    /// \pre Either run() or start() must be called before using this function.
    int matching(const Edge& edge) const {
      return (edge == (*_matching)[_graph.u(edge)] ? 1 : 0)
        + (edge == (*_matching)[_graph.v(edge)] ? 1 : 0);
    }

    /// \brief Return the fractional matching arc (or edge) incident
    /// to the given node.
    ///
    /// This function returns one of the fractional matching arc (or
    /// edge) incident to the given node in the found matching or \c
    /// INVALID if the node is not covered by the matching or if the
    /// node is on an odd length cycle then it is the successor edge
    /// on the cycle.
    ///
    /// \pre Either run() or start() must be called before using this function.
    Arc matching(const Node& node) const {
      return (*_matching)[node];
    }

    /// \brief Return a const reference to the matching map.
    ///
    /// This function returns a const reference to a node map that stores
    /// the matching arc (or edge) incident to each node.
    const MatchingMap& matchingMap() const {
      return *_matching;
    }

    /// @}

    /// \name Dual Solution
    /// Functions to get the dual solution.\n
    /// Either \ref run() or \ref start() function should be called before
    /// using them.

    /// @{

    /// \brief Return the value of the dual solution.
    ///
    /// This function returns the value of the dual solution.
    /// It should be equal to the primal value scaled by \ref dualScale
    /// "dual scale".
    ///
    /// \pre Either run() or start() must be called before using this function.
    Value dualValue() const {
      Value sum = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        sum += nodeValue(n);
      }
      return sum;
    }

    /// \brief Return the dual value (potential) of the given node.
    ///
    /// This function returns the dual value (potential) of the given node.
    ///
    /// \pre Either run() or start() must be called before using this function.
    Value nodeValue(const Node& n) const {
      return (*_node_potential)[n];
    }

    /// @}

  };

  /// \ingroup matching
  ///
  /// \brief Weighted fractional perfect matching in general graphs
  ///
  /// This class provides an efficient implementation of fractional
  /// matching algorithm. The implementation uses priority queues and
  /// provides \f$O(nm\log n)\f$ time complexity.
  ///
  /// The maximum weighted fractional perfect matching is a relaxation
  /// of the maximum weighted perfect matching problem where the odd
  /// set constraints are omitted.
  /// It can be formulated with the following linear program.
  /// \f[ \sum_{e \in \delta(u)}x_e = 1 \quad \forall u\in V\f]
  /// \f[x_e \ge 0\quad \forall e\in E\f]
  /// \f[\max \sum_{e\in E}x_ew_e\f]
  /// where \f$\delta(X)\f$ is the set of edges incident to a node in
  /// \f$X\f$. The result must be the union of a matching with one
  /// value edges and a set of odd length cycles with half value edges.
  ///
  /// The algorithm calculates an optimal fractional matching and a
  /// proof of the optimality. The solution of the dual problem can be
  /// used to check the result of the algorithm. The dual linear
  /// problem is the following.
  /// \f[ y_u + y_v \ge w_{uv} \quad \forall uv\in E\f]
  /// \f[\min \sum_{u \in V}y_u \f]
  ///
  /// The algorithm can be executed with the run() function.
  /// After it the matching (the primal solution) and the dual solution
  /// can be obtained using the query functions.
  ///
  /// The primal solution is multiplied by
  /// \ref MaxWeightedPerfectFractionalMatching::primalScale "2".
  /// If the value type is integer, then the dual
  /// solution is scaled by
  /// \ref MaxWeightedPerfectFractionalMatching::dualScale "4".
  ///
  /// \tparam GR The undirected graph type the algorithm runs on.
  /// \tparam WM The type edge weight map. The default type is
  /// \ref concepts::Graph::EdgeMap "GR::EdgeMap<int>".
#ifdef DOXYGEN
  template <typename GR, typename WM>
#else
  template <typename GR,
            typename WM = typename GR::template EdgeMap<int> >
#endif
  class MaxWeightedPerfectFractionalMatching {
  public:

    /// The graph type of the algorithm
    typedef GR Graph;
    /// The type of the edge weight map
    typedef WM WeightMap;
    /// The value type of the edge weights
    typedef typename WeightMap::Value Value;

    /// The type of the matching map
    typedef typename Graph::template NodeMap<typename Graph::Arc>
    MatchingMap;

    /// \brief Scaling factor for primal solution
    ///
    /// Scaling factor for primal solution.
    static const int primalScale = 2;

    /// \brief Scaling factor for dual solution
    ///
    /// Scaling factor for dual solution. It is equal to 4 or 1
    /// according to the value type.
    static const int dualScale =
      std::numeric_limits<Value>::is_integer ? 4 : 1;

  private:

    TEMPLATE_GRAPH_TYPEDEFS(Graph);

    typedef typename Graph::template NodeMap<Value> NodePotential;

    const Graph& _graph;
    const WeightMap& _weight;

    MatchingMap* _matching;
    NodePotential* _node_potential;

    int _node_num;
    bool _allow_loops;

    enum Status {
      EVEN = -1, MATCHED = 0, ODD = 1
    };

    typedef typename Graph::template NodeMap<Status> StatusMap;
    StatusMap* _status;

    typedef typename Graph::template NodeMap<Arc> PredMap;
    PredMap* _pred;

    typedef ExtendFindEnum<IntNodeMap> TreeSet;

    IntNodeMap *_tree_set_index;
    TreeSet *_tree_set;

    IntNodeMap *_delta2_index;
    BinHeap<Value, IntNodeMap> *_delta2;

    IntEdgeMap *_delta3_index;
    BinHeap<Value, IntEdgeMap> *_delta3;

    Value _delta_sum;

    void createStructures() {
      _node_num = countNodes(_graph);

      if (!_matching) {
        _matching = new MatchingMap(_graph);
      }
      if (!_node_potential) {
        _node_potential = new NodePotential(_graph);
      }
      if (!_status) {
        _status = new StatusMap(_graph);
      }
      if (!_pred) {
        _pred = new PredMap(_graph);
      }
      if (!_tree_set) {
        _tree_set_index = new IntNodeMap(_graph);
        _tree_set = new TreeSet(*_tree_set_index);
      }
      if (!_delta2) {
        _delta2_index = new IntNodeMap(_graph);
        _delta2 = new BinHeap<Value, IntNodeMap>(*_delta2_index);
      }
      if (!_delta3) {
        _delta3_index = new IntEdgeMap(_graph);
        _delta3 = new BinHeap<Value, IntEdgeMap>(*_delta3_index);
      }
    }

    void destroyStructures() {
      if (_matching) {
        delete _matching;
      }
      if (_node_potential) {
        delete _node_potential;
      }
      if (_status) {
        delete _status;
      }
      if (_pred) {
        delete _pred;
      }
      if (_tree_set) {
        delete _tree_set_index;
        delete _tree_set;
      }
      if (_delta2) {
        delete _delta2_index;
        delete _delta2;
      }
      if (_delta3) {
        delete _delta3_index;
        delete _delta3;
      }
    }

    void matchedToEven(Node node, int tree) {
      _tree_set->insert(node, tree);
      _node_potential->set(node, (*_node_potential)[node] + _delta_sum);

      if (_delta2->state(node) == _delta2->IN_HEAP) {
        _delta2->erase(node);
      }

      for (InArcIt a(_graph, node); a != INVALID; ++a) {
        Node v = _graph.source(a);
        Value rw = (*_node_potential)[node] + (*_node_potential)[v] -
          dualScale * _weight[a];
        if (node == v) {
          if (_allow_loops && _graph.direction(a)) {
            _delta3->push(a, rw / 2);
          }
        } else if ((*_status)[v] == EVEN) {
          _delta3->push(a, rw / 2);
        } else if ((*_status)[v] == MATCHED) {
          if (_delta2->state(v) != _delta2->IN_HEAP) {
            _pred->set(v, a);
            _delta2->push(v, rw);
          } else if ((*_delta2)[v] > rw) {
            _pred->set(v, a);
            _delta2->decrease(v, rw);
          }
        }
      }
    }

    void matchedToOdd(Node node, int tree) {
      _tree_set->insert(node, tree);
      _node_potential->set(node, (*_node_potential)[node] - _delta_sum);

      if (_delta2->state(node) == _delta2->IN_HEAP) {
        _delta2->erase(node);
      }
    }

    void evenToMatched(Node node, int tree) {
      _node_potential->set(node, (*_node_potential)[node] - _delta_sum);
      Arc min = INVALID;
      Value minrw = std::numeric_limits<Value>::max();
      for (InArcIt a(_graph, node); a != INVALID; ++a) {
        Node v = _graph.source(a);
        Value rw = (*_node_potential)[node] + (*_node_potential)[v] -
          dualScale * _weight[a];

        if (node == v) {
          if (_allow_loops && _graph.direction(a)) {
            _delta3->erase(a);
          }
        } else if ((*_status)[v] == EVEN) {
          _delta3->erase(a);
          if (minrw > rw) {
            min = _graph.oppositeArc(a);
            minrw = rw;
          }
        } else if ((*_status)[v]  == MATCHED) {
          if ((*_pred)[v] == a) {
            Arc mina = INVALID;
            Value minrwa = std::numeric_limits<Value>::max();
            for (OutArcIt aa(_graph, v); aa != INVALID; ++aa) {
              Node va = _graph.target(aa);
              if ((*_status)[va] != EVEN ||
                  _tree_set->find(va) == tree) continue;
              Value rwa = (*_node_potential)[v] + (*_node_potential)[va] -
                dualScale * _weight[aa];
              if (minrwa > rwa) {
                minrwa = rwa;
                mina = aa;
              }
            }
            if (mina != INVALID) {
              _pred->set(v, mina);
              _delta2->increase(v, minrwa);
            } else {
              _pred->set(v, INVALID);
              _delta2->erase(v);
            }
          }
        }
      }
      if (min != INVALID) {
        _pred->set(node, min);
        _delta2->push(node, minrw);
      } else {
        _pred->set(node, INVALID);
      }
    }

    void oddToMatched(Node node) {
      _node_potential->set(node, (*_node_potential)[node] + _delta_sum);
      Arc min = INVALID;
      Value minrw = std::numeric_limits<Value>::max();
      for (InArcIt a(_graph, node); a != INVALID; ++a) {
        Node v = _graph.source(a);
        if ((*_status)[v] != EVEN) continue;
        Value rw = (*_node_potential)[node] + (*_node_potential)[v] -
          dualScale * _weight[a];

        if (minrw > rw) {
          min = _graph.oppositeArc(a);
          minrw = rw;
        }
      }
      if (min != INVALID) {
        _pred->set(node, min);
        _delta2->push(node, minrw);
      } else {
        _pred->set(node, INVALID);
      }
    }

    void alternatePath(Node even, int tree) {
      Node odd;

      _status->set(even, MATCHED);
      evenToMatched(even, tree);

      Arc prev = (*_matching)[even];
      while (prev != INVALID) {
        odd = _graph.target(prev);
        even = _graph.target((*_pred)[odd]);
        _matching->set(odd, (*_pred)[odd]);
        _status->set(odd, MATCHED);
        oddToMatched(odd);

        prev = (*_matching)[even];
        _status->set(even, MATCHED);
        _matching->set(even, _graph.oppositeArc((*_matching)[odd]));
        evenToMatched(even, tree);
      }
    }

    void destroyTree(int tree) {
      for (typename TreeSet::ItemIt n(*_tree_set, tree); n != INVALID; ++n) {
        if ((*_status)[n] == EVEN) {
          _status->set(n, MATCHED);
          evenToMatched(n, tree);
        } else if ((*_status)[n] == ODD) {
          _status->set(n, MATCHED);
          oddToMatched(n);
        }
      }
      _tree_set->eraseClass(tree);
    }

    void augmentOnEdge(const Edge& edge) {
      Node left = _graph.u(edge);
      int left_tree = _tree_set->find(left);

      alternatePath(left, left_tree);
      destroyTree(left_tree);
      _matching->set(left, _graph.direct(edge, true));

      Node right = _graph.v(edge);
      int right_tree = _tree_set->find(right);

      alternatePath(right, right_tree);
      destroyTree(right_tree);
      _matching->set(right, _graph.direct(edge, false));
    }

    void augmentOnArc(const Arc& arc) {
      Node left = _graph.source(arc);
      _status->set(left, MATCHED);
      _matching->set(left, arc);
      _pred->set(left, arc);

      Node right = _graph.target(arc);
      int right_tree = _tree_set->find(right);

      alternatePath(right, right_tree);
      destroyTree(right_tree);
      _matching->set(right, _graph.oppositeArc(arc));
    }

    void extendOnArc(const Arc& arc) {
      Node base = _graph.target(arc);
      int tree = _tree_set->find(base);

      Node odd = _graph.source(arc);
      _tree_set->insert(odd, tree);
      _status->set(odd, ODD);
      matchedToOdd(odd, tree);
      _pred->set(odd, arc);

      Node even = _graph.target((*_matching)[odd]);
      _tree_set->insert(even, tree);
      _status->set(even, EVEN);
      matchedToEven(even, tree);
    }

    void cycleOnEdge(const Edge& edge, int tree) {
      Node nca = INVALID;
      std::vector<Node> left_path, right_path;

      {
        std::set<Node> left_set, right_set;
        Node left = _graph.u(edge);
        left_path.push_back(left);
        left_set.insert(left);

        Node right = _graph.v(edge);
        right_path.push_back(right);
        right_set.insert(right);

        while (true) {

          if (left_set.find(right) != left_set.end()) {
            nca = right;
            break;
          }

          if ((*_matching)[left] == INVALID) break;

          left = _graph.target((*_matching)[left]);
          left_path.push_back(left);
          left = _graph.target((*_pred)[left]);
          left_path.push_back(left);

          left_set.insert(left);

          if (right_set.find(left) != right_set.end()) {
            nca = left;
            break;
          }

          if ((*_matching)[right] == INVALID) break;

          right = _graph.target((*_matching)[right]);
          right_path.push_back(right);
          right = _graph.target((*_pred)[right]);
          right_path.push_back(right);

          right_set.insert(right);

        }

        if (nca == INVALID) {
          if ((*_matching)[left] == INVALID) {
            nca = right;
            while (left_set.find(nca) == left_set.end()) {
              nca = _graph.target((*_matching)[nca]);
              right_path.push_back(nca);
              nca = _graph.target((*_pred)[nca]);
              right_path.push_back(nca);
            }
          } else {
            nca = left;
            while (right_set.find(nca) == right_set.end()) {
              nca = _graph.target((*_matching)[nca]);
              left_path.push_back(nca);
              nca = _graph.target((*_pred)[nca]);
              left_path.push_back(nca);
            }
          }
        }
      }

      alternatePath(nca, tree);
      Arc prev;

      prev = _graph.direct(edge, true);
      for (int i = 0; left_path[i] != nca; i += 2) {
        _matching->set(left_path[i], prev);
        _status->set(left_path[i], MATCHED);
        evenToMatched(left_path[i], tree);

        prev = _graph.oppositeArc((*_pred)[left_path[i + 1]]);
        _status->set(left_path[i + 1], MATCHED);
        oddToMatched(left_path[i + 1]);
      }
      _matching->set(nca, prev);

      for (int i = 0; right_path[i] != nca; i += 2) {
        _status->set(right_path[i], MATCHED);
        evenToMatched(right_path[i], tree);

        _matching->set(right_path[i + 1], (*_pred)[right_path[i + 1]]);
        _status->set(right_path[i + 1], MATCHED);
        oddToMatched(right_path[i + 1]);
      }

      destroyTree(tree);
    }

    void extractCycle(const Arc &arc) {
      Node left = _graph.source(arc);
      Node odd = _graph.target((*_matching)[left]);
      Arc prev;
      while (odd != left) {
        Node even = _graph.target((*_matching)[odd]);
        prev = (*_matching)[odd];
        odd = _graph.target((*_matching)[even]);
        _matching->set(even, _graph.oppositeArc(prev));
      }
      _matching->set(left, arc);

      Node right = _graph.target(arc);
      int right_tree = _tree_set->find(right);
      alternatePath(right, right_tree);
      destroyTree(right_tree);
      _matching->set(right, _graph.oppositeArc(arc));
    }

  public:

    /// \brief Constructor
    ///
    /// Constructor.
    MaxWeightedPerfectFractionalMatching(const Graph& graph,
                                         const WeightMap& weight,
                                         bool allow_loops = true)
      : _graph(graph), _weight(weight), _matching(0),
      _node_potential(0), _node_num(0), _allow_loops(allow_loops),
      _status(0),  _pred(0),
      _tree_set_index(0), _tree_set(0),

      _delta2_index(0), _delta2(0),
      _delta3_index(0), _delta3(0),

      _delta_sum() {}

    ~MaxWeightedPerfectFractionalMatching() {
      destroyStructures();
    }

    /// \name Execution Control
    /// The simplest way to execute the algorithm is to use the
    /// \ref run() member function.

    ///@{

    /// \brief Initialize the algorithm
    ///
    /// This function initializes the algorithm.
    void init() {
      createStructures();

      for (NodeIt n(_graph); n != INVALID; ++n) {
        (*_delta2_index)[n] = _delta2->PRE_HEAP;
      }
      for (EdgeIt e(_graph); e != INVALID; ++e) {
        (*_delta3_index)[e] = _delta3->PRE_HEAP;
      }

      _delta2->clear();
      _delta3->clear();
      _tree_set->clear();

      for (NodeIt n(_graph); n != INVALID; ++n) {
        Value max = - std::numeric_limits<Value>::max();
        for (OutArcIt e(_graph, n); e != INVALID; ++e) {
          if (_graph.target(e) == n && !_allow_loops) continue;
          if ((dualScale * _weight[e]) / 2 > max) {
            max = (dualScale * _weight[e]) / 2;
          }
        }
        _node_potential->set(n, max);

        _tree_set->insert(n);

        _matching->set(n, INVALID);
        _status->set(n, EVEN);
      }

      for (EdgeIt e(_graph); e != INVALID; ++e) {
        Node left = _graph.u(e);
        Node right = _graph.v(e);
        if (left == right && !_allow_loops) continue;
        _delta3->push(e, ((*_node_potential)[left] +
                          (*_node_potential)[right] -
                          dualScale * _weight[e]) / 2);
      }
    }

    /// \brief Start the algorithm
    ///
    /// This function starts the algorithm.
    ///
    /// \pre \ref init() must be called before using this function.
    bool start() {
      enum OpType {
        D2, D3
      };

      int unmatched = _node_num;
      while (unmatched > 0) {
        Value d2 = !_delta2->empty() ?
          _delta2->prio() : std::numeric_limits<Value>::max();

        Value d3 = !_delta3->empty() ?
          _delta3->prio() : std::numeric_limits<Value>::max();

        _delta_sum = d3; OpType ot = D3;
        if (d2 < _delta_sum) { _delta_sum = d2; ot = D2; }

        if (_delta_sum == std::numeric_limits<Value>::max()) {
          return false;
        }

        switch (ot) {
        case D2:
          {
            Node n = _delta2->top();
            Arc a = (*_pred)[n];
            if ((*_matching)[n] == INVALID) {
              augmentOnArc(a);
              --unmatched;
            } else {
              Node v = _graph.target((*_matching)[n]);
              if ((*_matching)[n] !=
                  _graph.oppositeArc((*_matching)[v])) {
                extractCycle(a);
                --unmatched;
              } else {
                extendOnArc(a);
              }
            }
          } break;
        case D3:
          {
            Edge e = _delta3->top();

            Node left = _graph.u(e);
            Node right = _graph.v(e);

            int left_tree = _tree_set->find(left);
            int right_tree = _tree_set->find(right);

            if (left_tree == right_tree) {
              cycleOnEdge(e, left_tree);
              --unmatched;
            } else {
              augmentOnEdge(e);
              unmatched -= 2;
            }
          } break;
        }
      }
      return true;
    }

    /// \brief Run the algorithm.
    ///
    /// This method runs the \c %MaxWeightedPerfectFractionalMatching
    /// algorithm.
    ///
    /// \note mwfm.run() is just a shortcut of the following code.
    /// \code
    ///   mwpfm.init();
    ///   mwpfm.start();
    /// \endcode
    bool run() {
      init();
      return start();
    }

    /// @}

    /// \name Primal Solution
    /// Functions to get the primal solution, i.e. the maximum weighted
    /// matching.\n
    /// Either \ref run() or \ref start() function should be called before
    /// using them.

    /// @{

    /// \brief Return the weight of the matching.
    ///
    /// This function returns the weight of the found matching. This
    /// value is scaled by \ref primalScale "primal scale".
    ///
    /// \pre Either run() or start() must be called before using this function.
    Value matchingWeight() const {
      Value sum = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_matching)[n] != INVALID) {
          sum += _weight[(*_matching)[n]];
        }
      }
      return sum * primalScale / 2;
    }

    /// \brief Return the number of covered nodes in the matching.
    ///
    /// This function returns the number of covered nodes in the matching.
    ///
    /// \pre Either run() or start() must be called before using this function.
    int matchingSize() const {
      int num = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_matching)[n] != INVALID) {
          ++num;
        }
      }
      return num;
    }

    /// \brief Return \c true if the given edge is in the matching.
    ///
    /// This function returns \c true if the given edge is in the
    /// found matching. The result is scaled by \ref primalScale
    /// "primal scale".
    ///
    /// \pre Either run() or start() must be called before using this function.
    int matching(const Edge& edge) const {
      return (edge == (*_matching)[_graph.u(edge)] ? 1 : 0)
        + (edge == (*_matching)[_graph.v(edge)] ? 1 : 0);
    }

    /// \brief Return the fractional matching arc (or edge) incident
    /// to the given node.
    ///
    /// This function returns one of the fractional matching arc (or
    /// edge) incident to the given node in the found matching or \c
    /// INVALID if the node is not covered by the matching or if the
    /// node is on an odd length cycle then it is the successor edge
    /// on the cycle.
    ///
    /// \pre Either run() or start() must be called before using this function.
    Arc matching(const Node& node) const {
      return (*_matching)[node];
    }

    /// \brief Return a const reference to the matching map.
    ///
    /// This function returns a const reference to a node map that stores
    /// the matching arc (or edge) incident to each node.
    const MatchingMap& matchingMap() const {
      return *_matching;
    }

    /// @}

    /// \name Dual Solution
    /// Functions to get the dual solution.\n
    /// Either \ref run() or \ref start() function should be called before
    /// using them.

    /// @{

    /// \brief Return the value of the dual solution.
    ///
    /// This function returns the value of the dual solution.
    /// It should be equal to the primal value scaled by \ref dualScale
    /// "dual scale".
    ///
    /// \pre Either run() or start() must be called before using this function.
    Value dualValue() const {
      Value sum = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        sum += nodeValue(n);
      }
      return sum;
    }

    /// \brief Return the dual value (potential) of the given node.
    ///
    /// This function returns the dual value (potential) of the given node.
    ///
    /// \pre Either run() or start() must be called before using this function.
    Value nodeValue(const Node& n) const {
      return (*_node_potential)[n];
    }

    /// @}

  };

} //END OF NAMESPACE LEMON

#endif //LEMON_FRACTIONAL_MATCHING_H
