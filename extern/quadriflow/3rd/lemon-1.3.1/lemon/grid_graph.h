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

#ifndef GRID_GRAPH_H
#define GRID_GRAPH_H

#include <lemon/core.h>
#include <lemon/bits/graph_extender.h>
#include <lemon/dim2.h>
#include <lemon/assert.h>

///\ingroup graphs
///\file
///\brief GridGraph class.

namespace lemon {

  class GridGraphBase {

  public:

    typedef GridGraphBase Graph;

    class Node;
    class Edge;
    class Arc;

  public:

    GridGraphBase() {}

  protected:

    void construct(int width, int height) {
       _width = width; _height = height;
      _node_num = width * height;
      _edge_num = 2 * _node_num - width - height;
      _edge_limit = _node_num - _width;
    }

  public:

    Node operator()(int i, int j) const {
      LEMON_DEBUG(0 <= i && i < _width &&
                  0 <= j  && j < _height, "Index out of range");
      return Node(i + j * _width);
    }

    int col(Node n) const {
      return n._id % _width;
    }

    int row(Node n) const {
      return n._id / _width;
    }

    dim2::Point<int> pos(Node n) const {
      return dim2::Point<int>(col(n), row(n));
    }

    int width() const {
      return _width;
    }

    int height() const {
      return _height;
    }

    typedef True NodeNumTag;
    typedef True EdgeNumTag;
    typedef True ArcNumTag;

    int nodeNum() const { return _node_num; }
    int edgeNum() const { return _edge_num; }
    int arcNum() const { return 2 * _edge_num; }

    Node u(Edge edge) const {
      if (edge._id < _edge_limit) {
        return edge._id;
      } else {
        return (edge._id - _edge_limit) % (_width - 1) +
          (edge._id - _edge_limit) / (_width - 1) * _width;
      }
    }

    Node v(Edge edge) const {
      if (edge._id < _edge_limit) {
        return edge._id + _width;
      } else {
        return (edge._id - _edge_limit) % (_width - 1) +
          (edge._id - _edge_limit) / (_width - 1) * _width + 1;
      }
    }

    Node source(Arc arc) const {
      return (arc._id & 1) == 1 ? u(arc) : v(arc);
    }

    Node target(Arc arc) const {
      return (arc._id & 1) == 1 ? v(arc) : u(arc);
    }

    static int id(Node node) { return node._id; }
    static int id(Edge edge) { return edge._id; }
    static int id(Arc arc) { return arc._id; }

    int maxNodeId() const { return _node_num - 1; }
    int maxEdgeId() const { return _edge_num - 1; }
    int maxArcId() const { return 2 * _edge_num - 1; }

    static Node nodeFromId(int id) { return Node(id);}
    static Edge edgeFromId(int id) { return Edge(id);}
    static Arc arcFromId(int id) { return Arc(id);}

    typedef True FindEdgeTag;
    typedef True FindArcTag;

    Edge findEdge(Node u, Node v, Edge prev = INVALID) const {
      if (prev != INVALID) return INVALID;
      if (v._id > u._id) {
        if (v._id - u._id == _width)
          return Edge(u._id);
        if (v._id - u._id == 1 && u._id % _width < _width - 1) {
          return Edge(u._id / _width * (_width - 1) +
                      u._id % _width + _edge_limit);
        }
      } else {
        if (u._id - v._id == _width)
          return Edge(v._id);
        if (u._id - v._id == 1 && v._id % _width < _width - 1) {
          return Edge(v._id / _width * (_width - 1) +
                      v._id % _width + _edge_limit);
        }
      }
      return INVALID;
    }

    Arc findArc(Node u, Node v, Arc prev = INVALID) const {
      if (prev != INVALID) return INVALID;
      if (v._id > u._id) {
        if (v._id - u._id == _width)
          return Arc((u._id << 1) | 1);
        if (v._id - u._id == 1 && u._id % _width < _width - 1) {
          return Arc(((u._id / _width * (_width - 1) +
                       u._id % _width + _edge_limit) << 1) | 1);
        }
      } else {
        if (u._id - v._id == _width)
          return Arc(v._id << 1);
        if (u._id - v._id == 1 && v._id % _width < _width - 1) {
          return Arc((v._id / _width * (_width - 1) +
                       v._id % _width + _edge_limit) << 1);
        }
      }
      return INVALID;
    }

