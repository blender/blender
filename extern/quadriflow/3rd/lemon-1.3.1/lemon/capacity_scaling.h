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

#ifndef LEMON_CAPACITY_SCALING_H
#define LEMON_CAPACITY_SCALING_H

/// \ingroup min_cost_flow_algs
///
/// \file
/// \brief Capacity Scaling algorithm for finding a minimum cost flow.

#include <vector>
#include <limits>
#include <lemon/core.h>
#include <lemon/bin_heap.h>

namespace lemon {

  /// \brief Default traits class of CapacityScaling algorithm.
  ///
  /// Default traits class of CapacityScaling algorithm.
  /// \tparam GR Digraph type.
  /// \tparam V The number type used for flow amounts, capacity bounds
  /// and supply values. By default it is \c int.
  /// \tparam C The number type used for costs and potentials.
  /// By default it is the same as \c V.
  template <typename GR, typename V = int, typename C = V>
  struct CapacityScalingDefaultTraits
  {
    /// The type of the digraph
    typedef GR Digraph;
    /// The type of the flow amounts, capacity bounds and supply values
    typedef V Value;
    /// The type of the arc costs
    typedef C Cost;

    /// \brief The type of the heap used for internal Dijkstra computations.
    ///
    /// The type of the heap used for internal Dijkstra computations.
    /// It must conform to the \ref lemon::concepts::Heap "Heap" concept,
    /// its priority type must be \c Cost and its cross reference type
    /// must be \ref RangeMap "RangeMap<int>".
    typedef BinHeap<Cost, RangeMap<int> > Heap;
  };

  /// \addtogroup min_cost_flow_algs
  /// @{

  /// \brief Implementation of the Capacity Scaling algorithm for
  /// finding a \ref min_cost_flow "minimum cost flow".
  ///
  /// \ref CapacityScaling implements the capacity scaling version
  /// of the successive shortest path algorithm for finding a
  /// \ref min_cost_flow "minimum cost flow" \cite amo93networkflows,
  /// \cite edmondskarp72theoretical. It is an efficient dual
  /// solution method, which runs in polynomial time
  /// \f$O(m\log U (n+m)\log n)\f$, where <i>U</i> denotes the maximum
  /// of node supply and arc capacity values.
  ///
  /// This algorithm is typically slower than \ref CostScaling and
  /// \ref NetworkSimplex, but in special cases, it can be more
  /// efficient than them.
  /// (For more information, see \ref min_cost_flow_algs "the module page".)
  ///
  /// Most of the parameters of the problem (except for the digraph)
  /// can be given using separate functions, and the algorithm can be
  /// executed using the \ref run() function. If some parameters are not
  /// specified, then default values will be used.
  ///
  /// \tparam GR The digraph type the algorithm runs on.
  /// \tparam V The number type used for flow amounts, capacity bounds
  /// and supply values in the algorithm. By default, it is \c int.
  /// \tparam C The number type used for costs and potentials in the
  /// algorithm. By default, it is the same as \c V.
  /// \tparam TR The traits class that defines various types used by the
  /// algorithm. By default, it is \ref CapacityScalingDefaultTraits
  /// "CapacityScalingDefaultTraits<GR, V, C>".
  /// In most cases, this parameter should not be set directly,
  /// consider to use the named template parameters instead.
  ///
  /// \warning Both \c V and \c C must be signed number types.
  /// \warning Capacity bounds and supply values must be integer, but
  /// arc costs can be arbitrary real numbers.
  /// \warning This algorithm does not support negative costs for
  /// arcs having infinite upper bound.
#ifdef DOXYGEN
  template <typename GR, typename V, typename C, typename TR>
#else
  template < typename GR, typename V = int, typename C = V,
             typename TR = CapacityScalingDefaultTraits<GR, V, C> >
#endif
  class CapacityScaling
  {
  public:

    /// The type of the digraph
    typedef typename TR::Digraph Digraph;
    /// The type of the flow amounts, capacity bounds and supply values
    typedef typename TR::Value Value;
    /// The type of the arc costs
    typedef typename TR::Cost Cost;

