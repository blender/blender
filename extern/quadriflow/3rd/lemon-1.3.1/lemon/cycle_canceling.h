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

#ifndef LEMON_CYCLE_CANCELING_H
#define LEMON_CYCLE_CANCELING_H

/// \ingroup min_cost_flow_algs
/// \file
/// \brief Cycle-canceling algorithms for finding a minimum cost flow.

#include <vector>
#include <limits>

#include <lemon/core.h>
#include <lemon/maps.h>
#include <lemon/path.h>
#include <lemon/math.h>
#include <lemon/static_graph.h>
#include <lemon/adaptors.h>
#include <lemon/circulation.h>
#include <lemon/bellman_ford.h>
#include <lemon/howard_mmc.h>
#include <lemon/hartmann_orlin_mmc.h>

namespace lemon {

  /// \addtogroup min_cost_flow_algs
  /// @{

  /// \brief Implementation of cycle-canceling algorithms for
  /// finding a \ref min_cost_flow "minimum cost flow".
  ///
  /// \ref CycleCanceling implements three different cycle-canceling
  /// algorithms for finding a \ref min_cost_flow "minimum cost flow"
  /// \cite amo93networkflows, \cite klein67primal,
  /// \cite goldberg89cyclecanceling.
  /// The most efficent one is the \ref CANCEL_AND_TIGHTEN
  /// "Cancel-and-Tighten" algorithm, thus it is the default method.
  /// It runs in strongly polynomial time \f$O(n^2 m^2 \log n)\f$,
  /// but in practice, it is typically orders of magnitude slower than
  /// the scaling algorithms and \ref NetworkSimplex.
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
  ///
  /// \warning Both \c V and \c C must be signed number types.
  /// \warning All input data (capacities, supply values, and costs) must
  /// be integer.
  /// \warning This algorithm does not support negative costs for
  /// arcs having infinite upper bound.
  ///
  /// \note For more information about the three available methods,
  /// see \ref Method.
#ifdef DOXYGEN
  template <typename GR, typename V, typename C>
#else
  template <typename GR, typename V = int, typename C = V>
#endif
  class CycleCanceling
  {
  public:

    /// The type of the digraph
    typedef GR Digraph;
    /// The type of the flow amounts, capacity bounds and supply values
    typedef V Value;
    /// The type of the arc costs
    typedef C Cost;

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

    /// \brief Constants for selecting the used method.
    ///
    /// Enum type containing constants for selecting the used method
    /// for the \ref run() function.
    ///
    /// \ref CycleCanceling provides three different cycle-canceling
    /// methods. By default, \ref CANCEL_AND_TIGHTEN "Cancel-and-Tighten"
    /// is used, which is by far the most efficient and the most robust.
    /// However, the other methods can be selected using the \ref run()
    /// function with the proper parameter.
    enum Method {
      /// A simple cycle-canceling method, which uses the
      /// \ref BellmanFord "Bellman-Ford" algorithm for detecting negative
      /// cycles in the residual network.
      /// The number of Bellman-Ford iterations is bounded by a successively
      /// increased limit.
      SIMPLE_CYCLE_CANCELING,
      /// The "Minimum Mean Cycle-Canceling" algorithm, which is a
      /// well-known strongly polynomial method
      /// \cite goldberg89cyclecanceling. It improves along a
      /// \ref min_mean_cycle "minimum mean cycle" in each iteration.
      /// Its running time complexity is \f$O(n^2 m^3 \log n)\f$.
      MINIMUM_MEAN_CYCLE_CANCELING,
      /// The "Cancel-and-Tighten" algorithm, which can be viewed as an
      /// improved version of the previous method
      /// \cite goldberg89cyclecanceling.
      /// It is faster both in theory and in practice, its running time
      /// complexity is \f$O(n^2 m^2 \log n)\f$.
      CANCEL_AND_TIGHTEN
    };

  private:

    TEMPLATE_DIGRAPH_TYPEDEFS(GR);

    typedef std::vector<int> IntVector;
    typedef std::vector<double> DoubleVector;
    typedef std::vector<Value> ValueVector;
    typedef std::vector<Cost> CostVector;
    typedef std::vector<char> BoolVector;
    // Note: vector<char> is used instead of vector<bool> for efficiency reasons

  private:

