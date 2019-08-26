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

#ifndef LEMON_MAX_CARDINALITY_SEARCH_H
#define LEMON_MAX_CARDINALITY_SEARCH_H


/// \ingroup search
/// \file
/// \brief Maximum cardinality search in undirected digraphs.

#include <lemon/bin_heap.h>
#include <lemon/bucket_heap.h>

#include <lemon/error.h>
#include <lemon/maps.h>

#include <functional>

namespace lemon {

  /// \brief Default traits class of MaxCardinalitySearch class.
  ///
  /// Default traits class of MaxCardinalitySearch class.
  /// \param Digraph Digraph type.
  /// \param CapacityMap Type of capacity map.
  template <typename GR, typename CAP>
  struct MaxCardinalitySearchDefaultTraits {
    /// The digraph type the algorithm runs on.
    typedef GR Digraph;

    template <typename CM>
    struct CapMapSelector {

      typedef CM CapacityMap;

      static CapacityMap *createCapacityMap(const Digraph& g) {
        return new CapacityMap(g);
      }
    };

    template <typename CM>
    struct CapMapSelector<ConstMap<CM, Const<int, 1> > > {

      typedef ConstMap<CM, Const<int, 1> > CapacityMap;

      static CapacityMap *createCapacityMap(const Digraph&) {
        return new CapacityMap;
      }
    };

    /// \brief The type of the map that stores the arc capacities.
    ///
    /// The type of the map that stores the arc capacities.
    /// It must meet the \ref concepts::ReadMap "ReadMap" concept.
    typedef typename CapMapSelector<CAP>::CapacityMap CapacityMap;

    /// \brief The type of the capacity of the arcs.
    typedef typename CapacityMap::Value Value;

    /// \brief Instantiates a CapacityMap.
    ///
    /// This function instantiates a \ref CapacityMap.
    /// \param digraph is the digraph, to which we would like to define
    /// the CapacityMap.
    static CapacityMap *createCapacityMap(const Digraph& digraph) {
      return CapMapSelector<CapacityMap>::createCapacityMap(digraph);
    }

    /// \brief The cross reference type used by heap.
    ///
    /// The cross reference type used by heap.
    /// Usually it is \c Digraph::NodeMap<int>.
    typedef typename Digraph::template NodeMap<int> HeapCrossRef;

    /// \brief Instantiates a HeapCrossRef.
    ///
    /// This function instantiates a \ref HeapCrossRef.
    /// \param digraph is the digraph, to which we would like to define the
    /// HeapCrossRef.
    static HeapCrossRef *createHeapCrossRef(const Digraph &digraph) {
      return new HeapCrossRef(digraph);
    }

    template <typename CapacityMap>
    struct HeapSelector {
      template <typename Value, typename Ref>
      struct Selector {
        typedef BinHeap<Value, Ref, std::greater<Value> > Heap;
      };
    };

    template <typename CapacityKey>
    struct HeapSelector<ConstMap<CapacityKey, Const<int, 1> > > {
      template <typename Value, typename Ref>
      struct Selector {
        typedef BucketHeap<Ref, false > Heap;
      };
    };

    /// \brief The heap type used by MaxCardinalitySearch algorithm.
    ///
    /// The heap type used by MaxCardinalitySearch algorithm. It should
    /// maximalize the priorities. The default heap type is
    /// the \ref BinHeap, but it is specialized when the
    /// CapacityMap is ConstMap<Digraph::Node, Const<int, 1> >
    /// to BucketHeap.
    ///
    /// \sa MaxCardinalitySearch
    typedef typename HeapSelector<CapacityMap>
    ::template Selector<Value, HeapCrossRef>
    ::Heap Heap;

    /// \brief Instantiates a Heap.
    ///
    /// This function instantiates a \ref Heap.
    /// \param crossref The cross reference of the heap.
    static Heap *createHeap(HeapCrossRef& crossref) {
      return new Heap(crossref);
    }

    /// \brief The type of the map that stores whether a node is processed.
    ///
    /// The type of the map that stores whether a node is processed.
    /// It must meet the \ref concepts::WriteMap "WriteMap" concept.
    /// By default it is a NullMap.
    typedef NullMap<typename Digraph::Node, bool> ProcessedMap;