    /// The type of the heap used for internal Dijkstra computations
    typedef typename TR::Heap Heap;

    /// \brief The \ref lemon::CapacityScalingDefaultTraits "traits class"
    /// of the algorithm
    typedef TR Traits;

  public:

    /// \brief Problem type constants for the \c run() function.
    ///
    /// Enum type containing the problem type constants that can be
    /// returned by the \ref run() function of the algorithm.
    enum ProblemType {
      /// The problem has no feasible solution (flow).
      INFEASIBLE,
      /// The problem has optimal solution (i.e. it is feasible and
      /// bounded), and the algorithm has found optimal flow and node
      /// potentials (primal and dual solutions).
      OPTIMAL,
      /// The digraph contains an arc of negative cost and infinite
      /// upper bound. It means that the objective function is unbounded
      /// on that arc, however, note that it could actually be bounded
      /// over the feasible flows, but this algroithm cannot handle
      /// these cases.
      UNBOUNDED
    };

  private:

    TEMPLATE_DIGRAPH_TYPEDEFS(GR);

    typedef std::vector<int> IntVector;
    typedef std::vector<Value> ValueVector;
    typedef std::vector<Cost> CostVector;
    typedef std::vector<char> BoolVector;
    // Note: vector<char> is used instead of vector<bool> for efficiency reasons

  private:

    // Data related to the underlying digraph
    const GR &_graph;
    int _node_num;
    int _arc_num;
    int _res_arc_num;
    int _root;

    // Parameters of the problem
    bool _has_lower;
    Value _sum_supply;

    // Data structures for storing the digraph
    IntNodeMap _node_id;
    IntArcMap _arc_idf;
    IntArcMap _arc_idb;
    IntVector _first_out;
    BoolVector _forward;
    IntVector _source;
    IntVector _target;
    IntVector _reverse;

    // Node and arc data
    ValueVector _lower;
    ValueVector _upper;
    CostVector _cost;
    ValueVector _supply;

    ValueVector _res_cap;
    CostVector _pi;
    ValueVector _excess;
    IntVector _excess_nodes;
    IntVector _deficit_nodes;

    Value _delta;
    int _factor;
    IntVector _pred;

  public:

    /// \brief Constant for infinite upper bounds (capacities).
    ///
    /// Constant for infinite upper bounds (capacities).
    /// It is \c std::numeric_limits<Value>::infinity() if available,
    /// \c std::numeric_limits<Value>::max() otherwise.
    const Value INF;

  private:

    // Special implementation of the Dijkstra algorithm for finding
    // shortest paths in the residual network of the digraph with
    // respect to the reduced arc costs and modifying the node
    // potentials according to the found distance labels.
    class ResidualDijkstra
    {
    private:

      int _node_num;
      bool _geq;
      const IntVector &_first_out;
      const IntVector &_target;
      const CostVector &_cost;
      const ValueVector &_res_cap;
      const ValueVector &_excess;
      CostVector &_pi;
      IntVector &_pred;

      IntVector _proc_nodes;
      CostVector _dist;

    public:

      ResidualDijkstra(CapacityScaling& cs) :
        _node_num(cs._node_num), _geq(cs._sum_supply < 0),
        _first_out(cs._first_out), _target(cs._target), _cost(cs._cost),
        _res_cap(cs._res_cap), _excess(cs._excess), _pi(cs._pi),
        _pred(cs._pred), _dist(cs._node_num)
      {}

