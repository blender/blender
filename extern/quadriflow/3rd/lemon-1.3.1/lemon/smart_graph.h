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

#ifndef LEMON_SMART_GRAPH_H
#define LEMON_SMART_GRAPH_H

///\ingroup graphs
///\file
///\brief SmartDigraph and SmartGraph classes.

#include <vector>

#include <lemon/core.h>
#include <lemon/error.h>
#include <lemon/bits/graph_extender.h>

namespace lemon {

  class SmartDigraph;

  class SmartDigraphBase {
  protected:

    struct NodeT
    {
      int first_in, first_out;
      NodeT() {}
    };
    struct ArcT
    {
      int target, source, next_in, next_out;
      ArcT() {}
    };

    std::vector<NodeT> nodes;
    std::vector<ArcT> arcs;

  public:

    typedef SmartDigraphBase Digraph;

    class Node;
    class Arc;

  public:

    SmartDigraphBase() : nodes(), arcs() { }
    SmartDigraphBase(const SmartDigraphBase &_g)
      : nodes(_g.nodes), arcs(_g.arcs) { }

    typedef True NodeNumTag;
    typedef True ArcNumTag;

    int nodeNum() const { return nodes.size(); }
    int arcNum() const { return arcs.size(); }

    int maxNodeId() const { return nodes.size()-1; }
    int maxArcId() const { return arcs.size()-1; }

    Node addNode() {
      int n = nodes.size();
      nodes.push_back(NodeT());
      nodes[n].first_in = -1;
      nodes[n].first_out = -1;
      return Node(n);
    }

    Arc addArc(Node u, Node v) {
      int n = arcs.size();
      arcs.push_back(ArcT());
      arcs[n].source = u._id;
      arcs[n].target = v._id;
      arcs[n].next_out = nodes[u._id].first_out;
      arcs[n].next_in = nodes[v._id].first_in;
      nodes[u._id].first_out = nodes[v._id].first_in = n;

      return Arc(n);
    }

    void clear() {
      arcs.clear();
      nodes.clear();
    }

    Node source(Arc a) const { return Node(arcs[a._id].source); }
    Node target(Arc a) const { return Node(arcs[a._id].target); }

    static int id(Node v) { return v._id; }
    static int id(Arc a) { return a._id; }

    static Node nodeFromId(int id) { return Node(id);}
    static Arc arcFromId(int id) { return Arc(id);}

    bool valid(Node n) const {
      return n._id >= 0 && n._id < static_cast<int>(nodes.size());
    }
    bool valid(Arc a) const {
      return a._id >= 0 && a._id < static_cast<int>(arcs.size());
    }

    class Node {
      friend class SmartDigraphBase;
      friend class SmartDigraph;

    protected:
      int _id;
      explicit Node(int id) : _id(id) {}
    public:
      Node() {}
      Node (Invalid) : _id(-1) {}
      bool operator==(const Node i) const {return _id == i._id;}
      bool operator!=(const Node i) const {return _id != i._id;}
      bool operator<(const Node i) const {return _id < i._id;}
    };


    class Arc {
      friend class SmartDigraphBase;
      friend class SmartDigraph;

    protected:
      int _id;
      explicit Arc(int id) : _id(id) {}
    public:
      Arc() { }
      Arc (Invalid) : _id(-1) {}
      bool operator==(const Arc i) const {return _id == i._id;}
      bool operator!=(const Arc i) const {return _id != i._id;}
      bool operator<(const Arc i) const {return _id < i._id;}
    };

    void first(Node& node) const {
      node._id = nodes.size() - 1;
    }

    static void next(Node& node) {
      --node._id;
    }

    void first(Arc& arc) const {
      arc._id = arcs.size() - 1;
    }

    static void next(Arc& arc) {
      --arc._id;
    }

    void firstOut(Arc& arc, const Node& node) const {
      arc._id = nodes[node._id].first_out;
    }

    void nextOut(Arc& arc) const {
      arc._id = arcs[arc._id].next_out;
    }

    void firstIn(Arc& arc, const Node& node) const {
      arc._id = nodes[node._id].first_in;
    }

    void nextIn(Arc& arc) const {
      arc._id = arcs[arc._id].next_in;
    }

  };

  typedef DigraphExtender<SmartDigraphBase> ExtendedSmartDigraphBase;

