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

#ifndef LEMON_MATCHING_H
#define LEMON_MATCHING_H

#include <vector>
#include <queue>
#include <set>
#include <limits>

#include <lemon/core.h>
#include <lemon/unionfind.h>
#include <lemon/bin_heap.h>
#include <lemon/maps.h>
#include <lemon/fractional_matching.h>

///\ingroup matching
///\file
///\brief Maximum matching algorithms in general graphs.

namespace lemon {

  /// \ingroup matching
  ///
  /// \brief Maximum cardinality matching in general graphs
  ///
  /// This class implements Edmonds' alternating forest matching algorithm
  /// for finding a maximum cardinality matching in a general undirected graph.
  /// It can be started from an arbitrary initial matching
  /// (the default is the empty one).
  ///
  /// The dual solution of the problem is a map of the nodes to
  /// \ref MaxMatching::Status "Status", having values \c EVEN (or \c D),
  /// \c ODD (or \c A) and \c MATCHED (or \c C) defining the Gallai-Edmonds
  /// decomposition of the graph. The nodes in \c EVEN/D induce a subgraph
  /// with factor-critical components, the nodes in \c ODD/A form the
  /// canonical barrier, and the nodes in \c MATCHED/C induce a graph having
  /// a perfect matching. The number of the factor-critical components
  /// minus the number of barrier nodes is a lower bound on the
  /// unmatched nodes, and the matching is optimal if and only if this bound is
  /// tight. This decomposition can be obtained using \ref status() or
  /// \ref statusMap() after running the algorithm.
  ///
  /// \tparam GR The undirected graph type the algorithm runs on.
  template <typename GR>
  class MaxMatching {
  public:

    /// The graph type of the algorithm
    typedef GR Graph;
    /// The type of the matching map
    typedef typename Graph::template NodeMap<typename Graph::Arc>
    MatchingMap;

    ///\brief Status constants for Gallai-Edmonds decomposition.
    ///
    ///These constants are used for indicating the Gallai-Edmonds
    ///decomposition of a graph. The nodes with status \c EVEN (or \c D)
    ///induce a subgraph with factor-critical components, the nodes with
    ///status \c ODD (or \c A) form the canonical barrier, and the nodes
    ///with status \c MATCHED (or \c C) induce a subgraph having a
    ///perfect matching.
    enum Status {
      EVEN = 1,       ///< = 1. (\c D is an alias for \c EVEN.)
      D = 1,
      MATCHED = 0,    ///< = 0. (\c C is an alias for \c MATCHED.)
      C = 0,
      ODD = -1,       ///< = -1. (\c A is an alias for \c ODD.)
      A = -1,
      UNMATCHED = -2  ///< = -2.
    };

    /// The type of the status map
    typedef typename Graph::template NodeMap<Status> StatusMap;

  private:

    TEMPLATE_GRAPH_TYPEDEFS(Graph);

    typedef UnionFindEnum<IntNodeMap> BlossomSet;
    typedef ExtendFindEnum<IntNodeMap> TreeSet;
    typedef RangeMap<Node> NodeIntMap;
    typedef MatchingMap EarMap;
    typedef std::vector<Node> NodeQueue;

    const Graph& _graph;
    MatchingMap* _matching;
    StatusMap* _status;

    EarMap* _ear;

    IntNodeMap* _blossom_set_index;
    BlossomSet* _blossom_set;
    NodeIntMap* _blossom_rep;

    IntNodeMap* _tree_set_index;
    TreeSet* _tree_set;

    NodeQueue _node_queue;
    int _process, _postpone, _last;

    int _node_num;

  private:

    void createStructures() {
      _node_num = countNodes(_graph);
      if (!_matching) {
        _matching = new MatchingMap(_graph);
      }
      if (!_status) {
        _status = new StatusMap(_graph);
      }
      if (!_ear) {
        _ear = new EarMap(_graph);
      }
      if (!_blossom_set) {
        _blossom_set_index = new IntNodeMap(_graph);
        _blossom_set = new BlossomSet(*_blossom_set_index);
      }
      if (!_blossom_rep) {
        _blossom_rep = new NodeIntMap(_node_num);
      }
      if (!_tree_set) {
        _tree_set_index = new IntNodeMap(_graph);
        _tree_set = new TreeSet(*_tree_set_index);
      }
      _node_queue.resize(_node_num);
    }

    void destroyStructures() {
      if (_matching) {
        delete _matching;
      }
      if (_status) {
        delete _status;
      }
      if (_ear) {
        delete _ear;
      }
      if (_blossom_set) {
        delete _blossom_set;
        delete _blossom_set_index;
      }
      if (_blossom_rep) {
        delete _blossom_rep;
      }
      if (_tree_set) {
        delete _tree_set_index;
        delete _tree_set;
      }
    }

    void processDense(const Node& n) {
      _process = _postpone = _last = 0;
      _node_queue[_last++] = n;

      while (_process != _last) {
        Node u = _node_queue[_process++];
        for (OutArcIt a(_graph, u); a != INVALID; ++a) {
          Node v = _graph.target(a);
          if ((*_status)[v] == MATCHED) {
            extendOnArc(a);
          } else if ((*_status)[v] == UNMATCHED) {
            augmentOnArc(a);
            return;
          }
        }
      }

      while (_postpone != _last) {
        Node u = _node_queue[_postpone++];

        for (OutArcIt a(_graph, u); a != INVALID ; ++a) {
          Node v = _graph.target(a);

          if ((*_status)[v] == EVEN) {
            if (_blossom_set->find(u) != _blossom_set->find(v)) {
              shrinkOnEdge(a);
            }
          }

          while (_process != _last) {
            Node w = _node_queue[_process++];
            for (OutArcIt b(_graph, w); b != INVALID; ++b) {
              Node x = _graph.target(b);
              if ((*_status)[x] == MATCHED) {
                extendOnArc(b);
              } else if ((*_status)[x] == UNMATCHED) {
                augmentOnArc(b);
                return;
              }
            }
          }
        }
      }
    }

    void processSparse(const Node& n) {
      _process = _last = 0;
      _node_queue[_last++] = n;
      while (_process != _last) {
        Node u = _node_queue[_process++];
        for (OutArcIt a(_graph, u); a != INVALID; ++a) {
          Node v = _graph.target(a);

          if ((*_status)[v] == EVEN) {
            if (_blossom_set->find(u) != _blossom_set->find(v)) {
              shrinkOnEdge(a);
            }
          } else if ((*_status)[v] == MATCHED) {
            extendOnArc(a);
          } else if ((*_status)[v] == UNMATCHED) {
            augmentOnArc(a);
            return;
          }
        }
      }
    }

    void shrinkOnEdge(const Edge& e) {
      Node nca = INVALID;

      {
        std::set<Node> left_set, right_set;

        Node left = (*_blossom_rep)[_blossom_set->find(_graph.u(e))];
        left_set.insert(left);

        Node right = (*_blossom_rep)[_blossom_set->find(_graph.v(e))];
        right_set.insert(right);

        while (true) {
          if ((*_matching)[left] == INVALID) break;
          left = _graph.target((*_matching)[left]);
          left = (*_blossom_rep)[_blossom_set->
                                 find(_graph.target((*_ear)[left]))];
          if (right_set.find(left) != right_set.end()) {
            nca = left;
            break;
          }
          left_set.insert(left);

          if ((*_matching)[right] == INVALID) break;
          right = _graph.target((*_matching)[right]);
          right = (*_blossom_rep)[_blossom_set->
                                  find(_graph.target((*_ear)[right]))];
          if (left_set.find(right) != left_set.end()) {
            nca = right;
            break;
          }
          right_set.insert(right);
        }

        if (nca == INVALID) {
          if ((*_matching)[left] == INVALID) {
            nca = right;
            while (left_set.find(nca) == left_set.end()) {
              nca = _graph.target((*_matching)[nca]);
              nca =(*_blossom_rep)[_blossom_set->
                                   find(_graph.target((*_ear)[nca]))];
            }
          } else {
            nca = left;
            while (right_set.find(nca) == right_set.end()) {
              nca = _graph.target((*_matching)[nca]);
              nca = (*_blossom_rep)[_blossom_set->
                                   find(_graph.target((*_ear)[nca]))];
            }
          }
        }
      }

      {

        Node node = _graph.u(e);
        Arc arc = _graph.direct(e, true);
        Node base = (*_blossom_rep)[_blossom_set->find(node)];

        while (base != nca) {
          (*_ear)[node] = arc;

          Node n = node;
          while (n != base) {
            n = _graph.target((*_matching)[n]);
            Arc a = (*_ear)[n];
            n = _graph.target(a);
            (*_ear)[n] = _graph.oppositeArc(a);
          }
          node = _graph.target((*_matching)[base]);
          _tree_set->erase(base);
          _tree_set->erase(node);
          _blossom_set->insert(node, _blossom_set->find(base));
          (*_status)[node] = EVEN;
          _node_queue[_last++] = node;
          arc = _graph.oppositeArc((*_ear)[node]);
          node = _graph.target((*_ear)[node]);
          base = (*_blossom_rep)[_blossom_set->find(node)];
          _blossom_set->join(_graph.target(arc), base);
        }
      }

      (*_blossom_rep)[_blossom_set->find(nca)] = nca;

      {

        Node node = _graph.v(e);
        Arc arc = _graph.direct(e, false);
        Node base = (*_blossom_rep)[_blossom_set->find(node)];

        while (base != nca) {
          (*_ear)[node] = arc;

          Node n = node;
          while (n != base) {
            n = _graph.target((*_matching)[n]);
            Arc a = (*_ear)[n];
            n = _graph.target(a);
            (*_ear)[n] = _graph.oppositeArc(a);
          }
          node = _graph.target((*_matching)[base]);
          _tree_set->erase(base);
          _tree_set->erase(node);
          _blossom_set->insert(node, _blossom_set->find(base));
          (*_status)[node] = EVEN;
          _node_queue[_last++] = node;
          arc = _graph.oppositeArc((*_ear)[node]);
          node = _graph.target((*_ear)[node]);
          base = (*_blossom_rep)[_blossom_set->find(node)];
          _blossom_set->join(_graph.target(arc), base);
        }
      }

      (*_blossom_rep)[_blossom_set->find(nca)] = nca;
    }

    void extendOnArc(const Arc& a) {
      Node base = _graph.source(a);
      Node odd = _graph.target(a);

      (*_ear)[odd] = _graph.oppositeArc(a);
      Node even = _graph.target((*_matching)[odd]);
      (*_blossom_rep)[_blossom_set->insert(even)] = even;
      (*_status)[odd] = ODD;
      (*_status)[even] = EVEN;
      int tree = _tree_set->find((*_blossom_rep)[_blossom_set->find(base)]);
      _tree_set->insert(odd, tree);
      _tree_set->insert(even, tree);
      _node_queue[_last++] = even;

    }

    void augmentOnArc(const Arc& a) {
      Node even = _graph.source(a);
      Node odd = _graph.target(a);

      int tree = _tree_set->find((*_blossom_rep)[_blossom_set->find(even)]);

      (*_matching)[odd] = _graph.oppositeArc(a);
      (*_status)[odd] = MATCHED;

      Arc arc = (*_matching)[even];
      (*_matching)[even] = a;

      while (arc != INVALID) {
        odd = _graph.target(arc);
        arc = (*_ear)[odd];
        even = _graph.target(arc);
        (*_matching)[odd] = arc;
        arc = (*_matching)[even];
        (*_matching)[even] = _graph.oppositeArc((*_matching)[odd]);
      }

      for (typename TreeSet::ItemIt it(*_tree_set, tree);
           it != INVALID; ++it) {
        if ((*_status)[it] == ODD) {
          (*_status)[it] = MATCHED;
        } else {
          int blossom = _blossom_set->find(it);
          for (typename BlossomSet::ItemIt jt(*_blossom_set, blossom);
               jt != INVALID; ++jt) {
            (*_status)[jt] = MATCHED;
          }
          _blossom_set->eraseClass(blossom);
        }
      }
      _tree_set->eraseClass(tree);

    }

  public:

    /// \brief Constructor
    ///
    /// Constructor.
    MaxMatching(const Graph& graph)
      : _graph(graph), _matching(0), _status(0), _ear(0),
        _blossom_set_index(0), _blossom_set(0), _blossom_rep(0),
        _tree_set_index(0), _tree_set(0) {}

    ~MaxMatching() {
      destroyStructures();
    }

    /// \name Execution Control
    /// The simplest way to execute the algorithm is to use the
    /// \c run() member function.\n
    /// If you need better control on the execution, you have to call
    /// one of the functions \ref init(), \ref greedyInit() or
    /// \ref matchingInit() first, then you can start the algorithm with
    /// \ref startSparse() or \ref startDense().

    ///@{

    /// \brief Set the initial matching to the empty matching.
    ///
    /// This function sets the initial matching to the empty matching.
    void init() {
      createStructures();
      for(NodeIt n(_graph); n != INVALID; ++n) {
        (*_matching)[n] = INVALID;
        (*_status)[n] = UNMATCHED;
      }
    }