      int run(int s, Value delta = 1) {
        RangeMap<int> heap_cross_ref(_node_num, Heap::PRE_HEAP);
        Heap heap(heap_cross_ref);
        heap.push(s, 0);
        _pred[s] = -1;
        _proc_nodes.clear();

        // Process nodes
        while (!heap.empty() && _excess[heap.top()] > -delta) {
          int u = heap.top(), v;
          Cost d = heap.prio() + _pi[u], dn;
          _dist[u] = heap.prio();
          _proc_nodes.push_back(u);
          heap.pop();

          // Traverse outgoing residual arcs
          int last_out = _geq ? _first_out[u+1] : _first_out[u+1] - 1;
          for (int a = _first_out[u]; a != last_out; ++a) {
            if (_res_cap[a] < delta) continue;
            v = _target[a];
            switch (heap.state(v)) {
              case Heap::PRE_HEAP:
                heap.push(v, d + _cost[a] - _pi[v]);
                _pred[v] = a;
                break;
              case Heap::IN_HEAP:
                dn = d + _cost[a] - _pi[v];
                if (dn < heap[v]) {
                  heap.decrease(v, dn);
                  _pred[v] = a;
                }
                break;
              case Heap::POST_HEAP:
                break;
            }
          }
        }
        if (heap.empty()) return -1;

        // Update potentials of processed nodes
        int t = heap.top();
        Cost dt = heap.prio();
        for (int i = 0; i < int(_proc_nodes.size()); ++i) {
          _pi[_proc_nodes[i]] += _dist[_proc_nodes[i]] - dt;
        }

        return t;
      }

    }; //class ResidualDijkstra

  public:

    /// \name Named Template Parameters
    /// @{

    template <typename T>
    struct SetHeapTraits : public Traits {
      typedef T Heap;
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// \c Heap type.
    ///
    /// \ref named-templ-param "Named parameter" for setting \c Heap
    /// type, which is used for internal Dijkstra computations.
    /// It must conform to the \ref lemon::concepts::Heap "Heap" concept,
    /// its priority type must be \c Cost and its cross reference type
    /// must be \ref RangeMap "RangeMap<int>".
    template <typename T>
    struct SetHeap
      : public CapacityScaling<GR, V, C, SetHeapTraits<T> > {
      typedef  CapacityScaling<GR, V, C, SetHeapTraits<T> > Create;
    };

    /// @}

  protected:

    CapacityScaling() {}

  public:

    /// \brief Constructor.
    ///
    /// The constructor of the class.
    ///
    /// \param graph The digraph the algorithm runs on.
    CapacityScaling(const GR& graph) :
      _graph(graph), _node_id(graph), _arc_idf(graph), _arc_idb(graph),
      INF(std::numeric_limits<Value>::has_infinity ?
          std::numeric_limits<Value>::infinity() :
          std::numeric_limits<Value>::max())
    {
      // Check the number types
      LEMON_ASSERT(std::numeric_limits<Value>::is_signed,
        "The flow type of CapacityScaling must be signed");
      LEMON_ASSERT(std::numeric_limits<Cost>::is_signed,
        "The cost type of CapacityScaling must be signed");

      // Reset data structures
      reset();
    }

    /// \name Parameters
    /// The parameters of the algorithm can be specified using these
    /// functions.

    /// @{

    /// \brief Set the lower bounds on the arcs.
    ///
    /// This function sets the lower bounds on the arcs.
    /// If it is not used before calling \ref run(), the lower bounds
    /// will be set to zero on all arcs.
    ///
    /// \param map An arc map storing the lower bounds.
    /// Its \c Value type must be convertible to the \c Value type
    /// of the algorithm.
    ///
    /// \return <tt>(*this)</tt>
    template <typename LowerMap>
    CapacityScaling& lowerMap(const LowerMap& map) {
      _has_lower = true;
      for (ArcIt a(_graph); a != INVALID; ++a) {
        _lower[_arc_idf[a]] = map[a];
      }
      return *this;
    }

    /// \brief Set the upper bounds (capacities) on the arcs.
    ///
    /// This function sets the upper bounds (capacities) on the arcs.
    /// If it is not used before calling \ref run(), the upper bounds
    /// will be set to \ref INF on all arcs (i.e. the flow value will be
    /// unbounded from above).
    ///
    /// \param map An arc map storing the upper bounds.
    /// Its \c Value type must be convertible to the \c Value type
    /// of the algorithm.
    ///
    /// \return <tt>(*this)</tt>
    template<typename UpperMap>
    CapacityScaling& upperMap(const UpperMap& map) {
      for (ArcIt a(_graph); a != INVALID; ++a) {
        _upper[_arc_idf[a]] = map[a];
      }
      return *this;
    }