    /// \brief Instantiates a ProcessedMap.
    ///
    /// This function instantiates a \ref ProcessedMap.
    /// \param digraph is the digraph, to which
    /// we would like to define the \ref ProcessedMap
#ifdef DOXYGEN
    static ProcessedMap *createProcessedMap(const Digraph &digraph)
#else
    static ProcessedMap *createProcessedMap(const Digraph &)
#endif
    {
      return new ProcessedMap();
    }

    /// \brief The type of the map that stores the cardinalities of the nodes.
    ///
    /// The type of the map that stores the cardinalities of the nodes.
    /// It must meet the \ref concepts::WriteMap "WriteMap" concept.
    typedef typename Digraph::template NodeMap<Value> CardinalityMap;

    /// \brief Instantiates a CardinalityMap.
    ///
    /// This function instantiates a \ref CardinalityMap.
    /// \param digraph is the digraph, to which we would like to
    /// define the \ref CardinalityMap
    static CardinalityMap *createCardinalityMap(const Digraph &digraph) {
      return new CardinalityMap(digraph);
    }


  };

  /// \ingroup search
  ///
  /// \brief Maximum Cardinality Search algorithm class.
  ///
  /// This class provides an efficient implementation of Maximum Cardinality
  /// Search algorithm. The maximum cardinality search first chooses any
  /// node of the digraph. Then every time it chooses one unprocessed node
  /// with maximum cardinality, i.e the sum of capacities on out arcs
  /// to the nodes
  /// which were previusly processed.
  /// If there is a cut in the digraph the algorithm should choose
  /// again any unprocessed node of the digraph.

  /// The arc capacities are passed to the algorithm using a
  /// \ref concepts::ReadMap "ReadMap", so it is easy to change it to any
  /// kind of capacity.
  ///
  /// The type of the capacity is determined by the \ref
  /// concepts::ReadMap::Value "Value" of the capacity map.
  ///
  /// It is also possible to change the underlying priority heap.
  ///
  ///
  /// \param GR The digraph type the algorithm runs on. The value of
  /// Digraph is not used directly by the search algorithm, it
  /// is only passed to \ref MaxCardinalitySearchDefaultTraits.
  /// \param CAP This read-only ArcMap determines the capacities of
  /// the arcs. It is read once for each arc, so the map may involve in
  /// relatively time consuming process to compute the arc capacity if
  /// it is necessary. The default map type is \ref
  /// ConstMap "ConstMap<concepts::Digraph::Arc, Const<int,1> >". The value
  /// of CapacityMap is not used directly by search algorithm, it is only
  /// passed to \ref MaxCardinalitySearchDefaultTraits.
  /// \param TR Traits class to set various data types used by the
  /// algorithm.  The default traits class is
  /// \ref MaxCardinalitySearchDefaultTraits
  /// "MaxCardinalitySearchDefaultTraits<GR, CAP>".
  /// See \ref MaxCardinalitySearchDefaultTraits
  /// for the documentation of a MaxCardinalitySearch traits class.

#ifdef DOXYGEN
  template <typename GR, typename CAP, typename TR>
#else
  template <typename GR, typename CAP =
            ConstMap<typename GR::Arc, Const<int,1> >,
            typename TR =
            MaxCardinalitySearchDefaultTraits<GR, CAP> >
#endif
  class MaxCardinalitySearch {
  public:

    typedef TR Traits;
    ///The type of the underlying digraph.
    typedef typename Traits::Digraph Digraph;

    ///The type of the capacity of the arcs.
    typedef typename Traits::CapacityMap::Value Value;
    ///The type of the map that stores the arc capacities.
    typedef typename Traits::CapacityMap CapacityMap;
    ///The type of the map indicating if a node is processed.
    typedef typename Traits::ProcessedMap ProcessedMap;
    ///The type of the map that stores the cardinalities of the nodes.
    typedef typename Traits::CardinalityMap CardinalityMap;
    ///The cross reference type used for the current heap.
    typedef typename Traits::HeapCrossRef HeapCrossRef;
    ///The heap type used by the algorithm. It maximizes the priorities.
    typedef typename Traits::Heap Heap;
  private:
    // Pointer to the underlying digraph.
    const Digraph *_graph;
    // Pointer to the capacity map
    const CapacityMap *_capacity;
    // Indicates if \ref _capacity is locally allocated (\c true) or not.
    bool local_capacity;
    // Pointer to the map of cardinality.
    CardinalityMap *_cardinality;
    // Indicates if \ref _cardinality is locally allocated (\c true) or not.
    bool local_cardinality;
    // Pointer to the map of processed status of the nodes.
    ProcessedMap *_processed;
    // Indicates if \ref _processed is locally allocated (\c true) or not.
    bool local_processed;
    // Pointer to the heap cross references.
    HeapCrossRef *_heap_cross_ref;
    // Indicates if \ref _heap_cross_ref is locally allocated (\c true) or not.
    bool local_heap_cross_ref;
    // Pointer to the heap.
    Heap *_heap;
    // Indicates if \ref _heap is locally allocated (\c true) or not.
    bool local_heap;

