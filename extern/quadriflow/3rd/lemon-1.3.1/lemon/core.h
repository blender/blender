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

#ifndef LEMON_CORE_H
#define LEMON_CORE_H

#include <vector>
#include <algorithm>

#include <lemon/config.h>
#include <lemon/bits/enable_if.h>
#include <lemon/bits/traits.h>
#include <lemon/assert.h>

// Disable the following warnings when compiling with MSVC:
// C4250: 'class1' : inherits 'class2::member' via dominance
// C4355: 'this' : used in base member initializer list
// C4503: 'function' : decorated name length exceeded, name was truncated
// C4800: 'type' : forcing value to bool 'true' or 'false' (performance warning)
// C4996: 'function': was declared deprecated
#ifdef _MSC_VER
#pragma warning( disable : 4250 4355 4503 4800 4996 )
#endif

#ifdef __GNUC__
#define GCC_VERSION (__GNUC__ * 10000                   \
                     + __GNUC_MINOR__ * 100             \
                     + __GNUC_PATCHLEVEL__)
#endif

#if GCC_VERSION >= 40800
// Needed by the [DI]GRAPH_TYPEDEFS marcos for gcc 4.8
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif

///\file
///\brief LEMON core utilities.
///
///This header file contains core utilities for LEMON.
///It is automatically included by all graph types, therefore it usually
///do not have to be included directly.

namespace lemon {

  /// \brief Dummy type to make it easier to create invalid iterators.
  ///
  /// Dummy type to make it easier to create invalid iterators.
  /// See \ref INVALID for the usage.
  struct Invalid {
  public:
    bool operator==(Invalid) { return true;  }
    bool operator!=(Invalid) { return false; }
    bool operator< (Invalid) { return false; }
  };

  /// \brief Invalid iterators.
  ///
  /// \ref Invalid is a global type that converts to each iterator
  /// in such a way that the value of the target iterator will be invalid.
#ifdef LEMON_ONLY_TEMPLATES
  const Invalid INVALID = Invalid();
#else
  extern const Invalid INVALID;
#endif

  /// \addtogroup gutils
  /// @{

  ///Create convenience typedefs for the digraph types and iterators

  ///This \c \#define creates convenient type definitions for the following
  ///types of \c Digraph: \c Node,  \c NodeIt, \c Arc, \c ArcIt, \c InArcIt,
  ///\c OutArcIt, \c BoolNodeMap, \c IntNodeMap, \c DoubleNodeMap,
  ///\c BoolArcMap, \c IntArcMap, \c DoubleArcMap.
  ///
  ///\note If the graph type is a dependent type, ie. the graph type depend
  ///on a template parameter, then use \c TEMPLATE_DIGRAPH_TYPEDEFS()
  ///macro.
#define DIGRAPH_TYPEDEFS(Digraph)                                       \
  typedef Digraph::Node Node;                                           \
  typedef Digraph::NodeIt NodeIt;                                       \
  typedef Digraph::Arc Arc;                                             \
  typedef Digraph::ArcIt ArcIt;                                         \
  typedef Digraph::InArcIt InArcIt;                                     \
  typedef Digraph::OutArcIt OutArcIt;                                   \
  typedef Digraph::NodeMap<bool> BoolNodeMap;                           \
  typedef Digraph::NodeMap<int> IntNodeMap;                             \
  typedef Digraph::NodeMap<double> DoubleNodeMap;                       \
  typedef Digraph::ArcMap<bool> BoolArcMap;                             \
  typedef Digraph::ArcMap<int> IntArcMap;                               \
  typedef Digraph::ArcMap<double> DoubleArcMap

  ///Create convenience typedefs for the digraph types and iterators

  ///\see DIGRAPH_TYPEDEFS
  ///
  ///\note Use this macro, if the graph type is a dependent type,
  ///ie. the graph type depend on a template parameter.
#define TEMPLATE_DIGRAPH_TYPEDEFS(Digraph)                              \
  typedef typename Digraph::Node Node;                                  \
  typedef typename Digraph::NodeIt NodeIt;                              \
  typedef typename Digraph::Arc Arc;                                    \
  typedef typename Digraph::ArcIt ArcIt;                                \
  typedef typename Digraph::InArcIt InArcIt;                            \
  typedef typename Digraph::OutArcIt OutArcIt;                          \
  typedef typename Digraph::template NodeMap<bool> BoolNodeMap;         \
  typedef typename Digraph::template NodeMap<int> IntNodeMap;           \
  typedef typename Digraph::template NodeMap<double> DoubleNodeMap;     \
  typedef typename Digraph::template ArcMap<bool> BoolArcMap;           \
  typedef typename Digraph::template ArcMap<int> IntArcMap;             \
  typedef typename Digraph::template ArcMap<double> DoubleArcMap

  ///Create convenience typedefs for the graph types and iterators

  ///This \c \#define creates the same convenient type definitions as defined
  ///by \ref DIGRAPH_TYPEDEFS(Graph) and six more, namely it creates
  ///\c Edge, \c EdgeIt, \c IncEdgeIt, \c BoolEdgeMap, \c IntEdgeMap,
  ///\c DoubleEdgeMap.
  ///
  ///\note If the graph type is a dependent type, ie. the graph type depend
  ///on a template parameter, then use \c TEMPLATE_GRAPH_TYPEDEFS()
  ///macro.
#define GRAPH_TYPEDEFS(Graph)                                           \
  DIGRAPH_TYPEDEFS(Graph);                                              \
  typedef Graph::Edge Edge;                                             \
  typedef Graph::EdgeIt EdgeIt;                                         \
  typedef Graph::IncEdgeIt IncEdgeIt;                                   \
  typedef Graph::EdgeMap<bool> BoolEdgeMap;                             \
  typedef Graph::EdgeMap<int> IntEdgeMap;                               \
  typedef Graph::EdgeMap<double> DoubleEdgeMap

  ///Create convenience typedefs for the graph types and iterators

  ///\see GRAPH_TYPEDEFS
  ///
  ///\note Use this macro, if the graph type is a dependent type,
  ///ie. the graph type depend on a template parameter.
#define TEMPLATE_GRAPH_TYPEDEFS(Graph)                                  \
  TEMPLATE_DIGRAPH_TYPEDEFS(Graph);                                     \
  typedef typename Graph::Edge Edge;                                    \
  typedef typename Graph::EdgeIt EdgeIt;                                \
  typedef typename Graph::IncEdgeIt IncEdgeIt;                          \
  typedef typename Graph::template EdgeMap<bool> BoolEdgeMap;           \
  typedef typename Graph::template EdgeMap<int> IntEdgeMap;             \
  typedef typename Graph::template EdgeMap<double> DoubleEdgeMap

  ///Create convenience typedefs for the bipartite graph types and iterators

  ///This \c \#define creates the same convenient type definitions as
  ///defined by \ref GRAPH_TYPEDEFS(BpGraph) and ten more, namely it
  ///creates \c RedNode, \c RedNodeIt, \c BoolRedNodeMap,
  ///\c IntRedNodeMap, \c DoubleRedNodeMap, \c BlueNode, \c BlueNodeIt,
  ///\c BoolBlueNodeMap, \c IntBlueNodeMap, \c DoubleBlueNodeMap.
  ///
  ///\note If the graph type is a dependent type, ie. the graph type depend
  ///on a template parameter, then use \c TEMPLATE_BPGRAPH_TYPEDEFS()
  ///macro.
#define BPGRAPH_TYPEDEFS(BpGraph)                                       \
  GRAPH_TYPEDEFS(BpGraph);                                              \
  typedef BpGraph::RedNode RedNode;                                     \
  typedef BpGraph::RedNodeIt RedNodeIt;                                 \
  typedef BpGraph::RedNodeMap<bool> BoolRedNodeMap;                     \
  typedef BpGraph::RedNodeMap<int> IntRedNodeMap;                       \
  typedef BpGraph::RedNodeMap<double> DoubleRedNodeMap;                 \
  typedef BpGraph::BlueNode BlueNode;                                   \
  typedef BpGraph::BlueNodeIt BlueNodeIt;                               \
  typedef BpGraph::BlueNodeMap<bool> BoolBlueNodeMap;                   \
  typedef BpGraph::BlueNodeMap<int> IntBlueNodeMap;                     \
  typedef BpGraph::BlueNodeMap<double> DoubleBlueNodeMap

  ///Create convenience typedefs for the bipartite graph types and iterators

  ///\see BPGRAPH_TYPEDEFS
  ///
  ///\note Use this macro, if the graph type is a dependent type,
  ///ie. the graph type depend on a template parameter.
#define TEMPLATE_BPGRAPH_TYPEDEFS(BpGraph)                                  \
  TEMPLATE_GRAPH_TYPEDEFS(BpGraph);                                         \
  typedef typename BpGraph::RedNode RedNode;                                \
  typedef typename BpGraph::RedNodeIt RedNodeIt;                            \
  typedef typename BpGraph::template RedNodeMap<bool> BoolRedNodeMap;       \
  typedef typename BpGraph::template RedNodeMap<int> IntRedNodeMap;         \
  typedef typename BpGraph::template RedNodeMap<double> DoubleRedNodeMap;   \
  typedef typename BpGraph::BlueNode BlueNode;                              \
  typedef typename BpGraph::BlueNodeIt BlueNodeIt;                          \
  typedef typename BpGraph::template BlueNodeMap<bool> BoolBlueNodeMap;     \
  typedef typename BpGraph::template BlueNodeMap<int> IntBlueNodeMap;       \
  typedef typename BpGraph::template BlueNodeMap<double> DoubleBlueNodeMap

  /// \brief Function to count the items in a graph.
  ///
  /// This function counts the items (nodes, arcs etc.) in a graph.
  /// The complexity of the function is linear because
  /// it iterates on all of the items.
  template <typename Graph, typename Item>
  inline int countItems(const Graph& g) {
    typedef typename ItemSetTraits<Graph, Item>::ItemIt ItemIt;
    int num = 0;
    for (ItemIt it(g); it != INVALID; ++it) {
      ++num;
    }
    return num;
  }

  // Node counting:

  namespace _core_bits {

    template <typename Graph, typename Enable = void>
    struct CountNodesSelector {
      static int count(const Graph &g) {
        return countItems<Graph, typename Graph::Node>(g);
      }
    };

    template <typename Graph>
    struct CountNodesSelector<
      Graph, typename
      enable_if<typename Graph::NodeNumTag, void>::type>
    {
      static int count(const Graph &g) {
        return g.nodeNum();
      }
    };
  }

  /// \brief Function to count the nodes in the graph.
  ///
  /// This function counts the nodes in the graph.
  /// The complexity of the function is <em>O</em>(<em>n</em>), but for some
  /// graph structures it is specialized to run in <em>O</em>(1).
  ///
  /// \note If the graph contains a \c nodeNum() member function and a
  /// \c NodeNumTag tag then this function calls directly the member
  /// function to query the cardinality of the node set.
  template <typename Graph>
  inline int countNodes(const Graph& g) {
    return _core_bits::CountNodesSelector<Graph>::count(g);
  }

