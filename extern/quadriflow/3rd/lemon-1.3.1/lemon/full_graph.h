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

#ifndef LEMON_FULL_GRAPH_H
#define LEMON_FULL_GRAPH_H

#include <lemon/core.h>
#include <lemon/bits/graph_extender.h>

///\ingroup graphs
///\file
///\brief FullDigraph and FullGraph classes.

namespace lemon {

  class FullDigraphBase {
  public:

    typedef FullDigraphBase Digraph;

    class Node;
    class Arc;

  protected:

    int _node_num;
    int _arc_num;

    FullDigraphBase() {}

    void construct(int n) { _node_num = n; _arc_num = n * n; }

  public:

    typedef True NodeNumTag;
    typedef True ArcNumTag;

    Node operator()(int ix) const { return Node(ix); }
    static int index(const Node& node) { return node._id; }

    Arc arc(const Node& s, const Node& t) const {
      return Arc(s._id * _node_num + t._id);
    }

    int nodeNum() const { return _node_num; }
    int arcNum() const { return _arc_num; }

    int maxNodeId() const { return _node_num - 1; }
    int maxArcId() const { return _arc_num - 1; }

    Node source(Arc arc) const { return arc._id / _node_num; }
    Node target(Arc arc) const { return arc._id % _node_num; }

    static int id(Node node) { return node._id; }
    static int id(Arc arc) { return arc._id; }

    static Node nodeFromId(int id) { return Node(id);}
    static Arc arcFromId(int id) { return Arc(id);}

    typedef True FindArcTag;

    Arc findArc(Node s, Node t, Arc prev = INVALID) const {
      return prev == INVALID ? arc(s, t) : INVALID;
    }

    class Node {
      friend class FullDigraphBase;

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

    class Arc {
      friend class FullDigraphBase;

    protected:
      int _id;  // _node_num * source + target;

      Arc(int id) : _id(id) {}

    public:
      Arc() { }
      Arc (Invalid) { _id = -1; }
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

    void first(Arc& arc) const {
      arc._id = _arc_num - 1;
    }

    static void next(Arc& arc) {
      --arc._id;
    }

    void firstOut(Arc& arc, const Node& node) const {
      arc._id = (node._id + 1) * _node_num - 1;
    }

    void nextOut(Arc& arc) const {
      if (arc._id % _node_num == 0) arc._id = 0;
      --arc._id;
    }

    void firstIn(Arc& arc, const Node& node) const {
      arc._id = _arc_num + node._id - _node_num;
    }

    void nextIn(Arc& arc) const {
      arc._id -= _node_num;
      if (arc._id < 0) arc._id = -1;
    }

  };

  typedef DigraphExtender<FullDigraphBase> ExtendedFullDigraphBase;

  /// \ingroup graphs
  ///
  /// \brief A directed full graph class.
  ///
  /// FullDigraph is a simple and fast implmenetation of directed full
  /// (complete) graphs. It contains an arc from each node to each node
  /// (including a loop for each node), therefore the number of arcs
  /// is the square of the number of nodes.
  /// This class is completely static and it needs constant memory space.
  /// Thus you can neither add nor delete nodes or arcs, however
  /// the structure can be resized using resize().
  ///
  /// This type fully conforms to the \ref concepts::Digraph "Digraph concept".
  /// Most of its member functions and nested classes are documented
  /// only in the concept class.
  ///
  /// This class provides constant time counting for nodes and arcs.
  ///
  /// \note FullDigraph and FullGraph classes are very similar,
  /// but there are two differences. While this class conforms only
  /// to the \ref concepts::Digraph "Digraph" concept, FullGraph
  /// conforms to the \ref concepts::Graph "Graph" concept,
  /// moreover FullGraph does not contain a loop for each
  /// node as this class does.
  ///
  /// \sa FullGraph
  class FullDigraph : public ExtendedFullDigraphBase {
    typedef ExtendedFullDigraphBase Parent;

  public:

    /// \brief Default constructor.
    ///
    /// Default constructor. The number of nodes and arcs will be zero.
    FullDigraph() { construct(0); }

    /// \brief Constructor
    ///
    /// Constructor.
    /// \param n The number of the nodes.
    FullDigraph(int n) { construct(n); }