  ///\ingroup graphs
  ///
  ///\brief A smart directed graph class.
  ///
  ///\ref SmartDigraph is a simple and fast digraph implementation.
  ///It is also quite memory efficient but at the price
  ///that it does not support node and arc deletion
  ///(except for the Snapshot feature).
  ///
  ///This type fully conforms to the \ref concepts::Digraph "Digraph concept"
  ///and it also provides some additional functionalities.
  ///Most of its member functions and nested classes are documented
  ///only in the concept class.
  ///
  ///This class provides constant time counting for nodes and arcs.
  ///
  ///\sa concepts::Digraph
  ///\sa SmartGraph
  class SmartDigraph : public ExtendedSmartDigraphBase {
    typedef ExtendedSmartDigraphBase Parent;

  private:
    /// Digraphs are \e not copy constructible. Use DigraphCopy instead.
    SmartDigraph(const SmartDigraph &) : ExtendedSmartDigraphBase() {};
    /// \brief Assignment of a digraph to another one is \e not allowed.
    /// Use DigraphCopy instead.
    void operator=(const SmartDigraph &) {}

  public:

    /// Constructor

    /// Constructor.
    ///
    SmartDigraph() {};

    ///Add a new node to the digraph.

    ///This function adds a new node to the digraph.
    ///\return The new node.
    Node addNode() { return Parent::addNode(); }

    ///Add a new arc to the digraph.

    ///This function adds a new arc to the digraph with source node \c s
    ///and target node \c t.
    ///\return The new arc.
    Arc addArc(Node s, Node t) {
      return Parent::addArc(s, t);
    }

    /// \brief Node validity check
    ///
    /// This function gives back \c true if the given node is valid,
    /// i.e. it is a real node of the digraph.
    ///
    /// \warning A removed node (using Snapshot) could become valid again
    /// if new nodes are added to the digraph.
    bool valid(Node n) const { return Parent::valid(n); }

    /// \brief Arc validity check
    ///
    /// This function gives back \c true if the given arc is valid,
    /// i.e. it is a real arc of the digraph.
    ///
    /// \warning A removed arc (using Snapshot) could become valid again
    /// if new arcs are added to the graph.
    bool valid(Arc a) const { return Parent::valid(a); }

    ///Split a node.

    ///This function splits the given node. First, a new node is added
    ///to the digraph, then the source of each outgoing arc of node \c n
    ///is moved to this new node.
    ///If the second parameter \c connect is \c true (this is the default
    ///value), then a new arc from node \c n to the newly created node
    ///is also added.
    ///\return The newly created node.
    ///
    ///\note All iterators remain valid.
    ///
    ///\warning This functionality cannot be used together with the Snapshot
    ///feature.
    Node split(Node n, bool connect = true)
    {
      Node b = addNode();
      nodes[b._id].first_out=nodes[n._id].first_out;
      nodes[n._id].first_out=-1;
      for(int i=nodes[b._id].first_out; i!=-1; i=arcs[i].next_out) {
        arcs[i].source=b._id;
      }
      if(connect) addArc(n,b);
      return b;
    }

    ///Clear the digraph.

    ///This function erases all nodes and arcs from the digraph.
    ///
    void clear() {
      Parent::clear();
    }

    /// Reserve memory for nodes.

    /// Using this function, it is possible to avoid superfluous memory
    /// allocation: if you know that the digraph you want to build will
    /// be large (e.g. it will contain millions of nodes and/or arcs),
    /// then it is worth reserving space for this amount before starting
    /// to build the digraph.
    /// \sa reserveArc()
    void reserveNode(int n) { nodes.reserve(n); };

    /// Reserve memory for arcs.

    /// Using this function, it is possible to avoid superfluous memory
    /// allocation: if you know that the digraph you want to build will
    /// be large (e.g. it will contain millions of nodes and/or arcs),
    /// then it is worth reserving space for this amount before starting
    /// to build the digraph.
    /// \sa reserveNode()
    void reserveArc(int m) { arcs.reserve(m); };

  public:

    class Snapshot;

  protected:

    void restoreSnapshot(const Snapshot &s)
    {
      while(s.arc_num<arcs.size()) {
        Arc arc = arcFromId(arcs.size()-1);
        Parent::notifier(Arc()).erase(arc);
        nodes[arcs.back().source].first_out=arcs.back().next_out;
        nodes[arcs.back().target].first_in=arcs.back().next_in;
        arcs.pop_back();
      }
      while(s.node_num<nodes.size()) {
        Node node = nodeFromId(nodes.size()-1);
        Parent::notifier(Node()).erase(node);
        nodes.pop_back();
      }
    }

  public:

    ///Class to make a snapshot of the digraph and to restore it later.

