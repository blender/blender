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

///\ingroup graph_concepts
///\file
///\brief The concept of undirected graphs.

#ifndef LEMON_CONCEPTS_GRAPH_H
#define LEMON_CONCEPTS_GRAPH_H

#include <lemon/concepts/graph_components.h>
#include <lemon/concepts/maps.h>
#include <lemon/concept_check.h>
#include <lemon/core.h>

namespace lemon {
  namespace concepts {

    /// \ingroup graph_concepts
    ///
    /// \brief Class describing the concept of undirected graphs.
    ///
    /// This class describes the common interface of all undirected
    /// graphs.
    ///
    /// Like all concept classes, it only provides an interface
    /// without any sensible implementation. So any general algorithm for
    /// undirected graphs should compile with this class, but it will not
    /// run properly, of course.
    /// An actual graph implementation like \ref ListGraph or
    /// \ref SmartGraph may have additional functionality.
    ///
    /// The undirected graphs also fulfill the concept of \ref Digraph
    /// "directed graphs", since each edge can also be regarded as two
    /// oppositely directed arcs.
    /// Undirected graphs provide an Edge type for the undirected edges and
    /// an Arc type for the directed arcs. The Arc type is convertible to
    /// Edge or inherited from it, i.e. the corresponding edge can be
    /// obtained from an arc.
    /// EdgeIt and EdgeMap classes can be used for the edges, while ArcIt
    /// and ArcMap classes can be used for the arcs (just like in digraphs).
    /// Both InArcIt and OutArcIt iterates on the same edges but with
    /// opposite direction. IncEdgeIt also iterates on the same edges
    /// as OutArcIt and InArcIt, but it is not convertible to Arc,
    /// only to Edge.
    ///
    /// In LEMON, each undirected edge has an inherent orientation.
    /// Thus it can defined if an arc is forward or backward oriented in
    /// an undirected graph with respect to this default oriantation of
    /// the represented edge.
    /// With the direction() and direct() functions the direction
    /// of an arc can be obtained and set, respectively.
    ///
    /// Only nodes and edges can be added to or removed from an undirected
    /// graph and the corresponding arcs are added or removed automatically.
    ///
    /// \sa Digraph
    class Graph {
    private:
      /// Graphs are \e not copy constructible. Use GraphCopy instead.
      Graph(const Graph&) {}
      /// \brief Assignment of a graph to another one is \e not allowed.
      /// Use GraphCopy instead.
      void operator=(const Graph&) {}

    public:
      /// Default constructor.
      Graph() {}

      /// \brief Undirected graphs should be tagged with \c UndirectedTag.
      ///
      /// Undirected graphs should be tagged with \c UndirectedTag.
      ///
      /// This tag helps the \c enable_if technics to make compile time
      /// specializations for undirected graphs.
      typedef True UndirectedTag;

      /// The node type of the graph

      /// This class identifies a node of the graph. It also serves
      /// as a base class of the node iterators,
      /// thus they convert to this type.
      class Node {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the object to an undefined value.
        Node() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        Node(const Node&) { }

        /// %Invalid constructor \& conversion.

        /// Initializes the object to be invalid.
        /// \sa Invalid for more details.
        Node(Invalid) { }
        /// Equality operator

        /// Equality operator.
        ///
        /// Two iterators are equal if and only if they point to the
        /// same object or both are \c INVALID.
        bool operator==(Node) const { return true; }

        /// Inequality operator

        /// Inequality operator.
        bool operator!=(Node) const { return true; }

        /// Artificial ordering operator.

        /// Artificial ordering operator.
        ///
        /// \note This operator only has to define some strict ordering of
        /// the items; this order has nothing to do with the iteration
        /// ordering of the items.
        bool operator<(Node) const { return false; }

      };

      /// Iterator class for the nodes.

      /// This iterator goes through each node of the graph.
      /// Its usage is quite simple, for example, you can count the number
      /// of nodes in a graph \c g of type \c %Graph like this:
      ///\code
      /// int count=0;
      /// for (Graph::NodeIt n(g); n!=INVALID; ++n) ++count;
      ///\endcode
      class NodeIt : public Node {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the iterator to an undefined value.
        NodeIt() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        NodeIt(const NodeIt& n) : Node(n) { }
        /// %Invalid constructor \& conversion.

        /// Initializes the iterator to be invalid.
        /// \sa Invalid for more details.
        NodeIt(Invalid) { }
        /// Sets the iterator to the first node.

