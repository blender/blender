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

#ifndef LEMON_CONCEPTS_BPGRAPH_H
#define LEMON_CONCEPTS_BPGRAPH_H

#include <lemon/concepts/graph_components.h>
#include <lemon/concepts/maps.h>
#include <lemon/concept_check.h>
#include <lemon/core.h>

namespace lemon {
  namespace concepts {

    /// \ingroup graph_concepts
    ///
    /// \brief Class describing the concept of undirected bipartite graphs.
    ///
    /// This class describes the common interface of all undirected
    /// bipartite graphs.
    ///
    /// Like all concept classes, it only provides an interface
    /// without any sensible implementation. So any general algorithm for
    /// undirected bipartite graphs should compile with this class,
    /// but it will not run properly, of course.
    /// An actual graph implementation like \ref ListBpGraph or
    /// \ref SmartBpGraph may have additional functionality.
    ///
    /// The bipartite graphs also fulfill the concept of \ref Graph
    /// "undirected graphs". Bipartite graphs provide a bipartition of
    /// the node set, namely a red and blue set of the nodes. The
    /// nodes can be iterated with the RedNodeIt and BlueNodeIt in the
    /// two node sets. With RedNodeMap and BlueNodeMap values can be
    /// assigned to the nodes in the two sets.
    ///
    /// The edges of the graph cannot connect two nodes of the same
    /// set. The edges inherent orientation is from the red nodes to
    /// the blue nodes.
    ///
    /// \sa Graph
    class BpGraph {
    private:
      /// BpGraphs are \e not copy constructible. Use bpGraphCopy instead.
      BpGraph(const BpGraph&) {}
      /// \brief Assignment of a graph to another one is \e not allowed.
      /// Use bpGraphCopy instead.
      void operator=(const BpGraph&) {}

    public:
      /// Default constructor.
      BpGraph() {}

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

      /// Class to represent red nodes.

      /// This class represents the red nodes of the graph. It does
      /// not supposed to be used directly, because the nodes can be
      /// represented as Node instances. This class can be used as
      /// template parameter for special map classes.
      class RedNode : public Node {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the object to an undefined value.
        RedNode() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        RedNode(const RedNode&) : Node() { }

        /// %Invalid constructor \& conversion.

        /// Initializes the object to be invalid.
        /// \sa Invalid for more details.
        RedNode(Invalid) { }

      };

      /// Class to represent blue nodes.

      /// This class represents the blue nodes of the graph. It does
      /// not supposed to be used directly, because the nodes can be
      /// represented as Node instances. This class can be used as
      /// template parameter for special map classes.
      class BlueNode : public Node {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the object to an undefined value.
        BlueNode() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        BlueNode(const BlueNode&) : Node() { }

        /// %Invalid constructor \& conversion.

        /// Initializes the object to be invalid.
        /// \sa Invalid for more details.
        BlueNode(Invalid) { }

      };

      /// Iterator class for the red nodes.

      /// This iterator goes through each red node of the graph.
      /// Its usage is quite simple, for example, you can count the number
      /// of red nodes in a graph \c g of type \c %BpGraph like this:
      ///\code
      /// int count=0;
      /// for (BpGraph::RedNodeIt n(g); n!=INVALID; ++n) ++count;
      ///\endcode
      class RedNodeIt : public RedNode {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the iterator to an undefined value.
        RedNodeIt() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        RedNodeIt(const RedNodeIt& n) : RedNode(n) { }
        /// %Invalid constructor \& conversion.

        /// Initializes the iterator to be invalid.
        /// \sa Invalid for more details.
        RedNodeIt(Invalid) { }
        /// Sets the iterator to the first red node.

        /// Sets the iterator to the first red node of the given
        /// digraph.
        explicit RedNodeIt(const BpGraph&) { }
        /// Sets the iterator to the given red node.

        /// Sets the iterator to the given red node of the given
        /// digraph.
        RedNodeIt(const BpGraph&, const RedNode&) { }
        /// Next node.

        /// Assign the iterator to the next red node.
        ///
        RedNodeIt& operator++() { return *this; }
      };

      /// Iterator class for the blue nodes.

      /// This iterator goes through each blue node of the graph.
      /// Its usage is quite simple, for example, you can count the number
      /// of blue nodes in a graph \c g of type \c %BpGraph like this:
      ///\code
      /// int count=0;
      /// for (BpGraph::BlueNodeIt n(g); n!=INVALID; ++n) ++count;
      ///\endcode
      class BlueNodeIt : public BlueNode {
      public:
        /// Default constructor

