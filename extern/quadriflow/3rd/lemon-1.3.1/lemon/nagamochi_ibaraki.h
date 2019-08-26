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

#ifndef LEMON_NAGAMOCHI_IBARAKI_H
#define LEMON_NAGAMOCHI_IBARAKI_H


/// \ingroup min_cut
/// \file
/// \brief Implementation of the Nagamochi-Ibaraki algorithm.

#include <lemon/core.h>
#include <lemon/bin_heap.h>
#include <lemon/bucket_heap.h>
#include <lemon/maps.h>
#include <lemon/radix_sort.h>
#include <lemon/unionfind.h>

#include <cassert>

namespace lemon {

  /// \brief Default traits class for NagamochiIbaraki class.
  ///
  /// Default traits class for NagamochiIbaraki class.
  /// \param GR The undirected graph type.
  /// \param CM Type of capacity map.
  template <typename GR, typename CM>
  struct NagamochiIbarakiDefaultTraits {
    /// The type of the capacity map.
    typedef typename CM::Value Value;

    /// The undirected graph type the algorithm runs on.
    typedef GR Graph;

    /// \brief The type of the map that stores the edge capacities.
    ///
    /// The type of the map that stores the edge capacities.
    /// It must meet the \ref concepts::ReadMap "ReadMap" concept.
    typedef CM CapacityMap;

    /// \brief Instantiates a CapacityMap.
    ///
    /// This function instantiates a \ref CapacityMap.
#ifdef DOXYGEN
    static CapacityMap *createCapacityMap(const Graph& graph)
#else
    static CapacityMap *createCapacityMap(const Graph&)
#endif
    {
        LEMON_ASSERT(false, "CapacityMap is not initialized");
        return 0; // ignore warnings
    }

    /// \brief The cross reference type used by heap.
    ///
    /// The cross reference type used by heap.
    /// Usually \c Graph::NodeMap<int>.
    typedef typename Graph::template NodeMap<int> HeapCrossRef;

    /// \brief Instantiates a HeapCrossRef.
    ///
    /// This function instantiates a \ref HeapCrossRef.
    /// \param g is the graph, to which we would like to define the
    /// \ref HeapCrossRef.
    static HeapCrossRef *createHeapCrossRef(const Graph& g) {
      return new HeapCrossRef(g);
    }

    /// \brief The heap type used by NagamochiIbaraki algorithm.
    ///
    /// The heap type used by NagamochiIbaraki algorithm. It has to
    /// maximize the priorities.
    ///
    /// \sa BinHeap
    /// \sa NagamochiIbaraki
    typedef BinHeap<Value, HeapCrossRef, std::greater<Value> > Heap;

    /// \brief Instantiates a Heap.
    ///
    /// This function instantiates a \ref Heap.
    /// \param r is the cross reference of the heap.
    static Heap *createHeap(HeapCrossRef& r) {
      return new Heap(r);
    }
  };

  /// \ingroup min_cut
  ///
  /// \brief Calculates the minimum cut in an undirected graph.
  ///
  /// Calculates the minimum cut in an undirected graph with the
  /// Nagamochi-Ibaraki algorithm. The algorithm separates the graph's
  /// nodes into two partitions with the minimum sum of edge capacities
  /// between the two partitions. The algorithm can be used to test
  /// the network reliability, especially to test how many links have
  /// to be destroyed in the network to split it to at least two
  /// distinict subnetworks.
  ///
  /// The complexity of the algorithm is \f$ O(nm\log(n)) \f$ but with
  /// \ref FibHeap "Fibonacci heap" it can be decreased to
  /// \f$ O(nm+n^2\log(n)) \f$.  When the edges have unit capacities,
  /// \c BucketHeap can be used which yields \f$ O(nm) \f$ time
  /// complexity.
  ///
  /// \warning The value type of the capacity map should be able to
  /// hold any cut value of the graph, otherwise the result can
  /// overflow.
  /// \note This capacity is supposed to be integer type.
#ifdef DOXYGEN
  template <typename GR, typename CM, typename TR>
#else
  template <typename GR,
            typename CM = typename GR::template EdgeMap<int>,
            typename TR = NagamochiIbarakiDefaultTraits<GR, CM> >
#endif
  class NagamochiIbaraki {
  public:

    typedef TR Traits;
    /// The type of the underlying graph.
    typedef typename Traits::Graph Graph;

    /// The type of the capacity map.
    typedef typename Traits::CapacityMap CapacityMap;
    /// The value type of the capacity map.
    typedef typename Traits::CapacityMap::Value Value;