    /// \brief Resizes the digraph
    ///
    /// This function resizes the digraph. It fully destroys and
    /// rebuilds the structure, therefore the maps of the digraph will be
    /// reallocated automatically and the previous values will be lost.
    void resize(int n) {
      Parent::notifier(Arc()).clear();
      Parent::notifier(Node()).clear();
      construct(n);
      Parent::notifier(Node()).build();
      Parent::notifier(Arc()).build();
    }

    /// \brief Returns the node with the given index.
    ///
    /// Returns the node with the given index. Since this structure is
    /// completely static, the nodes can be indexed with integers from
    /// the range <tt>[0..nodeNum()-1]</tt>.
    /// The index of a node is the same as its ID.
    /// \sa index()
    Node operator()(int ix) const { return Parent::operator()(ix); }

    /// \brief Returns the index of the given node.
    ///
    /// Returns the index of the given node. Since this structure is
    /// completely static, the nodes can be indexed with integers from
    /// the range <tt>[0..nodeNum()-1]</tt>.
    /// The index of a node is the same as its ID.
    /// \sa operator()()
    static int index(const Node& node) { return Parent::index(node); }

    /// \brief Returns the arc connecting the given nodes.
    ///
    /// Returns the arc connecting the given nodes.
    Arc arc(Node u, Node v) const {
      return Parent::arc(u, v);
    }

    /// \brief Number of nodes.
    int nodeNum() const { return Parent::nodeNum(); }
    /// \brief Number of arcs.
    int arcNum() const { return Parent::arcNum(); }
  };


  class FullGraphBase {
  public:

    typedef FullGraphBase Graph;

    class Node;
    class Arc;
    class Edge;

  protected:

    int _node_num;
    int _edge_num;

    FullGraphBase() {}

    void construct(int n) { _node_num = n; _edge_num = n * (n - 1) / 2; }

    int _uid(int e) const {
      int u = e / _node_num;
      int v = e % _node_num;
      return u < v ? u : _node_num - 2 - u;
    }

    int _vid(int e) const {
      int u = e / _node_num;
      int v = e % _node_num;
      return u < v ? v : _node_num - 1 - v;
    }

    void _uvid(int e, int& u, int& v) const {
      u = e / _node_num;
      v = e % _node_num;
      if  (u >= v) {
        u = _node_num - 2 - u;
        v = _node_num - 1 - v;
      }
    }

    void _stid(int a, int& s, int& t) const {
      if ((a & 1) == 1) {
        _uvid(a >> 1, s, t);
      } else {
        _uvid(a >> 1, t, s);
      }
    }

    int _eid(int u, int v) const {
      if (u < (_node_num - 1) / 2) {
        return u * _node_num + v;
      } else {
        return (_node_num - 1 - u) * _node_num - v - 1;
      }
    }

  public:

    Node operator()(int ix) const { return Node(ix); }
    static int index(const Node& node) { return node._id; }

    Edge edge(const Node& u, const Node& v) const {
      if (u._id < v._id) {
        return Edge(_eid(u._id, v._id));
      } else if (u._id != v._id) {
        return Edge(_eid(v._id, u._id));
      } else {
        return INVALID;
      }
    }

    Arc arc(const Node& s, const Node& t) const {
      if (s._id < t._id) {
        return Arc((_eid(s._id, t._id) << 1) | 1);
      } else if (s._id != t._id) {
        return Arc(_eid(t._id, s._id) << 1);
      } else {
        return INVALID;
      }
    }

    typedef True NodeNumTag;
    typedef True ArcNumTag;
    typedef True EdgeNumTag;

    int nodeNum() const { return _node_num; }
    int arcNum() const { return 2 * _edge_num; }
    int edgeNum() const { return _edge_num; }

    static int id(Node node) { return node._id; }
    static int id(Arc arc) { return arc._id; }
    static int id(Edge edge) { return edge._id; }

    int maxNodeId() const { return _node_num-1; }
    int maxArcId() const { return 2 * _edge_num-1; }
    int maxEdgeId() const { return _edge_num-1; }

    static Node nodeFromId(int id) { return Node(id);}
    static Arc arcFromId(int id) { return Arc(id);}
    static Edge edgeFromId(int id) { return Edge(id);}

    Node u(Edge edge) const {
      return Node(_uid(edge._id));
    }