        /// Default constructor.
        /// \warning It sets the iterator to an undefined value.
        BlueNodeIt() { }
        /// Copy constructor.

        /// Copy constructor.
        ///
        BlueNodeIt(const BlueNodeIt& n) : BlueNode(n) { }
        /// %Invalid constructor \& conversion.

        /// Initializes the iterator to be invalid.
        /// \sa Invalid for more details.
        BlueNodeIt(Invalid) { }
        /// Sets the iterator to the first blue node.

        /// Sets the iterator to the first blue node of the given
        /// digraph.
        explicit BlueNodeIt(const BpGraph&) { }
        /// Sets the iterator to the given blue node.

        /// Sets the iterator to the given blue node of the given
        /// digraph.
        BlueNodeIt(const BpGraph&, const BlueNode&) { }
        /// Next node.

        /// Assign the iterator to the next blue node.
        ///
        BlueNodeIt& operator++() { return *this; }
      };

      /// Iterator class for the nodes.

      /// This iterator goes through each node of the graph.
      /// Its usage is quite simple, for example, you can count the number
      /// of nodes in a graph \c g of type \c %BpGraph like this:
      ///\code
      /// int count=0;
      /// for (BpGraph::NodeIt n(g); n!=INVALID; ++n) ++count;
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
        explicit NodeIt(const BpGraph&) { }
        /// Sets the iterator to the given node.

        /// Sets the iterator to the given node of the given digraph.
        ///
        NodeIt(const BpGraph&, const Node&) { }
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
      /// of edges in a graph \c g of type \c %BpGraph as follows:
      ///\code
      /// int count=0;
      /// for(BpGraph::EdgeIt e(g); e!=INVALID; ++e) ++count;
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
        explicit EdgeIt(const BpGraph&) { }
        /// Sets the iterator to the given edge.

        /// Sets the iterator to the given edge of the given graph.
        ///
        EdgeIt(const BpGraph&, const Edge&) { }
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
      /// in a graph \c g of type \c %BpGraph as follows.
      ///
      ///\code
      /// int count=0;
      /// for(BpGraph::IncEdgeIt e(g, n); e!=INVALID; ++e) ++count;
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
        IncEdgeIt(const BpGraph&, const Node&) { }
        /// Sets the iterator to the given edge.

        /// Sets the iterator to the given edge of the given graph.
        ///
        IncEdgeIt(const BpGraph&, const Edge&) { }
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
      /// of arcs in a graph \c g of type \c %BpGraph as follows:
      ///\code
      /// int count=0;
      /// for(BpGraph::ArcIt a(g); a!=INVALID; ++a) ++count;
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
        explicit ArcIt(const BpGraph &g)
        {
          ::lemon::ignore_unused_variable_warning(g);
        }
        /// Sets the iterator to the given arc.

        /// Sets the iterator to the given arc of the given graph.
        ///
        ArcIt(const BpGraph&, const Arc&) { }
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
      /// in a graph \c g of type \c %BpGraph as follows.
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
        OutArcIt(const BpGraph& n, const Node& g) {
          ::lemon::ignore_unused_variable_warning(n);
          ::lemon::ignore_unused_variable_warning(g);
        }
        /// Sets the iterator to the given arc.

        /// Sets the iterator to the given arc of the given graph.
        ///
        OutArcIt(const BpGraph&, const Arc&) { }
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
      /// in a graph \c g of type \c %BpGraph as follows.
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
        InArcIt(const BpGraph& g, const Node& n) {
          ::lemon::ignore_unused_variable_warning(n);
          ::lemon::ignore_unused_variable_warning(g);
        }
        /// Sets the iterator to the given arc.

        /// Sets the iterator to the given arc of the given graph.
        ///
        InArcIt(const BpGraph&, const Arc&) { }
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
        explicit NodeMap(const BpGraph&) { }
        /// Constructor with given initial value
        NodeMap(const BpGraph&, T) { }

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

      /// \brief Standard graph map type for the red nodes.
      ///
      /// Standard graph map type for the red nodes.
      /// It conforms to the ReferenceMap concept.
      template<class T>
      class RedNodeMap : public ReferenceMap<Node, T, T&, const T&>
      {
      public:

        /// Constructor
        explicit RedNodeMap(const BpGraph&) { }
        /// Constructor with given initial value
        RedNodeMap(const BpGraph&, T) { }

      private:
        ///Copy constructor
        RedNodeMap(const RedNodeMap& nm) :
          ReferenceMap<Node, T, T&, const T&>(nm) { }
        ///Assignment operator
        template <typename CMap>
        RedNodeMap& operator=(const CMap&) {
          checkConcept<ReadMap<Node, T>, CMap>();
          return *this;
        }
      };

