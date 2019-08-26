/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2009
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

#ifndef HYPERCUBE_GRAPH_H
#define HYPERCUBE_GRAPH_H

#include <vector>
#include <lemon/core.h>
#include <lemon/assert.h>
#include <lemon/bits/graph_extender.h>

///\ingroup graphs
///\file
///\brief HypercubeGraph class.

namespace lemon {

  class HypercubeGraphBase {

  public:

    typedef HypercubeGraphBase Graph;

    class Node;
    class Edge;
    class Arc;

  public:

    HypercubeGraphBase() {}

  protected:

    void construct(int dim) {
      LEMON_ASSERT(dim >= 1, "The number of dimensions must be at least 1.");
      _dim = dim;
      _node_num = 1 << dim;
      _edge_num = dim * (1 << (dim-1));
    }

  public:

    typedef True NodeNumTag;
    typedef True EdgeNumTag;
    typedef True ArcNumTag;

    int nodeNum() const { return _node_num; }
    int edgeNum() const { return _edge_num; }
    int arcNum() const { return 2 * _edge_num; }

    int maxNodeId() const { return _node_num - 1; }
    int maxEdgeId() const { return _edge_num - 1; }
    int maxArcId() const { return 2 * _edge_num - 1; }

    static Node nodeFromId(int id) { return Node(id); }
    static Edge edgeFromId(int id) { return Edge(id); }
    static Arc arcFromId(int id) { return Arc(id); }

    static int id(Node node) { return node._id; }
    static int id(Edge edge) { return edge._id; }
    static int id(Arc arc) { return arc._id; }

    Node u(Edge edge) const {
      int base = edge._id & ((1 << (_dim-1)) - 1);
      int k = edge._id >> (_dim-1);
      return ((base >> k) << (k+1)) | (base & ((1 << k) - 1));
    }

    Node v(Edge edge) const {
      int base = edge._id & ((1 << (_dim-1)) - 1);
      int k = edge._id >> (_dim-1);
      return ((base >> k) << (k+1)) | (base & ((1 << k) - 1)) | (1 << k);
    }

    Node source(Arc arc) const {
      return (arc._id & 1) == 1 ? u(arc) : v(arc);
    }

    Node target(Arc arc) const {
      return (arc._id & 1) == 1 ? v(arc) : u(arc);
    }

    typedef True FindEdgeTag;
    typedef True FindArcTag;

    Edge findEdge(Node u, Node v, Edge prev = INVALID) const {
      if (prev != INVALID) return INVALID;
      int d = u._id ^ v._id;
      int k = 0;
      if (d == 0) return INVALID;
      for ( ; (d & 1) == 0; d >>= 1) ++k;
      if (d >> 1 != 0) return INVALID;
      return (k << (_dim-1)) | ((u._id >> (k+1)) << k) |
        (u._id & ((1 << k) - 1));
    }

    Arc findArc(Node u, Node v, Arc prev = INVALID) const {
      Edge edge = findEdge(u, v, prev);
      if (edge == INVALID) return INVALID;
      int k = edge._id >> (_dim-1);
      return ((u._id >> k) & 1) == 1 ? edge._id << 1 : (edge._id << 1) | 1;
    }

    class Node {
      friend class HypercubeGraphBase;

    protected:
      int _id;
      Node(int id) : _id(id) {}
    public:
      Node() {}
      Node (Invalid) : _id(-1) {}
      bool operator==(const Node node) const {return _id == node._id;}
      bool operator!=(const Node node) const {return _id != node._id;}
      bool operator<(const Node node) const {return _id < node._id;}
    };

    class Edge {
      friend class HypercubeGraphBase;
      friend class Arc;

    protected:
      int _id;

      Edge(int id) : _id(id) {}

    public:
      Edge() {}
      Edge (Invalid) : _id(-1) {}
      bool operator==(const Edge edge) const {return _id == edge._id;}
      bool operator!=(const Edge edge) const {return _id != edge._id;}
      bool operator<(const Edge edge) const {return _id < edge._id;}
    };

    class Arc {
      friend class HypercubeGraphBase;

    protected:
      int _id;

      Arc(int id) : _id(id) {}

    public:
      Arc() {}
      Arc (Invalid) : _id(-1) {}
      operator Edge() const { return _id != -1 ? Edge(_id >> 1) : INVALID; }
      bool operator==(const Arc arc) const {return _id == arc._id;}
      bool operator!=(const Arc arc) const {return _id != arc._id;}
      bool operator<(const Arc arc) const {return _id < arc._id;}
    };

