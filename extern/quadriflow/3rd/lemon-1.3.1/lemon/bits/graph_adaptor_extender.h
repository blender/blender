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

#ifndef LEMON_BITS_GRAPH_ADAPTOR_EXTENDER_H
#define LEMON_BITS_GRAPH_ADAPTOR_EXTENDER_H

#include <lemon/core.h>
#include <lemon/error.h>

namespace lemon {

  template <typename _Digraph>
  class DigraphAdaptorExtender : public _Digraph {
    typedef _Digraph Parent;

  public:

    typedef _Digraph Digraph;
    typedef DigraphAdaptorExtender Adaptor;

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

    class NodeIt : public Node {
      const Adaptor* _adaptor;
    public:

      NodeIt() {}

      NodeIt(Invalid i) : Node(i) { }

      explicit NodeIt(const Adaptor& adaptor) : _adaptor(&adaptor) {
        _adaptor->first(static_cast<Node&>(*this));
      }

      NodeIt(const Adaptor& adaptor, const Node& node)
        : Node(node), _adaptor(&adaptor) {}

      NodeIt& operator++() {
        _adaptor->next(*this);
        return *this;
      }

    };


    class ArcIt : public Arc {
      const Adaptor* _adaptor;
    public:

      ArcIt() { }

      ArcIt(Invalid i) : Arc(i) { }

      explicit ArcIt(const Adaptor& adaptor) : _adaptor(&adaptor) {
        _adaptor->first(static_cast<Arc&>(*this));
      }

      ArcIt(const Adaptor& adaptor, const Arc& e) :
        Arc(e), _adaptor(&adaptor) { }

      ArcIt& operator++() {
        _adaptor->next(*this);
        return *this;
      }

    };


    class OutArcIt : public Arc {
      const Adaptor* _adaptor;
    public:

      OutArcIt() { }

      OutArcIt(Invalid i) : Arc(i) { }

      OutArcIt(const Adaptor& adaptor, const Node& node)
        : _adaptor(&adaptor) {
        _adaptor->firstOut(*this, node);
      }

      OutArcIt(const Adaptor& adaptor, const Arc& arc)
        : Arc(arc), _adaptor(&adaptor) {}

      OutArcIt& operator++() {
        _adaptor->nextOut(*this);
        return *this;
      }

    };


    class InArcIt : public Arc {
      const Adaptor* _adaptor;
    public:

      InArcIt() { }

      InArcIt(Invalid i) : Arc(i) { }

      InArcIt(const Adaptor& adaptor, const Node& node)
        : _adaptor(&adaptor) {
        _adaptor->firstIn(*this, node);
      }

      InArcIt(const Adaptor& adaptor, const Arc& arc) :
        Arc(arc), _adaptor(&adaptor) {}

      InArcIt& operator++() {
        _adaptor->nextIn(*this);
        return *this;
      }

    };

    Node baseNode(const OutArcIt &e) const {
      return Parent::source(e);
    }
    Node runningNode(const OutArcIt &e) const {
      return Parent::target(e);
    }

    Node baseNode(const InArcIt &e) const {
      return Parent::target(e);
    }
    Node runningNode(const InArcIt &e) const {
      return Parent::source(e);
    }

  };

  template <typename _Graph>
  class GraphAdaptorExtender : public _Graph {
    typedef _Graph Parent;

  public:

    typedef _Graph Graph;
    typedef GraphAdaptorExtender Adaptor;

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

    Arc oppositeArc(const Arc &a) const {
      return Parent::direct(a, !Parent::direction(a));
    }

    using Parent::direct;
    Arc direct(const Edge &e, const Node &s) const {
      return Parent::direct(e, Parent::u(e) == s);
    }


    class NodeIt : public Node {
      const Adaptor* _adaptor;
    public:

      NodeIt() {}

      NodeIt(Invalid i) : Node(i) { }

      explicit NodeIt(const Adaptor& adaptor) : _adaptor(&adaptor) {
        _adaptor->first(static_cast<Node&>(*this));
      }