    Node v(Edge edge) const {
      return Node(_vid(edge._id));
    }

    Node source(Arc arc) const {
      return Node((arc._id & 1) == 1 ?
                  _uid(arc._id >> 1) : _vid(arc._id >> 1));
    }

    Node target(Arc arc) const {
      return Node((arc._id & 1) == 1 ?
                  _vid(arc._id >> 1) : _uid(arc._id >> 1));
    }

    typedef True FindEdgeTag;
    typedef True FindArcTag;

    Edge findEdge(Node u, Node v, Edge prev = INVALID) const {
      return prev != INVALID ? INVALID : edge(u, v);
    }

    Arc findArc(Node s, Node t, Arc prev = INVALID) const {
      return prev != INVALID ? INVALID : arc(s, t);
    }

    class Node {
      friend class FullGraphBase;

    protected:
      int _id;
      Node(int id) : _id(id) {}
    public:
      Node() {}
      Node (Invalid) { _id = -1; }
      bool operator==(const Node node) const {return _id == node._id;}
      bool operator!=(const Node node) const {return _id != node._id;}
      bool operator<(const Node node) const {return _id < node._id;}
    };

    class Edge {
      friend class FullGraphBase;
      friend class Arc;

    protected:
      int _id;

      Edge(int id) : _id(id) {}

    public:
      Edge() { }
      Edge (Invalid) { _id = -1; }

      bool operator==(const Edge edge) const {return _id == edge._id;}
      bool operator!=(const Edge edge) const {return _id != edge._id;}
      bool operator<(const Edge edge) const {return _id < edge._id;}
    };

    class Arc {
      friend class FullGraphBase;

    protected:
      int _id;

      Arc(int id) : _id(id) {}

    public:
      Arc() { }
      Arc (Invalid) { _id = -1; }

      operator Edge() const { return Edge(_id != -1 ? (_id >> 1) : -1); }

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

    void first(Arc& arc) const {
      arc._id = (_edge_num << 1) - 1;
    }

    static void next(Arc& arc) {
      --arc._id;
    }

    void first(Edge& edge) const {
      edge._id = _edge_num - 1;
    }

    static void next(Edge& edge) {
      --edge._id;
    }

    void firstOut(Arc& arc, const Node& node) const {
      int s = node._id, t = _node_num - 1;
      if (s < t) {
        arc._id = (_eid(s, t) << 1) | 1;
      } else {
        --t;
        arc._id = (t != -1 ? (_eid(t, s) << 1) : -1);
      }
    }

    void nextOut(Arc& arc) const {
      int s, t;
      _stid(arc._id, s, t);
      --t;
      if (s < t) {
        arc._id = (_eid(s, t) << 1) | 1;
      } else {
        if (s == t) --t;
        arc._id = (t != -1 ? (_eid(t, s) << 1) : -1);
      }
    }

    void firstIn(Arc& arc, const Node& node) const {
      int s = _node_num - 1, t = node._id;
      if (s > t) {
        arc._id = (_eid(t, s) << 1);
      } else {
        --s;
        arc._id = (s != -1 ? (_eid(s, t) << 1) | 1 : -1);
      }
    }

    void nextIn(Arc& arc) const {
      int s, t;
      _stid(arc._id, s, t);
      --s;
      if (s > t) {
        arc._id = (_eid(t, s) << 1);
      } else {
        if (s == t) --s;
        arc._id = (s != -1 ? (_eid(s, t) << 1) | 1 : -1);
      }
    }

    void firstInc(Edge& edge, bool& dir, const Node& node) const {
      int u = node._id, v = _node_num - 1;
      if (u < v) {
        edge._id = _eid(u, v);
        dir = true;
      } else {
        --v;
        edge._id = (v != -1 ? _eid(v, u) : -1);
        dir = false;
      }
    }

    void nextInc(Edge& edge, bool& dir) const {
      int u, v;
      if (dir) {
        _uvid(edge._id, u, v);
        --v;
        if (u < v) {
          edge._id = _eid(u, v);
        } else {
          --v;
          edge._id = (v != -1 ? _eid(v, u) : -1);
          dir = false;
        }
      } else {
        _uvid(edge._id, v, u);
        --v;
        edge._id = (v != -1 ? _eid(v, u) : -1);
      }
    }

  };

