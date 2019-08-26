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

#ifndef LEMON_MIN_COST_ARBORESCENCE_H
#define LEMON_MIN_COST_ARBORESCENCE_H

///\ingroup spantree
///\file
///\brief Minimum Cost Arborescence algorithm.

#include <vector>

#include <lemon/list_graph.h>
#include <lemon/bin_heap.h>
#include <lemon/assert.h>

namespace lemon {


  /// \brief Default traits class for MinCostArborescence class.
  ///
  /// Default traits class for MinCostArborescence class.
  /// \param GR Digraph type.
  /// \param CM Type of the cost map.
  template <class GR, class CM>
  struct MinCostArborescenceDefaultTraits{

    /// \brief The digraph type the algorithm runs on.
    typedef GR Digraph;

    /// \brief The type of the map that stores the arc costs.
    ///
    /// The type of the map that stores the arc costs.
    /// It must conform to the \ref concepts::ReadMap "ReadMap" concept.
    typedef CM CostMap;

    /// \brief The value type of the costs.
    ///
    /// The value type of the costs.
    typedef typename CostMap::Value Value;

    /// \brief The type of the map that stores which arcs are in the
    /// arborescence.
    ///
    /// The type of the map that stores which arcs are in the
    /// arborescence.  It must conform to the \ref concepts::WriteMap
    /// "WriteMap" concept, and its value type must be \c bool
    /// (or convertible). Initially it will be set to \c false on each
    /// arc, then it will be set on each arborescence arc once.
    typedef typename Digraph::template ArcMap<bool> ArborescenceMap;

    /// \brief Instantiates a \c ArborescenceMap.
    ///
    /// This function instantiates a \c ArborescenceMap.
    /// \param digraph The digraph to which we would like to calculate
    /// the \c ArborescenceMap.
    static ArborescenceMap *createArborescenceMap(const Digraph &digraph){
      return new ArborescenceMap(digraph);
    }

    /// \brief The type of the \c PredMap
    ///
    /// The type of the \c PredMap. It must confrom to the
    /// \ref concepts::WriteMap "WriteMap" concept, and its value type
    /// must be the \c Arc type of the digraph.
    typedef typename Digraph::template NodeMap<typename Digraph::Arc> PredMap;

    /// \brief Instantiates a \c PredMap.
    ///
    /// This function instantiates a \c PredMap.
    /// \param digraph The digraph to which we would like to define the
    /// \c PredMap.
    static PredMap *createPredMap(const Digraph &digraph){
      return new PredMap(digraph);
    }

  };

  /// \ingroup spantree
  ///
  /// \brief Minimum Cost Arborescence algorithm class.
  ///
  /// This class provides an efficient implementation of the
  /// Minimum Cost Arborescence algorithm. The arborescence is a tree
  /// which is directed from a given source node of the digraph. One or
  /// more sources should be given to the algorithm and it will calculate
  /// the minimum cost subgraph that is the union of arborescences with the
  /// given sources and spans all the nodes which are reachable from the
  /// sources. The time complexity of the algorithm is O(n<sup>2</sup>+m).
  ///
  /// The algorithm also provides an optimal dual solution, therefore
  /// the optimality of the solution can be checked.
  ///
  /// \param GR The digraph type the algorithm runs on.
  /// \param CM A read-only arc map storing the costs of the
  /// arcs. It is read once for each arc, so the map may involve in
  /// relatively time consuming process to compute the arc costs if
  /// it is necessary. The default map type is \ref
  /// concepts::Digraph::ArcMap "Digraph::ArcMap<int>".
  /// \tparam TR The traits class that defines various types used by the
  /// algorithm. By default, it is \ref MinCostArborescenceDefaultTraits
  /// "MinCostArborescenceDefaultTraits<GR, CM>".
  /// In most cases, this parameter should not be set directly,
  /// consider to use the named template parameters instead.
#ifndef DOXYGEN
  template <typename GR,
            typename CM = typename GR::template ArcMap<int>,
            typename TR =
              MinCostArborescenceDefaultTraits<GR, CM> >
#else
  template <typename GR, typename CM, typename TR>
#endif
  class MinCostArborescence {
  public:

    /// \brief The \ref lemon::MinCostArborescenceDefaultTraits "traits class"
    /// of the algorithm.
    typedef TR Traits;
    /// The type of the underlying digraph.
    typedef typename Traits::Digraph Digraph;
    /// The type of the map that stores the arc costs.
    typedef typename Traits::CostMap CostMap;
    ///The type of the costs of the arcs.
    typedef typename Traits::Value Value;
    ///The type of the predecessor map.
    typedef typename Traits::PredMap PredMap;
    ///The type of the map that stores which arcs are in the arborescence.
    typedef typename Traits::ArborescenceMap ArborescenceMap;

    typedef MinCostArborescence Create;

  private:

    TEMPLATE_DIGRAPH_TYPEDEFS(Digraph);

    struct CostArc {

      Arc arc;
      Value value;

      CostArc() {}
      CostArc(Arc _arc, Value _value) : arc(_arc), value(_value) {}

    };

    const Digraph *_digraph;
    const CostMap *_cost;

    PredMap *_pred;
    bool local_pred;

    ArborescenceMap *_arborescence;
    bool local_arborescence;

    typedef typename Digraph::template ArcMap<int> ArcOrder;
    ArcOrder *_arc_order;

    typedef typename Digraph::template NodeMap<int> NodeOrder;
    NodeOrder *_node_order;

    typedef typename Digraph::template NodeMap<CostArc> CostArcMap;
    CostArcMap *_cost_arcs;

    struct StackLevel {

      std::vector<CostArc> arcs;
      int node_level;

    };

    std::vector<StackLevel> level_stack;
    std::vector<Node> queue;

    typedef std::vector<typename Digraph::Node> DualNodeList;

    DualNodeList _dual_node_list;

    struct DualVariable {
      int begin, end;
      Value value;

      DualVariable(int _begin, int _end, Value _value)
        : begin(_begin), end(_end), value(_value) {}

    };

    typedef std::vector<DualVariable> DualVariables;

    DualVariables _dual_variables;

    typedef typename Digraph::template NodeMap<int> HeapCrossRef;

    HeapCrossRef *_heap_cross_ref;

    typedef BinHeap<int, HeapCrossRef> Heap;

    Heap *_heap;

  protected:

    MinCostArborescence() {}

  private:

    void createStructures() {
      if (!_pred) {
        local_pred = true;
        _pred = Traits::createPredMap(*_digraph);
      }
      if (!_arborescence) {
        local_arborescence = true;
        _arborescence = Traits::createArborescenceMap(*_digraph);
      }
      if (!_arc_order) {
        _arc_order = new ArcOrder(*_digraph);
      }
      if (!_node_order) {
        _node_order = new NodeOrder(*_digraph);
      }
      if (!_cost_arcs) {
        _cost_arcs = new CostArcMap(*_digraph);
      }
      if (!_heap_cross_ref) {
        _heap_cross_ref = new HeapCrossRef(*_digraph, -1);
      }
      if (!_heap) {
        _heap = new Heap(*_heap_cross_ref);
      }
    }

    void destroyStructures() {
      if (local_arborescence) {
        delete _arborescence;
      }
      if (local_pred) {
        delete _pred;
      }
      if (_arc_order) {
        delete _arc_order;
      }
      if (_node_order) {
        delete _node_order;
      }
      if (_cost_arcs) {
        delete _cost_arcs;
      }
      if (_heap) {
        delete _heap;
      }
      if (_heap_cross_ref) {
        delete _heap_cross_ref;
      }
    }

