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

#ifndef LEMON_NETWORK_SIMPLEX_H
#define LEMON_NETWORK_SIMPLEX_H

/// \ingroup min_cost_flow_algs
///
/// \file
/// \brief Network Simplex algorithm for finding a minimum cost flow.

#include <vector>
#include <limits>
#include <algorithm>

#include <lemon/core.h>
#include <lemon/math.h>

namespace lemon {

  /// \addtogroup min_cost_flow_algs
  /// @{

  /// \brief Implementation of the primal Network Simplex algorithm
  /// for finding a \ref min_cost_flow "minimum cost flow".
  ///
  /// \ref NetworkSimplex implements the primal Network Simplex algorithm
  /// for finding a \ref min_cost_flow "minimum cost flow"
  /// \cite amo93networkflows, \cite dantzig63linearprog,
  /// \cite kellyoneill91netsimplex.
  /// This algorithm is a highly efficient specialized version of the
  /// linear programming simplex method directly for the minimum cost
  /// flow problem.
  ///
  /// In general, \ref NetworkSimplex and \ref CostScaling are the fastest
  /// implementations available in LEMON for solving this problem.
  /// (For more information, see \ref min_cost_flow_algs "the module page".)
  /// Furthermore, this class supports both directions of the supply/demand
  /// inequality constraints. For more information, see \ref SupplyType.
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
  ///
  /// \note %NetworkSimplex provides five different pivot rule
  /// implementations, from which the most efficient one is used
  /// by default. For more information, see \ref PivotRule.
  template <typename GR, typename V = int, typename C = V>
  class NetworkSimplex
  {
  public:

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
      /// The objective function of the problem is unbounded, i.e.
      /// there is a directed cycle having negative total cost and
      /// infinite upper bound.
      UNBOUNDED
    };

    /// \brief Constants for selecting the type of the supply constraints.
    ///
    /// Enum type containing constants for selecting the supply type,
    /// i.e. the direction of the inequalities in the supply/demand
    /// constraints of the \ref min_cost_flow "minimum cost flow problem".
    ///
    /// The default supply type is \c GEQ, the \c LEQ type can be
    /// selected using \ref supplyType().
    /// The equality form is a special case of both supply types.
    enum SupplyType {
      /// This option means that there are <em>"greater or equal"</em>
      /// supply/demand constraints in the definition of the problem.
      GEQ,
      /// This option means that there are <em>"less or equal"</em>
      /// supply/demand constraints in the definition of the problem.
      LEQ
    };

    /// \brief Constants for selecting the pivot rule.
    ///
    /// Enum type containing constants for selecting the pivot rule for
    /// the \ref run() function.
    ///
    /// \ref NetworkSimplex provides five different implementations for
    /// the pivot strategy that significantly affects the running time
    /// of the algorithm.
    /// According to experimental tests conducted on various problem
    /// instances, \ref BLOCK_SEARCH "Block Search" and
    /// \ref ALTERING_LIST "Altering Candidate List" rules turned out
    /// to be the most efficient.
    /// Since \ref BLOCK_SEARCH "Block Search" is a simpler strategy that
    /// seemed to be slightly more robust, it is used by default.
    /// However, another pivot rule can easily be selected using the
    /// \ref run() function with the proper parameter.
    enum PivotRule {

      /// The \e First \e Eligible pivot rule.
      /// The next eligible arc is selected in a wraparound fashion
      /// in every iteration.
      FIRST_ELIGIBLE,

      /// The \e Best \e Eligible pivot rule.
      /// The best eligible arc is selected in every iteration.
      BEST_ELIGIBLE,

      /// The \e Block \e Search pivot rule.
      /// A specified number of arcs are examined in every iteration
      /// in a wraparound fashion and the best eligible arc is selected
      /// from this block.
      BLOCK_SEARCH,

      /// The \e Candidate \e List pivot rule.
      /// In a major iteration a candidate list is built from eligible arcs
      /// in a wraparound fashion and in the following minor iterations
      /// the best eligible arc is selected from this list.
      CANDIDATE_LIST,

      /// The \e Altering \e Candidate \e List pivot rule.
      /// It is a modified version of the Candidate List method.
      /// It keeps only a few of the best eligible arcs from the former
      /// candidate list and extends this list in every iteration.
      ALTERING_LIST
    };

  private:

    TEMPLATE_DIGRAPH_TYPEDEFS(GR);

    typedef std::vector<int> IntVector;
    typedef std::vector<Value> ValueVector;
    typedef std::vector<Cost> CostVector;
    typedef std::vector<signed char> CharVector;
    // Note: vector<signed char> is used instead of vector<ArcState> and
    // vector<ArcDirection> for efficiency reasons

    // State constants for arcs
    enum ArcState {
      STATE_UPPER = -1,
      STATE_TREE  =  0,
      STATE_LOWER =  1
    };

    // Direction constants for tree arcs
    enum ArcDirection {
      DIR_DOWN = -1,
      DIR_UP   =  1
    };

  private:

    // Data related to the underlying digraph
    const GR &_graph;
    int _node_num;
    int _arc_num;
    int _all_arc_num;
    int _search_arc_num;

    // Parameters of the problem
    bool _has_lower;
    SupplyType _stype;
    Value _sum_supply;

    // Data structures for storing the digraph
    IntNodeMap _node_id;
    IntArcMap _arc_id;
    IntVector _source;
    IntVector _target;
    bool _arc_mixing;

    // Node and arc data
    ValueVector _lower;
    ValueVector _upper;
    ValueVector _cap;
    CostVector _cost;
    ValueVector _supply;
    ValueVector _flow;
    CostVector _pi;

