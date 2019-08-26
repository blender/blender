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
///\brief The concepts of graph components.

#ifndef LEMON_CONCEPTS_GRAPH_COMPONENTS_H
#define LEMON_CONCEPTS_GRAPH_COMPONENTS_H

#include <lemon/core.h>
#include <lemon/concepts/maps.h>

#include <lemon/bits/alteration_notifier.h>

namespace lemon {
  namespace concepts {

    /// \brief Concept class for \c Node, \c Arc and \c Edge types.
    ///
    /// This class describes the concept of \c Node, \c Arc and \c Edge
    /// subtypes of digraph and graph types.
    ///
    /// \note This class is a template class so that we can use it to
    /// create graph skeleton classes. The reason for this is that \c Node
    /// and \c Arc (or \c Edge) types should \e not derive from the same
    /// base class. For \c Node you should instantiate it with character
    /// \c 'n', for \c Arc with \c 'a' and for \c Edge with \c 'e'.
#ifndef DOXYGEN
    template <char sel = '0'>
#endif
    class GraphItem {
    public:
      /// \brief Default constructor.
      ///
      /// Default constructor.
      /// \warning The default constructor is not required to set
      /// the item to some well-defined value. So you should consider it
      /// as uninitialized.
      GraphItem() {}

      /// \brief Copy constructor.
      ///
      /// Copy constructor.
      GraphItem(const GraphItem &) {}

      /// \brief Constructor for conversion from \c INVALID.
      ///
      /// Constructor for conversion from \c INVALID.
      /// It initializes the item to be invalid.
      /// \sa Invalid for more details.
      GraphItem(Invalid) {}

      /// \brief Assignment operator.
      ///
      /// Assignment operator for the item.
      GraphItem& operator=(const GraphItem&) { return *this; }

      /// \brief Assignment operator for INVALID.
      ///
      /// This operator makes the item invalid.
      GraphItem& operator=(Invalid) { return *this; }

      /// \brief Equality operator.
      ///
      /// Equality operator.
      bool operator==(const GraphItem&) const { return false; }

      /// \brief Inequality operator.
      ///
      /// Inequality operator.
      bool operator!=(const GraphItem&) const { return false; }

      /// \brief Ordering operator.
      ///
      /// This operator defines an ordering of the items.
      /// It makes possible to use graph item types as key types in
      /// associative containers (e.g. \c std::map).
      ///
      /// \note This operator only has to define some strict ordering of
      /// the items; this order has nothing to do with the iteration
      /// ordering of the items.
      bool operator<(const GraphItem&) const { return false; }

      template<typename _GraphItem>
      struct Constraints {
        void constraints() {
          _GraphItem i1;
          i1=INVALID;
          _GraphItem i2 = i1;
          _GraphItem i3 = INVALID;

          i1 = i2 = i3;

          bool b;
          ::lemon::ignore_unused_variable_warning(b);

          b = (ia == ib) && (ia != ib);
          b = (ia == INVALID) && (ib != INVALID);
          b = (ia < ib);
        }

        const _GraphItem &ia;
        const _GraphItem &ib;
        Constraints() {}
      };
    };

    /// \brief Base skeleton class for directed graphs.
    ///
    /// This class describes the base interface of directed graph types.
    /// All digraph %concepts have to conform to this class.
    /// It just provides types for nodes and arcs and functions
    /// to get the source and the target nodes of arcs.
    class BaseDigraphComponent {
    public:

      typedef BaseDigraphComponent Digraph;

      /// \brief Node class of the digraph.
      ///
      /// This class represents the nodes of the digraph.
      typedef GraphItem<'n'> Node;

      /// \brief Arc class of the digraph.
      ///
      /// This class represents the arcs of the digraph.
      typedef GraphItem<'a'> Arc;

      /// \brief Return the source node of an arc.
      ///
      /// This function returns the source node of an arc.
      Node source(const Arc&) const { return INVALID; }

      /// \brief Return the target node of an arc.
      ///
      /// This function returns the target node of an arc.
      Node target(const Arc&) const { return INVALID; }

      /// \brief Return the opposite node on the given arc.
      ///
      /// This function returns the opposite node on the given arc.
      Node oppositeNode(const Node&, const Arc&) const {
        return INVALID;
      }

      template <typename _Digraph>
      struct Constraints {
        typedef typename _Digraph::Node Node;
        typedef typename _Digraph::Arc Arc;

        void constraints() {
          checkConcept<GraphItem<'n'>, Node>();
          checkConcept<GraphItem<'a'>, Arc>();
          {
            Node n;
            Arc e(INVALID);
            n = digraph.source(e);
            n = digraph.target(e);
            n = digraph.oppositeNode(n, e);
          }
        }

        const _Digraph& digraph;
        Constraints() {}
      };
    };

    /// \brief Base skeleton class for undirected graphs.
    ///
    /// This class describes the base interface of undirected graph types.
    /// All graph %concepts have to conform to this class.
    /// It extends the interface of \ref BaseDigraphComponent with an
    /// \c Edge type and functions to get the end nodes of edges,
    /// to convert from arcs to edges and to get both direction of edges.
    class BaseGraphComponent : public BaseDigraphComponent {
    public:

      typedef BaseGraphComponent Graph;

      typedef BaseDigraphComponent::Node Node;
      typedef BaseDigraphComponent::Arc Arc;

      /// \brief Undirected edge class of the graph.
      ///
      /// This class represents the undirected edges of the graph.
      /// Undirected graphs can be used as directed graphs, each edge is
      /// represented by two opposite directed arcs.
      class Edge : public GraphItem<'e'> {
        typedef GraphItem<'e'> Parent;

      public:
        /// \brief Default constructor.
        ///
        /// Default constructor.
        /// \warning The default constructor is not required to set
        /// the item to some well-defined value. So you should consider it
        /// as uninitialized.
        Edge() {}

        /// \brief Copy constructor.
        ///
        /// Copy constructor.
        Edge(const Edge &) : Parent() {}

        /// \brief Constructor for conversion from \c INVALID.
        ///
        /// Constructor for conversion from \c INVALID.
        /// It initializes the item to be invalid.
        /// \sa Invalid for more details.
        Edge(Invalid) {}

        /// \brief Constructor for conversion from an arc.
        ///
        /// Constructor for conversion from an arc.
        /// Besides the core graph item functionality each arc should
        /// be convertible to the represented edge.
        Edge(const Arc&) {}
     };

      /// \brief Return one end node of an edge.
      ///
      /// This function returns one end node of an edge.
      Node u(const Edge&) const { return INVALID; }

      /// \brief Return the other end node of an edge.
      ///
      /// This function returns the other end node of an edge.
      Node v(const Edge&) const { return INVALID; }

      /// \brief Return a directed arc related to an edge.
      ///
      /// This function returns a directed arc from its direction and the
      /// represented edge.
      Arc direct(const Edge&, bool) const { return INVALID; }

      /// \brief Return a directed arc related to an edge.
      ///
      /// This function returns a directed arc from its source node and the
      /// represented edge.
      Arc direct(const Edge&, const Node&) const { return INVALID; }

      /// \brief Return the direction of the arc.
      ///
      /// Returns the direction of the arc. Each arc represents an
      /// edge with a direction. It gives back the
      /// direction.
      bool direction(const Arc&) const { return true; }

      /// \brief Return the opposite arc.
      ///
      /// This function returns the opposite arc, i.e. the arc representing
      /// the same edge and has opposite direction.
      Arc oppositeArc(const Arc&) const { return INVALID; }

      template <typename _Graph>
      struct Constraints {
        typedef typename _Graph::Node Node;
        typedef typename _Graph::Arc Arc;
        typedef typename _Graph::Edge Edge;