    ///Class to make a snapshot of the digraph and to restore it later.
    ///
    ///The newly added nodes and arcs can be removed using the
    ///restore() function. This is the only way for deleting nodes and/or
    ///arcs from a SmartDigraph structure.
    ///
    ///\note After a state is restored, you cannot restore a later state,
    ///i.e. you cannot add the removed nodes and arcs again using
    ///another Snapshot instance.
    ///
    ///\warning Node splitting cannot be restored.
    ///\warning The validity of the snapshot is not stored due to
    ///performance reasons. If you do not use the snapshot correctly,
    ///it can cause broken program, invalid or not restored state of
    ///the digraph or no change.
    class Snapshot
    {
      SmartDigraph *_graph;
    protected:
      friend class SmartDigraph;
      unsigned int node_num;
      unsigned int arc_num;
    public:
      ///Default constructor.

      ///Default constructor.
      ///You have to call save() to actually make a snapshot.
      Snapshot() : _graph(0) {}
      ///Constructor that immediately makes a snapshot

      ///This constructor immediately makes a snapshot of the given digraph.
      ///
      Snapshot(SmartDigraph &gr) : _graph(&gr) {
        node_num=_graph->nodes.size();
        arc_num=_graph->arcs.size();
      }

      ///Make a snapshot.

      ///This function makes a snapshot of the given digraph.
      ///It can be called more than once. In case of a repeated
      ///call, the previous snapshot gets lost.
      void save(SmartDigraph &gr) {
        _graph=&gr;
        node_num=_graph->nodes.size();
        arc_num=_graph->arcs.size();
      }

      ///Undo the changes until a snapshot.

      ///This function undos the changes until the last snapshot
      ///created by save() or Snapshot(SmartDigraph&).
      void restore()
      {
        _graph->restoreSnapshot(*this);
      }
    };
  };


  class SmartGraphBase {

  protected:

    struct NodeT {
      int first_out;
    };

    struct ArcT {
      int target;
      int next_out;
    };

    std::vector<NodeT> nodes;
    std::vector<ArcT> arcs;

  public:

    typedef SmartGraphBase Graph;

    class Node;
    class Arc;
    class Edge;

    class Node {
      friend class SmartGraphBase;
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

    class Edge {
      friend class SmartGraphBase;
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
      friend class SmartGraphBase;
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



    SmartGraphBase()
      : nodes(), arcs() {}

    typedef True NodeNumTag;
    typedef True EdgeNumTag;
    typedef True ArcNumTag;

    int nodeNum() const { return nodes.size(); }
    int edgeNum() const { return arcs.size() / 2; }
    int arcNum() const { return arcs.size(); }

    int maxNodeId() const { return nodes.size()-1; }
    int maxEdgeId() const { return arcs.size() / 2 - 1; }
    int maxArcId() const { return arcs.size()-1; }

    Node source(Arc e) const { return Node(arcs[e._id ^ 1].target); }
    Node target(Arc e) const { return Node(arcs[e._id].target); }

    Node u(Edge e) const { return Node(arcs[2 * e._id].target); }
    Node v(Edge e) const { return Node(arcs[2 * e._id + 1].target); }

    static bool direction(Arc e) {
      return (e._id & 1) == 1;
    }

    static Arc direct(Edge e, bool d) {
      return Arc(e._id * 2 + (d ? 1 : 0));
    }

    void first(Node& node) const {
      node._id = nodes.size() - 1;
    }

    static void next(Node& node) {
      --node._id;
    }

    void first(Arc& arc) const {
      arc._id = arcs.size() - 1;
    }

    static void next(Arc& arc) {
      --arc._id;
    }

    void first(Edge& arc) const {
      arc._id = arcs.size() / 2 - 1;
    }

    static void next(Edge& arc) {
      --arc._id;
    }

    void firstOut(Arc &arc, const Node& v) const {
      arc._id = nodes[v._id].first_out;
    }
    void nextOut(Arc &arc) const {
      arc._id = arcs[arc._id].next_out;
    }

    void firstIn(Arc &arc, const Node& v) const {
      arc._id = ((nodes[v._id].first_out) ^ 1);
      if (arc._id == -2) arc._id = -1;
    }
    void nextIn(Arc &arc) const {
      arc._id = ((arcs[arc._id ^ 1].next_out) ^ 1);
      if (arc._id == -2) arc._id = -1;
    }