    template <typename KT, typename VT>
    class StaticVectorMap {
    public:
      typedef KT Key;
      typedef VT Value;

      StaticVectorMap(std::vector<Value>& v) : _v(v) {}

      const Value& operator[](const Key& key) const {
        return _v[StaticDigraph::id(key)];
      }

      Value& operator[](const Key& key) {
        return _v[StaticDigraph::id(key)];
      }

      void set(const Key& key, const Value& val) {
        _v[StaticDigraph::id(key)] = val;
      }

    private:
      std::vector<Value>& _v;
    };

    typedef StaticVectorMap<StaticDigraph::Node, Cost> CostNodeMap;
    typedef StaticVectorMap<StaticDigraph::Arc, Cost> CostArcMap;

  private:


    // Data related to the underlying digraph
    const GR &_graph;
    int _node_num;
    int _arc_num;
    int _res_node_num;
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

    // Data for a StaticDigraph structure
    typedef std::pair<int, int> IntPair;
    StaticDigraph _sgr;
    std::vector<IntPair> _arc_vec;
    std::vector<Cost> _cost_vec;
    IntVector _id_vec;
    CostArcMap _cost_map;
    CostNodeMap _pi_map;

  public:

    /// \brief Constant for infinite upper bounds (capacities).
    ///
    /// Constant for infinite upper bounds (capacities).
    /// It is \c std::numeric_limits<Value>::infinity() if available,
    /// \c std::numeric_limits<Value>::max() otherwise.
    const Value INF;

  public:

    /// \brief Constructor.
    ///
    /// The constructor of the class.
    ///
    /// \param graph The digraph the algorithm runs on.
    CycleCanceling(const GR& graph) :
      _graph(graph), _node_id(graph), _arc_idf(graph), _arc_idb(graph),
      _cost_map(_cost_vec), _pi_map(_pi),
      INF(std::numeric_limits<Value>::has_infinity ?
          std::numeric_limits<Value>::infinity() :
          std::numeric_limits<Value>::max())
    {
      // Check the number types
      LEMON_ASSERT(std::numeric_limits<Value>::is_signed,
        "The flow type of CycleCanceling must be signed");
      LEMON_ASSERT(std::numeric_limits<Cost>::is_signed,
        "The cost type of CycleCanceling must be signed");

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
    CycleCanceling& lowerMap(const LowerMap& map) {
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
    CycleCanceling& upperMap(const UpperMap& map) {
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
    CycleCanceling& costMap(const CostMap& map) {
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
    CycleCanceling& supplyMap(const SupplyMap& map) {
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
    CycleCanceling& stSupply(const Node& s, const Node& t, Value k) {
      for (int i = 0; i != _res_node_num; ++i) {
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
    ///   CycleCanceling<ListDigraph> cc(graph);
    ///   cc.lowerMap(lower).upperMap(upper).costMap(cost)
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
    /// \param method The cycle-canceling method that will be used.
    /// For more information, see \ref Method.
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
    /// \see ProblemType, Method
    /// \see resetParams(), reset()
    ProblemType run(Method method = CANCEL_AND_TIGHTEN) {
      ProblemType pt = init();
      if (pt != OPTIMAL) return pt;
      start(method);
      return OPTIMAL;
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
    ///   CycleCanceling<ListDigraph> cs(graph);
    ///
    ///   // First run
    ///   cc.lowerMap(lower).upperMap(upper).costMap(cost)
    ///     .supplyMap(sup).run();
    ///
    ///   // Run again with modified cost map (resetParams() is not called,
    ///   // so only the cost map have to be set again)
    ///   cost[e] += 100;
    ///   cc.costMap(cost).run();
    ///
    ///   // Run again from scratch using resetParams()
    ///   // (the lower bounds will be set to zero on all arcs)
    ///   cc.resetParams();
    ///   cc.upperMap(capacity).costMap(cost)
    ///     .supplyMap(sup).run();
    /// \endcode
    ///
    /// \return <tt>(*this)</tt>
    ///
    /// \see reset(), run()
    CycleCanceling& resetParams() {
      for (int i = 0; i != _res_node_num; ++i) {
        _supply[i] = 0;
      }
      int limit = _first_out[_root];
      for (int j = 0; j != limit; ++j) {
        _lower[j] = 0;
        _upper[j] = INF;
        _cost[j] = _forward[j] ? 1 : -1;
      }
      for (int j = limit; j != _res_arc_num; ++j) {
        _lower[j] = 0;
        _upper[j] = INF;
        _cost[j] = 0;
        _cost[_reverse[j]] = 0;
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
    CycleCanceling& reset() {
      // Resize vectors
      _node_num = countNodes(_graph);
      _arc_num = countArcs(_graph);
      _res_node_num = _node_num + 1;
      _res_arc_num = 2 * (_arc_num + _node_num);
      _root = _node_num;

      _first_out.resize(_res_node_num + 1);
      _forward.resize(_res_arc_num);
      _source.resize(_res_arc_num);
      _target.resize(_res_arc_num);
      _reverse.resize(_res_arc_num);

      _lower.resize(_res_arc_num);
      _upper.resize(_res_arc_num);
      _cost.resize(_res_arc_num);
      _supply.resize(_res_node_num);

      _res_cap.resize(_res_arc_num);
      _pi.resize(_res_node_num);

      _arc_vec.reserve(_res_arc_num);
      _cost_vec.reserve(_res_arc_num);
      _id_vec.reserve(_res_arc_num);

      // Copy the graph
      int i = 0, j = 0, k = 2 * _arc_num + _node_num;
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
      _first_out[_res_node_num] = k;
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
    ///   cc.totalCost<double>();
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
      return static_cast<Cost>(_pi[_node_id[n]]);
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
        map.set(n, static_cast<Cost>(_pi[_node_id[n]]));
      }
    }

    /// @}

  private:

    // Initialize the algorithm
    ProblemType init() {
      if (_res_node_num <= 1) return INFEASIBLE;

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
      for (int i = 0; i != _res_node_num; ++i) {
        _pi[i] = 0;
      }
      ValueVector excess(_supply);

      // Remove infinite upper bounds and check negative arcs
      const Value MAX = std::numeric_limits<Value>::max();
      int last_out;
      if (_has_lower) {
        for (int i = 0; i != _root; ++i) {
          last_out = _first_out[i+1];
          for (int j = _first_out[i]; j != last_out; ++j) {
            if (_forward[j]) {
              Value c = _cost[j] < 0 ? _upper[j] : _lower[j];
              if (c >= MAX) return UNBOUNDED;
              excess[i] -= c;
              excess[_target[j]] += c;
            }
          }
        }
      } else {
        for (int i = 0; i != _root; ++i) {
          last_out = _first_out[i+1];
          for (int j = _first_out[i]; j != last_out; ++j) {
            if (_forward[j] && _cost[j] < 0) {
              Value c = _upper[j];
              if (c >= MAX) return UNBOUNDED;
              excess[i] -= c;
              excess[_target[j]] += c;
            }
          }
        }
      }
      Value ex, max_cap = 0;
      for (int i = 0; i != _res_node_num; ++i) {
        ex = excess[i];
        if (ex < 0) max_cap -= ex;
      }
      for (int j = 0; j != _res_arc_num; ++j) {
        if (_upper[j] >= MAX) _upper[j] = max_cap;
      }

      // Initialize maps for Circulation and remove non-zero lower bounds
      ConstMap<Arc, Value> low(0);
      typedef typename Digraph::template ArcMap<Value> ValueArcMap;
      typedef typename Digraph::template NodeMap<Value> ValueNodeMap;
      ValueArcMap cap(_graph), flow(_graph);
      ValueNodeMap sup(_graph);
      for (NodeIt n(_graph); n != INVALID; ++n) {
        sup[n] = _supply[_node_id[n]];
      }
      if (_has_lower) {
        for (ArcIt a(_graph); a != INVALID; ++a) {
          int j = _arc_idf[a];
          Value c = _lower[j];
          cap[a] = _upper[j] - c;
          sup[_graph.source(a)] -= c;
          sup[_graph.target(a)] += c;
        }
      } else {
        for (ArcIt a(_graph); a != INVALID; ++a) {
          cap[a] = _upper[_arc_idf[a]];
        }
      }

      // Find a feasible flow using Circulation
      Circulation<Digraph, ConstMap<Arc, Value>, ValueArcMap, ValueNodeMap>
        circ(_graph, low, cap, sup);
      if (!circ.flowMap(flow).run()) return INFEASIBLE;

      // Set residual capacities and handle GEQ supply type
      if (_sum_supply < 0) {
        for (ArcIt a(_graph); a != INVALID; ++a) {
          Value fa = flow[a];
          _res_cap[_arc_idf[a]] = cap[a] - fa;
          _res_cap[_arc_idb[a]] = fa;
          sup[_graph.source(a)] -= fa;
          sup[_graph.target(a)] += fa;
        }
        for (NodeIt n(_graph); n != INVALID; ++n) {
          excess[_node_id[n]] = sup[n];
        }
        for (int a = _first_out[_root]; a != _res_arc_num; ++a) {
          int u = _target[a];
          int ra = _reverse[a];
          _res_cap[a] = -_sum_supply + 1;
          _res_cap[ra] = -excess[u];
          _cost[a] = 0;
          _cost[ra] = 0;
        }
      } else {
        for (ArcIt a(_graph); a != INVALID; ++a) {
          Value fa = flow[a];
          _res_cap[_arc_idf[a]] = cap[a] - fa;
          _res_cap[_arc_idb[a]] = fa;
        }
        for (int a = _first_out[_root]; a != _res_arc_num; ++a) {
          int ra = _reverse[a];
          _res_cap[a] = 1;
          _res_cap[ra] = 0;
          _cost[a] = 0;
          _cost[ra] = 0;
        }
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

    // Build a StaticDigraph structure containing the current
    // residual network
    void buildResidualNetwork() {
      _arc_vec.clear();
      _cost_vec.clear();
      _id_vec.clear();
      for (int j = 0; j != _res_arc_num; ++j) {
        if (_res_cap[j] > 0) {
          _arc_vec.push_back(IntPair(_source[j], _target[j]));
          _cost_vec.push_back(_cost[j]);
          _id_vec.push_back(j);
        }
      }
      _sgr.build(_res_node_num, _arc_vec.begin(), _arc_vec.end());
    }

    // Execute the algorithm and transform the results
    void start(Method method) {
      // Execute the algorithm
      switch (method) {
        case SIMPLE_CYCLE_CANCELING:
          startSimpleCycleCanceling();
          break;
        case MINIMUM_MEAN_CYCLE_CANCELING:
          startMinMeanCycleCanceling();
          break;
        case CANCEL_AND_TIGHTEN:
          startCancelAndTighten();
          break;
      }

      // Compute node potentials
      if (method != SIMPLE_CYCLE_CANCELING) {
        buildResidualNetwork();
        typename BellmanFord<StaticDigraph, CostArcMap>
          ::template SetDistMap<CostNodeMap>::Create bf(_sgr, _cost_map);
        bf.distMap(_pi_map);
        bf.init(0);
        bf.start();
      }

      // Handle non-zero lower bounds
      if (_has_lower) {
        int limit = _first_out[_root];
        for (int j = 0; j != limit; ++j) {
          if (_forward[j]) _res_cap[_reverse[j]] += _lower[j];
        }
      }
    }

    // Execute the "Simple Cycle Canceling" method
    void startSimpleCycleCanceling() {
      // Constants for computing the iteration limits
      const int BF_FIRST_LIMIT  = 2;
      const double BF_LIMIT_FACTOR = 1.5;

      typedef StaticVectorMap<StaticDigraph::Arc, Value> FilterMap;
      typedef FilterArcs<StaticDigraph, FilterMap> ResDigraph;
      typedef StaticVectorMap<StaticDigraph::Node, StaticDigraph::Arc> PredMap;
      typedef typename BellmanFord<ResDigraph, CostArcMap>
        ::template SetDistMap<CostNodeMap>
        ::template SetPredMap<PredMap>::Create BF;

      // Build the residual network
      _arc_vec.clear();
      _cost_vec.clear();
      for (int j = 0; j != _res_arc_num; ++j) {
        _arc_vec.push_back(IntPair(_source[j], _target[j]));
        _cost_vec.push_back(_cost[j]);
      }
      _sgr.build(_res_node_num, _arc_vec.begin(), _arc_vec.end());

      FilterMap filter_map(_res_cap);
      ResDigraph rgr(_sgr, filter_map);
      std::vector<int> cycle;
      std::vector<StaticDigraph::Arc> pred(_res_arc_num);
      PredMap pred_map(pred);
      BF bf(rgr, _cost_map);
      bf.distMap(_pi_map).predMap(pred_map);

      int length_bound = BF_FIRST_LIMIT;
      bool optimal = false;
      while (!optimal) {
        bf.init(0);
        int iter_num = 0;
        bool cycle_found = false;
        while (!cycle_found) {
          // Perform some iterations of the Bellman-Ford algorithm
          int curr_iter_num = iter_num + length_bound <= _node_num ?
            length_bound : _node_num - iter_num;
          iter_num += curr_iter_num;
          int real_iter_num = curr_iter_num;
          for (int i = 0; i < curr_iter_num; ++i) {
            if (bf.processNextWeakRound()) {
              real_iter_num = i;
              break;
            }
          }
          if (real_iter_num < curr_iter_num) {
            // Optimal flow is found
            optimal = true;
            break;
          } else {
            // Search for node disjoint negative cycles
            std::vector<int> state(_res_node_num, 0);
            int id = 0;
            for (int u = 0; u != _res_node_num; ++u) {
              if (state[u] != 0) continue;
              ++id;
              int v = u;
              for (; v != -1 && state[v] == 0; v = pred[v] == INVALID ?
                   -1 : rgr.id(rgr.source(pred[v]))) {
                state[v] = id;
              }
              if (v != -1 && state[v] == id) {
                // A negative cycle is found
                cycle_found = true;
                cycle.clear();
                StaticDigraph::Arc a = pred[v];
                Value d, delta = _res_cap[rgr.id(a)];
                cycle.push_back(rgr.id(a));
                while (rgr.id(rgr.source(a)) != v) {
                  a = pred_map[rgr.source(a)];
                  d = _res_cap[rgr.id(a)];
                  if (d < delta) delta = d;
                  cycle.push_back(rgr.id(a));
                }

                // Augment along the cycle
                for (int i = 0; i < int(cycle.size()); ++i) {
                  int j = cycle[i];
                  _res_cap[j] -= delta;
                  _res_cap[_reverse[j]] += delta;
                }
              }
            }
          }

          // Increase iteration limit if no cycle is found
          if (!cycle_found) {
            length_bound = static_cast<int>(length_bound * BF_LIMIT_FACTOR);
          }
        }
      }
    }

    // Execute the "Minimum Mean Cycle Canceling" method
    void startMinMeanCycleCanceling() {
      typedef Path<StaticDigraph> SPath;
      typedef typename SPath::ArcIt SPathArcIt;
      typedef typename HowardMmc<StaticDigraph, CostArcMap>
        ::template SetPath<SPath>::Create HwMmc;
      typedef typename HartmannOrlinMmc<StaticDigraph, CostArcMap>
        ::template SetPath<SPath>::Create HoMmc;

      const double HW_ITER_LIMIT_FACTOR = 1.0;
      const int HW_ITER_LIMIT_MIN_VALUE = 5;

      const int hw_iter_limit =
          std::max(static_cast<int>(HW_ITER_LIMIT_FACTOR * _node_num),
                   HW_ITER_LIMIT_MIN_VALUE);

      SPath cycle;
      HwMmc hw_mmc(_sgr, _cost_map);
      hw_mmc.cycle(cycle);
      buildResidualNetwork();
      while (true) {

        typename HwMmc::TerminationCause hw_tc =
            hw_mmc.findCycleMean(hw_iter_limit);
        if (hw_tc == HwMmc::ITERATION_LIMIT) {
          // Howard's algorithm reached the iteration limit, start a
          // strongly polynomial algorithm instead
          HoMmc ho_mmc(_sgr, _cost_map);
          ho_mmc.cycle(cycle);
          // Find a minimum mean cycle (Hartmann-Orlin algorithm)
          if (!(ho_mmc.findCycleMean() && ho_mmc.cycleCost() < 0)) break;
          ho_mmc.findCycle();
        } else {
          // Find a minimum mean cycle (Howard algorithm)
          if (!(hw_tc == HwMmc::OPTIMAL && hw_mmc.cycleCost() < 0)) break;
          hw_mmc.findCycle();
        }

        // Compute delta value
        Value delta = INF;
        for (SPathArcIt a(cycle); a != INVALID; ++a) {
          Value d = _res_cap[_id_vec[_sgr.id(a)]];
          if (d < delta) delta = d;
        }

        // Augment along the cycle
        for (SPathArcIt a(cycle); a != INVALID; ++a) {
          int j = _id_vec[_sgr.id(a)];
          _res_cap[j] -= delta;
          _res_cap[_reverse[j]] += delta;
        }

        // Rebuild the residual network
        buildResidualNetwork();
      }
    }

    // Execute the "Cancel-and-Tighten" method
    void startCancelAndTighten() {
      // Constants for the min mean cycle computations
      const double LIMIT_FACTOR = 1.0;
      const int MIN_LIMIT = 5;
      const double HW_ITER_LIMIT_FACTOR = 1.0;
      const int HW_ITER_LIMIT_MIN_VALUE = 5;

      const int hw_iter_limit =
          std::max(static_cast<int>(HW_ITER_LIMIT_FACTOR * _node_num),
                   HW_ITER_LIMIT_MIN_VALUE);

      // Contruct auxiliary data vectors
      DoubleVector pi(_res_node_num, 0.0);
      IntVector level(_res_node_num);
      BoolVector reached(_res_node_num);
      BoolVector processed(_res_node_num);
      IntVector pred_node(_res_node_num);
      IntVector pred_arc(_res_node_num);
      std::vector<int> stack(_res_node_num);
      std::vector<int> proc_vector(_res_node_num);

      // Initialize epsilon
      double epsilon = 0;
      for (int a = 0; a != _res_arc_num; ++a) {
        if (_res_cap[a] > 0 && -_cost[a] > epsilon)
          epsilon = -_cost[a];
      }

      // Start phases
      Tolerance<double> tol;
      tol.epsilon(1e-6);
      int limit = int(LIMIT_FACTOR * std::sqrt(double(_res_node_num)));
      if (limit < MIN_LIMIT) limit = MIN_LIMIT;
      int iter = limit;
      while (epsilon * _res_node_num >= 1) {
        // Find and cancel cycles in the admissible network using DFS
        for (int u = 0; u != _res_node_num; ++u) {
          reached[u] = false;
          processed[u] = false;
        }
        int stack_head = -1;
        int proc_head = -1;
        for (int start = 0; start != _res_node_num; ++start) {
          if (reached[start]) continue;

          // New start node
          reached[start] = true;
          pred_arc[start] = -1;
          pred_node[start] = -1;

          // Find the first admissible outgoing arc
          double p = pi[start];
          int a = _first_out[start];
          int last_out = _first_out[start+1];
          for (; a != last_out && (_res_cap[a] == 0 ||
               !tol.negative(_cost[a] + p - pi[_target[a]])); ++a) ;
          if (a == last_out) {
            processed[start] = true;
            proc_vector[++proc_head] = start;
            continue;
          }
          stack[++stack_head] = a;

          while (stack_head >= 0) {
            int sa = stack[stack_head];
            int u = _source[sa];
            int v = _target[sa];

            if (!reached[v]) {
              // A new node is reached
              reached[v] = true;
              pred_node[v] = u;
              pred_arc[v] = sa;
              p = pi[v];
              a = _first_out[v];
              last_out = _first_out[v+1];
              for (; a != last_out && (_res_cap[a] == 0 ||
                   !tol.negative(_cost[a] + p - pi[_target[a]])); ++a) ;
              stack[++stack_head] = a == last_out ? -1 : a;
            } else {
              if (!processed[v]) {
                // A cycle is found
                int n, w = u;
                Value d, delta = _res_cap[sa];
                for (n = u; n != v; n = pred_node[n]) {
                  d = _res_cap[pred_arc[n]];
                  if (d <= delta) {
                    delta = d;
                    w = pred_node[n];
                  }
                }

                // Augment along the cycle
                _res_cap[sa] -= delta;
                _res_cap[_reverse[sa]] += delta;
                for (n = u; n != v; n = pred_node[n]) {
                  int pa = pred_arc[n];
                  _res_cap[pa] -= delta;
                  _res_cap[_reverse[pa]] += delta;
                }
                for (n = u; stack_head > 0 && n != w; n = pred_node[n]) {
                  --stack_head;
                  reached[n] = false;
                }
                u = w;
              }
              v = u;

              // Find the next admissible outgoing arc
              p = pi[v];
              a = stack[stack_head] + 1;
              last_out = _first_out[v+1];
              for (; a != last_out && (_res_cap[a] == 0 ||
                   !tol.negative(_cost[a] + p - pi[_target[a]])); ++a) ;
              stack[stack_head] = a == last_out ? -1 : a;
            }

            while (stack_head >= 0 && stack[stack_head] == -1) {
              processed[v] = true;
              proc_vector[++proc_head] = v;
              if (--stack_head >= 0) {
                // Find the next admissible outgoing arc
                v = _source[stack[stack_head]];
                p = pi[v];
                a = stack[stack_head] + 1;
                last_out = _first_out[v+1];
                for (; a != last_out && (_res_cap[a] == 0 ||
                     !tol.negative(_cost[a] + p - pi[_target[a]])); ++a) ;
                stack[stack_head] = a == last_out ? -1 : a;
              }
            }
          }
        }

        // Tighten potentials and epsilon
        if (--iter > 0) {
          for (int u = 0; u != _res_node_num; ++u) {
            level[u] = 0;
          }
          for (int i = proc_head; i > 0; --i) {
            int u = proc_vector[i];
            double p = pi[u];
            int l = level[u] + 1;
            int last_out = _first_out[u+1];
            for (int a = _first_out[u]; a != last_out; ++a) {
              int v = _target[a];
              if (_res_cap[a] > 0 && tol.negative(_cost[a] + p - pi[v]) &&
                  l > level[v]) level[v] = l;
            }
          }

          // Modify potentials
          double q = std::numeric_limits<double>::max();
          for (int u = 0; u != _res_node_num; ++u) {
            int lu = level[u];
            double p, pu = pi[u];
            int last_out = _first_out[u+1];
            for (int a = _first_out[u]; a != last_out; ++a) {
              if (_res_cap[a] == 0) continue;
              int v = _target[a];
              int ld = lu - level[v];
              if (ld > 0) {
                p = (_cost[a] + pu - pi[v] + epsilon) / (ld + 1);
                if (p < q) q = p;
              }
            }
          }
          for (int u = 0; u != _res_node_num; ++u) {
            pi[u] -= q * level[u];
          }

          // Modify epsilon
          epsilon = 0;
          for (int u = 0; u != _res_node_num; ++u) {
            double curr, pu = pi[u];
            int last_out = _first_out[u+1];
            for (int a = _first_out[u]; a != last_out; ++a) {
              if (_res_cap[a] == 0) continue;
              curr = _cost[a] + pu - pi[_target[a]];
              if (-curr > epsilon) epsilon = -curr;
            }
          }
        } else {
          typedef HowardMmc<StaticDigraph, CostArcMap> HwMmc;
          typedef HartmannOrlinMmc<StaticDigraph, CostArcMap> HoMmc;
          typedef typename BellmanFord<StaticDigraph, CostArcMap>
            ::template SetDistMap<CostNodeMap>::Create BF;

          // Set epsilon to the minimum cycle mean
          Cost cycle_cost = 0;
          int cycle_size = 1;
          buildResidualNetwork();
          HwMmc hw_mmc(_sgr, _cost_map);
          if (hw_mmc.findCycleMean(hw_iter_limit) == HwMmc::ITERATION_LIMIT) {
            // Howard's algorithm reached the iteration limit, start a
            // strongly polynomial algorithm instead
            HoMmc ho_mmc(_sgr, _cost_map);
            ho_mmc.findCycleMean();
            epsilon = -ho_mmc.cycleMean();
            cycle_cost = ho_mmc.cycleCost();
            cycle_size = ho_mmc.cycleSize();
          } else {
            // Set epsilon
            epsilon = -hw_mmc.cycleMean();
            cycle_cost = hw_mmc.cycleCost();
            cycle_size = hw_mmc.cycleSize();
          }

          // Compute feasible potentials for the current epsilon
          for (int i = 0; i != int(_cost_vec.size()); ++i) {
            _cost_vec[i] = cycle_size * _cost_vec[i] - cycle_cost;
          }
          BF bf(_sgr, _cost_map);
          bf.distMap(_pi_map);
          bf.init(0);
          bf.start();
          for (int u = 0; u != _res_node_num; ++u) {
            pi[u] = static_cast<double>(_pi[u]) / cycle_size;
          }

          iter = limit;
        }
      }
    }

  }; //class CycleCanceling

  ///@}

} //namespace lemon

#endif //LEMON_CYCLE_CANCELING_H