        void constraints() {
          checkConcept<BaseDigraphComponent, _Graph>();
          checkConcept<GraphItem<'e'>, Edge>();
          {
            Node n;
            Edge ue(INVALID);
            Arc e;
            n = graph.u(ue);
            n = graph.v(ue);
            e = graph.direct(ue, true);
            e = graph.direct(ue, false);
            e = graph.direct(ue, n);
            e = graph.oppositeArc(e);
            ue = e;
            bool d = graph.direction(e);
            ::lemon::ignore_unused_variable_warning(d);
          }
        }

        const _Graph& graph;
      Constraints() {}
      };

    };

    /// \brief Base skeleton class for undirected bipartite graphs.
    ///
    /// This class describes the base interface of undirected
    /// bipartite graph types.  All bipartite graph %concepts have to
    /// conform to this class.  It extends the interface of \ref
    /// BaseGraphComponent with an \c Edge type and functions to get
    /// the end nodes of edges, to convert from arcs to edges and to
    /// get both direction of edges.
    class BaseBpGraphComponent : public BaseGraphComponent {
    public:

      typedef BaseBpGraphComponent BpGraph;

      typedef BaseDigraphComponent::Node Node;
      typedef BaseDigraphComponent::Arc Arc;

      /// \brief Class to represent red nodes.
      ///
      /// This class represents the red nodes of the graph. The red
      /// nodes can also be used as normal nodes.
      class RedNode : public Node {
        typedef Node Parent;

      public:
        /// \brief Default constructor.
        ///
        /// Default constructor.
        /// \warning The default constructor is not required to set
        /// the item to some well-defined value. So you should consider it
        /// as uninitialized.
        RedNode() {}

        /// \brief Copy constructor.
        ///
        /// Copy constructor.
        RedNode(const RedNode &) : Parent() {}

        /// \brief Constructor for conversion from \c INVALID.
        ///
        /// Constructor for conversion from \c INVALID.
        /// It initializes the item to be invalid.
        /// \sa Invalid for more details.
        RedNode(Invalid) {}
      };

      /// \brief Class to represent blue nodes.
      ///
      /// This class represents the blue nodes of the graph. The blue
      /// nodes can also be used as normal nodes.
      class BlueNode : public Node {
        typedef Node Parent;

      public:
        /// \brief Default constructor.
        ///
        /// Default constructor.
        /// \warning The default constructor is not required to set
        /// the item to some well-defined value. So you should consider it
        /// as uninitialized.
        BlueNode() {}

        /// \brief Copy constructor.
        ///
        /// Copy constructor.
        BlueNode(const BlueNode &) : Parent() {}

        /// \brief Constructor for conversion from \c INVALID.
        ///
        /// Constructor for conversion from \c INVALID.
        /// It initializes the item to be invalid.
        /// \sa Invalid for more details.
        BlueNode(Invalid) {}

        /// \brief Constructor for conversion from a node.
        ///
        /// Constructor for conversion from a node. The conversion can
        /// be invalid, since the Node can be member of the red
        /// set.
        BlueNode(const Node&) {}
      };

      /// \brief Gives back %true for red nodes.
      ///
      /// Gives back %true for red nodes.
      bool red(const Node&) const { return true; }

      /// \brief Gives back %true for blue nodes.
      ///
      /// Gives back %true for blue nodes.
      bool blue(const Node&) const { return true; }

      /// \brief Gives back the red end node of the edge.
      ///
      /// Gives back the red end node of the edge.
      RedNode redNode(const Edge&) const { return RedNode(); }

      /// \brief Gives back the blue end node of the edge.
      ///
      /// Gives back the blue end node of the edge.
      BlueNode blueNode(const Edge&) const { return BlueNode(); }

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

      template <typename _BpGraph>
      struct Constraints {
        typedef typename _BpGraph::Node Node;
        typedef typename _BpGraph::RedNode RedNode;
        typedef typename _BpGraph::BlueNode BlueNode;
        typedef typename _BpGraph::Arc Arc;
        typedef typename _BpGraph::Edge Edge;

        void constraints() {
          checkConcept<BaseGraphComponent, _BpGraph>();
          checkConcept<GraphItem<'n'>, RedNode>();
          checkConcept<GraphItem<'n'>, BlueNode>();
          {
            Node n;
            RedNode rn;
            BlueNode bn;
            Node rnan = rn;
            Node bnan = bn;
            Edge e;
            bool b;
            b = bpgraph.red(rnan);
            b = bpgraph.blue(bnan);
            rn = bpgraph.redNode(e);
            bn = bpgraph.blueNode(e);
            rn = bpgraph.asRedNodeUnsafe(rnan);
            bn = bpgraph.asBlueNodeUnsafe(bnan);
            rn = bpgraph.asRedNode(rnan);
            bn = bpgraph.asBlueNode(bnan);
            ::lemon::ignore_unused_variable_warning(b);
          }
        }

        const _BpGraph& bpgraph;
      };

    };

    /// \brief Skeleton class for \e idable directed graphs.
    ///
    /// This class describes the interface of \e idable directed graphs.
    /// It extends \ref BaseDigraphComponent with the core ID functions.
    /// The ids of the items must be unique and immutable.
    /// This concept is part of the Digraph concept.
    template <typename BAS = BaseDigraphComponent>
    class IDableDigraphComponent : public BAS {
    public:

      typedef BAS Base;
      typedef typename Base::Node Node;
      typedef typename Base::Arc Arc;

      /// \brief Return a unique integer id for the given node.
      ///
      /// This function returns a unique integer id for the given node.
      int id(const Node&) const { return -1; }

      /// \brief Return the node by its unique id.
      ///
      /// This function returns the node by its unique id.
      /// If the digraph does not contain a node with the given id,
      /// then the result of the function is undefined.
      Node nodeFromId(int) const { return INVALID; }

      /// \brief Return a unique integer id for the given arc.
      ///
      /// This function returns a unique integer id for the given arc.
      int id(const Arc&) const { return -1; }

      /// \brief Return the arc by its unique id.
      ///
      /// This function returns the arc by its unique id.
      /// If the digraph does not contain an arc with the given id,
      /// then the result of the function is undefined.
      Arc arcFromId(int) const { return INVALID; }

      /// \brief Return an integer greater or equal to the maximum
      /// node id.
      ///
      /// This function returns an integer greater or equal to the
      /// maximum node id.
      int maxNodeId() const { return -1; }

      /// \brief Return an integer greater or equal to the maximum
      /// arc id.
      ///
      /// This function returns an integer greater or equal to the
      /// maximum arc id.
      int maxArcId() const { return -1; }

      template <typename _Digraph>
      struct Constraints {

        void constraints() {
          checkConcept<Base, _Digraph >();
          typename _Digraph::Node node;
          node=INVALID;
          int nid = digraph.id(node);
          nid = digraph.id(node);
          node = digraph.nodeFromId(nid);
          typename _Digraph::Arc arc;
          arc=INVALID;
          int eid = digraph.id(arc);
          eid = digraph.id(arc);
          arc = digraph.arcFromId(eid);

          nid = digraph.maxNodeId();
          ::lemon::ignore_unused_variable_warning(nid);
          eid = digraph.maxArcId();
          ::lemon::ignore_unused_variable_warning(eid);
        }

        const _Digraph& digraph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for \e idable undirected graphs.
    ///
    /// This class describes the interface of \e idable undirected
    /// graphs. It extends \ref IDableDigraphComponent with the core ID
    /// functions of undirected graphs.
    /// The ids of the items must be unique and immutable.
    /// This concept is part of the Graph concept.
    template <typename BAS = BaseGraphComponent>
    class IDableGraphComponent : public IDableDigraphComponent<BAS> {
    public:

      typedef BAS Base;
      typedef typename Base::Edge Edge;

      using IDableDigraphComponent<Base>::id;

      /// \brief Return a unique integer id for the given edge.
      ///
      /// This function returns a unique integer id for the given edge.
      int id(const Edge&) const { return -1; }

      /// \brief Return the edge by its unique id.
      ///
      /// This function returns the edge by its unique id.
      /// If the graph does not contain an edge with the given id,
      /// then the result of the function is undefined.
      Edge edgeFromId(int) const { return INVALID; }