    void first(Node& node) const {
      node._id = _node_num - 1;
    }

    static void next(Node& node) {
      --node._id;
    }

    void first(Edge& edge) const {
      edge._id = _edge_num - 1;
    }

    static void next(Edge& edge) {
      --edge._id;
    }

    void first(Arc& arc) const {
      arc._id = 2 * _edge_num - 1;
    }

    static void next(Arc& arc) {
      --arc._id;
    }

    void firstInc(Edge& edge, bool& dir, const Node& node) const {
      edge._id = node._id >> 1;
      dir = (node._id & 1) == 0;
    }

    void nextInc(Edge& edge, bool& dir) const {
      Node n = dir ? u(edge) : v(edge);
      int k = (edge._id >> (_dim-1)) + 1;
      if (k < _dim) {
        edge._id = (k << (_dim-1)) |
          ((n._id >> (k+1)) << k) | (n._id & ((1 << k) - 1));
        dir = ((n._id >> k) & 1) == 0;
      } else {
        edge._id = -1;
        dir = true;
      }
    }

    void firstOut(Arc& arc, const Node& node) const {
      arc._id = ((node._id >> 1) << 1) | (~node._id & 1);
    }

    void nextOut(Arc& arc) const {
      Node n = (arc._id & 1) == 1 ? u(arc) : v(arc);
      int k = (arc._id >> _dim) + 1;
      if (k < _dim) {
        arc._id = (k << (_dim-1)) |
          ((n._id >> (k+1)) << k) | (n._id & ((1 << k) - 1));
        arc._id = (arc._id << 1) | (~(n._id >> k) & 1);
      } else {
        arc._id = -1;
      }
    }

    void firstIn(Arc& arc, const Node& node) const {
      arc._id = ((node._id >> 1) << 1) | (node._id & 1);
    }

    void nextIn(Arc& arc) const {
      Node n = (arc._id & 1) == 1 ? v(arc) : u(arc);
      int k = (arc._id >> _dim) + 1;
      if (k < _dim) {
        arc._id = (k << (_dim-1)) |
          ((n._id >> (k+1)) << k) | (n._id & ((1 << k) - 1));
        arc._id = (arc._id << 1) | ((n._id >> k) & 1);
      } else {
        arc._id = -1;
      }
    }

    static bool direction(Arc arc) {
      return (arc._id & 1) == 1;
    }

    static Arc direct(Edge edge, bool dir) {
      return Arc((edge._id << 1) | (dir ? 1 : 0));
    }

    int dimension() const {
      return _dim;
    }

    bool projection(Node node, int n) const {
      return static_cast<bool>(node._id & (1 << n));
    }

    int dimension(Edge edge) const {
      return edge._id >> (_dim-1);
    }

    int dimension(Arc arc) const {
      return arc._id >> _dim;
    }

    static int index(Node node) {
      return node._id;
    }

    Node operator()(int ix) const {
      return Node(ix);
    }

  private:
    int _dim;
    int _node_num, _edge_num;
  };


  typedef GraphExtender<HypercubeGraphBase> ExtendedHypercubeGraphBase;

  /// \ingroup graphs
  ///
  /// \brief Hypercube graph class
  ///
  /// HypercubeGraph implements a special graph type. The nodes of the
  /// graph are indexed with integers having at most \c dim binary digits.
  /// Two nodes are connected in the graph if and only if their indices
  /// differ only on one position in the binary form.
  /// This class is completely static and it needs constant memory space.
  /// Thus you can neither add nor delete nodes or edges, however,
  /// the structure can be resized using resize().
  ///
  /// This type fully conforms to the \ref concepts::Graph "Graph concept".
  /// Most of its member functions and nested classes are documented
  /// only in the concept class.
  ///
  /// This class provides constant time counting for nodes, edges and arcs.
  ///
  /// \note The type of the indices is chosen to \c int for efficiency
  /// reasons. Thus the maximum dimension of this implementation is 26
  /// (assuming that the size of \c int is 32 bit).
  class HypercubeGraph : public ExtendedHypercubeGraphBase {
    typedef ExtendedHypercubeGraphBase Parent;

  public:

    /// \brief Constructs a hypercube graph with \c dim dimensions.
    ///
    /// Constructs a hypercube graph with \c dim dimensions.
    HypercubeGraph(int dim) { construct(dim); }