  typedef GraphExtender<FullGraphBase> ExtendedFullGraphBase;

  /// \ingroup graphs
  ///
  /// \brief An undirected full graph class.
  ///
  /// FullGraph is a simple and fast implmenetation of undirected full
  /// (complete) graphs. It contains an edge between every distinct pair
  /// of nodes, therefore the number of edges is <tt>n(n-1)/2</tt>.
  /// This class is completely static and it needs constant memory space.
  /// Thus you can neither add nor delete nodes or edges, however
  /// the structure can be resized using resize().
  ///
  /// This type fully conforms to the \ref concepts::Graph "Graph concept".
  /// Most of its member functions and nested classes are documented
  /// only in the concept class.
  ///
  /// This class provides constant time counting for nodes, edges and arcs.
  ///
  /// \note FullDigraph and FullGraph classes are very similar,
  /// but there are two differences. While FullDigraph
  /// conforms only to the \ref concepts::Digraph "Digraph" concept,
  /// this class conforms to the \ref concepts::Graph "Graph" concept,
  /// moreover this class does not contain a loop for each
  /// node as FullDigraph does.
  ///
  /// \sa FullDigraph
  class FullGraph : public ExtendedFullGraphBase {
    typedef ExtendedFullGraphBase Parent;

  public:

    /// \brief Default constructor.
    ///
    /// Default constructor. The number of nodes and edges will be zero.
    FullGraph() { construct(0); }

    /// \brief Constructor
    ///
    /// Constructor.
    /// \param n The number of the nodes.
    FullGraph(int n) { construct(n); }

    /// \brief Resizes the graph
    ///
    /// This function resizes the graph. It fully destroys and
    /// rebuilds the structure, therefore the maps of the graph will be
    /// reallocated automatically and the previous values will be lost.
    void resize(int n) {
      Parent::notifier(Arc()).clear();
      Parent::notifier(Edge()).clear();
      Parent::notifier(Node()).clear();
      construct(n);
      Parent::notifier(Node()).build();
      Parent::notifier(Edge()).build();
      Parent::notifier(Arc()).build();
    }

    /// \brief Returns the node with the given index.
    ///
    /// Returns the node with the given index. Since this structure is
    /// completely static, the nodes can be indexed with integers from
    /// the range <tt>[0..nodeNum()-1]</tt>.
    /// The index of a node is the same as its ID.
    /// \sa index()
    Node operator()(int ix) const { return Parent::operator()(ix); }

    /// \brief Returns the index of the given node.
    ///
    /// Returns the index of the given node. Since this structure is
    /// completely static, the nodes can be indexed with integers from
    /// the range <tt>[0..nodeNum()-1]</tt>.
    /// The index of a node is the same as its ID.
    /// \sa operator()()
    static int index(const Node& node) { return Parent::index(node); }

    /// \brief Returns the arc connecting the given nodes.
    ///
    /// Returns the arc connecting the given nodes.
    Arc arc(Node s, Node t) const {
      return Parent::arc(s, t);
    }

    /// \brief Returns the edge connecting the given nodes.
    ///
    /// Returns the edge connecting the given nodes.
    Edge edge(Node u, Node v) const {
      return Parent::edge(u, v);
    }

    /// \brief Number of nodes.
    int nodeNum() const { return Parent::nodeNum(); }
    /// \brief Number of arcs.
    int arcNum() const { return Parent::arcNum(); }
    /// \brief Number of edges.
    int edgeNum() const { return Parent::edgeNum(); }

  };

  class FullBpGraphBase {

  protected:

    int _red_num, _blue_num;
    int _node_num, _edge_num;

  public:

    typedef FullBpGraphBase Graph;

    class Node;
    class Arc;
    class Edge;

    class Node {
      friend class FullBpGraphBase;
    protected:

      int _id;
      explicit Node(int id) { _id = id;}

    public:
      Node() {}
      Node (Invalid) { _id = -1; }
      bool operator==(const Node& node) const {return _id == node._id;}
      bool operator!=(const Node& node) const {return _id != node._id;}
      bool operator<(const Node& node) const {return _id < node._id;}
    };

    class RedNode : public Node {
      friend class FullBpGraphBase;
    protected:

      explicit RedNode(int pid) : Node(pid) {}