    /// \brief Set the costs of the arcs.
    ///
    /// This function sets the costs of the arcs.
    /// If it is not used before calling \ref run(), the costs
    /// will be set to \c 1 on all arcs.
    ///
    /// \param map An arc map storing the costs.
    /// Its \c Value type must be convertible to the \c Cost type
    /// of the algorithm.
    ///
    /// \return <tt>(*this)</tt>
    template<typename CostMap>
    CapacityScaling& costMap(const CostMap& map) {
      for (ArcIt a(_graph); a != INVALID; ++a) {
        _cost[_arc_idf[a]] =  map[a];
        _cost[_arc_idb[a]] = -map[a];
      }
      return *this;
    }

    /// \brief Set the supply values of the nodes.
    ///
    /// This function sets the supply values of the nodes.
    /// If neither this function nor \ref stSupply() is used before
    /// calling \ref run(), the supply of each node will be set to zero.
    ///
    /// \param map A node map storing the supply values.
    /// Its \c Value type must be convertible to the \c Value type
    /// of the algorithm.
    ///
    /// \return <tt>(*this)</tt>
    template<typename SupplyMap>
    CapacityScaling& supplyMap(const SupplyMap& map) {
      for (NodeIt n(_graph); n != INVALID; ++n) {
        _supply[_node_id[n]] = map[n];
      }
      return *this;
    }

    /// \brief Set single source and target nodes and a supply value.
    ///
    /// This function sets a single source node and a single target node
    /// and the required flow value.
    /// If neither this function nor \ref supplyMap() is used before
    /// calling \ref run(), the supply of each node will be set to zero.
    ///
    /// Using this function has the same effect as using \ref supplyMap()
    /// with a map in which \c k is assigned to \c s, \c -k is
    /// assigned to \c t and all other nodes have zero supply value.
    ///
    /// \param s The source node.
    /// \param t The target node.
    /// \param k The required amount of flow from node \c s to node \c t
    /// (i.e. the supply of \c s and the demand of \c t).
    ///
    /// \return <tt>(*this)</tt>
    CapacityScaling& stSupply(const Node& s, const Node& t, Value k) {
      for (int i = 0; i != _node_num; ++i) {
        _supply[i] = 0;
      }
      _supply[_node_id[s]] =  k;
      _supply[_node_id[t]] = -k;
      return *this;
    }

    /// @}

    /// \name Execution control
    /// The algorithm can be executed using \ref run().

    /// @{

    /// \brief Run the algorithm.
    ///
    /// This function runs the algorithm.
    /// The paramters can be specified using functions \ref lowerMap(),
    /// \ref upperMap(), \ref costMap(), \ref supplyMap(), \ref stSupply().
    /// For example,
    /// \code
    ///   CapacityScaling<ListDigraph> cs(graph);
    ///   cs.lowerMap(lower).upperMap(upper).costMap(cost)
    ///     .supplyMap(sup).run();
    /// \endcode
    ///
    /// This function can be called more than once. All the given parameters
    /// are kept for the next call, unless \ref resetParams() or \ref reset()
    /// is used, thus only the modified parameters have to be set again.
    /// If the underlying digraph was also modified after the construction
    /// of the class (or the last \ref reset() call), then the \ref reset()
    /// function must be called.
    ///
    /// \param factor The capacity scaling factor. It must be larger than
    /// one to use scaling. If it is less or equal to one, then scaling
    /// will be disabled.
    ///
    /// \return \c INFEASIBLE if no feasible flow exists,
    /// \n \c OPTIMAL if the problem has optimal solution
    /// (i.e. it is feasible and bounded), and the algorithm has found
    /// optimal flow and node potentials (primal and dual solutions),
    /// \n \c UNBOUNDED if the digraph contains an arc of negative cost
    /// and infinite upper bound. It means that the objective function
    /// is unbounded on that arc, however, note that it could actually be
    /// bounded over the feasible flows, but this algroithm cannot handle
    /// these cases.
    ///
    /// \see ProblemType
    /// \see resetParams(), reset()
    ProblemType run(int factor = 4) {
      _factor = factor;
      ProblemType pt = init();
      if (pt != OPTIMAL) return pt;
      return start();
    }