    /// \brief Resizes the graph
    ///
    /// This function resizes the graph. It fully destroys and
    /// rebuilds the structure, therefore the maps of the graph will be
    /// reallocated automatically and the previous values will be lost.
    void resize(int dim) {
      Parent::notifier(Arc()).clear();
      Parent::notifier(Edge()).clear();
      Parent::notifier(Node()).clear();
      construct(dim);
      Parent::notifier(Node()).build();
      Parent::notifier(Edge()).build();
      Parent::notifier(Arc()).build();
    }

    /// \brief The number of dimensions.
    ///
    /// Gives back the number of dimensions.
    int dimension() const {
      return Parent::dimension();
    }

    /// \brief Returns \c true if the n'th bit of the node is one.
    ///
    /// Returns \c true if the n'th bit of the node is one.
    bool projection(Node node, int n) const {
      return Parent::projection(node, n);
    }

    /// \brief The dimension id of an edge.
    ///
    /// Gives back the dimension id of the given edge.
    /// It is in the range <tt>[0..dim-1]</tt>.
    int dimension(Edge edge) const {
      return Parent::dimension(edge);
    }

    /// \brief The dimension id of an arc.
    ///
    /// Gives back the dimension id of the given arc.
    /// It is in the range <tt>[0..dim-1]</tt>.
    int dimension(Arc arc) const {
      return Parent::dimension(arc);
    }

    /// \brief The index of a node.
    ///
    /// Gives back the index of the given node.
    /// The lower bits of the integer describes the node.
    static int index(Node node) {
      return Parent::index(node);
    }

    /// \brief Gives back a node by its index.
    ///
    /// Gives back a node by its index.
    Node operator()(int ix) const {
      return Parent::operator()(ix);
    }

    /// \brief Number of nodes.
    int nodeNum() const { return Parent::nodeNum(); }
    /// \brief Number of edges.
    int edgeNum() const { return Parent::edgeNum(); }
    /// \brief Number of arcs.
    int arcNum() const { return Parent::arcNum(); }

    /// \brief Linear combination map.
    ///
    /// This map makes possible to give back a linear combination
    /// for each node. It works like the \c std::accumulate function,
    /// so it accumulates the \c bf binary function with the \c fv first
    /// value. The map accumulates only on that positions (dimensions)
    /// where the index of the node is one. The values that have to be
    /// accumulated should be given by the \c begin and \c end iterators
    /// and the length of this range should be equal to the dimension
    /// number of the graph.
    ///
    ///\code
    /// const int DIM = 3;
    /// HypercubeGraph graph(DIM);
    /// dim2::Point<double> base[DIM];
    /// for (int k = 0; k < DIM; ++k) {
    ///   base[k].x = rnd();
    ///   base[k].y = rnd();
    /// }
    /// HypercubeGraph::HyperMap<dim2::Point<double> >
    ///   pos(graph, base, base + DIM, dim2::Point<double>(0.0, 0.0));
    ///\endcode
    ///
    /// \see HypercubeGraph
    template <typename T, typename BF = std::plus<T> >
    class HyperMap {
    public:

      /// \brief The key type of the map
      typedef Node Key;
      /// \brief The value type of the map
      typedef T Value;

      /// \brief Constructor for HyperMap.
      ///
      /// Construct a HyperMap for the given graph. The values that have
      /// to be accumulated should be given by the \c begin and \c end
      /// iterators and the length of this range should be equal to the
      /// dimension number of the graph.
      ///
      /// This map accumulates the \c bf binary function with the \c fv
      /// first value on that positions (dimensions) where the index of
      /// the node is one.
      template <typename It>
      HyperMap(const Graph& graph, It begin, It end,
               T fv = 0, const BF& bf = BF())
        : _graph(graph), _values(begin, end), _first_value(fv), _bin_func(bf)
      {
        LEMON_ASSERT(_values.size() == graph.dimension(),
                     "Wrong size of range");
      }

      /// \brief The partial accumulated value.
      ///
      /// Gives back the partial accumulated value.
      Value operator[](const Key& k) const {
        Value val = _first_value;
        int id = _graph.index(k);
        int n = 0;
        while (id != 0) {
          if (id & 1) {
            val = _bin_func(val, _values[n]);
          }
          id >>= 1;
          ++n;
        }
        return val;
      }

    private:
      const Graph& _graph;
      std::vector<T> _values;
      T _first_value;
      BF _bin_func;
    };

  };

}

#endif