    class Node {
      friend class GridGraphBase;

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
      friend class GridGraphBase;
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
      friend class GridGraphBase;

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

    static bool direction(Arc arc) {
      return (arc._id & 1) == 1;
    }

    static Arc direct(Edge edge, bool dir) {
      return Arc((edge._id << 1) | (dir ? 1 : 0));
    }

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

    void firstOut(Arc& arc, const Node& node) const {
      if (node._id % _width < _width - 1) {
        arc._id = (_edge_limit + node._id % _width +
                   (node._id / _width) * (_width - 1)) << 1 | 1;
        return;
      }
      if (node._id < _node_num - _width) {
        arc._id = node._id << 1 | 1;
        return;
      }
      if (node._id % _width > 0) {
        arc._id = (_edge_limit + node._id % _width +
                   (node._id / _width) * (_width - 1) - 1) << 1;
        return;
      }
      if (node._id >= _width) {
        arc._id = (node._id - _width) << 1;
        return;
      }
      arc._id = -1;
    }

    void nextOut(Arc& arc) const {
      int nid = arc._id >> 1;
      if ((arc._id & 1) == 1) {
        if (nid >= _edge_limit) {
          nid = (nid - _edge_limit) % (_width - 1) +
            (nid - _edge_limit) / (_width - 1) * _width;
          if (nid < _node_num - _width) {
            arc._id = nid << 1 | 1;
            return;
          }
        }
        if (nid % _width > 0) {
          arc._id = (_edge_limit + nid % _width +
                     (nid / _width) * (_width - 1) - 1) << 1;
          return;
        }
        if (nid >= _width) {
          arc._id = (nid - _width) << 1;
          return;
        }
      } else {
        if (nid >= _edge_limit) {
          nid = (nid - _edge_limit) % (_width - 1) +
            (nid - _edge_limit) / (_width - 1) * _width + 1;
          if (nid >= _width) {
            arc._id = (nid - _width) << 1;
            return;
          }
        }
      }
      arc._id = -1;
    }

    void firstIn(Arc& arc, const Node& node) const {
      if (node._id % _width < _width - 1) {
        arc._id = (_edge_limit + node._id % _width +
                   (node._id / _width) * (_width - 1)) << 1;
        return;
      }
      if (node._id < _node_num - _width) {
        arc._id = node._id << 1;
        return;
      }
      if (node._id % _width > 0) {
        arc._id = (_edge_limit + node._id % _width +
                   (node._id / _width) * (_width - 1) - 1) << 1 | 1;
        return;
      }
      if (node._id >= _width) {
        arc._id = (node._id - _width) << 1 | 1;
        return;
      }
      arc._id = -1;
    }

    void nextIn(Arc& arc) const {
      int nid = arc._id >> 1;
      if ((arc._id & 1) == 0) {
        if (nid >= _edge_limit) {
          nid = (nid - _edge_limit) % (_width - 1) +
            (nid - _edge_limit) / (_width - 1) * _width;
          if (nid < _node_num - _width) {
            arc._id = nid << 1;
            return;
          }
        }
        if (nid % _width > 0) {
          arc._id = (_edge_limit + nid % _width +
                     (nid / _width) * (_width - 1) - 1) << 1 | 1;
          return;
        }
        if (nid >= _width) {
          arc._id = (nid - _width) << 1 | 1;
          return;
        }
      } else {
        if (nid >= _edge_limit) {
          nid = (nid - _edge_limit) % (_width - 1) +
            (nid - _edge_limit) / (_width - 1) * _width + 1;
          if (nid >= _width) {
            arc._id = (nid - _width) << 1 | 1;
            return;
          }
        }
      }
      arc._id = -1;
    }

    void firstInc(Edge& edge, bool& dir, const Node& node) const {
      if (node._id % _width < _width - 1) {
        edge._id = _edge_limit + node._id % _width +
          (node._id / _width) * (_width - 1);
        dir = true;
        return;
      }
      if (node._id < _node_num - _width) {
        edge._id = node._id;
        dir = true;
        return;
      }
      if (node._id % _width > 0) {
        edge._id = _edge_limit + node._id % _width +
          (node._id / _width) * (_width - 1) - 1;
        dir = false;
        return;
      }
      if (node._id >= _width) {
        edge._id = node._id - _width;
        dir = false;
        return;
      }
      edge._id = -1;
      dir = true;
    }