    public:
      RedNode() {}
      RedNode(const RedNode& node) : Node(node) {}
      RedNode(Invalid) : Node(INVALID){}
    };

    class BlueNode : public Node {
      friend class FullBpGraphBase;
    protected:

      explicit BlueNode(int pid) : Node(pid) {}

    public:
      BlueNode() {}
      BlueNode(const BlueNode& node) : Node(node) {}
      BlueNode(Invalid) : Node(INVALID){}
    };

    class Edge {
      friend class FullBpGraphBase;
    protected:

      int _id;
      explicit Edge(int id) { _id = id;}

    public:
      Edge() {}
      Edge (Invalid) { _id = -1; }
      bool operator==(const Edge& arc) const {return _id == arc._id;}
      bool operator!=(const Edge& arc) const {return _id != arc._id;}
      bool operator<(const Edge& arc) const {return _id < arc._id;}
    };

    class Arc {
      friend class FullBpGraphBase;
    protected:

      int _id;
      explicit Arc(int id) { _id = id;}

    public:
      operator Edge() const {
        return _id != -1 ? edgeFromId(_id / 2) : INVALID;
      }

      Arc() {}
      Arc (Invalid) { _id = -1; }
      bool operator==(const Arc& arc) const {return _id == arc._id;}
      bool operator!=(const Arc& arc) const {return _id != arc._id;}
      bool operator<(const Arc& arc) const {return _id < arc._id;}
    };


  protected:

    FullBpGraphBase()
      : _red_num(0), _blue_num(0), _node_num(0), _edge_num(0) {}

    void construct(int redNum, int blueNum) {
      _red_num = redNum; _blue_num = blueNum;
      _node_num = redNum + blueNum; _edge_num = redNum * blueNum;
    }

  public:

    typedef True NodeNumTag;
    typedef True EdgeNumTag;
    typedef True ArcNumTag;

    int nodeNum() const { return _node_num; }
    int redNum() const { return _red_num; }
    int blueNum() const { return _blue_num; }
    int edgeNum() const { return _edge_num; }
    int arcNum() const { return 2 * _edge_num; }

    int maxNodeId() const { return _node_num - 1; }
    int maxRedId() const { return _red_num - 1; }
    int maxBlueId() const { return _blue_num - 1; }
    int maxEdgeId() const { return _edge_num - 1; }
    int maxArcId() const { return 2 * _edge_num - 1; }

    bool red(Node n) const { return n._id < _red_num; }
    bool blue(Node n) const { return n._id >= _red_num; }

    static RedNode asRedNodeUnsafe(Node n) { return RedNode(n._id); }
    static BlueNode asBlueNodeUnsafe(Node n) { return BlueNode(n._id); }

    Node source(Arc a) const {
      if (a._id & 1) {
        return Node((a._id >> 1) % _red_num);
      } else {
        return Node((a._id >> 1) / _red_num + _red_num);
      }
    }
    Node target(Arc a) const {
      if (a._id & 1) {
        return Node((a._id >> 1) / _red_num + _red_num);
      } else {
        return Node((a._id >> 1) % _red_num);
      }
    }

    RedNode redNode(Edge e) const {
      return RedNode(e._id % _red_num);
    }
    BlueNode blueNode(Edge e) const {
      return BlueNode(e._id / _red_num + _red_num);
    }

    static bool direction(Arc a) {
      return (a._id & 1) == 1;
    }

    static Arc direct(Edge e, bool d) {
      return Arc(e._id * 2 + (d ? 1 : 0));
    }

    void first(Node& node) const {
      node._id = _node_num - 1;
    }

    static void next(Node& node) {
      --node._id;
    }

    void first(RedNode& node) const {
      node._id = _red_num - 1;
    }

    static void next(RedNode& node) {
      --node._id;
    }

    void first(BlueNode& node) const {
      if (_red_num == _node_num) node._id = -1;
      else node._id = _node_num - 1;
    }

    void next(BlueNode& node) const {
      if (node._id == _red_num) node._id = -1;
      else --node._id;
    }

    void first(Arc& arc) const {
      arc._id = 2 * _edge_num - 1;
    }

    static void next(Arc& arc) {
      --arc._id;
    }

    void first(Edge& arc) const {
      arc._id = _edge_num - 1;
    }

    static void next(Edge& arc) {
      --arc._id;
    }

