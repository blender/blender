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

#ifndef LEMON_BITS_GRAPH_EXTENDER_H
#define LEMON_BITS_GRAPH_EXTENDER_H

#include <lemon/core.h>

#include <lemon/bits/map_extender.h>
#include <lemon/bits/default_map.h>

#include <lemon/concept_check.h>
#include <lemon/concepts/maps.h>

//\ingroup graphbits
//\file
//\brief Extenders for the graph types
namespace lemon {

  // \ingroup graphbits
  //
  // \brief Extender for the digraph implementations
  template <typename Base>
  class DigraphExtender : public Base {
    typedef Base Parent;

  public:

    typedef DigraphExtender Digraph;

    // Base extensions

    typedef typename Parent::Node Node;
    typedef typename Parent::Arc Arc;

    int maxId(Node) const {
      return Parent::maxNodeId();
    }

    int maxId(Arc) const {
      return Parent::maxArcId();
    }

    static Node fromId(int id, Node) {
      return Parent::nodeFromId(id);
    }

    static Arc fromId(int id, Arc) {
      return Parent::arcFromId(id);
    }

    Node oppositeNode(const Node &node, const Arc &arc) const {
      if (node == Parent::source(arc))
        return Parent::target(arc);
      else if(node == Parent::target(arc))
        return Parent::source(arc);
      else
        return INVALID;
    }

    // Alterable extension

    typedef AlterationNotifier<DigraphExtender, Node> NodeNotifier;
    typedef AlterationNotifier<DigraphExtender, Arc> ArcNotifier;


  protected:

    mutable NodeNotifier node_notifier;
    mutable ArcNotifier arc_notifier;

  public:

    NodeNotifier& notifier(Node) const {
      return node_notifier;
    }

    ArcNotifier& notifier(Arc) const {
      return arc_notifier;
    }

    class NodeIt : public Node {
      const Digraph* _digraph;
    public:

      NodeIt() {}

      NodeIt(Invalid i) : Node(i) { }

      explicit NodeIt(const Digraph& digraph) : _digraph(&digraph) {
        _digraph->first(static_cast<Node&>(*this));
      }

      NodeIt(const Digraph& digraph, const Node& node)
        : Node(node), _digraph(&digraph) {}

      NodeIt& operator++() {
        _digraph->next(*this);
        return *this;
      }

    };


    class ArcIt : public Arc {
      const Digraph* _digraph;
    public:

      ArcIt() { }

      ArcIt(Invalid i) : Arc(i) { }

      explicit ArcIt(const Digraph& digraph) : _digraph(&digraph) {
        _digraph->first(static_cast<Arc&>(*this));
      }

      ArcIt(const Digraph& digraph, const Arc& arc) :
        Arc(arc), _digraph(&digraph) { }

      ArcIt& operator++() {
        _digraph->next(*this);
        return *this;
      }

    };


    class OutArcIt : public Arc {
      const Digraph* _digraph;
    public:

      OutArcIt() { }

      OutArcIt(Invalid i) : Arc(i) { }

      OutArcIt(const Digraph& digraph, const Node& node)
        : _digraph(&digraph) {
        _digraph->firstOut(*this, node);
      }

      OutArcIt(const Digraph& digraph, const Arc& arc)
        : Arc(arc), _digraph(&digraph) {}

      OutArcIt& operator++() {
        _digraph->nextOut(*this);
        return *this;
      }

    };


    class InArcIt : public Arc {
      const Digraph* _digraph;
    public:

      InArcIt() { }

      InArcIt(Invalid i) : Arc(i) { }

      InArcIt(const Digraph& digraph, const Node& node)
        : _digraph(&digraph) {
        _digraph->firstIn(*this, node);
      }

      InArcIt(const Digraph& digraph, const Arc& arc) :
        Arc(arc), _digraph(&digraph) {}

      InArcIt& operator++() {
        _digraph->nextIn(*this);
        return *this;
      }

    };

    // \brief Base node of the iterator
    //
    // Returns the base node (i.e. the source in this case) of the iterator
    Node baseNode(const OutArcIt &arc) const {
      return Parent::source(arc);
    }
    // \brief Running node of the iterator
    //
    // Returns the running node (i.e. the target in this case) of the
    // iterator
    Node runningNode(const OutArcIt &arc) const {
      return Parent::target(arc);
    }

