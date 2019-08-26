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

#ifndef LEMON_SUURBALLE_H
#define LEMON_SUURBALLE_H

///\ingroup shortest_path
///\file
///\brief An algorithm for finding arc-disjoint paths between two
/// nodes having minimum total length.

#include <vector>
#include <limits>
#include <lemon/bin_heap.h>
#include <lemon/path.h>
#include <lemon/list_graph.h>
#include <lemon/dijkstra.h>
#include <lemon/maps.h>

namespace lemon {

  /// \brief Default traits class of Suurballe algorithm.
  ///
  /// Default traits class of Suurballe algorithm.
  /// \tparam GR The digraph type the algorithm runs on.
  /// \tparam LEN The type of the length map.
  /// The default value is <tt>GR::ArcMap<int></tt>.
#ifdef DOXYGEN
  template <typename GR, typename LEN>
#else
  template < typename GR,
             typename LEN = typename GR::template ArcMap<int> >
#endif
  struct SuurballeDefaultTraits
  {
    /// The type of the digraph.
    typedef GR Digraph;
    /// The type of the length map.
    typedef LEN LengthMap;
    /// The type of the lengths.
    typedef typename LEN::Value Length;
    /// The type of the flow map.
    typedef typename GR::template ArcMap<int> FlowMap;
    /// The type of the potential map.
    typedef typename GR::template NodeMap<Length> PotentialMap;

    /// \brief The path type
    ///
    /// The type used for storing the found arc-disjoint paths.
    /// It must conform to the \ref lemon::concepts::Path "Path" concept
    /// and it must have an \c addBack() function.
    typedef lemon::Path<Digraph> Path;

    /// The cross reference type used for the heap.
    typedef typename GR::template NodeMap<int> HeapCrossRef;

    /// \brief The heap type used for internal Dijkstra computations.
    ///
    /// The type of the heap used for internal Dijkstra computations.
    /// It must conform to the \ref lemon::concepts::Heap "Heap" concept
    /// and its priority type must be \c Length.
    typedef BinHeap<Length, HeapCrossRef> Heap;
  };

  /// \addtogroup shortest_path
  /// @{

  /// \brief Algorithm for finding arc-disjoint paths between two nodes
  /// having minimum total length.
  ///
  /// \ref lemon::Suurballe "Suurballe" implements an algorithm for
  /// finding arc-disjoint paths having minimum total length (cost)
  /// from a given source node to a given target node in a digraph.
  ///
  /// Note that this problem is a special case of the \ref min_cost_flow
  /// "minimum cost flow problem". This implementation is actually an
  /// efficient specialized version of the \ref CapacityScaling
  /// "successive shortest path" algorithm directly for this problem.
  /// Therefore this class provides query functions for flow values and
  /// node potentials (the dual solution) just like the minimum cost flow
  /// algorithms.
  ///
  /// \tparam GR The digraph type the algorithm runs on.
  /// \tparam LEN The type of the length map.
  /// The default value is <tt>GR::ArcMap<int></tt>.
  ///
  /// \warning Length values should be \e non-negative.
  ///
  /// \note For finding \e node-disjoint paths, this algorithm can be used
  /// along with the \ref SplitNodes adaptor.
#ifdef DOXYGEN
  template <typename GR, typename LEN, typename TR>
#else
  template < typename GR,
             typename LEN = typename GR::template ArcMap<int>,
             typename TR = SuurballeDefaultTraits<GR, LEN> >
#endif
  class Suurballe
  {
    TEMPLATE_DIGRAPH_TYPEDEFS(GR);

    typedef ConstMap<Arc, int> ConstArcMap;
    typedef typename GR::template NodeMap<Arc> PredMap;

  public:

    /// The type of the digraph.
    typedef typename TR::Digraph Digraph;
    /// The type of the length map.
    typedef typename TR::LengthMap LengthMap;
    /// The type of the lengths.
    typedef typename TR::Length Length;

    /// The type of the flow map.
    typedef typename TR::FlowMap FlowMap;
    /// The type of the potential map.
    typedef typename TR::PotentialMap PotentialMap;
    /// The type of the path structures.
    typedef typename TR::Path Path;
    /// The cross reference type used for the heap.
    typedef typename TR::HeapCrossRef HeapCrossRef;
    /// The heap type used for internal Dijkstra computations.
    typedef typename TR::Heap Heap;

    /// The \ref lemon::SuurballeDefaultTraits "traits class" of the algorithm.
    typedef TR Traits;

  private:

    // ResidualDijkstra is a special implementation of the
    // Dijkstra algorithm for finding shortest paths in the
    // residual network with respect to the reduced arc lengths
    // and modifying the node potentials according to the
    // distance of the nodes.
    class ResidualDijkstra
    {
    private:

      const Digraph &_graph;
      const LengthMap &_length;
      const FlowMap &_flow;
      PotentialMap &_pi;
      PredMap &_pred;
      Node _s;
      Node _t;

      PotentialMap _dist;
      std::vector<Node> _proc_nodes;

    public:

      // Constructor
      ResidualDijkstra(Suurballe &srb) :
        _graph(srb._graph), _length(srb._length),
        _flow(*srb._flow), _pi(*srb._potential), _pred(srb._pred),
        _s(srb._s), _t(srb._t), _dist(_graph) {}

      // Run the algorithm and return true if a path is found
      // from the source node to the target node.
      bool run(int cnt) {
        return cnt == 0 ? startFirst() : start();
      }

    private:

      // Execute the algorithm for the first time (the flow and potential
      // functions have to be identically zero).
      bool startFirst() {
        HeapCrossRef heap_cross_ref(_graph, Heap::PRE_HEAP);
        Heap heap(heap_cross_ref);
        heap.push(_s, 0);
        _pred[_s] = INVALID;
        _proc_nodes.clear();

        // Process nodes
        while (!heap.empty() && heap.top() != _t) {
          Node u = heap.top(), v;
          Length d = heap.prio(), dn;
          _dist[u] = heap.prio();
          _proc_nodes.push_back(u);
          heap.pop();

          // Traverse outgoing arcs
          for (OutArcIt e(_graph, u); e != INVALID; ++e) {
            v = _graph.target(e);
            switch(heap.state(v)) {
              case Heap::PRE_HEAP:
                heap.push(v, d + _length[e]);
                _pred[v] = e;
                break;
              case Heap::IN_HEAP:
                dn = d + _length[e];
                if (dn < heap[v]) {
                  heap.decrease(v, dn);
                  _pred[v] = e;
                }
                break;
              case Heap::POST_HEAP:
                break;
            }
          }
        }
        if (heap.empty()) return false;

        // Update potentials of processed nodes
        Length t_dist = heap.prio();
        for (int i = 0; i < int(_proc_nodes.size()); ++i)
          _pi[_proc_nodes[i]] = _dist[_proc_nodes[i]] - t_dist;
        return true;
      }

      // Execute the algorithm.
      bool start() {
        HeapCrossRef heap_cross_ref(_graph, Heap::PRE_HEAP);
        Heap heap(heap_cross_ref);
        heap.push(_s, 0);
        _pred[_s] = INVALID;
        _proc_nodes.clear();

        // Process nodes
        while (!heap.empty() && heap.top() != _t) {
          Node u = heap.top(), v;
          Length d = heap.prio() + _pi[u], dn;
          _dist[u] = heap.prio();
          _proc_nodes.push_back(u);
          heap.pop();

          // Traverse outgoing arcs
          for (OutArcIt e(_graph, u); e != INVALID; ++e) {
            if (_flow[e] == 0) {
              v = _graph.target(e);
              switch(heap.state(v)) {
                case Heap::PRE_HEAP:
                  heap.push(v, d + _length[e] - _pi[v]);
                  _pred[v] = e;
                  break;
                case Heap::IN_HEAP:
                  dn = d + _length[e] - _pi[v];
                  if (dn < heap[v]) {
                    heap.decrease(v, dn);
                    _pred[v] = e;
                  }
                  break;
                case Heap::POST_HEAP:
                  break;
              }
            }
          }

          // Traverse incoming arcs
          for (InArcIt e(_graph, u); e != INVALID; ++e) {
            if (_flow[e] == 1) {
              v = _graph.source(e);
              switch(heap.state(v)) {
                case Heap::PRE_HEAP:
                  heap.push(v, d - _length[e] - _pi[v]);
                  _pred[v] = e;
                  break;
                case Heap::IN_HEAP:
                  dn = d - _length[e] - _pi[v];
                  if (dn < heap[v]) {
                    heap.decrease(v, dn);
                    _pred[v] = e;
                  }
                  break;
                case Heap::POST_HEAP:
                  break;
              }
            }
          }
        }
        if (heap.empty()) return false;

        // Update potentials of processed nodes
        Length t_dist = heap.prio();
        for (int i = 0; i < int(_proc_nodes.size()); ++i)
          _pi[_proc_nodes[i]] += _dist[_proc_nodes[i]] - t_dist;
        return true;
      }

    }; //class ResidualDijkstra

  public:

    /// \name Named Template Parameters
    /// @{

    template <typename T>
    struct SetFlowMapTraits : public Traits {
      typedef T FlowMap;
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// \c FlowMap type.
    ///
    /// \ref named-templ-param "Named parameter" for setting
    /// \c FlowMap type.
    template <typename T>
    struct SetFlowMap
      : public Suurballe<GR, LEN, SetFlowMapTraits<T> > {
      typedef Suurballe<GR, LEN, SetFlowMapTraits<T> > Create;
    };

    template <typename T>
    struct SetPotentialMapTraits : public Traits {
      typedef T PotentialMap;
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// \c PotentialMap type.
    ///
    /// \ref named-templ-param "Named parameter" for setting
    /// \c PotentialMap type.
    template <typename T>
    struct SetPotentialMap
      : public Suurballe<GR, LEN, SetPotentialMapTraits<T> > {
      typedef Suurballe<GR, LEN, SetPotentialMapTraits<T> > Create;
    };

    template <typename T>
    struct SetPathTraits : public Traits {
      typedef T Path;
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// \c %Path type.
    ///
    /// \ref named-templ-param "Named parameter" for setting \c %Path type.
    /// It must conform to the \ref lemon::concepts::Path "Path" concept
    /// and it must have an \c addBack() function.
    template <typename T>
    struct SetPath
      : public Suurballe<GR, LEN, SetPathTraits<T> > {
      typedef Suurballe<GR, LEN, SetPathTraits<T> > Create;
    };

    template <typename H, typename CR>
    struct SetHeapTraits : public Traits {
      typedef H Heap;
      typedef CR HeapCrossRef;
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// \c Heap and \c HeapCrossRef types.
    ///
    /// \ref named-templ-param "Named parameter" for setting \c Heap
    /// and \c HeapCrossRef types with automatic allocation.
    /// They will be used for internal Dijkstra computations.
    /// The heap type must conform to the \ref lemon::concepts::Heap "Heap"
    /// concept and its priority type must be \c Length.
    template <typename H,
              typename CR = typename Digraph::template NodeMap<int> >
    struct SetHeap
      : public Suurballe<GR, LEN, SetHeapTraits<H, CR> > {
      typedef Suurballe<GR, LEN, SetHeapTraits<H, CR> > Create;
    };

    /// @}

  private:

    // The digraph the algorithm runs on
    const Digraph &_graph;
    // The length map
    const LengthMap &_length;

    // Arc map of the current flow
    FlowMap *_flow;
    bool _local_flow;
    // Node map of the current potentials
    PotentialMap *_potential;
    bool _local_potential;

    // The source node
    Node _s;
    // The target node
    Node _t;

    // Container to store the found paths
    std::vector<Path> _paths;
    int _path_num;

    // The pred arc map
    PredMap _pred;

    // Data for full init
    PotentialMap *_init_dist;
    PredMap *_init_pred;
    bool _full_init;

  protected:

    Suurballe() {}

  public:

    /// \brief Constructor.
    ///
    /// Constructor.
    ///
    /// \param graph The digraph the algorithm runs on.
    /// \param length The length (cost) values of the arcs.
    Suurballe( const Digraph &graph,
               const LengthMap &length ) :
      _graph(graph), _length(length), _flow(0), _local_flow(false),
      _potential(0), _local_potential(false), _pred(graph),
      _init_dist(0), _init_pred(0)
    {}

    /// Destructor.
    ~Suurballe() {
      if (_local_flow) delete _flow;
      if (_local_potential) delete _potential;
      delete _init_dist;
      delete _init_pred;
    }

    /// \brief Set the flow map.
    ///
    /// This function sets the flow map.
    /// If it is not used before calling \ref run() or \ref init(),
    /// an instance will be allocated automatically. The destructor
    /// deallocates this automatically allocated map, of course.
    ///
    /// The found flow contains only 0 and 1 values, since it is the
    /// union of the found arc-disjoint paths.
    ///
    /// \return <tt>(*this)</tt>
    Suurballe& flowMap(FlowMap &map) {
      if (_local_flow) {
        delete _flow;
        _local_flow = false;
      }
      _flow = &map;
      return *this;
    }

    /// \brief Set the potential map.
    ///
    /// This function sets the potential map.
    /// If it is not used before calling \ref run() or \ref init(),
    /// an instance will be allocated automatically. The destructor
    /// deallocates this automatically allocated map, of course.
    ///
    /// The node potentials provide the dual solution of the underlying
    /// \ref min_cost_flow "minimum cost flow problem".
    ///
    /// \return <tt>(*this)</tt>
    Suurballe& potentialMap(PotentialMap &map) {
      if (_local_potential) {
        delete _potential;
        _local_potential = false;
      }
      _potential = &map;
      return *this;
    }