      /// \brief Return an integer greater or equal to the maximum
      /// edge id.
      ///
      /// This function returns an integer greater or equal to the
      /// maximum edge id.
      int maxEdgeId() const { return -1; }

      template <typename _Graph>
      struct Constraints {

        void constraints() {
          checkConcept<IDableDigraphComponent<Base>, _Graph >();
          typename _Graph::Edge edge;
          int ueid = graph.id(edge);
          ueid = graph.id(edge);
          edge = graph.edgeFromId(ueid);
          ueid = graph.maxEdgeId();
          ::lemon::ignore_unused_variable_warning(ueid);
        }

        const _Graph& graph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for \e idable undirected bipartite graphs.
    ///
    /// This class describes the interface of \e idable undirected
    /// bipartite graphs. It extends \ref IDableGraphComponent with
    /// the core ID functions of undirected bipartite graphs. Beside
    /// the regular node ids, this class also provides ids within the
    /// the red and blue sets of the nodes. This concept is part of
    /// the BpGraph concept.
    template <typename BAS = BaseBpGraphComponent>
    class IDableBpGraphComponent : public IDableGraphComponent<BAS> {
    public:

      typedef BAS Base;
      typedef IDableGraphComponent<BAS> Parent;
      typedef typename Base::Node Node;
      typedef typename Base::RedNode RedNode;
      typedef typename Base::BlueNode BlueNode;

      using Parent::id;

      /// \brief Return a unique integer id for the given node in the red set.
      ///
      /// Return a unique integer id for the given node in the red set.
      int id(const RedNode&) const { return -1; }

      /// \brief Return a unique integer id for the given node in the blue set.
      ///
      /// Return a unique integer id for the given node in the blue set.
      int id(const BlueNode&) const { return -1; }

      /// \brief Return an integer greater or equal to the maximum
      /// node id in the red set.
      ///
      /// Return an integer greater or equal to the maximum
      /// node id in the red set.
      int maxRedId() const { return -1; }

      /// \brief Return an integer greater or equal to the maximum
      /// node id in the blue set.
      ///
      /// Return an integer greater or equal to the maximum
      /// node id in the blue set.
      int maxBlueId() const { return -1; }

      template <typename _BpGraph>
      struct Constraints {

        void constraints() {
          checkConcept<IDableGraphComponent<Base>, _BpGraph>();
          typename _BpGraph::Node node;
          typename _BpGraph::RedNode red;
          typename _BpGraph::BlueNode blue;
          int rid = bpgraph.id(red);
          int bid = bpgraph.id(blue);
          rid = bpgraph.maxRedId();
          bid = bpgraph.maxBlueId();
          ::lemon::ignore_unused_variable_warning(rid);
          ::lemon::ignore_unused_variable_warning(bid);
        }

        const _BpGraph& bpgraph;
      };
    };

    /// \brief Concept class for \c NodeIt, \c ArcIt and \c EdgeIt types.
    ///
    /// This class describes the concept of \c NodeIt, \c ArcIt and
    /// \c EdgeIt subtypes of digraph and graph types.
    template <typename GR, typename Item>
    class GraphItemIt : public Item {
    public:
      /// \brief Default constructor.
      ///
      /// Default constructor.
      /// \warning The default constructor is not required to set
      /// the iterator to some well-defined value. So you should consider it
      /// as uninitialized.
      GraphItemIt() {}

      /// \brief Copy constructor.
      ///
      /// Copy constructor.
      GraphItemIt(const GraphItemIt& it) : Item(it) {}

      /// \brief Constructor that sets the iterator to the first item.
      ///
      /// Constructor that sets the iterator to the first item.
      explicit GraphItemIt(const GR&) {}

      /// \brief Constructor for conversion from \c INVALID.
      ///
      /// Constructor for conversion from \c INVALID.
      /// It initializes the iterator to be invalid.
      /// \sa Invalid for more details.
      GraphItemIt(Invalid) {}

      /// \brief Assignment operator.
      ///
      /// Assignment operator for the iterator.
      GraphItemIt& operator=(const GraphItemIt&) { return *this; }

      /// \brief Increment the iterator.
      ///
      /// This operator increments the iterator, i.e. assigns it to the
      /// next item.
      GraphItemIt& operator++() { return *this; }

      /// \brief Equality operator
      ///
      /// Equality operator.
      /// Two iterators are equal if and only if they point to the
      /// same object or both are invalid.
      bool operator==(const GraphItemIt&) const { return true;}

      /// \brief Inequality operator
      ///
      /// Inequality operator.
      /// Two iterators are equal if and only if they point to the
      /// same object or both are invalid.
      bool operator!=(const GraphItemIt&) const { return true;}

      template<typename _GraphItemIt>
      struct Constraints {
        void constraints() {
          checkConcept<GraphItem<>, _GraphItemIt>();
          _GraphItemIt it1(g);
          _GraphItemIt it2;
          _GraphItemIt it3 = it1;
          _GraphItemIt it4 = INVALID;
          ::lemon::ignore_unused_variable_warning(it3);
          ::lemon::ignore_unused_variable_warning(it4);

          it2 = ++it1;
          ++it2 = it1;
          ++(++it1);

          Item bi = it1;
          bi = it2;
        }
        const GR& g;
        Constraints() {}
      };
    };

    /// \brief Concept class for \c InArcIt, \c OutArcIt and
    /// \c IncEdgeIt types.
    ///
    /// This class describes the concept of \c InArcIt, \c OutArcIt
    /// and \c IncEdgeIt subtypes of digraph and graph types.
    ///
    /// \note Since these iterator classes do not inherit from the same
    /// base class, there is an additional template parameter (selector)
    /// \c sel. For \c InArcIt you should instantiate it with character
    /// \c 'i', for \c OutArcIt with \c 'o' and for \c IncEdgeIt with \c 'e'.
    template <typename GR,
              typename Item = typename GR::Arc,
              typename Base = typename GR::Node,
              char sel = '0'>
    class GraphIncIt : public Item {
    public:
      /// \brief Default constructor.
      ///
      /// Default constructor.
      /// \warning The default constructor is not required to set
      /// the iterator to some well-defined value. So you should consider it
      /// as uninitialized.
      GraphIncIt() {}

      /// \brief Copy constructor.
      ///
      /// Copy constructor.
      GraphIncIt(const GraphIncIt& it) : Item(it) {}

      /// \brief Constructor that sets the iterator to the first
      /// incoming or outgoing arc.
      ///
      /// Constructor that sets the iterator to the first arc
      /// incoming to or outgoing from the given node.
      explicit GraphIncIt(const GR&, const Base&) {}

      /// \brief Constructor for conversion from \c INVALID.
      ///
      /// Constructor for conversion from \c INVALID.
      /// It initializes the iterator to be invalid.
      /// \sa Invalid for more details.
      GraphIncIt(Invalid) {}

      /// \brief Assignment operator.
      ///
      /// Assignment operator for the iterator.
      GraphIncIt& operator=(const GraphIncIt&) { return *this; }

      /// \brief Increment the iterator.
      ///
      /// This operator increments the iterator, i.e. assigns it to the
      /// next arc incoming to or outgoing from the given node.
      GraphIncIt& operator++() { return *this; }

      /// \brief Equality operator
      ///
      /// Equality operator.
      /// Two iterators are equal if and only if they point to the
      /// same object or both are invalid.
      bool operator==(const GraphIncIt&) const { return true;}

      /// \brief Inequality operator
      ///
      /// Inequality operator.
      /// Two iterators are equal if and only if they point to the
      /// same object or both are invalid.
      bool operator!=(const GraphIncIt&) const { return true;}

