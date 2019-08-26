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

#ifndef LEMON_HARTMANN_ORLIN_MMC_H
#define LEMON_HARTMANN_ORLIN_MMC_H

/// \ingroup min_mean_cycle
///
/// \file
/// \brief Hartmann-Orlin's algorithm for finding a minimum mean cycle.

#include <vector>
#include <limits>
#include <lemon/core.h>
#include <lemon/path.h>
#include <lemon/tolerance.h>
#include <lemon/connectivity.h>

namespace lemon {

  /// \brief Default traits class of HartmannOrlinMmc class.
  ///
  /// Default traits class of HartmannOrlinMmc class.
  /// \tparam GR The type of the digraph.
  /// \tparam CM The type of the cost map.
  /// It must conform to the \ref concepts::ReadMap "ReadMap" concept.
#ifdef DOXYGEN
  template <typename GR, typename CM>
#else
  template <typename GR, typename CM,
    bool integer = std::numeric_limits<typename CM::Value>::is_integer>
#endif
  struct HartmannOrlinMmcDefaultTraits
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
    /// and it must have an \c addFront() function.
    typedef lemon::Path<Digraph> Path;
  };

  // Default traits class for integer cost types
  template <typename GR, typename CM>
  struct HartmannOrlinMmcDefaultTraits<GR, CM, true>
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

  /// \brief Implementation of the Hartmann-Orlin algorithm for finding
  /// a minimum mean cycle.
  ///
  /// This class implements the Hartmann-Orlin algorithm for finding
  /// a directed cycle of minimum mean cost in a digraph
  /// \cite hartmann93finding, \cite dasdan98minmeancycle.
  /// This method is based on \ref KarpMmc "Karp"'s original algorithm, but
  /// applies an early termination scheme. It makes the algorithm
  /// significantly faster for some problem instances, but slower for others.
  /// The algorithm runs in time O(nm) and uses space O(n<sup>2</sup>+m).
  ///
  /// \tparam GR The type of the digraph the algorithm runs on.
  /// \tparam CM The type of the cost map. The default
  /// map type is \ref concepts::Digraph::ArcMap "GR::ArcMap<int>".
  /// \tparam TR The traits class that defines various types used by the
  /// algorithm. By default, it is \ref HartmannOrlinMmcDefaultTraits
  /// "HartmannOrlinMmcDefaultTraits<GR, CM>".
  /// In most cases, this parameter should not be set directly,
  /// consider to use the named template parameters instead.
#ifdef DOXYGEN
  template <typename GR, typename CM, typename TR>
#else
  template < typename GR,
             typename CM = typename GR::template ArcMap<int>,
             typename TR = HartmannOrlinMmcDefaultTraits<GR, CM> >