    // \brief Base node of the iterator
    //
    // Returns the base node (i.e. the target in this case) of the iterator
    Node baseNode(const InArcIt &arc) const {
      return Parent::target(arc);
    }
    // \brief Running node of the iterator
    //
    // Returns the running node (i.e. the source in this case) of the
    // iterator
    Node runningNode(const InArcIt &arc) const {
      return Parent::source(arc);
    }


    template <typename _Value>
    class NodeMap
      : public MapExtender<DefaultMap<Digraph, Node, _Value> > {
      typedef MapExtender<DefaultMap<Digraph, Node, _Value> > Parent;

    public:
      explicit NodeMap(const Digraph& digraph)
        : Parent(digraph) {}
      NodeMap(const Digraph& digraph, const _Value& value)
        : Parent(digraph, value) {}

    private:
      NodeMap& operator=(const NodeMap& cmap) {
        return operator=<NodeMap>(cmap);
      }

      template <typename CMap>
      NodeMap& operator=(const CMap& cmap) {
        Parent::operator=(cmap);
        return *this;
      }

    };

    template <typename _Value>
    class ArcMap
      : public MapExtender<DefaultMap<Digraph, Arc, _Value> > {
      typedef MapExtender<DefaultMap<Digraph, Arc, _Value> > Parent;

    public:
      explicit ArcMap(const Digraph& digraph)
        : Parent(digraph) {}
      ArcMap(const Digraph& digraph, const _Value& value)
        : Parent(digraph, value) {}

    private:
      ArcMap& operator=(const ArcMap& cmap) {
        return operator=<ArcMap>(cmap);
      }

      template <typename CMap>
      ArcMap& operator=(const CMap& cmap) {
        Parent::operator=(cmap);
        return *this;
      }
    };


    Node addNode() {
      Node node = Parent::addNode();
      notifier(Node()).add(node);
      return node;
    }

    Arc addArc(const Node& from, const Node& to) {
      Arc arc = Parent::addArc(from, to);
      notifier(Arc()).add(arc);
      return arc;
    }

    void clear() {
      notifier(Arc()).clear();
      notifier(Node()).clear();
      Parent::clear();
    }

    template <typename Digraph, typename NodeRefMap, typename ArcRefMap>
    void build(const Digraph& digraph, NodeRefMap& nodeRef, ArcRefMap& arcRef) {
      Parent::build(digraph, nodeRef, arcRef);
      notifier(Node()).build();
      notifier(Arc()).build();
    }

    void erase(const Node& node) {
      Arc arc;
      Parent::firstOut(arc, node);
      while (arc != INVALID ) {
        erase(arc);
        Parent::firstOut(arc, node);
      }

      Parent::firstIn(arc, node);
      while (arc != INVALID ) {
        erase(arc);
        Parent::firstIn(arc, node);
      }

      notifier(Node()).erase(node);
      Parent::erase(node);
    }

    void erase(const Arc& arc) {
      notifier(Arc()).erase(arc);
      Parent::erase(arc);
    }

    DigraphExtender() {
      node_notifier.setContainer(*this);
      arc_notifier.setContainer(*this);
    }


    ~DigraphExtender() {
      arc_notifier.clear();
      node_notifier.clear();
    }
  };

  // \ingroup _graphbits
  //
  // \brief Extender for the Graphs
  template <typename Base>
  class GraphExtender : public Base {
    typedef Base Parent;

  public:

    typedef GraphExtender Graph;

    typedef True UndirectedTag;

    typedef typename Parent::Node Node;
    typedef typename Parent::Arc Arc;
    typedef typename Parent::Edge Edge;

    // Graph extension

    int maxId(Node) const {
      return Parent::maxNodeId();
    }

    int maxId(Arc) const {
      return Parent::maxArcId();
    }

    int maxId(Edge) const {
      return Parent::maxEdgeId();
    }

    static Node fromId(int id, Node) {
      return Parent::nodeFromId(id);
    }

    static Arc fromId(int id, Arc) {
      return Parent::arcFromId(id);
    }

    static Edge fromId(int id, Edge) {
      return Parent::edgeFromId(id);
    }