    void nextInc(Edge& edge, bool& dir) const {
      int nid = edge._id;
      if (dir) {
        if (nid >= _edge_limit) {
          nid = (nid - _edge_limit) % (_width - 1) +
            (nid - _edge_limit) / (_width - 1) * _width;
          if (nid < _node_num - _width) {
            edge._id = nid;
            return;
          }
        }
        if (nid % _width > 0) {
          edge._id = _edge_limit + nid % _width +
            (nid / _width) * (_width - 1) - 1;
          dir = false;
          return;
        }
        if (nid >= _width) {
          edge._id = nid - _width;
          dir = false;
          return;
        }
      } else {
        if (nid >= _edge_limit) {
          nid = (nid - _edge_limit) % (_width - 1) +
            (nid - _edge_limit) / (_width - 1) * _width + 1;
          if (nid >= _width) {
            edge._id = nid - _width;
            return;
          }
        }
      }
      edge._id = -1;
      dir = true;
    }

    Arc right(Node n) const {
      if (n._id % _width < _width - 1) {
        return Arc(((_edge_limit + n._id % _width +
                    (n._id / _width) * (_width - 1)) << 1) | 1);
      } else {
        return INVALID;
      }
    }

    Arc left(Node n) const {
      if (n._id % _width > 0) {
        return Arc((_edge_limit + n._id % _width +
                     (n._id / _width) * (_width - 1) - 1) << 1);
      } else {
        return INVALID;
      }
    }

    Arc up(Node n) const {
      if (n._id < _edge_limit) {
        return Arc((n._id << 1) | 1);
      } else {
        return INVALID;
      }
    }

    Arc down(Node n) const {
      if (n._id >= _width) {
        return Arc((n._id - _width) << 1);
      } else {
        return INVALID;
      }
    }

  private:
    int _width, _height;
    int _node_num, _edge_num;
    int _edge_limit;
  };


  typedef GraphExtender<GridGraphBase> ExtendedGridGraphBase;

  /// \ingroup graphs
  ///
  /// \brief Grid graph class
  ///
  /// GridGraph implements a special graph type. The nodes of the
  /// graph can be indexed by two integer values \c (i,j) where \c i is
  /// in the range <tt>[0..width()-1]</tt> and j is in the range
  /// <tt>[0..height()-1]</tt>. Two nodes are connected in the graph if
  /// the indices differ exactly on one position and the difference is
  /// also exactly one. The nodes of the graph can be obtained by position
  /// using the \c operator()() function and the indices of the nodes can
  /// be obtained using \c pos(), \c col() and \c row() members. The outgoing
  /// arcs can be retrieved with the \c right(), \c up(), \c left()
  /// and \c down() functions, where the bottom-left corner is the
  /// origin.
  ///
  /// This class is completely static and it needs constant memory space.
  /// Thus you can neither add nor delete nodes or edges, however
  /// the structure can be resized using resize().
  ///
  /// \image html grid_graph.png
  /// \image latex grid_graph.eps "Grid graph" width=\textwidth
  ///
  /// A short example about the basic usage:
  ///\code
  /// GridGraph graph(rows, cols);
  /// GridGraph::NodeMap<int> val(graph);
  /// for (int i = 0; i < graph.width(); ++i) {
  ///   for (int j = 0; j < graph.height(); ++j) {
  ///     val[graph(i, j)] = i + j;
  ///   }
  /// }
  ///\endcode
  ///
  /// This type fully conforms to the \ref concepts::Graph "Graph concept".
  /// Most of its member functions and nested classes are documented
  /// only in the concept class.
  ///
  /// This class provides constant time counting for nodes, edges and arcs.
  class GridGraph : public ExtendedGridGraphBase {
    typedef ExtendedGridGraphBase Parent;

  public:

    /// \brief Map to get the indices of the nodes as \ref dim2::Point
    /// "dim2::Point<int>".
    ///
    /// Map to get the indices of the nodes as \ref dim2::Point
    /// "dim2::Point<int>".
    class IndexMap {
    public:
      /// \brief The key type of the map
      typedef GridGraph::Node Key;
      /// \brief The value type of the map
      typedef dim2::Point<int> Value;