    /// The heap type used by the algorithm.
    typedef typename Traits::Heap Heap;
    /// The cross reference type used for the heap.
    typedef typename Traits::HeapCrossRef HeapCrossRef;

    ///\name Named template parameters

    ///@{

    struct SetUnitCapacityTraits : public Traits {
      typedef ConstMap<typename Graph::Edge, Const<int, 1> > CapacityMap;
      static CapacityMap *createCapacityMap(const Graph&) {
        return new CapacityMap();
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// the capacity map to a constMap<Edge, int, 1>() instance
    ///
    /// \ref named-templ-param "Named parameter" for setting
    /// the capacity map to a constMap<Edge, int, 1>() instance
    struct SetUnitCapacity
      : public NagamochiIbaraki<Graph, CapacityMap,
                                SetUnitCapacityTraits> {
      typedef NagamochiIbaraki<Graph, CapacityMap,
                               SetUnitCapacityTraits> Create;
    };


    template <class H, class CR>
    struct SetHeapTraits : public Traits {
      typedef CR HeapCrossRef;
      typedef H Heap;
      static HeapCrossRef *createHeapCrossRef(int num) {
        LEMON_ASSERT(false, "HeapCrossRef is not initialized");
        return 0; // ignore warnings
      }
      static Heap *createHeap(HeapCrossRef &) {
        LEMON_ASSERT(false, "Heap is not initialized");
        return 0; // ignore warnings
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// heap and cross reference type
    ///
    /// \ref named-templ-param "Named parameter" for setting heap and
    /// cross reference type. The heap has to maximize the priorities.
    template <class H, class CR = RangeMap<int> >
    struct SetHeap
      : public NagamochiIbaraki<Graph, CapacityMap, SetHeapTraits<H, CR> > {
      typedef NagamochiIbaraki< Graph, CapacityMap, SetHeapTraits<H, CR> >
      Create;
    };

    template <class H, class CR>
    struct SetStandardHeapTraits : public Traits {
      typedef CR HeapCrossRef;
      typedef H Heap;
      static HeapCrossRef *createHeapCrossRef(int size) {
        return new HeapCrossRef(size);
      }
      static Heap *createHeap(HeapCrossRef &crossref) {
        return new Heap(crossref);
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting
    /// heap and cross reference type with automatic allocation
    ///
    /// \ref named-templ-param "Named parameter" for setting heap and
    /// cross reference type with automatic allocation. They should
    /// have standard constructor interfaces to be able to
    /// automatically created by the algorithm (i.e. the graph should
    /// be passed to the constructor of the cross reference and the
    /// cross reference should be passed to the constructor of the
    /// heap). However, external heap and cross reference objects
    /// could also be passed to the algorithm using the \ref heap()
    /// function before calling \ref run() or \ref init(). The heap
    /// has to maximize the priorities.
    /// \sa SetHeap
    template <class H, class CR = RangeMap<int> >
    struct SetStandardHeap
      : public NagamochiIbaraki<Graph, CapacityMap,
                                SetStandardHeapTraits<H, CR> > {
      typedef NagamochiIbaraki<Graph, CapacityMap,
                               SetStandardHeapTraits<H, CR> > Create;
    };

    ///@}


  private:

    const Graph &_graph;
    const CapacityMap *_capacity;
    bool _local_capacity; // unit capacity

    struct ArcData {
      typename Graph::Node target;
      int prev, next;
    };
    struct EdgeData {
      Value capacity;
      Value cut;
    };

    struct NodeData {
      int first_arc;
      typename Graph::Node prev, next;
      int curr_arc;
      typename Graph::Node last_rep;
      Value sum;
    };

    typename Graph::template NodeMap<NodeData> *_nodes;
    std::vector<ArcData> _arcs;
    std::vector<EdgeData> _edges;

    typename Graph::Node _first_node;
    int _node_num;

    Value _min_cut;

    HeapCrossRef *_heap_cross_ref;
    bool _local_heap_cross_ref;
    Heap *_heap;
    bool _local_heap;

    typedef typename Graph::template NodeMap<typename Graph::Node> NodeList;
    NodeList *_next_rep;

    typedef typename Graph::template NodeMap<bool> MinCutMap;
    MinCutMap *_cut_map;

    void createStructures() {
      if (!_nodes) {
        _nodes = new (typename Graph::template NodeMap<NodeData>)(_graph);
      }
      if (!_capacity) {
        _local_capacity = true;
        _capacity = Traits::createCapacityMap(_graph);
      }
      if (!_heap_cross_ref) {
        _local_heap_cross_ref = true;
        _heap_cross_ref = Traits::createHeapCrossRef(_graph);
      }
      if (!_heap) {
        _local_heap = true;
        _heap = Traits::createHeap(*_heap_cross_ref);
      }
      if (!_next_rep) {
        _next_rep = new NodeList(_graph);
      }
      if (!_cut_map) {
        _cut_map = new MinCutMap(_graph);
      }
    }

  protected:
    //This is here to avoid a gcc-3.3 compilation error.
    //It should never be called.
    NagamochiIbaraki() {}

  public:

    typedef NagamochiIbaraki Create;


    /// \brief Constructor.
    ///
    /// \param graph The graph the algorithm runs on.
    /// \param capacity The capacity map used by the algorithm.
    NagamochiIbaraki(const Graph& graph, const CapacityMap& capacity)
      : _graph(graph), _capacity(&capacity), _local_capacity(false),
        _nodes(0), _arcs(), _edges(), _min_cut(),
        _heap_cross_ref(0), _local_heap_cross_ref(false),
        _heap(0), _local_heap(false),
        _next_rep(0), _cut_map(0) {}

    /// \brief Constructor.
    ///
    /// This constructor can be used only when the Traits class
    /// defines how can the local capacity map be instantiated.
    /// If the SetUnitCapacity used the algorithm automatically
    /// constructs the capacity map.
    ///
    ///\param graph The graph the algorithm runs on.
    NagamochiIbaraki(const Graph& graph)
      : _graph(graph), _capacity(0), _local_capacity(false),
        _nodes(0), _arcs(), _edges(), _min_cut(),
        _heap_cross_ref(0), _local_heap_cross_ref(false),
        _heap(0), _local_heap(false),
        _next_rep(0), _cut_map(0) {}

    /// \brief Destructor.
    ///
    /// Destructor.
    ~NagamochiIbaraki() {
      if (_local_capacity) delete _capacity;
      if (_nodes) delete _nodes;
      if (_local_heap) delete _heap;
      if (_local_heap_cross_ref) delete _heap_cross_ref;
      if (_next_rep) delete _next_rep;
      if (_cut_map) delete _cut_map;
    }

    /// \brief Sets the heap and the cross reference used by algorithm.
    ///
    /// Sets the heap and the cross reference used by algorithm.
    /// If you don't use this function before calling \ref run(),
    /// it will allocate one. The destuctor deallocates this
    /// automatically allocated heap and cross reference, of course.
    /// \return <tt> (*this) </tt>
    NagamochiIbaraki &heap(Heap& hp, HeapCrossRef &cr)
    {
      if (_local_heap_cross_ref) {
        delete _heap_cross_ref;
        _local_heap_cross_ref = false;
      }
      _heap_cross_ref = &cr;
      if (_local_heap) {
        delete _heap;
        _local_heap = false;
      }
      _heap = &hp;
      return *this;
    }

    /// \name Execution control
    /// The simplest way to execute the algorithm is to use
    /// one of the member functions called \c run().
    /// \n
    /// If you need more control on the execution,
    /// first you must call \ref init() and then call the start()
    /// or proper times the processNextPhase() member functions.

    ///@{

    /// \brief Initializes the internal data structures.
    ///
    /// Initializes the internal data structures.
    void init() {
      createStructures();

      int edge_num = countEdges(_graph);
      _edges.resize(edge_num);
      _arcs.resize(2 * edge_num);

      typename Graph::Node prev = INVALID;
      _node_num = 0;
      for (typename Graph::NodeIt n(_graph); n != INVALID; ++n) {
        (*_cut_map)[n] = false;
        (*_next_rep)[n] = INVALID;
        (*_nodes)[n].last_rep = n;
        (*_nodes)[n].first_arc = -1;
        (*_nodes)[n].curr_arc = -1;
        (*_nodes)[n].prev = prev;
        if (prev != INVALID) {
          (*_nodes)[prev].next = n;
        }
        (*_nodes)[n].next = INVALID;
        (*_nodes)[n].sum = 0;
        prev = n;
        ++_node_num;
      }

      _first_node = typename Graph::NodeIt(_graph);

      int index = 0;
      for (typename Graph::NodeIt n(_graph); n != INVALID; ++n) {
        for (typename Graph::OutArcIt a(_graph, n); a != INVALID; ++a) {
          typename Graph::Node m = _graph.target(a);

          if (!(n < m)) continue;

          (*_nodes)[n].sum += (*_capacity)[a];
          (*_nodes)[m].sum += (*_capacity)[a];

          int c = (*_nodes)[m].curr_arc;
          if (c != -1 && _arcs[c ^ 1].target == n) {
            _edges[c >> 1].capacity += (*_capacity)[a];
          } else {
            _edges[index].capacity = (*_capacity)[a];

            _arcs[index << 1].prev = -1;
            if ((*_nodes)[n].first_arc != -1) {
              _arcs[(*_nodes)[n].first_arc].prev = (index << 1);
            }
            _arcs[index << 1].next = (*_nodes)[n].first_arc;
            (*_nodes)[n].first_arc = (index << 1);
            _arcs[index << 1].target = m;

            (*_nodes)[m].curr_arc = (index << 1);

            _arcs[(index << 1) | 1].prev = -1;
            if ((*_nodes)[m].first_arc != -1) {
              _arcs[(*_nodes)[m].first_arc].prev = ((index << 1) | 1);
            }
            _arcs[(index << 1) | 1].next = (*_nodes)[m].first_arc;
            (*_nodes)[m].first_arc = ((index << 1) | 1);
            _arcs[(index << 1) | 1].target = n;

            ++index;
          }
        }
      }

      typename Graph::Node cut_node = INVALID;
      _min_cut = std::numeric_limits<Value>::max();

      for (typename Graph::Node n = _first_node;
           n != INVALID; n = (*_nodes)[n].next) {
        if ((*_nodes)[n].sum < _min_cut) {
          cut_node = n;
          _min_cut = (*_nodes)[n].sum;
        }
      }
      (*_cut_map)[cut_node] = true;
      if (_min_cut == 0) {
        _first_node = INVALID;
      }
    }

  public:

    /// \brief Processes the next phase
    ///
    /// Processes the next phase in the algorithm. It must be called
    /// at most one less the number of the nodes in the graph.
    ///
    ///\return %True when the algorithm finished.
    bool processNextPhase() {
      if (_first_node == INVALID) return true;

      _heap->clear();
      for (typename Graph::Node n = _first_node;
           n != INVALID; n = (*_nodes)[n].next) {
        (*_heap_cross_ref)[n] = Heap::PRE_HEAP;
      }

      std::vector<typename Graph::Node> order;
      order.reserve(_node_num);
      int sep = 0;

      Value alpha = 0;
      Value pmc = std::numeric_limits<Value>::max();

      _heap->push(_first_node, static_cast<Value>(0));
      while (!_heap->empty()) {
        typename Graph::Node n = _heap->top();
        Value v = _heap->prio();

        _heap->pop();
        for (int a = (*_nodes)[n].first_arc; a != -1; a = _arcs[a].next) {
          switch (_heap->state(_arcs[a].target)) {
          case Heap::PRE_HEAP:
            {
              Value nv = _edges[a >> 1].capacity;
              _heap->push(_arcs[a].target, nv);
              _edges[a >> 1].cut = nv;
            } break;
          case Heap::IN_HEAP:
            {
              Value nv = _edges[a >> 1].capacity + (*_heap)[_arcs[a].target];
              _heap->decrease(_arcs[a].target, nv);
              _edges[a >> 1].cut = nv;
            } break;
          case Heap::POST_HEAP:
            break;
          }
        }

        alpha += (*_nodes)[n].sum;
        alpha -= 2 * v;

        order.push_back(n);
        if (!_heap->empty()) {
          if (alpha < pmc) {
            pmc = alpha;
            sep = order.size();
          }
        }
      }

      if (static_cast<int>(order.size()) < _node_num) {
        _first_node = INVALID;
        for (typename Graph::NodeIt n(_graph); n != INVALID; ++n) {
          (*_cut_map)[n] = false;
        }
        for (int i = 0; i < static_cast<int>(order.size()); ++i) {
          typename Graph::Node n = order[i];
          while (n != INVALID) {
            (*_cut_map)[n] = true;
            n = (*_next_rep)[n];
          }
        }
        _min_cut = 0;
        return true;
      }

      if (pmc < _min_cut) {
        for (typename Graph::NodeIt n(_graph); n != INVALID; ++n) {
          (*_cut_map)[n] = false;
        }
        for (int i = 0; i < sep; ++i) {
          typename Graph::Node n = order[i];
          while (n != INVALID) {
            (*_cut_map)[n] = true;
            n = (*_next_rep)[n];
          }
        }
        _min_cut = pmc;
      }

      for (typename Graph::Node n = _first_node;
           n != INVALID; n = (*_nodes)[n].next) {
        bool merged = false;
        for (int a = (*_nodes)[n].first_arc; a != -1; a = _arcs[a].next) {
          if (!(_edges[a >> 1].cut < pmc)) {
            if (!merged) {
              for (int b = (*_nodes)[n].first_arc; b != -1; b = _arcs[b].next) {
                (*_nodes)[_arcs[b].target].curr_arc = b;
              }
              merged = true;
            }
            typename Graph::Node m = _arcs[a].target;
            int nb = 0;
            for (int b = (*_nodes)[m].first_arc; b != -1; b = nb) {
              nb = _arcs[b].next;
              if ((b ^ a) == 1) continue;
              typename Graph::Node o = _arcs[b].target;
              int c = (*_nodes)[o].curr_arc;
              if (c != -1 && _arcs[c ^ 1].target == n) {
                _edges[c >> 1].capacity += _edges[b >> 1].capacity;
                (*_nodes)[n].sum += _edges[b >> 1].capacity;
                if (_edges[b >> 1].cut < _edges[c >> 1].cut) {
                  _edges[b >> 1].cut = _edges[c >> 1].cut;
                }
                if (_arcs[b ^ 1].prev != -1) {
                  _arcs[_arcs[b ^ 1].prev].next = _arcs[b ^ 1].next;
                } else {
                  (*_nodes)[o].first_arc = _arcs[b ^ 1].next;
                }
                if (_arcs[b ^ 1].next != -1) {
                  _arcs[_arcs[b ^ 1].next].prev = _arcs[b ^ 1].prev;
                }
              } else {
                if (_arcs[a].next != -1) {
                  _arcs[_arcs[a].next].prev = b;
                }
                _arcs[b].next = _arcs[a].next;
                _arcs[b].prev = a;
                _arcs[a].next = b;
                _arcs[b ^ 1].target = n;

                (*_nodes)[n].sum += _edges[b >> 1].capacity;
                (*_nodes)[o].curr_arc = b;
              }
            }

            if (_arcs[a].prev != -1) {
              _arcs[_arcs[a].prev].next = _arcs[a].next;
            } else {
              (*_nodes)[n].first_arc = _arcs[a].next;
            }
            if (_arcs[a].next != -1) {
              _arcs[_arcs[a].next].prev = _arcs[a].prev;
            }

            (*_nodes)[n].sum -= _edges[a >> 1].capacity;
            (*_next_rep)[(*_nodes)[n].last_rep] = m;
            (*_nodes)[n].last_rep = (*_nodes)[m].last_rep;

            if ((*_nodes)[m].prev != INVALID) {
              (*_nodes)[(*_nodes)[m].prev].next = (*_nodes)[m].next;
            } else{
              _first_node = (*_nodes)[m].next;
            }
            if ((*_nodes)[m].next != INVALID) {
              (*_nodes)[(*_nodes)[m].next].prev = (*_nodes)[m].prev;
            }
            --_node_num;
          }
        }
      }

      if (_node_num == 1) {
        _first_node = INVALID;
        return true;
      }

      return false;
    }

    /// \brief Executes the algorithm.
    ///
    /// Executes the algorithm.
    ///
    /// \pre init() must be called
    void start() {
      while (!processNextPhase()) {}
    }


    /// \brief Runs %NagamochiIbaraki algorithm.
    ///
    /// This method runs the %Min cut algorithm
    ///
    /// \note mc.run(s) is just a shortcut of the following code.
    ///\code
    ///  mc.init();
    ///  mc.start();
    ///\endcode
    void run() {
      init();
      start();
    }

    ///@}

    /// \name Query Functions
    ///
    /// The result of the %NagamochiIbaraki
    /// algorithm can be obtained using these functions.\n
    /// Before the use of these functions, either run() or start()
    /// must be called.

    ///@{

    /// \brief Returns the min cut value.
    ///
    /// Returns the min cut value if the algorithm finished.
    /// After the first processNextPhase() it is a value of a
    /// valid cut in the graph.
    Value minCutValue() const {
      return _min_cut;
    }

    /// \brief Returns a min cut in a NodeMap.
    ///
    /// It sets the nodes of one of the two partitions to true and
    /// the other partition to false.
    /// \param cutMap A \ref concepts::WriteMap "writable" node map with
    /// \c bool (or convertible) value type.
    template <typename CutMap>
    Value minCutMap(CutMap& cutMap) const {
      for (typename Graph::NodeIt n(_graph); n != INVALID; ++n) {
        cutMap.set(n, (*_cut_map)[n]);
      }
      return minCutValue();
    }

    ///@}

  };
}

#endif