    Node oppositeNode(const Node &n, const Edge &e) const {
      if( n == Parent::u(e))
        return Parent::v(e);
      else if( n == Parent::v(e))
        return Parent::u(e);
      else
        return INVALID;
    }

    Arc oppositeArc(const Arc &arc) const {
      return Parent::direct(arc, !Parent::direction(arc));
    }

    using Parent::direct;
    Arc direct(const Edge &edge, const Node &node) const {
      return Parent::direct(edge, Parent::u(edge) == node);
    }

    // Alterable extension

    typedef AlterationNotifier<GraphExtender, Node> NodeNotifier;
    typedef AlterationNotifier<GraphExtender, Arc> ArcNotifier;
    typedef AlterationNotifier<GraphExtender, Edge> EdgeNotifier;


  protected:

    mutable NodeNotifier node_notifier;
    mutable ArcNotifier arc_notifier;
    mutable EdgeNotifier edge_notifier;

  public:

    NodeNotifier& notifier(Node) const {
      return node_notifier;
    }

    ArcNotifier& notifier(Arc) const {
      return arc_notifier;
    }

    EdgeNotifier& notifier(Edge) const {
      return edge_notifier;
    }



    class NodeIt : public Node {
      const Graph* _graph;
    public:

      NodeIt() {}

      NodeIt(Invalid i) : Node(i) { }

      explicit NodeIt(const Graph& graph) : _graph(&graph) {
        _graph->first(static_cast<Node&>(*this));
      }

      NodeIt(const Graph& graph, const Node& node)
        : Node(node), _graph(&graph) {}

      NodeIt& operator++() {
        _graph->next(*this);
        return *this;
      }

    };


    class ArcIt : public Arc {
      const Graph* _graph;
    public:

      ArcIt() { }

      ArcIt(Invalid i) : Arc(i) { }

      explicit ArcIt(const Graph& graph) : _graph(&graph) {
        _graph->first(static_cast<Arc&>(*this));
      }

      ArcIt(const Graph& graph, const Arc& arc) :
        Arc(arc), _graph(&graph) { }

      ArcIt& operator++() {
        _graph->next(*this);
        return *this;
      }

    };


    class OutArcIt : public Arc {
      const Graph* _graph;
    public:

      OutArcIt() { }

      OutArcIt(Invalid i) : Arc(i) { }

      OutArcIt(const Graph& graph, const Node& node)
        : _graph(&graph) {
        _graph->firstOut(*this, node);
      }

      OutArcIt(const Graph& graph, const Arc& arc)
        : Arc(arc), _graph(&graph) {}

      OutArcIt& operator++() {
        _graph->nextOut(*this);
        return *this;
      }

    };


    class InArcIt : public Arc {
      const Graph* _graph;
    public:

      InArcIt() { }

      InArcIt(Invalid i) : Arc(i) { }

      InArcIt(const Graph& graph, const Node& node)
        : _graph(&graph) {
        _graph->firstIn(*this, node);
      }

      InArcIt(const Graph& graph, const Arc& arc) :
        Arc(arc), _graph(&graph) {}

      InArcIt& operator++() {
        _graph->nextIn(*this);
        return *this;
      }

    };


    class EdgeIt : public Parent::Edge {
      const Graph* _graph;
    public:

      EdgeIt() { }

      EdgeIt(Invalid i) : Edge(i) { }

      explicit EdgeIt(const Graph& graph) : _graph(&graph) {
        _graph->first(static_cast<Edge&>(*this));
      }

      EdgeIt(const Graph& graph, const Edge& edge) :
        Edge(edge), _graph(&graph) { }

      EdgeIt& operator++() {
        _graph->next(*this);
        return *this;
      }

    };

    class IncEdgeIt : public Parent::Edge {
      friend class GraphExtender;
      const Graph* _graph;
      bool _direction;
    public:

      IncEdgeIt() { }

      IncEdgeIt(Invalid i) : Edge(i), _direction(false) { }

      IncEdgeIt(const Graph& graph, const Node &node) : _graph(&graph) {
        _graph->firstInc(*this, _direction, node);
      }

      IncEdgeIt(const Graph& graph, const Edge &edge, const Node &node)
        : _graph(&graph), Edge(edge) {
        _direction = (_graph->source(edge) == node);
      }