    /// \brief Reset all the parameters that have been given before.
    ///
    /// This function resets all the paramaters that have been given
    /// before using functions \ref lowerMap(), \ref upperMap(),
    /// \ref costMap(), \ref supplyMap(), \ref stSupply().
    ///
    /// It is useful for multiple \ref run() calls. Basically, all the given
    /// parameters are kept for the next \ref run() call, unless
    /// \ref resetParams() or \ref reset() is used.
    /// If the underlying digraph was also modified after the construction
    /// of the class or the last \ref reset() call, then the \ref reset()
    /// function must be used, otherwise \ref resetParams() is sufficient.
    ///
    /// For example,
    /// \code
    ///   CapacityScaling<ListDigraph> cs(graph);
    ///
    ///   // First run
    ///   cs.lowerMap(lower).upperMap(upper).costMap(cost)
    ///     .supplyMap(sup).run();
    ///
    ///   // Run again with modified cost map (resetParams() is not called,
    ///   // so only the cost map have to be set again)
    ///   cost[e] += 100;
    ///   cs.costMap(cost).run();
    ///
    ///   // Run again from scratch using resetParams()
    ///   // (the lower bounds will be set to zero on all arcs)
    ///   cs.resetParams();
    ///   cs.upperMap(capacity).costMap(cost)
    ///     .supplyMap(sup).run();
    /// \endcode
    ///
    /// \return <tt>(*this)</tt>
    ///
    /// \see reset(), run()
    CapacityScaling& resetParams() {
      for (int i = 0; i != _node_num; ++i) {
        _supply[i] = 0;
      }
      for (int j = 0; j != _res_arc_num; ++j) {
        _lower[j] = 0;
        _upper[j] = INF;
        _cost[j] = _forward[j] ? 1 : -1;
      }
      _has_lower = false;
      return *this;
    }

    /// \brief Reset the internal data structures and all the parameters
    /// that have been given before.
    ///
    /// This function resets the internal data structures and all the
    /// paramaters that have been given before using functions \ref lowerMap(),
    /// \ref upperMap(), \ref costMap(), \ref supplyMap(), \ref stSupply().
    ///
    /// It is useful for multiple \ref run() calls. Basically, all the given
    /// parameters are kept for the next \ref run() call, unless
    /// \ref resetParams() or \ref reset() is used.
    /// If the underlying digraph was also modified after the construction
    /// of the class or the last \ref reset() call, then the \ref reset()
    /// function must be used, otherwise \ref resetParams() is sufficient.
    ///
    /// See \ref resetParams() for examples.
    ///
    /// \return <tt>(*this)</tt>
    ///
    /// \see resetParams(), run()
    CapacityScaling& reset() {
      // Resize vectors
      _node_num = countNodes(_graph);
      _arc_num = countArcs(_graph);
      _res_arc_num = 2 * (_arc_num + _node_num);
      _root = _node_num;
      ++_node_num;

      _first_out.resize(_node_num + 1);
      _forward.resize(_res_arc_num);
      _source.resize(_res_arc_num);
      _target.resize(_res_arc_num);
      _reverse.resize(_res_arc_num);

      _lower.resize(_res_arc_num);
      _upper.resize(_res_arc_num);
      _cost.resize(_res_arc_num);
      _supply.resize(_node_num);

      _res_cap.resize(_res_arc_num);
      _pi.resize(_node_num);
      _excess.resize(_node_num);
      _pred.resize(_node_num);

      // Copy the graph
      int i = 0, j = 0, k = 2 * _arc_num + _node_num - 1;
      for (NodeIt n(_graph); n != INVALID; ++n, ++i) {
        _node_id[n] = i;
      }
      i = 0;
      for (NodeIt n(_graph); n != INVALID; ++n, ++i) {
        _first_out[i] = j;
        for (OutArcIt a(_graph, n); a != INVALID; ++a, ++j) {
          _arc_idf[a] = j;
          _forward[j] = true;
          _source[j] = i;
          _target[j] = _node_id[_graph.runningNode(a)];
        }
        for (InArcIt a(_graph, n); a != INVALID; ++a, ++j) {
          _arc_idb[a] = j;
          _forward[j] = false;
          _source[j] = i;
          _target[j] = _node_id[_graph.runningNode(a)];
        }
        _forward[j] = false;
        _source[j] = i;
        _target[j] = _root;
        _reverse[j] = k;
        _forward[k] = true;
        _source[k] = _root;
        _target[k] = i;
        _reverse[k] = j;
        ++j; ++k;
      }
      _first_out[i] = j;
      _first_out[_node_num] = k;
      for (ArcIt a(_graph); a != INVALID; ++a) {
        int fi = _arc_idf[a];
        int bi = _arc_idb[a];
        _reverse[fi] = bi;
        _reverse[bi] = fi;
      }

      // Reset parameters
      resetParams();
      return *this;
    }