    void firstOut(Arc &a, const Node& v) const {
      if (v._id < _red_num) {
        a._id = 2 * (v._id + _red_num * (_blue_num - 1)) + 1;
      } else {
        a._id = 2 * (_red_num - 1 + _red_num * (v._id - _red_num));
      }
    }
    void nextOut(Arc &a) const {
      if (a._id & 1) {
        a._id -= 2 * _red_num;
        if (a._id < 0) a._id = -1;
      } else {
        if (a._id % (2 * _red_num) == 0) a._id = -1;
        else a._id -= 2;
      }
    }

    void firstIn(Arc &a, const Node& v) const {
      if (v._id < _red_num) {
        a._id = 2 * (v._id + _red_num * (_blue_num - 1));
      } else {
        a._id = 2 * (_red_num - 1 + _red_num * (v._id - _red_num)) + 1;
      }
    }
    void nextIn(Arc &a) const {
      if (a._id & 1) {
        if (a._id % (2 * _red_num) == 1) a._id = -1;
        else a._id -= 2;
      } else {
        a._id -= 2 * _red_num;
        if (a._id < 0) a._id = -1;
      }
    }

    void firstInc(Edge &e, bool& d, const Node& v) const {
      if (v._id < _red_num) {
        d = true;
        e._id = v._id + _red_num * (_blue_num - 1);
      } else {
        d = false;
        e._id = _red_num - 1 + _red_num * (v._id - _red_num);
      }
    }
    void nextInc(Edge &e, bool& d) const {
      if (d) {
        e._id -= _red_num;
        if (e._id < 0) e._id = -1;
      } else {
        if (e._id % _red_num == 0) e._id = -1;
        else --e._id;
      }
    }

    static int id(const Node& v) { return v._id; }
    int id(const RedNode& v) const { return v._id; }
    int id(const BlueNode& v) const { return v._id - _red_num; }
    static int id(Arc e) { return e._id; }
    static int id(Edge e) { return e._id; }

    static Node nodeFromId(int id) { return Node(id);}
    static Arc arcFromId(int id) { return Arc(id);}
    static Edge edgeFromId(int id) { return Edge(id);}

    bool valid(Node n) const {
      return n._id >= 0 && n._id < _node_num;
    }
    bool valid(Arc a) const {
      return a._id >= 0 && a._id < 2 * _edge_num;
    }
    bool valid(Edge e) const {
      return e._id >= 0 && e._id < _edge_num;
    }

    RedNode redNode(int index) const {
      return RedNode(index);
    }

    int index(RedNode n) const {
      return n._id;
    }

    BlueNode blueNode(int index) const {
      return BlueNode(index + _red_num);
    }

    int index(BlueNode n) const {
      return n._id - _red_num;
    }

    void clear() {
      _red_num = 0; _blue_num = 0;
      _node_num = 0; _edge_num = 0;
    }

    Edge edge(const Node& u, const Node& v) const {
      if (u._id < _red_num) {
        if (v._id < _red_num) {
          return Edge(-1);
        } else {
          return Edge(u._id + _red_num * (v._id - _red_num));
        }
      } else {
        if (v._id < _red_num) {
          return Edge(v._id + _red_num * (u._id - _red_num));
        } else {
          return Edge(-1);
        }
      }
    }

    Arc arc(const Node& u, const Node& v) const {
      if (u._id < _red_num) {
        if (v._id < _red_num) {
          return Arc(-1);
        } else {
          return Arc(2 * (u._id + _red_num * (v._id - _red_num)) + 1);
        }
      } else {
        if (v._id < _red_num) {
          return Arc(2 * (v._id + _red_num * (u._id - _red_num)));
        } else {
          return Arc(-1);
        }
      }
    }

    typedef True FindEdgeTag;
    typedef True FindArcTag;

    Edge findEdge(Node u, Node v, Edge prev = INVALID) const {
      return prev != INVALID ? INVALID : edge(u, v);
    }

    Arc findArc(Node s, Node t, Arc prev = INVALID) const {
      return prev != INVALID ? INVALID : arc(s, t);
    }

  };

  typedef BpGraphExtender<FullBpGraphBase> ExtendedFullBpGraphBase;