      template <typename _GraphIncIt>
      struct Constraints {
        void constraints() {
          checkConcept<GraphItem<sel>, _GraphIncIt>();
          _GraphIncIt it1(graph, node);
          _GraphIncIt it2;
          _GraphIncIt it3 = it1;
          _GraphIncIt it4 = INVALID;
          ::lemon::ignore_unused_variable_warning(it3);
          ::lemon::ignore_unused_variable_warning(it4);

          it2 = ++it1;
          ++it2 = it1;
          ++(++it1);
          Item e = it1;
          e = it2;
        }
        const Base& node;
        const GR& graph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for iterable directed graphs.
    ///
    /// This class describes the interface of iterable directed
    /// graphs. It extends \ref BaseDigraphComponent with the core
    /// iterable interface.
    /// This concept is part of the Digraph concept.
    template <typename BAS = BaseDigraphComponent>
    class IterableDigraphComponent : public BAS {

    public:

      typedef BAS Base;
      typedef typename Base::Node Node;
      typedef typename Base::Arc Arc;

      typedef IterableDigraphComponent Digraph;

      /// \name Base Iteration
      ///
      /// This interface provides functions for iteration on digraph items.
      ///
      /// @{

      /// \brief Return the first node.
      ///
      /// This function gives back the first node in the iteration order.
      void first(Node&) const {}

      /// \brief Return the next node.
      ///
      /// This function gives back the next node in the iteration order.
      void next(Node&) const {}

      /// \brief Return the first arc.
      ///
      /// This function gives back the first arc in the iteration order.
      void first(Arc&) const {}

      /// \brief Return the next arc.
      ///
      /// This function gives back the next arc in the iteration order.
      void next(Arc&) const {}

      /// \brief Return the first arc incoming to the given node.
      ///
      /// This function gives back the first arc incoming to the
      /// given node.
      void firstIn(Arc&, const Node&) const {}

      /// \brief Return the next arc incoming to the given node.
      ///
      /// This function gives back the next arc incoming to the
      /// given node.
      void nextIn(Arc&) const {}

      /// \brief Return the first arc outgoing form the given node.
      ///
      /// This function gives back the first arc outgoing form the
      /// given node.
      void firstOut(Arc&, const Node&) const {}

      /// \brief Return the next arc outgoing form the given node.
      ///
      /// This function gives back the next arc outgoing form the
      /// given node.
      void nextOut(Arc&) const {}

      /// @}

      /// \name Class Based Iteration
      ///
      /// This interface provides iterator classes for digraph items.
      ///
      /// @{

      /// \brief This iterator goes through each node.
      ///
      /// This iterator goes through each node.
      ///
      typedef GraphItemIt<Digraph, Node> NodeIt;

      /// \brief This iterator goes through each arc.
      ///
      /// This iterator goes through each arc.
      ///
      typedef GraphItemIt<Digraph, Arc> ArcIt;

      /// \brief This iterator goes trough the incoming arcs of a node.
      ///
      /// This iterator goes trough the \e incoming arcs of a certain node
      /// of a digraph.
      typedef GraphIncIt<Digraph, Arc, Node, 'i'> InArcIt;

      /// \brief This iterator goes trough the outgoing arcs of a node.
      ///
      /// This iterator goes trough the \e outgoing arcs of a certain node
      /// of a digraph.
      typedef GraphIncIt<Digraph, Arc, Node, 'o'> OutArcIt;

      /// \brief The base node of the iterator.
      ///
      /// This function gives back the base node of the iterator.
      /// It is always the target node of the pointed arc.
      Node baseNode(const InArcIt&) const { return INVALID; }

      /// \brief The running node of the iterator.
      ///
      /// This function gives back the running node of the iterator.
      /// It is always the source node of the pointed arc.
      Node runningNode(const InArcIt&) const { return INVALID; }

      /// \brief The base node of the iterator.
      ///
      /// This function gives back the base node of the iterator.
      /// It is always the source node of the pointed arc.
      Node baseNode(const OutArcIt&) const { return INVALID; }

      /// \brief The running node of the iterator.
      ///
      /// This function gives back the running node of the iterator.
      /// It is always the target node of the pointed arc.
      Node runningNode(const OutArcIt&) const { return INVALID; }

      /// @}

      template <typename _Digraph>
      struct Constraints {
        void constraints() {
          checkConcept<Base, _Digraph>();

          {
            typename _Digraph::Node node(INVALID);
            typename _Digraph::Arc arc(INVALID);
            {
              digraph.first(node);
              digraph.next(node);
            }
            {
              digraph.first(arc);
              digraph.next(arc);
            }
            {
              digraph.firstIn(arc, node);
              digraph.nextIn(arc);
            }
            {
              digraph.firstOut(arc, node);
              digraph.nextOut(arc);
            }
          }

          {
            checkConcept<GraphItemIt<_Digraph, typename _Digraph::Arc>,
              typename _Digraph::ArcIt >();
            checkConcept<GraphItemIt<_Digraph, typename _Digraph::Node>,
              typename _Digraph::NodeIt >();
            checkConcept<GraphIncIt<_Digraph, typename _Digraph::Arc,
              typename _Digraph::Node, 'i'>, typename _Digraph::InArcIt>();
            checkConcept<GraphIncIt<_Digraph, typename _Digraph::Arc,
              typename _Digraph::Node, 'o'>, typename _Digraph::OutArcIt>();

            typename _Digraph::Node n;
            const typename _Digraph::InArcIt iait(INVALID);
            const typename _Digraph::OutArcIt oait(INVALID);
            n = digraph.baseNode(iait);
            n = digraph.runningNode(iait);
            n = digraph.baseNode(oait);
            n = digraph.runningNode(oait);
            ::lemon::ignore_unused_variable_warning(n);
          }
        }

        const _Digraph& digraph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for iterable undirected graphs.
    ///
    /// This class describes the interface of iterable undirected
    /// graphs. It extends \ref IterableDigraphComponent with the core
    /// iterable interface of undirected graphs.
    /// This concept is part of the Graph concept.
    template <typename BAS = BaseGraphComponent>
    class IterableGraphComponent : public IterableDigraphComponent<BAS> {
    public:

      typedef BAS Base;
      typedef typename Base::Node Node;
      typedef typename Base::Arc Arc;
      typedef typename Base::Edge Edge;


      typedef IterableGraphComponent Graph;

      /// \name Base Iteration
      ///
      /// This interface provides functions for iteration on edges.
      ///
      /// @{

      using IterableDigraphComponent<Base>::first;
      using IterableDigraphComponent<Base>::next;

      /// \brief Return the first edge.
      ///
      /// This function gives back the first edge in the iteration order.
      void first(Edge&) const {}

      /// \brief Return the next edge.
      ///
      /// This function gives back the next edge in the iteration order.
      void next(Edge&) const {}

      /// \brief Return the first edge incident to the given node.
      ///
      /// This function gives back the first edge incident to the given
      /// node. The bool parameter gives back the direction for which the
      /// source node of the directed arc representing the edge is the
      /// given node.
      void firstInc(Edge&, bool&, const Node&) const {}

      /// \brief Gives back the next of the edges from the
      /// given node.
      ///
      /// This function gives back the next edge incident to the given
      /// node. The bool parameter should be used as \c firstInc() use it.
      void nextInc(Edge&, bool&) const {}

      using IterableDigraphComponent<Base>::baseNode;
      using IterableDigraphComponent<Base>::runningNode;

      /// @}

      /// \name Class Based Iteration
      ///
      /// This interface provides iterator classes for edges.
      ///
      /// @{

      /// \brief This iterator goes through each edge.
      ///
      /// This iterator goes through each edge.
      typedef GraphItemIt<Graph, Edge> EdgeIt;

      /// \brief This iterator goes trough the incident edges of a
      /// node.
      ///
      /// This iterator goes trough the incident edges of a certain
      /// node of a graph.
      typedef GraphIncIt<Graph, Edge, Node, 'e'> IncEdgeIt;

      /// \brief The base node of the iterator.
      ///
      /// This function gives back the base node of the iterator.
      Node baseNode(const IncEdgeIt&) const { return INVALID; }