    /// \name Execution Control
    /// The simplest way to execute the algorithm is to call the run()
    /// function.\n
    /// If you need to execute the algorithm many times using the same
    /// source node, then you may call fullInit() once and start()
    /// for each target node.\n
    /// If you only need the flow that is the union of the found
    /// arc-disjoint paths, then you may call findFlow() instead of
    /// start().

    /// @{

    /// \brief Run the algorithm.
    ///
    /// This function runs the algorithm.
    ///
    /// \param s The source node.
    /// \param t The target node.
    /// \param k The number of paths to be found.
    ///
    /// \return \c k if there are at least \c k arc-disjoint paths from
    /// \c s to \c t in the digraph. Otherwise it returns the number of
    /// arc-disjoint paths found.
    ///
    /// \note Apart from the return value, <tt>s.run(s, t, k)</tt> is
    /// just a shortcut of the following code.
    /// \code
    ///   s.init(s);
    ///   s.start(t, k);
    /// \endcode
    int run(const Node& s, const Node& t, int k = 2) {
      init(s);
      start(t, k);
      return _path_num;
    }

    /// \brief Initialize the algorithm.
    ///
    /// This function initializes the algorithm with the given source node.
    ///
    /// \param s The source node.
    void init(const Node& s) {
      _s = s;

      // Initialize maps
      if (!_flow) {
        _flow = new FlowMap(_graph);
        _local_flow = true;
      }
      if (!_potential) {
        _potential = new PotentialMap(_graph);
        _local_potential = true;
      }
      _full_init = false;
    }

    /// \brief Initialize the algorithm and perform Dijkstra.
    ///
    /// This function initializes the algorithm and performs a full
    /// Dijkstra search from the given source node. It makes consecutive
    /// executions of \ref start() "start(t, k)" faster, since they
    /// have to perform %Dijkstra only k-1 times.
    ///
    /// This initialization is usually worth using instead of \ref init()
    /// if the algorithm is executed many times using the same source node.
    ///
    /// \param s The source node.
    void fullInit(const Node& s) {
      // Initialize maps
      init(s);
      if (!_init_dist) {
        _init_dist = new PotentialMap(_graph);
      }
      if (!_init_pred) {
        _init_pred = new PredMap(_graph);
      }

      // Run a full Dijkstra
      typename Dijkstra<Digraph, LengthMap>
        ::template SetStandardHeap<Heap>
        ::template SetDistMap<PotentialMap>
        ::template SetPredMap<PredMap>
        ::Create dijk(_graph, _length);
      dijk.distMap(*_init_dist).predMap(*_init_pred);
      dijk.run(s);

      _full_init = true;
    }

    /// \brief Execute the algorithm.
    ///
    /// This function executes the algorithm.
    ///
    /// \param t The target node.
    /// \param k The number of paths to be found.
    ///
    /// \return \c k if there are at least \c k arc-disjoint paths from
    /// \c s to \c t in the digraph. Otherwise it returns the number of
    /// arc-disjoint paths found.
    ///
    /// \note Apart from the return value, <tt>s.start(t, k)</tt> is
    /// just a shortcut of the following code.
    /// \code
    ///   s.findFlow(t, k);
    ///   s.findPaths();
    /// \endcode
    int start(const Node& t, int k = 2) {
      findFlow(t, k);
      findPaths();
      return _path_num;
    }

    /// \brief Execute the algorithm to find an optimal flow.
    ///
    /// This function executes the successive shortest path algorithm to
    /// find a minimum cost flow, which is the union of \c k (or less)
    /// arc-disjoint paths.
    ///
    /// \param t The target node.
    /// \param k The number of paths to be found.
    ///
    /// \return \c k if there are at least \c k arc-disjoint paths from
    /// the source node to the given node \c t in the digraph.
    /// Otherwise it returns the number of arc-disjoint paths found.
    ///
    /// \pre \ref init() must be called before using this function.
    int findFlow(const Node& t, int k = 2) {
      _t = t;
      ResidualDijkstra dijkstra(*this);

      // Initialization
      for (ArcIt e(_graph); e != INVALID; ++e) {
        (*_flow)[e] = 0;
      }
      if (_full_init) {
        for (NodeIt n(_graph); n != INVALID; ++n) {
          (*_potential)[n] = (*_init_dist)[n];
        }
        Node u = _t;
        Arc e;
        while ((e = (*_init_pred)[u]) != INVALID) {
          (*_flow)[e] = 1;
          u = _graph.source(e);
        }
        _path_num = 1;
      } else {
        for (NodeIt n(_graph); n != INVALID; ++n) {
          (*_potential)[n] = 0;
        }
        _path_num = 0;
      }

      // Find shortest paths
      while (_path_num < k) {
        // Run Dijkstra
        if (!dijkstra.run(_path_num)) break;
        ++_path_num;

        // Set the flow along the found shortest path
        Node u = _t;
        Arc e;
        while ((e = _pred[u]) != INVALID) {
          if (u == _graph.target(e)) {
            (*_flow)[e] = 1;
            u = _graph.source(e);
          } else {
            (*_flow)[e] = 0;
            u = _graph.target(e);
          }
        }
      }
      return _path_num;
    }