      IncEdgeIt& operator++() {
        _graph->nextInc(*this, _direction);
        return *this;
      }
    };

    // \brief Base node of the iterator
    //
    // Returns the base node (ie. the source in this case) of the iterator
    Node baseNode(const OutArcIt &arc) const {
      return Parent::source(static_cast<const Arc&>(arc));
    }
    // \brief Running node of the iterator
    //
    // Returns the running node (ie. the target in this case) of the
    // iterator
    Node runningNode(const OutArcIt &arc) const {
      return Parent::target(static_cast<const Arc&>(arc));
    }

    // \brief Base node of the iterator
    //
    // Returns the base node (ie. the target in this case) of the iterator
    Node baseNode(const InArcIt &arc) const {
      return Parent::target(static_cast<const Arc&>(arc));
    }
    // \brief Running node of the iterator
    //
    // Returns the running node (ie. the source in this case) of the
    // iterator
    Node runningNode(const InArcIt &arc) const {
      return Parent::source(static_cast<const Arc&>(arc));
    }

    // Base node of the iterator
    //
    // Returns the base node of the iterator
    Node baseNode(const IncEdgeIt &edge) const {
      return edge._direction ? this->u(edge) : this->v(edge);
    }
    // Running node of the iterator
    //
    // Returns the running node of the iterator
    Node runningNode(const IncEdgeIt &edge) const {
      return edge._direction ? this->v(edge) : this->u(edge);
    }

    // Mappable extension

    template <typename _Value>
    class NodeMap
      : public MapExtender<DefaultMap<Graph, Node, _Value> > {
      typedef MapExtender<DefaultMap<Graph, Node, _Value> > Parent;

    public:
      explicit NodeMap(const Graph& graph)
        : Parent(graph) {}
      NodeMap(const Graph& graph, const _Value& value)
        : Parent(graph, value) {}

    private:
      NodeMap& operator=(const NodeMap& cmap) {
        return operator=<NodeMap>(cmap);
      }

      template <typename CMap>
      NodeMap& operator=(const CMap& cmap) {
        Parent::operator=(cmap);
        return *this;
      }

    };

    template <typename _Value>
    class ArcMap
      : public MapExtender<DefaultMap<Graph, Arc, _Value> > {
      typedef MapExtender<DefaultMap<Graph, Arc, _Value> > Parent;

    public:
      explicit ArcMap(const Graph& graph)
        : Parent(graph) {}
      ArcMap(const Graph& graph, const _Value& value)
        : Parent(graph, value) {}

    private:
      ArcMap& operator=(const ArcMap& cmap) {
        return operator=<ArcMap>(cmap);
      }

      template <typename CMap>
      ArcMap& operator=(const CMap& cmap) {
        Parent::operator=(cmap);
        return *this;
      }
    };


    template <typename _Value>
    class EdgeMap
      : public MapExtender<DefaultMap<Graph, Edge, _Value> > {
      typedef MapExtender<DefaultMap<Graph, Edge, _Value> > Parent;

    public:
      explicit EdgeMap(const Graph& graph)
        : Parent(graph) {}

      EdgeMap(const Graph& graph, const _Value& value)
        : Parent(graph, value) {}

    private:
      EdgeMap& operator=(const EdgeMap& cmap) {
        return operator=<EdgeMap>(cmap);
      }

      template <typename CMap>
      EdgeMap& operator=(const CMap& cmap) {
        Parent::operator=(cmap);
        return *this;
      }

    };

    // Alteration extension

    Node addNode() {
      Node node = Parent::addNode();
      notifier(Node()).add(node);
      return node;
    }

    Edge addEdge(const Node& from, const Node& to) {
      Edge edge = Parent::addEdge(from, to);
      notifier(Edge()).add(edge);
      std::vector<Arc> ev;
      ev.push_back(Parent::direct(edge, true));
      ev.push_back(Parent::direct(edge, false));
      notifier(Arc()).add(ev);
      return edge;
    }

    void clear() {
      notifier(Arc()).clear();
      notifier(Edge()).clear();
      notifier(Node()).clear();
      Parent::clear();
    }