      /// \brief The running node of the iterator.
      ///
      /// This function gives back the running node of the iterator.
      Node runningNode(const IncEdgeIt&) const { return INVALID; }

      /// @}

      template <typename _Graph>
      struct Constraints {
        void constraints() {
          checkConcept<IterableDigraphComponent<Base>, _Graph>();

          {
            typename _Graph::Node node(INVALID);
            typename _Graph::Edge edge(INVALID);
            bool dir;
            {
              graph.first(edge);
              graph.next(edge);
            }
            {
              graph.firstInc(edge, dir, node);
              graph.nextInc(edge, dir);
            }

          }

          {
            checkConcept<GraphItemIt<_Graph, typename _Graph::Edge>,
              typename _Graph::EdgeIt >();
            checkConcept<GraphIncIt<_Graph, typename _Graph::Edge,
              typename _Graph::Node, 'e'>, typename _Graph::IncEdgeIt>();

            typename _Graph::Node n;
            const typename _Graph::IncEdgeIt ieit(INVALID);
            n = graph.baseNode(ieit);
            n = graph.runningNode(ieit);
          }
        }

        const _Graph& graph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for iterable undirected bipartite graphs.
    ///
    /// This class describes the interface of iterable undirected
    /// bipartite graphs. It extends \ref IterableGraphComponent with
    /// the core iterable interface of undirected bipartite graphs.
    /// This concept is part of the BpGraph concept.
    template <typename BAS = BaseBpGraphComponent>
    class IterableBpGraphComponent : public IterableGraphComponent<BAS> {
    public:

      typedef BAS Base;
      typedef typename Base::Node Node;
      typedef typename Base::RedNode RedNode;
      typedef typename Base::BlueNode BlueNode;
      typedef typename Base::Arc Arc;
      typedef typename Base::Edge Edge;

      typedef IterableBpGraphComponent BpGraph;

      using IterableGraphComponent<BAS>::first;
      using IterableGraphComponent<BAS>::next;

      /// \name Base Iteration
      ///
      /// This interface provides functions for iteration on red and blue nodes.
      ///
      /// @{

      /// \brief Return the first red node.
      ///
      /// This function gives back the first red node in the iteration order.
      void first(RedNode&) const {}

      /// \brief Return the next red node.
      ///
      /// This function gives back the next red node in the iteration order.
      void next(RedNode&) const {}

      /// \brief Return the first blue node.
      ///
      /// This function gives back the first blue node in the iteration order.
      void first(BlueNode&) const {}

      /// \brief Return the next blue node.
      ///
      /// This function gives back the next blue node in the iteration order.
      void next(BlueNode&) const {}


      /// @}

      /// \name Class Based Iteration
      ///
      /// This interface provides iterator classes for red and blue nodes.
      ///
      /// @{

      /// \brief This iterator goes through each red node.
      ///
      /// This iterator goes through each red node.
      typedef GraphItemIt<BpGraph, RedNode> RedNodeIt;

      /// \brief This iterator goes through each blue node.
      ///
      /// This iterator goes through each blue node.
      typedef GraphItemIt<BpGraph, BlueNode> BlueNodeIt;

      /// @}

      template <typename _BpGraph>
      struct Constraints {
        void constraints() {
          checkConcept<IterableGraphComponent<Base>, _BpGraph>();

          typename _BpGraph::RedNode rn(INVALID);
          bpgraph.first(rn);
          bpgraph.next(rn);
          typename _BpGraph::BlueNode bn(INVALID);
          bpgraph.first(bn);
          bpgraph.next(bn);

          checkConcept<GraphItemIt<_BpGraph, typename _BpGraph::RedNode>,
            typename _BpGraph::RedNodeIt>();
          checkConcept<GraphItemIt<_BpGraph, typename _BpGraph::BlueNode>,
            typename _BpGraph::BlueNodeIt>();
        }

        const _BpGraph& bpgraph;
      };
    };

    /// \brief Skeleton class for alterable directed graphs.
    ///
    /// This class describes the interface of alterable directed
    /// graphs. It extends \ref BaseDigraphComponent with the alteration
    /// notifier interface. It implements
    /// an observer-notifier pattern for each digraph item. More
    /// obsevers can be registered into the notifier and whenever an
    /// alteration occured in the digraph all the observers will be
    /// notified about it.
    template <typename BAS = BaseDigraphComponent>
    class AlterableDigraphComponent : public BAS {
    public:

      typedef BAS Base;
      typedef typename Base::Node Node;
      typedef typename Base::Arc Arc;


      /// Node alteration notifier class.
      typedef AlterationNotifier<AlterableDigraphComponent, Node>
      NodeNotifier;
      /// Arc alteration notifier class.
      typedef AlterationNotifier<AlterableDigraphComponent, Arc>
      ArcNotifier;

      mutable NodeNotifier node_notifier;
      mutable ArcNotifier arc_notifier;

      /// \brief Return the node alteration notifier.
      ///
      /// This function gives back the node alteration notifier.
      NodeNotifier& notifier(Node) const {
        return node_notifier;
      }

      /// \brief Return the arc alteration notifier.
      ///
      /// This function gives back the arc alteration notifier.
      ArcNotifier& notifier(Arc) const {
        return arc_notifier;
      }

      template <typename _Digraph>
      struct Constraints {
        void constraints() {
          checkConcept<Base, _Digraph>();
          typename _Digraph::NodeNotifier& nn
            = digraph.notifier(typename _Digraph::Node());

          typename _Digraph::ArcNotifier& en
            = digraph.notifier(typename _Digraph::Arc());

          ::lemon::ignore_unused_variable_warning(nn);
          ::lemon::ignore_unused_variable_warning(en);
        }

        const _Digraph& digraph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for alterable undirected graphs.
    ///
    /// This class describes the interface of alterable undirected
    /// graphs. It extends \ref AlterableDigraphComponent with the alteration
    /// notifier interface of undirected graphs. It implements
    /// an observer-notifier pattern for the edges. More
    /// obsevers can be registered into the notifier and whenever an
    /// alteration occured in the graph all the observers will be
    /// notified about it.
    template <typename BAS = BaseGraphComponent>
    class AlterableGraphComponent : public AlterableDigraphComponent<BAS> {
    public:

      typedef BAS Base;
      typedef AlterableDigraphComponent<Base> Parent;
      typedef typename Base::Edge Edge;


      /// Edge alteration notifier class.
      typedef AlterationNotifier<AlterableGraphComponent, Edge>
      EdgeNotifier;

      mutable EdgeNotifier edge_notifier;

      using Parent::notifier;

      /// \brief Return the edge alteration notifier.
      ///
      /// This function gives back the edge alteration notifier.
      EdgeNotifier& notifier(Edge) const {
        return edge_notifier;
      }

      template <typename _Graph>
      struct Constraints {
        void constraints() {
          checkConcept<AlterableDigraphComponent<Base>, _Graph>();
          typename _Graph::EdgeNotifier& uen
            = graph.notifier(typename _Graph::Edge());
          ::lemon::ignore_unused_variable_warning(uen);
        }

        const _Graph& graph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for alterable undirected bipartite graphs.
    ///
    /// This class describes the interface of alterable undirected
    /// bipartite graphs. It extends \ref AlterableGraphComponent with
    /// the alteration notifier interface of bipartite graphs. It
    /// implements an observer-notifier pattern for the red and blue
    /// nodes. More obsevers can be registered into the notifier and
    /// whenever an alteration occured in the graph all the observers
    /// will be notified about it.
    template <typename BAS = BaseBpGraphComponent>
    class AlterableBpGraphComponent : public AlterableGraphComponent<BAS> {
    public:

      typedef BAS Base;
      typedef AlterableGraphComponent<Base> Parent;
      typedef typename Base::RedNode RedNode;
      typedef typename Base::BlueNode BlueNode;


      /// Red node alteration notifier class.
      typedef AlterationNotifier<AlterableBpGraphComponent, RedNode>
      RedNodeNotifier;