        /// Sets the iterator to the first node of the given digraph.
        ///
        explicit NodeIt(const Graph&) { }
        /// Sets the iterator to the given node.

        /// Sets the iterator to the given node of the given digraph.
        ///
        NodeIt(const Graph&, const Node&) { }
        /// Next node.

        /// Assign the iterator to the next node.
        ///
        NodeIt& operator++() { return *this; }
      };


      /// The edge type of the graph

      /// This class identifies an edge of the graph. It also serves
      /// as a base class of the edge iterators,
      /// thus they will convert to this type.
      class Edge {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the object to an undefined value.
        Edge() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        Edge(const Edge&) { }
        /// %Invalid constructor \& conversion.

        /// Initializes the object to be invalid.
        /// \sa Invalid for more details.
        Edge(Invalid) { }
        /// Equality operator

        /// Equality operator.
        ///
        /// Two iterators are equal if and only if they point to the
        /// same object or both are \c INVALID.
        bool operator==(Edge) const { return true; }
        /// Inequality operator

        /// Inequality operator.
        bool operator!=(Edge) const { return true; }

        /// Artificial ordering operator.

        /// Artificial ordering operator.
        ///
        /// \note This operator only has to define some strict ordering of
        /// the edges; this order has nothing to do with the iteration
        /// ordering of the edges.
        bool operator<(Edge) const { return false; }
      };

      /// Iterator class for the edges.

      /// This iterator goes through each edge of the graph.
      /// Its usage is quite simple, for example, you can count the number
      /// of edges in a graph \c g of type \c %Graph as follows:
      ///\code
      /// int count=0;
      /// for(Graph::EdgeIt e(g); e!=INVALID; ++e) ++count;
      ///\endcode
      class EdgeIt : public Edge {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the iterator to an undefined value.
        EdgeIt() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        EdgeIt(const EdgeIt& e) : Edge(e) { }
        /// %Invalid constructor \& conversion.

        /// Initializes the iterator to be invalid.
        /// \sa Invalid for more details.
        EdgeIt(Invalid) { }
        /// Sets the iterator to the first edge.

        /// Sets the iterator to the first edge of the given graph.
        ///
        explicit EdgeIt(const Graph&) { }
        /// Sets the iterator to the given edge.

        /// Sets the iterator to the given edge of the given graph.
        ///
        EdgeIt(const Graph&, const Edge&) { }
        /// Next edge

        /// Assign the iterator to the next edge.
        ///
        EdgeIt& operator++() { return *this; }
      };

      /// Iterator class for the incident edges of a node.

      /// This iterator goes trough the incident undirected edges
      /// of a certain node of a graph.
      /// Its usage is quite simple, for example, you can compute the
      /// degree (i.e. the number of incident edges) of a node \c n
      /// in a graph \c g of type \c %Graph as follows.
      ///
      ///\code
      /// int count=0;
      /// for(Graph::IncEdgeIt e(g, n); e!=INVALID; ++e) ++count;
      ///\endcode
      ///
      /// \warning Loop edges will be iterated twice.
      class IncEdgeIt : public Edge {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the iterator to an undefined value.
        IncEdgeIt() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        IncEdgeIt(const IncEdgeIt& e) : Edge(e) { }
        /// %Invalid constructor \& conversion.

        /// Initializes the iterator to be invalid.
        /// \sa Invalid for more details.
        IncEdgeIt(Invalid) { }
        /// Sets the iterator to the first incident edge.

        /// Sets the iterator to the first incident edge of the given node.
        ///
        IncEdgeIt(const Graph&, const Node&) { }
        /// Sets the iterator to the given edge.

        /// Sets the iterator to the given edge of the given graph.
        ///
        IncEdgeIt(const Graph&, const Edge&) { }
        /// Next incident edge

        /// Assign the iterator to the next incident edge
        /// of the corresponding node.
        IncEdgeIt& operator++() { return *this; }
      };

      /// The arc type of the graph

      /// This class identifies a directed arc of the graph. It also serves
      /// as a base class of the arc iterators,
      /// thus they will convert to this type.
      class Arc {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the object to an undefined value.
        Arc() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        Arc(const Arc&) { }
        /// %Invalid constructor \& conversion.

        /// Initializes the object to be invalid.
        /// \sa Invalid for more details.
        Arc(Invalid) { }
        /// Equality operator