  namespace _graph_utils_bits {

    template <typename Graph, typename Enable = void>
    struct CountRedNodesSelector {
      static int count(const Graph &g) {
        return countItems<Graph, typename Graph::RedNode>(g);
      }
    };

    template <typename Graph>
    struct CountRedNodesSelector<
      Graph, typename
      enable_if<typename Graph::NodeNumTag, void>::type>
    {
      static int count(const Graph &g) {
        return g.redNum();
      }
    };
  }

  /// \brief Function to count the red nodes in the graph.
  ///
  /// This function counts the red nodes in the graph.
  /// The complexity of the function is O(n) but for some
  /// graph structures it is specialized to run in O(1).
  ///
  /// If the graph contains a \e redNum() member function and a
  /// \e NodeNumTag tag then this function calls directly the member
  /// function to query the cardinality of the node set.
  template <typename Graph>
  inline int countRedNodes(const Graph& g) {
    return _graph_utils_bits::CountRedNodesSelector<Graph>::count(g);
  }

  namespace _graph_utils_bits {

    template <typename Graph, typename Enable = void>
    struct CountBlueNodesSelector {
      static int count(const Graph &g) {
        return countItems<Graph, typename Graph::BlueNode>(g);
      }
    };

    template <typename Graph>
    struct CountBlueNodesSelector<
      Graph, typename
      enable_if<typename Graph::NodeNumTag, void>::type>
    {
      static int count(const Graph &g) {
        return g.blueNum();
      }
    };
  }

  /// \brief Function to count the blue nodes in the graph.
  ///
  /// This function counts the blue nodes in the graph.
  /// The complexity of the function is O(n) but for some
  /// graph structures it is specialized to run in O(1).
  ///
  /// If the graph contains a \e blueNum() member function and a
  /// \e NodeNumTag tag then this function calls directly the member
  /// function to query the cardinality of the node set.
  template <typename Graph>
  inline int countBlueNodes(const Graph& g) {
    return _graph_utils_bits::CountBlueNodesSelector<Graph>::count(g);
  }

  // Arc counting:

  namespace _core_bits {

    template <typename Graph, typename Enable = void>
    struct CountArcsSelector {
      static int count(const Graph &g) {
        return countItems<Graph, typename Graph::Arc>(g);
      }
    };

    template <typename Graph>
    struct CountArcsSelector<
      Graph,
      typename enable_if<typename Graph::ArcNumTag, void>::type>
    {
      static int count(const Graph &g) {
        return g.arcNum();
      }
    };
  }

  /// \brief Function to count the arcs in the graph.
  ///
  /// This function counts the arcs in the graph.
  /// The complexity of the function is <em>O</em>(<em>m</em>), but for some
  /// graph structures it is specialized to run in <em>O</em>(1).
  ///
  /// \note If the graph contains a \c arcNum() member function and a
  /// \c ArcNumTag tag then this function calls directly the member
  /// function to query the cardinality of the arc set.
  template <typename Graph>
  inline int countArcs(const Graph& g) {
    return _core_bits::CountArcsSelector<Graph>::count(g);
  }

  // Edge counting:

  namespace _core_bits {

    template <typename Graph, typename Enable = void>
    struct CountEdgesSelector {
      static int count(const Graph &g) {
        return countItems<Graph, typename Graph::Edge>(g);
      }
    };

    template <typename Graph>
    struct CountEdgesSelector<
      Graph,
      typename enable_if<typename Graph::EdgeNumTag, void>::type>
    {
      static int count(const Graph &g) {
        return g.edgeNum();
      }
    };
  }

  /// \brief Function to count the edges in the graph.
  ///
  /// This function counts the edges in the graph.
  /// The complexity of the function is <em>O</em>(<em>m</em>), but for some
  /// graph structures it is specialized to run in <em>O</em>(1).
  ///
  /// \note If the graph contains a \c edgeNum() member function and a
  /// \c EdgeNumTag tag then this function calls directly the member
  /// function to query the cardinality of the edge set.
  template <typename Graph>
  inline int countEdges(const Graph& g) {
    return _core_bits::CountEdgesSelector<Graph>::count(g);

  }


  template <typename Graph, typename DegIt>
  inline int countNodeDegree(const Graph& _g, const typename Graph::Node& _n) {
    int num = 0;
    for (DegIt it(_g, _n); it != INVALID; ++it) {
      ++num;
    }
    return num;
  }

  /// \brief Function to count the number of the out-arcs from node \c n.
  ///
  /// This function counts the number of the out-arcs from node \c n
  /// in the graph \c g.
  template <typename Graph>
  inline int countOutArcs(const Graph& g,  const typename Graph::Node& n) {
    return countNodeDegree<Graph, typename Graph::OutArcIt>(g, n);
  }

  /// \brief Function to count the number of the in-arcs to node \c n.
  ///
  /// This function counts the number of the in-arcs to node \c n
  /// in the graph \c g.
  template <typename Graph>
  inline int countInArcs(const Graph& g,  const typename Graph::Node& n) {
    return countNodeDegree<Graph, typename Graph::InArcIt>(g, n);
  }

  /// \brief Function to count the number of the inc-edges to node \c n.
  ///
  /// This function counts the number of the inc-edges to node \c n
  /// in the undirected graph \c g.
  template <typename Graph>
  inline int countIncEdges(const Graph& g,  const typename Graph::Node& n) {
    return countNodeDegree<Graph, typename Graph::IncEdgeIt>(g, n);
  }

  namespace _core_bits {

    template <typename Digraph, typename Item, typename RefMap>
    class MapCopyBase {
    public:
      virtual void copy(const Digraph& from, const RefMap& refMap) = 0;

      virtual ~MapCopyBase() {}
    };

    template <typename Digraph, typename Item, typename RefMap,
              typename FromMap, typename ToMap>
    class MapCopy : public MapCopyBase<Digraph, Item, RefMap> {
    public:

      MapCopy(const FromMap& map, ToMap& tmap)
        : _map(map), _tmap(tmap) {}

      virtual void copy(const Digraph& digraph, const RefMap& refMap) {
        typedef typename ItemSetTraits<Digraph, Item>::ItemIt ItemIt;
        for (ItemIt it(digraph); it != INVALID; ++it) {
          _tmap.set(refMap[it], _map[it]);
        }
      }

    private:
      const FromMap& _map;
      ToMap& _tmap;
    };

    template <typename Digraph, typename Item, typename RefMap, typename It>
    class ItemCopy : public MapCopyBase<Digraph, Item, RefMap> {
    public:

      ItemCopy(const Item& item, It& it) : _item(item), _it(it) {}

      virtual void copy(const Digraph&, const RefMap& refMap) {
        _it = refMap[_item];
      }

    private:
      Item _item;
      It& _it;
    };

    template <typename Digraph, typename Item, typename RefMap, typename Ref>
    class RefCopy : public MapCopyBase<Digraph, Item, RefMap> {
    public:

      RefCopy(Ref& map) : _map(map) {}

      virtual void copy(const Digraph& digraph, const RefMap& refMap) {
        typedef typename ItemSetTraits<Digraph, Item>::ItemIt ItemIt;
        for (ItemIt it(digraph); it != INVALID; ++it) {
          _map.set(it, refMap[it]);
        }
      }

    private:
      Ref& _map;
    };

    template <typename Digraph, typename Item, typename RefMap,
              typename CrossRef>
    class CrossRefCopy : public MapCopyBase<Digraph, Item, RefMap> {
    public:

      CrossRefCopy(CrossRef& cmap) : _cmap(cmap) {}

      virtual void copy(const Digraph& digraph, const RefMap& refMap) {
        typedef typename ItemSetTraits<Digraph, Item>::ItemIt ItemIt;
        for (ItemIt it(digraph); it != INVALID; ++it) {
          _cmap.set(refMap[it], it);
        }
      }

    private:
      CrossRef& _cmap;
    };

    template <typename Digraph, typename Enable = void>
    struct DigraphCopySelector {
      template <typename From, typename NodeRefMap, typename ArcRefMap>
      static void copy(const From& from, Digraph &to,
                       NodeRefMap& nodeRefMap, ArcRefMap& arcRefMap) {
        to.clear();
        for (typename From::NodeIt it(from); it != INVALID; ++it) {
          nodeRefMap[it] = to.addNode();
        }
        for (typename From::ArcIt it(from); it != INVALID; ++it) {
          arcRefMap[it] = to.addArc(nodeRefMap[from.source(it)],
                                    nodeRefMap[from.target(it)]);
        }
      }
    };

    template <typename Digraph>
    struct DigraphCopySelector<
      Digraph,
      typename enable_if<typename Digraph::BuildTag, void>::type>
    {
      template <typename From, typename NodeRefMap, typename ArcRefMap>
      static void copy(const From& from, Digraph &to,
                       NodeRefMap& nodeRefMap, ArcRefMap& arcRefMap) {
        to.build(from, nodeRefMap, arcRefMap);
      }
    };

    template <typename Graph, typename Enable = void>
    struct GraphCopySelector {
      template <typename From, typename NodeRefMap, typename EdgeRefMap>
      static void copy(const From& from, Graph &to,
                       NodeRefMap& nodeRefMap, EdgeRefMap& edgeRefMap) {
        to.clear();
        for (typename From::NodeIt it(from); it != INVALID; ++it) {
          nodeRefMap[it] = to.addNode();
        }
        for (typename From::EdgeIt it(from); it != INVALID; ++it) {
          edgeRefMap[it] = to.addEdge(nodeRefMap[from.u(it)],
                                      nodeRefMap[from.v(it)]);
        }
      }
    };

    template <typename Graph>
    struct GraphCopySelector<
      Graph,
      typename enable_if<typename Graph::BuildTag, void>::type>
    {
      template <typename From, typename NodeRefMap, typename EdgeRefMap>
      static void copy(const From& from, Graph &to,
                       NodeRefMap& nodeRefMap,
                       EdgeRefMap& edgeRefMap) {
        to.build(from, nodeRefMap, edgeRefMap);
      }
    };

    template <typename BpGraph, typename Enable = void>
    struct BpGraphCopySelector {
      template <typename From, typename RedNodeRefMap,
                typename BlueNodeRefMap, typename EdgeRefMap>
      static void copy(const From& from, BpGraph &to,
                       RedNodeRefMap& redNodeRefMap,
                       BlueNodeRefMap& blueNodeRefMap,
                       EdgeRefMap& edgeRefMap) {
        to.clear();
        for (typename From::RedNodeIt it(from); it != INVALID; ++it) {
          redNodeRefMap[it] = to.addRedNode();
        }
        for (typename From::BlueNodeIt it(from); it != INVALID; ++it) {
          blueNodeRefMap[it] = to.addBlueNode();
        }
        for (typename From::EdgeIt it(from); it != INVALID; ++it) {
          edgeRefMap[it] = to.addEdge(redNodeRefMap[from.redNode(it)],
                                      blueNodeRefMap[from.blueNode(it)]);
        }
      }
    };