    void firstInc(Edge &arc, bool& d, const Node& v) const {
      int de = nodes[v._id].first_out;
      if (de != -1) {
        arc._id = de / 2;
        d = ((de & 1) == 1);
      } else {
        arc._id = -1;
        d = true;
      }
    }
    void nextInc(Edge &arc, bool& d) const {
      int de = (arcs[(arc._id * 2) | (d ? 1 : 0)].next_out);
      if (de != -1) {
        arc._id = de / 2;
        d = ((de & 1) == 1);
      } else {
        arc._id = -1;
        d = true;
      }
    }

    static int id(Node v) { return v._id; }
    static int id(Arc e) { return e._id; }
    static int id(Edge e) { return e._id; }

    static Node nodeFromId(int id) { return Node(id);}
    static Arc arcFromId(int id) { return Arc(id);}
    static Edge edgeFromId(int id) { return Edge(id);}

    bool valid(Node n) const {
      return n._id >= 0 && n._id < static_cast<int>(nodes.size());
    }
    bool valid(Arc a) const {
      return a._id >= 0 && a._id < static_cast<int>(arcs.size());
    }
    bool valid(Edge e) const {
      return e._id >= 0 && 2 * e._id < static_cast<int>(arcs.size());
    }

    Node addNode() {
      int n = nodes.size();
      nodes.push_back(NodeT());
      nodes[n].first_out = -1;

      return Node(n);
    }

    Edge addEdge(Node u, Node v) {
      int n = arcs.size();
      arcs.push_back(ArcT());
      arcs.push_back(ArcT());

      arcs[n].target = u._id;
      arcs[n | 1].target = v._id;

      arcs[n].next_out = nodes[v._id].first_out;
      nodes[v._id].first_out = n;

      arcs[n | 1].next_out = nodes[u._id].first_out;
      nodes[u._id].first_out = (n | 1);

      return Edge(n / 2);
    }

    void clear() {
      arcs.clear();
      nodes.clear();
    }

  };

  typedef GraphExtender<SmartGraphBase> ExtendedSmartGraphBase;

  /// \ingroup graphs
  ///
  /// \brief A smart undirected graph class.
  ///
  /// \ref SmartGraph is a simple and fast graph implementation.
  /// It is also quite memory efficient but at the price
  /// that it does not support node and edge deletion
  /// (except for the Snapshot feature).
  ///
  /// This type fully conforms to the \ref concepts::Graph "Graph concept"
  /// and it also provides some additional functionalities.
  /// Most of its member functions and nested classes are documented
  /// only in the concept class.
  ///
  /// This class provides constant time counting for nodes, edges and arcs.
  ///
  /// \sa concepts::Graph
  /// \sa SmartDigraph
  class SmartGraph : public ExtendedSmartGraphBase {
    typedef ExtendedSmartGraphBase Parent;

  private:
    /// Graphs are \e not copy constructible. Use GraphCopy instead.
    SmartGraph(const SmartGraph &) : ExtendedSmartGraphBase() {};
    /// \brief Assignment of a graph to another one is \e not allowed.
    /// Use GraphCopy instead.
    void operator=(const SmartGraph &) {}

  public:

    /// Constructor

    /// Constructor.
    ///
    SmartGraph() {}

    /// \brief Add a new node to the graph.
    ///
    /// This function adds a new node to the graph.
    /// \return The new node.
    Node addNode() { return Parent::addNode(); }

    /// \brief Add a new edge to the graph.
    ///
    /// This function adds a new edge to the graph between nodes
    /// \c u and \c v with inherent orientation from node \c u to
    /// node \c v.
    /// \return The new edge.
    Edge addEdge(Node u, Node v) {
      return Parent::addEdge(u, v);
    }

    /// \brief Node validity check
    ///
    /// This function gives back \c true if the given node is valid,
    /// i.e. it is a real node of the graph.
    ///
    /// \warning A removed node (using Snapshot) could become valid again
    /// if new nodes are added to the graph.
    bool valid(Node n) const { return Parent::valid(n); }

    /// \brief Edge validity check
    ///
    /// This function gives back \c true if the given edge is valid,
    /// i.e. it is a real edge of the graph.
    ///
    /// \warning A removed edge (using Snapshot) could become valid again
    /// if new edges are added to the graph.
    bool valid(Edge e) const { return Parent::valid(e); }

    /// \brief Arc validity check
    ///
    /// This function gives back \c true if the given arc is valid,
    /// i.e. it is a real arc of the graph.
    ///
    /// \warning A removed arc (using Snapshot) could become valid again
    /// if new edges are added to the graph.
    bool valid(Arc a) const { return Parent::valid(a); }