      /// Blue node alteration notifier class.
      typedef AlterationNotifier<AlterableBpGraphComponent, BlueNode>
      BlueNodeNotifier;

      mutable RedNodeNotifier red_node_notifier;
      mutable BlueNodeNotifier blue_node_notifier;

      using Parent::notifier;

      /// \brief Return the red node alteration notifier.
      ///
      /// This function gives back the red node alteration notifier.
      RedNodeNotifier& notifier(RedNode) const {
        return red_node_notifier;
      }

      /// \brief Return the blue node alteration notifier.
      ///
      /// This function gives back the blue node alteration notifier.
      BlueNodeNotifier& notifier(BlueNode) const {
        return blue_node_notifier;
      }

      template <typename _BpGraph>
      struct Constraints {
        void constraints() {
          checkConcept<AlterableGraphComponent<Base>, _BpGraph>();
          typename _BpGraph::RedNodeNotifier& rnn
            = bpgraph.notifier(typename _BpGraph::RedNode());
          typename _BpGraph::BlueNodeNotifier& bnn
            = bpgraph.notifier(typename _BpGraph::BlueNode());
          ::lemon::ignore_unused_variable_warning(rnn);
          ::lemon::ignore_unused_variable_warning(bnn);
        }

        const _BpGraph& bpgraph;
      };
    };

    /// \brief Concept class for standard graph maps.
    ///
    /// This class describes the concept of standard graph maps, i.e.
    /// the \c NodeMap, \c ArcMap and \c EdgeMap subtypes of digraph and
    /// graph types, which can be used for associating data to graph items.
    /// The standard graph maps must conform to the ReferenceMap concept.
    template <typename GR, typename K, typename V>
    class GraphMap : public ReferenceMap<K, V, V&, const V&> {
      typedef ReferenceMap<K, V, V&, const V&> Parent;

    public:

      /// The key type of the map.
      typedef K Key;
      /// The value type of the map.
      typedef V Value;
      /// The reference type of the map.
      typedef Value& Reference;
      /// The const reference type of the map.
      typedef const Value& ConstReference;

      // The reference map tag.
      typedef True ReferenceMapTag;

      /// \brief Construct a new map.
      ///
      /// Construct a new map for the graph.
      explicit GraphMap(const GR&) {}
      /// \brief Construct a new map with default value.
      ///
      /// Construct a new map for the graph and initalize the values.
      GraphMap(const GR&, const Value&) {}

    private:
      /// \brief Copy constructor.
      ///
      /// Copy Constructor.
      GraphMap(const GraphMap&) : Parent() {}

      /// \brief Assignment operator.
      ///
      /// Assignment operator. It does not mofify the underlying graph,
      /// it just iterates on the current item set and set the  map
      /// with the value returned by the assigned map.
      template <typename CMap>
      GraphMap& operator=(const CMap&) {
        checkConcept<ReadMap<Key, Value>, CMap>();
        return *this;
      }

    public:
      template<typename _Map>
      struct Constraints {
        void constraints() {
          checkConcept
            <ReferenceMap<Key, Value, Value&, const Value&>, _Map>();
          _Map m1(g);
          _Map m2(g,t);

          // Copy constructor
          // _Map m3(m);

          // Assignment operator
          // ReadMap<Key, Value> cmap;
          // m3 = cmap;

          ::lemon::ignore_unused_variable_warning(m1);
          ::lemon::ignore_unused_variable_warning(m2);
          // ::lemon::ignore_unused_variable_warning(m3);
        }

        const _Map &m;
        const GR &g;
        const typename GraphMap::Value &t;
        Constraints() {}
      };

    };

    /// \brief Skeleton class for mappable directed graphs.
    ///
    /// This class describes the interface of mappable directed graphs.
    /// It extends \ref BaseDigraphComponent with the standard digraph
    /// map classes, namely \c NodeMap and \c ArcMap.
    /// This concept is part of the Digraph concept.
    template <typename BAS = BaseDigraphComponent>
    class MappableDigraphComponent : public BAS  {
    public:

      typedef BAS Base;
      typedef typename Base::Node Node;
      typedef typename Base::Arc Arc;

      typedef MappableDigraphComponent Digraph;

      /// \brief Standard graph map for the nodes.
      ///
      /// Standard graph map for the nodes.
      /// It conforms to the ReferenceMap concept.
      template <typename V>
      class NodeMap : public GraphMap<MappableDigraphComponent, Node, V> {
        typedef GraphMap<MappableDigraphComponent, Node, V> Parent;

      public:
        /// \brief Construct a new map.
        ///
        /// Construct a new map for the digraph.
        explicit NodeMap(const MappableDigraphComponent& digraph)
          : Parent(digraph) {}

        /// \brief Construct a new map with default value.
        ///
        /// Construct a new map for the digraph and initalize the values.
        NodeMap(const MappableDigraphComponent& digraph, const V& value)
          : Parent(digraph, value) {}

      private:
        /// \brief Copy constructor.
        ///
        /// Copy Constructor.
        NodeMap(const NodeMap& nm) : Parent(nm) {}

        /// \brief Assignment operator.
        ///
        /// Assignment operator.
        template <typename CMap>
        NodeMap& operator=(const CMap&) {
          checkConcept<ReadMap<Node, V>, CMap>();
          return *this;
        }

      };

      /// \brief Standard graph map for the arcs.
      ///
      /// Standard graph map for the arcs.
      /// It conforms to the ReferenceMap concept.
      template <typename V>
      class ArcMap : public GraphMap<MappableDigraphComponent, Arc, V> {
        typedef GraphMap<MappableDigraphComponent, Arc, V> Parent;

      public:
        /// \brief Construct a new map.
        ///
        /// Construct a new map for the digraph.
        explicit ArcMap(const MappableDigraphComponent& digraph)
          : Parent(digraph) {}

        /// \brief Construct a new map with default value.
        ///
        /// Construct a new map for the digraph and initalize the values.
        ArcMap(const MappableDigraphComponent& digraph, const V& value)
          : Parent(digraph, value) {}

      private:
        /// \brief Copy constructor.
        ///
        /// Copy Constructor.
        ArcMap(const ArcMap& nm) : Parent(nm) {}

        /// \brief Assignment operator.
        ///
        /// Assignment operator.
        template <typename CMap>
        ArcMap& operator=(const CMap&) {
          checkConcept<ReadMap<Arc, V>, CMap>();
          return *this;
        }

      };


      template <typename _Digraph>
      struct Constraints {

        struct Dummy {
          int value;
          Dummy() : value(0) {}
          Dummy(int _v) : value(_v) {}
        };

        void constraints() {
          checkConcept<Base, _Digraph>();
          { // int map test
            typedef typename _Digraph::template NodeMap<int> IntNodeMap;
            checkConcept<GraphMap<_Digraph, typename _Digraph::Node, int>,
              IntNodeMap >();
          } { // bool map test
            typedef typename _Digraph::template NodeMap<bool> BoolNodeMap;
            checkConcept<GraphMap<_Digraph, typename _Digraph::Node, bool>,
              BoolNodeMap >();
          } { // Dummy map test
            typedef typename _Digraph::template NodeMap<Dummy> DummyNodeMap;
            checkConcept<GraphMap<_Digraph, typename _Digraph::Node, Dummy>,
              DummyNodeMap >();
          }

          { // int map test
            typedef typename _Digraph::template ArcMap<int> IntArcMap;
            checkConcept<GraphMap<_Digraph, typename _Digraph::Arc, int>,
              IntArcMap >();
          } { // bool map test
            typedef typename _Digraph::template ArcMap<bool> BoolArcMap;
            checkConcept<GraphMap<_Digraph, typename _Digraph::Arc, bool>,
              BoolArcMap >();
          } { // Dummy map test
            typedef typename _Digraph::template ArcMap<Dummy> DummyArcMap;
            checkConcept<GraphMap<_Digraph, typename _Digraph::Arc, Dummy>,
              DummyArcMap >();
          }
        }