    // Data for storing the spanning tree structure
    IntVector _parent;
    IntVector _pred;
    IntVector _thread;
    IntVector _rev_thread;
    IntVector _succ_num;
    IntVector _last_succ;
    CharVector _pred_dir;
    CharVector _state;
    IntVector _dirty_revs;
    int _root;

    // Temporary data used in the current pivot iteration
    int in_arc, join, u_in, v_in, u_out, v_out;
    Value delta;

    const Value MAX;

  public:

    /// \brief Constant for infinite upper bounds (capacities).
    ///
    /// Constant for infinite upper bounds (capacities).
    /// It is \c std::numeric_limits<Value>::infinity() if available,
    /// \c std::numeric_limits<Value>::max() otherwise.
    const Value INF;

  private:

    // Implementation of the First Eligible pivot rule
    class FirstEligiblePivotRule
    {
    private:

      // References to the NetworkSimplex class
      const IntVector  &_source;
      const IntVector  &_target;
      const CostVector &_cost;
      const CharVector &_state;
      const CostVector &_pi;
      int &_in_arc;
      int _search_arc_num;

      // Pivot rule data
      int _next_arc;

    public:

      // Constructor
      FirstEligiblePivotRule(NetworkSimplex &ns) :
        _source(ns._source), _target(ns._target),
        _cost(ns._cost), _state(ns._state), _pi(ns._pi),
        _in_arc(ns.in_arc), _search_arc_num(ns._search_arc_num),
        _next_arc(0)
      {}

      // Find next entering arc
      bool findEnteringArc() {
        Cost c;
        for (int e = _next_arc; e != _search_arc_num; ++e) {
          c = _state[e] * (_cost[e] + _pi[_source[e]] - _pi[_target[e]]);
          if (c < 0) {
            _in_arc = e;
            _next_arc = e + 1;
            return true;
          }
        }
        for (int e = 0; e != _next_arc; ++e) {
          c = _state[e] * (_cost[e] + _pi[_source[e]] - _pi[_target[e]]);
          if (c < 0) {
            _in_arc = e;
            _next_arc = e + 1;
            return true;
          }
        }
        return false;
      }

    }; //class FirstEligiblePivotRule


    // Implementation of the Best Eligible pivot rule
    class BestEligiblePivotRule
    {
    private:

      // References to the NetworkSimplex class
      const IntVector  &_source;
      const IntVector  &_target;
      const CostVector &_cost;
      const CharVector &_state;
      const CostVector &_pi;
      int &_in_arc;
      int _search_arc_num;

    public:

      // Constructor
      BestEligiblePivotRule(NetworkSimplex &ns) :
        _source(ns._source), _target(ns._target),
        _cost(ns._cost), _state(ns._state), _pi(ns._pi),
        _in_arc(ns.in_arc), _search_arc_num(ns._search_arc_num)
      {}

      // Find next entering arc
      bool findEnteringArc() {
        Cost c, min = 0;
        for (int e = 0; e != _search_arc_num; ++e) {
          c = _state[e] * (_cost[e] + _pi[_source[e]] - _pi[_target[e]]);
          if (c < min) {
            min = c;
            _in_arc = e;
          }
        }
        return min < 0;
      }

    }; //class BestEligiblePivotRule


    // Implementation of the Block Search pivot rule
    class BlockSearchPivotRule
    {
    private:

      // References to the NetworkSimplex class
      const IntVector  &_source;
      const IntVector  &_target;
      const CostVector &_cost;
      const CharVector &_state;
      const CostVector &_pi;
      int &_in_arc;
      int _search_arc_num;

      // Pivot rule data
      int _block_size;
      int _next_arc;

    public:

      // Constructor
      BlockSearchPivotRule(NetworkSimplex &ns) :
        _source(ns._source), _target(ns._target),
        _cost(ns._cost), _state(ns._state), _pi(ns._pi),
        _in_arc(ns.in_arc), _search_arc_num(ns._search_arc_num),
        _next_arc(0)
      {
        // The main parameters of the pivot rule
        const double BLOCK_SIZE_FACTOR = 1.0;
        const int MIN_BLOCK_SIZE = 10;

        _block_size = std::max( int(BLOCK_SIZE_FACTOR *
                                    std::sqrt(double(_search_arc_num))),
                                MIN_BLOCK_SIZE );
      }

      // Find next entering arc
      bool findEnteringArc() {
        Cost c, min = 0;
        int cnt = _block_size;
        int e;
        for (e = _next_arc; e != _search_arc_num; ++e) {
          c = _state[e] * (_cost[e] + _pi[_source[e]] - _pi[_target[e]]);
          if (c < min) {
            min = c;
            _in_arc = e;
          }
          if (--cnt == 0) {
            if (min < 0) goto search_end;
            cnt = _block_size;
          }
        }
        for (e = 0; e != _next_arc; ++e) {
          c = _state[e] * (_cost[e] + _pi[_source[e]] - _pi[_target[e]]);
          if (c < min) {
            min = c;
            _in_arc = e;
          }
          if (--cnt == 0) {
            if (min < 0) goto search_end;
            cnt = _block_size;
          }
        }
        if (min >= 0) return false;

      search_end:
        _next_arc = e;
        return true;
      }

    }; //class BlockSearchPivotRule


    // Implementation of the Candidate List pivot rule
    class CandidateListPivotRule
    {
    private:

      // References to the NetworkSimplex class
      const IntVector  &_source;
      const IntVector  &_target;
      const CostVector &_cost;
      const CharVector &_state;
      const CostVector &_pi;
      int &_in_arc;
      int _search_arc_num;

