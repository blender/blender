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

#ifndef LEMON_HOWARD_MMC_H
#define LEMON_HOWARD_MMC_H

/// \ingroup min_mean_cycle
///
/// \file
/// \brief Howard's algorithm for finding a minimum mean cycle.

#include <vector>
#include <limits>
#include <lemon/core.h>
#include <lemon/path.h>
#include <lemon/tolerance.h>
#include <lemon/connectivity.h>

namespace lemon {

  /// \brief Default traits class of HowardMmc class.
  ///
  /// Default traits class of HowardMmc class.
  /// \tparam GR The type of the digraph.
  /// \tparam CM The type of the cost map.
  /// It must conform to the \ref concepts::ReadMap "ReadMap" concept.
#ifdef DOXYGEN
  template <typename GR, typename CM>
#else
  template <typename GR, typename CM,
    bool integer = std::numeric_limits<typename CM::Value>::is_integer>
#endif
  struct HowardMmcDefaultTraits
  {
    /// The type of the digraph
    typedef GR Digraph;
    /// The type of the cost map
    typedef CM CostMap;
    /// The type of the arc costs
    typedef typename CostMap::Value Cost;

    /// \brief The large cost type used for internal computations
    ///
    /// The large cost type used for internal computations.
    /// It is \c long \c long if the \c Cost type is integer,
    /// otherwise it is \c double.
    /// \c Cost must be convertible to \c LargeCost.
    typedef double LargeCost;

    /// The tolerance type used for internal computations
    typedef lemon::Tolerance<LargeCost> Tolerance;

    /// \brief The path type of the found cycles
    ///
    /// The path type of the found cycles.
    /// It must conform to the \ref lemon::concepts::Path "Path" concept
    /// and it must have an \c addBack() function.
    typedef lemon::Path<Digraph> Path;
  };

  // Default traits class for integer cost types
  template <typename GR, typename CM>
  struct HowardMmcDefaultTraits<GR, CM, true>
  {
    typedef GR Digraph;
    typedef CM CostMap;
    typedef typename CostMap::Value Cost;
#ifdef LEMON_HAVE_LONG_LONG
    typedef long long LargeCost;
#else
    typedef long LargeCost;
#endif
    typedef lemon::Tolerance<LargeCost> Tolerance;
    typedef lemon::Path<Digraph> Path;
  };


  /// \addtogroup min_mean_cycle
  /// @{

  /// \brief Implementation of Howard's algorithm for finding a minimum
  /// mean cycle.
  ///
  /// This class implements Howard's policy iteration algorithm for finding
  /// a directed cycle of minimum mean cost in a digraph
  /// \cite dasdan98minmeancycle, \cite dasdan04experimental.
  /// This class provides the most efficient algorithm for the
  /// minimum mean cycle problem, though the best known theoretical
  /// bound on its running time is exponential.
  ///
  /// \tparam GR The type of the digraph the algorithm runs on.
  /// \tparam CM The type of the cost map. The default
  /// map type is \ref concepts::Digraph::ArcMap "GR::ArcMap<int>".
  /// \tparam TR The traits class that defines various types used by the
  /// algorithm. By default, it is \ref HowardMmcDefaultTraits
  /// "HowardMmcDefaultTraits<GR, CM>".
  /// In most cases, this parameter should not be set directly,
  /// consider to use the named template parameters instead.
#ifdef DOXYGEN
  template <typename GR, typename CM, typename TR>
#else
  template < typename GR,
             typename CM = typename GR::template ArcMap<int>,
             typename TR = HowardMmcDefaultTraits<GR, CM> >
#endif
  class HowardMmc
  {
  public:

    /// The type of the digraph
    typedef typename TR::Digraph Digraph;
    /// The type of the cost map
    typedef typename TR::CostMap CostMap;
    /// The type of the arc costs
    typedef typename TR::Cost Cost;