    /// \brief Find an initial matching in a greedy way.
    ///
    /// This function finds an initial matching in a greedy way.
    void greedyInit() {
      createStructures();
      for (NodeIt n(_graph); n != INVALID; ++n) {
        (*_matching)[n] = INVALID;
        (*_status)[n] = UNMATCHED;
      }
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_matching)[n] == INVALID) {
          for (OutArcIt a(_graph, n); a != INVALID ; ++a) {
            Node v = _graph.target(a);
            if ((*_matching)[v] == INVALID && v != n) {
              (*_matching)[n] = a;
              (*_status)[n] = MATCHED;
              (*_matching)[v] = _graph.oppositeArc(a);
              (*_status)[v] = MATCHED;
              break;
            }
          }
        }
      }
    }


    /// \brief Initialize the matching from a map.
    ///
    /// This function initializes the matching from a \c bool valued edge
    /// map. This map should have the property that there are no two incident
    /// edges with \c true value, i.e. it really contains a matching.
    /// \return \c true if the map contains a matching.
    template <typename MatchingMap>
    bool matchingInit(const MatchingMap& matching) {
      createStructures();

      for (NodeIt n(_graph); n != INVALID; ++n) {
        (*_matching)[n] = INVALID;
        (*_status)[n] = UNMATCHED;
      }
      for(EdgeIt e(_graph); e!=INVALID; ++e) {
        if (matching[e]) {

          Node u = _graph.u(e);
          if ((*_matching)[u] != INVALID) return false;
          (*_matching)[u] = _graph.direct(e, true);
          (*_status)[u] = MATCHED;

          Node v = _graph.v(e);
          if ((*_matching)[v] != INVALID) return false;
          (*_matching)[v] = _graph.direct(e, false);
          (*_status)[v] = MATCHED;
        }
      }
      return true;
    }

    /// \brief Start Edmonds' algorithm
    ///
    /// This function runs the original Edmonds' algorithm.
    ///
    /// \pre \ref init(), \ref greedyInit() or \ref matchingInit() must be
    /// called before using this function.
    void startSparse() {
      for(NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_status)[n] == UNMATCHED) {
          (*_blossom_rep)[_blossom_set->insert(n)] = n;
          _tree_set->insert(n);
          (*_status)[n] = EVEN;
          processSparse(n);
        }
      }
    }

    /// \brief Start Edmonds' algorithm with a heuristic improvement
    /// for dense graphs
    ///
    /// This function runs Edmonds' algorithm with a heuristic of postponing
    /// shrinks, therefore resulting in a faster algorithm for dense graphs.
    ///
    /// \pre \ref init(), \ref greedyInit() or \ref matchingInit() must be
    /// called before using this function.
    void startDense() {
      for(NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_status)[n] == UNMATCHED) {
          (*_blossom_rep)[_blossom_set->insert(n)] = n;
          _tree_set->insert(n);
          (*_status)[n] = EVEN;
          processDense(n);
        }
      }
    }


    /// \brief Run Edmonds' algorithm
    ///
    /// This function runs Edmonds' algorithm. An additional heuristic of
    /// postponing shrinks is used for relatively dense graphs
    /// (for which <tt>m>=2*n</tt> holds).
    void run() {
      if (countEdges(_graph) < 2 * countNodes(_graph)) {
        greedyInit();
        startSparse();
      } else {
        init();
        startDense();
      }
    }

    /// @}

    /// \name Primal Solution
    /// Functions to get the primal solution, i.e. the maximum matching.

    /// @{

    /// \brief Return the size (cardinality) of the matching.
    ///
    /// This function returns the size (cardinality) of the current matching.
    /// After run() it returns the size of the maximum matching in the graph.
    int matchingSize() const {
      int size = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_matching)[n] != INVALID) {
          ++size;
        }
      }
      return size / 2;
    }

    /// \brief Return \c true if the given edge is in the matching.
    ///
    /// This function returns \c true if the given edge is in the current
    /// matching.
    bool matching(const Edge& edge) const {
      return edge == (*_matching)[_graph.u(edge)];
    }

    /// \brief Return the matching arc (or edge) incident to the given node.
    ///
    /// This function returns the matching arc (or edge) incident to the
    /// given node in the current matching or \c INVALID if the node is
    /// not covered by the matching.
    Arc matching(const Node& n) const {
      return (*_matching)[n];
    }

    /// \brief Return a const reference to the matching map.
    ///
    /// This function returns a const reference to a node map that stores
    /// the matching arc (or edge) incident to each node.
    const MatchingMap& matchingMap() const {
      return *_matching;
    }

    /// \brief Return the mate of the given node.
    ///
    /// This function returns the mate of the given node in the current
    /// matching or \c INVALID if the node is not covered by the matching.
    Node mate(const Node& n) const {
      return (*_matching)[n] != INVALID ?
        _graph.target((*_matching)[n]) : INVALID;
    }

    /// @}

    /// \name Dual Solution
    /// Functions to get the dual solution, i.e. the Gallai-Edmonds
    /// decomposition.

    /// @{

    /// \brief Return the status of the given node in the Edmonds-Gallai
    /// decomposition.
    ///
    /// This function returns the \ref Status "status" of the given node
    /// in the Edmonds-Gallai decomposition.
    Status status(const Node& n) const {
      return (*_status)[n];
    }

    /// \brief Return a const reference to the status map, which stores
    /// the Edmonds-Gallai decomposition.
    ///
    /// This function returns a const reference to a node map that stores the
    /// \ref Status "status" of each node in the Edmonds-Gallai decomposition.
    const StatusMap& statusMap() const {
      return *_status;
    }

    /// \brief Return \c true if the given node is in the barrier.
    ///
    /// This function returns \c true if the given node is in the barrier.
    bool barrier(const Node& n) const {
      return (*_status)[n] == ODD;
    }

    /// @}

  };

  /// \ingroup matching
  ///
  /// \brief Weighted matching in general graphs
  ///
  /// This class provides an efficient implementation of Edmond's
  /// maximum weighted matching algorithm. The implementation is based
  /// on extensive use of priority queues and provides
  /// \f$O(nm\log n)\f$ time complexity.
  ///
  /// The maximum weighted matching problem is to find a subset of the
  /// edges in an undirected graph with maximum overall weight for which
  /// each node has at most one incident edge.
  /// It can be formulated with the following linear program.
  /// \f[ \sum_{e \in \delta(u)}x_e \le 1 \quad \forall u\in V\f]
  /** \f[ \sum_{e \in \gamma(B)}x_e \le \frac{\vert B \vert - 1}{2}
      \quad \forall B\in\mathcal{O}\f] */
  /// \f[x_e \ge 0\quad \forall e\in E\f]
  /// \f[\max \sum_{e\in E}x_ew_e\f]
  /// where \f$\delta(X)\f$ is the set of edges incident to a node in
  /// \f$X\f$, \f$\gamma(X)\f$ is the set of edges with both ends in
  /// \f$X\f$ and \f$\mathcal{O}\f$ is the set of odd cardinality
  /// subsets of the nodes.
  ///
  /// The algorithm calculates an optimal matching and a proof of the
  /// optimality. The solution of the dual problem can be used to check
  /// the result of the algorithm. The dual linear problem is the
  /// following.
  /** \f[ y_u + y_v + \sum_{B \in \mathcal{O}, uv \in \gamma(B)}
      z_B \ge w_{uv} \quad \forall uv\in E\f] */
  /// \f[y_u \ge 0 \quad \forall u \in V\f]
  /// \f[z_B \ge 0 \quad \forall B \in \mathcal{O}\f]
  /** \f[\min \sum_{u \in V}y_u + \sum_{B \in \mathcal{O}}
      \frac{\vert B \vert - 1}{2}z_B\f] */
  ///
  /// The algorithm can be executed with the run() function.
  /// After it the matching (the primal solution) and the dual solution
  /// can be obtained using the query functions and the
  /// \ref MaxWeightedMatching::BlossomIt "BlossomIt" nested class,
  /// which is able to iterate on the nodes of a blossom.
  /// If the value type is integer, then the dual solution is multiplied
  /// by \ref MaxWeightedMatching::dualScale "4".
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
  class MaxWeightedMatching {
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

    /// \brief Scaling factor for dual solution
    ///
    /// Scaling factor for dual solution. It is equal to 4 or 1
    /// according to the value type.
    static const int dualScale =
      std::numeric_limits<Value>::is_integer ? 4 : 1;

  private:

    TEMPLATE_GRAPH_TYPEDEFS(Graph);

    typedef typename Graph::template NodeMap<Value> NodePotential;
    typedef std::vector<Node> BlossomNodeList;

    struct BlossomVariable {
      int begin, end;
      Value value;

      BlossomVariable(int _begin, int _end, Value _value)
        : begin(_begin), end(_end), value(_value) {}

    };

    typedef std::vector<BlossomVariable> BlossomPotential;

    const Graph& _graph;
    const WeightMap& _weight;

    MatchingMap* _matching;

    NodePotential* _node_potential;

    BlossomPotential _blossom_potential;
    BlossomNodeList _blossom_node_list;

    int _node_num;
    int _blossom_num;

    typedef RangeMap<int> IntIntMap;

    enum Status {
      EVEN = -1, MATCHED = 0, ODD = 1
    };

    typedef HeapUnionFind<Value, IntNodeMap> BlossomSet;
    struct BlossomData {
      int tree;
      Status status;
      Arc pred, next;
      Value pot, offset;
      Node base;
    };

    IntNodeMap *_blossom_index;
    BlossomSet *_blossom_set;
    RangeMap<BlossomData>* _blossom_data;

    IntNodeMap *_node_index;
    IntArcMap *_node_heap_index;

    struct NodeData {

      NodeData(IntArcMap& node_heap_index)
        : heap(node_heap_index) {}

      int blossom;
      Value pot;
      BinHeap<Value, IntArcMap> heap;
      std::map<int, Arc> heap_index;

      int tree;
    };

    RangeMap<NodeData>* _node_data;

    typedef ExtendFindEnum<IntIntMap> TreeSet;

    IntIntMap *_tree_set_index;
    TreeSet *_tree_set;

    IntNodeMap *_delta1_index;
    BinHeap<Value, IntNodeMap> *_delta1;

    IntIntMap *_delta2_index;
    BinHeap<Value, IntIntMap> *_delta2;

    IntEdgeMap *_delta3_index;
    BinHeap<Value, IntEdgeMap> *_delta3;

    IntIntMap *_delta4_index;
    BinHeap<Value, IntIntMap> *_delta4;

    Value _delta_sum;
    int _unmatched;

    typedef MaxWeightedFractionalMatching<Graph, WeightMap> FractionalMatching;
    FractionalMatching *_fractional;

    void createStructures() {
      _node_num = countNodes(_graph);
      _blossom_num = _node_num * 3 / 2;

      if (!_matching) {
        _matching = new MatchingMap(_graph);
      }

      if (!_node_potential) {
        _node_potential = new NodePotential(_graph);
      }

      if (!_blossom_set) {
        _blossom_index = new IntNodeMap(_graph);
        _blossom_set = new BlossomSet(*_blossom_index);
        _blossom_data = new RangeMap<BlossomData>(_blossom_num);
      } else if (_blossom_data->size() != _blossom_num) {
        delete _blossom_data;
        _blossom_data = new RangeMap<BlossomData>(_blossom_num);
      }

      if (!_node_index) {
        _node_index = new IntNodeMap(_graph);
        _node_heap_index = new IntArcMap(_graph);
        _node_data = new RangeMap<NodeData>(_node_num,
                                            NodeData(*_node_heap_index));
      } else {
        delete _node_data;
        _node_data = new RangeMap<NodeData>(_node_num,
                                            NodeData(*_node_heap_index));
      }

      if (!_tree_set) {
        _tree_set_index = new IntIntMap(_blossom_num);
        _tree_set = new TreeSet(*_tree_set_index);
      } else {
        _tree_set_index->resize(_blossom_num);
      }

      if (!_delta1) {
        _delta1_index = new IntNodeMap(_graph);
        _delta1 = new BinHeap<Value, IntNodeMap>(*_delta1_index);
      }

      if (!_delta2) {
        _delta2_index = new IntIntMap(_blossom_num);
        _delta2 = new BinHeap<Value, IntIntMap>(*_delta2_index);
      } else {
        _delta2_index->resize(_blossom_num);
      }

      if (!_delta3) {
        _delta3_index = new IntEdgeMap(_graph);
        _delta3 = new BinHeap<Value, IntEdgeMap>(*_delta3_index);
      }

      if (!_delta4) {
        _delta4_index = new IntIntMap(_blossom_num);
        _delta4 = new BinHeap<Value, IntIntMap>(*_delta4_index);
      } else {
        _delta4_index->resize(_blossom_num);
      }
    }

    void destroyStructures() {
      if (_matching) {
        delete _matching;
      }
      if (_node_potential) {
        delete _node_potential;
      }
      if (_blossom_set) {
        delete _blossom_index;
        delete _blossom_set;
        delete _blossom_data;
      }

      if (_node_index) {
        delete _node_index;
        delete _node_heap_index;
        delete _node_data;
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
      if (_delta4) {
        delete _delta4_index;
        delete _delta4;
      }
    }

    void matchedToEven(int blossom, int tree) {
      if (_delta2->state(blossom) == _delta2->IN_HEAP) {
        _delta2->erase(blossom);
      }

      if (!_blossom_set->trivial(blossom)) {
        (*_blossom_data)[blossom].pot -=
          2 * (_delta_sum - (*_blossom_data)[blossom].offset);
      }

      for (typename BlossomSet::ItemIt n(*_blossom_set, blossom);
           n != INVALID; ++n) {

        _blossom_set->increase(n, std::numeric_limits<Value>::max());
        int ni = (*_node_index)[n];

        (*_node_data)[ni].heap.clear();
        (*_node_data)[ni].heap_index.clear();

        (*_node_data)[ni].pot += _delta_sum - (*_blossom_data)[blossom].offset;

        _delta1->push(n, (*_node_data)[ni].pot);

        for (InArcIt e(_graph, n); e != INVALID; ++e) {
          Node v = _graph.source(e);
          int vb = _blossom_set->find(v);
          int vi = (*_node_index)[v];

          Value rw = (*_node_data)[ni].pot + (*_node_data)[vi].pot -
            dualScale * _weight[e];

          if ((*_blossom_data)[vb].status == EVEN) {
            if (_delta3->state(e) != _delta3->IN_HEAP && blossom != vb) {
              _delta3->push(e, rw / 2);
            }
          } else {
            typename std::map<int, Arc>::iterator it =
              (*_node_data)[vi].heap_index.find(tree);

            if (it != (*_node_data)[vi].heap_index.end()) {
              if ((*_node_data)[vi].heap[it->second] > rw) {
                (*_node_data)[vi].heap.replace(it->second, e);
                (*_node_data)[vi].heap.decrease(e, rw);
                it->second = e;
              }
            } else {
              (*_node_data)[vi].heap.push(e, rw);
              (*_node_data)[vi].heap_index.insert(std::make_pair(tree, e));
            }

            if ((*_blossom_set)[v] > (*_node_data)[vi].heap.prio()) {
              _blossom_set->decrease(v, (*_node_data)[vi].heap.prio());

              if ((*_blossom_data)[vb].status == MATCHED) {
                if (_delta2->state(vb) != _delta2->IN_HEAP) {
                  _delta2->push(vb, _blossom_set->classPrio(vb) -
                               (*_blossom_data)[vb].offset);
                } else if ((*_delta2)[vb] > _blossom_set->classPrio(vb) -
                           (*_blossom_data)[vb].offset) {
                  _delta2->decrease(vb, _blossom_set->classPrio(vb) -
                                   (*_blossom_data)[vb].offset);
                }
              }
            }
          }
        }
      }
      (*_blossom_data)[blossom].offset = 0;
    }

    void matchedToOdd(int blossom) {
      if (_delta2->state(blossom) == _delta2->IN_HEAP) {
        _delta2->erase(blossom);
      }
      (*_blossom_data)[blossom].offset += _delta_sum;
      if (!_blossom_set->trivial(blossom)) {
        _delta4->push(blossom, (*_blossom_data)[blossom].pot / 2 +
                      (*_blossom_data)[blossom].offset);
      }
    }

    void evenToMatched(int blossom, int tree) {
      if (!_blossom_set->trivial(blossom)) {
        (*_blossom_data)[blossom].pot += 2 * _delta_sum;
      }

      for (typename BlossomSet::ItemIt n(*_blossom_set, blossom);
           n != INVALID; ++n) {
        int ni = (*_node_index)[n];
        (*_node_data)[ni].pot -= _delta_sum;

        _delta1->erase(n);

        for (InArcIt e(_graph, n); e != INVALID; ++e) {
          Node v = _graph.source(e);
          int vb = _blossom_set->find(v);
          int vi = (*_node_index)[v];

          Value rw = (*_node_data)[ni].pot + (*_node_data)[vi].pot -
            dualScale * _weight[e];

          if (vb == blossom) {
            if (_delta3->state(e) == _delta3->IN_HEAP) {
              _delta3->erase(e);
            }
          } else if ((*_blossom_data)[vb].status == EVEN) {

            if (_delta3->state(e) == _delta3->IN_HEAP) {
              _delta3->erase(e);
            }

            int vt = _tree_set->find(vb);

            if (vt != tree) {

              Arc r = _graph.oppositeArc(e);

              typename std::map<int, Arc>::iterator it =
                (*_node_data)[ni].heap_index.find(vt);

              if (it != (*_node_data)[ni].heap_index.end()) {
                if ((*_node_data)[ni].heap[it->second] > rw) {
                  (*_node_data)[ni].heap.replace(it->second, r);
                  (*_node_data)[ni].heap.decrease(r, rw);
                  it->second = r;
                }
              } else {
                (*_node_data)[ni].heap.push(r, rw);
                (*_node_data)[ni].heap_index.insert(std::make_pair(vt, r));
              }

              if ((*_blossom_set)[n] > (*_node_data)[ni].heap.prio()) {
                _blossom_set->decrease(n, (*_node_data)[ni].heap.prio());

                if (_delta2->state(blossom) != _delta2->IN_HEAP) {
                  _delta2->push(blossom, _blossom_set->classPrio(blossom) -
                               (*_blossom_data)[blossom].offset);
                } else if ((*_delta2)[blossom] >
                           _blossom_set->classPrio(blossom) -
                           (*_blossom_data)[blossom].offset){
                  _delta2->decrease(blossom, _blossom_set->classPrio(blossom) -
                                   (*_blossom_data)[blossom].offset);
                }
              }
            }
          } else {

            typename std::map<int, Arc>::iterator it =
              (*_node_data)[vi].heap_index.find(tree);

            if (it != (*_node_data)[vi].heap_index.end()) {
              (*_node_data)[vi].heap.erase(it->second);
              (*_node_data)[vi].heap_index.erase(it);
              if ((*_node_data)[vi].heap.empty()) {
                _blossom_set->increase(v, std::numeric_limits<Value>::max());
              } else if ((*_blossom_set)[v] < (*_node_data)[vi].heap.prio()) {
                _blossom_set->increase(v, (*_node_data)[vi].heap.prio());
              }

              if ((*_blossom_data)[vb].status == MATCHED) {
                if (_blossom_set->classPrio(vb) ==
                    std::numeric_limits<Value>::max()) {
                  _delta2->erase(vb);
                } else if ((*_delta2)[vb] < _blossom_set->classPrio(vb) -
                           (*_blossom_data)[vb].offset) {
                  _delta2->increase(vb, _blossom_set->classPrio(vb) -
                                   (*_blossom_data)[vb].offset);
                }
              }
            }
          }
        }
      }
    }

    void oddToMatched(int blossom) {
      (*_blossom_data)[blossom].offset -= _delta_sum;

      if (_blossom_set->classPrio(blossom) !=
          std::numeric_limits<Value>::max()) {
        _delta2->push(blossom, _blossom_set->classPrio(blossom) -
                      (*_blossom_data)[blossom].offset);
      }

      if (!_blossom_set->trivial(blossom)) {
        _delta4->erase(blossom);
      }
    }

    void oddToEven(int blossom, int tree) {
      if (!_blossom_set->trivial(blossom)) {
        _delta4->erase(blossom);
        (*_blossom_data)[blossom].pot -=
          2 * (2 * _delta_sum - (*_blossom_data)[blossom].offset);
      }

      for (typename BlossomSet::ItemIt n(*_blossom_set, blossom);
           n != INVALID; ++n) {
        int ni = (*_node_index)[n];

        _blossom_set->increase(n, std::numeric_limits<Value>::max());

        (*_node_data)[ni].heap.clear();
        (*_node_data)[ni].heap_index.clear();
        (*_node_data)[ni].pot +=
          2 * _delta_sum - (*_blossom_data)[blossom].offset;

        _delta1->push(n, (*_node_data)[ni].pot);

        for (InArcIt e(_graph, n); e != INVALID; ++e) {
          Node v = _graph.source(e);
          int vb = _blossom_set->find(v);
          int vi = (*_node_index)[v];

          Value rw = (*_node_data)[ni].pot + (*_node_data)[vi].pot -
            dualScale * _weight[e];

          if ((*_blossom_data)[vb].status == EVEN) {
            if (_delta3->state(e) != _delta3->IN_HEAP && blossom != vb) {
              _delta3->push(e, rw / 2);
            }
          } else {

            typename std::map<int, Arc>::iterator it =
              (*_node_data)[vi].heap_index.find(tree);

            if (it != (*_node_data)[vi].heap_index.end()) {
              if ((*_node_data)[vi].heap[it->second] > rw) {
                (*_node_data)[vi].heap.replace(it->second, e);
                (*_node_data)[vi].heap.decrease(e, rw);
                it->second = e;
              }
            } else {
              (*_node_data)[vi].heap.push(e, rw);
              (*_node_data)[vi].heap_index.insert(std::make_pair(tree, e));
            }

            if ((*_blossom_set)[v] > (*_node_data)[vi].heap.prio()) {
              _blossom_set->decrease(v, (*_node_data)[vi].heap.prio());

              if ((*_blossom_data)[vb].status == MATCHED) {
                if (_delta2->state(vb) != _delta2->IN_HEAP) {
                  _delta2->push(vb, _blossom_set->classPrio(vb) -
                               (*_blossom_data)[vb].offset);
                } else if ((*_delta2)[vb] > _blossom_set->classPrio(vb) -
                           (*_blossom_data)[vb].offset) {
                  _delta2->decrease(vb, _blossom_set->classPrio(vb) -
                                   (*_blossom_data)[vb].offset);
                }
              }
            }
          }
        }
      }
      (*_blossom_data)[blossom].offset = 0;
    }

    void alternatePath(int even, int tree) {
      int odd;

      evenToMatched(even, tree);
      (*_blossom_data)[even].status = MATCHED;

      while ((*_blossom_data)[even].pred != INVALID) {
        odd = _blossom_set->find(_graph.target((*_blossom_data)[even].pred));
        (*_blossom_data)[odd].status = MATCHED;
        oddToMatched(odd);
        (*_blossom_data)[odd].next = (*_blossom_data)[odd].pred;

        even = _blossom_set->find(_graph.target((*_blossom_data)[odd].pred));
        (*_blossom_data)[even].status = MATCHED;
        evenToMatched(even, tree);
        (*_blossom_data)[even].next =
          _graph.oppositeArc((*_blossom_data)[odd].pred);
      }

    }

    void destroyTree(int tree) {
      for (TreeSet::ItemIt b(*_tree_set, tree); b != INVALID; ++b) {
        if ((*_blossom_data)[b].status == EVEN) {
          (*_blossom_data)[b].status = MATCHED;
          evenToMatched(b, tree);
        } else if ((*_blossom_data)[b].status == ODD) {
          (*_blossom_data)[b].status = MATCHED;
          oddToMatched(b);
        }
      }
      _tree_set->eraseClass(tree);
    }


    void unmatchNode(const Node& node) {
      int blossom = _blossom_set->find(node);
      int tree = _tree_set->find(blossom);

      alternatePath(blossom, tree);
      destroyTree(tree);

      (*_blossom_data)[blossom].base = node;
      (*_blossom_data)[blossom].next = INVALID;
    }

    void augmentOnEdge(const Edge& edge) {

      int left = _blossom_set->find(_graph.u(edge));
      int right = _blossom_set->find(_graph.v(edge));

      int left_tree = _tree_set->find(left);
      alternatePath(left, left_tree);
      destroyTree(left_tree);

      int right_tree = _tree_set->find(right);
      alternatePath(right, right_tree);
      destroyTree(right_tree);

      (*_blossom_data)[left].next = _graph.direct(edge, true);
      (*_blossom_data)[right].next = _graph.direct(edge, false);
    }

    void augmentOnArc(const Arc& arc) {

      int left = _blossom_set->find(_graph.source(arc));
      int right = _blossom_set->find(_graph.target(arc));

      (*_blossom_data)[left].status = MATCHED;

      int right_tree = _tree_set->find(right);
      alternatePath(right, right_tree);
      destroyTree(right_tree);

      (*_blossom_data)[left].next = arc;
      (*_blossom_data)[right].next = _graph.oppositeArc(arc);
    }

    void extendOnArc(const Arc& arc) {
      int base = _blossom_set->find(_graph.target(arc));
      int tree = _tree_set->find(base);

      int odd = _blossom_set->find(_graph.source(arc));
      _tree_set->insert(odd, tree);
      (*_blossom_data)[odd].status = ODD;
      matchedToOdd(odd);
      (*_blossom_data)[odd].pred = arc;

      int even = _blossom_set->find(_graph.target((*_blossom_data)[odd].next));
      (*_blossom_data)[even].pred = (*_blossom_data)[even].next;
      _tree_set->insert(even, tree);
      (*_blossom_data)[even].status = EVEN;
      matchedToEven(even, tree);
    }

    void shrinkOnEdge(const Edge& edge, int tree) {
      int nca = -1;
      std::vector<int> left_path, right_path;

      {
        std::set<int> left_set, right_set;
        int left = _blossom_set->find(_graph.u(edge));
        left_path.push_back(left);
        left_set.insert(left);

        int right = _blossom_set->find(_graph.v(edge));
        right_path.push_back(right);
        right_set.insert(right);

        while (true) {

          if ((*_blossom_data)[left].pred == INVALID) break;

          left =
            _blossom_set->find(_graph.target((*_blossom_data)[left].pred));
          left_path.push_back(left);
          left =
            _blossom_set->find(_graph.target((*_blossom_data)[left].pred));
          left_path.push_back(left);

          left_set.insert(left);

          if (right_set.find(left) != right_set.end()) {
            nca = left;
            break;
          }

          if ((*_blossom_data)[right].pred == INVALID) break;

          right =
            _blossom_set->find(_graph.target((*_blossom_data)[right].pred));
          right_path.push_back(right);
          right =
            _blossom_set->find(_graph.target((*_blossom_data)[right].pred));
          right_path.push_back(right);

          right_set.insert(right);

          if (left_set.find(right) != left_set.end()) {
            nca = right;
            break;
          }

        }

        if (nca == -1) {
          if ((*_blossom_data)[left].pred == INVALID) {
            nca = right;
            while (left_set.find(nca) == left_set.end()) {
              nca =
                _blossom_set->find(_graph.target((*_blossom_data)[nca].pred));
              right_path.push_back(nca);
              nca =
                _blossom_set->find(_graph.target((*_blossom_data)[nca].pred));
              right_path.push_back(nca);
            }
          } else {
            nca = left;
            while (right_set.find(nca) == right_set.end()) {
              nca =
                _blossom_set->find(_graph.target((*_blossom_data)[nca].pred));
              left_path.push_back(nca);
              nca =
                _blossom_set->find(_graph.target((*_blossom_data)[nca].pred));
              left_path.push_back(nca);
            }
          }
        }
      }

      std::vector<int> subblossoms;
      Arc prev;

      prev = _graph.direct(edge, true);
      for (int i = 0; left_path[i] != nca; i += 2) {
        subblossoms.push_back(left_path[i]);
        (*_blossom_data)[left_path[i]].next = prev;
        _tree_set->erase(left_path[i]);

        subblossoms.push_back(left_path[i + 1]);
        (*_blossom_data)[left_path[i + 1]].status = EVEN;
        oddToEven(left_path[i + 1], tree);
        _tree_set->erase(left_path[i + 1]);
        prev = _graph.oppositeArc((*_blossom_data)[left_path[i + 1]].pred);
      }

      int k = 0;
      while (right_path[k] != nca) ++k;

      subblossoms.push_back(nca);
      (*_blossom_data)[nca].next = prev;

      for (int i = k - 2; i >= 0; i -= 2) {
        subblossoms.push_back(right_path[i + 1]);
        (*_blossom_data)[right_path[i + 1]].status = EVEN;
        oddToEven(right_path[i + 1], tree);
        _tree_set->erase(right_path[i + 1]);

        (*_blossom_data)[right_path[i + 1]].next =
          (*_blossom_data)[right_path[i + 1]].pred;

        subblossoms.push_back(right_path[i]);
        _tree_set->erase(right_path[i]);
      }

      int surface =
        _blossom_set->join(subblossoms.begin(), subblossoms.end());

      for (int i = 0; i < int(subblossoms.size()); ++i) {
        if (!_blossom_set->trivial(subblossoms[i])) {
          (*_blossom_data)[subblossoms[i]].pot += 2 * _delta_sum;
        }
        (*_blossom_data)[subblossoms[i]].status = MATCHED;
      }

      (*_blossom_data)[surface].pot = -2 * _delta_sum;
      (*_blossom_data)[surface].offset = 0;
      (*_blossom_data)[surface].status = EVEN;
      (*_blossom_data)[surface].pred = (*_blossom_data)[nca].pred;
      (*_blossom_data)[surface].next = (*_blossom_data)[nca].pred;

      _tree_set->insert(surface, tree);
      _tree_set->erase(nca);
    }

    void splitBlossom(int blossom) {
      Arc next = (*_blossom_data)[blossom].next;
      Arc pred = (*_blossom_data)[blossom].pred;

      int tree = _tree_set->find(blossom);

      (*_blossom_data)[blossom].status = MATCHED;
      oddToMatched(blossom);
      if (_delta2->state(blossom) == _delta2->IN_HEAP) {
        _delta2->erase(blossom);
      }

      std::vector<int> subblossoms;
      _blossom_set->split(blossom, std::back_inserter(subblossoms));

      Value offset = (*_blossom_data)[blossom].offset;
      int b = _blossom_set->find(_graph.source(pred));
      int d = _blossom_set->find(_graph.source(next));

      int ib = -1, id = -1;
      for (int i = 0; i < int(subblossoms.size()); ++i) {
        if (subblossoms[i] == b) ib = i;
        if (subblossoms[i] == d) id = i;

        (*_blossom_data)[subblossoms[i]].offset = offset;
        if (!_blossom_set->trivial(subblossoms[i])) {
          (*_blossom_data)[subblossoms[i]].pot -= 2 * offset;
        }
        if (_blossom_set->classPrio(subblossoms[i]) !=
            std::numeric_limits<Value>::max()) {
          _delta2->push(subblossoms[i],
                        _blossom_set->classPrio(subblossoms[i]) -
                        (*_blossom_data)[subblossoms[i]].offset);
        }
      }

      if (id > ib ? ((id - ib) % 2 == 0) : ((ib - id) % 2 == 1)) {
        for (int i = (id + 1) % subblossoms.size();
             i != ib; i = (i + 2) % subblossoms.size()) {
          int sb = subblossoms[i];
          int tb = subblossoms[(i + 1) % subblossoms.size()];
          (*_blossom_data)[sb].next =
            _graph.oppositeArc((*_blossom_data)[tb].next);
        }

        for (int i = ib; i != id; i = (i + 2) % subblossoms.size()) {
          int sb = subblossoms[i];
          int tb = subblossoms[(i + 1) % subblossoms.size()];
          int ub = subblossoms[(i + 2) % subblossoms.size()];

          (*_blossom_data)[sb].status = ODD;
          matchedToOdd(sb);
          _tree_set->insert(sb, tree);
          (*_blossom_data)[sb].pred = pred;
          (*_blossom_data)[sb].next =
            _graph.oppositeArc((*_blossom_data)[tb].next);

          pred = (*_blossom_data)[ub].next;

          (*_blossom_data)[tb].status = EVEN;
          matchedToEven(tb, tree);
          _tree_set->insert(tb, tree);
          (*_blossom_data)[tb].pred = (*_blossom_data)[tb].next;
        }

        (*_blossom_data)[subblossoms[id]].status = ODD;
        matchedToOdd(subblossoms[id]);
        _tree_set->insert(subblossoms[id], tree);
        (*_blossom_data)[subblossoms[id]].next = next;
        (*_blossom_data)[subblossoms[id]].pred = pred;

      } else {

        for (int i = (ib + 1) % subblossoms.size();
             i != id; i = (i + 2) % subblossoms.size()) {
          int sb = subblossoms[i];
          int tb = subblossoms[(i + 1) % subblossoms.size()];
          (*_blossom_data)[sb].next =
            _graph.oppositeArc((*_blossom_data)[tb].next);
        }

        for (int i = id; i != ib; i = (i + 2) % subblossoms.size()) {
          int sb = subblossoms[i];
          int tb = subblossoms[(i + 1) % subblossoms.size()];
          int ub = subblossoms[(i + 2) % subblossoms.size()];

          (*_blossom_data)[sb].status = ODD;
          matchedToOdd(sb);
          _tree_set->insert(sb, tree);
          (*_blossom_data)[sb].next = next;
          (*_blossom_data)[sb].pred =
            _graph.oppositeArc((*_blossom_data)[tb].next);

          (*_blossom_data)[tb].status = EVEN;
          matchedToEven(tb, tree);
          _tree_set->insert(tb, tree);
          (*_blossom_data)[tb].pred =
            (*_blossom_data)[tb].next =
            _graph.oppositeArc((*_blossom_data)[ub].next);
          next = (*_blossom_data)[ub].next;
        }

        (*_blossom_data)[subblossoms[ib]].status = ODD;
        matchedToOdd(subblossoms[ib]);
        _tree_set->insert(subblossoms[ib], tree);
        (*_blossom_data)[subblossoms[ib]].next = next;
        (*_blossom_data)[subblossoms[ib]].pred = pred;
      }
      _tree_set->erase(blossom);
    }

    void extractBlossom(int blossom, const Node& base, const Arc& matching) {
      if (_blossom_set->trivial(blossom)) {
        int bi = (*_node_index)[base];
        Value pot = (*_node_data)[bi].pot;

        (*_matching)[base] = matching;
        _blossom_node_list.push_back(base);
        (*_node_potential)[base] = pot;
      } else {

        Value pot = (*_blossom_data)[blossom].pot;
        int bn = _blossom_node_list.size();

        std::vector<int> subblossoms;
        _blossom_set->split(blossom, std::back_inserter(subblossoms));
        int b = _blossom_set->find(base);
        int ib = -1;
        for (int i = 0; i < int(subblossoms.size()); ++i) {
          if (subblossoms[i] == b) { ib = i; break; }
        }

        for (int i = 1; i < int(subblossoms.size()); i += 2) {
          int sb = subblossoms[(ib + i) % subblossoms.size()];
          int tb = subblossoms[(ib + i + 1) % subblossoms.size()];

          Arc m = (*_blossom_data)[tb].next;
          extractBlossom(sb, _graph.target(m), _graph.oppositeArc(m));
          extractBlossom(tb, _graph.source(m), m);
        }
        extractBlossom(subblossoms[ib], base, matching);

        int en = _blossom_node_list.size();

        _blossom_potential.push_back(BlossomVariable(bn, en, pot));
      }
    }

    void extractMatching() {
      std::vector<int> blossoms;
      for (typename BlossomSet::ClassIt c(*_blossom_set); c != INVALID; ++c) {
        blossoms.push_back(c);
      }

      for (int i = 0; i < int(blossoms.size()); ++i) {
        if ((*_blossom_data)[blossoms[i]].next != INVALID) {

          Value offset = (*_blossom_data)[blossoms[i]].offset;
          (*_blossom_data)[blossoms[i]].pot += 2 * offset;
          for (typename BlossomSet::ItemIt n(*_blossom_set, blossoms[i]);
               n != INVALID; ++n) {
            (*_node_data)[(*_node_index)[n]].pot -= offset;
          }

          Arc matching = (*_blossom_data)[blossoms[i]].next;
          Node base = _graph.source(matching);
          extractBlossom(blossoms[i], base, matching);
        } else {
          Node base = (*_blossom_data)[blossoms[i]].base;
          extractBlossom(blossoms[i], base, INVALID);
        }
      }
    }

  public:

    /// \brief Constructor
    ///
    /// Constructor.
    MaxWeightedMatching(const Graph& graph, const WeightMap& weight)
      : _graph(graph), _weight(weight), _matching(0),
        _node_potential(0), _blossom_potential(), _blossom_node_list(),
        _node_num(0), _blossom_num(0),

        _blossom_index(0), _blossom_set(0), _blossom_data(0),
        _node_index(0), _node_heap_index(0), _node_data(0),
        _tree_set_index(0), _tree_set(0),

        _delta1_index(0), _delta1(0),
        _delta2_index(0), _delta2(0),
        _delta3_index(0), _delta3(0),
        _delta4_index(0), _delta4(0),

        _delta_sum(), _unmatched(0),

        _fractional(0)
    {}

    ~MaxWeightedMatching() {
      destroyStructures();
      if (_fractional) {
        delete _fractional;
      }
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

      _blossom_node_list.clear();
      _blossom_potential.clear();

      for (ArcIt e(_graph); e != INVALID; ++e) {
        (*_node_heap_index)[e] = BinHeap<Value, IntArcMap>::PRE_HEAP;
      }
      for (NodeIt n(_graph); n != INVALID; ++n) {
        (*_delta1_index)[n] = _delta1->PRE_HEAP;
      }
      for (EdgeIt e(_graph); e != INVALID; ++e) {
        (*_delta3_index)[e] = _delta3->PRE_HEAP;
      }
      for (int i = 0; i < _blossom_num; ++i) {
        (*_delta2_index)[i] = _delta2->PRE_HEAP;
        (*_delta4_index)[i] = _delta4->PRE_HEAP;
      }

      _unmatched = _node_num;

      _delta1->clear();
      _delta2->clear();
      _delta3->clear();
      _delta4->clear();
      _blossom_set->clear();
      _tree_set->clear();

      int index = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        Value max = 0;
        for (OutArcIt e(_graph, n); e != INVALID; ++e) {
          if (_graph.target(e) == n) continue;
          if ((dualScale * _weight[e]) / 2 > max) {
            max = (dualScale * _weight[e]) / 2;
          }
        }
        (*_node_index)[n] = index;
        (*_node_data)[index].heap_index.clear();
        (*_node_data)[index].heap.clear();
        (*_node_data)[index].pot = max;
        _delta1->push(n, max);
        int blossom =
          _blossom_set->insert(n, std::numeric_limits<Value>::max());

        _tree_set->insert(blossom);

        (*_blossom_data)[blossom].status = EVEN;
        (*_blossom_data)[blossom].pred = INVALID;
        (*_blossom_data)[blossom].next = INVALID;
        (*_blossom_data)[blossom].pot = 0;
        (*_blossom_data)[blossom].offset = 0;
        ++index;
      }
      for (EdgeIt e(_graph); e != INVALID; ++e) {
        int si = (*_node_index)[_graph.u(e)];
        int ti = (*_node_index)[_graph.v(e)];
        if (_graph.u(e) != _graph.v(e)) {
          _delta3->push(e, ((*_node_data)[si].pot + (*_node_data)[ti].pot -
                            dualScale * _weight[e]) / 2);
        }
      }
    }

    /// \brief Initialize the algorithm with fractional matching
    ///
    /// This function initializes the algorithm with a fractional
    /// matching. This initialization is also called jumpstart heuristic.
    void fractionalInit() {
      createStructures();

      _blossom_node_list.clear();
      _blossom_potential.clear();

      if (_fractional == 0) {
        _fractional = new FractionalMatching(_graph, _weight, false);
      }
      _fractional->run();

      for (ArcIt e(_graph); e != INVALID; ++e) {
        (*_node_heap_index)[e] = BinHeap<Value, IntArcMap>::PRE_HEAP;
      }
      for (NodeIt n(_graph); n != INVALID; ++n) {
        (*_delta1_index)[n] = _delta1->PRE_HEAP;
      }
      for (EdgeIt e(_graph); e != INVALID; ++e) {
        (*_delta3_index)[e] = _delta3->PRE_HEAP;
      }
      for (int i = 0; i < _blossom_num; ++i) {
        (*_delta2_index)[i] = _delta2->PRE_HEAP;
        (*_delta4_index)[i] = _delta4->PRE_HEAP;
      }

      _unmatched = 0;

      _delta1->clear();
      _delta2->clear();
      _delta3->clear();
      _delta4->clear();
      _blossom_set->clear();
      _tree_set->clear();

      int index = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        Value pot = _fractional->nodeValue(n);
        (*_node_index)[n] = index;
        (*_node_data)[index].pot = pot;
        (*_node_data)[index].heap_index.clear();
        (*_node_data)[index].heap.clear();
        int blossom =
          _blossom_set->insert(n, std::numeric_limits<Value>::max());

        (*_blossom_data)[blossom].status = MATCHED;
        (*_blossom_data)[blossom].pred = INVALID;
        (*_blossom_data)[blossom].next = _fractional->matching(n);
        if (_fractional->matching(n) == INVALID) {
          (*_blossom_data)[blossom].base = n;
        }
        (*_blossom_data)[blossom].pot = 0;
        (*_blossom_data)[blossom].offset = 0;
        ++index;
      }

      typename Graph::template NodeMap<bool> processed(_graph, false);
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if (processed[n]) continue;
        processed[n] = true;
        if (_fractional->matching(n) == INVALID) continue;
        int num = 1;
        Node v = _graph.target(_fractional->matching(n));
        while (n != v) {
          processed[v] = true;
          v = _graph.target(_fractional->matching(v));
          ++num;
        }

        if (num % 2 == 1) {
          std::vector<int> subblossoms(num);

          subblossoms[--num] = _blossom_set->find(n);
          _delta1->push(n, _fractional->nodeValue(n));
          v = _graph.target(_fractional->matching(n));
          while (n != v) {
            subblossoms[--num] = _blossom_set->find(v);
            _delta1->push(v, _fractional->nodeValue(v));
            v = _graph.target(_fractional->matching(v));
          }

          int surface =
            _blossom_set->join(subblossoms.begin(), subblossoms.end());
          (*_blossom_data)[surface].status = EVEN;
          (*_blossom_data)[surface].pred = INVALID;
          (*_blossom_data)[surface].next = INVALID;
          (*_blossom_data)[surface].pot = 0;
          (*_blossom_data)[surface].offset = 0;

          _tree_set->insert(surface);
          ++_unmatched;
        }
      }

      for (EdgeIt e(_graph); e != INVALID; ++e) {
        int si = (*_node_index)[_graph.u(e)];
        int sb = _blossom_set->find(_graph.u(e));
        int ti = (*_node_index)[_graph.v(e)];
        int tb = _blossom_set->find(_graph.v(e));
        if ((*_blossom_data)[sb].status == EVEN &&
            (*_blossom_data)[tb].status == EVEN && sb != tb) {
          _delta3->push(e, ((*_node_data)[si].pot + (*_node_data)[ti].pot -
                            dualScale * _weight[e]) / 2);
        }
      }

      for (NodeIt n(_graph); n != INVALID; ++n) {
        int nb = _blossom_set->find(n);
        if ((*_blossom_data)[nb].status != MATCHED) continue;
        int ni = (*_node_index)[n];

        for (OutArcIt e(_graph, n); e != INVALID; ++e) {
          Node v = _graph.target(e);
          int vb = _blossom_set->find(v);
          int vi = (*_node_index)[v];

          Value rw = (*_node_data)[ni].pot + (*_node_data)[vi].pot -
            dualScale * _weight[e];

          if ((*_blossom_data)[vb].status == EVEN) {

            int vt = _tree_set->find(vb);

            typename std::map<int, Arc>::iterator it =
              (*_node_data)[ni].heap_index.find(vt);

            if (it != (*_node_data)[ni].heap_index.end()) {
              if ((*_node_data)[ni].heap[it->second] > rw) {
                (*_node_data)[ni].heap.replace(it->second, e);
                (*_node_data)[ni].heap.decrease(e, rw);
                it->second = e;
              }
            } else {
              (*_node_data)[ni].heap.push(e, rw);
              (*_node_data)[ni].heap_index.insert(std::make_pair(vt, e));
            }
          }
        }

        if (!(*_node_data)[ni].heap.empty()) {
          _blossom_set->decrease(n, (*_node_data)[ni].heap.prio());
          _delta2->push(nb, _blossom_set->classPrio(nb));
        }
      }
    }

    /// \brief Start the algorithm
    ///
    /// This function starts the algorithm.
    ///
    /// \pre \ref init() or \ref fractionalInit() must be called
    /// before using this function.
    void start() {
      enum OpType {
        D1, D2, D3, D4
      };

      while (_unmatched > 0) {
        Value d1 = !_delta1->empty() ?
          _delta1->prio() : std::numeric_limits<Value>::max();

        Value d2 = !_delta2->empty() ?
          _delta2->prio() : std::numeric_limits<Value>::max();

        Value d3 = !_delta3->empty() ?
          _delta3->prio() : std::numeric_limits<Value>::max();

        Value d4 = !_delta4->empty() ?
          _delta4->prio() : std::numeric_limits<Value>::max();

        _delta_sum = d3; OpType ot = D3;
        if (d1 < _delta_sum) { _delta_sum = d1; ot = D1; }
        if (d2 < _delta_sum) { _delta_sum = d2; ot = D2; }
        if (d4 < _delta_sum) { _delta_sum = d4; ot = D4; }

        switch (ot) {
        case D1:
          {
            Node n = _delta1->top();
            unmatchNode(n);
            --_unmatched;
          }
          break;
        case D2:
          {
            int blossom = _delta2->top();
            Node n = _blossom_set->classTop(blossom);
            Arc a = (*_node_data)[(*_node_index)[n]].heap.top();
            if ((*_blossom_data)[blossom].next == INVALID) {
              augmentOnArc(a);
              --_unmatched;
            } else {
              extendOnArc(a);
            }
          }
          break;
        case D3:
          {
            Edge e = _delta3->top();

            int left_blossom = _blossom_set->find(_graph.u(e));
            int right_blossom = _blossom_set->find(_graph.v(e));

            if (left_blossom == right_blossom) {
              _delta3->pop();
            } else {
              int left_tree = _tree_set->find(left_blossom);
              int right_tree = _tree_set->find(right_blossom);

              if (left_tree == right_tree) {
                shrinkOnEdge(e, left_tree);
              } else {
                augmentOnEdge(e);
                _unmatched -= 2;
              }
            }
          } break;
        case D4:
          splitBlossom(_delta4->top());
          break;
        }
      }
      extractMatching();
    }

    /// \brief Run the algorithm.
    ///
    /// This method runs the \c %MaxWeightedMatching algorithm.
    ///
    /// \note mwm.run() is just a shortcut of the following code.
    /// \code
    ///   mwm.fractionalInit();
    ///   mwm.start();
    /// \endcode
    void run() {
      fractionalInit();
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
    /// This function returns the weight of the found matching.
    ///
    /// \pre Either run() or start() must be called before using this function.
    Value matchingWeight() const {
      Value sum = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_matching)[n] != INVALID) {
          sum += _weight[(*_matching)[n]];
        }
      }
      return sum / 2;
    }

    /// \brief Return the size (cardinality) of the matching.
    ///
    /// This function returns the size (cardinality) of the found matching.
    ///
    /// \pre Either run() or start() must be called before using this function.
    int matchingSize() const {
      int num = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_matching)[n] != INVALID) {
          ++num;
        }
      }
      return num /= 2;
    }

    /// \brief Return \c true if the given edge is in the matching.
    ///
    /// This function returns \c true if the given edge is in the found
    /// matching.
    ///
    /// \pre Either run() or start() must be called before using this function.
    bool matching(const Edge& edge) const {
      return edge == (*_matching)[_graph.u(edge)];
    }

    /// \brief Return the matching arc (or edge) incident to the given node.
    ///
    /// This function returns the matching arc (or edge) incident to the
    /// given node in the found matching or \c INVALID if the node is
    /// not covered by the matching.
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

    /// \brief Return the mate of the given node.
    ///
    /// This function returns the mate of the given node in the found
    /// matching or \c INVALID if the node is not covered by the matching.
    ///
    /// \pre Either run() or start() must be called before using this function.
    Node mate(const Node& node) const {
      return (*_matching)[node] != INVALID ?
        _graph.target((*_matching)[node]) : INVALID;
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
      for (int i = 0; i < blossomNum(); ++i) {
        sum += blossomValue(i) * (blossomSize(i) / 2);
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

    /// \brief Return the number of the blossoms in the basis.
    ///
    /// This function returns the number of the blossoms in the basis.
    ///
    /// \pre Either run() or start() must be called before using this function.
    /// \see BlossomIt
    int blossomNum() const {
      return _blossom_potential.size();
    }

    /// \brief Return the number of the nodes in the given blossom.
    ///
    /// This function returns the number of the nodes in the given blossom.
    ///
    /// \pre Either run() or start() must be called before using this function.
    /// \see BlossomIt
    int blossomSize(int k) const {
      return _blossom_potential[k].end - _blossom_potential[k].begin;
    }

    /// \brief Return the dual value (ptential) of the given blossom.
    ///
    /// This function returns the dual value (ptential) of the given blossom.
    ///
    /// \pre Either run() or start() must be called before using this function.
    Value blossomValue(int k) const {
      return _blossom_potential[k].value;
    }

    /// \brief Iterator for obtaining the nodes of a blossom.
    ///
    /// This class provides an iterator for obtaining the nodes of the
    /// given blossom. It lists a subset of the nodes.
    /// Before using this iterator, you must allocate a
    /// MaxWeightedMatching class and execute it.
    class BlossomIt {
    public:

      /// \brief Constructor.
      ///
      /// Constructor to get the nodes of the given variable.
      ///
      /// \pre Either \ref MaxWeightedMatching::run() "algorithm.run()" or
      /// \ref MaxWeightedMatching::start() "algorithm.start()" must be
      /// called before initializing this iterator.
      BlossomIt(const MaxWeightedMatching& algorithm, int variable)
        : _algorithm(&algorithm)
      {
        _index = _algorithm->_blossom_potential[variable].begin;
        _last = _algorithm->_blossom_potential[variable].end;
      }

      /// \brief Conversion to \c Node.
      ///
      /// Conversion to \c Node.
      operator Node() const {
        return _algorithm->_blossom_node_list[_index];
      }

      /// \brief Increment operator.
      ///
      /// Increment operator.
      BlossomIt& operator++() {
        ++_index;
        return *this;
      }

      /// \brief Validity checking
      ///
      /// Checks whether the iterator is invalid.
      bool operator==(Invalid) const { return _index == _last; }

      /// \brief Validity checking
      ///
      /// Checks whether the iterator is valid.
      bool operator!=(Invalid) const { return _index != _last; }

    private:
      const MaxWeightedMatching* _algorithm;
      int _last;
      int _index;
    };

    /// @}

  };

  /// \ingroup matching
  ///
  /// \brief Weighted perfect matching in general graphs
  ///
  /// This class provides an efficient implementation of Edmond's
  /// maximum weighted perfect matching algorithm. The implementation
  /// is based on extensive use of priority queues and provides
  /// \f$O(nm\log n)\f$ time complexity.
  ///
  /// The maximum weighted perfect matching problem is to find a subset of
  /// the edges in an undirected graph with maximum overall weight for which
  /// each node has exactly one incident edge.
  /// It can be formulated with the following linear program.
  /// \f[ \sum_{e \in \delta(u)}x_e = 1 \quad \forall u\in V\f]
  /** \f[ \sum_{e \in \gamma(B)}x_e \le \frac{\vert B \vert - 1}{2}
      \quad \forall B\in\mathcal{O}\f] */
  /// \f[x_e \ge 0\quad \forall e\in E\f]
  /// \f[\max \sum_{e\in E}x_ew_e\f]
  /// where \f$\delta(X)\f$ is the set of edges incident to a node in
  /// \f$X\f$, \f$\gamma(X)\f$ is the set of edges with both ends in
  /// \f$X\f$ and \f$\mathcal{O}\f$ is the set of odd cardinality
  /// subsets of the nodes.
  ///
  /// The algorithm calculates an optimal matching and a proof of the
  /// optimality. The solution of the dual problem can be used to check
  /// the result of the algorithm. The dual linear problem is the
  /// following.
  /** \f[ y_u + y_v + \sum_{B \in \mathcal{O}, uv \in \gamma(B)}z_B \ge
      w_{uv} \quad \forall uv\in E\f] */
  /// \f[z_B \ge 0 \quad \forall B \in \mathcal{O}\f]
  /** \f[\min \sum_{u \in V}y_u + \sum_{B \in \mathcal{O}}
      \frac{\vert B \vert - 1}{2}z_B\f] */
  ///
  /// The algorithm can be executed with the run() function.
  /// After it the matching (the primal solution) and the dual solution
  /// can be obtained using the query functions and the
  /// \ref MaxWeightedPerfectMatching::BlossomIt "BlossomIt" nested class,
  /// which is able to iterate on the nodes of a blossom.
  /// If the value type is integer, then the dual solution is multiplied
  /// by \ref MaxWeightedMatching::dualScale "4".
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
  class MaxWeightedPerfectMatching {
  public:

    /// The graph type of the algorithm
    typedef GR Graph;
    /// The type of the edge weight map
    typedef WM WeightMap;
    /// The value type of the edge weights
    typedef typename WeightMap::Value Value;

    /// \brief Scaling factor for dual solution
    ///
    /// Scaling factor for dual solution, it is equal to 4 or 1
    /// according to the value type.
    static const int dualScale =
      std::numeric_limits<Value>::is_integer ? 4 : 1;

    /// The type of the matching map
    typedef typename Graph::template NodeMap<typename Graph::Arc>
    MatchingMap;

  private:

    TEMPLATE_GRAPH_TYPEDEFS(Graph);

    typedef typename Graph::template NodeMap<Value> NodePotential;
    typedef std::vector<Node> BlossomNodeList;

    struct BlossomVariable {
      int begin, end;
      Value value;

      BlossomVariable(int _begin, int _end, Value _value)
        : begin(_begin), end(_end), value(_value) {}

    };

    typedef std::vector<BlossomVariable> BlossomPotential;

    const Graph& _graph;
    const WeightMap& _weight;

    MatchingMap* _matching;

    NodePotential* _node_potential;

    BlossomPotential _blossom_potential;
    BlossomNodeList _blossom_node_list;

    int _node_num;
    int _blossom_num;

    typedef RangeMap<int> IntIntMap;

    enum Status {
      EVEN = -1, MATCHED = 0, ODD = 1
    };

    typedef HeapUnionFind<Value, IntNodeMap> BlossomSet;
    struct BlossomData {
      int tree;
      Status status;
      Arc pred, next;
      Value pot, offset;
    };

    IntNodeMap *_blossom_index;
    BlossomSet *_blossom_set;
    RangeMap<BlossomData>* _blossom_data;

    IntNodeMap *_node_index;
    IntArcMap *_node_heap_index;

    struct NodeData {

      NodeData(IntArcMap& node_heap_index)
        : heap(node_heap_index) {}

      int blossom;
      Value pot;
      BinHeap<Value, IntArcMap> heap;
      std::map<int, Arc> heap_index;

      int tree;
    };

    RangeMap<NodeData>* _node_data;

    typedef ExtendFindEnum<IntIntMap> TreeSet;

    IntIntMap *_tree_set_index;
    TreeSet *_tree_set;

    IntIntMap *_delta2_index;
    BinHeap<Value, IntIntMap> *_delta2;

    IntEdgeMap *_delta3_index;
    BinHeap<Value, IntEdgeMap> *_delta3;

    IntIntMap *_delta4_index;
    BinHeap<Value, IntIntMap> *_delta4;

    Value _delta_sum;
    int _unmatched;

    typedef MaxWeightedPerfectFractionalMatching<Graph, WeightMap>
    FractionalMatching;
    FractionalMatching *_fractional;

    void createStructures() {
      _node_num = countNodes(_graph);
      _blossom_num = _node_num * 3 / 2;

      if (!_matching) {
        _matching = new MatchingMap(_graph);
      }

      if (!_node_potential) {
        _node_potential = new NodePotential(_graph);
      }

      if (!_blossom_set) {
        _blossom_index = new IntNodeMap(_graph);
        _blossom_set = new BlossomSet(*_blossom_index);
        _blossom_data = new RangeMap<BlossomData>(_blossom_num);
      } else if (_blossom_data->size() != _blossom_num) {
        delete _blossom_data;
        _blossom_data = new RangeMap<BlossomData>(_blossom_num);
      }

      if (!_node_index) {
        _node_index = new IntNodeMap(_graph);
        _node_heap_index = new IntArcMap(_graph);
        _node_data = new RangeMap<NodeData>(_node_num,
                                            NodeData(*_node_heap_index));
      } else if (_node_data->size() != _node_num) {
        delete _node_data;
        _node_data = new RangeMap<NodeData>(_node_num,
                                            NodeData(*_node_heap_index));
      }

      if (!_tree_set) {
        _tree_set_index = new IntIntMap(_blossom_num);
        _tree_set = new TreeSet(*_tree_set_index);
      } else {
        _tree_set_index->resize(_blossom_num);
      }

      if (!_delta2) {
        _delta2_index = new IntIntMap(_blossom_num);
        _delta2 = new BinHeap<Value, IntIntMap>(*_delta2_index);
      } else {
        _delta2_index->resize(_blossom_num);
      }

      if (!_delta3) {
        _delta3_index = new IntEdgeMap(_graph);
        _delta3 = new BinHeap<Value, IntEdgeMap>(*_delta3_index);
      }

      if (!_delta4) {
        _delta4_index = new IntIntMap(_blossom_num);
        _delta4 = new BinHeap<Value, IntIntMap>(*_delta4_index);
      } else {
        _delta4_index->resize(_blossom_num);
      }
    }

    void destroyStructures() {
      if (_matching) {
        delete _matching;
      }
      if (_node_potential) {
        delete _node_potential;
      }
      if (_blossom_set) {
        delete _blossom_index;
        delete _blossom_set;
        delete _blossom_data;
      }

      if (_node_index) {
        delete _node_index;
        delete _node_heap_index;
        delete _node_data;
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
      if (_delta4) {
        delete _delta4_index;
        delete _delta4;
      }
    }

    void matchedToEven(int blossom, int tree) {
      if (_delta2->state(blossom) == _delta2->IN_HEAP) {
        _delta2->erase(blossom);
      }

      if (!_blossom_set->trivial(blossom)) {
        (*_blossom_data)[blossom].pot -=
          2 * (_delta_sum - (*_blossom_data)[blossom].offset);
      }

      for (typename BlossomSet::ItemIt n(*_blossom_set, blossom);
           n != INVALID; ++n) {

        _blossom_set->increase(n, std::numeric_limits<Value>::max());
        int ni = (*_node_index)[n];

        (*_node_data)[ni].heap.clear();
        (*_node_data)[ni].heap_index.clear();

        (*_node_data)[ni].pot += _delta_sum - (*_blossom_data)[blossom].offset;

        for (InArcIt e(_graph, n); e != INVALID; ++e) {
          Node v = _graph.source(e);
          int vb = _blossom_set->find(v);
          int vi = (*_node_index)[v];

          Value rw = (*_node_data)[ni].pot + (*_node_data)[vi].pot -
            dualScale * _weight[e];

          if ((*_blossom_data)[vb].status == EVEN) {
            if (_delta3->state(e) != _delta3->IN_HEAP && blossom != vb) {
              _delta3->push(e, rw / 2);
            }
          } else {
            typename std::map<int, Arc>::iterator it =
              (*_node_data)[vi].heap_index.find(tree);

            if (it != (*_node_data)[vi].heap_index.end()) {
              if ((*_node_data)[vi].heap[it->second] > rw) {
                (*_node_data)[vi].heap.replace(it->second, e);
                (*_node_data)[vi].heap.decrease(e, rw);
                it->second = e;
              }
            } else {
              (*_node_data)[vi].heap.push(e, rw);
              (*_node_data)[vi].heap_index.insert(std::make_pair(tree, e));
            }

            if ((*_blossom_set)[v] > (*_node_data)[vi].heap.prio()) {
              _blossom_set->decrease(v, (*_node_data)[vi].heap.prio());

              if ((*_blossom_data)[vb].status == MATCHED) {
                if (_delta2->state(vb) != _delta2->IN_HEAP) {
                  _delta2->push(vb, _blossom_set->classPrio(vb) -
                               (*_blossom_data)[vb].offset);
                } else if ((*_delta2)[vb] > _blossom_set->classPrio(vb) -
                           (*_blossom_data)[vb].offset){
                  _delta2->decrease(vb, _blossom_set->classPrio(vb) -
                                   (*_blossom_data)[vb].offset);
                }
              }
            }
          }
        }
      }
      (*_blossom_data)[blossom].offset = 0;
    }

    void matchedToOdd(int blossom) {
      if (_delta2->state(blossom) == _delta2->IN_HEAP) {
        _delta2->erase(blossom);
      }
      (*_blossom_data)[blossom].offset += _delta_sum;
      if (!_blossom_set->trivial(blossom)) {
        _delta4->push(blossom, (*_blossom_data)[blossom].pot / 2 +
                     (*_blossom_data)[blossom].offset);
      }
    }

    void evenToMatched(int blossom, int tree) {
      if (!_blossom_set->trivial(blossom)) {
        (*_blossom_data)[blossom].pot += 2 * _delta_sum;
      }

      for (typename BlossomSet::ItemIt n(*_blossom_set, blossom);
           n != INVALID; ++n) {
        int ni = (*_node_index)[n];
        (*_node_data)[ni].pot -= _delta_sum;

        for (InArcIt e(_graph, n); e != INVALID; ++e) {
          Node v = _graph.source(e);
          int vb = _blossom_set->find(v);
          int vi = (*_node_index)[v];

          Value rw = (*_node_data)[ni].pot + (*_node_data)[vi].pot -
            dualScale * _weight[e];

          if (vb == blossom) {
            if (_delta3->state(e) == _delta3->IN_HEAP) {
              _delta3->erase(e);
            }
          } else if ((*_blossom_data)[vb].status == EVEN) {

            if (_delta3->state(e) == _delta3->IN_HEAP) {
              _delta3->erase(e);
            }

            int vt = _tree_set->find(vb);

            if (vt != tree) {

              Arc r = _graph.oppositeArc(e);

              typename std::map<int, Arc>::iterator it =
                (*_node_data)[ni].heap_index.find(vt);

              if (it != (*_node_data)[ni].heap_index.end()) {
                if ((*_node_data)[ni].heap[it->second] > rw) {
                  (*_node_data)[ni].heap.replace(it->second, r);
                  (*_node_data)[ni].heap.decrease(r, rw);
                  it->second = r;
                }
              } else {
                (*_node_data)[ni].heap.push(r, rw);
                (*_node_data)[ni].heap_index.insert(std::make_pair(vt, r));
              }

              if ((*_blossom_set)[n] > (*_node_data)[ni].heap.prio()) {
                _blossom_set->decrease(n, (*_node_data)[ni].heap.prio());

                if (_delta2->state(blossom) != _delta2->IN_HEAP) {
                  _delta2->push(blossom, _blossom_set->classPrio(blossom) -
                               (*_blossom_data)[blossom].offset);
                } else if ((*_delta2)[blossom] >
                           _blossom_set->classPrio(blossom) -
                           (*_blossom_data)[blossom].offset){
                  _delta2->decrease(blossom, _blossom_set->classPrio(blossom) -
                                   (*_blossom_data)[blossom].offset);
                }
              }
            }
          } else {

            typename std::map<int, Arc>::iterator it =
              (*_node_data)[vi].heap_index.find(tree);

            if (it != (*_node_data)[vi].heap_index.end()) {
              (*_node_data)[vi].heap.erase(it->second);
              (*_node_data)[vi].heap_index.erase(it);
              if ((*_node_data)[vi].heap.empty()) {
                _blossom_set->increase(v, std::numeric_limits<Value>::max());
              } else if ((*_blossom_set)[v] < (*_node_data)[vi].heap.prio()) {
                _blossom_set->increase(v, (*_node_data)[vi].heap.prio());
              }

              if ((*_blossom_data)[vb].status == MATCHED) {
                if (_blossom_set->classPrio(vb) ==
                    std::numeric_limits<Value>::max()) {
                  _delta2->erase(vb);
                } else if ((*_delta2)[vb] < _blossom_set->classPrio(vb) -
                           (*_blossom_data)[vb].offset) {
                  _delta2->increase(vb, _blossom_set->classPrio(vb) -
                                   (*_blossom_data)[vb].offset);
                }
              }
            }
          }
        }
      }
    }

    void oddToMatched(int blossom) {
      (*_blossom_data)[blossom].offset -= _delta_sum;

      if (_blossom_set->classPrio(blossom) !=
          std::numeric_limits<Value>::max()) {
        _delta2->push(blossom, _blossom_set->classPrio(blossom) -
                       (*_blossom_data)[blossom].offset);
      }

      if (!_blossom_set->trivial(blossom)) {
        _delta4->erase(blossom);
      }
    }

    void oddToEven(int blossom, int tree) {
      if (!_blossom_set->trivial(blossom)) {
        _delta4->erase(blossom);
        (*_blossom_data)[blossom].pot -=
          2 * (2 * _delta_sum - (*_blossom_data)[blossom].offset);
      }

      for (typename BlossomSet::ItemIt n(*_blossom_set, blossom);
           n != INVALID; ++n) {
        int ni = (*_node_index)[n];

        _blossom_set->increase(n, std::numeric_limits<Value>::max());

        (*_node_data)[ni].heap.clear();
        (*_node_data)[ni].heap_index.clear();
        (*_node_data)[ni].pot +=
          2 * _delta_sum - (*_blossom_data)[blossom].offset;

        for (InArcIt e(_graph, n); e != INVALID; ++e) {
          Node v = _graph.source(e);
          int vb = _blossom_set->find(v);
          int vi = (*_node_index)[v];

          Value rw = (*_node_data)[ni].pot + (*_node_data)[vi].pot -
            dualScale * _weight[e];

          if ((*_blossom_data)[vb].status == EVEN) {
            if (_delta3->state(e) != _delta3->IN_HEAP && blossom != vb) {
              _delta3->push(e, rw / 2);
            }
          } else {

            typename std::map<int, Arc>::iterator it =
              (*_node_data)[vi].heap_index.find(tree);

            if (it != (*_node_data)[vi].heap_index.end()) {
              if ((*_node_data)[vi].heap[it->second] > rw) {
                (*_node_data)[vi].heap.replace(it->second, e);
                (*_node_data)[vi].heap.decrease(e, rw);
                it->second = e;
              }
            } else {
              (*_node_data)[vi].heap.push(e, rw);
              (*_node_data)[vi].heap_index.insert(std::make_pair(tree, e));
            }

            if ((*_blossom_set)[v] > (*_node_data)[vi].heap.prio()) {
              _blossom_set->decrease(v, (*_node_data)[vi].heap.prio());

              if ((*_blossom_data)[vb].status == MATCHED) {
                if (_delta2->state(vb) != _delta2->IN_HEAP) {
                  _delta2->push(vb, _blossom_set->classPrio(vb) -
                               (*_blossom_data)[vb].offset);
                } else if ((*_delta2)[vb] > _blossom_set->classPrio(vb) -
                           (*_blossom_data)[vb].offset) {
                  _delta2->decrease(vb, _blossom_set->classPrio(vb) -
                                   (*_blossom_data)[vb].offset);
                }
              }
            }
          }
        }
      }
      (*_blossom_data)[blossom].offset = 0;
    }

    void alternatePath(int even, int tree) {
      int odd;

      evenToMatched(even, tree);
      (*_blossom_data)[even].status = MATCHED;

      while ((*_blossom_data)[even].pred != INVALID) {
        odd = _blossom_set->find(_graph.target((*_blossom_data)[even].pred));
        (*_blossom_data)[odd].status = MATCHED;
        oddToMatched(odd);
        (*_blossom_data)[odd].next = (*_blossom_data)[odd].pred;

        even = _blossom_set->find(_graph.target((*_blossom_data)[odd].pred));
        (*_blossom_data)[even].status = MATCHED;
        evenToMatched(even, tree);
        (*_blossom_data)[even].next =
          _graph.oppositeArc((*_blossom_data)[odd].pred);
      }

    }

    void destroyTree(int tree) {
      for (TreeSet::ItemIt b(*_tree_set, tree); b != INVALID; ++b) {
        if ((*_blossom_data)[b].status == EVEN) {
          (*_blossom_data)[b].status = MATCHED;
          evenToMatched(b, tree);
        } else if ((*_blossom_data)[b].status == ODD) {
          (*_blossom_data)[b].status = MATCHED;
          oddToMatched(b);
        }
      }
      _tree_set->eraseClass(tree);
    }

    void augmentOnEdge(const Edge& edge) {

      int left = _blossom_set->find(_graph.u(edge));
      int right = _blossom_set->find(_graph.v(edge));

      int left_tree = _tree_set->find(left);
      alternatePath(left, left_tree);
      destroyTree(left_tree);

      int right_tree = _tree_set->find(right);
      alternatePath(right, right_tree);
      destroyTree(right_tree);

      (*_blossom_data)[left].next = _graph.direct(edge, true);
      (*_blossom_data)[right].next = _graph.direct(edge, false);
    }

    void extendOnArc(const Arc& arc) {
      int base = _blossom_set->find(_graph.target(arc));
      int tree = _tree_set->find(base);

      int odd = _blossom_set->find(_graph.source(arc));
      _tree_set->insert(odd, tree);
      (*_blossom_data)[odd].status = ODD;
      matchedToOdd(odd);
      (*_blossom_data)[odd].pred = arc;

      int even = _blossom_set->find(_graph.target((*_blossom_data)[odd].next));
      (*_blossom_data)[even].pred = (*_blossom_data)[even].next;
      _tree_set->insert(even, tree);
      (*_blossom_data)[even].status = EVEN;
      matchedToEven(even, tree);
    }

    void shrinkOnEdge(const Edge& edge, int tree) {
      int nca = -1;
      std::vector<int> left_path, right_path;

      {
        std::set<int> left_set, right_set;
        int left = _blossom_set->find(_graph.u(edge));
        left_path.push_back(left);
        left_set.insert(left);

        int right = _blossom_set->find(_graph.v(edge));
        right_path.push_back(right);
        right_set.insert(right);

        while (true) {

          if ((*_blossom_data)[left].pred == INVALID) break;

          left =
            _blossom_set->find(_graph.target((*_blossom_data)[left].pred));
          left_path.push_back(left);
          left =
            _blossom_set->find(_graph.target((*_blossom_data)[left].pred));
          left_path.push_back(left);

          left_set.insert(left);

          if (right_set.find(left) != right_set.end()) {
            nca = left;
            break;
          }

          if ((*_blossom_data)[right].pred == INVALID) break;

          right =
            _blossom_set->find(_graph.target((*_blossom_data)[right].pred));
          right_path.push_back(right);
          right =
            _blossom_set->find(_graph.target((*_blossom_data)[right].pred));
          right_path.push_back(right);

          right_set.insert(right);

          if (left_set.find(right) != left_set.end()) {
            nca = right;
            break;
          }

        }

        if (nca == -1) {
          if ((*_blossom_data)[left].pred == INVALID) {
            nca = right;
            while (left_set.find(nca) == left_set.end()) {
              nca =
                _blossom_set->find(_graph.target((*_blossom_data)[nca].pred));
              right_path.push_back(nca);
              nca =
                _blossom_set->find(_graph.target((*_blossom_data)[nca].pred));
              right_path.push_back(nca);
            }
          } else {
            nca = left;
            while (right_set.find(nca) == right_set.end()) {
              nca =
                _blossom_set->find(_graph.target((*_blossom_data)[nca].pred));
              left_path.push_back(nca);
              nca =
                _blossom_set->find(_graph.target((*_blossom_data)[nca].pred));
              left_path.push_back(nca);
            }
          }
        }
      }

      std::vector<int> subblossoms;
      Arc prev;

      prev = _graph.direct(edge, true);
      for (int i = 0; left_path[i] != nca; i += 2) {
        subblossoms.push_back(left_path[i]);
        (*_blossom_data)[left_path[i]].next = prev;
        _tree_set->erase(left_path[i]);

        subblossoms.push_back(left_path[i + 1]);
        (*_blossom_data)[left_path[i + 1]].status = EVEN;
        oddToEven(left_path[i + 1], tree);
        _tree_set->erase(left_path[i + 1]);
        prev = _graph.oppositeArc((*_blossom_data)[left_path[i + 1]].pred);
      }

      int k = 0;
      while (right_path[k] != nca) ++k;

      subblossoms.push_back(nca);
      (*_blossom_data)[nca].next = prev;

      for (int i = k - 2; i >= 0; i -= 2) {
        subblossoms.push_back(right_path[i + 1]);
        (*_blossom_data)[right_path[i + 1]].status = EVEN;
        oddToEven(right_path[i + 1], tree);
        _tree_set->erase(right_path[i + 1]);

        (*_blossom_data)[right_path[i + 1]].next =
          (*_blossom_data)[right_path[i + 1]].pred;

        subblossoms.push_back(right_path[i]);
        _tree_set->erase(right_path[i]);
      }

      int surface =
        _blossom_set->join(subblossoms.begin(), subblossoms.end());

      for (int i = 0; i < int(subblossoms.size()); ++i) {
        if (!_blossom_set->trivial(subblossoms[i])) {
          (*_blossom_data)[subblossoms[i]].pot += 2 * _delta_sum;
        }
        (*_blossom_data)[subblossoms[i]].status = MATCHED;
      }

      (*_blossom_data)[surface].pot = -2 * _delta_sum;
      (*_blossom_data)[surface].offset = 0;
      (*_blossom_data)[surface].status = EVEN;
      (*_blossom_data)[surface].pred = (*_blossom_data)[nca].pred;
      (*_blossom_data)[surface].next = (*_blossom_data)[nca].pred;

      _tree_set->insert(surface, tree);
      _tree_set->erase(nca);
    }

    void splitBlossom(int blossom) {
      Arc next = (*_blossom_data)[blossom].next;
      Arc pred = (*_blossom_data)[blossom].pred;

      int tree = _tree_set->find(blossom);

      (*_blossom_data)[blossom].status = MATCHED;
      oddToMatched(blossom);
      if (_delta2->state(blossom) == _delta2->IN_HEAP) {
        _delta2->erase(blossom);
      }

      std::vector<int> subblossoms;
      _blossom_set->split(blossom, std::back_inserter(subblossoms));

      Value offset = (*_blossom_data)[blossom].offset;
      int b = _blossom_set->find(_graph.source(pred));
      int d = _blossom_set->find(_graph.source(next));

      int ib = -1, id = -1;
      for (int i = 0; i < int(subblossoms.size()); ++i) {
        if (subblossoms[i] == b) ib = i;
        if (subblossoms[i] == d) id = i;

        (*_blossom_data)[subblossoms[i]].offset = offset;
        if (!_blossom_set->trivial(subblossoms[i])) {
          (*_blossom_data)[subblossoms[i]].pot -= 2 * offset;
        }
        if (_blossom_set->classPrio(subblossoms[i]) !=
            std::numeric_limits<Value>::max()) {
          _delta2->push(subblossoms[i],
                        _blossom_set->classPrio(subblossoms[i]) -
                        (*_blossom_data)[subblossoms[i]].offset);
        }
      }

      if (id > ib ? ((id - ib) % 2 == 0) : ((ib - id) % 2 == 1)) {
        for (int i = (id + 1) % subblossoms.size();
             i != ib; i = (i + 2) % subblossoms.size()) {
          int sb = subblossoms[i];
          int tb = subblossoms[(i + 1) % subblossoms.size()];
          (*_blossom_data)[sb].next =
            _graph.oppositeArc((*_blossom_data)[tb].next);
        }

        for (int i = ib; i != id; i = (i + 2) % subblossoms.size()) {
          int sb = subblossoms[i];
          int tb = subblossoms[(i + 1) % subblossoms.size()];
          int ub = subblossoms[(i + 2) % subblossoms.size()];

          (*_blossom_data)[sb].status = ODD;
          matchedToOdd(sb);
          _tree_set->insert(sb, tree);
          (*_blossom_data)[sb].pred = pred;
          (*_blossom_data)[sb].next =
                           _graph.oppositeArc((*_blossom_data)[tb].next);

          pred = (*_blossom_data)[ub].next;

          (*_blossom_data)[tb].status = EVEN;
          matchedToEven(tb, tree);
          _tree_set->insert(tb, tree);
          (*_blossom_data)[tb].pred = (*_blossom_data)[tb].next;
        }

        (*_blossom_data)[subblossoms[id]].status = ODD;
        matchedToOdd(subblossoms[id]);
        _tree_set->insert(subblossoms[id], tree);
        (*_blossom_data)[subblossoms[id]].next = next;
        (*_blossom_data)[subblossoms[id]].pred = pred;

      } else {

        for (int i = (ib + 1) % subblossoms.size();
             i != id; i = (i + 2) % subblossoms.size()) {
          int sb = subblossoms[i];
          int tb = subblossoms[(i + 1) % subblossoms.size()];
          (*_blossom_data)[sb].next =
            _graph.oppositeArc((*_blossom_data)[tb].next);
        }

        for (int i = id; i != ib; i = (i + 2) % subblossoms.size()) {
          int sb = subblossoms[i];
          int tb = subblossoms[(i + 1) % subblossoms.size()];
          int ub = subblossoms[(i + 2) % subblossoms.size()];

          (*_blossom_data)[sb].status = ODD;
          matchedToOdd(sb);
          _tree_set->insert(sb, tree);
          (*_blossom_data)[sb].next = next;
          (*_blossom_data)[sb].pred =
            _graph.oppositeArc((*_blossom_data)[tb].next);

          (*_blossom_data)[tb].status = EVEN;
          matchedToEven(tb, tree);
          _tree_set->insert(tb, tree);
          (*_blossom_data)[tb].pred =
            (*_blossom_data)[tb].next =
            _graph.oppositeArc((*_blossom_data)[ub].next);
          next = (*_blossom_data)[ub].next;
        }

        (*_blossom_data)[subblossoms[ib]].status = ODD;
        matchedToOdd(subblossoms[ib]);
        _tree_set->insert(subblossoms[ib], tree);
        (*_blossom_data)[subblossoms[ib]].next = next;
        (*_blossom_data)[subblossoms[ib]].pred = pred;
      }
      _tree_set->erase(blossom);
    }

    void extractBlossom(int blossom, const Node& base, const Arc& matching) {
      if (_blossom_set->trivial(blossom)) {
        int bi = (*_node_index)[base];
        Value pot = (*_node_data)[bi].pot;

        (*_matching)[base] = matching;
        _blossom_node_list.push_back(base);
        (*_node_potential)[base] = pot;
      } else {

        Value pot = (*_blossom_data)[blossom].pot;
        int bn = _blossom_node_list.size();

        std::vector<int> subblossoms;
        _blossom_set->split(blossom, std::back_inserter(subblossoms));
        int b = _blossom_set->find(base);
        int ib = -1;
        for (int i = 0; i < int(subblossoms.size()); ++i) {
          if (subblossoms[i] == b) { ib = i; break; }
        }

        for (int i = 1; i < int(subblossoms.size()); i += 2) {
          int sb = subblossoms[(ib + i) % subblossoms.size()];
          int tb = subblossoms[(ib + i + 1) % subblossoms.size()];

          Arc m = (*_blossom_data)[tb].next;
          extractBlossom(sb, _graph.target(m), _graph.oppositeArc(m));
          extractBlossom(tb, _graph.source(m), m);
        }
        extractBlossom(subblossoms[ib], base, matching);

        int en = _blossom_node_list.size();

        _blossom_potential.push_back(BlossomVariable(bn, en, pot));
      }
    }

    void extractMatching() {
      std::vector<int> blossoms;
      for (typename BlossomSet::ClassIt c(*_blossom_set); c != INVALID; ++c) {
        blossoms.push_back(c);
      }

      for (int i = 0; i < int(blossoms.size()); ++i) {

        Value offset = (*_blossom_data)[blossoms[i]].offset;
        (*_blossom_data)[blossoms[i]].pot += 2 * offset;
        for (typename BlossomSet::ItemIt n(*_blossom_set, blossoms[i]);
             n != INVALID; ++n) {
          (*_node_data)[(*_node_index)[n]].pot -= offset;
        }

        Arc matching = (*_blossom_data)[blossoms[i]].next;
        Node base = _graph.source(matching);
        extractBlossom(blossoms[i], base, matching);
      }
    }

  public:

    /// \brief Constructor
    ///
    /// Constructor.
    MaxWeightedPerfectMatching(const Graph& graph, const WeightMap& weight)
      : _graph(graph), _weight(weight), _matching(0),
        _node_potential(0), _blossom_potential(), _blossom_node_list(),
        _node_num(0), _blossom_num(0),

        _blossom_index(0), _blossom_set(0), _blossom_data(0),
        _node_index(0), _node_heap_index(0), _node_data(0),
        _tree_set_index(0), _tree_set(0),

        _delta2_index(0), _delta2(0),
        _delta3_index(0), _delta3(0),
        _delta4_index(0), _delta4(0),

        _delta_sum(), _unmatched(0),

        _fractional(0)
    {}

    ~MaxWeightedPerfectMatching() {
      destroyStructures();
      if (_fractional) {
        delete _fractional;
      }
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

      _blossom_node_list.clear();
      _blossom_potential.clear();

      for (ArcIt e(_graph); e != INVALID; ++e) {
        (*_node_heap_index)[e] = BinHeap<Value, IntArcMap>::PRE_HEAP;
      }
      for (EdgeIt e(_graph); e != INVALID; ++e) {
        (*_delta3_index)[e] = _delta3->PRE_HEAP;
      }
      for (int i = 0; i < _blossom_num; ++i) {
        (*_delta2_index)[i] = _delta2->PRE_HEAP;
        (*_delta4_index)[i] = _delta4->PRE_HEAP;
      }

      _unmatched = _node_num;

      _delta2->clear();
      _delta3->clear();
      _delta4->clear();
      _blossom_set->clear();
      _tree_set->clear();

      int index = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        Value max = - std::numeric_limits<Value>::max();
        for (OutArcIt e(_graph, n); e != INVALID; ++e) {
          if (_graph.target(e) == n) continue;
          if ((dualScale * _weight[e]) / 2 > max) {
            max = (dualScale * _weight[e]) / 2;
          }
        }
        (*_node_index)[n] = index;
        (*_node_data)[index].heap_index.clear();
        (*_node_data)[index].heap.clear();
        (*_node_data)[index].pot = max;
        int blossom =
          _blossom_set->insert(n, std::numeric_limits<Value>::max());

        _tree_set->insert(blossom);

        (*_blossom_data)[blossom].status = EVEN;
        (*_blossom_data)[blossom].pred = INVALID;
        (*_blossom_data)[blossom].next = INVALID;
        (*_blossom_data)[blossom].pot = 0;
        (*_blossom_data)[blossom].offset = 0;
        ++index;
      }
      for (EdgeIt e(_graph); e != INVALID; ++e) {
        int si = (*_node_index)[_graph.u(e)];
        int ti = (*_node_index)[_graph.v(e)];
        if (_graph.u(e) != _graph.v(e)) {
          _delta3->push(e, ((*_node_data)[si].pot + (*_node_data)[ti].pot -
                            dualScale * _weight[e]) / 2);
        }
      }
    }

    /// \brief Initialize the algorithm with fractional matching
    ///
    /// This function initializes the algorithm with a fractional
    /// matching. This initialization is also called jumpstart heuristic.
    void fractionalInit() {
      createStructures();

      _blossom_node_list.clear();
      _blossom_potential.clear();

      if (_fractional == 0) {
        _fractional = new FractionalMatching(_graph, _weight, false);
      }
      if (!_fractional->run()) {
        _unmatched = -1;
        return;
      }

      for (ArcIt e(_graph); e != INVALID; ++e) {
        (*_node_heap_index)[e] = BinHeap<Value, IntArcMap>::PRE_HEAP;
      }
      for (EdgeIt e(_graph); e != INVALID; ++e) {
        (*_delta3_index)[e] = _delta3->PRE_HEAP;
      }
      for (int i = 0; i < _blossom_num; ++i) {
        (*_delta2_index)[i] = _delta2->PRE_HEAP;
        (*_delta4_index)[i] = _delta4->PRE_HEAP;
      }

      _unmatched = 0;

      _delta2->clear();
      _delta3->clear();
      _delta4->clear();
      _blossom_set->clear();
      _tree_set->clear();

      int index = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        Value pot = _fractional->nodeValue(n);
        (*_node_index)[n] = index;
        (*_node_data)[index].pot = pot;
        (*_node_data)[index].heap_index.clear();
        (*_node_data)[index].heap.clear();
        int blossom =
          _blossom_set->insert(n, std::numeric_limits<Value>::max());

        (*_blossom_data)[blossom].status = MATCHED;
        (*_blossom_data)[blossom].pred = INVALID;
        (*_blossom_data)[blossom].next = _fractional->matching(n);
        (*_blossom_data)[blossom].pot = 0;
        (*_blossom_data)[blossom].offset = 0;
        ++index;
      }

      typename Graph::template NodeMap<bool> processed(_graph, false);
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if (processed[n]) continue;
        processed[n] = true;
        if (_fractional->matching(n) == INVALID) continue;
        int num = 1;
        Node v = _graph.target(_fractional->matching(n));
        while (n != v) {
          processed[v] = true;
          v = _graph.target(_fractional->matching(v));
          ++num;
        }

        if (num % 2 == 1) {
          std::vector<int> subblossoms(num);

          subblossoms[--num] = _blossom_set->find(n);
          v = _graph.target(_fractional->matching(n));
          while (n != v) {
            subblossoms[--num] = _blossom_set->find(v);
            v = _graph.target(_fractional->matching(v));
          }

          int surface =
            _blossom_set->join(subblossoms.begin(), subblossoms.end());
          (*_blossom_data)[surface].status = EVEN;
          (*_blossom_data)[surface].pred = INVALID;
          (*_blossom_data)[surface].next = INVALID;
          (*_blossom_data)[surface].pot = 0;
          (*_blossom_data)[surface].offset = 0;

          _tree_set->insert(surface);
          ++_unmatched;
        }
      }

      for (EdgeIt e(_graph); e != INVALID; ++e) {
        int si = (*_node_index)[_graph.u(e)];
        int sb = _blossom_set->find(_graph.u(e));
        int ti = (*_node_index)[_graph.v(e)];
        int tb = _blossom_set->find(_graph.v(e));
        if ((*_blossom_data)[sb].status == EVEN &&
            (*_blossom_data)[tb].status == EVEN && sb != tb) {
          _delta3->push(e, ((*_node_data)[si].pot + (*_node_data)[ti].pot -
                            dualScale * _weight[e]) / 2);
        }
      }

      for (NodeIt n(_graph); n != INVALID; ++n) {
        int nb = _blossom_set->find(n);
        if ((*_blossom_data)[nb].status != MATCHED) continue;
        int ni = (*_node_index)[n];

        for (OutArcIt e(_graph, n); e != INVALID; ++e) {
          Node v = _graph.target(e);
          int vb = _blossom_set->find(v);
          int vi = (*_node_index)[v];

          Value rw = (*_node_data)[ni].pot + (*_node_data)[vi].pot -
            dualScale * _weight[e];

          if ((*_blossom_data)[vb].status == EVEN) {

            int vt = _tree_set->find(vb);

            typename std::map<int, Arc>::iterator it =
              (*_node_data)[ni].heap_index.find(vt);

            if (it != (*_node_data)[ni].heap_index.end()) {
              if ((*_node_data)[ni].heap[it->second] > rw) {
                (*_node_data)[ni].heap.replace(it->second, e);
                (*_node_data)[ni].heap.decrease(e, rw);
                it->second = e;
              }
            } else {
              (*_node_data)[ni].heap.push(e, rw);
              (*_node_data)[ni].heap_index.insert(std::make_pair(vt, e));
            }
          }
        }

        if (!(*_node_data)[ni].heap.empty()) {
          _blossom_set->decrease(n, (*_node_data)[ni].heap.prio());
          _delta2->push(nb, _blossom_set->classPrio(nb));
        }
      }
    }

    /// \brief Start the algorithm
    ///
    /// This function starts the algorithm.
    ///
    /// \pre \ref init() or \ref fractionalInit() must be called before
    /// using this function.
    bool start() {
      enum OpType {
        D2, D3, D4
      };

      if (_unmatched == -1) return false;

      while (_unmatched > 0) {
        Value d2 = !_delta2->empty() ?
          _delta2->prio() : std::numeric_limits<Value>::max();

        Value d3 = !_delta3->empty() ?
          _delta3->prio() : std::numeric_limits<Value>::max();

        Value d4 = !_delta4->empty() ?
          _delta4->prio() : std::numeric_limits<Value>::max();

        _delta_sum = d3; OpType ot = D3;
        if (d2 < _delta_sum) { _delta_sum = d2; ot = D2; }
        if (d4 < _delta_sum) { _delta_sum = d4; ot = D4; }

        if (_delta_sum == std::numeric_limits<Value>::max()) {
          return false;
        }

        switch (ot) {
        case D2:
          {
            int blossom = _delta2->top();
            Node n = _blossom_set->classTop(blossom);
            Arc e = (*_node_data)[(*_node_index)[n]].heap.top();
            extendOnArc(e);
          }
          break;
        case D3:
          {
            Edge e = _delta3->top();

            int left_blossom = _blossom_set->find(_graph.u(e));
            int right_blossom = _blossom_set->find(_graph.v(e));

            if (left_blossom == right_blossom) {
              _delta3->pop();
            } else {
              int left_tree = _tree_set->find(left_blossom);
              int right_tree = _tree_set->find(right_blossom);

              if (left_tree == right_tree) {
                shrinkOnEdge(e, left_tree);
              } else {
                augmentOnEdge(e);
                _unmatched -= 2;
              }
            }
          } break;
        case D4:
          splitBlossom(_delta4->top());
          break;
        }
      }
      extractMatching();
      return true;
    }

    /// \brief Run the algorithm.
    ///
    /// This method runs the \c %MaxWeightedPerfectMatching algorithm.
    ///
    /// \note mwpm.run() is just a shortcut of the following code.
    /// \code
    ///   mwpm.fractionalInit();
    ///   mwpm.start();
    /// \endcode
    bool run() {
      fractionalInit();
      return start();
    }

    /// @}

    /// \name Primal Solution
    /// Functions to get the primal solution, i.e. the maximum weighted
    /// perfect matching.\n
    /// Either \ref run() or \ref start() function should be called before
    /// using them.

    /// @{

    /// \brief Return the weight of the matching.
    ///
    /// This function returns the weight of the found matching.
    ///
    /// \pre Either run() or start() must be called before using this function.
    Value matchingWeight() const {
      Value sum = 0;
      for (NodeIt n(_graph); n != INVALID; ++n) {
        if ((*_matching)[n] != INVALID) {
          sum += _weight[(*_matching)[n]];
        }
      }
      return sum / 2;
    }

    /// \brief Return \c true if the given edge is in the matching.
    ///
    /// This function returns \c true if the given edge is in the found
    /// matching.
    ///
    /// \pre Either run() or start() must be called before using this function.
    bool matching(const Edge& edge) const {
      return static_cast<const Edge&>((*_matching)[_graph.u(edge)]) == edge;
    }

    /// \brief Return the matching arc (or edge) incident to the given node.
    ///
    /// This function returns the matching arc (or edge) incident to the
    /// given node in the found matching or \c INVALID if the node is
    /// not covered by the matching.
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

    /// \brief Return the mate of the given node.
    ///
    /// This function returns the mate of the given node in the found
    /// matching or \c INVALID if the node is not covered by the matching.
    ///
    /// \pre Either run() or start() must be called before using this function.
    Node mate(const Node& node) const {
      return _graph.target((*_matching)[node]);
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
      for (int i = 0; i < blossomNum(); ++i) {
        sum += blossomValue(i) * (blossomSize(i) / 2);
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

    /// \brief Return the number of the blossoms in the basis.
    ///
    /// This function returns the number of the blossoms in the basis.
    ///
    /// \pre Either run() or start() must be called before using this function.
    /// \see BlossomIt
    int blossomNum() const {
      return _blossom_potential.size();
    }

    /// \brief Return the number of the nodes in the given blossom.
    ///
    /// This function returns the number of the nodes in the given blossom.
    ///
    /// \pre Either run() or start() must be called before using this function.
    /// \see BlossomIt
    int blossomSize(int k) const {
      return _blossom_potential[k].end - _blossom_potential[k].begin;
    }

    /// \brief Return the dual value (ptential) of the given blossom.
    ///
    /// This function returns the dual value (ptential) of the given blossom.
    ///
    /// \pre Either run() or start() must be called before using this function.
    Value blossomValue(int k) const {
      return _blossom_potential[k].value;
    }

    /// \brief Iterator for obtaining the nodes of a blossom.
    ///
    /// This class provides an iterator for obtaining the nodes of the
    /// given blossom. It lists a subset of the nodes.
    /// Before using this iterator, you must allocate a
    /// MaxWeightedPerfectMatching class and execute it.
    class BlossomIt {
    public:

      /// \brief Constructor.
      ///
      /// Constructor to get the nodes of the given variable.
      ///
      /// \pre Either \ref MaxWeightedPerfectMatching::run() "algorithm.run()"
      /// or \ref MaxWeightedPerfectMatching::start() "algorithm.start()"
      /// must be called before initializing this iterator.
      BlossomIt(const MaxWeightedPerfectMatching& algorithm, int variable)
        : _algorithm(&algorithm)
      {
        _index = _algorithm->_blossom_potential[variable].begin;
        _last = _algorithm->_blossom_potential[variable].end;
      }

      /// \brief Conversion to \c Node.
      ///
      /// Conversion to \c Node.
      operator Node() const {
        return _algorithm->_blossom_node_list[_index];
      }

      /// \brief Increment operator.
      ///
      /// Increment operator.
      BlossomIt& operator++() {
        ++_index;
        return *this;
      }

      /// \brief Validity checking
      ///
      /// This function checks whether the iterator is invalid.
      bool operator==(Invalid) const { return _index == _last; }

      /// \brief Validity checking
      ///
      /// This function checks whether the iterator is valid.
      bool operator!=(Invalid) const { return _index != _last; }

    private:
      const MaxWeightedPerfectMatching* _algorithm;
      int _last;
      int _index;
    };

    /// @}

  };

} //END OF NAMESPACE LEMON

#endif //LEMON_MATCHING_H