        const _Digraph& digraph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for mappable undirected graphs.
    ///
    /// This class describes the interface of mappable undirected graphs.
    /// It extends \ref MappableDigraphComponent with the standard graph
    /// map class for edges (\c EdgeMap).
    /// This concept is part of the Graph concept.
    template <typename BAS = BaseGraphComponent>
    class MappableGraphComponent : public MappableDigraphComponent<BAS>  {
    public:

      typedef BAS Base;
      typedef typename Base::Edge Edge;

      typedef MappableGraphComponent Graph;

      /// \brief Standard graph map for the edges.
      ///
      /// Standard graph map for the edges.
      /// It conforms to the ReferenceMap concept.
      template <typename V>
      class EdgeMap : public GraphMap<MappableGraphComponent, Edge, V> {
        typedef GraphMap<MappableGraphComponent, Edge, V> Parent;

      public:
        /// \brief Construct a new map.
        ///
        /// Construct a new map for the graph.
        explicit EdgeMap(const MappableGraphComponent& graph)
          : Parent(graph) {}

        /// \brief Construct a new map with default value.
        ///
        /// Construct a new map for the graph and initalize the values.
        EdgeMap(const MappableGraphComponent& graph, const V& value)
          : Parent(graph, value) {}

      private:
        /// \brief Copy constructor.
        ///
        /// Copy Constructor.
        EdgeMap(const EdgeMap& nm) : Parent(nm) {}

        /// \brief Assignment operator.
        ///
        /// Assignment operator.
        template <typename CMap>
        EdgeMap& operator=(const CMap&) {
          checkConcept<ReadMap<Edge, V>, CMap>();
          return *this;
        }

      };


      template <typename _Graph>
      struct Constraints {

        struct Dummy {
          int value;
          Dummy() : value(0) {}
          Dummy(int _v) : value(_v) {}
        };

        void constraints() {
          checkConcept<MappableDigraphComponent<Base>, _Graph>();

          { // int map test
            typedef typename _Graph::template EdgeMap<int> IntEdgeMap;
            checkConcept<GraphMap<_Graph, typename _Graph::Edge, int>,
              IntEdgeMap >();
          } { // bool map test
            typedef typename _Graph::template EdgeMap<bool> BoolEdgeMap;
            checkConcept<GraphMap<_Graph, typename _Graph::Edge, bool>,
              BoolEdgeMap >();
          } { // Dummy map test
            typedef typename _Graph::template EdgeMap<Dummy> DummyEdgeMap;
            checkConcept<GraphMap<_Graph, typename _Graph::Edge, Dummy>,
              DummyEdgeMap >();
          }
        }

        const _Graph& graph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for mappable undirected bipartite graphs.
    ///
    /// This class describes the interface of mappable undirected
    /// bipartite graphs.  It extends \ref MappableGraphComponent with
    /// the standard graph map class for red and blue nodes (\c
    /// RedNodeMap and BlueNodeMap). This concept is part of the
    /// BpGraph concept.
    template <typename BAS = BaseBpGraphComponent>
    class MappableBpGraphComponent : public MappableGraphComponent<BAS>  {
    public:

      typedef BAS Base;
      typedef typename Base::Node Node;

      typedef MappableBpGraphComponent BpGraph;

      /// \brief Standard graph map for the red nodes.
      ///
      /// Standard graph map for the red nodes.
      /// It conforms to the ReferenceMap concept.
      template <typename V>
      class RedNodeMap : public GraphMap<MappableBpGraphComponent, Node, V> {
        typedef GraphMap<MappableBpGraphComponent, Node, V> Parent;

      public:
        /// \brief Construct a new map.
        ///
        /// Construct a new map for the graph.
        explicit RedNodeMap(const MappableBpGraphComponent& graph)
          : Parent(graph) {}

        /// \brief Construct a new map with default value.
        ///
        /// Construct a new map for the graph and initalize the values.
        RedNodeMap(const MappableBpGraphComponent& graph, const V& value)
          : Parent(graph, value) {}

      private:
        /// \brief Copy constructor.
        ///
        /// Copy Constructor.
        RedNodeMap(const RedNodeMap& nm) : Parent(nm) {}

        /// \brief Assignment operator.
        ///
        /// Assignment operator.
        template <typename CMap>
        RedNodeMap& operator=(const CMap&) {
          checkConcept<ReadMap<Node, V>, CMap>();
          return *this;
        }

      };

      /// \brief Standard graph map for the blue nodes.
      ///
      /// Standard graph map for the blue nodes.
      /// It conforms to the ReferenceMap concept.
      template <typename V>
      class BlueNodeMap : public GraphMap<MappableBpGraphComponent, Node, V> {
        typedef GraphMap<MappableBpGraphComponent, Node, V> Parent;

      public:
        /// \brief Construct a new map.
        ///
        /// Construct a new map for the graph.
        explicit BlueNodeMap(const MappableBpGraphComponent& graph)
          : Parent(graph) {}

        /// \brief Construct a new map with default value.
        ///
        /// Construct a new map for the graph and initalize the values.
        BlueNodeMap(const MappableBpGraphComponent& graph, const V& value)
          : Parent(graph, value) {}

      private:
        /// \brief Copy constructor.
        ///
        /// Copy Constructor.
        BlueNodeMap(const BlueNodeMap& nm) : Parent(nm) {}

        /// \brief Assignment operator.
        ///
        /// Assignment operator.
        template <typename CMap>
        BlueNodeMap& operator=(const CMap&) {
          checkConcept<ReadMap<Node, V>, CMap>();
          return *this;
        }

      };


      template <typename _BpGraph>
      struct Constraints {

        struct Dummy {
          int value;
          Dummy() : value(0) {}
          Dummy(int _v) : value(_v) {}
        };

        void constraints() {
          checkConcept<MappableGraphComponent<Base>, _BpGraph>();

          { // int map test
            typedef typename _BpGraph::template RedNodeMap<int>
              IntRedNodeMap;
            checkConcept<GraphMap<_BpGraph, typename _BpGraph::RedNode, int>,
              IntRedNodeMap >();
          } { // bool map test
            typedef typename _BpGraph::template RedNodeMap<bool>
              BoolRedNodeMap;
            checkConcept<GraphMap<_BpGraph, typename _BpGraph::RedNode, bool>,
              BoolRedNodeMap >();
          } { // Dummy map test
            typedef typename _BpGraph::template RedNodeMap<Dummy>
              DummyRedNodeMap;
            checkConcept<GraphMap<_BpGraph, typename _BpGraph::RedNode, Dummy>,
              DummyRedNodeMap >();
          }

          { // int map test
            typedef typename _BpGraph::template BlueNodeMap<int>
              IntBlueNodeMap;
            checkConcept<GraphMap<_BpGraph, typename _BpGraph::BlueNode, int>,
              IntBlueNodeMap >();
          } { // bool map test
            typedef typename _BpGraph::template BlueNodeMap<bool>
              BoolBlueNodeMap;
            checkConcept<GraphMap<_BpGraph, typename _BpGraph::BlueNode, bool>,
              BoolBlueNodeMap >();
          } { // Dummy map test
            typedef typename _BpGraph::template BlueNodeMap<Dummy>
              DummyBlueNodeMap;
            checkConcept<GraphMap<_BpGraph, typename _BpGraph::BlueNode, Dummy>,
              DummyBlueNodeMap >();
          }
        }

        const _BpGraph& bpgraph;
      };
    };

    /// \brief Skeleton class for extendable directed graphs.
    ///
    /// This class describes the interface of extendable directed graphs.
    /// It extends \ref BaseDigraphComponent with functions for adding
    /// nodes and arcs to the digraph.
    /// This concept requires \ref AlterableDigraphComponent.
    template <typename BAS = BaseDigraphComponent>
    class ExtendableDigraphComponent : public BAS {
    public:
      typedef BAS Base;

      typedef typename Base::Node Node;
      typedef typename Base::Arc Arc;