  public :

    typedef MaxCardinalitySearch Create;

    ///\name Named template parameters

    ///@{

    template <class T>
    struct DefCapacityMapTraits : public Traits {
      typedef T CapacityMap;
      static CapacityMap *createCapacityMap(const Digraph &) {
               LEMON_ASSERT(false,"Uninitialized parameter.");
        return 0;
      }
    };
    /// \brief \ref named-templ-param "Named parameter" for setting
    /// CapacityMap type
    ///
    /// \ref named-templ-param "Named parameter" for setting CapacityMap type
    /// for the algorithm.
    template <class T>
    struct SetCapacityMap
      : public MaxCardinalitySearch<Digraph, CapacityMap,
                                    DefCapacityMapTraits<T> > {
      typedef MaxCardinalitySearch<Digraph, CapacityMap,
                                   DefCapacityMapTraits<T> > Create;
    };

    template <class T>
    struct DefCardinalityMapTraits : public Traits {
      typedef T CardinalityMap;
      static CardinalityMap *createCardinalityMap(const Digraph &)
      {
        LEMON_ASSERT(false,"Uninitialized parameter.");
        return 0;
      }
    };
    /// \brief \ref named-templ-param "Named parameter" for setting
    /// CardinalityMap type
    ///
    /// \ref named-templ-param "Named parameter" for setting CardinalityMap
    /// type for the algorithm.
    template <class T>
    struct SetCardinalityMap
      : public MaxCardinalitySearch<Digraph, CapacityMap,
                                    DefCardinalityMapTraits<T> > {
      typedef MaxCardinalitySearch<Digraph, CapacityMap,
                                   DefCardinalityMapTraits<T> > Create;
    };

    template <class T>
    struct DefProcessedMapTraits : public Traits {
      typedef T ProcessedMap;
      static ProcessedMap *createProcessedMap(const Digraph &) {
               LEMON_ASSERT(false,"Uninitialized parameter.");
        return 0;
      }
    };
    /// \brief \ref named-templ-param "Named parameter" for setting
    /// ProcessedMap type
    ///
    /// \ref named-templ-param "Named parameter" for setting ProcessedMap type
    /// for the algorithm.
    template <class T>
    struct SetProcessedMap
      : public MaxCardinalitySearch<Digraph, CapacityMap,
                                    DefProcessedMapTraits<T> > {
      typedef MaxCardinalitySearch<Digraph, CapacityMap,
                                   DefProcessedMapTraits<T> > Create;
    };

    template <class H, class CR>
    struct DefHeapTraits : public Traits {
      typedef CR HeapCrossRef;
      typedef H Heap;
      static HeapCrossRef *createHeapCrossRef(const Digraph &) {
             LEMON_ASSERT(false,"Uninitialized parameter.");
        return 0;
      }
      static Heap *createHeap(HeapCrossRef &) {
               LEMON_ASSERT(false,"Uninitialized parameter.");
        return 0;
      }
    };
    /// \brief \ref named-templ-param "Named parameter" for setting heap
    /// and cross reference type
    ///
    /// \ref named-templ-param "Named parameter" for setting heap and cross
    /// reference type for the algorithm.
    template <class H, class CR = typename Digraph::template NodeMap<int> >
    struct SetHeap
      : public MaxCardinalitySearch<Digraph, CapacityMap,
                                    DefHeapTraits<H, CR> > {
      typedef MaxCardinalitySearch< Digraph, CapacityMap,
                                    DefHeapTraits<H, CR> > Create;
    };

