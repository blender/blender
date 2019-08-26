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

#ifndef LEMON_BITS_EDGE_SET_EXTENDER_H
#define LEMON_BITS_EDGE_SET_EXTENDER_H

#include <lemon/core.h>
#include <lemon/error.h>
#include <lemon/bits/default_map.h>
#include <lemon/bits/map_extender.h>

//\ingroup digraphbits
//\file
//\brief Extenders for the arc set types
namespace lemon {

  // \ingroup digraphbits
  //
  // \brief Extender for the ArcSets
  template <typename Base>
  class ArcSetExtender : public Base {
    typedef Base Parent;

  public:

    typedef ArcSetExtender Digraph;

    // Base extensions

    typedef typename Parent::Node Node;
    typedef typename Parent::Arc Arc;

    int maxId(Node) const {
      return Parent::maxNodeId();
    }

    int maxId(Arc) const {
      return Parent::maxArcId();
    }

    Node fromId(int id, Node) const {
      return Parent::nodeFromId(id);
    }

    Arc fromId(int id, Arc) const {
      return Parent::arcFromId(id);
    }

    Node oppositeNode(const Node &n, const Arc &e) const {
      if (n == Parent::source(e))
        return Parent::target(e);
      else if(n==Parent::target(e))
        return Parent::source(e);
      else
        return INVALID;
    }


    // Alteration notifier extensions

    // The arc observer registry.
    typedef AlterationNotifier<ArcSetExtender, Arc> ArcNotifier;

  protected:

    mutable ArcNotifier arc_notifier;

  public:

    using Parent::notifier;

    // Gives back the arc alteration notifier.
    ArcNotifier& notifier(Arc) const {
      return arc_notifier;
    }

    // Iterable extensions

    class NodeIt : public Node {
      const Digraph* digraph;
    public:

      NodeIt() {}

      NodeIt(Invalid i) : Node(i) { }

      explicit NodeIt(const Digraph& _graph) : digraph(&_graph) {
        _graph.first(static_cast<Node&>(*this));
      }

      NodeIt(const Digraph& _graph, const Node& node)
        : Node(node), digraph(&_graph) {}

      NodeIt& operator++() {
        digraph->next(*this);
        return *this;
      }

    };


    class ArcIt : public Arc {
      const Digraph* digraph;
    public:

      ArcIt() { }

      ArcIt(Invalid i) : Arc(i) { }

      explicit ArcIt(const Digraph& _graph) : digraph(&_graph) {
        _graph.first(static_cast<Arc&>(*this));
      }

      ArcIt(const Digraph& _graph, const Arc& e) :
        Arc(e), digraph(&_graph) { }

      ArcIt& operator++() {
        digraph->next(*this);
        return *this;
      }

    };


    class OutArcIt : public Arc {
      const Digraph* digraph;
    public:

      OutArcIt() { }

      OutArcIt(Invalid i) : Arc(i) { }

      OutArcIt(const Digraph& _graph, const Node& node)
        : digraph(&_graph) {
        _graph.firstOut(*this, node);
      }

      OutArcIt(const Digraph& _graph, const Arc& arc)
        : Arc(arc), digraph(&_graph) {}

      OutArcIt& operator++() {
        digraph->nextOut(*this);
        return *this;
      }

    };


    class InArcIt : public Arc {
      const Digraph* digraph;
    public:

      InArcIt() { }

      InArcIt(Invalid i) : Arc(i) { }

      InArcIt(const Digraph& _graph, const Node& node)
        : digraph(&_graph) {
        _graph.firstIn(*this, node);
      }

      InArcIt(const Digraph& _graph, const Arc& arc) :
        Arc(arc), digraph(&_graph) {}

      InArcIt& operator++() {
        digraph->nextIn(*this);
        return *this;
      }

    };

    // \brief Base node of the iterator
    //
    // Returns the base node (ie. the source in this case) of the iterator
    Node baseNode(const OutArcIt &e) const {
      return Parent::source(static_cast<const Arc&>(e));
    }
    // \brief Running node of the iterator
    //
    // Returns the running node (ie. the target in this case) of the
    // iterator
    Node runningNode(const OutArcIt &e) const {
      return Parent::target(static_cast<const Arc&>(e));
    }

    // \brief Base node of the iterator
    //
    // Returns the base node (ie. the target in this case) of the iterator
    Node baseNode(const InArcIt &e) const {
      return Parent::target(static_cast<const Arc&>(e));
    }
    // \brief Running node of the iterator
    //
    // Returns the running node (ie. the source in this case) of the
    // iterator
    Node runningNode(const InArcIt &e) const {
      return Parent::source(static_cast<const Arc&>(e));
    }