    ///Clear the graph.

    ///This function erases all nodes and arcs from the graph.
    ///
    void clear() {
      Parent::clear();
    }

    /// Reserve memory for nodes.

    /// Using this function, it is possible to avoid superfluous memory
    /// allocation: if you know that the graph you want to build will
    /// be large (e.g. it will contain millions of nodes and/or edges),
    /// then it is worth reserving space for this amount before starting
    /// to build the graph.
    /// \sa reserveEdge()
    void reserveNode(int n) { nodes.reserve(n); };

    /// Reserve memory for edges.

    /// Using this function, it is possible to avoid superfluous memory
    /// allocation: if you know that the graph you want to build will
    /// be large (e.g. it will contain millions of nodes and/or edges),
    /// then it is worth reserving space for this amount before starting
    /// to build the graph.
    /// \sa reserveNode()
    void reserveEdge(int m) { arcs.reserve(2 * m); };

  public:

    class Snapshot;

  protected:

    void saveSnapshot(Snapshot &s)
    {
      s._graph = this;
      s.node_num = nodes.size();
      s.arc_num = arcs.size();
    }

    void restoreSnapshot(const Snapshot &s)
    {
      while(s.arc_num<arcs.size()) {
        int n=arcs.size()-1;
        Edge arc=edgeFromId(n/2);
        Parent::notifier(Edge()).erase(arc);
        std::vector<Arc> dir;
        dir.push_back(arcFromId(n));
        dir.push_back(arcFromId(n-1));
        Parent::notifier(Arc()).erase(dir);
        nodes[arcs[n-1].target].first_out=arcs[n].next_out;
        nodes[arcs[n].target].first_out=arcs[n-1].next_out;
        arcs.pop_back();
        arcs.pop_back();
      }
      while(s.node_num<nodes.size()) {
        int n=nodes.size()-1;
        Node node = nodeFromId(n);
        Parent::notifier(Node()).erase(node);
        nodes.pop_back();
      }
    }

  public:

    ///Class to make a snapshot of the graph and to restore it later.

    ///Class to make a snapshot of the graph and to restore it later.
    ///
    ///The newly added nodes and edges can be removed using the
    ///restore() function. This is the only way for deleting nodes and/or
    ///edges from a SmartGraph structure.
    ///
    ///\note After a state is restored, you cannot restore a later state,
    ///i.e. you cannot add the removed nodes and edges again using
    ///another Snapshot instance.
    ///
    ///\warning The validity of the snapshot is not stored due to
    ///performance reasons. If you do not use the snapshot correctly,
    ///it can cause broken program, invalid or not restored state of
    ///the graph or no change.
    class Snapshot
    {
      SmartGraph *_graph;
    protected:
      friend class SmartGraph;
      unsigned int node_num;
      unsigned int arc_num;
    public:
      ///Default constructor.

      ///Default constructor.
      ///You have to call save() to actually make a snapshot.
      Snapshot() : _graph(0) {}
      ///Constructor that immediately makes a snapshot

      /// This constructor immediately makes a snapshot of the given graph.
      ///
      Snapshot(SmartGraph &gr) {
        gr.saveSnapshot(*this);
      }

      ///Make a snapshot.

      ///This function makes a snapshot of the given graph.
      ///It can be called more than once. In case of a repeated
      ///call, the previous snapshot gets lost.
      void save(SmartGraph &gr)
      {
        gr.saveSnapshot(*this);
      }

      ///Undo the changes until the last snapshot.

      ///This function undos the changes until the last snapshot
      ///created by save() or Snapshot(SmartGraph&).
      void restore()
      {
        _graph->restoreSnapshot(*this);
      }
    };
  };

  class SmartBpGraphBase {

  protected:

    struct NodeT {
      int first_out;
      int partition_next;
      int partition_index;
      bool red;
    };

    struct ArcT {
      int target;
      int next_out;
    };

    std::vector<NodeT> nodes;
    std::vector<ArcT> arcs;

    int first_red, first_blue;
    int max_red, max_blue;

  public:

    typedef SmartBpGraphBase Graph;

    class Node;
    class Arc;
    class Edge;

    class Node {
      friend class SmartBpGraphBase;
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
      friend class SmartBpGraphBase;
    protected:

      explicit RedNode(int pid) : Node(pid) {}

    public:
      RedNode() {}
      RedNode(const RedNode& node) : Node(node) {}
      RedNode(Invalid) : Node(INVALID){}
    };

    class BlueNode : public Node {
      friend class SmartBpGraphBase;
    protected:

      explicit BlueNode(int pid) : Node(pid) {}

    public:
      BlueNode() {}
      BlueNode(const BlueNode& node) : Node(node) {}
      BlueNode(Invalid) : Node(INVALID){}
    };

    class Edge {
      friend class SmartBpGraphBase;
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
      friend class SmartBpGraphBase;
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



    SmartBpGraphBase()
      : nodes(), arcs(), first_red(-1), first_blue(-1),
        max_red(-1), max_blue(-1) {}

    typedef True NodeNumTag;
    typedef True EdgeNumTag;
    typedef True ArcNumTag;

    int nodeNum() const { return nodes.size(); }
    int redNum() const { return max_red + 1; }
    int blueNum() const { return max_blue + 1; }
    int edgeNum() const { return arcs.size() / 2; }
    int arcNum() const { return arcs.size(); }

    int maxNodeId() const { return nodes.size()-1; }
    int maxRedId() const { return max_red; }
    int maxBlueId() const { return max_blue; }
    int maxEdgeId() const { return arcs.size() / 2 - 1; }
    int maxArcId() const { return arcs.size()-1; }

    bool red(Node n) const { return nodes[n._id].red; }
    bool blue(Node n) const { return !nodes[n._id].red; }

    static RedNode asRedNodeUnsafe(Node n) { return RedNode(n._id); }
    static BlueNode asBlueNodeUnsafe(Node n) { return BlueNode(n._id); }

    Node source(Arc a) const { return Node(arcs[a._id ^ 1].target); }
    Node target(Arc a) const { return Node(arcs[a._id].target); }

    RedNode redNode(Edge e) const {
      return RedNode(arcs[2 * e._id].target);
    }
    BlueNode blueNode(Edge e) const {
      return BlueNode(arcs[2 * e._id + 1].target);
    }

    static bool direction(Arc a) {
      return (a._id & 1) == 1;
    }

    static Arc direct(Edge e, bool d) {
      return Arc(e._id * 2 + (d ? 1 : 0));
    }

    void first(Node& node) const {
      node._id = nodes.size() - 1;
    }

    static void next(Node& node) {
      --node._id;
    }

    void first(RedNode& node) const {
      node._id = first_red;
    }

    void next(RedNode& node) const {
      node._id = nodes[node._id].partition_next;
    }

    void first(BlueNode& node) const {
      node._id = first_blue;
    }

    void next(BlueNode& node) const {
      node._id = nodes[node._id].partition_next;
    }

    void first(Arc& arc) const {
      arc._id = arcs.size() - 1;
    }

    static void next(Arc& arc) {
      --arc._id;
    }

    void first(Edge& arc) const {
      arc._id = arcs.size() / 2 - 1;
    }

    static void next(Edge& arc) {
      --arc._id;
    }

    void firstOut(Arc &arc, const Node& v) const {
      arc._id = nodes[v._id].first_out;
    }
    void nextOut(Arc &arc) const {
      arc._id = arcs[arc._id].next_out;
    }

    void firstIn(Arc &arc, const Node& v) const {
      arc._id = ((nodes[v._id].first_out) ^ 1);
      if (arc._id == -2) arc._id = -1;
    }
    void nextIn(Arc &arc) const {
      arc._id = ((arcs[arc._id ^ 1].next_out) ^ 1);
      if (arc._id == -2) arc._id = -1;
    }

    void firstInc(Edge &arc, bool& d, const Node& v) const {
      int de = nodes[v._id].first_out;
      if (de != -1) {
        arc._id = de / 2;
        d = ((de & 1) == 1);
      } else {
        arc._id = -1;
        d = true;
      }
    }
    void nextInc(Edge &arc, bool& d) const {
      int de = (arcs[(arc._id * 2) | (d ? 1 : 0)].next_out);
      if (de != -1) {
        arc._id = de / 2;
        d = ((de & 1) == 1);
      } else {
        arc._id = -1;
        d = true;
      }
    }

    static int id(Node v) { return v._id; }
    int id(RedNode v) const { return nodes[v._id].partition_index; }
    int id(BlueNode v) const { return nodes[v._id].partition_index; }
    static int id(Arc e) { return e._id; }
    static int id(Edge e) { return e._id; }

    static Node nodeFromId(int id) { return Node(id);}
    static Arc arcFromId(int id) { return Arc(id);}
    static Edge edgeFromId(int id) { return Edge(id);}