    template <typename BpGraph>
    struct BpGraphCopySelector<
      BpGraph,
      typename enable_if<typename BpGraph::BuildTag, void>::type>
    {
      template <typename From, typename RedNodeRefMap,
                typename BlueNodeRefMap, typename EdgeRefMap>
      static void copy(const From& from, BpGraph &to,
                       RedNodeRefMap& redNodeRefMap,
                       BlueNodeRefMap& blueNodeRefMap,
                       EdgeRefMap& edgeRefMap) {
        to.build(from, redNodeRefMap, blueNodeRefMap, edgeRefMap);
      }
    };

  }

  /// \brief Check whether a graph is undirected.
  ///
  /// This function returns \c true if the given graph is undirected.
#ifdef DOXYGEN
  template <typename GR>
  bool undirected(const GR& g) { return false; }
#else
  template <typename GR>
  typename enable_if<UndirectedTagIndicator<GR>, bool>::type
  undirected(const GR&) {
    return true;
  }
  template <typename GR>
  typename disable_if<UndirectedTagIndicator<GR>, bool>::type
  undirected(const GR&) {
    return false;
  }
#endif

  /// \brief Class to copy a digraph.
  ///
  /// Class to copy a digraph to another digraph (duplicate a digraph). The
  /// simplest way of using it is through the \c digraphCopy() function.
  ///
  /// This class not only make a copy of a digraph, but it can create
  /// references and cross references between the nodes and arcs of
  /// the two digraphs, and it can copy maps to use with the newly created
  /// digraph.
  ///
  /// To make a copy from a digraph, first an instance of DigraphCopy
  /// should be created, then the data belongs to the digraph should
  /// assigned to copy. In the end, the \c run() member should be
  /// called.
  ///
  /// The next code copies a digraph with several data:
  ///\code
  ///  DigraphCopy<OrigGraph, NewGraph> cg(orig_graph, new_graph);
  ///  // Create references for the nodes
  ///  OrigGraph::NodeMap<NewGraph::Node> nr(orig_graph);
  ///  cg.nodeRef(nr);
  ///  // Create cross references (inverse) for the arcs
  ///  NewGraph::ArcMap<OrigGraph::Arc> acr(new_graph);
  ///  cg.arcCrossRef(acr);
  ///  // Copy an arc map
  ///  OrigGraph::ArcMap<double> oamap(orig_graph);
  ///  NewGraph::ArcMap<double> namap(new_graph);
  ///  cg.arcMap(oamap, namap);
  ///  // Copy a node
  ///  OrigGraph::Node on;
  ///  NewGraph::Node nn;
  ///  cg.node(on, nn);
  ///  // Execute copying
  ///  cg.run();
  ///\endcode
  template <typename From, typename To>
  class DigraphCopy {
  private:

    typedef typename From::Node Node;
    typedef typename From::NodeIt NodeIt;
    typedef typename From::Arc Arc;
    typedef typename From::ArcIt ArcIt;

    typedef typename To::Node TNode;
    typedef typename To::Arc TArc;

    typedef typename From::template NodeMap<TNode> NodeRefMap;
    typedef typename From::template ArcMap<TArc> ArcRefMap;

  public:

    /// \brief Constructor of DigraphCopy.
    ///
    /// Constructor of DigraphCopy for copying the content of the
    /// \c from digraph into the \c to digraph.
    DigraphCopy(const From& from, To& to)
      : _from(from), _to(to) {}

    /// \brief Destructor of DigraphCopy
    ///
    /// Destructor of DigraphCopy.
    ~DigraphCopy() {
      for (int i = 0; i < int(_node_maps.size()); ++i) {
        delete _node_maps[i];
      }
      for (int i = 0; i < int(_arc_maps.size()); ++i) {
        delete _arc_maps[i];
      }

    }

    /// \brief Copy the node references into the given map.
    ///
    /// This function copies the node references into the given map.
    /// The parameter should be a map, whose key type is the Node type of
    /// the source digraph, while the value type is the Node type of the
    /// destination digraph.
    template <typename NodeRef>
    DigraphCopy& nodeRef(NodeRef& map) {
      _node_maps.push_back(new _core_bits::RefCopy<From, Node,
                           NodeRefMap, NodeRef>(map));
      return *this;
    }

    /// \brief Copy the node cross references into the given map.
    ///
    /// This function copies the node cross references (reverse references)
    /// into the given map. The parameter should be a map, whose key type
    /// is the Node type of the destination digraph, while the value type is
    /// the Node type of the source digraph.
    template <typename NodeCrossRef>
    DigraphCopy& nodeCrossRef(NodeCrossRef& map) {
      _node_maps.push_back(new _core_bits::CrossRefCopy<From, Node,
                           NodeRefMap, NodeCrossRef>(map));
      return *this;
    }

    /// \brief Make a copy of the given node map.
    ///
    /// This function makes a copy of the given node map for the newly
    /// created digraph.
    /// The key type of the new map \c tmap should be the Node type of the
    /// destination digraph, and the key type of the original map \c map
    /// should be the Node type of the source digraph.
    template <typename FromMap, typename ToMap>
    DigraphCopy& nodeMap(const FromMap& map, ToMap& tmap) {
      _node_maps.push_back(new _core_bits::MapCopy<From, Node,
                           NodeRefMap, FromMap, ToMap>(map, tmap));
      return *this;
    }

    /// \brief Make a copy of the given node.
    ///
    /// This function makes a copy of the given node.
    DigraphCopy& node(const Node& node, TNode& tnode) {
      _node_maps.push_back(new _core_bits::ItemCopy<From, Node,
                           NodeRefMap, TNode>(node, tnode));
      return *this;
    }

    /// \brief Copy the arc references into the given map.
    ///
    /// This function copies the arc references into the given map.
    /// The parameter should be a map, whose key type is the Arc type of
    /// the source digraph, while the value type is the Arc type of the
    /// destination digraph.
    template <typename ArcRef>
    DigraphCopy& arcRef(ArcRef& map) {
      _arc_maps.push_back(new _core_bits::RefCopy<From, Arc,
                          ArcRefMap, ArcRef>(map));
      return *this;
    }

    /// \brief Copy the arc cross references into the given map.
    ///
    /// This function copies the arc cross references (reverse references)
    /// into the given map. The parameter should be a map, whose key type
    /// is the Arc type of the destination digraph, while the value type is
    /// the Arc type of the source digraph.
    template <typename ArcCrossRef>
    DigraphCopy& arcCrossRef(ArcCrossRef& map) {
      _arc_maps.push_back(new _core_bits::CrossRefCopy<From, Arc,
                          ArcRefMap, ArcCrossRef>(map));
      return *this;
    }

    /// \brief Make a copy of the given arc map.
    ///
    /// This function makes a copy of the given arc map for the newly
    /// created digraph.
    /// The key type of the new map \c tmap should be the Arc type of the
    /// destination digraph, and the key type of the original map \c map
    /// should be the Arc type of the source digraph.
    template <typename FromMap, typename ToMap>
    DigraphCopy& arcMap(const FromMap& map, ToMap& tmap) {
      _arc_maps.push_back(new _core_bits::MapCopy<From, Arc,
                          ArcRefMap, FromMap, ToMap>(map, tmap));
      return *this;
    }

    /// \brief Make a copy of the given arc.
    ///
    /// This function makes a copy of the given arc.
    DigraphCopy& arc(const Arc& arc, TArc& tarc) {
      _arc_maps.push_back(new _core_bits::ItemCopy<From, Arc,
                          ArcRefMap, TArc>(arc, tarc));
      return *this;
    }

    /// \brief Execute copying.
    ///
    /// This function executes the copying of the digraph along with the
    /// copying of the assigned data.
    void run() {
      NodeRefMap nodeRefMap(_from);
      ArcRefMap arcRefMap(_from);
      _core_bits::DigraphCopySelector<To>::
        copy(_from, _to, nodeRefMap, arcRefMap);
      for (int i = 0; i < int(_node_maps.size()); ++i) {
        _node_maps[i]->copy(_from, nodeRefMap);
      }
      for (int i = 0; i < int(_arc_maps.size()); ++i) {
        _arc_maps[i]->copy(_from, arcRefMap);
      }
    }

  protected:

    const From& _from;
    To& _to;

    std::vector<_core_bits::MapCopyBase<From, Node, NodeRefMap>* >
      _node_maps;

    std::vector<_core_bits::MapCopyBase<From, Arc, ArcRefMap>* >
      _arc_maps;

  };

  /// \brief Copy a digraph to another digraph.
  ///
  /// This function copies a digraph to another digraph.
  /// The complete usage of it is detailed in the DigraphCopy class, but
  /// a short example shows a basic work:
  ///\code
  /// digraphCopy(src, trg).nodeRef(nr).arcCrossRef(acr).run();
  ///\endcode
  ///
  /// After the copy the \c nr map will contain the mapping from the
  /// nodes of the \c from digraph to the nodes of the \c to digraph and
  /// \c acr will contain the mapping from the arcs of the \c to digraph
  /// to the arcs of the \c from digraph.
  ///
  /// \see DigraphCopy
  template <typename From, typename To>
  DigraphCopy<From, To> digraphCopy(const From& from, To& to) {
    return DigraphCopy<From, To>(from, to);
  }

  /// \brief Class to copy a graph.
  ///
  /// Class to copy a graph to another graph (duplicate a graph). The
  /// simplest way of using it is through the \c graphCopy() function.
  ///
  /// This class not only make a copy of a graph, but it can create
  /// references and cross references between the nodes, edges and arcs of
  /// the two graphs, and it can copy maps for using with the newly created
  /// graph.
  ///
  /// To make a copy from a graph, first an instance of GraphCopy
  /// should be created, then the data belongs to the graph should
  /// assigned to copy. In the end, the \c run() member should be
  /// called.
  ///
  /// The next code copies a graph with several data:
  ///\code
  ///  GraphCopy<OrigGraph, NewGraph> cg(orig_graph, new_graph);
  ///  // Create references for the nodes
  ///  OrigGraph::NodeMap<NewGraph::Node> nr(orig_graph);
  ///  cg.nodeRef(nr);
  ///  // Create cross references (inverse) for the edges
  ///  NewGraph::EdgeMap<OrigGraph::Edge> ecr(new_graph);
  ///  cg.edgeCrossRef(ecr);
  ///  // Copy an edge map
  ///  OrigGraph::EdgeMap<double> oemap(orig_graph);
  ///  NewGraph::EdgeMap<double> nemap(new_graph);
  ///  cg.edgeMap(oemap, nemap);
  ///  // Copy a node
  ///  OrigGraph::Node on;
  ///  NewGraph::Node nn;
  ///  cg.node(on, nn);
  ///  // Execute copying
  ///  cg.run();
  ///\endcode
  template <typename From, typename To>
  class GraphCopy {
  private:

    typedef typename From::Node Node;
    typedef typename From::NodeIt NodeIt;
    typedef typename From::Arc Arc;
    typedef typename From::ArcIt ArcIt;
    typedef typename From::Edge Edge;
    typedef typename From::EdgeIt EdgeIt;

    typedef typename To::Node TNode;
    typedef typename To::Arc TArc;
    typedef typename To::Edge TEdge;

    typedef typename From::template NodeMap<TNode> NodeRefMap;
    typedef typename From::template EdgeMap<TEdge> EdgeRefMap;

    struct ArcRefMap {
      ArcRefMap(const From& from, const To& to,
                const EdgeRefMap& edge_ref, const NodeRefMap& node_ref)
        : _from(from), _to(to),
          _edge_ref(edge_ref), _node_ref(node_ref) {}

      typedef typename From::Arc Key;
      typedef typename To::Arc Value;

      Value operator[](const Key& key) const {
        bool forward = _from.u(key) != _from.v(key) ?
          _node_ref[_from.source(key)] ==
          _to.source(_to.direct(_edge_ref[key], true)) :
          _from.direction(key);
        return _to.direct(_edge_ref[key], forward);
      }

      const From& _from;
      const To& _to;
      const EdgeRefMap& _edge_ref;
      const NodeRefMap& _node_ref;
    };

  public:

    /// \brief Constructor of GraphCopy.
    ///
    /// Constructor of GraphCopy for copying the content of the
    /// \c from graph into the \c to graph.
    GraphCopy(const From& from, To& to)
      : _from(from), _to(to) {}

    /// \brief Destructor of GraphCopy
    ///
    /// Destructor of GraphCopy.
    ~GraphCopy() {
      for (int i = 0; i < int(_node_maps.size()); ++i) {
        delete _node_maps[i];
      }
      for (int i = 0; i < int(_arc_maps.size()); ++i) {
        delete _arc_maps[i];
      }
      for (int i = 0; i < int(_edge_maps.size()); ++i) {
        delete _edge_maps[i];
      }
    }

    /// \brief Copy the node references into the given map.
    ///
    /// This function copies the node references into the given map.
    /// The parameter should be a map, whose key type is the Node type of
    /// the source graph, while the value type is the Node type of the
    /// destination graph.
    template <typename NodeRef>
    GraphCopy& nodeRef(NodeRef& map) {
      _node_maps.push_back(new _core_bits::RefCopy<From, Node,
                           NodeRefMap, NodeRef>(map));
      return *this;
    }

    /// \brief Copy the node cross references into the given map.
    ///
    /// This function copies the node cross references (reverse references)
    /// into the given map. The parameter should be a map, whose key type
    /// is the Node type of the destination graph, while the value type is
    /// the Node type of the source graph.
    template <typename NodeCrossRef>
    GraphCopy& nodeCrossRef(NodeCrossRef& map) {
      _node_maps.push_back(new _core_bits::CrossRefCopy<From, Node,
                           NodeRefMap, NodeCrossRef>(map));
      return *this;
    }

    /// \brief Make a copy of the given node map.
    ///
    /// This function makes a copy of the given node map for the newly
    /// created graph.
    /// The key type of the new map \c tmap should be the Node type of the
    /// destination graph, and the key type of the original map \c map
    /// should be the Node type of the source graph.
    template <typename FromMap, typename ToMap>
    GraphCopy& nodeMap(const FromMap& map, ToMap& tmap) {
      _node_maps.push_back(new _core_bits::MapCopy<From, Node,
                           NodeRefMap, FromMap, ToMap>(map, tmap));
      return *this;
    }

    /// \brief Make a copy of the given node.
    ///
    /// This function makes a copy of the given node.
    GraphCopy& node(const Node& node, TNode& tnode) {
      _node_maps.push_back(new _core_bits::ItemCopy<From, Node,
                           NodeRefMap, TNode>(node, tnode));
      return *this;
    }

    /// \brief Copy the arc references into the given map.
    ///
    /// This function copies the arc references into the given map.
    /// The parameter should be a map, whose key type is the Arc type of
    /// the source graph, while the value type is the Arc type of the
    /// destination graph.
    template <typename ArcRef>
    GraphCopy& arcRef(ArcRef& map) {
      _arc_maps.push_back(new _core_bits::RefCopy<From, Arc,
                          ArcRefMap, ArcRef>(map));
      return *this;
    }

    /// \brief Copy the arc cross references into the given map.
    ///
    /// This function copies the arc cross references (reverse references)
    /// into the given map. The parameter should be a map, whose key type
    /// is the Arc type of the destination graph, while the value type is
    /// the Arc type of the source graph.
    template <typename ArcCrossRef>
    GraphCopy& arcCrossRef(ArcCrossRef& map) {
      _arc_maps.push_back(new _core_bits::CrossRefCopy<From, Arc,
                          ArcRefMap, ArcCrossRef>(map));
      return *this;
    }

    /// \brief Make a copy of the given arc map.
    ///
    /// This function makes a copy of the given arc map for the newly
    /// created graph.
    /// The key type of the new map \c tmap should be the Arc type of the
    /// destination graph, and the key type of the original map \c map
    /// should be the Arc type of the source graph.
    template <typename FromMap, typename ToMap>
    GraphCopy& arcMap(const FromMap& map, ToMap& tmap) {
      _arc_maps.push_back(new _core_bits::MapCopy<From, Arc,
                          ArcRefMap, FromMap, ToMap>(map, tmap));
      return *this;
    }

    /// \brief Make a copy of the given arc.
    ///
    /// This function makes a copy of the given arc.
    GraphCopy& arc(const Arc& arc, TArc& tarc) {
      _arc_maps.push_back(new _core_bits::ItemCopy<From, Arc,
                          ArcRefMap, TArc>(arc, tarc));
      return *this;
    }

    /// \brief Copy the edge references into the given map.
    ///
    /// This function copies the edge references into the given map.
    /// The parameter should be a map, whose key type is the Edge type of
    /// the source graph, while the value type is the Edge type of the
    /// destination graph.
    template <typename EdgeRef>
    GraphCopy& edgeRef(EdgeRef& map) {
      _edge_maps.push_back(new _core_bits::RefCopy<From, Edge,
                           EdgeRefMap, EdgeRef>(map));
      return *this;
    }

    /// \brief Copy the edge cross references into the given map.
    ///
    /// This function copies the edge cross references (reverse references)
    /// into the given map. The parameter should be a map, whose key type
    /// is the Edge type of the destination graph, while the value type is
    /// the Edge type of the source graph.
    template <typename EdgeCrossRef>
    GraphCopy& edgeCrossRef(EdgeCrossRef& map) {
      _edge_maps.push_back(new _core_bits::CrossRefCopy<From,
                           Edge, EdgeRefMap, EdgeCrossRef>(map));
      return *this;
    }

    /// \brief Make a copy of the given edge map.
    ///
    /// This function makes a copy of the given edge map for the newly
    /// created graph.
    /// The key type of the new map \c tmap should be the Edge type of the
    /// destination graph, and the key type of the original map \c map
    /// should be the Edge type of the source graph.
    template <typename FromMap, typename ToMap>
    GraphCopy& edgeMap(const FromMap& map, ToMap& tmap) {
      _edge_maps.push_back(new _core_bits::MapCopy<From, Edge,
                           EdgeRefMap, FromMap, ToMap>(map, tmap));
      return *this;
    }

    /// \brief Make a copy of the given edge.
    ///
    /// This function makes a copy of the given edge.
    GraphCopy& edge(const Edge& edge, TEdge& tedge) {
      _edge_maps.push_back(new _core_bits::ItemCopy<From, Edge,
                           EdgeRefMap, TEdge>(edge, tedge));
      return *this;
    }

    /// \brief Execute copying.
    ///
    /// This function executes the copying of the graph along with the
    /// copying of the assigned data.
    void run() {
      NodeRefMap nodeRefMap(_from);
      EdgeRefMap edgeRefMap(_from);
      ArcRefMap arcRefMap(_from, _to, edgeRefMap, nodeRefMap);
      _core_bits::GraphCopySelector<To>::
        copy(_from, _to, nodeRefMap, edgeRefMap);
      for (int i = 0; i < int(_node_maps.size()); ++i) {
        _node_maps[i]->copy(_from, nodeRefMap);
      }
      for (int i = 0; i < int(_edge_maps.size()); ++i) {
        _edge_maps[i]->copy(_from, edgeRefMap);
      }
      for (int i = 0; i < int(_arc_maps.size()); ++i) {
        _arc_maps[i]->copy(_from, arcRefMap);
      }
    }

  private:

    const From& _from;
    To& _to;

    std::vector<_core_bits::MapCopyBase<From, Node, NodeRefMap>* >
      _node_maps;

    std::vector<_core_bits::MapCopyBase<From, Arc, ArcRefMap>* >
      _arc_maps;

    std::vector<_core_bits::MapCopyBase<From, Edge, EdgeRefMap>* >
      _edge_maps;

  };

  /// \brief Copy a graph to another graph.
  ///
  /// This function copies a graph to another graph.
  /// The complete usage of it is detailed in the GraphCopy class,
  /// but a short example shows a basic work:
  ///\code
  /// graphCopy(src, trg).nodeRef(nr).edgeCrossRef(ecr).run();
  ///\endcode
  ///
  /// After the copy the \c nr map will contain the mapping from the
  /// nodes of the \c from graph to the nodes of the \c to graph and
  /// \c ecr will contain the mapping from the edges of the \c to graph
  /// to the edges of the \c from graph.
  ///
  /// \see GraphCopy
  template <typename From, typename To>
  GraphCopy<From, To>
  graphCopy(const From& from, To& to) {
    return GraphCopy<From, To>(from, to);
  }