    /// \brief The large cost type
    ///
    /// The large cost type used for internal computations.
    /// By default, it is \c long \c long if the \c Cost type is integer,
    /// otherwise it is \c double.
    typedef typename TR::LargeCost LargeCost;

    /// The tolerance type
    typedef typename TR::Tolerance Tolerance;

    /// \brief The path type of the found cycles
    ///
    /// The path type of the found cycles.
    /// Using the \ref lemon::HowardMmcDefaultTraits "default traits class",
    /// it is \ref lemon::Path "Path<Digraph>".
    typedef typename TR::Path Path;

    /// The \ref lemon::HowardMmcDefaultTraits "traits class" of the algorithm
    typedef TR Traits;

    /// \brief Constants for the causes of search termination.
    ///
    /// Enum type containing constants for the different causes of search
    /// termination. The \ref findCycleMean() function returns one of
    /// these values.
    enum TerminationCause {

      /// No directed cycle can be found in the digraph.
      NO_CYCLE = 0,

      /// Optimal solution (minimum cycle mean) is found.
      OPTIMAL = 1,

      /// The iteration count limit is reached.
      ITERATION_LIMIT
    };

  private:

    TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

    // The digraph the algorithm runs on
    const Digraph &_gr;
    // The cost of the arcs
    const CostMap &_cost;

    // Data for the found cycles
    bool _curr_found, _best_found;
    LargeCost _curr_cost, _best_cost;
    int _curr_size, _best_size;
    Node _curr_node, _best_node;

    Path *_cycle_path;
    bool _local_path;

    // Internal data used by the algorithm
    typename Digraph::template NodeMap<Arc> _policy;
    typename Digraph::template NodeMap<bool> _reached;
    typename Digraph::template NodeMap<int> _level;
    typename Digraph::template NodeMap<LargeCost> _dist;

    // Data for storing the strongly connected components
    int _comp_num;
    typename Digraph::template NodeMap<int> _comp;
    std::vector<std::vector<Node> > _comp_nodes;
    std::vector<Node>* _nodes;
    typename Digraph::template NodeMap<std::vector<Arc> > _in_arcs;

    // Queue used for BFS search
    std::vector<Node> _queue;
    int _qfront, _qback;

    Tolerance _tolerance;

    // Infinite constant
    const LargeCost INF;

  public:

    /// \name Named Template Parameters
    /// @{

    template <typename T>
    struct SetLargeCostTraits : public Traits {
      typedef T LargeCost;
      typedef lemon::Tolerance<T> Tolerance;
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// \c LargeCost type.
    ///
    /// \ref named-templ-param "Named parameter" for setting \c LargeCost
    /// type. It is used for internal computations in the algorithm.
    template <typename T>
    struct SetLargeCost
      : public HowardMmc<GR, CM, SetLargeCostTraits<T> > {
      typedef HowardMmc<GR, CM, SetLargeCostTraits<T> > Create;
    };

    template <typename T>
    struct SetPathTraits : public Traits {
      typedef T Path;
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// \c %Path type.
    ///
    /// \ref named-templ-param "Named parameter" for setting the \c %Path
    /// type of the found cycles.
    /// It must conform to the \ref lemon::concepts::Path "Path" concept
    /// and it must have an \c addBack() function.
    template <typename T>
    struct SetPath
      : public HowardMmc<GR, CM, SetPathTraits<T> > {
      typedef HowardMmc<GR, CM, SetPathTraits<T> > Create;
    };

    /// @}

  protected:

    HowardMmc() {}

  public:

    /// \brief Constructor.
    ///
    /// The constructor of the class.
    ///
    /// \param digraph The digraph the algorithm runs on.
    /// \param cost The costs of the arcs.
    HowardMmc( const Digraph &digraph,
               const CostMap &cost ) :
      _gr(digraph), _cost(cost), _best_found(false),
      _best_cost(0), _best_size(1), _cycle_path(NULL), _local_path(false),
      _policy(digraph), _reached(digraph), _level(digraph), _dist(digraph),
      _comp(digraph), _in_arcs(digraph),
      INF(std::numeric_limits<LargeCost>::has_infinity ?
          std::numeric_limits<LargeCost>::infinity() :
          std::numeric_limits<LargeCost>::max())
    {}