      /// \brief Standard graph map type for the blue nodes.
      ///
      /// Standard graph map type for the blue nodes.
      /// It conforms to the ReferenceMap concept.
      template<class T>
      class BlueNodeMap : public ReferenceMap<Node, T, T&, const T&>
      {
      public:

        /// Constructor
        explicit BlueNodeMap(const BpGraph&) { }
        /// Constructor with given initial value
        BlueNodeMap(const BpGraph&, T) { }

      private:
        ///Copy constructor
        BlueNodeMap(const BlueNodeMap& nm) :
          ReferenceMap<Node, T, T&, const T&>(nm) { }
        ///Assignment operator
        template <typename CMap>
        BlueNodeMap& operator=(const CMap&) {
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
        explicit ArcMap(const BpGraph&) { }
        /// Constructor with given initial value
        ArcMap(const BpGraph&, T) { }

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
        explicit EdgeMap(const BpGraph&) { }
        /// Constructor with given initial value
        EdgeMap(const BpGraph&, T) { }

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

      /// \brief Gives back %true for red nodes.
      ///
      /// Gives back %true for red nodes.
      bool red(const Node&) const { return true; }

      /// \brief Gives back %true for blue nodes.
      ///
      /// Gives back %true for blue nodes.
      bool blue(const Node&) const { return true; }

      /// \brief Converts the node to red node object.
      ///
      /// This function converts unsafely the node to red node
      /// object. It should be called only if the node is from the red
      /// partition or INVALID.
      RedNode asRedNodeUnsafe(const Node&) const { return RedNode(); }

      /// \brief Converts the node to blue node object.
      ///
      /// This function converts unsafely the node to blue node
      /// object. It should be called only if the node is from the red
      /// partition or INVALID.
      BlueNode asBlueNodeUnsafe(const Node&) const { return BlueNode(); }

      /// \brief Converts the node to red node object.
      ///
      /// This function converts safely the node to red node
      /// object. If the node is not from the red partition, then it
      /// returns INVALID.
      RedNode asRedNode(const Node&) const { return RedNode(); }

      /// \brief Converts the node to blue node object.
      ///
      /// This function converts unsafely the node to blue node
      /// object. If the node is not from the blue partition, then it
      /// returns INVALID.
      BlueNode asBlueNode(const Node&) const { return BlueNode(); }

      /// \brief Gives back the red end node of the edge.
      ///
      /// Gives back the red end node of the edge.
      RedNode redNode(const Edge&) const { return RedNode(); }

      /// \brief Gives back the blue end node of the edge.
      ///
      /// Gives back the blue end node of the edge.
      BlueNode blueNode(const Edge&) const { return BlueNode(); }

      /// \brief The first node of the edge.
      ///
      /// It is a synonim for the \c redNode().
      Node u(Edge) const { return INVALID; }

      /// \brief The second node of the edge.
      ///
      /// It is a synonim for the \c blueNode().
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

      /// \brief The red ID of the node.
      ///
      /// Returns the red ID of the given node.
      int id(RedNode) const { return -1; }

      /// \brief The blue ID of the node.
      ///
      /// Returns the blue ID of the given node.
      int id(BlueNode) const { return -1; }

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

      /// \brief An upper bound on the red IDs.
      ///
      /// Returns an upper bound on the red IDs.
      int maxRedId() const { return -1; }

      /// \brief An upper bound on the blue IDs.
      ///
      /// Returns an upper bound on the blue IDs.
      int maxBlueId() const { return -1; }

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
      /// Returns \c true if the given arc goes from a red node to a blue node.
      bool direction(Arc) const { return true; }

      /// \brief Direct the edge.
      ///
      /// Direct the given edge. The returned arc
      /// represents the given edge and its direction comes
      /// from the bool parameter. If it is \c true, then the source of the node
      /// will be a red node.
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

      void firstRed(RedNode&) const {}
      void nextRed(RedNode&) const {}

      void firstBlue(BlueNode&) const {}
      void nextBlue(BlueNode&) const {}

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
      int maxId(RedNode) const { return -1; }
      // Dummy parameter.
      int maxId(BlueNode) const { return -1; }
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

      template <typename _BpGraph>
      struct Constraints {
        void constraints() {
          checkConcept<BaseBpGraphComponent, _BpGraph>();
          checkConcept<IterableBpGraphComponent<>, _BpGraph>();
          checkConcept<IDableBpGraphComponent<>, _BpGraph>();
          checkConcept<MappableBpGraphComponent<>, _BpGraph>();
        }
      };

    };

  }

}

#endif