    Arc prepare(Node node) {
      std::vector<Node> nodes;
      (*_node_order)[node] = _dual_node_list.size();
      StackLevel level;
      level.node_level = _dual_node_list.size();
      _dual_node_list.push_back(node);
      for (InArcIt it(*_digraph, node); it != INVALID; ++it) {
        Arc arc = it;
        Node source = _digraph->source(arc);
        Value value = (*_cost)[it];
        if (source == node || (*_node_order)[source] == -3) continue;
        if ((*_cost_arcs)[source].arc == INVALID) {
          (*_cost_arcs)[source].arc = arc;
          (*_cost_arcs)[source].value = value;
          nodes.push_back(source);
        } else {
          if ((*_cost_arcs)[source].value > value) {
            (*_cost_arcs)[source].arc = arc;
            (*_cost_arcs)[source].value = value;
          }
        }
      }
      CostArc minimum = (*_cost_arcs)[nodes[0]];
      for (int i = 1; i < int(nodes.size()); ++i) {
        if ((*_cost_arcs)[nodes[i]].value < minimum.value) {
          minimum = (*_cost_arcs)[nodes[i]];
        }
      }
      (*_arc_order)[minimum.arc] = _dual_variables.size();
      DualVariable var(_dual_node_list.size() - 1,
                       _dual_node_list.size(), minimum.value);
      _dual_variables.push_back(var);
      for (int i = 0; i < int(nodes.size()); ++i) {
        (*_cost_arcs)[nodes[i]].value -= minimum.value;
        level.arcs.push_back((*_cost_arcs)[nodes[i]]);
        (*_cost_arcs)[nodes[i]].arc = INVALID;
      }
      level_stack.push_back(level);
      return minimum.arc;
    }

    Arc contract(Node node) {
      int node_bottom = bottom(node);
      std::vector<Node> nodes;
      while (!level_stack.empty() &&
             level_stack.back().node_level >= node_bottom) {
        for (int i = 0; i < int(level_stack.back().arcs.size()); ++i) {
          Arc arc = level_stack.back().arcs[i].arc;
          Node source = _digraph->source(arc);
          Value value = level_stack.back().arcs[i].value;
          if ((*_node_order)[source] >= node_bottom) continue;
          if ((*_cost_arcs)[source].arc == INVALID) {
            (*_cost_arcs)[source].arc = arc;
            (*_cost_arcs)[source].value = value;
            nodes.push_back(source);
          } else {
            if ((*_cost_arcs)[source].value > value) {
              (*_cost_arcs)[source].arc = arc;
              (*_cost_arcs)[source].value = value;
            }
          }
        }
        level_stack.pop_back();
      }
      CostArc minimum = (*_cost_arcs)[nodes[0]];
      for (int i = 1; i < int(nodes.size()); ++i) {
        if ((*_cost_arcs)[nodes[i]].value < minimum.value) {
          minimum = (*_cost_arcs)[nodes[i]];
        }
      }
      (*_arc_order)[minimum.arc] = _dual_variables.size();
      DualVariable var(node_bottom, _dual_node_list.size(), minimum.value);
      _dual_variables.push_back(var);
      StackLevel level;
      level.node_level = node_bottom;
      for (int i = 0; i < int(nodes.size()); ++i) {
        (*_cost_arcs)[nodes[i]].value -= minimum.value;
        level.arcs.push_back((*_cost_arcs)[nodes[i]]);
        (*_cost_arcs)[nodes[i]].arc = INVALID;
      }
      level_stack.push_back(level);
      return minimum.arc;
    }

    int bottom(Node node) {
      int k = level_stack.size() - 1;
      while (level_stack[k].node_level > (*_node_order)[node]) {
        --k;
      }
      return level_stack[k].node_level;
    }

    void finalize(Arc arc) {
      Node node = _digraph->target(arc);
      _heap->push(node, (*_arc_order)[arc]);
      _pred->set(node, arc);
      while (!_heap->empty()) {
        Node source = _heap->top();
        _heap->pop();
        (*_node_order)[source] = -1;
        for (OutArcIt it(*_digraph, source); it != INVALID; ++it) {
          if ((*_arc_order)[it] < 0) continue;
          Node target = _digraph->target(it);
          switch(_heap->state(target)) {
          case Heap::PRE_HEAP:
            _heap->push(target, (*_arc_order)[it]);
            _pred->set(target, it);
            break;
          case Heap::IN_HEAP:
            if ((*_arc_order)[it] < (*_heap)[target]) {
              _heap->decrease(target, (*_arc_order)[it]);
              _pred->set(target, it);
            }
            break;
          case Heap::POST_HEAP:
            break;
          }
        }
        _arborescence->set((*_pred)[source], true);
      }
    }