    /// Destructor.
    ~HowardMmc() {
      if (_local_path) delete _cycle_path;
    }

    /// \brief Set the path structure for storing the found cycle.
    ///
    /// This function sets an external path structure for storing the
    /// found cycle.
    ///
    /// If you don't call this function before calling \ref run() or
    /// \ref findCycleMean(), a local \ref Path "path" structure
    /// will be allocated. The destuctor deallocates this automatically
    /// allocated object, of course.
    ///
    /// \note The algorithm calls only the \ref lemon::Path::addBack()
    /// "addBack()" function of the given path structure.
    ///
    /// \return <tt>(*this)</tt>
    HowardMmc& cycle(Path &path) {
      if (_local_path) {
        delete _cycle_path;
        _local_path = false;
      }
      _cycle_path = &path;
      return *this;
    }

    /// \brief Set the tolerance used by the algorithm.
    ///
    /// This function sets the tolerance object used by the algorithm.
    ///
    /// \return <tt>(*this)</tt>
    HowardMmc& tolerance(const Tolerance& tolerance) {
      _tolerance = tolerance;
      return *this;
    }

    /// \brief Return a const reference to the tolerance.
    ///
    /// This function returns a const reference to the tolerance object
    /// used by the algorithm.
    const Tolerance& tolerance() const {
      return _tolerance;
    }

    /// \name Execution control
    /// The simplest way to execute the algorithm is to call the \ref run()
    /// function.\n
    /// If you only need the minimum mean cost, you may call
    /// \ref findCycleMean().

    /// @{

    /// \brief Run the algorithm.
    ///
    /// This function runs the algorithm.
    /// It can be called more than once (e.g. if the underlying digraph
    /// and/or the arc costs have been modified).
    ///
    /// \return \c true if a directed cycle exists in the digraph.
    ///
    /// \note <tt>mmc.run()</tt> is just a shortcut of the following code.
    /// \code
    ///   return mmc.findCycleMean() && mmc.findCycle();
    /// \endcode
    bool run() {
      return findCycleMean() && findCycle();
    }

    /// \brief Find the minimum cycle mean (or an upper bound).
    ///
    /// This function finds the minimum mean cost of the directed
    /// cycles in the digraph (or an upper bound for it).
    ///
    /// By default, the function finds the exact minimum cycle mean,
    /// but an optional limit can also be specified for the number of
    /// iterations performed during the search process.
    /// The return value indicates if the optimal solution is found
    /// or the iteration limit is reached. In the latter case, an
    /// approximate solution is provided, which corresponds to a directed
    /// cycle whose mean cost is relatively small, but not necessarily
    /// minimal.
    ///
    /// \param limit  The maximum allowed number of iterations during
    /// the search process. Its default value implies that the algorithm
    /// runs until it finds the exact optimal solution.
    ///
    /// \return The termination cause of the search process.
    /// For more information, see \ref TerminationCause.
    TerminationCause findCycleMean(int limit =
                                   std::numeric_limits<int>::max()) {
      // Initialize and find strongly connected components
      init();
      findComponents();

      // Find the minimum cycle mean in the components
      int iter_count = 0;
      bool iter_limit_reached = false;
      for (int comp = 0; comp < _comp_num; ++comp) {
        // Find the minimum mean cycle in the current component
        if (!buildPolicyGraph(comp)) continue;
        while (true) {
          if (++iter_count > limit) {
            iter_limit_reached = true;
            break;
          }
          findPolicyCycle();
          if (!computeNodeDistances()) break;
        }

        // Update the best cycle (global minimum mean cycle)
        if ( _curr_found && (!_best_found ||
             _curr_cost * _best_size < _best_cost * _curr_size) ) {
          _best_found = true;
          _best_cost = _curr_cost;
          _best_size = _curr_size;
          _best_node = _curr_node;
        }

        if (iter_limit_reached) break;
      }

      if (iter_limit_reached) {
        return ITERATION_LIMIT;
      } else {
        return _best_found ? OPTIMAL : NO_CYCLE;
      }
    }