    template <class H, class CR>
    struct DefStandardHeapTraits : public Traits {
      typedef CR HeapCrossRef;
      typedef H Heap;
      static HeapCrossRef *createHeapCrossRef(const Digraph &digraph) {
        return new HeapCrossRef(digraph);
      }
      static Heap *createHeap(HeapCrossRef &crossref) {
        return new Heap(crossref);
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for setting heap and
    /// cross reference type with automatic allocation
    ///
    /// \ref named-templ-param "Named parameter" for setting heap and cross
    /// reference type. It can allocate the heap and the cross reference
    /// object if the cross reference's constructor waits for the digraph as
    /// parameter and the heap's constructor waits for the cross reference.
    template <class H, class CR = typename Digraph::template NodeMap<int> >
    struct SetStandardHeap
      : public MaxCardinalitySearch<Digraph, CapacityMap,
                                    DefStandardHeapTraits<H, CR> > {
      typedef MaxCardinalitySearch<Digraph, CapacityMap,
                                   DefStandardHeapTraits<H, CR> >
      Create;
    };

    ///@}


  protected:

    MaxCardinalitySearch() {}

  public:

    /// \brief Constructor.
    ///
    ///\param digraph the digraph the algorithm will run on.
    ///\param capacity the capacity map used by the algorithm.
    MaxCardinalitySearch(const Digraph& digraph,
                         const CapacityMap& capacity) :
      _graph(&digraph),
      _capacity(&capacity), local_capacity(false),
      _cardinality(0), local_cardinality(false),
      _processed(0), local_processed(false),
      _heap_cross_ref(0), local_heap_cross_ref(false),
      _heap(0), local_heap(false)
    { }

    /// \brief Constructor.
    ///
    ///\param digraph the digraph the algorithm will run on.
    ///
    ///A constant 1 capacity map will be allocated.
    MaxCardinalitySearch(const Digraph& digraph) :
      _graph(&digraph),
      _capacity(0), local_capacity(false),
      _cardinality(0), local_cardinality(false),
      _processed(0), local_processed(false),
      _heap_cross_ref(0), local_heap_cross_ref(false),
      _heap(0), local_heap(false)
    { }

    /// \brief Destructor.
    ~MaxCardinalitySearch() {
      if(local_capacity) delete _capacity;
      if(local_cardinality) delete _cardinality;
      if(local_processed) delete _processed;
      if(local_heap_cross_ref) delete _heap_cross_ref;
      if(local_heap) delete _heap;
    }

    /// \brief Sets the capacity map.
    ///
    /// Sets the capacity map.
    /// \return <tt> (*this) </tt>
    MaxCardinalitySearch &capacityMap(const CapacityMap &m) {
      if (local_capacity) {
        delete _capacity;
        local_capacity=false;
      }
      _capacity=&m;
      return *this;
    }

    /// \brief Returns a const reference to the capacity map.
    ///
    /// Returns a const reference to the capacity map used by
    /// the algorithm.
    const CapacityMap &capacityMap() const {
      return *_capacity;
    }

    /// \brief Sets the map storing the cardinalities calculated by the
    /// algorithm.
    ///
    /// Sets the map storing the cardinalities calculated by the algorithm.
    /// If you don't use this function before calling \ref run(),
    /// it will allocate one. The destuctor deallocates this
    /// automatically allocated map, of course.
    /// \return <tt> (*this) </tt>
    MaxCardinalitySearch &cardinalityMap(CardinalityMap &m) {
      if(local_cardinality) {
        delete _cardinality;
        local_cardinality=false;
      }
      _cardinality = &m;
      return *this;
    }

    /// \brief Sets the map storing the processed nodes.
    ///
    /// Sets the map storing the processed nodes.
    /// If you don't use this function before calling \ref run(),
    /// it will allocate one. The destuctor deallocates this
    /// automatically allocated map, of course.
    /// \return <tt> (*this) </tt>
    MaxCardinalitySearch &processedMap(ProcessedMap &m)
    {
      if(local_processed) {
        delete _processed;
        local_processed=false;
      }
      _processed = &m;
      return *this;
    }

    /// \brief Returns a const reference to the cardinality map.
    ///
    /// Returns a const reference to the cardinality map used by
    /// the algorithm.
    const ProcessedMap &processedMap() const {
      return *_processed;
    }

    /// \brief Sets the heap and the cross reference used by algorithm.
    ///
    /// Sets the heap and the cross reference used by algorithm.
    /// If you don't use this function before calling \ref run(),
    /// it will allocate one. The destuctor deallocates this
    /// automatically allocated map, of course.
    /// \return <tt> (*this) </tt>
    MaxCardinalitySearch &heap(Heap& hp, HeapCrossRef &cr) {
      if(local_heap_cross_ref) {
        delete _heap_cross_ref;
        local_heap_cross_ref = false;
      }
      _heap_cross_ref = &cr;
      if(local_heap) {
        delete _heap;
        local_heap = false;
      }
      _heap = &hp;
      return *this;
    }

    /// \brief Returns a const reference to the heap.
    ///
    /// Returns a const reference to the heap used by
    /// the algorithm.
    const Heap &heap() const {
      return *_heap;
    }

    /// \brief Returns a const reference to the cross reference.
    ///
    /// Returns a const reference to the cross reference
    /// of the heap.
    const HeapCrossRef &heapCrossRef() const {
      return *_heap_cross_ref;
    }

  private:

    typedef typename Digraph::Node Node;
    typedef typename Digraph::NodeIt NodeIt;
    typedef typename Digraph::Arc Arc;
    typedef typename Digraph::InArcIt InArcIt;

    void create_maps() {
      if(!_capacity) {
        local_capacity = true;
        _capacity = Traits::createCapacityMap(*_graph);
      }
      if(!_cardinality) {
        local_cardinality = true;
        _cardinality = Traits::createCardinalityMap(*_graph);
      }
      if(!_processed) {
        local_processed = true;
        _processed = Traits::createProcessedMap(*_graph);
      }
      if (!_heap_cross_ref) {
        local_heap_cross_ref = true;
        _heap_cross_ref = Traits::createHeapCrossRef(*_graph);
      }
      if (!_heap) {
        local_heap = true;
        _heap = Traits::createHeap(*_heap_cross_ref);
      }
    }

    void finalizeNodeData(Node node, Value capacity) {
      _processed->set(node, true);
      _cardinality->set(node, capacity);
    }

  public:
    /// \name Execution control
    /// The simplest way to execute the algorithm is to use
    /// one of the member functions called \ref run().
    /// \n
    /// If you need more control on the execution,
    /// first you must call \ref init(), then you can add several source nodes
    /// with \ref addSource().
    /// Finally \ref start() will perform the computation.

    ///@{

    /// \brief Initializes the internal data structures.
    ///
    /// Initializes the internal data structures, and clears the heap.
    void init() {
      create_maps();
      _heap->clear();
      for (NodeIt it(*_graph) ; it != INVALID ; ++it) {
        _processed->set(it, false);
        _heap_cross_ref->set(it, Heap::PRE_HEAP);
      }
    }

    /// \brief Adds a new source node.
    ///
    /// Adds a new source node to the priority heap.
    ///
    /// It checks if the node has not yet been added to the heap.
    void addSource(Node source, Value capacity = 0) {
      if(_heap->state(source) == Heap::PRE_HEAP) {
        _heap->push(source, capacity);
      }
    }

    /// \brief Processes the next node in the priority heap
    ///
    /// Processes the next node in the priority heap.
    ///
    /// \return The processed node.
    ///
    /// \warning The priority heap must not be empty!
    Node processNextNode() {
      Node node = _heap->top();
      finalizeNodeData(node, _heap->prio());
      _heap->pop();

      for (InArcIt it(*_graph, node); it != INVALID; ++it) {
        Node source = _graph->source(it);
        switch (_heap->state(source)) {
        case Heap::PRE_HEAP:
          _heap->push(source, (*_capacity)[it]);
          break;
        case Heap::IN_HEAP:
          _heap->decrease(source, (*_heap)[source] + (*_capacity)[it]);
          break;
        case Heap::POST_HEAP:
          break;
        }
      }
      return node;
    }

    /// \brief Next node to be processed.
    ///
    /// Next node to be processed.
    ///
    /// \return The next node to be processed or INVALID if the
    /// priority heap is empty.
    Node nextNode() {
      return !_heap->empty() ? _heap->top() : INVALID;
    }

    /// \brief Returns \c false if there are nodes
    /// to be processed in the priority heap
    ///
    /// Returns \c false if there are nodes
    /// to be processed in the priority heap
    bool emptyQueue() { return _heap->empty(); }
    /// \brief Returns the number of the nodes to be processed
    /// in the priority heap
    ///
    /// Returns the number of the nodes to be processed in the priority heap
    int emptySize() { return _heap->size(); }

    /// \brief Executes the algorithm.
    ///
    /// Executes the algorithm.
    ///
    ///\pre init() must be called and at least one node should be added
    /// with addSource() before using this function.
    ///
    /// This method runs the Maximum Cardinality Search algorithm from the
    /// source node(s).
    void start() {
      while ( !_heap->empty() ) processNextNode();
    }

    /// \brief Executes the algorithm until \c dest is reached.
    ///
    /// Executes the algorithm until \c dest is reached.
    ///
    /// \pre init() must be called and at least one node should be added
    /// with addSource() before using this function.
    ///
    /// This method runs the %MaxCardinalitySearch algorithm from the source
    /// nodes.
    void start(Node dest) {
      while ( !_heap->empty() && _heap->top()!=dest ) processNextNode();
      if ( !_heap->empty() ) finalizeNodeData(_heap->top(), _heap->prio());
    }

    /// \brief Executes the algorithm until a condition is met.
    ///
    /// Executes the algorithm until a condition is met.
    ///
    /// \pre init() must be called and at least one node should be added
    /// with addSource() before using this function.
    ///
    /// \param nm must be a bool (or convertible) node map. The algorithm
    /// will stop when it reaches a node \c v with <tt>nm[v]==true</tt>.
    template <typename NodeBoolMap>
    void start(const NodeBoolMap &nm) {
      while ( !_heap->empty() && !nm[_heap->top()] ) processNextNode();
      if ( !_heap->empty() ) finalizeNodeData(_heap->top(),_heap->prio());
    }

    /// \brief Runs the maximum cardinality search algorithm from node \c s.
    ///
    /// This method runs the %MaxCardinalitySearch algorithm from a root
    /// node \c s.
    ///
    ///\note d.run(s) is just a shortcut of the following code.
    ///\code
    ///  d.init();
    ///  d.addSource(s);
    ///  d.start();
    ///\endcode
    void run(Node s) {
      init();
      addSource(s);
      start();
    }

    /// \brief Runs the maximum cardinality search algorithm for the
    /// whole digraph.
    ///
    /// This method runs the %MaxCardinalitySearch algorithm from all
    /// unprocessed node of the digraph.
    ///
    ///\note d.run(s) is just a shortcut of the following code.
    ///\code
    ///  d.init();
    ///  for (NodeIt it(digraph); it != INVALID; ++it) {
    ///    if (!d.reached(it)) {
    ///      d.addSource(s);
    ///      d.start();
    ///    }
    ///  }
    ///\endcode
    void run() {
      init();
      for (NodeIt it(*_graph); it != INVALID; ++it) {
        if (!reached(it)) {
          addSource(it);
          start();
        }
      }
    }

    ///@}

    /// \name Query Functions
    /// The results of the maximum cardinality search algorithm can be
    /// obtained using these functions.
    /// \n
    /// Before the use of these functions, either run() or start() must be
    /// called.

    ///@{

    /// \brief The cardinality of a node.
    ///
    /// Returns the cardinality of a node.
    /// \pre \ref run() must be called before using this function.
    /// \warning If node \c v in unreachable from the root the return value
    /// of this funcion is undefined.
    Value cardinality(Node node) const { return (*_cardinality)[node]; }

    /// \brief The current cardinality of a node.
    ///
    /// Returns the current cardinality of a node.
    /// \pre the given node should be reached but not processed
    Value currentCardinality(Node node) const { return (*_heap)[node]; }

    /// \brief Returns a reference to the NodeMap of cardinalities.
    ///
    /// Returns a reference to the NodeMap of cardinalities. \pre \ref run()
    /// must be called before using this function.
    const CardinalityMap &cardinalityMap() const { return *_cardinality;}

    /// \brief Checks if a node is reachable from the root.
    ///
    /// Returns \c true if \c v is reachable from the root.
    /// \warning The source nodes are initated as unreached.
    /// \pre \ref run() must be called before using this function.
    bool reached(Node v) { return (*_heap_cross_ref)[v] != Heap::PRE_HEAP; }

    /// \brief Checks if a node is processed.
    ///
    /// Returns \c true if \c v is processed, i.e. the shortest
    /// path to \c v has already found.
    /// \pre \ref run() must be called before using this function.
    bool processed(Node v) { return (*_heap_cross_ref)[v] == Heap::POST_HEAP; }

    ///@}
  };

}

#endif