#endif
  class HartmannOrlinMmc
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
    /// Using the \ref lemon::HartmannOrlinMmcDefaultTraits
    /// "default traits class",
    /// it is \ref lemon::Path "Path<Digraph>".
    typedef typename TR::Path Path;

    /// \brief The
    /// \ref lemon::HartmannOrlinMmcDefaultTraits "traits class"
    /// of the algorithm
    typedef TR Traits;

  private:

    TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

    // Data sturcture for path data
    struct PathData
    {
      LargeCost dist;
      Arc pred;
      PathData(LargeCost d, Arc p = INVALID) :
        dist(d), pred(p) {}
    };

    typedef typename Digraph::template NodeMap<std::vector<PathData> >
      PathDataNodeMap;

  private:

    // The digraph the algorithm runs on
    const Digraph &_gr;
    // The cost of the arcs
    const CostMap &_cost;

    // Data for storing the strongly connected components
    int _comp_num;
    typename Digraph::template NodeMap<int> _comp;
    std::vector<std::vector<Node> > _comp_nodes;
    std::vector<Node>* _nodes;
    typename Digraph::template NodeMap<std::vector<Arc> > _out_arcs;

    // Data for the found cycles
    bool _curr_found, _best_found;
    LargeCost _curr_cost, _best_cost;
    int _curr_size, _best_size;
    Node _curr_node, _best_node;
    int _curr_level, _best_level;

    Path *_cycle_path;
    bool _local_path;

    // Node map for storing path data
    PathDataNodeMap _data;
    // The processed nodes in the last round
    std::vector<Node> _process;

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
      : public HartmannOrlinMmc<GR, CM, SetLargeCostTraits<T> > {
      typedef HartmannOrlinMmc<GR, CM, SetLargeCostTraits<T> > Create;
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
    /// and it must have an \c addFront() function.
    template <typename T>
    struct SetPath
      : public HartmannOrlinMmc<GR, CM, SetPathTraits<T> > {
      typedef HartmannOrlinMmc<GR, CM, SetPathTraits<T> > Create;
    };

    /// @}

  protected:

    HartmannOrlinMmc() {}

  public:

    /// \brief Constructor.
    ///
    /// The constructor of the class.
    ///
    /// \param digraph The digraph the algorithm runs on.
    /// \param cost The costs of the arcs.
    HartmannOrlinMmc( const Digraph &digraph,
                      const CostMap &cost ) :
      _gr(digraph), _cost(cost), _comp(digraph), _out_arcs(digraph),
      _best_found(false), _best_cost(0), _best_size(1),
      _cycle_path(NULL), _local_path(false), _data(digraph),
      INF(std::numeric_limits<LargeCost>::has_infinity ?
          std::numeric_limits<LargeCost>::infinity() :
          std::numeric_limits<LargeCost>::max())
    {}

    /// Destructor.
    ~HartmannOrlinMmc() {
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
    /// \note The algorithm calls only the \ref lemon::Path::addFront()
    /// "addFront()" function of the given path structure.
    ///
    /// \return <tt>(*this)</tt>
    HartmannOrlinMmc& cycle(Path &path) {
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
    HartmannOrlinMmc& tolerance(const Tolerance& tolerance) {
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

    /// \brief Find the minimum cycle mean.
    ///
    /// This function finds the minimum mean cost of the directed
    /// cycles in the digraph.
    ///
    /// \return \c true if a directed cycle exists in the digraph.
    bool findCycleMean() {
      // Initialization and find strongly connected components
      init();
      findComponents();

      // Find the minimum cycle mean in the components
      for (int comp = 0; comp < _comp_num; ++comp) {
        if (!initComponent(comp)) continue;
        processRounds();

        // Update the best cycle (global minimum mean cycle)
        if ( _curr_found && (!_best_found ||
             _curr_cost * _best_size < _best_cost * _curr_size) ) {
          _best_found = true;
          _best_cost = _curr_cost;
          _best_size = _curr_size;
          _best_node = _curr_node;
          _best_level = _curr_level;
        }
      }
      return _best_found;
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
      IntNodeMap reached(_gr, -1);
      int r = _best_level + 1;
      Node u = _best_node;
      while (reached[u] < 0) {
        reached[u] = --r;
        u = _gr.source(_data[u][r].pred);
      }
      r = reached[u];
      Arc e = _data[u][r].pred;
      _cycle_path->addFront(e);
      _best_cost = _cost[e];
      _best_size = 1;
      Node v;
      while ((v = _gr.source(e)) != u) {
        e = _data[v][--r].pred;
        _cycle_path->addFront(e);
        _best_cost += _cost[e];
        ++_best_size;
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

    // Initialization
    void init() {
      if (!_cycle_path) {
        _local_path = true;
        _cycle_path = new Path;
      }
      _cycle_path->clear();
      _best_found = false;
      _best_cost = 0;
      _best_size = 1;
      _cycle_path->clear();
      for (NodeIt u(_gr); u != INVALID; ++u)
        _data[u].clear();
    }

    // Find strongly connected components and initialize _comp_nodes
    // and _out_arcs
    void findComponents() {
      _comp_num = stronglyConnectedComponents(_gr, _comp);
      _comp_nodes.resize(_comp_num);
      if (_comp_num == 1) {
        _comp_nodes[0].clear();
        for (NodeIt n(_gr); n != INVALID; ++n) {
          _comp_nodes[0].push_back(n);
          _out_arcs[n].clear();
          for (OutArcIt a(_gr, n); a != INVALID; ++a) {
            _out_arcs[n].push_back(a);
          }
        }
      } else {
        for (int i = 0; i < _comp_num; ++i)
          _comp_nodes[i].clear();
        for (NodeIt n(_gr); n != INVALID; ++n) {
          int k = _comp[n];
          _comp_nodes[k].push_back(n);
          _out_arcs[n].clear();
          for (OutArcIt a(_gr, n); a != INVALID; ++a) {
            if (_comp[_gr.target(a)] == k) _out_arcs[n].push_back(a);
          }
        }
      }
    }

    // Initialize path data for the current component
    bool initComponent(int comp) {
      _nodes = &(_comp_nodes[comp]);
      int n = _nodes->size();
      if (n < 1 || (n == 1 && _out_arcs[(*_nodes)[0]].size() == 0)) {
        return false;
      }
      for (int i = 0; i < n; ++i) {
        _data[(*_nodes)[i]].resize(n + 1, PathData(INF));
      }
      return true;
    }

    // Process all rounds of computing path data for the current component.
    // _data[v][k] is the cost of a shortest directed walk from the root
    // node to node v containing exactly k arcs.
    void processRounds() {
      Node start = (*_nodes)[0];
      _data[start][0] = PathData(0);
      _process.clear();
      _process.push_back(start);

      int k, n = _nodes->size();
      int next_check = 4;
      bool terminate = false;
      for (k = 1; k <= n && int(_process.size()) < n && !terminate; ++k) {
        processNextBuildRound(k);
        if (k == next_check || k == n) {
          terminate = checkTermination(k);
          next_check = next_check * 3 / 2;
        }
      }
      for ( ; k <= n && !terminate; ++k) {
        processNextFullRound(k);
        if (k == next_check || k == n) {
          terminate = checkTermination(k);
          next_check = next_check * 3 / 2;
        }
      }
    }

    // Process one round and rebuild _process
    void processNextBuildRound(int k) {
      std::vector<Node> next;
      Node u, v;
      Arc e;
      LargeCost d;
      for (int i = 0; i < int(_process.size()); ++i) {
        u = _process[i];
        for (int j = 0; j < int(_out_arcs[u].size()); ++j) {
          e = _out_arcs[u][j];
          v = _gr.target(e);
          d = _data[u][k-1].dist + _cost[e];
          if (_tolerance.less(d, _data[v][k].dist)) {
            if (_data[v][k].dist == INF) next.push_back(v);
            _data[v][k] = PathData(d, e);
          }
        }
      }
      _process.swap(next);
    }

    // Process one round using _nodes instead of _process
    void processNextFullRound(int k) {
      Node u, v;
      Arc e;
      LargeCost d;
      for (int i = 0; i < int(_nodes->size()); ++i) {
        u = (*_nodes)[i];
        for (int j = 0; j < int(_out_arcs[u].size()); ++j) {
          e = _out_arcs[u][j];
          v = _gr.target(e);
          d = _data[u][k-1].dist + _cost[e];
          if (_tolerance.less(d, _data[v][k].dist)) {
            _data[v][k] = PathData(d, e);
          }
        }
      }
    }

    // Check early termination
    bool checkTermination(int k) {
      typedef std::pair<int, int> Pair;
      typename GR::template NodeMap<Pair> level(_gr, Pair(-1, 0));
      typename GR::template NodeMap<LargeCost> pi(_gr);
      int n = _nodes->size();
      LargeCost cost;
      int size;
      Node u;

      // Search for cycles that are already found
      _curr_found = false;
      for (int i = 0; i < n; ++i) {
        u = (*_nodes)[i];
        if (_data[u][k].dist == INF) continue;
        for (int j = k; j >= 0; --j) {
          if (level[u].first == i && level[u].second > 0) {
            // A cycle is found
            cost = _data[u][level[u].second].dist - _data[u][j].dist;
            size = level[u].second - j;
            if (!_curr_found || cost * _curr_size < _curr_cost * size) {
              _curr_cost = cost;
              _curr_size = size;
              _curr_node = u;
              _curr_level = level[u].second;
              _curr_found = true;
            }
          }
          level[u] = Pair(i, j);
          if (j != 0) {
            u = _gr.source(_data[u][j].pred);
          }
        }
      }

      // If at least one cycle is found, check the optimality condition
      LargeCost d;
      if (_curr_found && k < n) {
        // Find node potentials
        for (int i = 0; i < n; ++i) {
          u = (*_nodes)[i];
          pi[u] = INF;
          for (int j = 0; j <= k; ++j) {
            if (_data[u][j].dist < INF) {
              d = _data[u][j].dist * _curr_size - j * _curr_cost;
              if (_tolerance.less(d, pi[u])) pi[u] = d;
            }
          }
        }

        // Check the optimality condition for all arcs
        bool done = true;
        for (ArcIt a(_gr); a != INVALID; ++a) {
          if (_tolerance.less(_cost[a] * _curr_size - _curr_cost,
                              pi[_gr.target(a)] - pi[_gr.source(a)]) ) {
            done = false;
            break;
          }
        }
        return done;
      }
      return (k == n);
    }

  }; //class HartmannOrlinMmc

  ///@}

} //namespace lemon

#endif //LEMON_HARTMANN_ORLIN_MMC_H