      // Pivot rule data
      IntVector _candidates;
      int _list_length, _minor_limit;
      int _curr_length, _minor_count;
      int _next_arc;

    public:

      /// Constructor
      CandidateListPivotRule(NetworkSimplex &ns) :
        _source(ns._source), _target(ns._target),
        _cost(ns._cost), _state(ns._state), _pi(ns._pi),
        _in_arc(ns.in_arc), _search_arc_num(ns._search_arc_num),
        _next_arc(0)
      {
        // The main parameters of the pivot rule
        const double LIST_LENGTH_FACTOR = 0.25;
        const int MIN_LIST_LENGTH = 10;
        const double MINOR_LIMIT_FACTOR = 0.1;
        const int MIN_MINOR_LIMIT = 3;

        _list_length = std::max( int(LIST_LENGTH_FACTOR *
                                     std::sqrt(double(_search_arc_num))),
                                 MIN_LIST_LENGTH );
        _minor_limit = std::max( int(MINOR_LIMIT_FACTOR * _list_length),
                                 MIN_MINOR_LIMIT );
        _curr_length = _minor_count = 0;
        _candidates.resize(_list_length);
      }

      /// Find next entering arc
      bool findEnteringArc() {
        Cost min, c;
        int e;
        if (_curr_length > 0 && _minor_count < _minor_limit) {
          // Minor iteration: select the best eligible arc from the
          // current candidate list
          ++_minor_count;
          min = 0;
          for (int i = 0; i < _curr_length; ++i) {
            e = _candidates[i];
            c = _state[e] * (_cost[e] + _pi[_source[e]] - _pi[_target[e]]);
            if (c < min) {
              min = c;
              _in_arc = e;
            }
            else if (c >= 0) {
              _candidates[i--] = _candidates[--_curr_length];
            }
          }
          if (min < 0) return true;
        }

        // Major iteration: build a new candidate list
        min = 0;
        _curr_length = 0;
        for (e = _next_arc; e != _search_arc_num; ++e) {
          c = _state[e] * (_cost[e] + _pi[_source[e]] - _pi[_target[e]]);
          if (c < 0) {
            _candidates[_curr_length++] = e;
            if (c < min) {
              min = c;
              _in_arc = e;
            }
            if (_curr_length == _list_length) goto search_end;
          }
        }
        for (e = 0; e != _next_arc; ++e) {
          c = _state[e] * (_cost[e] + _pi[_source[e]] - _pi[_target[e]]);
          if (c < 0) {
            _candidates[_curr_length++] = e;
            if (c < min) {
              min = c;
              _in_arc = e;
            }
            if (_curr_length == _list_length) goto search_end;
          }
        }
        if (_curr_length == 0) return false;

      search_end:
        _minor_count = 1;
        _next_arc = e;
        return true;
      }

    }; //class CandidateListPivotRule


    // Implementation of the Altering Candidate List pivot rule
    class AlteringListPivotRule
    {
    private:

      // References to the NetworkSimplex class
      const IntVector  &_source;
      const IntVector  &_target;
      const CostVector &_cost;
      const CharVector &_state;
      const CostVector &_pi;
      int &_in_arc;
      int _search_arc_num;

      // Pivot rule data
      int _block_size, _head_length, _curr_length;
      int _next_arc;
      IntVector _candidates;
      CostVector _cand_cost;

      // Functor class to compare arcs during sort of the candidate list
      class SortFunc
      {
      private:
        const CostVector &_map;
      public:
        SortFunc(const CostVector &map) : _map(map) {}
        bool operator()(int left, int right) {
          return _map[left] < _map[right];
        }
      };

      SortFunc _sort_func;

    public:

      // Constructor
      AlteringListPivotRule(NetworkSimplex &ns) :
        _source(ns._source), _target(ns._target),
        _cost(ns._cost), _state(ns._state), _pi(ns._pi),
        _in_arc(ns.in_arc), _search_arc_num(ns._search_arc_num),
        _next_arc(0), _cand_cost(ns._search_arc_num), _sort_func(_cand_cost)
      {
        // The main parameters of the pivot rule
        const double BLOCK_SIZE_FACTOR = 1.0;
        const int MIN_BLOCK_SIZE = 10;
        const double HEAD_LENGTH_FACTOR = 0.01;
        const int MIN_HEAD_LENGTH = 3;

        _block_size = std::max( int(BLOCK_SIZE_FACTOR *
                                    std::sqrt(double(_search_arc_num))),
                                MIN_BLOCK_SIZE );
        _head_length = std::max( int(HEAD_LENGTH_FACTOR * _block_size),
                                 MIN_HEAD_LENGTH );
        _candidates.resize(_head_length + _block_size);
        _curr_length = 0;
      }

      // Find next entering arc
      bool findEnteringArc() {
        // Check the current candidate list
        int e;
        Cost c;
        for (int i = 0; i != _curr_length; ++i) {
          e = _candidates[i];
          c = _state[e] * (_cost[e] + _pi[_source[e]] - _pi[_target[e]]);
          if (c < 0) {
            _cand_cost[e] = c;
          } else {
            _candidates[i--] = _candidates[--_curr_length];
          }
        }

        // Extend the list
        int cnt = _block_size;
        int limit = _head_length;

        for (e = _next_arc; e != _search_arc_num; ++e) {
          c = _state[e] * (_cost[e] + _pi[_source[e]] - _pi[_target[e]]);
          if (c < 0) {
            _cand_cost[e] = c;
            _candidates[_curr_length++] = e;
          }
          if (--cnt == 0) {
            if (_curr_length > limit) goto search_end;
            limit = 0;
            cnt = _block_size;
          }
        }
        for (e = 0; e != _next_arc; ++e) {
          c = _state[e] * (_cost[e] + _pi[_source[e]] - _pi[_target[e]]);
          if (c < 0) {
            _cand_cost[e] = c;
            _candidates[_curr_length++] = e;
          }
          if (--cnt == 0) {
            if (_curr_length > limit) goto search_end;
            limit = 0;
            cnt = _block_size;
          }
        }
        if (_curr_length == 0) return false;

      search_end:

        // Perform partial sort operation on the candidate list
        int new_length = std::min(_head_length + 1, _curr_length);
        std::partial_sort(_candidates.begin(), _candidates.begin() + new_length,
                          _candidates.begin() + _curr_length, _sort_func);

        // Select the entering arc and remove it from the list
        _in_arc = _candidates[0];
        _next_arc = e;
        _candidates[0] = _candidates[new_length - 1];
        _curr_length = new_length - 1;
        return true;
      }

    }; //class AlteringListPivotRule