  /// \ingroup graphs
  ///
  /// \brief An undirected full bipartite graph class.
  ///
  /// FullBpGraph is a simple and fast implmenetation of undirected
  /// full bipartite graphs. It contains an edge between every
  /// red-blue pairs of nodes, therefore the number of edges is
  /// <tt>nr*nb</tt>.  This class is completely static and it needs
  /// constant memory space.  Thus you can neither add nor delete
  /// nodes or edges, however the structure can be resized using
  /// resize().
  ///
  /// This type fully conforms to the \ref concepts::BpGraph "BpGraph concept".
  /// Most of its member functions and nested classes are documented
  /// only in the concept class.
  ///
  /// This class provides constant time counting for nodes, edges and arcs.
  ///
  /// \sa FullGraph
  class FullBpGraph : public ExtendedFullBpGraphBase {
  public:

    typedef ExtendedFullBpGraphBase Parent;

    /// \brief Default constructor.
    ///
    /// Default constructor. The number of nodes and edges will be zero.
    FullBpGraph() { construct(0, 0); }

    /// \brief Constructor
    ///
    /// Constructor.
    /// \param redNum The number of the red nodes.
    /// \param blueNum The number of the blue nodes.
    FullBpGraph(int redNum, int blueNum) { construct(redNum, blueNum); }

    /// \brief Resizes the graph
    ///
    /// This function resizes the graph. It fully destroys and
    /// rebuilds the structure, therefore the maps of the graph will be
    /// reallocated automatically and the previous values will be lost.
    void resize(int redNum, int blueNum) {
      Parent::notifier(Arc()).clear();
      Parent::notifier(Edge()).clear();
      Parent::notifier(Node()).clear();
      Parent::notifier(BlueNode()).clear();
      Parent::notifier(RedNode()).clear();
      construct(redNum, blueNum);
      Parent::notifier(RedNode()).build();
      Parent::notifier(BlueNode()).build();
      Parent::notifier(Node()).build();
      Parent::notifier(Edge()).build();
      Parent::notifier(Arc()).build();
    }

    using Parent::redNode;
    using Parent::blueNode;

    /// \brief Returns the red node with the given index.
    ///
    /// Returns the red node with the given index. Since this
    /// structure is completely static, the red nodes can be indexed
    /// with integers from the range <tt>[0..redNum()-1]</tt>.
    /// \sa redIndex()
    RedNode redNode(int index) const { return Parent::redNode(index); }

    /// \brief Returns the index of the given red node.
    ///
    /// Returns the index of the given red node. Since this structure
    /// is completely static, the red nodes can be indexed with
    /// integers from the range <tt>[0..redNum()-1]</tt>.
    ///
    /// \sa operator()()
    int index(RedNode node) const { return Parent::index(node); }

    /// \brief Returns the blue node with the given index.
    ///
    /// Returns the blue node with the given index. Since this
    /// structure is completely static, the blue nodes can be indexed
    /// with integers from the range <tt>[0..blueNum()-1]</tt>.
    /// \sa blueIndex()
    BlueNode blueNode(int index) const { return Parent::blueNode(index); }

    /// \brief Returns the index of the given blue node.
    ///
    /// Returns the index of the given blue node. Since this structure
    /// is completely static, the blue nodes can be indexed with
    /// integers from the range <tt>[0..blueNum()-1]</tt>.
    ///
    /// \sa operator()()
    int index(BlueNode node) const { return Parent::index(node); }

    /// \brief Returns the edge which connects the given nodes.
    ///
    /// Returns the edge which connects the given nodes.
    Edge edge(const Node& u, const Node& v) const {
      return Parent::edge(u, v);
    }

    /// \brief Returns the arc which connects the given nodes.
    ///
    /// Returns the arc which connects the given nodes.
    Arc arc(const Node& u, const Node& v) const {
      return Parent::arc(u, v);
    }

    /// \brief Number of nodes.
    int nodeNum() const { return Parent::nodeNum(); }
    /// \brief Number of red nodes.
    int redNum() const { return Parent::redNum(); }
    /// \brief Number of blue nodes.
    int blueNum() const { return Parent::blueNum(); }
    /// \brief Number of arcs.
    int arcNum() const { return Parent::arcNum(); }
    /// \brief Number of edges.
    int edgeNum() const { return Parent::edgeNum(); }
  };


} //namespace lemon


#endif //LEMON_FULL_GRAPH_H