    using Parent::first;

    // Mappable extension

    template <typename _Value>
    class ArcMap
      : public MapExtender<DefaultMap<Digraph, Arc, _Value> > {
      typedef MapExtender<DefaultMap<Digraph, Arc, _Value> > Parent;

    public:
      explicit ArcMap(const Digraph& _g)
        : Parent(_g) {}
      ArcMap(const Digraph& _g, const _Value& _v)
        : Parent(_g, _v) {}

      ArcMap& operator=(const ArcMap& cmap) {
        return operator=<ArcMap>(cmap);
      }

      template <typename CMap>
      ArcMap& operator=(const CMap& cmap) {
        Parent::operator=(cmap);
        return *this;
      }

    };


    // Alteration extension

    Arc addArc(const Node& from, const Node& to) {
      Arc arc = Parent::addArc(from, to);
      notifier(Arc()).add(arc);
      return arc;
    }

    void clear() {
      notifier(Arc()).clear();
      Parent::clear();
    }

    void erase(const Arc& arc) {
      notifier(Arc()).erase(arc);
      Parent::erase(arc);
    }

    ArcSetExtender() {
      arc_notifier.setContainer(*this);
    }

    ~ArcSetExtender() {
      arc_notifier.clear();
    }

  };


  // \ingroup digraphbits
  //
  // \brief Extender for the EdgeSets
  template <typename Base>
  class EdgeSetExtender : public Base {
    typedef Base Parent;

  public:

    typedef EdgeSetExtender Graph;

    typedef True UndirectedTag;

    typedef typename Parent::Node Node;
    typedef typename Parent::Arc Arc;
    typedef typename Parent::Edge Edge;

    int maxId(Node) const {
      return Parent::maxNodeId();
    }

    int maxId(Arc) const {
      return Parent::maxArcId();
    }

    int maxId(Edge) const {
      return Parent::maxEdgeId();
    }

    Node fromId(int id, Node) const {
      return Parent::nodeFromId(id);
    }

    Arc fromId(int id, Arc) const {
      return Parent::arcFromId(id);
    }