    /// @}

    /// \name Query Functions
    /// The results of the algorithm can be obtained using these
    /// functions.\n
    /// The \ref run() function must be called before using them.

    /// @{

    /// \brief Return the total cost of the found flow.
    ///
    /// This function returns the total cost of the found flow.
    /// Its complexity is O(m).
    ///
    /// \note The return type of the function can be specified as a
    /// template parameter. For example,
    /// \code
    ///   cs.totalCost<double>();
    /// \endcode
    /// It is useful if the total cost cannot be stored in the \c Cost
    /// type of the algorithm, which is the default return type of the
    /// function.
    ///
    /// \pre \ref run() must be called before using this function.
    template <typename Number>
    Number totalCost() const {
      Number c = 0;
      for (ArcIt a(_graph); a != INVALID; ++a) {
        int i = _arc_idb[a];
        c += static_cast<Number>(_res_cap[i]) *
             (-static_cast<Number>(_cost[i]));
      }
      return c;
    }

#ifndef DOXYGEN
    Cost totalCost() const {
      return totalCost<Cost>();
    }
#endif

    /// \brief Return the flow on the given arc.
    ///
    /// This function returns the flow on the given arc.
    ///
    /// \pre \ref run() must be called before using this function.
    Value flow(const Arc& a) const {
      return _res_cap[_arc_idb[a]];
    }

    /// \brief Copy the flow values (the primal solution) into the
    /// given map.
    ///
    /// This function copies the flow value on each arc into the given
    /// map. The \c Value type of the algorithm must be convertible to
    /// the \c Value type of the map.
    ///
    /// \pre \ref run() must be called before using this function.
    template <typename FlowMap>
    void flowMap(FlowMap &map) const {
      for (ArcIt a(_graph); a != INVALID; ++a) {
        map.set(a, _res_cap[_arc_idb[a]]);
      }
    }

    /// \brief Return the potential (dual value) of the given node.
    ///
    /// This function returns the potential (dual value) of the
    /// given node.
    ///
    /// \pre \ref run() must be called before using this function.
    Cost potential(const Node& n) const {
      return _pi[_node_id[n]];
    }

    /// \brief Copy the potential values (the dual solution) into the
    /// given map.
    ///
    /// This function copies the potential (dual value) of each node
    /// into the given map.
    /// The \c Cost type of the algorithm must be convertible to the
    /// \c Value type of the map.
    ///
    /// \pre \ref run() must be called before using this function.
    template <typename PotentialMap>
    void potentialMap(PotentialMap &map) const {
      for (NodeIt n(_graph); n != INVALID; ++n) {
        map.set(n, _pi[_node_id[n]]);
      }
    }

    /// @}

  private:

    // Initialize the algorithm
    ProblemType init() {
      if (_node_num <= 1) return INFEASIBLE;

      // Check the sum of supply values
      _sum_supply = 0;
      for (int i = 0; i != _root; ++i) {
        _sum_supply += _supply[i];
      }
      if (_sum_supply > 0) return INFEASIBLE;

      // Check lower and upper bounds
      LEMON_DEBUG(checkBoundMaps(),
          "Upper bounds must be greater or equal to the lower bounds");


      // Initialize vectors
      for (int i = 0; i != _root; ++i) {
        _pi[i] = 0;
        _excess[i] = _supply[i];
      }

      // Remove non-zero lower bounds
      const Value MAX = std::numeric_limits<Value>::max();
      int last_out;
      if (_has_lower) {
        for (int i = 0; i != _root; ++i) {
          last_out = _first_out[i+1];
          for (int j = _first_out[i]; j != last_out; ++j) {
            if (_forward[j]) {
              Value c = _lower[j];
              if (c >= 0) {
                _res_cap[j] = _upper[j] < MAX ? _upper[j] - c : INF;
              } else {
                _res_cap[j] = _upper[j] < MAX + c ? _upper[j] - c : INF;
              }
              _excess[i] -= c;
              _excess[_target[j]] += c;
            } else {
              _res_cap[j] = 0;
            }
          }
        }
      } else {
        for (int j = 0; j != _res_arc_num; ++j) {
          _res_cap[j] = _forward[j] ? _upper[j] : 0;
        }
      }

      // Handle negative costs
      for (int i = 0; i != _root; ++i) {
        last_out = _first_out[i+1] - 1;
        for (int j = _first_out[i]; j != last_out; ++j) {
          Value rc = _res_cap[j];
          if (_cost[j] < 0 && rc > 0) {
            if (rc >= MAX) return UNBOUNDED;
            _excess[i] -= rc;
            _excess[_target[j]] += rc;
            _res_cap[j] = 0;
            _res_cap[_reverse[j]] += rc;
          }
        }
      }

      // Handle GEQ supply type
      if (_sum_supply < 0) {
        _pi[_root] = 0;
        _excess[_root] = -_sum_supply;
        for (int a = _first_out[_root]; a != _res_arc_num; ++a) {
          int ra = _reverse[a];
          _res_cap[a] = -_sum_supply + 1;
          _res_cap[ra] = 0;
          _cost[a] = 0;
          _cost[ra] = 0;
        }
      } else {
        _pi[_root] = 0;
        _excess[_root] = 0;
        for (int a = _first_out[_root]; a != _res_arc_num; ++a) {
          int ra = _reverse[a];
          _res_cap[a] = 1;
          _res_cap[ra] = 0;
          _cost[a] = 0;
          _cost[ra] = 0;
        }
      }

      // Initialize delta value
      if (_factor > 1) {
        // With scaling
        Value max_sup = 0, max_dem = 0, max_cap = 0;
        for (int i = 0; i != _root; ++i) {
          Value ex = _excess[i];
          if ( ex > max_sup) max_sup =  ex;
          if (-ex > max_dem) max_dem = -ex;
          int last_out = _first_out[i+1] - 1;
          for (int j = _first_out[i]; j != last_out; ++j) {
            if (_res_cap[j] > max_cap) max_cap = _res_cap[j];
          }
        }
        max_sup = std::min(std::min(max_sup, max_dem), max_cap);
        for (_delta = 1; 2 * _delta <= max_sup; _delta *= 2) ;
      } else {
        // Without scaling
        _delta = 1;
      }

      return OPTIMAL;
    }

    // Check if the upper bound is greater than or equal to the lower bound
    // on each forward arc.
    bool checkBoundMaps() {
      for (int j = 0; j != _res_arc_num; ++j) {
        if (_forward[j] && _upper[j] < _lower[j]) return false;
      }
      return true;
    }

    ProblemType start() {
      // Execute the algorithm
      ProblemType pt;
      if (_delta > 1)
        pt = startWithScaling();
      else
        pt = startWithoutScaling();

      // Handle non-zero lower bounds
      if (_has_lower) {
        int limit = _first_out[_root];
        for (int j = 0; j != limit; ++j) {
          if (_forward[j]) _res_cap[_reverse[j]] += _lower[j];
        }
      }

      // Shift potentials if necessary
      Cost pr = _pi[_root];
      if (_sum_supply < 0 || pr > 0) {
        for (int i = 0; i != _node_num; ++i) {
          _pi[i] -= pr;
        }
      }

      return pt;
    }