      NodeIt(const Adaptor& adaptor, const Node& node)
        : Node(node), _adaptor(&adaptor) {}

      NodeIt& operator++() {
        _adaptor->next(*this);
        return *this;
      }

    };


    class ArcIt : public Arc {
      const Adaptor* _adaptor;
    public:

      ArcIt() { }

      ArcIt(Invalid i) : Arc(i) { }

      explicit ArcIt(const Adaptor& adaptor) : _adaptor(&adaptor) {
        _adaptor->first(static_cast<Arc&>(*this));
      }

      ArcIt(const Adaptor& adaptor, const Arc& e) :
        Arc(e), _adaptor(&adaptor) { }

      ArcIt& operator++() {
        _adaptor->next(*this);
        return *this;
      }

    };


    class OutArcIt : public Arc {
      const Adaptor* _adaptor;
    public:

      OutArcIt() { }

      OutArcIt(Invalid i) : Arc(i) { }

      OutArcIt(const Adaptor& adaptor, const Node& node)
        : _adaptor(&adaptor) {
        _adaptor->firstOut(*this, node);
      }

      OutArcIt(const Adaptor& adaptor, const Arc& arc)
        : Arc(arc), _adaptor(&adaptor) {}

      OutArcIt& operator++() {
        _adaptor->nextOut(*this);
        return *this;
      }

    };


    class InArcIt : public Arc {
      const Adaptor* _adaptor;
    public:

      InArcIt() { }

      InArcIt(Invalid i) : Arc(i) { }

      InArcIt(const Adaptor& adaptor, const Node& node)
        : _adaptor(&adaptor) {
        _adaptor->firstIn(*this, node);
      }

      InArcIt(const Adaptor& adaptor, const Arc& arc) :
        Arc(arc), _adaptor(&adaptor) {}

      InArcIt& operator++() {
        _adaptor->nextIn(*this);
        return *this;
      }

    };

    class EdgeIt : public Parent::Edge {
      const Adaptor* _adaptor;
    public:

      EdgeIt() { }

      EdgeIt(Invalid i) : Edge(i) { }

      explicit EdgeIt(const Adaptor& adaptor) : _adaptor(&adaptor) {
        _adaptor->first(static_cast<Edge&>(*this));
      }

      EdgeIt(const Adaptor& adaptor, const Edge& e) :
        Edge(e), _adaptor(&adaptor) { }

      EdgeIt& operator++() {
        _adaptor->next(*this);
        return *this;
      }

    };

    class IncEdgeIt : public Edge {
      friend class GraphAdaptorExtender;
      const Adaptor* _adaptor;
      bool direction;
    public:

      IncEdgeIt() { }

      IncEdgeIt(Invalid i) : Edge(i), direction(false) { }

      IncEdgeIt(const Adaptor& adaptor, const Node &n) : _adaptor(&adaptor) {
        _adaptor->firstInc(static_cast<Edge&>(*this), direction, n);
      }

      IncEdgeIt(const Adaptor& adaptor, const Edge &e, const Node &n)
        : _adaptor(&adaptor), Edge(e) {
        direction = (_adaptor->u(e) == n);
      }

      IncEdgeIt& operator++() {
        _adaptor->nextInc(*this, direction);
        return *this;
      }
    };

    Node baseNode(const OutArcIt &a) const {
      return Parent::source(a);
    }
    Node runningNode(const OutArcIt &a) const {
      return Parent::target(a);
    }

    Node baseNode(const InArcIt &a) const {
      return Parent::target(a);
    }
    Node runningNode(const InArcIt &a) const {
      return Parent::source(a);
    }

    Node baseNode(const IncEdgeIt &e) const {
      return e.direction ? Parent::u(e) : Parent::v(e);
    }
    Node runningNode(const IncEdgeIt &e) const {
      return e.direction ? Parent::v(e) : Parent::u(e);
    }

  };

}


#endif