    template <typename Graph, typename NodeRefMap, typename EdgeRefMap>
    void build(const Graph& graph, NodeRefMap& nodeRef,
               EdgeRefMap& edgeRef) {
      Parent::build(graph, nodeRef, edgeRef);
      notifier(Node()).build();
      notifier(Edge()).build();
      notifier(Arc()).build();
    }

    void erase(const Node& node) {
      Arc arc;
      Parent::firstOut(arc, node);
      while (arc != INVALID ) {
        erase(arc);
        Parent::firstOut(arc, node);
      }

      Parent::firstIn(arc, node);
      while (arc != INVALID ) {
        erase(arc);
        Parent::firstIn(arc, node);
      }

      notifier(Node()).erase(node);
      Parent::erase(node);
    }

    void erase(const Edge& edge) {
      std::vector<Arc> av;
      av.push_back(Parent::direct(edge, true));
      av.push_back(Parent::direct(edge, false));
      notifier(Arc()).erase(av);
      notifier(Edge()).erase(edge);
      Parent::erase(edge);
    }

    GraphExtender() {
      node_notifier.setContainer(*this);
      arc_notifier.setContainer(*this);
      edge_notifier.setContainer(*this);
    }

    ~GraphExtender() {
      edge_notifier.clear();
      arc_notifier.clear();
      node_notifier.clear();
    }

  };

  // \ingroup _graphbits
  //
  // \brief Extender for the BpGraphs
  template <typename Base>
  class BpGraphExtender : public Base {
    typedef Base Parent;

  public:

    typedef BpGraphExtender BpGraph;

    typedef True UndirectedTag;

    typedef typename Parent::Node Node;
    typedef typename Parent::RedNode RedNode;
    typedef typename Parent::BlueNode BlueNode;
    typedef typename Parent::Arc Arc;
    typedef typename Parent::Edge Edge;

    // BpGraph extension

    using Parent::first;
    using Parent::next;
    using Parent::id;

    int maxId(Node) const {
      return Parent::maxNodeId();
    }

    int maxId(RedNode) const {
      return Parent::maxRedId();
    }

    int maxId(BlueNode) const {
      return Parent::maxBlueId();
    }

    int maxId(Arc) const {
      return Parent::maxArcId();
    }

    int maxId(Edge) const {
      return Parent::maxEdgeId();
    }

    static Node fromId(int id, Node) {
      return Parent::nodeFromId(id);
    }

    static Arc fromId(int id, Arc) {
      return Parent::arcFromId(id);
    }

    static Edge fromId(int id, Edge) {
      return Parent::edgeFromId(id);
    }

    Node u(Edge e) const { return this->redNode(e); }
    Node v(Edge e) const { return this->blueNode(e); }

    Node oppositeNode(const Node &n, const Edge &e) const {
      if( n == u(e))
        return v(e);
      else if( n == v(e))
        return u(e);
      else
        return INVALID;
    }

    Arc oppositeArc(const Arc &arc) const {
      return Parent::direct(arc, !Parent::direction(arc));
    }

    using Parent::direct;
    Arc direct(const Edge &edge, const Node &node) const {
      return Parent::direct(edge, Parent::redNode(edge) == node);
    }

    RedNode asRedNode(const Node& node) const {
      if (node == INVALID || Parent::blue(node)) {
        return INVALID;
      } else {
        return Parent::asRedNodeUnsafe(node);
      }
    }

    BlueNode asBlueNode(const Node& node) const {
      if (node == INVALID || Parent::red(node)) {
        return INVALID;
      } else {
        return Parent::asBlueNodeUnsafe(node);
      }
    }

    // Alterable extension

    typedef AlterationNotifier<BpGraphExtender, Node> NodeNotifier;
    typedef AlterationNotifier<BpGraphExtender, RedNode> RedNodeNotifier;
    typedef AlterationNotifier<BpGraphExtender, BlueNode> BlueNodeNotifier;
    typedef AlterationNotifier<BpGraphExtender, Arc> ArcNotifier;
    typedef AlterationNotifier<BpGraphExtender, Edge> EdgeNotifier;


  protected:

    mutable NodeNotifier node_notifier;
    mutable RedNodeNotifier red_node_notifier;
    mutable BlueNodeNotifier blue_node_notifier;
    mutable ArcNotifier arc_notifier;
    mutable EdgeNotifier edge_notifier;