    bool valid(Node n) const {
      return n._id >= 0 && n._id < static_cast<int>(nodes.size());
    }
    bool valid(Arc a) const {
      return a._id >= 0 && a._id < static_cast<int>(arcs.size());
    }
    bool valid(Edge e) const {
      return e._id >= 0 && 2 * e._id < static_cast<int>(arcs.size());
    }

    RedNode addRedNode() {
      int n = nodes.size();
      nodes.push_back(NodeT());
      nodes[n].first_out = -1;
      nodes[n].red = true;
      nodes[n].partition_index = ++max_red;
      nodes[n].partition_next = first_red;
      first_red = n;

      return RedNode(n);
    }

    BlueNode addBlueNode() {
      int n = nodes.size();
      nodes.push_back(NodeT());
      nodes[n].first_out = -1;
      nodes[n].red = false;
      nodes[n].partition_index = ++max_blue;
      nodes[n].partition_next = first_blue;
      first_blue = n;

      return BlueNode(n);
    }

    Edge addEdge(RedNode u, BlueNode v) {
      int n = arcs.size();
      arcs.push_back(ArcT());
      arcs.push_back(ArcT());

      arcs[n].target = u._id;
      arcs[n | 1].target = v._id;

      arcs[n].next_out = nodes[v._id].first_out;
      nodes[v._id].first_out = n;

      arcs[n | 1].next_out = nodes[u._id].first_out;
      nodes[u._id].first_out = (n | 1);

      return Edge(n / 2);
    }

    void clear() {
      arcs.clear();
      nodes.clear();
      first_red = -1;
      first_blue = -1;
      max_blue = -1;
      max_red = -1;
    }

  };

  typedef BpGraphExtender<SmartBpGraphBase> ExtendedSmartBpGraphBase;

  /// \ingroup graphs
  ///
  /// \brief A smart undirected bipartite graph class.
  ///
  /// \ref SmartBpGraph is a simple and fast bipartite graph implementation.
  /// It is also quite memory efficient but at the price
  /// that it does not support node and edge deletion
  /// (except for the Snapshot feature).
  ///
  /// This type fully conforms to the \ref concepts::BpGraph "BpGraph concept"
  /// and it also provides some additional functionalities.
  /// Most of its member functions and nested classes are documented
  /// only in the concept class.
  ///
  /// This class provides constant time counting for nodes, edges and arcs.
  ///
  /// \sa concepts::BpGraph
  /// \sa SmartGraph
  class SmartBpGraph : public ExtendedSmartBpGraphBase {
    typedef ExtendedSmartBpGraphBase Parent;

  private:
    /// Graphs are \e not copy constructible. Use GraphCopy instead.
    SmartBpGraph(const SmartBpGraph &) : ExtendedSmartBpGraphBase() {};
    /// \brief Assignment of a graph to another one is \e not allowed.
    /// Use GraphCopy instead.
    void operator=(const SmartBpGraph &) {}

  public:

    /// Constructor

    /// Constructor.
    ///
    SmartBpGraph() {}

    /// \brief Add a new red node to the graph.
    ///
    /// This function adds a red new node to the graph.
    /// \return The new node.
    RedNode addRedNode() { return Parent::addRedNode(); }

    /// \brief Add a new blue node to the graph.
    ///
    /// This function adds a blue new node to the graph.
    /// \return The new node.
    BlueNode addBlueNode() { return Parent::addBlueNode(); }

    /// \brief Add a new edge to the graph.
    ///
    /// This function adds a new edge to the graph between nodes
    /// \c u and \c v with inherent orientation from node \c u to
    /// node \c v.
    /// \return The new edge.
    Edge addEdge(RedNode u, BlueNode v) {
      return Parent::addEdge(u, v);
    }
    Edge addEdge(BlueNode v, RedNode u) {
      return Parent::addEdge(u, v);
    }

    /// \brief Node validity check
    ///
    /// This function gives back \c true if the given node is valid,
    /// i.e. it is a real node of the graph.
    ///
    /// \warning A removed node (using Snapshot) could become valid again
    /// if new nodes are added to the graph.
    bool valid(Node n) const { return Parent::valid(n); }

    /// \brief Edge validity check
    ///
    /// This function gives back \c true if the given edge is valid,
    /// i.e. it is a real edge of the graph.
    ///
    /// \warning A removed edge (using Snapshot) could become valid again
    /// if new edges are added to the graph.
    bool valid(Edge e) const { return Parent::valid(e); }