        /// Equality operator.
        ///
        /// Two iterators are equal if and only if they point to the
        /// same object or both are \c INVALID.
        bool operator==(Arc) const { return true; }
        /// Inequality operator

        /// Inequality operator.
        bool operator!=(Arc) const { return true; }

        /// Artificial ordering operator.

        /// Artificial ordering operator.
        ///
        /// \note This operator only has to define some strict ordering of
        /// the arcs; this order has nothing to do with the iteration
        /// ordering of the arcs.
        bool operator<(Arc) const { return false; }

        /// Converison to \c Edge

        /// Converison to \c Edge.
        ///
        operator Edge() const { return Edge(); }
      };

      /// Iterator class for the arcs.

      /// This iterator goes through each directed arc of the graph.
      /// Its usage is quite simple, for example, you can count the number
      /// of arcs in a graph \c g of type \c %Graph as follows:
      ///\code
      /// int count=0;
      /// for(Graph::ArcIt a(g); a!=INVALID; ++a) ++count;
      ///\endcode
      class ArcIt : public Arc {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the iterator to an undefined value.
        ArcIt() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        ArcIt(const ArcIt& e) : Arc(e) { }
        /// %Invalid constructor \& conversion.

        /// Initializes the iterator to be invalid.
        /// \sa Invalid for more details.
        ArcIt(Invalid) { }
        /// Sets the iterator to the first arc.

        /// Sets the iterator to the first arc of the given graph.
        ///
        explicit ArcIt(const Graph &g) {
          ::lemon::ignore_unused_variable_warning(g);
        }
        /// Sets the iterator to the given arc.

        /// Sets the iterator to the given arc of the given graph.
        ///
        ArcIt(const Graph&, const Arc&) { }
        /// Next arc

        /// Assign the iterator to the next arc.
        ///
        ArcIt& operator++() { return *this; }
      };

      /// Iterator class for the outgoing arcs of a node.

      /// This iterator goes trough the \e outgoing directed arcs of a
      /// certain node of a graph.
      /// Its usage is quite simple, for example, you can count the number
      /// of outgoing arcs of a node \c n
      /// in a graph \c g of type \c %Graph as follows.
      ///\code
      /// int count=0;
      /// for (Digraph::OutArcIt a(g, n); a!=INVALID; ++a) ++count;
      ///\endcode
      class OutArcIt : public Arc {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the iterator to an undefined value.
        OutArcIt() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        OutArcIt(const OutArcIt& e) : Arc(e) { }
        /// %Invalid constructor \& conversion.

        /// Initializes the iterator to be invalid.
        /// \sa Invalid for more details.
        OutArcIt(Invalid) { }
        /// Sets the iterator to the first outgoing arc.

        /// Sets the iterator to the first outgoing arc of the given node.
        ///
        OutArcIt(const Graph& n, const Node& g) {
          ::lemon::ignore_unused_variable_warning(n);
          ::lemon::ignore_unused_variable_warning(g);
        }
        /// Sets the iterator to the given arc.

        /// Sets the iterator to the given arc of the given graph.
        ///
        OutArcIt(const Graph&, const Arc&) { }
        /// Next outgoing arc

        /// Assign the iterator to the next
        /// outgoing arc of the corresponding node.
        OutArcIt& operator++() { return *this; }
      };

      /// Iterator class for the incoming arcs of a node.

      /// This iterator goes trough the \e incoming directed arcs of a
      /// certain node of a graph.
      /// Its usage is quite simple, for example, you can count the number
      /// of incoming arcs of a node \c n
      /// in a graph \c g of type \c %Graph as follows.
      ///\code
      /// int count=0;
      /// for (Digraph::InArcIt a(g, n); a!=INVALID; ++a) ++count;
      ///\endcode
      class InArcIt : public Arc {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the iterator to an undefined value.
        InArcIt() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        InArcIt(const InArcIt& e) : Arc(e) { }
        /// %Invalid constructor \& conversion.

        /// Initializes the iterator to be invalid.
        /// \sa Invalid for more details.
        InArcIt(Invalid) { }
        /// Sets the iterator to the first incoming arc.

        /// Sets the iterator to the first incoming arc of the given node.
        ///
        InArcIt(const Graph& g, const Node& n) {
          ::lemon::ignore_unused_variable_warning(n);
          ::lemon::ignore_unused_variable_warning(g);
        }
        /// Sets the iterator to the given arc.