  public:

    /// \brief Constructor.
    ///
    /// The constructor of the class.
    ///
    /// \param graph The digraph the algorithm runs on.
    /// \param arc_mixing Indicate if the arcs will be stored in a
    /// mixed order in the internal data structure.
    /// In general, it leads to similar performance as using the original
    /// arc order, but it makes the algorithm more robust and in special
    /// cases, even significantly faster. Therefore, it is enabled by default.
    NetworkSimplex(const GR& graph, bool arc_mixing = true) :
      _graph(graph), _node_id(graph), _arc_id(graph),
      _arc_mixing(arc_mixing),
      MAX(std::numeric_limits<Value>::max()),
      INF(std::numeric_limits<Value>::has_infinity ?
          std::numeric_limits<Value>::infinity() : MAX)
    {
      // Check the number types
      LEMON_ASSERT(std::numeric_limits<Value>::is_signed,
        "The flow type of NetworkSimplex must be signed");
      LEMON_ASSERT(std::numeric_limits<Cost>::is_signed,
        "The cost type of NetworkSimplex must be signed");

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
    NetworkSimplex& lowerMap(const LowerMap& map) {
      _has_lower = true;
      for (ArcIt a(_graph); a != INVALID; ++a) {
        _lower[_arc_id[a]] = map[a];
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
    NetworkSimplex& upperMap(const UpperMap& map) {
      for (ArcIt a(_graph); a != INVALID; ++a) {
        _upper[_arc_id[a]] = map[a];
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
    NetworkSimplex& costMap(const CostMap& map) {
      for (ArcIt a(_graph); a != INVALID; ++a) {
        _cost[_arc_id[a]] = map[a];
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
    ///
    /// \sa supplyType()
    template<typename SupplyMap>
    NetworkSimplex& supplyMap(const SupplyMap& map) {
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
    NetworkSimplex& stSupply(const Node& s, const Node& t, Value k) {
      for (int i = 0; i != _node_num; ++i) {
        _supply[i] = 0;
      }
      _supply[_node_id[s]] =  k;
      _supply[_node_id[t]] = -k;
      return *this;
    }

    /// \brief Set the type of the supply constraints.
    ///
    /// This function sets the type of the supply/demand constraints.
    /// If it is not used before calling \ref run(), the \ref GEQ supply
    /// type will be used.
    ///
    /// For more information, see \ref SupplyType.
    ///
    /// \return <tt>(*this)</tt>
    NetworkSimplex& supplyType(SupplyType supply_type) {
      _stype = supply_type;
      return *this;
    }

    /// @}

    /// \name Execution Control
    /// The algorithm can be executed using \ref run().

    /// @{

    /// \brief Run the algorithm.
    ///
    /// This function runs the algorithm.
    /// The paramters can be specified using functions \ref lowerMap(),
    /// \ref upperMap(), \ref costMap(), \ref supplyMap(), \ref stSupply(),
    /// \ref supplyType().
    /// For example,
    /// \code
    ///   NetworkSimplex<ListDigraph> ns(graph);
    ///   ns.lowerMap(lower).upperMap(upper).costMap(cost)
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
    /// \param pivot_rule The pivot rule that will be used during the
    /// algorithm. For more information, see \ref PivotRule.
    ///
    /// \return \c INFEASIBLE if no feasible flow exists,
    /// \n \c OPTIMAL if the problem has optimal solution
    /// (i.e. it is feasible and bounded), and the algorithm has found
    /// optimal flow and node potentials (primal and dual solutions),
    /// \n \c UNBOUNDED if the objective function of the problem is
    /// unbounded, i.e. there is a directed cycle having negative total
    /// cost and infinite upper bound.
    ///
    /// \see ProblemType, PivotRule
    /// \see resetParams(), reset()
    ProblemType run(PivotRule pivot_rule = BLOCK_SEARCH) {
      if (!init()) return INFEASIBLE;
      return start(pivot_rule);
    }

    /// \brief Reset all the parameters that have been given before.
    ///
    /// This function resets all the paramaters that have been given
    /// before using functions \ref lowerMap(), \ref upperMap(),
    /// \ref costMap(), \ref supplyMap(), \ref stSupply(), \ref supplyType().
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
    ///   NetworkSimplex<ListDigraph> ns(graph);
    ///
    ///   // First run
    ///   ns.lowerMap(lower).upperMap(upper).costMap(cost)
    ///     .supplyMap(sup).run();
    ///
    ///   // Run again with modified cost map (resetParams() is not called,
    ///   // so only the cost map have to be set again)
    ///   cost[e] += 100;
    ///   ns.costMap(cost).run();
    ///
    ///   // Run again from scratch using resetParams()
    ///   // (the lower bounds will be set to zero on all arcs)
    ///   ns.resetParams();
    ///   ns.upperMap(capacity).costMap(cost)
    ///     .supplyMap(sup).run();
    /// \endcode
    ///
    /// \return <tt>(*this)</tt>
    ///
    /// \see reset(), run()
    NetworkSimplex& resetParams() {
      for (int i = 0; i != _node_num; ++i) {
        _supply[i] = 0;
      }
      for (int i = 0; i != _arc_num; ++i) {
        _lower[i] = 0;
        _upper[i] = INF;
        _cost[i] = 1;
      }
      _has_lower = false;
      _stype = GEQ;
      return *this;
    }

    /// \brief Reset the internal data structures and all the parameters
    /// that have been given before.
    ///
    /// This function resets the internal data structures and all the
    /// paramaters that have been given before using functions \ref lowerMap(),
    /// \ref upperMap(), \ref costMap(), \ref supplyMap(), \ref stSupply(),
    /// \ref supplyType().
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
    NetworkSimplex& reset() {
      // Resize vectors
      _node_num = countNodes(_graph);
      _arc_num = countArcs(_graph);
      int all_node_num = _node_num + 1;
      int max_arc_num = _arc_num + 2 * _node_num;

      _source.resize(max_arc_num);
      _target.resize(max_arc_num);

      _lower.resize(_arc_num);
      _upper.resize(_arc_num);
      _cap.resize(max_arc_num);
      _cost.resize(max_arc_num);
      _supply.resize(all_node_num);
      _flow.resize(max_arc_num);
      _pi.resize(all_node_num);

      _parent.resize(all_node_num);
      _pred.resize(all_node_num);
      _pred_dir.resize(all_node_num);
      _thread.resize(all_node_num);
      _rev_thread.resize(all_node_num);
      _succ_num.resize(all_node_num);
      _last_succ.resize(all_node_num);
      _state.resize(max_arc_num);

      // Copy the graph
      int i = 0;
      for (NodeIt n(_graph); n != INVALID; ++n, ++i) {
        _node_id[n] = i;
      }
      if (_arc_mixing && _node_num > 1) {
        // Store the arcs in a mixed order
        const int skip = std::max(_arc_num / _node_num, 3);
        int i = 0, j = 0;
        for (ArcIt a(_graph); a != INVALID; ++a) {
          _arc_id[a] = i;
          _source[i] = _node_id[_graph.source(a)];
          _target[i] = _node_id[_graph.target(a)];
          if ((i += skip) >= _arc_num) i = ++j;
        }
      } else {
        // Store the arcs in the original order
        int i = 0;
        for (ArcIt a(_graph); a != INVALID; ++a, ++i) {
          _arc_id[a] = i;
          _source[i] = _node_id[_graph.source(a)];
          _target[i] = _node_id[_graph.target(a)];
        }
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
    ///   ns.totalCost<double>();
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
        int i = _arc_id[a];
        c += Number(_flow[i]) * Number(_cost[i]);
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
      return _flow[_arc_id[a]];
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
        map.set(a, _flow[_arc_id[a]]);
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

    // Initialize internal data structures
    bool init() {
      if (_node_num == 0) return false;

      // Check the sum of supply values
      _sum_supply = 0;
      for (int i = 0; i != _node_num; ++i) {
        _sum_supply += _supply[i];
      }
      if ( !((_stype == GEQ && _sum_supply <= 0) ||
             (_stype == LEQ && _sum_supply >= 0)) ) return false;

      // Check lower and upper bounds
      LEMON_DEBUG(checkBoundMaps(),
          "Upper bounds must be greater or equal to the lower bounds");

      // Remove non-zero lower bounds
      if (_has_lower) {
        for (int i = 0; i != _arc_num; ++i) {
          Value c = _lower[i];
          if (c >= 0) {
            _cap[i] = _upper[i] < MAX ? _upper[i] - c : INF;
          } else {
            _cap[i] = _upper[i] < MAX + c ? _upper[i] - c : INF;
          }
          _supply[_source[i]] -= c;
          _supply[_target[i]] += c;
        }
      } else {
        for (int i = 0; i != _arc_num; ++i) {
          _cap[i] = _upper[i];
        }
      }

      // Initialize artifical cost
      Cost ART_COST;
      if (std::numeric_limits<Cost>::is_exact) {
        ART_COST = std::numeric_limits<Cost>::max() / 2 + 1;
      } else {
        ART_COST = 0;
        for (int i = 0; i != _arc_num; ++i) {
          if (_cost[i] > ART_COST) ART_COST = _cost[i];
        }
        ART_COST = (ART_COST + 1) * _node_num;
      }

      // Initialize arc maps
      for (int i = 0; i != _arc_num; ++i) {
        _flow[i] = 0;
        _state[i] = STATE_LOWER;
      }

      // Set data for the artificial root node
      _root = _node_num;
      _parent[_root] = -1;
      _pred[_root] = -1;
      _thread[_root] = 0;
      _rev_thread[0] = _root;
      _succ_num[_root] = _node_num + 1;
      _last_succ[_root] = _root - 1;
      _supply[_root] = -_sum_supply;
      _pi[_root] = 0;

      // Add artificial arcs and initialize the spanning tree data structure
      if (_sum_supply == 0) {
        // EQ supply constraints
        _search_arc_num = _arc_num;
        _all_arc_num = _arc_num + _node_num;
        for (int u = 0, e = _arc_num; u != _node_num; ++u, ++e) {
          _parent[u] = _root;
          _pred[u] = e;
          _thread[u] = u + 1;
          _rev_thread[u + 1] = u;
          _succ_num[u] = 1;
          _last_succ[u] = u;
          _cap[e] = INF;
          _state[e] = STATE_TREE;
          if (_supply[u] >= 0) {
            _pred_dir[u] = DIR_UP;
            _pi[u] = 0;
            _source[e] = u;
            _target[e] = _root;
            _flow[e] = _supply[u];
            _cost[e] = 0;
          } else {
            _pred_dir[u] = DIR_DOWN;
            _pi[u] = ART_COST;
            _source[e] = _root;
            _target[e] = u;
            _flow[e] = -_supply[u];
            _cost[e] = ART_COST;
          }
        }
      }
      else if (_sum_supply > 0) {
        // LEQ supply constraints
        _search_arc_num = _arc_num + _node_num;
        int f = _arc_num + _node_num;
        for (int u = 0, e = _arc_num; u != _node_num; ++u, ++e) {
          _parent[u] = _root;
          _thread[u] = u + 1;
          _rev_thread[u + 1] = u;
          _succ_num[u] = 1;
          _last_succ[u] = u;
          if (_supply[u] >= 0) {
            _pred_dir[u] = DIR_UP;
            _pi[u] = 0;
            _pred[u] = e;
            _source[e] = u;
            _target[e] = _root;
            _cap[e] = INF;
            _flow[e] = _supply[u];
            _cost[e] = 0;
            _state[e] = STATE_TREE;
          } else {
            _pred_dir[u] = DIR_DOWN;
            _pi[u] = ART_COST;
            _pred[u] = f;
            _source[f] = _root;
            _target[f] = u;
            _cap[f] = INF;
            _flow[f] = -_supply[u];
            _cost[f] = ART_COST;
            _state[f] = STATE_TREE;
            _source[e] = u;
            _target[e] = _root;
            _cap[e] = INF;
            _flow[e] = 0;
            _cost[e] = 0;
            _state[e] = STATE_LOWER;
            ++f;
          }
        }
        _all_arc_num = f;
      }
      else {
        // GEQ supply constraints
        _search_arc_num = _arc_num + _node_num;
        int f = _arc_num + _node_num;
        for (int u = 0, e = _arc_num; u != _node_num; ++u, ++e) {
          _parent[u] = _root;
          _thread[u] = u + 1;
          _rev_thread[u + 1] = u;
          _succ_num[u] = 1;
          _last_succ[u] = u;
          if (_supply[u] <= 0) {
            _pred_dir[u] = DIR_DOWN;
            _pi[u] = 0;
            _pred[u] = e;
            _source[e] = _root;
            _target[e] = u;
            _cap[e] = INF;
            _flow[e] = -_supply[u];
            _cost[e] = 0;
            _state[e] = STATE_TREE;
          } else {
            _pred_dir[u] = DIR_UP;
            _pi[u] = -ART_COST;
            _pred[u] = f;
            _source[f] = u;
            _target[f] = _root;
            _cap[f] = INF;
            _flow[f] = _supply[u];
            _state[f] = STATE_TREE;
            _cost[f] = ART_COST;
            _source[e] = _root;
            _target[e] = u;
            _cap[e] = INF;
            _flow[e] = 0;
            _cost[e] = 0;
            _state[e] = STATE_LOWER;
            ++f;
          }
        }
        _all_arc_num = f;
      }

      return true;
    }

    // Check if the upper bound is greater than or equal to the lower bound
    // on each arc.
    bool checkBoundMaps() {
      for (int j = 0; j != _arc_num; ++j) {
        if (_upper[j] < _lower[j]) return false;
      }
      return true;
    }

    // Find the join node
    void findJoinNode() {
      int u = _source[in_arc];
      int v = _target[in_arc];
      while (u != v) {
        if (_succ_num[u] < _succ_num[v]) {
          u = _parent[u];
        } else {
          v = _parent[v];
        }
      }
      join = u;
    }

    // Find the leaving arc of the cycle and returns true if the
    // leaving arc is not the same as the entering arc
    bool findLeavingArc() {
      // Initialize first and second nodes according to the direction
      // of the cycle
      int first, second;
      if (_state[in_arc] == STATE_LOWER) {
        first  = _source[in_arc];
        second = _target[in_arc];
      } else {
        first  = _target[in_arc];
        second = _source[in_arc];
      }
      delta = _cap[in_arc];
      int result = 0;
      Value c, d;
      int e;

      // Search the cycle form the first node to the join node
      for (int u = first; u != join; u = _parent[u]) {
        e = _pred[u];
        d = _flow[e];
        if (_pred_dir[u] == DIR_DOWN) {
          c = _cap[e];
          d = c >= MAX ? INF : c - d;
        }
        if (d < delta) {
          delta = d;
          u_out = u;
          result = 1;
        }
      }

      // Search the cycle form the second node to the join node
      for (int u = second; u != join; u = _parent[u]) {
        e = _pred[u];
        d = _flow[e];
        if (_pred_dir[u] == DIR_UP) {
          c = _cap[e];
          d = c >= MAX ? INF : c - d;
        }
        if (d <= delta) {
          delta = d;
          u_out = u;
          result = 2;
        }
      }

      if (result == 1) {
        u_in = first;
        v_in = second;
      } else {
        u_in = second;
        v_in = first;
      }
      return result != 0;
    }

    // Change _flow and _state vectors
    void changeFlow(bool change) {
      // Augment along the cycle
      if (delta > 0) {
        Value val = _state[in_arc] * delta;
        _flow[in_arc] += val;
        for (int u = _source[in_arc]; u != join; u = _parent[u]) {
          _flow[_pred[u]] -= _pred_dir[u] * val;
        }
        for (int u = _target[in_arc]; u != join; u = _parent[u]) {
          _flow[_pred[u]] += _pred_dir[u] * val;
        }
      }
      // Update the state of the entering and leaving arcs
      if (change) {
        _state[in_arc] = STATE_TREE;
        _state[_pred[u_out]] =
          (_flow[_pred[u_out]] == 0) ? STATE_LOWER : STATE_UPPER;
      } else {
        _state[in_arc] = -_state[in_arc];
      }
    }

    // Update the tree structure
    void updateTreeStructure() {
      int old_rev_thread = _rev_thread[u_out];
      int old_succ_num = _succ_num[u_out];
      int old_last_succ = _last_succ[u_out];
      v_out = _parent[u_out];

      // Check if u_in and u_out coincide
      if (u_in == u_out) {
        // Update _parent, _pred, _pred_dir
        _parent[u_in] = v_in;
        _pred[u_in] = in_arc;
        _pred_dir[u_in] = u_in == _source[in_arc] ? DIR_UP : DIR_DOWN;

        // Update _thread and _rev_thread
        if (_thread[v_in] != u_out) {
          int after = _thread[old_last_succ];
          _thread[old_rev_thread] = after;
          _rev_thread[after] = old_rev_thread;
          after = _thread[v_in];
          _thread[v_in] = u_out;
          _rev_thread[u_out] = v_in;
          _thread[old_last_succ] = after;
          _rev_thread[after] = old_last_succ;
        }
      } else {
        // Handle the case when old_rev_thread equals to v_in
        // (it also means that join and v_out coincide)
        int thread_continue = old_rev_thread == v_in ?
          _thread[old_last_succ] : _thread[v_in];

        // Update _thread and _parent along the stem nodes (i.e. the nodes
        // between u_in and u_out, whose parent have to be changed)
        int stem = u_in;              // the current stem node
        int par_stem = v_in;          // the new parent of stem
        int next_stem;                // the next stem node
        int last = _last_succ[u_in];  // the last successor of stem
        int before, after = _thread[last];
        _thread[v_in] = u_in;
        _dirty_revs.clear();
        _dirty_revs.push_back(v_in);
        while (stem != u_out) {
          // Insert the next stem node into the thread list
          next_stem = _parent[stem];
          _thread[last] = next_stem;
          _dirty_revs.push_back(last);

          // Remove the subtree of stem from the thread list
          before = _rev_thread[stem];
          _thread[before] = after;
          _rev_thread[after] = before;

          // Change the parent node and shift stem nodes
          _parent[stem] = par_stem;
          par_stem = stem;
          stem = next_stem;

          // Update last and after
          last = _last_succ[stem] == _last_succ[par_stem] ?
            _rev_thread[par_stem] : _last_succ[stem];
          after = _thread[last];
        }
        _parent[u_out] = par_stem;
        _thread[last] = thread_continue;
        _rev_thread[thread_continue] = last;
        _last_succ[u_out] = last;

        // Remove the subtree of u_out from the thread list except for
        // the case when old_rev_thread equals to v_in
        if (old_rev_thread != v_in) {
          _thread[old_rev_thread] = after;
          _rev_thread[after] = old_rev_thread;
        }

        // Update _rev_thread using the new _thread values
        for (int i = 0; i != int(_dirty_revs.size()); ++i) {
          int u = _dirty_revs[i];
          _rev_thread[_thread[u]] = u;
        }

        // Update _pred, _pred_dir, _last_succ and _succ_num for the
        // stem nodes from u_out to u_in
        int tmp_sc = 0, tmp_ls = _last_succ[u_out];
        for (int u = u_out, p = _parent[u]; u != u_in; u = p, p = _parent[u]) {
          _pred[u] = _pred[p];
          _pred_dir[u] = -_pred_dir[p];
          tmp_sc += _succ_num[u] - _succ_num[p];
          _succ_num[u] = tmp_sc;
          _last_succ[p] = tmp_ls;
        }
        _pred[u_in] = in_arc;
        _pred_dir[u_in] = u_in == _source[in_arc] ? DIR_UP : DIR_DOWN;
        _succ_num[u_in] = old_succ_num;
      }

      // Update _last_succ from v_in towards the root
      int up_limit_out = _last_succ[join] == v_in ? join : -1;
      int last_succ_out = _last_succ[u_out];
      for (int u = v_in; u != -1 && _last_succ[u] == v_in; u = _parent[u]) {
        _last_succ[u] = last_succ_out;
      }

      // Update _last_succ from v_out towards the root
      if (join != old_rev_thread && v_in != old_rev_thread) {
        for (int u = v_out; u != up_limit_out && _last_succ[u] == old_last_succ;
             u = _parent[u]) {
          _last_succ[u] = old_rev_thread;
        }
      }
      else if (last_succ_out != old_last_succ) {
        for (int u = v_out; u != up_limit_out && _last_succ[u] == old_last_succ;
             u = _parent[u]) {
          _last_succ[u] = last_succ_out;
        }
      }

      // Update _succ_num from v_in to join
      for (int u = v_in; u != join; u = _parent[u]) {
        _succ_num[u] += old_succ_num;
      }
      // Update _succ_num from v_out to join
      for (int u = v_out; u != join; u = _parent[u]) {
        _succ_num[u] -= old_succ_num;
      }
    }

    // Update potentials in the subtree that has been moved
    void updatePotential() {
      Cost sigma = _pi[v_in] - _pi[u_in] -
                   _pred_dir[u_in] * _cost[in_arc];
      int end = _thread[_last_succ[u_in]];
      for (int u = u_in; u != end; u = _thread[u]) {
        _pi[u] += sigma;
      }
    }

    // Heuristic initial pivots
    bool initialPivots() {
      Value curr, total = 0;
      std::vector<Node> supply_nodes, demand_nodes;
      for (NodeIt u(_graph); u != INVALID; ++u) {
        curr = _supply[_node_id[u]];
        if (curr > 0) {
          total += curr;
          supply_nodes.push_back(u);
        }
        else if (curr < 0) {
          demand_nodes.push_back(u);
        }
      }
      if (_sum_supply > 0) total -= _sum_supply;
      if (total <= 0) return true;

      IntVector arc_vector;
      if (_sum_supply >= 0) {
        if (supply_nodes.size() == 1 && demand_nodes.size() == 1) {
          // Perform a reverse graph search from the sink to the source
          typename GR::template NodeMap<bool> reached(_graph, false);
          Node s = supply_nodes[0], t = demand_nodes[0];
          std::vector<Node> stack;
          reached[t] = true;
          stack.push_back(t);
          while (!stack.empty()) {
            Node u, v = stack.back();
            stack.pop_back();
            if (v == s) break;
            for (InArcIt a(_graph, v); a != INVALID; ++a) {
              if (reached[u = _graph.source(a)]) continue;
              int j = _arc_id[a];
              if (_cap[j] >= total) {
                arc_vector.push_back(j);
                reached[u] = true;
                stack.push_back(u);
              }
            }
          }
        } else {
          // Find the min. cost incoming arc for each demand node
          for (int i = 0; i != int(demand_nodes.size()); ++i) {
            Node v = demand_nodes[i];
            Cost c, min_cost = std::numeric_limits<Cost>::max();
            Arc min_arc = INVALID;
            for (InArcIt a(_graph, v); a != INVALID; ++a) {
              c = _cost[_arc_id[a]];
              if (c < min_cost) {
                min_cost = c;
                min_arc = a;
              }
            }
            if (min_arc != INVALID) {
              arc_vector.push_back(_arc_id[min_arc]);
            }
          }
        }
      } else {
        // Find the min. cost outgoing arc for each supply node
        for (int i = 0; i != int(supply_nodes.size()); ++i) {
          Node u = supply_nodes[i];
          Cost c, min_cost = std::numeric_limits<Cost>::max();
          Arc min_arc = INVALID;
          for (OutArcIt a(_graph, u); a != INVALID; ++a) {
            c = _cost[_arc_id[a]];
            if (c < min_cost) {
              min_cost = c;
              min_arc = a;
            }
          }
          if (min_arc != INVALID) {
            arc_vector.push_back(_arc_id[min_arc]);
          }
        }
      }

      // Perform heuristic initial pivots
      for (int i = 0; i != int(arc_vector.size()); ++i) {
        in_arc = arc_vector[i];
        if (_state[in_arc] * (_cost[in_arc] + _pi[_source[in_arc]] -
            _pi[_target[in_arc]]) >= 0) continue;
        findJoinNode();
        bool change = findLeavingArc();
        if (delta >= MAX) return false;
        changeFlow(change);
        if (change) {
          updateTreeStructure();
          updatePotential();
        }
      }
      return true;
    }

    // Execute the algorithm
    ProblemType start(PivotRule pivot_rule) {
      // Select the pivot rule implementation
      switch (pivot_rule) {
        case FIRST_ELIGIBLE:
          return start<FirstEligiblePivotRule>();
        case BEST_ELIGIBLE:
          return start<BestEligiblePivotRule>();
        case BLOCK_SEARCH:
          return start<BlockSearchPivotRule>();
        case CANDIDATE_LIST:
          return start<CandidateListPivotRule>();
        case ALTERING_LIST:
          return start<AlteringListPivotRule>();
      }
      return INFEASIBLE; // avoid warning
    }

    template <typename PivotRuleImpl>
    ProblemType start() {
      PivotRuleImpl pivot(*this);

      // Perform heuristic initial pivots
      if (!initialPivots()) return UNBOUNDED;

      // Execute the Network Simplex algorithm
      while (pivot.findEnteringArc()) {
        findJoinNode();
        bool change = findLeavingArc();
        if (delta >= MAX) return UNBOUNDED;
        changeFlow(change);
        if (change) {
          updateTreeStructure();
          updatePotential();
        }
      }

      // Check feasibility
      for (int e = _search_arc_num; e != _all_arc_num; ++e) {
        if (_flow[e] != 0) return INFEASIBLE;
      }

      // Transform the solution and the supply map to the original form
      if (_has_lower) {
        for (int i = 0; i != _arc_num; ++i) {
          Value c = _lower[i];
          if (c != 0) {
            _flow[i] += c;
            _supply[_source[i]] += c;
            _supply[_target[i]] -= c;
          }
        }
      }

      // Shift potentials to meet the requirements of the GEQ/LEQ type
      // optimality conditions
      if (_sum_supply == 0) {
        if (_stype == GEQ) {
          Cost max_pot = -std::numeric_limits<Cost>::max();
          for (int i = 0; i != _node_num; ++i) {
            if (_pi[i] > max_pot) max_pot = _pi[i];
          }
          if (max_pot > 0) {
            for (int i = 0; i != _node_num; ++i)
              _pi[i] -= max_pot;
          }
        } else {
          Cost min_pot = std::numeric_limits<Cost>::max();
          for (int i = 0; i != _node_num; ++i) {
            if (_pi[i] < min_pot) min_pot = _pi[i];
          }
          if (min_pot < 0) {
            for (int i = 0; i != _node_num; ++i)
              _pi[i] -= min_pot;
          }
        }
      }

      return OPTIMAL;
    }

  }; //class NetworkSimplex

  ///@}

} //namespace lemon

#endif //LEMON_NETWORK_SIMPLEX_H