  /// \brief Class to copy a bipartite graph.
  ///
  /// Class to copy a bipartite graph to another graph (duplicate a
  /// graph). The simplest way of using it is through the
  /// \c bpGraphCopy() function.
  ///
  /// This class not only make a copy of a bipartite graph, but it can
  /// create references and cross references between the nodes, edges
  /// and arcs of the two graphs, and it can copy maps for using with
  /// the newly created graph.
  ///
  /// To make a copy from a graph, first an instance of BpGraphCopy
  /// should be created, then the data belongs to the graph should
  /// assigned to copy. In the end, the \c run() member should be
  /// called.
  ///
  /// The next code copies a graph with several data:
  ///\code
  ///  BpGraphCopy<OrigBpGraph, NewBpGraph> cg(orig_graph, new_graph);
  ///  // Create references for the nodes
  ///  OrigBpGraph::NodeMap<NewBpGraph::Node> nr(orig_graph);
  ///  cg.nodeRef(nr);
  ///  // Create cross references (inverse) for the edges
  ///  NewBpGraph::EdgeMap<OrigBpGraph::Edge> ecr(new_graph);
  ///  cg.edgeCrossRef(ecr);
  ///  // Copy a red node map
  ///  OrigBpGraph::RedNodeMap<double> ormap(orig_graph);
  ///  NewBpGraph::RedNodeMap<double> nrmap(new_graph);
  ///  cg.redNodeMap(ormap, nrmap);
  ///  // Copy a node
  ///  OrigBpGraph::Node on;
  ///  NewBpGraph::Node nn;
  ///  cg.node(on, nn);
  ///  // Execute copying
  ///  cg.run();
  ///\endcode
  template <typename From, typename To>
  class BpGraphCopy {
  private:

    typedef typename From::Node Node;
    typedef typename From::RedNode RedNode;
    typedef typename From::BlueNode BlueNode;
    typedef typename From::NodeIt NodeIt;
    typedef typename From::Arc Arc;
    typedef typename From::ArcIt ArcIt;
    typedef typename From::Edge Edge;
    typedef typename From::EdgeIt EdgeIt;

    typedef typename To::Node TNode;
    typedef typename To::RedNode TRedNode;
    typedef typename To::BlueNode TBlueNode;
    typedef typename To::Arc TArc;
    typedef typename To::Edge TEdge;

    typedef typename From::template RedNodeMap<TRedNode> RedNodeRefMap;
    typedef typename From::template BlueNodeMap<TBlueNode> BlueNodeRefMap;
    typedef typename From::template EdgeMap<TEdge> EdgeRefMap;

    struct NodeRefMap {
      NodeRefMap(const From& from, const RedNodeRefMap& red_node_ref,
                 const BlueNodeRefMap& blue_node_ref)
        : _from(from), _red_node_ref(red_node_ref),
          _blue_node_ref(blue_node_ref) {}

      typedef typename From::Node Key;
      typedef typename To::Node Value;

      Value operator[](const Key& key) const {
        if (_from.red(key)) {
          return _red_node_ref[_from.asRedNodeUnsafe(key)];
        } else {
          return _blue_node_ref[_from.asBlueNodeUnsafe(key)];
        }
      }

      const From& _from;
      const RedNodeRefMap& _red_node_ref;
      const BlueNodeRefMap& _blue_node_ref;
    };

    struct ArcRefMap {
      ArcRefMap(const From& from, const To& to, const EdgeRefMap& edge_ref)
        : _from(from), _to(to), _edge_ref(edge_ref) {}

      typedef typename From::Arc Key;
      typedef typename To::Arc Value;

      Value operator[](const Key& key) const {
        return _to.direct(_edge_ref[key], _from.direction(key));
      }

      const From& _from;
      const To& _to;
      const EdgeRefMap& _edge_ref;
    };

  public:

    /// \brief Constructor of BpGraphCopy.
    ///
    /// Constructor of BpGraphCopy for copying the content of the
    /// \c from graph into the \c to graph.
    BpGraphCopy(const From& from, To& to)
      : _from(from), _to(to) {}

    /// \brief Destructor of BpGraphCopy
    ///
    /// Destructor of BpGraphCopy.
    ~BpGraphCopy() {
      for (int i = 0; i < int(_node_maps.size()); ++i) {
        delete _node_maps[i];
      }
      for (int i = 0; i < int(_red_maps.size()); ++i) {
        delete _red_maps[i];
      }
      for (int i = 0; i < int(_blue_maps.size()); ++i) {
        delete _blue_maps[i];
      }
      for (int i = 0; i < int(_arc_maps.size()); ++i) {
        delete _arc_maps[i];
      }
      for (int i = 0; i < int(_edge_maps.size()); ++i) {
        delete _edge_maps[i];
      }
    }

    /// \brief Copy the node references into the given map.
    ///
    /// This function copies the node references into the given map.
    /// The parameter should be a map, whose key type is the Node type of
    /// the source graph, while the value type is the Node type of the
    /// destination graph.
    template <typename NodeRef>
    BpGraphCopy& nodeRef(NodeRef& map) {
      _node_maps.push_back(new _core_bits::RefCopy<From, Node,
                           NodeRefMap, NodeRef>(map));
      return *this;
    }

    /// \brief Copy the node cross references into the given map.
    ///
    /// This function copies the node cross references (reverse references)
    /// into the given map. The parameter should be a map, whose key type
    /// is the Node type of the destination graph, while the value type is
    /// the Node type of the source graph.
    template <typename NodeCrossRef>
    BpGraphCopy& nodeCrossRef(NodeCrossRef& map) {
      _node_maps.push_back(new _core_bits::CrossRefCopy<From, Node,
                           NodeRefMap, NodeCrossRef>(map));
      return *this;
    }

    /// \brief Make a copy of the given node map.
    ///
    /// This function makes a copy of the given node map for the newly
    /// created graph.
    /// The key type of the new map \c tmap should be the Node type of the
    /// destination graph, and the key type of the original map \c map
    /// should be the Node type of the source graph.
    template <typename FromMap, typename ToMap>
    BpGraphCopy& nodeMap(const FromMap& map, ToMap& tmap) {
      _node_maps.push_back(new _core_bits::MapCopy<From, Node,
                           NodeRefMap, FromMap, ToMap>(map, tmap));
      return *this;
    }

    /// \brief Make a copy of the given node.
    ///
    /// This function makes a copy of the given node.
    BpGraphCopy& node(const Node& node, TNode& tnode) {
      _node_maps.push_back(new _core_bits::ItemCopy<From, Node,
                           NodeRefMap, TNode>(node, tnode));
      return *this;
    }

    /// \brief Copy the red node references into the given map.
    ///
    /// This function copies the red node references into the given
    /// map.  The parameter should be a map, whose key type is the
    /// Node type of the source graph with the red item set, while the
    /// value type is the Node type of the destination graph.
    template <typename RedRef>
    BpGraphCopy& redRef(RedRef& map) {
      _red_maps.push_back(new _core_bits::RefCopy<From, RedNode,
                          RedNodeRefMap, RedRef>(map));
      return *this;
    }

    /// \brief Copy the red node cross references into the given map.
    ///
    /// This function copies the red node cross references (reverse
    /// references) into the given map. The parameter should be a map,
    /// whose key type is the Node type of the destination graph with
    /// the red item set, while the value type is the Node type of the
    /// source graph.
    template <typename RedCrossRef>
    BpGraphCopy& redCrossRef(RedCrossRef& map) {
      _red_maps.push_back(new _core_bits::CrossRefCopy<From, RedNode,
                          RedNodeRefMap, RedCrossRef>(map));
      return *this;
    }

    /// \brief Make a copy of the given red node map.
    ///
    /// This function makes a copy of the given red node map for the newly
    /// created graph.
    /// The key type of the new map \c tmap should be the Node type of
    /// the destination graph with the red items, and the key type of
    /// the original map \c map should be the Node type of the source
    /// graph.
    template <typename FromMap, typename ToMap>
    BpGraphCopy& redNodeMap(const FromMap& map, ToMap& tmap) {
      _red_maps.push_back(new _core_bits::MapCopy<From, RedNode,
                          RedNodeRefMap, FromMap, ToMap>(map, tmap));
      return *this;
    }

    /// \brief Make a copy of the given red node.
    ///
    /// This function makes a copy of the given red node.
    BpGraphCopy& redNode(const RedNode& node, TRedNode& tnode) {
      _red_maps.push_back(new _core_bits::ItemCopy<From, RedNode,
                          RedNodeRefMap, TRedNode>(node, tnode));
      return *this;
    }

    /// \brief Copy the blue node references into the given map.
    ///
    /// This function copies the blue node references into the given
    /// map.  The parameter should be a map, whose key type is the
    /// Node type of the source graph with the blue item set, while the
    /// value type is the Node type of the destination graph.
    template <typename BlueRef>
    BpGraphCopy& blueRef(BlueRef& map) {
      _blue_maps.push_back(new _core_bits::RefCopy<From, BlueNode,
                           BlueNodeRefMap, BlueRef>(map));
      return *this;
    }

    /// \brief Copy the blue node cross references into the given map.
    ///
    /// This function copies the blue node cross references (reverse
    /// references) into the given map. The parameter should be a map,
    /// whose key type is the Node type of the destination graph with
    /// the blue item set, while the value type is the Node type of the
    /// source graph.
    template <typename BlueCrossRef>
    BpGraphCopy& blueCrossRef(BlueCrossRef& map) {
      _blue_maps.push_back(new _core_bits::CrossRefCopy<From, BlueNode,
                           BlueNodeRefMap, BlueCrossRef>(map));
      return *this;
    }

    /// \brief Make a copy of the given blue node map.
    ///
    /// This function makes a copy of the given blue node map for the newly
    /// created graph.
    /// The key type of the new map \c tmap should be the Node type of
    /// the destination graph with the blue items, and the key type of
    /// the original map \c map should be the Node type of the source
    /// graph.
    template <typename FromMap, typename ToMap>
    BpGraphCopy& blueNodeMap(const FromMap& map, ToMap& tmap) {
      _blue_maps.push_back(new _core_bits::MapCopy<From, BlueNode,
                           BlueNodeRefMap, FromMap, ToMap>(map, tmap));
      return *this;
    }

    /// \brief Make a copy of the given blue node.
    ///
    /// This function makes a copy of the given blue node.
    BpGraphCopy& blueNode(const BlueNode& node, TBlueNode& tnode) {
      _blue_maps.push_back(new _core_bits::ItemCopy<From, BlueNode,
                           BlueNodeRefMap, TBlueNode>(node, tnode));
      return *this;
    }

    /// \brief Copy the arc references into the given map.
    ///
    /// This function copies the arc references into the given map.
    /// The parameter should be a map, whose key type is the Arc type of
    /// the source graph, while the value type is the Arc type of the
    /// destination graph.
    template <typename ArcRef>
    BpGraphCopy& arcRef(ArcRef& map) {
      _arc_maps.push_back(new _core_bits::RefCopy<From, Arc,
                          ArcRefMap, ArcRef>(map));
      return *this;
    }

    /// \brief Copy the arc cross references into the given map.
    ///
    /// This function copies the arc cross references (reverse references)
    /// into the given map. The parameter should be a map, whose key type
    /// is the Arc type of the destination graph, while the value type is
    /// the Arc type of the source graph.
    template <typename ArcCrossRef>
    BpGraphCopy& arcCrossRef(ArcCrossRef& map) {
      _arc_maps.push_back(new _core_bits::CrossRefCopy<From, Arc,
                          ArcRefMap, ArcCrossRef>(map));
      return *this;
    }