        /// Sets the iterator to the given arc of the given graph.
        ///
        InArcIt(const Graph&, const Arc&) { }
        /// Next incoming arc

        /// Assign the iterator to the next
        /// incoming arc of the corresponding node.
        InArcIt& operator++() { return *this; }
      };

      /// \brief Standard graph map type for the nodes.
      ///
      /// Standard graph map type for the nodes.
      /// It conforms to the ReferenceMap concept.
      template<class T>
      class NodeMap : public ReferenceMap<Node, T, T&, const T&>
      {
      public:

        /// Constructor
        explicit NodeMap(const Graph&) { }
        /// Constructor with given initial value
        NodeMap(const Graph&, T) { }

      private:
        ///Copy constructor
        NodeMap(const NodeMap& nm) :
          ReferenceMap<Node, T, T&, const T&>(nm) { }
        ///Assignment operator
        template <typename CMap>
        NodeMap& operator=(const CMap&) {
          checkConcept<ReadMap<Node, T>, CMap>();
          return *this;
        }
      };

      /// \brief Standard graph map type for the arcs.
      ///
      /// Standard graph map type for the arcs.
      /// It conforms to the ReferenceMap concept.
      template<class T>
      class ArcMap : public ReferenceMap<Arc, T, T&, const T&>
      {
      public:

        /// Constructor
        explicit ArcMap(const Graph&) { }
        /// Constructor with given initial value
        ArcMap(const Graph&, T) { }

      private:
        ///Copy constructor
        ArcMap(const ArcMap& em) :
          ReferenceMap<Arc, T, T&, const T&>(em) { }
        ///Assignment operator
        template <typename CMap>
        ArcMap& operator=(const CMap&) {
          checkConcept<ReadMap<Arc, T>, CMap>();
          return *this;
        }
      };

      /// \brief Standard graph map type for the edges.
      ///
      /// Standard graph map type for the edges.
      /// It conforms to the ReferenceMap concept.
      template<class T>
      class EdgeMap : public ReferenceMap<Edge, T, T&, const T&>
      {
      public:

        /// Constructor
        explicit EdgeMap(const Graph&) { }
        /// Constructor with given initial value
        EdgeMap(const Graph&, T) { }

      private:
        ///Copy constructor
        EdgeMap(const EdgeMap& em) :
          ReferenceMap<Edge, T, T&, const T&>(em) {}
        ///Assignment operator
        template <typename CMap>
        EdgeMap& operator=(const CMap&) {
          checkConcept<ReadMap<Edge, T>, CMap>();
          return *this;
        }
      };

      /// \brief The first node of the edge.
      ///
      /// Returns the first node of the given edge.
      ///
      /// Edges don't have source and target nodes, however, methods
      /// u() and v() are used to query the two end-nodes of an edge.
      /// The orientation of an edge that arises this way is called
      /// the inherent direction, it is used to define the default
      /// direction for the corresponding arcs.
      /// \sa v()
      /// \sa direction()
      Node u(Edge) const { return INVALID; }

      /// \brief The second node of the edge.
      ///
      /// Returns the second node of the given edge.
      ///
      /// Edges don't have source and target nodes, however, methods
      /// u() and v() are used to query the two end-nodes of an edge.
      /// The orientation of an edge that arises this way is called
      /// the inherent direction, it is used to define the default
      /// direction for the corresponding arcs.
      /// \sa u()
      /// \sa direction()
      Node v(Edge) const { return INVALID; }

      /// \brief The source node of the arc.
      ///
      /// Returns the source node of the given arc.
      Node source(Arc) const { return INVALID; }

      /// \brief The target node of the arc.
      ///
      /// Returns the target node of the given arc.
      Node target(Arc) const { return INVALID; }

      /// \brief The ID of the node.
      ///
      /// Returns the ID of the given node.
      int id(Node) const { return -1; }

      /// \brief The ID of the edge.
      ///
      /// Returns the ID of the given edge.
      int id(Edge) const { return -1; }

      /// \brief The ID of the arc.
      ///
      /// Returns the ID of the given arc.
      int id(Arc) const { return -1; }

      /// \brief The node with the given ID.
      ///
      /// Returns the node with the given ID.
      /// \pre The argument should be a valid node ID in the graph.
      Node nodeFromId(int) const { return INVALID; }