    /// \brief Find a minimum mean directed cycle.
    ///
    /// This function finds a directed cycle of minimum mean cost
    /// in the digraph using the data computed by findCycleMean().
    ///
    /// \return \c true if a directed cycle exists in the digraph.
    ///
    /// \pre \ref findCycleMean() must be called before using this function.
    bool findCycle() {
      if (!_best_found) return false;
      _cycle_path->addBack(_policy[_best_node]);
      for ( Node v = _best_node;
            (v = _gr.target(_policy[v])) != _best_node; ) {
        _cycle_path->addBack(_policy[v]);
      }
      return true;
    }

    /// @}

    /// \name Query Functions
    /// The results of the algorithm can be obtained using these
    /// functions.\n
    /// The algorithm should be executed before using them.

    /// @{

    /// \brief Return the total cost of the found cycle.
    ///
    /// This function returns the total cost of the found cycle.
    ///
    /// \pre \ref run() or \ref findCycleMean() must be called before
    /// using this function.
    Cost cycleCost() const {
      return static_cast<Cost>(_best_cost);
    }

    /// \brief Return the number of arcs on the found cycle.
    ///
    /// This function returns the number of arcs on the found cycle.
    ///
    /// \pre \ref run() or \ref findCycleMean() must be called before
    /// using this function.
    int cycleSize() const {
      return _best_size;
    }

    /// \brief Return the mean cost of the found cycle.
    ///
    /// This function returns the mean cost of the found cycle.
    ///
    /// \note <tt>alg.cycleMean()</tt> is just a shortcut of the
    /// following code.
    /// \code
    ///   return static_cast<double>(alg.cycleCost()) / alg.cycleSize();
    /// \endcode
    ///
    /// \pre \ref run() or \ref findCycleMean() must be called before
    /// using this function.
    double cycleMean() const {
      return static_cast<double>(_best_cost) / _best_size;
    }

    /// \brief Return the found cycle.
    ///
    /// This function returns a const reference to the path structure
    /// storing the found cycle.
    ///
    /// \pre \ref run() or \ref findCycle() must be called before using
    /// this function.
    const Path& cycle() const {
      return *_cycle_path;
    }

    ///@}

  private:

    // Initialize
    void init() {
      if (!_cycle_path) {
        _local_path = true;
        _cycle_path = new Path;
      }
      _queue.resize(countNodes(_gr));
      _best_found = false;
      _best_cost = 0;
      _best_size = 1;
      _cycle_path->clear();
    }

    // Find strongly connected components and initialize _comp_nodes
    // and _in_arcs
    void findComponents() {
      _comp_num = stronglyConnectedComponents(_gr, _comp);
      _comp_nodes.resize(_comp_num);
      if (_comp_num == 1) {
        _comp_nodes[0].clear();
        for (NodeIt n(_gr); n != INVALID; ++n) {
          _comp_nodes[0].push_back(n);
          _in_arcs[n].clear();
          for (InArcIt a(_gr, n); a != INVALID; ++a) {
            _in_arcs[n].push_back(a);
          }
        }
      } else {
        for (int i = 0; i < _comp_num; ++i)
          _comp_nodes[i].clear();
        for (NodeIt n(_gr); n != INVALID; ++n) {
          int k = _comp[n];
          _comp_nodes[k].push_back(n);
          _in_arcs[n].clear();
          for (InArcIt a(_gr, n); a != INVALID; ++a) {
            if (_comp[_gr.source(a)] == k) _in_arcs[n].push_back(a);
          }
        }
      }
    }