    /// \brief Make a copy of the given arc map.
    ///
    /// This function makes a copy of the given arc map for the newly
    /// created graph.
    /// The key type of the new map \c tmap should be the Arc type of the
    /// destination graph, and the key type of the original map \c map
    /// should be the Arc type of the source graph.
    template <typename FromMap, typename ToMap>
    BpGraphCopy& arcMap(const FromMap& map, ToMap& tmap) {
      _arc_maps.push_back(new _core_bits::MapCopy<From, Arc,
                          ArcRefMap, FromMap, ToMap>(map, tmap));
      return *this;
    }

    /// \brief Make a copy of the given arc.
    ///
    /// This function makes a copy of the given arc.
    BpGraphCopy& arc(const Arc& arc, TArc& tarc) {
      _arc_maps.push_back(new _core_bits::ItemCopy<From, Arc,
                          ArcRefMap, TArc>(arc, tarc));
      return *this;
    }

    /// \brief Copy the edge references into the given map.
    ///
    /// This function copies the edge references into the given map.
    /// The parameter should be a map, whose key type is the Edge type of
    /// the source graph, while the value type is the Edge type of the
    /// destination graph.
    template <typename EdgeRef>
    BpGraphCopy& edgeRef(EdgeRef& map) {
      _edge_maps.push_back(new _core_bits::RefCopy<From, Edge,
                           EdgeRefMap, EdgeRef>(map));
      return *this;
    }

    /// \brief Copy the edge cross references into the given map.
    ///
    /// This function copies the edge cross references (reverse references)
    /// into the given map. The parameter should be a map, whose key type
    /// is the Edge type of the destination graph, while the value type is
    /// the Edge type of the source graph.
    template <typename EdgeCrossRef>
    BpGraphCopy& edgeCrossRef(EdgeCrossRef& map) {
      _edge_maps.push_back(new _core_bits::CrossRefCopy<From,
                           Edge, EdgeRefMap, EdgeCrossRef>(map));
      return *this;
    }

    /// \brief Make a copy of the given edge map.
    ///
    /// This function makes a copy of the given edge map for the newly
    /// created graph.
    /// The key type of the new map \c tmap should be the Edge type of the
    /// destination graph, and the key type of the original map \c map
    /// should be the Edge type of the source graph.
    template <typename FromMap, typename ToMap>
    BpGraphCopy& edgeMap(const FromMap& map, ToMap& tmap) {
      _edge_maps.push_back(new _core_bits::MapCopy<From, Edge,
                           EdgeRefMap, FromMap, ToMap>(map, tmap));
      return *this;
    }

    /// \brief Make a copy of the given edge.
    ///
    /// This function makes a copy of the given edge.
    BpGraphCopy& edge(const Edge& edge, TEdge& tedge) {
      _edge_maps.push_back(new _core_bits::ItemCopy<From, Edge,
                           EdgeRefMap, TEdge>(edge, tedge));
      return *this;
    }

    /// \brief Execute copying.
    ///
    /// This function executes the copying of the graph along with the
    /// copying of the assigned data.
    void run() {
      RedNodeRefMap redNodeRefMap(_from);
      BlueNodeRefMap blueNodeRefMap(_from);
      NodeRefMap nodeRefMap(_from, redNodeRefMap, blueNodeRefMap);
      EdgeRefMap edgeRefMap(_from);
      ArcRefMap arcRefMap(_from, _to, edgeRefMap);
      _core_bits::BpGraphCopySelector<To>::
        copy(_from, _to, redNodeRefMap, blueNodeRefMap, edgeRefMap);
      for (int i = 0; i < int(_node_maps.size()); ++i) {
        _node_maps[i]->copy(_from, nodeRefMap);
      }
      for (int i = 0; i < int(_red_maps.size()); ++i) {
        _red_maps[i]->copy(_from, redNodeRefMap);
      }
      for (int i = 0; i < int(_blue_maps.size()); ++i) {
        _blue_maps[i]->copy(_from, blueNodeRefMap);
      }
      for (int i = 0; i < int(_edge_maps.size()); ++i) {
        _edge_maps[i]->copy(_from, edgeRefMap);
      }
      for (int i = 0; i < int(_arc_maps.size()); ++i) {
        _arc_maps[i]->copy(_from, arcRefMap);
      }
    }

  private:

    const From& _from;
    To& _to;

    std::vector<_core_bits::MapCopyBase<From, Node, NodeRefMap>* >
      _node_maps;

    std::vector<_core_bits::MapCopyBase<From, RedNode, RedNodeRefMap>* >
      _red_maps;

    std::vector<_core_bits::MapCopyBase<From, BlueNode, BlueNodeRefMap>* >
      _blue_maps;

    std::vector<_core_bits::MapCopyBase<From, Arc, ArcRefMap>* >
      _arc_maps;

    std::vector<_core_bits::MapCopyBase<From, Edge, EdgeRefMap>* >
      _edge_maps;

  };

  /// \brief Copy a graph to another graph.
  ///
  /// This function copies a graph to another graph.
  /// The complete usage of it is detailed in the BpGraphCopy class,
  /// but a short example shows a basic work:
  ///\code
  /// graphCopy(src, trg).nodeRef(nr).edgeCrossRef(ecr).run();
  ///\endcode
  ///
  /// After the copy the \c nr map will contain the mapping from the
  /// nodes of the \c from graph to the nodes of the \c to graph and
  /// \c ecr will contain the mapping from the edges of the \c to graph
  /// to the edges of the \c from graph.
  ///
  /// \see BpGraphCopy
  template <typename From, typename To>
  BpGraphCopy<From, To>
  bpGraphCopy(const From& from, To& to) {
    return BpGraphCopy<From, To>(from, to);
  }

  namespace _core_bits {

    template <typename Graph, typename Enable = void>
    struct FindArcSelector {
      typedef typename Graph::Node Node;
      typedef typename Graph::Arc Arc;
      static Arc find(const Graph &g, Node u, Node v, Arc e) {
        if (e == INVALID) {
          g.firstOut(e, u);
        } else {
          g.nextOut(e);
        }
        while (e != INVALID && g.target(e) != v) {
          g.nextOut(e);
        }
        return e;
      }
    };

    template <typename Graph>
    struct FindArcSelector<
      Graph,
      typename enable_if<typename Graph::FindArcTag, void>::type>
    {
      typedef typename Graph::Node Node;
      typedef typename Graph::Arc Arc;
      static Arc find(const Graph &g, Node u, Node v, Arc prev) {
        return g.findArc(u, v, prev);
      }
    };
  }

  /// \brief Find an arc between two nodes of a digraph.
  ///
  /// This function finds an arc from node \c u to node \c v in the
  /// digraph \c g.
  ///
  /// If \c prev is \ref INVALID (this is the default value), then
  /// it finds the first arc from \c u to \c v. Otherwise it looks for
  /// the next arc from \c u to \c v after \c prev.
  /// \return The found arc or \ref INVALID if there is no such an arc.
  ///
  /// Thus you can iterate through each arc from \c u to \c v as it follows.
  ///\code
  /// for(Arc e = findArc(g,u,v); e != INVALID; e = findArc(g,u,v,e)) {
  ///   ...
  /// }
  ///\endcode
  ///
  /// \note \ref ConArcIt provides iterator interface for the same
  /// functionality.
  ///
  ///\sa ConArcIt
  ///\sa ArcLookUp, AllArcLookUp, DynArcLookUp
  template <typename Graph>
  inline typename Graph::Arc
  findArc(const Graph &g, typename Graph::Node u, typename Graph::Node v,
          typename Graph::Arc prev = INVALID) {
    return _core_bits::FindArcSelector<Graph>::find(g, u, v, prev);
  }

  /// \brief Iterator for iterating on parallel arcs connecting the same nodes.
  ///
  /// Iterator for iterating on parallel arcs connecting the same nodes. It is
  /// a higher level interface for the \ref findArc() function. You can
  /// use it the following way:
  ///\code
  /// for (ConArcIt<Graph> it(g, src, trg); it != INVALID; ++it) {
  ///   ...
  /// }
  ///\endcode
  ///
  ///\sa findArc()
  ///\sa ArcLookUp, AllArcLookUp, DynArcLookUp
  template <typename GR>
  class ConArcIt : public GR::Arc {
    typedef typename GR::Arc Parent;

  public:

    typedef typename GR::Arc Arc;
    typedef typename GR::Node Node;

    /// \brief Constructor.
    ///
    /// Construct a new ConArcIt iterating on the arcs that
    /// connects nodes \c u and \c v.
    ConArcIt(const GR& g, Node u, Node v) : _graph(g) {
      Parent::operator=(findArc(_graph, u, v));
    }

    /// \brief Constructor.
    ///
    /// Construct a new ConArcIt that continues the iterating from arc \c a.
    ConArcIt(const GR& g, Arc a) : Parent(a), _graph(g) {}

    /// \brief Increment operator.
    ///
    /// It increments the iterator and gives back the next arc.
    ConArcIt& operator++() {
      Parent::operator=(findArc(_graph, _graph.source(*this),
                                _graph.target(*this), *this));
      return *this;
    }
  private:
    const GR& _graph;
  };

  namespace _core_bits {

    template <typename Graph, typename Enable = void>
    struct FindEdgeSelector {
      typedef typename Graph::Node Node;
      typedef typename Graph::Edge Edge;
      static Edge find(const Graph &g, Node u, Node v, Edge e) {
        bool b;
        if (u != v) {
          if (e == INVALID) {
            g.firstInc(e, b, u);
          } else {
            b = g.u(e) == u;
            g.nextInc(e, b);
          }
          while (e != INVALID && (b ? g.v(e) : g.u(e)) != v) {
            g.nextInc(e, b);
          }
        } else {
          if (e == INVALID) {
            g.firstInc(e, b, u);
          } else {
            b = true;
            g.nextInc(e, b);
          }
          while (e != INVALID && (!b || g.v(e) != v)) {
            g.nextInc(e, b);
          }
        }
        return e;
      }
    };

    template <typename Graph>
    struct FindEdgeSelector<
      Graph,
      typename enable_if<typename Graph::FindEdgeTag, void>::type>
    {
      typedef typename Graph::Node Node;
      typedef typename Graph::Edge Edge;
      static Edge find(const Graph &g, Node u, Node v, Edge prev) {
        return g.findEdge(u, v, prev);
      }
    };
  }