  public:

    NodeNotifier& notifier(Node) const {
      return node_notifier;
    }

    RedNodeNotifier& notifier(RedNode) const {
      return red_node_notifier;
    }

    BlueNodeNotifier& notifier(BlueNode) const {
      return blue_node_notifier;
    }

    ArcNotifier& notifier(Arc) const {
      return arc_notifier;
    }

    EdgeNotifier& notifier(Edge) const {
      return edge_notifier;
    }



    class NodeIt : public Node {
      const BpGraph* _graph;
    public:

      NodeIt() {}

      NodeIt(Invalid i) : Node(i) { }

      explicit NodeIt(const BpGraph& graph) : _graph(&graph) {
        _graph->first(static_cast<Node&>(*this));
      }

      NodeIt(const BpGraph& graph, const Node& node)
        : Node(node), _graph(&graph) {}

      NodeIt& operator++() {
        _graph->next(*this);
        return *this;
      }

    };

    class RedNodeIt : public RedNode {
      const BpGraph* _graph;
    public:

      RedNodeIt() {}

      RedNodeIt(Invalid i) : RedNode(i) { }

      explicit RedNodeIt(const BpGraph& graph) : _graph(&graph) {
        _graph->first(static_cast<RedNode&>(*this));
      }

      RedNodeIt(const BpGraph& graph, const RedNode& node)
        : RedNode(node), _graph(&graph) {}

      RedNodeIt& operator++() {
        _graph->next(static_cast<RedNode&>(*this));
        return *this;
      }

    };

    class BlueNodeIt : public BlueNode {
      const BpGraph* _graph;
    public:

      BlueNodeIt() {}

      BlueNodeIt(Invalid i) : BlueNode(i) { }

      explicit BlueNodeIt(const BpGraph& graph) : _graph(&graph) {
        _graph->first(static_cast<BlueNode&>(*this));
      }

      BlueNodeIt(const BpGraph& graph, const BlueNode& node)
        : BlueNode(node), _graph(&graph) {}

      BlueNodeIt& operator++() {
        _graph->next(static_cast<BlueNode&>(*this));
        return *this;
      }

    };


    class ArcIt : public Arc {
      const BpGraph* _graph;
    public:

      ArcIt() { }

      ArcIt(Invalid i) : Arc(i) { }

      explicit ArcIt(const BpGraph& graph) : _graph(&graph) {
        _graph->first(static_cast<Arc&>(*this));
      }

      ArcIt(const BpGraph& graph, const Arc& arc) :
        Arc(arc), _graph(&graph) { }

      ArcIt& operator++() {
        _graph->next(*this);
        return *this;
      }

    };


    class OutArcIt : public Arc {
      const BpGraph* _graph;
    public:

      OutArcIt() { }

      OutArcIt(Invalid i) : Arc(i) { }

      OutArcIt(const BpGraph& graph, const Node& node)
        : _graph(&graph) {
        _graph->firstOut(*this, node);
      }

      OutArcIt(const BpGraph& graph, const Arc& arc)
        : Arc(arc), _graph(&graph) {}

      OutArcIt& operator++() {
        _graph->nextOut(*this);
        return *this;
      }

    };


    class InArcIt : public Arc {
      const BpGraph* _graph;
    public:

      InArcIt() { }

      InArcIt(Invalid i) : Arc(i) { }

      InArcIt(const BpGraph& graph, const Node& node)
        : _graph(&graph) {
        _graph->firstIn(*this, node);
      }

      InArcIt(const BpGraph& graph, const Arc& arc) :
        Arc(arc), _graph(&graph) {}

      InArcIt& operator++() {
        _graph->nextIn(*this);
        return *this;
      }

    };


    class EdgeIt : public Parent::Edge {
      const BpGraph* _graph;
    public:

      EdgeIt() { }

      EdgeIt(Invalid i) : Edge(i) { }

      explicit EdgeIt(const BpGraph& graph) : _graph(&graph) {
        _graph->first(static_cast<Edge&>(*this));
      }

      EdgeIt(const BpGraph& graph, const Edge& edge) :
        Edge(edge), _graph(&graph) { }

      EdgeIt& operator++() {
        _graph->next(*this);
        return *this;
      }

    };