    // Build the policy graph in the given strongly connected component
    // (the out-degree of every node is 1)
    bool buildPolicyGraph(int comp) {
      _nodes = &(_comp_nodes[comp]);
      if (_nodes->size() < 1 ||
          (_nodes->size() == 1 && _in_arcs[(*_nodes)[0]].size() == 0)) {
        return false;
      }
      for (int i = 0; i < int(_nodes->size()); ++i) {
        _dist[(*_nodes)[i]] = INF;
      }
      Node u, v;
      Arc e;
      for (int i = 0; i < int(_nodes->size()); ++i) {
        v = (*_nodes)[i];
        for (int j = 0; j < int(_in_arcs[v].size()); ++j) {
          e = _in_arcs[v][j];
          u = _gr.source(e);
          if (_cost[e] < _dist[u]) {
            _dist[u] = _cost[e];
            _policy[u] = e;
          }
        }
      }
      return true;
    }

    // Find the minimum mean cycle in the policy graph
    void findPolicyCycle() {
      for (int i = 0; i < int(_nodes->size()); ++i) {
        _level[(*_nodes)[i]] = -1;
      }
      LargeCost ccost;
      int csize;
      Node u, v;
      _curr_found = false;
      for (int i = 0; i < int(_nodes->size()); ++i) {
        u = (*_nodes)[i];
        if (_level[u] >= 0) continue;
        for (; _level[u] < 0; u = _gr.target(_policy[u])) {
          _level[u] = i;
        }
        if (_level[u] == i) {
          // A cycle is found
          ccost = _cost[_policy[u]];
          csize = 1;
          for (v = u; (v = _gr.target(_policy[v])) != u; ) {
            ccost += _cost[_policy[v]];
            ++csize;
          }
          if ( !_curr_found ||
               (ccost * _curr_size < _curr_cost * csize) ) {
            _curr_found = true;
            _curr_cost = ccost;
            _curr_size = csize;
            _curr_node = u;
          }
        }
      }
    }

    // Contract the policy graph and compute node distances
    bool computeNodeDistances() {
      // Find the component of the main cycle and compute node distances
      // using reverse BFS
      for (int i = 0; i < int(_nodes->size()); ++i) {
        _reached[(*_nodes)[i]] = false;
      }
      _qfront = _qback = 0;
      _queue[0] = _curr_node;
      _reached[_curr_node] = true;
      _dist[_curr_node] = 0;
      Node u, v;
      Arc e;
      while (_qfront <= _qback) {
        v = _queue[_qfront++];
        for (int j = 0; j < int(_in_arcs[v].size()); ++j) {
          e = _in_arcs[v][j];
          u = _gr.source(e);
          if (_policy[u] == e && !_reached[u]) {
            _reached[u] = true;
            _dist[u] = _dist[v] + _cost[e] * _curr_size - _curr_cost;
            _queue[++_qback] = u;
          }
        }
      }

      // Connect all other nodes to this component and compute node
      // distances using reverse BFS
      _qfront = 0;
      while (_qback < int(_nodes->size())-1) {
        v = _queue[_qfront++];
        for (int j = 0; j < int(_in_arcs[v].size()); ++j) {
          e = _in_arcs[v][j];
          u = _gr.source(e);
          if (!_reached[u]) {
            _reached[u] = true;
            _policy[u] = e;
            _dist[u] = _dist[v] + _cost[e] * _curr_size - _curr_cost;
            _queue[++_qback] = u;
          }
        }
      }

      // Improve node distances
      bool improved = false;
      for (int i = 0; i < int(_nodes->size()); ++i) {
        v = (*_nodes)[i];
        for (int j = 0; j < int(_in_arcs[v].size()); ++j) {
          e = _in_arcs[v][j];
          u = _gr.source(e);
          LargeCost delta = _dist[v] + _cost[e] * _curr_size - _curr_cost;
          if (_tolerance.less(delta, _dist[u])) {
            _dist[u] = delta;
            _policy[u] = e;
            improved = true;
          }
        }
      }
      return improved;
    }

  }; //class HowardMmc

  ///@}

} //namespace lemon

#endif //LEMON_HOWARD_MMC_H
