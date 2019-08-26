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

#ifndef LEMON_CONCEPTS_DIGRAPH_H
#define LEMON_CONCEPTS_DIGRAPH_H

///\ingroup graph_concepts
///\file
///\brief The concept of directed graphs.

#include <lemon/core.h>
#include <lemon/concepts/maps.h>
#include <lemon/concept_check.h>
#include <lemon/concepts/graph_components.h>

namespace lemon {
  namespace concepts {

    /// \ingroup graph_concepts
    ///
    /// \brief Class describing the concept of directed graphs.
    ///
    /// This class describes the common interface of all directed
    /// graphs (digraphs).
    ///
    /// Like all concept classes, it only provides an interface
    /// without any sensible implementation. So any general algorithm for
    /// directed graphs should compile with this class, but it will not
    /// run properly, of course.
    /// An actual digraph implementation like \ref ListDigraph or
    /// \ref SmartDigraph may have additional functionality.
    ///
    /// \sa Graph
    class Digraph {
    private:
      /// Diraphs are \e not copy constructible. Use DigraphCopy instead.
      Digraph(const Digraph &) {}
      /// \brief Assignment of a digraph to another one is \e not allowed.
      /// Use DigraphCopy instead.
      void operator=(const Digraph &) {}

    public:
      /// Default constructor.
      Digraph() { }

      /// The node type of the digraph

      /// This class identifies a node of the digraph. It also serves
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
        /// the nodes; this order has nothing to do with the iteration
        /// ordering of the nodes.
        bool operator<(Node) const { return false; }
      };

      /// Iterator class for the nodes.

      /// This iterator goes through each node of the digraph.
      /// Its usage is quite simple, for example, you can count the number
      /// of nodes in a digraph \c g of type \c %Digraph like this:
      ///\code
      /// int count=0;
      /// for (Digraph::NodeIt n(g); n!=INVALID; ++n) ++count;
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
        explicit NodeIt(const Digraph&) { }
        /// Sets the iterator to the given node.

        /// Sets the iterator to the given node of the given digraph.
        ///
        NodeIt(const Digraph&, const Node&) { }
        /// Next node.

        /// Assign the iterator to the next node.
        ///
        NodeIt& operator++() { return *this; }
      };


      /// The arc type of the digraph

      /// This class identifies an arc of the digraph. It also serves
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
      };

      /// Iterator class for the outgoing arcs of a node.

      /// This iterator goes trough the \e outgoing arcs of a certain node
      /// of a digraph.
      /// Its usage is quite simple, for example, you can count the number
      /// of outgoing arcs of a node \c n
      /// in a digraph \c g of type \c %Digraph as follows.
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
        OutArcIt(const Digraph&, const Node&) { }
        /// Sets the iterator to the given arc.

        /// Sets the iterator to the given arc of the given digraph.
        ///
        OutArcIt(const Digraph&, const Arc&) { }
        /// Next outgoing arc

        /// Assign the iterator to the next
        /// outgoing arc of the corresponding node.
        OutArcIt& operator++() { return *this; }
      };

      /// Iterator class for the incoming arcs of a node.

      /// This iterator goes trough the \e incoming arcs of a certain node
      /// of a digraph.
      /// Its usage is quite simple, for example, you can count the number
      /// of incoming arcs of a node \c n
      /// in a digraph \c g of type \c %Digraph as follows.
      ///\code
      /// int count=0;
      /// for(Digraph::InArcIt a(g, n); a!=INVALID; ++a) ++count;
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
        InArcIt(const Digraph&, const Node&) { }
        /// Sets the iterator to the given arc.

        /// Sets the iterator to the given arc of the given digraph.
        ///
        InArcIt(const Digraph&, const Arc&) { }
        /// Next incoming arc

        /// Assign the iterator to the next
        /// incoming arc of the corresponding node.
        InArcIt& operator++() { return *this; }
      };

      /// Iterator class for the arcs.

      /// This iterator goes through each arc of the digraph.
      /// Its usage is quite simple, for example, you can count the number
      /// of arcs in a digraph \c g of type \c %Digraph as follows:
      ///\code
      /// int count=0;
      /// for(Digraph::ArcIt a(g); a!=INVALID; ++a) ++count;
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

        /// Sets the iterator to the first arc of the given digraph.
        ///
        explicit ArcIt(const Digraph& g) {
          ::lemon::ignore_unused_variable_warning(g);
        }
        /// Sets the iterator to the given arc.

        /// Sets the iterator to the given arc of the given digraph.
        ///
        ArcIt(const Digraph&, const Arc&) { }
        /// Next arc

        /// Assign the iterator to the next arc.
        ///
        ArcIt& operator++() { return *this; }
      };

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

      /// \brief The ID of the arc.
      ///
      /// Returns the ID of the given arc.
      int id(Arc) const { return -1; }

      /// \brief The node with the given ID.
      ///
      /// Returns the node with the given ID.
      /// \pre The argument should be a valid node ID in the digraph.
      Node nodeFromId(int) const { return INVALID; }

      /// \brief The arc with the given ID.
      ///
      /// Returns the arc with the given ID.
      /// \pre The argument should be a valid arc ID in the digraph.
      Arc arcFromId(int) const { return INVALID; }

      /// \brief An upper bound on the node IDs.
      ///
      /// Returns an upper bound on the node IDs.
      int maxNodeId() const { return -1; }

      /// \brief An upper bound on the arc IDs.
      ///
      /// Returns an upper bound on the arc IDs.
      int maxArcId() const { return -1; }

      void first(Node&) const {}
      void next(Node&) const {}

      void first(Arc&) const {}
      void next(Arc&) const {}


      void firstIn(Arc&, const Node&) const {}
      void nextIn(Arc&) const {}

      void firstOut(Arc&, const Node&) const {}
      void nextOut(Arc&) const {}

      // The second parameter is dummy.
      Node fromId(int, Node) const { return INVALID; }
      // The second parameter is dummy.
      Arc fromId(int, Arc) const { return INVALID; }

      // Dummy parameter.
      int maxId(Node) const { return -1; }
      // Dummy parameter.
      int maxId(Arc) const { return -1; }

      /// \brief The opposite node on the arc.
      ///
      /// Returns the opposite node on the given arc.
      Node oppositeNode(Node, Arc) const { return INVALID; }

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

      /// \brief Standard graph map type for the nodes.
      ///
      /// Standard graph map type for the nodes.
      /// It conforms to the ReferenceMap concept.
      template<class T>
      class NodeMap : public ReferenceMap<Node, T, T&, const T&> {
      public:

        /// Constructor
        explicit NodeMap(const Digraph&) { }
        /// Constructor with given initial value
        NodeMap(const Digraph&, T) { }

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
      class ArcMap : public ReferenceMap<Arc, T, T&, const T&> {
      public:

        /// Constructor
        explicit ArcMap(const Digraph&) { }
        /// Constructor with given initial value
        ArcMap(const Digraph&, T) { }

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

      template <typename _Digraph>
      struct Constraints {
        void constraints() {
          checkConcept<BaseDigraphComponent, _Digraph>();
          checkConcept<IterableDigraphComponent<>, _Digraph>();
          checkConcept<IDableDigraphComponent<>, _Digraph>();
          checkConcept<MappableDigraphComponent<>, _Digraph>();
        }
      };

    };

  } //namespace concepts
} //namespace lemon



#endif