      /// \brief The edge with the given ID.
      ///
      /// Returns the edge with the given ID.
      /// \pre The argument should be a valid edge ID in the graph.
      Edge edgeFromId(int) const { return INVALID; }

      /// \brief The arc with the given ID.
      ///
      /// Returns the arc with the given ID.
      /// \pre The argument should be a valid arc ID in the graph.
      Arc arcFromId(int) const { return INVALID; }

      /// \brief An upper bound on the node IDs.
      ///
      /// Returns an upper bound on the node IDs.
      int maxNodeId() const { return -1; }

      /// \brief An upper bound on the edge IDs.
      ///
      /// Returns an upper bound on the edge IDs.
      int maxEdgeId() const { return -1; }

      /// \brief An upper bound on the arc IDs.
      ///
      /// Returns an upper bound on the arc IDs.
      int maxArcId() const { return -1; }

      /// \brief The direction of the arc.
      ///
      /// Returns \c true if the direction of the given arc is the same as
      /// the inherent orientation of the represented edge.
      bool direction(Arc) const { return true; }

      /// \brief Direct the edge.
      ///
      /// Direct the given edge. The returned arc
      /// represents the given edge and its direction comes
      /// from the bool parameter. If it is \c true, then the direction
      /// of the arc is the same as the inherent orientation of the edge.
      Arc direct(Edge, bool) const {
        return INVALID;
      }

      /// \brief Direct the edge.
      ///
      /// Direct the given edge. The returned arc represents the given
      /// edge and its source node is the given node.
      Arc direct(Edge, Node) const {
        return INVALID;
      }

      /// \brief The oppositely directed arc.
      ///
      /// Returns the oppositely directed arc representing the same edge.
      Arc oppositeArc(Arc) const { return INVALID; }

      /// \brief The opposite node on the edge.
      ///
      /// Returns the opposite node on the given edge.
      Node oppositeNode(Node, Edge) const { return INVALID; }

      void first(Node&) const {}
      void next(Node&) const {}

      void first(Edge&) const {}
      void next(Edge&) const {}

      void first(Arc&) const {}
      void next(Arc&) const {}

      void firstOut(Arc&, Node) const {}
      void nextOut(Arc&) const {}

      void firstIn(Arc&, Node) const {}
      void nextIn(Arc&) const {}

      void firstInc(Edge &, bool &, const Node &) const {}
      void nextInc(Edge &, bool &) const {}

      // The second parameter is dummy.
      Node fromId(int, Node) const { return INVALID; }
      // The second parameter is dummy.
      Edge fromId(int, Edge) const { return INVALID; }
      // The second parameter is dummy.
      Arc fromId(int, Arc) const { return INVALID; }

      // Dummy parameter.
      int maxId(Node) const { return -1; }
      // Dummy parameter.
      int maxId(Edge) const { return -1; }
      // Dummy parameter.
      int maxId(Arc) const { return -1; }

      /// \brief The base node of the iterator.
      ///
      /// Returns the base node of the given incident edge iterator.
      Node baseNode(IncEdgeIt) const { return INVALID; }

      /// \brief The running node of the iterator.
      ///
      /// Returns the running node of the given incident edge iterator.
      Node runningNode(IncEdgeIt) const { return INVALID; }

      /// \brief The base node of the iterator.
      ///
      /// Returns the base node of the given outgoing arc iterator
      /// (i.e. the source node of the corresponding arc).
      Node baseNode(OutArcIt) const { return INVALID; }

      /// \brief The running node of the iterator.
      ///
      /// Returns the running node of the given outgoing arc iterator
      /// (i.e. the target node of the corresponding arc).
      Node runningNode(OutArcIt) const { return INVALID; }

      /// \brief The base node of the iterator.
      ///
      /// Returns the base node of the given incoming arc iterator
      /// (i.e. the target node of the corresponding arc).
      Node baseNode(InArcIt) const { return INVALID; }

      /// \brief The running node of the iterator.
      ///
      /// Returns the running node of the given incoming arc iterator
      /// (i.e. the source node of the corresponding arc).
      Node runningNode(InArcIt) const { return INVALID; }

      template <typename _Graph>
      struct Constraints {
        void constraints() {
          checkConcept<BaseGraphComponent, _Graph>();
          checkConcept<IterableGraphComponent<>, _Graph>();
          checkConcept<IDableGraphComponent<>, _Graph>();
          checkConcept<MappableGraphComponent<>, _Graph>();
        }
      };

    };

  }

}

#endif