    class IncEdgeIt : public Parent::Edge {
      friend class BpGraphExtender;
      const BpGraph* _graph;
      bool _direction;
    public:

      IncEdgeIt() { }

      IncEdgeIt(Invalid i) : Edge(i), _direction(false) { }

      IncEdgeIt(const BpGraph& graph, const Node &node) : _graph(&graph) {
        _graph->firstInc(*this, _direction, node);
      }

      IncEdgeIt(const BpGraph& graph, const Edge &edge, const Node &node)
        : _graph(&graph), Edge(edge) {
        _direction = (_graph->source(edge) == node);
      }

      IncEdgeIt& operator++() {
        _graph->nextInc(*this, _direction);
        return *this;
      }
    };

    // \brief Base node of the iterator
    //
    // Returns the base node (ie. the source in this case) of the iterator
    Node baseNode(const OutArcIt &arc) const {
      return Parent::source(static_cast<const Arc&>(arc));
    }
    // \brief Running node of the iterator
    //
    // Returns the running node (ie. the target in this case) of the
    // iterator
    Node runningNode(const OutArcIt &arc) const {
      return Parent::target(static_cast<const Arc&>(arc));
    }

    // \brief Base node of the iterator
    //
    // Returns the base node (ie. the target in this case) of the iterator
    Node baseNode(const InArcIt &arc) const {
      return Parent::target(static_cast<const Arc&>(arc));
    }
    // \brief Running node of the iterator
    //
    // Returns the running node (ie. the source in this case) of the
    // iterator
    Node runningNode(const InArcIt &arc) const {
      return Parent::source(static_cast<const Arc&>(arc));
    }

    // Base node of the iterator
    //
    // Returns the base node of the iterator
    Node baseNode(const IncEdgeIt &edge) const {
      return edge._direction ? this->u(edge) : this->v(edge);
    }
    // Running node of the iterator
    //
    // Returns the running node of the iterator
    Node runningNode(const IncEdgeIt &edge) const {
      return edge._direction ? this->v(edge) : this->u(edge);
    }

    // Mappable extension

    template <typename _Value>
    class NodeMap
      : public MapExtender<DefaultMap<BpGraph, Node, _Value> > {
      typedef MapExtender<DefaultMap<BpGraph, Node, _Value> > Parent;

    public:
      explicit NodeMap(const BpGraph& bpgraph)
        : Parent(bpgraph) {}
      NodeMap(const BpGraph& bpgraph, const _Value& value)
        : Parent(bpgraph, value) {}

    private:
      NodeMap& operator=(const NodeMap& cmap) {
        return operator=<NodeMap>(cmap);
      }

      template <typename CMap>
      NodeMap& operator=(const CMap& cmap) {
        Parent::operator=(cmap);
        return *this;
      }

    };

    template <typename _Value>
    class RedNodeMap
      : public MapExtender<DefaultMap<BpGraph, RedNode, _Value> > {
      typedef MapExtender<DefaultMap<BpGraph, RedNode, _Value> > Parent;

    public:
      explicit RedNodeMap(const BpGraph& bpgraph)
        : Parent(bpgraph) {}
      RedNodeMap(const BpGraph& bpgraph, const _Value& value)
        : Parent(bpgraph, value) {}

    private:
      RedNodeMap& operator=(const RedNodeMap& cmap) {
        return operator=<RedNodeMap>(cmap);
      }

      template <typename CMap>
      RedNodeMap& operator=(const CMap& cmap) {
        Parent::operator=(cmap);
        return *this;
      }

    };

    template <typename _Value>
    class BlueNodeMap
      : public MapExtender<DefaultMap<BpGraph, BlueNode, _Value> > {
      typedef MapExtender<DefaultMap<BpGraph, BlueNode, _Value> > Parent;

    public:
      explicit BlueNodeMap(const BpGraph& bpgraph)
        : Parent(bpgraph) {}
      BlueNodeMap(const BpGraph& bpgraph, const _Value& value)
        : Parent(bpgraph, value) {}

    private:
      BlueNodeMap& operator=(const BlueNodeMap& cmap) {
        return operator=<BlueNodeMap>(cmap);
      }

      template <typename CMap>
      BlueNodeMap& operator=(const CMap& cmap) {
        Parent::operator=(cmap);
        return *this;
      }

    };