  /// \brief Find an edge between two nodes of a graph.
  ///
  /// This function finds an edge from node \c u to node \c v in graph \c g.
  /// If node \c u and node \c v is equal then each loop edge
  /// will be enumerated once.
  ///
  /// If \c prev is \ref INVALID (this is the default value), then
  /// it finds the first edge from \c u to \c v. Otherwise it looks for
  /// the next edge from \c u to \c v after \c prev.
  /// \return The found edge or \ref INVALID if there is no such an edge.
  ///
  /// Thus you can iterate through each edge between \c u and \c v
  /// as it follows.
  ///\code
  /// for(Edge e = findEdge(g,u,v); e != INVALID; e = findEdge(g,u,v,e)) {
  ///   ...
  /// }
  ///\endcode
  ///
  /// \note \ref ConEdgeIt provides iterator interface for the same
  /// functionality.
  ///
  ///\sa ConEdgeIt
  template <typename Graph>
  inline typename Graph::Edge
  findEdge(const Graph &g, typename Graph::Node u, typename Graph::Node v,
            typename Graph::Edge p = INVALID) {
    return _core_bits::FindEdgeSelector<Graph>::find(g, u, v, p);
  }

  /// \brief Iterator for iterating on parallel edges connecting the same nodes.
  ///
  /// Iterator for iterating on parallel edges connecting the same nodes.
  /// It is a higher level interface for the findEdge() function. You can
  /// use it the following way:
  ///\code
  /// for (ConEdgeIt<Graph> it(g, u, v); it != INVALID; ++it) {
  ///   ...
  /// }
  ///\endcode
  ///
  ///\sa findEdge()
  template <typename GR>
  class ConEdgeIt : public GR::Edge {
    typedef typename GR::Edge Parent;

  public:

    typedef typename GR::Edge Edge;
    typedef typename GR::Node Node;

    /// \brief Constructor.
    ///
    /// Construct a new ConEdgeIt iterating on the edges that
    /// connects nodes \c u and \c v.
    ConEdgeIt(const GR& g, Node u, Node v) : _graph(g), _u(u), _v(v) {
      Parent::operator=(findEdge(_graph, _u, _v));
    }

    /// \brief Constructor.
    ///
    /// Construct a new ConEdgeIt that continues iterating from edge \c e.
    ConEdgeIt(const GR& g, Edge e) : Parent(e), _graph(g) {}

    /// \brief Increment operator.
    ///
    /// It increments the iterator and gives back the next edge.
    ConEdgeIt& operator++() {
      Parent::operator=(findEdge(_graph, _u, _v, *this));
      return *this;
    }
  private:
    const GR& _graph;
    Node _u, _v;
  };


  ///Dynamic arc look-up between given endpoints.

  ///Using this class, you can find an arc in a digraph from a given
  ///source to a given target in amortized time <em>O</em>(log<em>d</em>),
  ///where <em>d</em> is the out-degree of the source node.
  ///
  ///It is possible to find \e all parallel arcs between two nodes with
  ///the \c operator() member.
  ///
  ///This is a dynamic data structure. Consider to use \ref ArcLookUp or
  ///\ref AllArcLookUp if your digraph is not changed so frequently.
  ///
  ///This class uses a self-adjusting binary search tree, the Splay tree
  ///of Sleator and Tarjan to guarantee the logarithmic amortized
  ///time bound for arc look-ups. This class also guarantees the
  ///optimal time bound in a constant factor for any distribution of
  ///queries.
  ///
  ///\tparam GR The type of the underlying digraph.
  ///
  ///\sa ArcLookUp
  ///\sa AllArcLookUp
  template <typename GR>
  class DynArcLookUp
    : protected ItemSetTraits<GR, typename GR::Arc>::ItemNotifier::ObserverBase
  {
    typedef typename ItemSetTraits<GR, typename GR::Arc>
    ::ItemNotifier::ObserverBase Parent;

    TEMPLATE_DIGRAPH_TYPEDEFS(GR);

  public:

    /// The Digraph type
    typedef GR Digraph;

  protected:

    class AutoNodeMap : public ItemSetTraits<GR, Node>::template Map<Arc>::Type
    {
      typedef typename ItemSetTraits<GR, Node>::template Map<Arc>::Type Parent;

    public:

      AutoNodeMap(const GR& digraph) : Parent(digraph, INVALID) {}

      virtual void add(const Node& node) {
        Parent::add(node);
        Parent::set(node, INVALID);
      }

      virtual void add(const std::vector<Node>& nodes) {
        Parent::add(nodes);
        for (int i = 0; i < int(nodes.size()); ++i) {
          Parent::set(nodes[i], INVALID);
        }
      }

      virtual void build() {
        Parent::build();
        Node it;
        typename Parent::Notifier* nf = Parent::notifier();
        for (nf->first(it); it != INVALID; nf->next(it)) {
          Parent::set(it, INVALID);
        }
      }
    };

    class ArcLess {
      const Digraph &g;
    public:
      ArcLess(const Digraph &_g) : g(_g) {}
      bool operator()(Arc a,Arc b) const
      {
        return g.target(a)<g.target(b);
      }
    };

  protected:

    const Digraph &_g;
    AutoNodeMap _head;
    typename Digraph::template ArcMap<Arc> _parent;
    typename Digraph::template ArcMap<Arc> _left;
    typename Digraph::template ArcMap<Arc> _right;

  public:

    ///Constructor

    ///Constructor.
    ///
    ///It builds up the search database.
    DynArcLookUp(const Digraph &g)
      : _g(g),_head(g),_parent(g),_left(g),_right(g)
    {
      Parent::attach(_g.notifier(typename Digraph::Arc()));
      refresh();
    }

  protected:

    virtual void add(const Arc& arc) {
      insert(arc);
    }

    virtual void add(const std::vector<Arc>& arcs) {
      for (int i = 0; i < int(arcs.size()); ++i) {
        insert(arcs[i]);
      }
    }

    virtual void erase(const Arc& arc) {
      remove(arc);
    }

    virtual void erase(const std::vector<Arc>& arcs) {
      for (int i = 0; i < int(arcs.size()); ++i) {
        remove(arcs[i]);
      }
    }

    virtual void build() {
      refresh();
    }

    virtual void clear() {
      for(NodeIt n(_g);n!=INVALID;++n) {
        _head[n] = INVALID;
      }
    }

    void insert(Arc arc) {
      Node s = _g.source(arc);
      Node t = _g.target(arc);
      _left[arc] = INVALID;
      _right[arc] = INVALID;

      Arc e = _head[s];
      if (e == INVALID) {
        _head[s] = arc;
        _parent[arc] = INVALID;
        return;
      }
      while (true) {
        if (t < _g.target(e)) {
          if (_left[e] == INVALID) {
            _left[e] = arc;
            _parent[arc] = e;
            splay(arc);
            return;
          } else {
            e = _left[e];
          }
        } else {
          if (_right[e] == INVALID) {
            _right[e] = arc;
            _parent[arc] = e;
            splay(arc);
            return;
          } else {
            e = _right[e];
          }
        }
      }
    }

    void remove(Arc arc) {
      if (_left[arc] == INVALID) {
        if (_right[arc] != INVALID) {
          _parent[_right[arc]] = _parent[arc];
        }
        if (_parent[arc] != INVALID) {
          if (_left[_parent[arc]] == arc) {
            _left[_parent[arc]] = _right[arc];
          } else {
            _right[_parent[arc]] = _right[arc];
          }
        } else {
          _head[_g.source(arc)] = _right[arc];
        }
      } else if (_right[arc] == INVALID) {
        _parent[_left[arc]] = _parent[arc];
        if (_parent[arc] != INVALID) {
          if (_left[_parent[arc]] == arc) {
            _left[_parent[arc]] = _left[arc];
          } else {
            _right[_parent[arc]] = _left[arc];
          }
        } else {
          _head[_g.source(arc)] = _left[arc];
        }
      } else {
        Arc e = _left[arc];
        if (_right[e] != INVALID) {
          e = _right[e];
          while (_right[e] != INVALID) {
            e = _right[e];
          }
          Arc s = _parent[e];
          _right[_parent[e]] = _left[e];
          if (_left[e] != INVALID) {
            _parent[_left[e]] = _parent[e];
          }

          _left[e] = _left[arc];
          _parent[_left[arc]] = e;
          _right[e] = _right[arc];
          _parent[_right[arc]] = e;

          _parent[e] = _parent[arc];
          if (_parent[arc] != INVALID) {
            if (_left[_parent[arc]] == arc) {
              _left[_parent[arc]] = e;
            } else {
              _right[_parent[arc]] = e;
            }
          }
          splay(s);
        } else {
          _right[e] = _right[arc];
          _parent[_right[arc]] = e;
          _parent[e] = _parent[arc];

          if (_parent[arc] != INVALID) {
            if (_left[_parent[arc]] == arc) {
              _left[_parent[arc]] = e;
            } else {
              _right[_parent[arc]] = e;
            }
          } else {
            _head[_g.source(arc)] = e;
          }
        }
      }
    }

    Arc refreshRec(std::vector<Arc> &v,int a,int b)
    {
      int m=(a+b)/2;
      Arc me=v[m];
      if (a < m) {
        Arc left = refreshRec(v,a,m-1);
        _left[me] = left;
        _parent[left] = me;
      } else {
        _left[me] = INVALID;
      }
      if (m < b) {
        Arc right = refreshRec(v,m+1,b);
        _right[me] = right;
        _parent[right] = me;
      } else {
        _right[me] = INVALID;
      }
      return me;
    }

    void refresh() {
      for(NodeIt n(_g);n!=INVALID;++n) {
        std::vector<Arc> v;
        for(OutArcIt a(_g,n);a!=INVALID;++a) v.push_back(a);
        if (!v.empty()) {
          std::sort(v.begin(),v.end(),ArcLess(_g));
          Arc head = refreshRec(v,0,v.size()-1);
          _head[n] = head;
          _parent[head] = INVALID;
        }
        else _head[n] = INVALID;
      }
    }

    void zig(Arc v) {
      Arc w = _parent[v];
      _parent[v] = _parent[w];
      _parent[w] = v;
      _left[w] = _right[v];
      _right[v] = w;
      if (_parent[v] != INVALID) {
        if (_right[_parent[v]] == w) {
          _right[_parent[v]] = v;
        } else {
          _left[_parent[v]] = v;
        }
      }
      if (_left[w] != INVALID){
        _parent[_left[w]] = w;
      }
    }

    void zag(Arc v) {
      Arc w = _parent[v];
      _parent[v] = _parent[w];
      _parent[w] = v;
      _right[w] = _left[v];
      _left[v] = w;
      if (_parent[v] != INVALID){
        if (_left[_parent[v]] == w) {
          _left[_parent[v]] = v;
        } else {
          _right[_parent[v]] = v;
        }
      }
      if (_right[w] != INVALID){
        _parent[_right[w]] = w;
      }
    }

    void splay(Arc v) {
      while (_parent[v] != INVALID) {
        if (v == _left[_parent[v]]) {
          if (_parent[_parent[v]] == INVALID) {
            zig(v);
          } else {
            if (_parent[v] == _left[_parent[_parent[v]]]) {
              zig(_parent[v]);
              zig(v);
            } else {
              zig(v);
              zag(v);
            }
          }
        } else {
          if (_parent[_parent[v]] == INVALID) {
            zag(v);
          } else {
            if (_parent[v] == _left[_parent[_parent[v]]]) {
              zag(v);
              zig(v);
            } else {
              zag(_parent[v]);
              zag(v);
            }
          }
        }
      }
      _head[_g.source(v)] = v;
    }


  public:

    ///Find an arc between two nodes.

    ///Find an arc between two nodes.
    ///\param s The source node.
    ///\param t The target node.
    ///\param p The previous arc between \c s and \c t. It it is INVALID or
    ///not given, the operator finds the first appropriate arc.
    ///\return An arc from \c s to \c t after \c p or
    ///\ref INVALID if there is no more.
    ///
    ///For example, you can count the number of arcs from \c u to \c v in the
    ///following way.
    ///\code
    ///DynArcLookUp<ListDigraph> ae(g);
    ///...
    ///int n = 0;
    ///for(Arc a = ae(u,v); a != INVALID; a = ae(u,v,a)) n++;
    ///\endcode
    ///
    ///Finding the arcs take at most <em>O</em>(log<em>d</em>)
    ///amortized time, specifically, the time complexity of the lookups
    ///is equal to the optimal search tree implementation for the
    ///current query distribution in a constant factor.
    ///
    ///\note This is a dynamic data structure, therefore the data
    ///structure is updated after each graph alteration. Thus although
    ///this data structure is theoretically faster than \ref ArcLookUp
    ///and \ref AllArcLookUp, it often provides worse performance than
    ///them.
    Arc operator()(Node s, Node t, Arc p = INVALID) const  {
      if (p == INVALID) {
        Arc a = _head[s];
        if (a == INVALID) return INVALID;
        Arc r = INVALID;
        while (true) {
          if (_g.target(a) < t) {
            if (_right[a] == INVALID) {
              const_cast<DynArcLookUp&>(*this).splay(a);
              return r;
            } else {
              a = _right[a];
            }
          } else {
            if (_g.target(a) == t) {
              r = a;
            }
            if (_left[a] == INVALID) {
              const_cast<DynArcLookUp&>(*this).splay(a);
              return r;
            } else {
              a = _left[a];
            }
          }
        }
      } else {
        Arc a = p;
        if (_right[a] != INVALID) {
          a = _right[a];
          while (_left[a] != INVALID) {
            a = _left[a];
          }
          const_cast<DynArcLookUp&>(*this).splay(a);
        } else {
          while (_parent[a] != INVALID && _right[_parent[a]] ==  a) {
            a = _parent[a];
          }
          if (_parent[a] == INVALID) {
            return INVALID;
          } else {
            a = _parent[a];
            const_cast<DynArcLookUp&>(*this).splay(a);
          }
        }
        if (_g.target(a) == t) return a;
        else return INVALID;
      }
    }

  };