  public:

    /// \name Named Template Parameters

    /// @{

    template <class T>
    struct SetArborescenceMapTraits : public Traits {
      typedef T ArborescenceMap;
      static ArborescenceMap *createArborescenceMap(const Digraph &)
      {
        LEMON_ASSERT(false, "ArborescenceMap is not initialized");
        return 0; // ignore warnings
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for
    /// setting \c ArborescenceMap type
    ///
    /// \ref named-templ-param "Named parameter" for setting
    /// \c ArborescenceMap type.
    /// It must conform to the \ref concepts::WriteMap "WriteMap" concept,
    /// and its value type must be \c bool (or convertible).
    /// Initially it will be set to \c false on each arc,
    /// then it will be set on each arborescence arc once.
    template <class T>
    struct SetArborescenceMap
      : public MinCostArborescence<Digraph, CostMap,
                                   SetArborescenceMapTraits<T> > {
    };

    template <class T>
    struct SetPredMapTraits : public Traits {
      typedef T PredMap;
      static PredMap *createPredMap(const Digraph &)
      {
        LEMON_ASSERT(false, "PredMap is not initialized");
        return 0; // ignore warnings
      }
    };

    /// \brief \ref named-templ-param "Named parameter" for
    /// setting \c PredMap type
    ///
    /// \ref named-templ-param "Named parameter" for setting
    /// \c PredMap type.
    /// It must meet the \ref concepts::WriteMap "WriteMap" concept,
    /// and its value type must be the \c Arc type of the digraph.
    template <class T>
    struct SetPredMap
      : public MinCostArborescence<Digraph, CostMap, SetPredMapTraits<T> > {
    };

    /// @}

    /// \brief Constructor.
    ///
    /// \param digraph The digraph the algorithm will run on.
    /// \param cost The cost map used by the algorithm.
    MinCostArborescence(const Digraph& digraph, const CostMap& cost)
      : _digraph(&digraph), _cost(&cost), _pred(0), local_pred(false),
        _arborescence(0), local_arborescence(false),
        _arc_order(0), _node_order(0), _cost_arcs(0),
        _heap_cross_ref(0), _heap(0) {}

    /// \brief Destructor.
    ~MinCostArborescence() {
      destroyStructures();
    }

    /// \brief Sets the arborescence map.
    ///
    /// Sets the arborescence map.
    /// \return <tt>(*this)</tt>
    MinCostArborescence& arborescenceMap(ArborescenceMap& m) {
      if (local_arborescence) {
        delete _arborescence;
      }
      local_arborescence = false;
      _arborescence = &m;
      return *this;
    }

    /// \brief Sets the predecessor map.
    ///
    /// Sets the predecessor map.
    /// \return <tt>(*this)</tt>
    MinCostArborescence& predMap(PredMap& m) {
      if (local_pred) {
        delete _pred;
      }
      local_pred = false;
      _pred = &m;
      return *this;
    }

    /// \name Execution Control
    /// The simplest way to execute the algorithm is to use
    /// one of the member functions called \c run(...). \n
    /// If you need better control on the execution,
    /// you have to call \ref init() first, then you can add several
    /// source nodes with \ref addSource().
    /// Finally \ref start() will perform the arborescence
    /// computation.

    ///@{

    /// \brief Initializes the internal data structures.
    ///
    /// Initializes the internal data structures.
    ///
    void init() {
      createStructures();
      _heap->clear();
      for (NodeIt it(*_digraph); it != INVALID; ++it) {
        (*_cost_arcs)[it].arc = INVALID;
        (*_node_order)[it] = -3;
        (*_heap_cross_ref)[it] = Heap::PRE_HEAP;
        _pred->set(it, INVALID);
      }
      for (ArcIt it(*_digraph); it != INVALID; ++it) {
        _arborescence->set(it, false);
        (*_arc_order)[it] = -1;
      }
      _dual_node_list.clear();
      _dual_variables.clear();
    }

    /// \brief Adds a new source node.
    ///
    /// Adds a new source node to the algorithm.
    void addSource(Node source) {
      std::vector<Node> nodes;
      nodes.push_back(source);
      while (!nodes.empty()) {
        Node node = nodes.back();
        nodes.pop_back();
        for (OutArcIt it(*_digraph, node); it != INVALID; ++it) {
          Node target = _digraph->target(it);
          if ((*_node_order)[target] == -3) {
            (*_node_order)[target] = -2;
            nodes.push_back(target);
            queue.push_back(target);
          }
        }
      }
      (*_node_order)[source] = -1;
    }

    /// \brief Processes the next node in the priority queue.
    ///
    /// Processes the next node in the priority queue.
    ///
    /// \return The processed node.
    ///
    /// \warning The queue must not be empty.
    Node processNextNode() {
      Node node = queue.back();
      queue.pop_back();
      if ((*_node_order)[node] == -2) {
        Arc arc = prepare(node);
        Node source = _digraph->source(arc);
        while ((*_node_order)[source] != -1) {
          if ((*_node_order)[source] >= 0) {
            arc = contract(source);
          } else {
            arc = prepare(source);
          }
          source = _digraph->source(arc);
        }
        finalize(arc);
        level_stack.clear();
      }
      return node;
    }

    /// \brief Returns the number of the nodes to be processed.
    ///
    /// Returns the number of the nodes to be processed in the priority
    /// queue.
    int queueSize() const {
      return queue.size();
    }

    /// \brief Returns \c false if there are nodes to be processed.
    ///
    /// Returns \c false if there are nodes to be processed.
    bool emptyQueue() const {
      return queue.empty();
    }

    /// \brief Executes the algorithm.
    ///
    /// Executes the algorithm.
    ///
    /// \pre init() must be called and at least one node should be added
    /// with addSource() before using this function.
    ///
    ///\note mca.start() is just a shortcut of the following code.
    ///\code
    ///while (!mca.emptyQueue()) {
    ///  mca.processNextNode();
    ///}
    ///\endcode
    void start() {
      while (!emptyQueue()) {
        processNextNode();
      }
    }

    /// \brief Runs %MinCostArborescence algorithm from node \c s.
    ///
    /// This method runs the %MinCostArborescence algorithm from
    /// a root node \c s.
    ///
    /// \note mca.run(s) is just a shortcut of the following code.
    /// \code
    /// mca.init();
    /// mca.addSource(s);
    /// mca.start();
    /// \endcode
    void run(Node s) {
      init();
      addSource(s);
      start();
    }

    ///@}

    /// \name Query Functions
    /// The result of the %MinCostArborescence algorithm can be obtained
    /// using these functions.\n
    /// Either run() or start() must be called before using them.

    /// @{

    /// \brief Returns the cost of the arborescence.
    ///
    /// Returns the cost of the arborescence.
    Value arborescenceCost() const {
      Value sum = 0;
      for (ArcIt it(*_digraph); it != INVALID; ++it) {
        if (arborescence(it)) {
          sum += (*_cost)[it];
        }
      }
      return sum;
    }

    /// \brief Returns \c true if the arc is in the arborescence.
    ///
    /// Returns \c true if the given arc is in the arborescence.
    /// \param arc An arc of the digraph.
    /// \pre \ref run() must be called before using this function.
    bool arborescence(Arc arc) const {
      return (*_pred)[_digraph->target(arc)] == arc;
    }

    /// \brief Returns a const reference to the arborescence map.
    ///
    /// Returns a const reference to the arborescence map.
    /// \pre \ref run() must be called before using this function.
    const ArborescenceMap& arborescenceMap() const {
      return *_arborescence;
    }

    /// \brief Returns the predecessor arc of the given node.
    ///
    /// Returns the predecessor arc of the given node.
    /// \pre \ref run() must be called before using this function.
    Arc pred(Node node) const {
      return (*_pred)[node];
    }

    /// \brief Returns a const reference to the pred map.
    ///
    /// Returns a const reference to the pred map.
    /// \pre \ref run() must be called before using this function.
    const PredMap& predMap() const {
      return *_pred;
    }

    /// \brief Indicates that a node is reachable from the sources.
    ///
    /// Indicates that a node is reachable from the sources.
    bool reached(Node node) const {
      return (*_node_order)[node] != -3;
    }

    /// \brief Indicates that a node is processed.
    ///
    /// Indicates that a node is processed. The arborescence path exists
    /// from the source to the given node.
    bool processed(Node node) const {
      return (*_node_order)[node] == -1;
    }

    /// \brief Returns the number of the dual variables in basis.
    ///
    /// Returns the number of the dual variables in basis.
    int dualNum() const {
      return _dual_variables.size();
    }

    /// \brief Returns the value of the dual solution.
    ///
    /// Returns the value of the dual solution. It should be
    /// equal to the arborescence value.
    Value dualValue() const {
      Value sum = 0;
      for (int i = 0; i < int(_dual_variables.size()); ++i) {
        sum += _dual_variables[i].value;
      }
      return sum;
    }

    /// \brief Returns the number of the nodes in the dual variable.
    ///
    /// Returns the number of the nodes in the dual variable.
    int dualSize(int k) const {
      return _dual_variables[k].end - _dual_variables[k].begin;
    }

    /// \brief Returns the value of the dual variable.
    ///
    /// Returns the the value of the dual variable.
    Value dualValue(int k) const {
      return _dual_variables[k].value;
    }

    /// \brief LEMON iterator for getting a dual variable.
    ///
    /// This class provides a common style LEMON iterator for getting a
    /// dual variable of \ref MinCostArborescence algorithm.
    /// It iterates over a subset of the nodes.
    class DualIt {
    public:

      /// \brief Constructor.
      ///
      /// Constructor for getting the nodeset of the dual variable
      /// of \ref MinCostArborescence algorithm.
      DualIt(const MinCostArborescence& algorithm, int variable)
        : _algorithm(&algorithm)
      {
        _index = _algorithm->_dual_variables[variable].begin;
        _last = _algorithm->_dual_variables[variable].end;
      }

      /// \brief Conversion to \c Node.
      ///
      /// Conversion to \c Node.
      operator Node() const {
        return _algorithm->_dual_node_list[_index];
      }

      /// \brief Increment operator.
      ///
      /// Increment operator.
      DualIt& operator++() {
        ++_index;
        return *this;
      }

      /// \brief Validity checking
      ///
      /// Checks whether the iterator is invalid.
      bool operator==(Invalid) const {
        return _index == _last;
      }

      /// \brief Validity checking
      ///
      /// Checks whether the iterator is valid.
      bool operator!=(Invalid) const {
        return _index != _last;
      }

    private:
      const MinCostArborescence* _algorithm;
      int _index, _last;
    };

    /// @}

  };

  /// \ingroup spantree
  ///
  /// \brief Function type interface for MinCostArborescence algorithm.
  ///
  /// Function type interface for MinCostArborescence algorithm.
  /// \param digraph The digraph the algorithm runs on.
  /// \param cost An arc map storing the costs.
  /// \param source The source node of the arborescence.
  /// \retval arborescence An arc map with \c bool (or convertible) value
  /// type that stores the arborescence.
  /// \return The total cost of the arborescence.
  ///
  /// \sa MinCostArborescence
  template <typename Digraph, typename CostMap, typename ArborescenceMap>
  typename CostMap::Value minCostArborescence(const Digraph& digraph,
                                              const CostMap& cost,
                                              typename Digraph::Node source,
                                              ArborescenceMap& arborescence) {
    typename MinCostArborescence<Digraph, CostMap>
      ::template SetArborescenceMap<ArborescenceMap>
      ::Create mca(digraph, cost);
    mca.arborescenceMap(arborescence);
    mca.run(source);
    return mca.arborescenceCost();
  }

}

#endif