    Edge fromId(int id, Edge) const {
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

    Arc oppositeArc(const Arc &e) const {
      return Parent::direct(e, !Parent::direction(e));
    }

    using Parent::direct;
    Arc direct(const Edge &e, const Node &s) const {
      return Parent::direct(e, Parent::u(e) == s);
    }

    typedef AlterationNotifier<EdgeSetExtender, Arc> ArcNotifier;
    typedef AlterationNotifier<EdgeSetExtender, Edge> EdgeNotifier;


  protected:

    mutable ArcNotifier arc_notifier;
    mutable EdgeNotifier edge_notifier;

  public:

    using Parent::notifier;

    ArcNotifier& notifier(Arc) const {
      return arc_notifier;
    }

    EdgeNotifier& notifier(Edge) const {
      return edge_notifier;
    }


    class NodeIt : public Node {
      const Graph* graph;
    public:

      NodeIt() {}

      NodeIt(Invalid i) : Node(i) { }

      explicit NodeIt(const Graph& _graph) : graph(&_graph) {
        _graph.first(static_cast<Node&>(*this));
      }

      NodeIt(const Graph& _graph, const Node& node)
        : Node(node), graph(&_graph) {}

      NodeIt& operator++() {
        graph->next(*this);
        return *this;
      }

    };


    class ArcIt : public Arc {
      const Graph* graph;
    public:

      ArcIt() { }

      ArcIt(Invalid i) : Arc(i) { }

      explicit ArcIt(const Graph& _graph) : graph(&_graph) {
        _graph.first(static_cast<Arc&>(*this));
      }

      ArcIt(const Graph& _graph, const Arc& e) :
        Arc(e), graph(&_graph) { }

      ArcIt& operator++() {
        graph->next(*this);
        return *this;
      }

    };


    class OutArcIt : public Arc {
      const Graph* graph;
    public:

      OutArcIt() { }

      OutArcIt(Invalid i) : Arc(i) { }

      OutArcIt(const Graph& _graph, const Node& node)
        : graph(&_graph) {
        _graph.firstOut(*this, node);
      }

      OutArcIt(const Graph& _graph, const Arc& arc)
        : Arc(arc), graph(&_graph) {}

      OutArcIt& operator++() {
        graph->nextOut(*this);
        return *this;
      }

    };


    class InArcIt : public Arc {
      const Graph* graph;
    public:

      InArcIt() { }

      InArcIt(Invalid i) : Arc(i) { }

      InArcIt(const Graph& _graph, const Node& node)
        : graph(&_graph) {
        _graph.firstIn(*this, node);
      }

      InArcIt(const Graph& _graph, const Arc& arc) :
        Arc(arc), graph(&_graph) {}

      InArcIt& operator++() {
        graph->nextIn(*this);
        return *this;
      }

    };


    class EdgeIt : public Parent::Edge {
      const Graph* graph;
    public:

      EdgeIt() { }

      EdgeIt(Invalid i) : Edge(i) { }

      explicit EdgeIt(const Graph& _graph) : graph(&_graph) {
        _graph.first(static_cast<Edge&>(*this));
      }

      EdgeIt(const Graph& _graph, const Edge& e) :
        Edge(e), graph(&_graph) { }

      EdgeIt& operator++() {
        graph->next(*this);
        return *this;
      }

    };

    class IncEdgeIt : public Parent::Edge {
      friend class EdgeSetExtender;
      const Graph* graph;
      bool direction;
    public:

      IncEdgeIt() { }

      IncEdgeIt(Invalid i) : Edge(i), direction(false) { }

      IncEdgeIt(const Graph& _graph, const Node &n) : graph(&_graph) {
        _graph.firstInc(*this, direction, n);
      }

      IncEdgeIt(const Graph& _graph, const Edge &ue, const Node &n)
        : graph(&_graph), Edge(ue) {
        direction = (_graph.source(ue) == n);
      }

      IncEdgeIt& operator++() {
        graph->nextInc(*this, direction);
        return *this;
      }
    };

    // \brief Base node of the iterator
    //
    // Returns the base node (ie. the source in this case) of the iterator
    Node baseNode(const OutArcIt &e) const {
      return Parent::source(static_cast<const Arc&>(e));
    }
    // \brief Running node of the iterator
    //
    // Returns the running node (ie. the target in this case) of the
    // iterator
    Node runningNode(const OutArcIt &e) const {
      return Parent::target(static_cast<const Arc&>(e));
    }

    // \brief Base node of the iterator
    //
    // Returns the base node (ie. the target in this case) of the iterator
    Node baseNode(const InArcIt &e) const {
      return Parent::target(static_cast<const Arc&>(e));
    }
    // \brief Running node of the iterator
    //
    // Returns the running node (ie. the source in this case) of the
    // iterator
    Node runningNode(const InArcIt &e) const {
      return Parent::source(static_cast<const Arc&>(e));
    }

    // Base node of the iterator
    //
    // Returns the base node of the iterator
    Node baseNode(const IncEdgeIt &e) const {
      return e.direction ? this->u(e) : this->v(e);
    }
    // Running node of the iterator
    //
    // Returns the running node of the iterator
    Node runningNode(const IncEdgeIt &e) const {
      return e.direction ? this->v(e) : this->u(e);
    }


    template <typename _Value>
    class ArcMap
      : public MapExtender<DefaultMap<Graph, Arc, _Value> > {
      typedef MapExtender<DefaultMap<Graph, Arc, _Value> > Parent;

    public:
      explicit ArcMap(const Graph& _g)
        : Parent(_g) {}
      ArcMap(const Graph& _g, const _Value& _v)
        : Parent(_g, _v) {}

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
      explicit EdgeMap(const Graph& _g)
        : Parent(_g) {}

      EdgeMap(const Graph& _g, const _Value& _v)
        : Parent(_g, _v) {}

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

    Edge addEdge(const Node& from, const Node& to) {
      Edge edge = Parent::addEdge(from, to);
      notifier(Edge()).add(edge);
      std::vector<Arc> arcs;
      arcs.push_back(Parent::direct(edge, true));
      arcs.push_back(Parent::direct(edge, false));
      notifier(Arc()).add(arcs);
      return edge;
    }

    void clear() {
      notifier(Arc()).clear();
      notifier(Edge()).clear();
      Parent::clear();
    }

    void erase(const Edge& edge) {
      std::vector<Arc> arcs;
      arcs.push_back(Parent::direct(edge, true));
      arcs.push_back(Parent::direct(edge, false));
      notifier(Arc()).erase(arcs);
      notifier(Edge()).erase(edge);
      Parent::erase(edge);
    }


    EdgeSetExtender() {
      arc_notifier.setContainer(*this);
      edge_notifier.setContainer(*this);
    }

    ~EdgeSetExtender() {
      edge_notifier.clear();
      arc_notifier.clear();
    }

  };

}

#endif