  ///Fast arc look-up between given endpoints.

  ///Using this class, you can find an arc in a digraph from a given
  ///source to a given target in time <em>O</em>(log<em>d</em>),
  ///where <em>d</em> is the out-degree of the source node.
  ///
  ///It is not possible to find \e all parallel arcs between two nodes.
  ///Use \ref AllArcLookUp for this purpose.
  ///
  ///\warning This class is static, so you should call refresh() (or at
  ///least refresh(Node)) to refresh this data structure whenever the
  ///digraph changes. This is a time consuming (superlinearly proportional
  ///(<em>O</em>(<em>m</em> log<em>m</em>)) to the number of arcs).
  ///
  ///\tparam GR The type of the underlying digraph.
  ///
  ///\sa DynArcLookUp
  ///\sa AllArcLookUp
  template<class GR>
  class ArcLookUp
  {
    TEMPLATE_DIGRAPH_TYPEDEFS(GR);

  public:

    /// The Digraph type
    typedef GR Digraph;

  protected:
    const Digraph &_g;
    typename Digraph::template NodeMap<Arc> _head;
    typename Digraph::template ArcMap<Arc> _left;
    typename Digraph::template ArcMap<Arc> _right;

    class ArcLess {
      const Digraph &g;
    public:
      ArcLess(const Digraph &_g) : g(_g) {}
      bool operator()(Arc a,Arc b) const
      {
        return g.target(a)<g.target(b);
      }
    };

  public:

    ///Constructor

    ///Constructor.
    ///
    ///It builds up the search database, which remains valid until the digraph
    ///changes.
    ArcLookUp(const Digraph &g) :_g(g),_head(g),_left(g),_right(g) {refresh();}

  private:
    Arc refreshRec(std::vector<Arc> &v,int a,int b)
    {
      int m=(a+b)/2;
      Arc me=v[m];
      _left[me] = a<m?refreshRec(v,a,m-1):INVALID;
      _right[me] = m<b?refreshRec(v,m+1,b):INVALID;
      return me;
    }
  public:
    ///Refresh the search data structure at a node.

    ///Build up the search database of node \c n.
    ///
    ///It runs in time <em>O</em>(<em>d</em> log<em>d</em>), where <em>d</em>
    ///is the number of the outgoing arcs of \c n.
    void refresh(Node n)
    {
      std::vector<Arc> v;
      for(OutArcIt e(_g,n);e!=INVALID;++e) v.push_back(e);
      if(v.size()) {
        std::sort(v.begin(),v.end(),ArcLess(_g));
        _head[n]=refreshRec(v,0,v.size()-1);
      }
      else _head[n]=INVALID;
    }
    ///Refresh the full data structure.

    ///Build up the full search database. In fact, it simply calls
    ///\ref refresh(Node) "refresh(n)" for each node \c n.
    ///
    ///It runs in time <em>O</em>(<em>m</em> log<em>D</em>), where <em>m</em> is
    ///the number of the arcs in the digraph and <em>D</em> is the maximum
    ///out-degree of the digraph.
    void refresh()
    {
      for(NodeIt n(_g);n!=INVALID;++n) refresh(n);
    }

    ///Find an arc between two nodes.

    ///Find an arc between two nodes in time <em>O</em>(log<em>d</em>),
    ///where <em>d</em> is the number of outgoing arcs of \c s.
    ///\param s The source node.
    ///\param t The target node.
    ///\return An arc from \c s to \c t if there exists,
    ///\ref INVALID otherwise.
    ///
    ///\warning If you change the digraph, refresh() must be called before using
    ///this operator. If you change the outgoing arcs of
    ///a single node \c n, then \ref refresh(Node) "refresh(n)" is enough.
    Arc operator()(Node s, Node t) const
    {
      Arc e;
      for(e=_head[s];
          e!=INVALID&&_g.target(e)!=t;
          e = t < _g.target(e)?_left[e]:_right[e]) ;
      return e;
    }

  };

  ///Fast look-up of all arcs between given endpoints.

  ///This class is the same as \ref ArcLookUp, with the addition
  ///that it makes it possible to find all parallel arcs between given
  ///endpoints.
  ///
  ///\warning This class is static, so you should call refresh() (or at
  ///least refresh(Node)) to refresh this data structure whenever the
  ///digraph changes. This is a time consuming (superlinearly proportional
  ///(<em>O</em>(<em>m</em> log<em>m</em>)) to the number of arcs).
  ///
  ///\tparam GR The type of the underlying digraph.
  ///
  ///\sa DynArcLookUp
  ///\sa ArcLookUp
  template<class GR>
  class AllArcLookUp : public ArcLookUp<GR>
  {
    using ArcLookUp<GR>::_g;
    using ArcLookUp<GR>::_right;
    using ArcLookUp<GR>::_left;
    using ArcLookUp<GR>::_head;

    TEMPLATE_DIGRAPH_TYPEDEFS(GR);

    typename GR::template ArcMap<Arc> _next;

    Arc refreshNext(Arc head,Arc next=INVALID)
    {
      if(head==INVALID) return next;
      else {
        next=refreshNext(_right[head],next);
        _next[head]=( next!=INVALID && _g.target(next)==_g.target(head))
          ? next : INVALID;
        return refreshNext(_left[head],head);
      }
    }

    void refreshNext()
    {
      for(NodeIt n(_g);n!=INVALID;++n) refreshNext(_head[n]);
    }

  public:

    /// The Digraph type
    typedef GR Digraph;

    ///Constructor

    ///Constructor.
    ///
    ///It builds up the search database, which remains valid until the digraph
    ///changes.
    AllArcLookUp(const Digraph &g) : ArcLookUp<GR>(g), _next(g) {refreshNext();}

    ///Refresh the data structure at a node.

    ///Build up the search database of node \c n.
    ///
    ///It runs in time <em>O</em>(<em>d</em> log<em>d</em>), where <em>d</em> is
    ///the number of the outgoing arcs of \c n.
    void refresh(Node n)
    {
      ArcLookUp<GR>::refresh(n);
      refreshNext(_head[n]);
    }

    ///Refresh the full data structure.

    ///Build up the full search database. In fact, it simply calls
    ///\ref refresh(Node) "refresh(n)" for each node \c n.
    ///
    ///It runs in time <em>O</em>(<em>m</em> log<em>D</em>), where <em>m</em> is
    ///the number of the arcs in the digraph and <em>D</em> is the maximum
    ///out-degree of the digraph.
    void refresh()
    {
      for(NodeIt n(_g);n!=INVALID;++n) refresh(_head[n]);
    }

    ///Find an arc between two nodes.

    ///Find an arc between two nodes.
    ///\param s The source node.
    ///\param t The target node.
    ///\param prev The previous arc between \c s and \c t. It it is INVALID or
    ///not given, the operator finds the first appropriate arc.
    ///\return An arc from \c s to \c t after \c prev or
    ///\ref INVALID if there is no more.
    ///
    ///For example, you can count the number of arcs from \c u to \c v in the
    ///following way.
    ///\code
    ///AllArcLookUp<ListDigraph> ae(g);
    ///...
    ///int n = 0;
    ///for(Arc a = ae(u,v); a != INVALID; a=ae(u,v,a)) n++;
    ///\endcode
    ///
    ///Finding the first arc take <em>O</em>(log<em>d</em>) time,
    ///where <em>d</em> is the number of outgoing arcs of \c s. Then the
    ///consecutive arcs are found in constant time.
    ///
    ///\warning If you change the digraph, refresh() must be called before using
    ///this operator. If you change the outgoing arcs of
    ///a single node \c n, then \ref refresh(Node) "refresh(n)" is enough.
    ///
    Arc operator()(Node s, Node t, Arc prev=INVALID) const
    {
      if(prev==INVALID)
        {
          Arc f=INVALID;
          Arc e;
          for(e=_head[s];
              e!=INVALID&&_g.target(e)!=t;
              e = t < _g.target(e)?_left[e]:_right[e]) ;
          while(e!=INVALID)
            if(_g.target(e)==t)
              {
                f = e;
                e = _left[e];
              }
            else e = _right[e];
          return f;
        }
      else return _next[prev];
    }

  };

  /// @}

} //namespace lemon

#endif