    /// \brief Compute the paths from the flow.
    ///
    /// This function computes arc-disjoint paths from the found minimum
    /// cost flow, which is the union of them.
    ///
    /// \pre \ref init() and \ref findFlow() must be called before using
    /// this function.
    void findPaths() {
      FlowMap res_flow(_graph);
      for(ArcIt a(_graph); a != INVALID; ++a) res_flow[a] = (*_flow)[a];

      _paths.clear();
      _paths.resize(_path_num);
      for (int i = 0; i < _path_num; ++i) {
        Node n = _s;
        while (n != _t) {
          OutArcIt e(_graph, n);
          for ( ; res_flow[e] == 0; ++e) ;
          n = _graph.target(e);
          _paths[i].addBack(e);
          res_flow[e] = 0;
        }
      }
    }

    /// @}

    /// \name Query Functions
    /// The results of the algorithm can be obtained using these
    /// functions.
    /// \n The algorithm should be executed before using them.

    /// @{

    /// \brief Return the total length of the found paths.
    ///
    /// This function returns the total length of the found paths, i.e.
    /// the total cost of the found flow.
    /// The complexity of the function is O(m).
    ///
    /// \pre \ref run() or \ref findFlow() must be called before using
    /// this function.
    Length totalLength() const {
      Length c = 0;
      for (ArcIt e(_graph); e != INVALID; ++e)
        c += (*_flow)[e] * _length[e];
      return c;
    }

    /// \brief Return the flow value on the given arc.
    ///
    /// This function returns the flow value on the given arc.
    /// It is \c 1 if the arc is involved in one of the found arc-disjoint
    /// paths, otherwise it is \c 0.
    ///
    /// \pre \ref run() or \ref findFlow() must be called before using
    /// this function.
    int flow(const Arc& arc) const {
      return (*_flow)[arc];
    }

    /// \brief Return a const reference to an arc map storing the
    /// found flow.
    ///
    /// This function returns a const reference to an arc map storing
    /// the flow that is the union of the found arc-disjoint paths.
    ///
    /// \pre \ref run() or \ref findFlow() must be called before using
    /// this function.
    const FlowMap& flowMap() const {
      return *_flow;
    }

    /// \brief Return the potential of the given node.
    ///
    /// This function returns the potential of the given node.
    /// The node potentials provide the dual solution of the
    /// underlying \ref min_cost_flow "minimum cost flow problem".
    ///
    /// \pre \ref run() or \ref findFlow() must be called before using
    /// this function.
    Length potential(const Node& node) const {
      return (*_potential)[node];
    }

    /// \brief Return a const reference to a node map storing the
    /// found potentials (the dual solution).
    ///
    /// This function returns a const reference to a node map storing
    /// the found potentials that provide the dual solution of the
    /// underlying \ref min_cost_flow "minimum cost flow problem".
    ///
    /// \pre \ref run() or \ref findFlow() must be called before using
    /// this function.
    const PotentialMap& potentialMap() const {
      return *_potential;
    }

    /// \brief Return the number of the found paths.
    ///
    /// This function returns the number of the found paths.
    ///
    /// \pre \ref run() or \ref findFlow() must be called before using
    /// this function.
    int pathNum() const {
      return _path_num;
    }

    /// \brief Return a const reference to the specified path.
    ///
    /// This function returns a const reference to the specified path.
    ///
    /// \param i The function returns the <tt>i</tt>-th path.
    /// \c i must be between \c 0 and <tt>%pathNum()-1</tt>.
    ///
    /// \pre \ref run() or \ref findPaths() must be called before using
    /// this function.
    const Path& path(int i) const {
      return _paths[i];
    }

    /// @}

  }; //class Suurballe

  ///@}

} //namespace lemon

#endif //LEMON_SUURBALLE_H