    template <typename _Value>
    class ArcMap
      : public MapExtender<DefaultMap<BpGraph, Arc, _Value> > {
      typedef MapExtender<DefaultMap<BpGraph, Arc, _Value> > Parent;

    public:
      explicit ArcMap(const BpGraph& graph)
        : Parent(graph) {}
      ArcMap(const BpGraph& graph, const _Value& value)
        : Parent(graph, value) {}

    private:
      ArcMap& operator=(const ArcMap& cmap) {
        return operator=<ArcMap>(cmap);
      }

      template <typename CMap>
      ArcMap& operator=(const CMap& cmap) {
        Parent::operator=(cmap);
        return *this;
      }
    };


    template <typename _Value>
    class EdgeMap
      : public MapExtender<DefaultMap<BpGraph, Edge, _Value> > {
      typedef MapExtender<DefaultMap<BpGraph, Edge, _Value> > Parent;

    public:
      explicit EdgeMap(const BpGraph& graph)
        : Parent(graph) {}

      EdgeMap(const BpGraph& graph, const _Value& value)
        : Parent(graph, value) {}

    private:
      EdgeMap& operator=(const EdgeMap& cmap) {
        return operator=<EdgeMap>(cmap);
      }

      template <typename CMap>
      EdgeMap& operator=(const CMap& cmap) {
        Parent::operator=(cmap);
        return *this;
      }

    };

    // Alteration extension

    RedNode addRedNode() {
      RedNode node = Parent::addRedNode();
      notifier(RedNode()).add(node);
      notifier(Node()).add(node);
      return node;
    }

    BlueNode addBlueNode() {
      BlueNode node = Parent::addBlueNode();
      notifier(BlueNode()).add(node);
      notifier(Node()).add(node);
      return node;
    }

    Edge addEdge(const RedNode& from, const BlueNode& to) {
      Edge edge = Parent::addEdge(from, to);
      notifier(Edge()).add(edge);
      std::vector<Arc> av;
      av.push_back(Parent::direct(edge, true));
      av.push_back(Parent::direct(edge, false));
      notifier(Arc()).add(av);
      return edge;
    }

    void clear() {
      notifier(Arc()).clear();
      notifier(Edge()).clear();
      notifier(Node()).clear();
      notifier(BlueNode()).clear();
      notifier(RedNode()).clear();
      Parent::clear();
    }

    template <typename BpGraph, typename NodeRefMap, typename EdgeRefMap>
    void build(const BpGraph& graph, NodeRefMap& nodeRef,
               EdgeRefMap& edgeRef) {
      Parent::build(graph, nodeRef, edgeRef);
      notifier(RedNode()).build();
      notifier(BlueNode()).build();
      notifier(Node()).build();
      notifier(Edge()).build();
      notifier(Arc()).build();
    }

    void erase(const Node& node) {
      Arc arc;
      Parent::firstOut(arc, node);
      while (arc != INVALID ) {
        erase(arc);
        Parent::firstOut(arc, node);
      }

      Parent::firstIn(arc, node);
      while (arc != INVALID ) {
        erase(arc);
        Parent::firstIn(arc, node);
      }

      if (Parent::red(node)) {
        notifier(RedNode()).erase(this->asRedNodeUnsafe(node));
      } else {
        notifier(BlueNode()).erase(this->asBlueNodeUnsafe(node));
      }

      notifier(Node()).erase(node);
      Parent::erase(node);
    }

    void erase(const Edge& edge) {
      std::vector<Arc> av;
      av.push_back(Parent::direct(edge, true));
      av.push_back(Parent::direct(edge, false));
      notifier(Arc()).erase(av);
      notifier(Edge()).erase(edge);
      Parent::erase(edge);
    }

    BpGraphExtender() {
      red_node_notifier.setContainer(*this);
      blue_node_notifier.setContainer(*this);
      node_notifier.setContainer(*this);
      arc_notifier.setContainer(*this);
      edge_notifier.setContainer(*this);
    }

    ~BpGraphExtender() {
      edge_notifier.clear();
      arc_notifier.clear();
      node_notifier.clear();
      blue_node_notifier.clear();
      red_node_notifier.clear();
    }

  };

}

#endif