    /// \brief Arc validity check
    ///
    /// This function gives back \c true if the given arc is valid,
    /// i.e. it is a real arc of the graph.
    ///
    /// \warning A removed arc (using Snapshot) could become valid again
    /// if new edges are added to the graph.
    bool valid(Arc a) const { return Parent::valid(a); }

    ///Clear the graph.

    ///This function erases all nodes and arcs from the graph.
    ///
    void clear() {
      Parent::clear();
    }

    /// Reserve memory for nodes.

    /// Using this function, it is possible to avoid superfluous memory
    /// allocation: if you know that the graph you want to build will
    /// be large (e.g. it will contain millions of nodes and/or edges),
    /// then it is worth reserving space for this amount before starting
    /// to build the graph.
    /// \sa reserveEdge()
    void reserveNode(int n) { nodes.reserve(n); };

    /// Reserve memory for edges.

    /// Using this function, it is possible to avoid superfluous memory
    /// allocation: if you know that the graph you want to build will
    /// be large (e.g. it will contain millions of nodes and/or edges),
    /// then it is worth reserving space for this amount before starting
    /// to build the graph.
    /// \sa reserveNode()
    void reserveEdge(int m) { arcs.reserve(2 * m); };

  public:

    class Snapshot;

  protected:

    void saveSnapshot(Snapshot &s)
    {
      s._graph = this;
      s.node_num = nodes.size();
      s.arc_num = arcs.size();
    }

    void restoreSnapshot(const Snapshot &s)
    {
      while(s.arc_num<arcs.size()) {
        int n=arcs.size()-1;
        Edge arc=edgeFromId(n/2);
        Parent::notifier(Edge()).erase(arc);
        std::vector<Arc> dir;
        dir.push_back(arcFromId(n));
        dir.push_back(arcFromId(n-1));
        Parent::notifier(Arc()).erase(dir);
        nodes[arcs[n-1].target].first_out=arcs[n].next_out;
        nodes[arcs[n].target].first_out=arcs[n-1].next_out;
        arcs.pop_back();
        arcs.pop_back();
      }
      while(s.node_num<nodes.size()) {
        int n=nodes.size()-1;
        Node node = nodeFromId(n);
        if (Parent::red(node)) {
          first_red = nodes[n].partition_next;
          if (first_red != -1) {
            max_red = nodes[first_red].partition_index;
          } else {
            max_red = -1;
          }
          Parent::notifier(RedNode()).erase(asRedNodeUnsafe(node));
        } else {
          first_blue = nodes[n].partition_next;
          if (first_blue != -1) {
            max_blue = nodes[first_blue].partition_index;
          } else {
            max_blue = -1;
          }
          Parent::notifier(BlueNode()).erase(asBlueNodeUnsafe(node));
        }
        Parent::notifier(Node()).erase(node);
        nodes.pop_back();
      }
    }

  public:

    ///Class to make a snapshot of the graph and to restore it later.

    ///Class to make a snapshot of the graph and to restore it later.
    ///
    ///The newly added nodes and edges can be removed using the
    ///restore() function. This is the only way for deleting nodes and/or
    ///edges from a SmartBpGraph structure.
    ///
    ///\note After a state is restored, you cannot restore a later state,
    ///i.e. you cannot add the removed nodes and edges again using
    ///another Snapshot instance.
    ///
    ///\warning The validity of the snapshot is not stored due to
    ///performance reasons. If you do not use the snapshot correctly,
    ///it can cause broken program, invalid or not restored state of
    ///the graph or no change.
    class Snapshot
    {
      SmartBpGraph *_graph;
    protected:
      friend class SmartBpGraph;
      unsigned int node_num;
      unsigned int arc_num;
    public:
      ///Default constructor.

      ///Default constructor.
      ///You have to call save() to actually make a snapshot.
      Snapshot() : _graph(0) {}
      ///Constructor that immediately makes a snapshot

      /// This constructor immediately makes a snapshot of the given graph.
      ///
      Snapshot(SmartBpGraph &gr) {
        gr.saveSnapshot(*this);
      }

      ///Make a snapshot.

      ///This function makes a snapshot of the given graph.
      ///It can be called more than once. In case of a repeated
      ///call, the previous snapshot gets lost.
      void save(SmartBpGraph &gr)
      {
        gr.saveSnapshot(*this);
      }

      ///Undo the changes until the last snapshot.

      ///This function undos the changes until the last snapshot
      ///created by save() or Snapshot(SmartBpGraph&).
      void restore()
      {
        _graph->restoreSnapshot(*this);
      }
    };
  };

} //namespace lemon


#endif //LEMON_SMART_GRAPH_H