    // Execute the capacity scaling algorithm
    ProblemType startWithScaling() {
      // Perform capacity scaling phases
      int s, t;
      ResidualDijkstra _dijkstra(*this);
      while (true) {
        // Saturate all arcs not satisfying the optimality condition
        int last_out;
        for (int u = 0; u != _node_num; ++u) {
          last_out = _sum_supply < 0 ?
            _first_out[u+1] : _first_out[u+1] - 1;
          for (int a = _first_out[u]; a != last_out; ++a) {
            int v = _target[a];
            Cost c = _cost[a] + _pi[u] - _pi[v];
            Value rc = _res_cap[a];
            if (c < 0 && rc >= _delta) {
              _excess[u] -= rc;
              _excess[v] += rc;
              _res_cap[a] = 0;
              _res_cap[_reverse[a]] += rc;
            }
          }
        }

        // Find excess nodes and deficit nodes
        _excess_nodes.clear();
        _deficit_nodes.clear();
        for (int u = 0; u != _node_num; ++u) {
          Value ex = _excess[u];
          if (ex >=  _delta) _excess_nodes.push_back(u);
          if (ex <= -_delta) _deficit_nodes.push_back(u);
        }
        int next_node = 0, next_def_node = 0;

        // Find augmenting shortest paths
        while (next_node < int(_excess_nodes.size())) {
          // Check deficit nodes
          if (_delta > 1) {
            bool delta_deficit = false;
            for ( ; next_def_node < int(_deficit_nodes.size());
                    ++next_def_node ) {
              if (_excess[_deficit_nodes[next_def_node]] <= -_delta) {
                delta_deficit = true;
                break;
              }
            }
            if (!delta_deficit) break;
          }

          // Run Dijkstra in the residual network
          s = _excess_nodes[next_node];
          if ((t = _dijkstra.run(s, _delta)) == -1) {
            if (_delta > 1) {
              ++next_node;
              continue;
            }
            return INFEASIBLE;
          }

          // Augment along a shortest path from s to t
          Value d = std::min(_excess[s], -_excess[t]);
          int u = t;
          int a;
          if (d > _delta) {
            while ((a = _pred[u]) != -1) {
              if (_res_cap[a] < d) d = _res_cap[a];
              u = _source[a];
            }
          }
          u = t;
          while ((a = _pred[u]) != -1) {
            _res_cap[a] -= d;
            _res_cap[_reverse[a]] += d;
            u = _source[a];
          }
          _excess[s] -= d;
          _excess[t] += d;

          if (_excess[s] < _delta) ++next_node;
        }

        if (_delta == 1) break;
        _delta = _delta <= _factor ? 1 : _delta / _factor;
      }

      return OPTIMAL;
    }

    // Execute the successive shortest path algorithm
    ProblemType startWithoutScaling() {
      // Find excess nodes
      _excess_nodes.clear();
      for (int i = 0; i != _node_num; ++i) {
        if (_excess[i] > 0) _excess_nodes.push_back(i);
      }
      if (_excess_nodes.size() == 0) return OPTIMAL;
      int next_node = 0;

      // Find shortest paths
      int s, t;
      ResidualDijkstra _dijkstra(*this);
      while ( _excess[_excess_nodes[next_node]] > 0 ||
              ++next_node < int(_excess_nodes.size()) )
      {
        // Run Dijkstra in the residual network
        s = _excess_nodes[next_node];
        if ((t = _dijkstra.run(s)) == -1) return INFEASIBLE;

        // Augment along a shortest path from s to t
        Value d = std::min(_excess[s], -_excess[t]);
        int u = t;
        int a;
        if (d > 1) {
          while ((a = _pred[u]) != -1) {
            if (_res_cap[a] < d) d = _res_cap[a];
            u = _source[a];
          }
        }
        u = t;
        while ((a = _pred[u]) != -1) {
          _res_cap[a] -= d;
          _res_cap[_reverse[a]] += d;
          u = _source[a];
        }
        _excess[s] -= d;
        _excess[t] += d;
      }

      return OPTIMAL;
    }

  }; //class CapacityScaling

  ///@}

} //namespace lemon

#endif //LEMON_CAPACITY_SCALING_H