      /// \brief Add a new node to the digraph.
      ///
      /// This function adds a new node to the digraph.
      Node addNode() {
        return INVALID;
      }

      /// \brief Add a new arc connecting the given two nodes.
      ///
      /// This function adds a new arc connecting the given two nodes
      /// of the digraph.
      Arc addArc(const Node&, const Node&) {
        return INVALID;
      }

      template <typename _Digraph>
      struct Constraints {
        void constraints() {
          checkConcept<Base, _Digraph>();
          typename _Digraph::Node node_a, node_b;
          node_a = digraph.addNode();
          node_b = digraph.addNode();
          typename _Digraph::Arc arc;
          arc = digraph.addArc(node_a, node_b);
        }

        _Digraph& digraph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for extendable undirected graphs.
    ///
    /// This class describes the interface of extendable undirected graphs.
    /// It extends \ref BaseGraphComponent with functions for adding
    /// nodes and edges to the graph.
    /// This concept requires \ref AlterableGraphComponent.
    template <typename BAS = BaseGraphComponent>
    class ExtendableGraphComponent : public BAS {
    public:

      typedef BAS Base;
      typedef typename Base::Node Node;
      typedef typename Base::Edge Edge;

      /// \brief Add a new node to the digraph.
      ///
      /// This function adds a new node to the digraph.
      Node addNode() {
        return INVALID;
      }

      /// \brief Add a new edge connecting the given two nodes.
      ///
      /// This function adds a new edge connecting the given two nodes
      /// of the graph.
      Edge addEdge(const Node&, const Node&) {
        return INVALID;
      }

      template <typename _Graph>
      struct Constraints {
        void constraints() {
          checkConcept<Base, _Graph>();
          typename _Graph::Node node_a, node_b;
          node_a = graph.addNode();
          node_b = graph.addNode();
          typename _Graph::Edge edge;
          edge = graph.addEdge(node_a, node_b);
        }

        _Graph& graph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for extendable undirected bipartite graphs.
    ///
    /// This class describes the interface of extendable undirected
    /// bipartite graphs. It extends \ref BaseGraphComponent with
    /// functions for adding nodes and edges to the graph. This
    /// concept requires \ref AlterableBpGraphComponent.
    template <typename BAS = BaseBpGraphComponent>
    class ExtendableBpGraphComponent : public BAS {
    public:

      typedef BAS Base;
      typedef typename Base::Node Node;
      typedef typename Base::RedNode RedNode;
      typedef typename Base::BlueNode BlueNode;
      typedef typename Base::Edge Edge;

      /// \brief Add a new red node to the digraph.
      ///
      /// This function adds a red new node to the digraph.
      RedNode addRedNode() {
        return INVALID;
      }

      /// \brief Add a new blue node to the digraph.
      ///
      /// This function adds a blue new node to the digraph.
      BlueNode addBlueNode() {
        return INVALID;
      }

      /// \brief Add a new edge connecting the given two nodes.
      ///
      /// This function adds a new edge connecting the given two nodes
      /// of the graph. The first node has to be a red node, and the
      /// second one a blue node.
      Edge addEdge(const RedNode&, const BlueNode&) {
        return INVALID;
      }
      Edge addEdge(const BlueNode&, const RedNode&) {
        return INVALID;
      }

      template <typename _BpGraph>
      struct Constraints {
        void constraints() {
          checkConcept<Base, _BpGraph>();
          typename _BpGraph::RedNode red_node;
          typename _BpGraph::BlueNode blue_node;
          red_node = bpgraph.addRedNode();
          blue_node = bpgraph.addBlueNode();
          typename _BpGraph::Edge edge;
          edge = bpgraph.addEdge(red_node, blue_node);
          edge = bpgraph.addEdge(blue_node, red_node);
        }

        _BpGraph& bpgraph;
      };
    };

    /// \brief Skeleton class for erasable directed graphs.
    ///
    /// This class describes the interface of erasable directed graphs.
    /// It extends \ref BaseDigraphComponent with functions for removing
    /// nodes and arcs from the digraph.
    /// This concept requires \ref AlterableDigraphComponent.
    template <typename BAS = BaseDigraphComponent>
    class ErasableDigraphComponent : public BAS {
    public:

      typedef BAS Base;
      typedef typename Base::Node Node;
      typedef typename Base::Arc Arc;

      /// \brief Erase a node from the digraph.
      ///
      /// This function erases the given node from the digraph and all arcs
      /// connected to the node.
      void erase(const Node&) {}

      /// \brief Erase an arc from the digraph.
      ///
      /// This function erases the given arc from the digraph.
      void erase(const Arc&) {}

      template <typename _Digraph>
      struct Constraints {
        void constraints() {
          checkConcept<Base, _Digraph>();
          const typename _Digraph::Node node(INVALID);
          digraph.erase(node);
          const typename _Digraph::Arc arc(INVALID);
          digraph.erase(arc);
        }

        _Digraph& digraph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for erasable undirected graphs.
    ///
    /// This class describes the interface of erasable undirected graphs.
    /// It extends \ref BaseGraphComponent with functions for removing
    /// nodes and edges from the graph.
    /// This concept requires \ref AlterableGraphComponent.
    template <typename BAS = BaseGraphComponent>
    class ErasableGraphComponent : public BAS {
    public:

      typedef BAS Base;
      typedef typename Base::Node Node;
      typedef typename Base::Edge Edge;

      /// \brief Erase a node from the graph.
      ///
      /// This function erases the given node from the graph and all edges
      /// connected to the node.
      void erase(const Node&) {}

      /// \brief Erase an edge from the digraph.
      ///
      /// This function erases the given edge from the digraph.
      void erase(const Edge&) {}

      template <typename _Graph>
      struct Constraints {
        void constraints() {
          checkConcept<Base, _Graph>();
          const typename _Graph::Node node(INVALID);
          graph.erase(node);
          const typename _Graph::Edge edge(INVALID);
          graph.erase(edge);
        }

        _Graph& graph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for erasable undirected graphs.
    ///
    /// This class describes the interface of erasable undirected
    /// bipartite graphs. It extends \ref BaseBpGraphComponent with
    /// functions for removing nodes and edges from the graph. This
    /// concept requires \ref AlterableBpGraphComponent.
    template <typename BAS = BaseBpGraphComponent>
    class ErasableBpGraphComponent : public ErasableGraphComponent<BAS> {};

    /// \brief Skeleton class for clearable directed graphs.
    ///
    /// This class describes the interface of clearable directed graphs.
    /// It extends \ref BaseDigraphComponent with a function for clearing
    /// the digraph.
    /// This concept requires \ref AlterableDigraphComponent.
    template <typename BAS = BaseDigraphComponent>
    class ClearableDigraphComponent : public BAS {
    public:

      typedef BAS Base;

      /// \brief Erase all nodes and arcs from the digraph.
      ///
      /// This function erases all nodes and arcs from the digraph.
      void clear() {}

      template <typename _Digraph>
      struct Constraints {
        void constraints() {
          checkConcept<Base, _Digraph>();
          digraph.clear();
        }

        _Digraph& digraph;
        Constraints() {}
      };
    };

    /// \brief Skeleton class for clearable undirected graphs.
    ///
    /// This class describes the interface of clearable undirected graphs.
    /// It extends \ref BaseGraphComponent with a function for clearing
    /// the graph.
    /// This concept requires \ref AlterableGraphComponent.
    template <typename BAS = BaseGraphComponent>
    class ClearableGraphComponent : public ClearableDigraphComponent<BAS> {};

    /// \brief Skeleton class for clearable undirected biparite graphs.
    ///
    /// This class describes the interface of clearable undirected
    /// bipartite graphs. It extends \ref BaseBpGraphComponent with a
    /// function for clearing the graph.  This concept requires \ref
    /// AlterableBpGraphComponent.
    template <typename BAS = BaseBpGraphComponent>
    class ClearableBpGraphComponent : public ClearableGraphComponent<BAS> {};

  }

}

#endif