      /// \brief Constructor
      IndexMap(const GridGraph& graph) : _graph(graph) {}

      /// \brief The subscript operator
      Value operator[](Key key) const {
        return _graph.pos(key);
      }

    private:
      const GridGraph& _graph;
    };

    /// \brief Map to get the column of the nodes.
    ///
    /// Map to get the column of the nodes.
    class ColMap {
    public:
      /// \brief The key type of the map
      typedef GridGraph::Node Key;
      /// \brief The value type of the map
      typedef int Value;

      /// \brief Constructor
      ColMap(const GridGraph& graph) : _graph(graph) {}

      /// \brief The subscript operator
      Value operator[](Key key) const {
        return _graph.col(key);
      }

    private:
      const GridGraph& _graph;
    };

    /// \brief Map to get the row of the nodes.
    ///
    /// Map to get the row of the nodes.
    class RowMap {
    public:
      /// \brief The key type of the map
      typedef GridGraph::Node Key;
      /// \brief The value type of the map
      typedef int Value;

      /// \brief Constructor
      RowMap(const GridGraph& graph) : _graph(graph) {}

      /// \brief The subscript operator
      Value operator[](Key key) const {
        return _graph.row(key);
      }

    private:
      const GridGraph& _graph;
    };

    /// \brief Constructor
    ///
    /// Construct a grid graph with the given size.
    GridGraph(int width, int height) { construct(width, height); }

    /// \brief Resizes the graph
    ///
    /// This function resizes the graph. It fully destroys and
    /// rebuilds the structure, therefore the maps of the graph will be
    /// reallocated automatically and the previous values will be lost.
    void resize(int width, int height) {
      Parent::notifier(Arc()).clear();
      Parent::notifier(Edge()).clear();
      Parent::notifier(Node()).clear();
      construct(width, height);
      Parent::notifier(Node()).build();
      Parent::notifier(Edge()).build();
      Parent::notifier(Arc()).build();
    }

    /// \brief The node on the given position.
    ///
    /// Gives back the node on the given position.
    Node operator()(int i, int j) const {
      return Parent::operator()(i, j);
    }

    /// \brief The column index of the node.
    ///
    /// Gives back the column index of the node.
    int col(Node n) const {
      return Parent::col(n);
    }

    /// \brief The row index of the node.
    ///
    /// Gives back the row index of the node.
    int row(Node n) const {
      return Parent::row(n);
    }

    /// \brief The position of the node.
    ///
    /// Gives back the position of the node, ie. the <tt>(col,row)</tt> pair.
    dim2::Point<int> pos(Node n) const {
      return Parent::pos(n);
    }

    /// \brief The number of the columns.
    ///
    /// Gives back the number of the columns.
    int width() const {
      return Parent::width();
    }

    /// \brief The number of the rows.
    ///
    /// Gives back the number of the rows.
    int height() const {
      return Parent::height();
    }

    /// \brief The arc goes right from the node.
    ///
    /// Gives back the arc goes right from the node. If there is not
    /// outgoing arc then it gives back INVALID.
    Arc right(Node n) const {
      return Parent::right(n);
    }

    /// \brief The arc goes left from the node.
    ///
    /// Gives back the arc goes left from the node. If there is not
    /// outgoing arc then it gives back INVALID.
    Arc left(Node n) const {
      return Parent::left(n);
    }

    /// \brief The arc goes up from the node.
    ///
    /// Gives back the arc goes up from the node. If there is not
    /// outgoing arc then it gives back INVALID.
    Arc up(Node n) const {
      return Parent::up(n);
    }

    /// \brief The arc goes down from the node.
    ///
    /// Gives back the arc goes down from the node. If there is not
    /// outgoing arc then it gives back INVALID.
    Arc down(Node n) const {
      return Parent::down(n);
    }

    /// \brief Index map of the grid graph
    ///
    /// Just returns an IndexMap for the grid graph.
    IndexMap indexMap() const {
      return IndexMap(*this);
    }

    /// \brief Row map of the grid graph
    ///
    /// Just returns a RowMap for the grid graph.
    RowMap rowMap() const {
      return RowMap(*this);
    }

    /// \brief Column map of the grid graph
    ///
    /// Just returns a ColMap for the grid graph.
    ColMap colMap() const {
      return ColMap(*this);
    }

  };

}
#endif
