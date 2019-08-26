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

#ifndef LEMON_ADAPTORS_H
#define LEMON_ADAPTORS_H

/// \ingroup graph_adaptors
/// \file
/// \brief Adaptor classes for digraphs and graphs
///
/// This file contains several useful adaptors for digraphs and graphs.

#include <lemon/core.h>
#include <lemon/maps.h>
#include <lemon/bits/variant.h>

#include <lemon/bits/graph_adaptor_extender.h>
#include <lemon/bits/map_extender.h>
#include <lemon/tolerance.h>

#include <algorithm>

namespace lemon {

#ifdef _MSC_VER
#define LEMON_SCOPE_FIX(OUTER, NESTED) OUTER::NESTED
#else
#define LEMON_SCOPE_FIX(OUTER, NESTED) typename OUTER::template NESTED
#endif

  template<typename DGR>
  class DigraphAdaptorBase {
  public:
    typedef DGR Digraph;
    typedef DigraphAdaptorBase Adaptor;

  protected:
    DGR* _digraph;
    DigraphAdaptorBase() : _digraph(0) { }
    void initialize(DGR& digraph) { _digraph = &digraph; }

  public:
    DigraphAdaptorBase(DGR& digraph) : _digraph(&digraph) { }

    typedef typename DGR::Node Node;
    typedef typename DGR::Arc Arc;

    void first(Node& i) const { _digraph->first(i); }
    void first(Arc& i) const { _digraph->first(i); }
    void firstIn(Arc& i, const Node& n) const { _digraph->firstIn(i, n); }
    void firstOut(Arc& i, const Node& n ) const { _digraph->firstOut(i, n); }

    void next(Node& i) const { _digraph->next(i); }
    void next(Arc& i) const { _digraph->next(i); }
    void nextIn(Arc& i) const { _digraph->nextIn(i); }
    void nextOut(Arc& i) const { _digraph->nextOut(i); }

    Node source(const Arc& a) const { return _digraph->source(a); }
    Node target(const Arc& a) const { return _digraph->target(a); }

    typedef NodeNumTagIndicator<DGR> NodeNumTag;
    int nodeNum() const { return _digraph->nodeNum(); }

    typedef ArcNumTagIndicator<DGR> ArcNumTag;
    int arcNum() const { return _digraph->arcNum(); }

    typedef FindArcTagIndicator<DGR> FindArcTag;
    Arc findArc(const Node& u, const Node& v, const Arc& prev = INVALID) const {
      return _digraph->findArc(u, v, prev);
    }

    Node addNode() { return _digraph->addNode(); }
    Arc addArc(const Node& u, const Node& v) { return _digraph->addArc(u, v); }

    void erase(const Node& n) { _digraph->erase(n); }
    void erase(const Arc& a) { _digraph->erase(a); }

    void clear() { _digraph->clear(); }

    int id(const Node& n) const { return _digraph->id(n); }
    int id(const Arc& a) const { return _digraph->id(a); }

    Node nodeFromId(int ix) const { return _digraph->nodeFromId(ix); }
    Arc arcFromId(int ix) const { return _digraph->arcFromId(ix); }

    int maxNodeId() const { return _digraph->maxNodeId(); }
    int maxArcId() const { return _digraph->maxArcId(); }

    typedef typename ItemSetTraits<DGR, Node>::ItemNotifier NodeNotifier;
    NodeNotifier& notifier(Node) const { return _digraph->notifier(Node()); }

    typedef typename ItemSetTraits<DGR, Arc>::ItemNotifier ArcNotifier;
    ArcNotifier& notifier(Arc) const { return _digraph->notifier(Arc()); }

    template <typename V>
    class NodeMap : public DGR::template NodeMap<V> {
      typedef typename DGR::template NodeMap<V> Parent;

    public:
      explicit NodeMap(const Adaptor& adaptor)
        : Parent(*adaptor._digraph) {}
      NodeMap(const Adaptor& adaptor, const V& value)
        : Parent(*adaptor._digraph, value) { }

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

    template <typename V>
    class ArcMap : public DGR::template ArcMap<V> {
      typedef typename DGR::template ArcMap<V> Parent;

    public:
      explicit ArcMap(const DigraphAdaptorBase<DGR>& adaptor)
        : Parent(*adaptor._digraph) {}
      ArcMap(const DigraphAdaptorBase<DGR>& adaptor, const V& value)
        : Parent(*adaptor._digraph, value) {}

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

  };

  template<typename GR>
  class GraphAdaptorBase {
  public:
    typedef GR Graph;

  protected:
    GR* _graph;

    GraphAdaptorBase() : _graph(0) {}

    void initialize(GR& graph) { _graph = &graph; }

  public:
    GraphAdaptorBase(GR& graph) : _graph(&graph) {}

    typedef typename GR::Node Node;
    typedef typename GR::Arc Arc;
    typedef typename GR::Edge Edge;

    void first(Node& i) const { _graph->first(i); }
    void first(Arc& i) const { _graph->first(i); }
    void first(Edge& i) const { _graph->first(i); }
    void firstIn(Arc& i, const Node& n) const { _graph->firstIn(i, n); }
    void firstOut(Arc& i, const Node& n ) const { _graph->firstOut(i, n); }
    void firstInc(Edge &i, bool &d, const Node &n) const {
      _graph->firstInc(i, d, n);
    }

    void next(Node& i) const { _graph->next(i); }
    void next(Arc& i) const { _graph->next(i); }
    void next(Edge& i) const { _graph->next(i); }
    void nextIn(Arc& i) const { _graph->nextIn(i); }
    void nextOut(Arc& i) const { _graph->nextOut(i); }
    void nextInc(Edge &i, bool &d) const { _graph->nextInc(i, d); }

    Node u(const Edge& e) const { return _graph->u(e); }
    Node v(const Edge& e) const { return _graph->v(e); }

    Node source(const Arc& a) const { return _graph->source(a); }
    Node target(const Arc& a) const { return _graph->target(a); }

    typedef NodeNumTagIndicator<Graph> NodeNumTag;
    int nodeNum() const { return _graph->nodeNum(); }

    typedef ArcNumTagIndicator<Graph> ArcNumTag;
    int arcNum() const { return _graph->arcNum(); }

    typedef EdgeNumTagIndicator<Graph> EdgeNumTag;
    int edgeNum() const { return _graph->edgeNum(); }

    typedef FindArcTagIndicator<Graph> FindArcTag;
    Arc findArc(const Node& u, const Node& v,
                const Arc& prev = INVALID) const {
      return _graph->findArc(u, v, prev);
    }

    typedef FindEdgeTagIndicator<Graph> FindEdgeTag;
    Edge findEdge(const Node& u, const Node& v,
                  const Edge& prev = INVALID) const {
      return _graph->findEdge(u, v, prev);
    }

    Node addNode() { return _graph->addNode(); }
    Edge addEdge(const Node& u, const Node& v) { return _graph->addEdge(u, v); }

    void erase(const Node& i) { _graph->erase(i); }
    void erase(const Edge& i) { _graph->erase(i); }

    void clear() { _graph->clear(); }

    bool direction(const Arc& a) const { return _graph->direction(a); }
    Arc direct(const Edge& e, bool d) const { return _graph->direct(e, d); }

    int id(const Node& v) const { return _graph->id(v); }
    int id(const Arc& a) const { return _graph->id(a); }
    int id(const Edge& e) const { return _graph->id(e); }

    Node nodeFromId(int ix) const { return _graph->nodeFromId(ix); }
    Arc arcFromId(int ix) const { return _graph->arcFromId(ix); }
    Edge edgeFromId(int ix) const { return _graph->edgeFromId(ix); }

    int maxNodeId() const { return _graph->maxNodeId(); }
    int maxArcId() const { return _graph->maxArcId(); }
    int maxEdgeId() const { return _graph->maxEdgeId(); }

    typedef typename ItemSetTraits<GR, Node>::ItemNotifier NodeNotifier;
    NodeNotifier& notifier(Node) const { return _graph->notifier(Node()); }

    typedef typename ItemSetTraits<GR, Arc>::ItemNotifier ArcNotifier;
    ArcNotifier& notifier(Arc) const { return _graph->notifier(Arc()); }

    typedef typename ItemSetTraits<GR, Edge>::ItemNotifier EdgeNotifier;
    EdgeNotifier& notifier(Edge) const { return _graph->notifier(Edge()); }

    template <typename V>
    class NodeMap : public GR::template NodeMap<V> {
      typedef typename GR::template NodeMap<V> Parent;

    public:
      explicit NodeMap(const GraphAdaptorBase<GR>& adapter)
        : Parent(*adapter._graph) {}
      NodeMap(const GraphAdaptorBase<GR>& adapter, const V& value)
        : Parent(*adapter._graph, value) {}

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

    template <typename V>
    class ArcMap : public GR::template ArcMap<V> {
      typedef typename GR::template ArcMap<V> Parent;

    public:
      explicit ArcMap(const GraphAdaptorBase<GR>& adapter)
        : Parent(*adapter._graph) {}
      ArcMap(const GraphAdaptorBase<GR>& adapter, const V& value)
        : Parent(*adapter._graph, value) {}

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

    template <typename V>
    class EdgeMap : public GR::template EdgeMap<V> {
      typedef typename GR::template EdgeMap<V> Parent;

    public:
      explicit EdgeMap(const GraphAdaptorBase<GR>& adapter)
        : Parent(*adapter._graph) {}
      EdgeMap(const GraphAdaptorBase<GR>& adapter, const V& value)
        : Parent(*adapter._graph, value) {}

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

  };

  template <typename DGR>
  class ReverseDigraphBase : public DigraphAdaptorBase<DGR> {
    typedef DigraphAdaptorBase<DGR> Parent;
  public:
    typedef DGR Digraph;
  protected:
    ReverseDigraphBase() : Parent() { }
  public:
    typedef typename Parent::Node Node;
    typedef typename Parent::Arc Arc;

    void firstIn(Arc& a, const Node& n) const { Parent::firstOut(a, n); }
    void firstOut(Arc& a, const Node& n ) const { Parent::firstIn(a, n); }

    void nextIn(Arc& a) const { Parent::nextOut(a); }
    void nextOut(Arc& a) const { Parent::nextIn(a); }

    Node source(const Arc& a) const { return Parent::target(a); }
    Node target(const Arc& a) const { return Parent::source(a); }

    Arc addArc(const Node& u, const Node& v) { return Parent::addArc(v, u); }

    typedef FindArcTagIndicator<DGR> FindArcTag;
    Arc findArc(const Node& u, const Node& v,
                const Arc& prev = INVALID) const {
      return Parent::findArc(v, u, prev);
    }

  };

  /// \ingroup graph_adaptors
  ///
  /// \brief Adaptor class for reversing the orientation of the arcs in
  /// a digraph.
  ///
  /// ReverseDigraph can be used for reversing the arcs in a digraph.
  /// It conforms to the \ref concepts::Digraph "Digraph" concept.
  ///
  /// The adapted digraph can also be modified through this adaptor
  /// by adding or removing nodes or arcs, unless the \c GR template
  /// parameter is set to be \c const.
  ///
  /// This class provides item counting in the same time as the adapted
  /// digraph structure.
  ///
  /// \tparam DGR The type of the adapted digraph.
  /// It must conform to the \ref concepts::Digraph "Digraph" concept.
  /// It can also be specified to be \c const.
  ///
  /// \note The \c Node and \c Arc types of this adaptor and the adapted
  /// digraph are convertible to each other.
  template<typename DGR>
#ifdef DOXYGEN
  class ReverseDigraph {
#else
  class ReverseDigraph :
    public DigraphAdaptorExtender<ReverseDigraphBase<DGR> > {
#endif
    typedef DigraphAdaptorExtender<ReverseDigraphBase<DGR> > Parent;
  public:
    /// The type of the adapted digraph.
    typedef DGR Digraph;
  protected:
    ReverseDigraph() { }
  public:

    /// \brief Constructor
    ///
    /// Creates a reverse digraph adaptor for the given digraph.
    explicit ReverseDigraph(DGR& digraph) {
      Parent::initialize(digraph);
    }
  };

  /// \brief Returns a read-only ReverseDigraph adaptor
  ///
  /// This function just returns a read-only \ref ReverseDigraph adaptor.
  /// \ingroup graph_adaptors
  /// \relates ReverseDigraph
  template<typename DGR>
  ReverseDigraph<const DGR> reverseDigraph(const DGR& digraph) {
    return ReverseDigraph<const DGR>(digraph);
  }


  template <typename DGR, typename NF, typename AF, bool ch = true>
  class SubDigraphBase : public DigraphAdaptorBase<DGR> {
    typedef DigraphAdaptorBase<DGR> Parent;
  public:
    typedef DGR Digraph;
    typedef NF NodeFilterMap;
    typedef AF ArcFilterMap;

    typedef SubDigraphBase Adaptor;
  protected:
    NF* _node_filter;
    AF* _arc_filter;
    SubDigraphBase()
      : Parent(), _node_filter(0), _arc_filter(0) { }

    void initialize(DGR& digraph, NF& node_filter, AF& arc_filter) {
      Parent::initialize(digraph);
      _node_filter = &node_filter;
      _arc_filter = &arc_filter;
    }

  public:

    typedef typename Parent::Node Node;
    typedef typename Parent::Arc Arc;

    void first(Node& i) const {
      Parent::first(i);
      while (i != INVALID && !(*_node_filter)[i]) Parent::next(i);
    }

    void first(Arc& i) const {
      Parent::first(i);
      while (i != INVALID && (!(*_arc_filter)[i]
                              || !(*_node_filter)[Parent::source(i)]
                              || !(*_node_filter)[Parent::target(i)]))
        Parent::next(i);
    }

    void firstIn(Arc& i, const Node& n) const {
      Parent::firstIn(i, n);
      while (i != INVALID && (!(*_arc_filter)[i]
                              || !(*_node_filter)[Parent::source(i)]))
        Parent::nextIn(i);
    }

    void firstOut(Arc& i, const Node& n) const {
      Parent::firstOut(i, n);
      while (i != INVALID && (!(*_arc_filter)[i]
                              || !(*_node_filter)[Parent::target(i)]))
        Parent::nextOut(i);
    }

    void next(Node& i) const {
      Parent::next(i);
      while (i != INVALID && !(*_node_filter)[i]) Parent::next(i);
    }

    void next(Arc& i) const {
      Parent::next(i);
      while (i != INVALID && (!(*_arc_filter)[i]
                              || !(*_node_filter)[Parent::source(i)]
                              || !(*_node_filter)[Parent::target(i)]))
        Parent::next(i);
    }

    void nextIn(Arc& i) const {
      Parent::nextIn(i);
      while (i != INVALID && (!(*_arc_filter)[i]
                              || !(*_node_filter)[Parent::source(i)]))
        Parent::nextIn(i);
    }

    void nextOut(Arc& i) const {
      Parent::nextOut(i);
      while (i != INVALID && (!(*_arc_filter)[i]
                              || !(*_node_filter)[Parent::target(i)]))
        Parent::nextOut(i);
    }

    void status(const Node& n, bool v) const { _node_filter->set(n, v); }
    void status(const Arc& a, bool v) const { _arc_filter->set(a, v); }

    bool status(const Node& n) const { return (*_node_filter)[n]; }
    bool status(const Arc& a) const { return (*_arc_filter)[a]; }

    typedef False NodeNumTag;
    typedef False ArcNumTag;

    typedef FindArcTagIndicator<DGR> FindArcTag;
    Arc findArc(const Node& source, const Node& target,
                const Arc& prev = INVALID) const {
      if (!(*_node_filter)[source] || !(*_node_filter)[target]) {
        return INVALID;
      }
      Arc arc = Parent::findArc(source, target, prev);
      while (arc != INVALID && !(*_arc_filter)[arc]) {
        arc = Parent::findArc(source, target, arc);
      }
      return arc;
    }

  public:

    template <typename V>
    class NodeMap
      : public SubMapExtender<SubDigraphBase<DGR, NF, AF, ch>,
              LEMON_SCOPE_FIX(DigraphAdaptorBase<DGR>, NodeMap<V>)> {
      typedef SubMapExtender<SubDigraphBase<DGR, NF, AF, ch>,
        LEMON_SCOPE_FIX(DigraphAdaptorBase<DGR>, NodeMap<V>)> Parent;

    public:
      typedef V Value;

      NodeMap(const SubDigraphBase<DGR, NF, AF, ch>& adaptor)
        : Parent(adaptor) {}
      NodeMap(const SubDigraphBase<DGR, NF, AF, ch>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

    template <typename V>
    class ArcMap
      : public SubMapExtender<SubDigraphBase<DGR, NF, AF, ch>,
              LEMON_SCOPE_FIX(DigraphAdaptorBase<DGR>, ArcMap<V>)> {
      typedef SubMapExtender<SubDigraphBase<DGR, NF, AF, ch>,
        LEMON_SCOPE_FIX(DigraphAdaptorBase<DGR>, ArcMap<V>)> Parent;

    public:
      typedef V Value;

      ArcMap(const SubDigraphBase<DGR, NF, AF, ch>& adaptor)
        : Parent(adaptor) {}
      ArcMap(const SubDigraphBase<DGR, NF, AF, ch>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

  };

  template <typename DGR, typename NF, typename AF>
  class SubDigraphBase<DGR, NF, AF, false>
    : public DigraphAdaptorBase<DGR> {
    typedef DigraphAdaptorBase<DGR> Parent;
  public:
    typedef DGR Digraph;
    typedef NF NodeFilterMap;
    typedef AF ArcFilterMap;

    typedef SubDigraphBase Adaptor;
  protected:
    NF* _node_filter;
    AF* _arc_filter;
    SubDigraphBase()
      : Parent(), _node_filter(0), _arc_filter(0) { }

    void initialize(DGR& digraph, NF& node_filter, AF& arc_filter) {
      Parent::initialize(digraph);
      _node_filter = &node_filter;
      _arc_filter = &arc_filter;
    }

  public:

    typedef typename Parent::Node Node;
    typedef typename Parent::Arc Arc;

    void first(Node& i) const {
      Parent::first(i);
      while (i!=INVALID && !(*_node_filter)[i]) Parent::next(i);
    }

    void first(Arc& i) const {
      Parent::first(i);
      while (i!=INVALID && !(*_arc_filter)[i]) Parent::next(i);
    }

    void firstIn(Arc& i, const Node& n) const {
      Parent::firstIn(i, n);
      while (i!=INVALID && !(*_arc_filter)[i]) Parent::nextIn(i);
    }

    void firstOut(Arc& i, const Node& n) const {
      Parent::firstOut(i, n);
      while (i!=INVALID && !(*_arc_filter)[i]) Parent::nextOut(i);
    }

    void next(Node& i) const {
      Parent::next(i);
      while (i!=INVALID && !(*_node_filter)[i]) Parent::next(i);
    }
    void next(Arc& i) const {
      Parent::next(i);
      while (i!=INVALID && !(*_arc_filter)[i]) Parent::next(i);
    }
    void nextIn(Arc& i) const {
      Parent::nextIn(i);
      while (i!=INVALID && !(*_arc_filter)[i]) Parent::nextIn(i);
    }

    void nextOut(Arc& i) const {
      Parent::nextOut(i);
      while (i!=INVALID && !(*_arc_filter)[i]) Parent::nextOut(i);
    }

    void status(const Node& n, bool v) const { _node_filter->set(n, v); }
    void status(const Arc& a, bool v) const { _arc_filter->set(a, v); }

    bool status(const Node& n) const { return (*_node_filter)[n]; }
    bool status(const Arc& a) const { return (*_arc_filter)[a]; }

    typedef False NodeNumTag;
    typedef False ArcNumTag;

    typedef FindArcTagIndicator<DGR> FindArcTag;
    Arc findArc(const Node& source, const Node& target,
                const Arc& prev = INVALID) const {
      if (!(*_node_filter)[source] || !(*_node_filter)[target]) {
        return INVALID;
      }
      Arc arc = Parent::findArc(source, target, prev);
      while (arc != INVALID && !(*_arc_filter)[arc]) {
        arc = Parent::findArc(source, target, arc);
      }
      return arc;
    }

    template <typename V>
    class NodeMap
      : public SubMapExtender<SubDigraphBase<DGR, NF, AF, false>,
          LEMON_SCOPE_FIX(DigraphAdaptorBase<DGR>, NodeMap<V>)> {
      typedef SubMapExtender<SubDigraphBase<DGR, NF, AF, false>,
        LEMON_SCOPE_FIX(DigraphAdaptorBase<DGR>, NodeMap<V>)> Parent;

    public:
      typedef V Value;

      NodeMap(const SubDigraphBase<DGR, NF, AF, false>& adaptor)
        : Parent(adaptor) {}
      NodeMap(const SubDigraphBase<DGR, NF, AF, false>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

    template <typename V>
    class ArcMap
      : public SubMapExtender<SubDigraphBase<DGR, NF, AF, false>,
          LEMON_SCOPE_FIX(DigraphAdaptorBase<DGR>, ArcMap<V>)> {
      typedef SubMapExtender<SubDigraphBase<DGR, NF, AF, false>,
        LEMON_SCOPE_FIX(DigraphAdaptorBase<DGR>, ArcMap<V>)> Parent;

    public:
      typedef V Value;

      ArcMap(const SubDigraphBase<DGR, NF, AF, false>& adaptor)
        : Parent(adaptor) {}
      ArcMap(const SubDigraphBase<DGR, NF, AF, false>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

  };

  /// \ingroup graph_adaptors
  ///
  /// \brief Adaptor class for hiding nodes and arcs in a digraph
  ///
  /// SubDigraph can be used for hiding nodes and arcs in a digraph.
  /// A \c bool node map and a \c bool arc map must be specified, which
  /// define the filters for nodes and arcs.
  /// Only the nodes and arcs with \c true filter value are
  /// shown in the subdigraph. The arcs that are incident to hidden
  /// nodes are also filtered out.
  /// This adaptor conforms to the \ref concepts::Digraph "Digraph" concept.
  ///
  /// The adapted digraph can also be modified through this adaptor
  /// by adding or removing nodes or arcs, unless the \c GR template
  /// parameter is set to be \c const.
  ///
  /// This class provides only linear time counting for nodes and arcs.
  ///
  /// \tparam DGR The type of the adapted digraph.
  /// It must conform to the \ref concepts::Digraph "Digraph" concept.
  /// It can also be specified to be \c const.
  /// \tparam NF The type of the node filter map.
  /// It must be a \c bool (or convertible) node map of the
  /// adapted digraph. The default type is
  /// \ref concepts::Digraph::NodeMap "DGR::NodeMap<bool>".
  /// \tparam AF The type of the arc filter map.
  /// It must be \c bool (or convertible) arc map of the
  /// adapted digraph. The default type is
  /// \ref concepts::Digraph::ArcMap "DGR::ArcMap<bool>".
  ///
  /// \note The \c Node and \c Arc types of this adaptor and the adapted
  /// digraph are convertible to each other.
  ///
  /// \see FilterNodes
  /// \see FilterArcs
#ifdef DOXYGEN
  template<typename DGR, typename NF, typename AF>
  class SubDigraph {
#else
  template<typename DGR,
           typename NF = typename DGR::template NodeMap<bool>,
           typename AF = typename DGR::template ArcMap<bool> >
  class SubDigraph :
    public DigraphAdaptorExtender<SubDigraphBase<DGR, NF, AF, true> > {
#endif
  public:
    /// The type of the adapted digraph.
    typedef DGR Digraph;
    /// The type of the node filter map.
    typedef NF NodeFilterMap;
    /// The type of the arc filter map.
    typedef AF ArcFilterMap;

    typedef DigraphAdaptorExtender<SubDigraphBase<DGR, NF, AF, true> >
      Parent;

    typedef typename Parent::Node Node;
    typedef typename Parent::Arc Arc;

  protected:
    SubDigraph() { }
  public:

    /// \brief Constructor
    ///
    /// Creates a subdigraph for the given digraph with the
    /// given node and arc filter maps.
    SubDigraph(DGR& digraph, NF& node_filter, AF& arc_filter) {
      Parent::initialize(digraph, node_filter, arc_filter);
    }

    /// \brief Sets the status of the given node
    ///
    /// This function sets the status of the given node.
    /// It is done by simply setting the assigned value of \c n
    /// to \c v in the node filter map.
    void status(const Node& n, bool v) const { Parent::status(n, v); }

    /// \brief Sets the status of the given arc
    ///
    /// This function sets the status of the given arc.
    /// It is done by simply setting the assigned value of \c a
    /// to \c v in the arc filter map.
    void status(const Arc& a, bool v) const { Parent::status(a, v); }

    /// \brief Returns the status of the given node
    ///
    /// This function returns the status of the given node.
    /// It is \c true if the given node is enabled (i.e. not hidden).
    bool status(const Node& n) const { return Parent::status(n); }

    /// \brief Returns the status of the given arc
    ///
    /// This function returns the status of the given arc.
    /// It is \c true if the given arc is enabled (i.e. not hidden).
    bool status(const Arc& a) const { return Parent::status(a); }

    /// \brief Disables the given node
    ///
    /// This function disables the given node in the subdigraph,
    /// so the iteration jumps over it.
    /// It is the same as \ref status() "status(n, false)".
    void disable(const Node& n) const { Parent::status(n, false); }

    /// \brief Disables the given arc
    ///
    /// This function disables the given arc in the subdigraph,
    /// so the iteration jumps over it.
    /// It is the same as \ref status() "status(a, false)".
    void disable(const Arc& a) const { Parent::status(a, false); }

    /// \brief Enables the given node
    ///
    /// This function enables the given node in the subdigraph.
    /// It is the same as \ref status() "status(n, true)".
    void enable(const Node& n) const { Parent::status(n, true); }

    /// \brief Enables the given arc
    ///
    /// This function enables the given arc in the subdigraph.
    /// It is the same as \ref status() "status(a, true)".
    void enable(const Arc& a) const { Parent::status(a, true); }

  };

  /// \brief Returns a read-only SubDigraph adaptor
  ///
  /// This function just returns a read-only \ref SubDigraph adaptor.
  /// \ingroup graph_adaptors
  /// \relates SubDigraph
  template<typename DGR, typename NF, typename AF>
  SubDigraph<const DGR, NF, AF>
  subDigraph(const DGR& digraph,
             NF& node_filter, AF& arc_filter) {
    return SubDigraph<const DGR, NF, AF>
      (digraph, node_filter, arc_filter);
  }

  template<typename DGR, typename NF, typename AF>
  SubDigraph<const DGR, const NF, AF>
  subDigraph(const DGR& digraph,
             const NF& node_filter, AF& arc_filter) {
    return SubDigraph<const DGR, const NF, AF>
      (digraph, node_filter, arc_filter);
  }

  template<typename DGR, typename NF, typename AF>
  SubDigraph<const DGR, NF, const AF>
  subDigraph(const DGR& digraph,
             NF& node_filter, const AF& arc_filter) {
    return SubDigraph<const DGR, NF, const AF>
      (digraph, node_filter, arc_filter);
  }

  template<typename DGR, typename NF, typename AF>
  SubDigraph<const DGR, const NF, const AF>
  subDigraph(const DGR& digraph,
             const NF& node_filter, const AF& arc_filter) {
    return SubDigraph<const DGR, const NF, const AF>
      (digraph, node_filter, arc_filter);
  }


  template <typename GR, typename NF, typename EF, bool ch = true>
  class SubGraphBase : public GraphAdaptorBase<GR> {
    typedef GraphAdaptorBase<GR> Parent;
  public:
    typedef GR Graph;
    typedef NF NodeFilterMap;
    typedef EF EdgeFilterMap;

    typedef SubGraphBase Adaptor;
  protected:

    NF* _node_filter;
    EF* _edge_filter;

    SubGraphBase()
      : Parent(), _node_filter(0), _edge_filter(0) { }

    void initialize(GR& graph, NF& node_filter, EF& edge_filter) {
      Parent::initialize(graph);
      _node_filter = &node_filter;
      _edge_filter = &edge_filter;
    }

  public:

    typedef typename Parent::Node Node;
    typedef typename Parent::Arc Arc;
    typedef typename Parent::Edge Edge;

    void first(Node& i) const {
      Parent::first(i);
      while (i!=INVALID && !(*_node_filter)[i]) Parent::next(i);
    }

    void first(Arc& i) const {
      Parent::first(i);
      while (i!=INVALID && (!(*_edge_filter)[i]
                            || !(*_node_filter)[Parent::source(i)]
                            || !(*_node_filter)[Parent::target(i)]))
        Parent::next(i);
    }

    void first(Edge& i) const {
      Parent::first(i);
      while (i!=INVALID && (!(*_edge_filter)[i]
                            || !(*_node_filter)[Parent::u(i)]
                            || !(*_node_filter)[Parent::v(i)]))
        Parent::next(i);
    }

    void firstIn(Arc& i, const Node& n) const {
      Parent::firstIn(i, n);
      while (i!=INVALID && (!(*_edge_filter)[i]
                            || !(*_node_filter)[Parent::source(i)]))
        Parent::nextIn(i);
    }

    void firstOut(Arc& i, const Node& n) const {
      Parent::firstOut(i, n);
      while (i!=INVALID && (!(*_edge_filter)[i]
                            || !(*_node_filter)[Parent::target(i)]))
        Parent::nextOut(i);
    }

    void firstInc(Edge& i, bool& d, const Node& n) const {
      Parent::firstInc(i, d, n);
      while (i!=INVALID && (!(*_edge_filter)[i]
                            || !(*_node_filter)[Parent::u(i)]
                            || !(*_node_filter)[Parent::v(i)]))
        Parent::nextInc(i, d);
    }

    void next(Node& i) const {
      Parent::next(i);
      while (i!=INVALID && !(*_node_filter)[i]) Parent::next(i);
    }

    void next(Arc& i) const {
      Parent::next(i);
      while (i!=INVALID && (!(*_edge_filter)[i]
                            || !(*_node_filter)[Parent::source(i)]
                            || !(*_node_filter)[Parent::target(i)]))
        Parent::next(i);
    }

    void next(Edge& i) const {
      Parent::next(i);
      while (i!=INVALID && (!(*_edge_filter)[i]
                            || !(*_node_filter)[Parent::u(i)]
                            || !(*_node_filter)[Parent::v(i)]))
        Parent::next(i);
    }

    void nextIn(Arc& i) const {
      Parent::nextIn(i);
      while (i!=INVALID && (!(*_edge_filter)[i]
                            || !(*_node_filter)[Parent::source(i)]))
        Parent::nextIn(i);
    }

    void nextOut(Arc& i) const {
      Parent::nextOut(i);
      while (i!=INVALID && (!(*_edge_filter)[i]
                            || !(*_node_filter)[Parent::target(i)]))
        Parent::nextOut(i);
    }

    void nextInc(Edge& i, bool& d) const {
      Parent::nextInc(i, d);
      while (i!=INVALID && (!(*_edge_filter)[i]
                            || !(*_node_filter)[Parent::u(i)]
                            || !(*_node_filter)[Parent::v(i)]))
        Parent::nextInc(i, d);
    }

    void status(const Node& n, bool v) const { _node_filter->set(n, v); }
    void status(const Edge& e, bool v) const { _edge_filter->set(e, v); }

    bool status(const Node& n) const { return (*_node_filter)[n]; }
    bool status(const Edge& e) const { return (*_edge_filter)[e]; }

    typedef False NodeNumTag;
    typedef False ArcNumTag;
    typedef False EdgeNumTag;

    typedef FindArcTagIndicator<Graph> FindArcTag;
    Arc findArc(const Node& u, const Node& v,
                const Arc& prev = INVALID) const {
      if (!(*_node_filter)[u] || !(*_node_filter)[v]) {
        return INVALID;
      }
      Arc arc = Parent::findArc(u, v, prev);
      while (arc != INVALID && !(*_edge_filter)[arc]) {
        arc = Parent::findArc(u, v, arc);
      }
      return arc;
    }

    typedef FindEdgeTagIndicator<Graph> FindEdgeTag;
    Edge findEdge(const Node& u, const Node& v,
                  const Edge& prev = INVALID) const {
      if (!(*_node_filter)[u] || !(*_node_filter)[v]) {
        return INVALID;
      }
      Edge edge = Parent::findEdge(u, v, prev);
      while (edge != INVALID && !(*_edge_filter)[edge]) {
        edge = Parent::findEdge(u, v, edge);
      }
      return edge;
    }

    template <typename V>
    class NodeMap
      : public SubMapExtender<SubGraphBase<GR, NF, EF, ch>,
          LEMON_SCOPE_FIX(GraphAdaptorBase<GR>, NodeMap<V>)> {
      typedef SubMapExtender<SubGraphBase<GR, NF, EF, ch>,
        LEMON_SCOPE_FIX(GraphAdaptorBase<GR>, NodeMap<V>)> Parent;

    public:
      typedef V Value;

      NodeMap(const SubGraphBase<GR, NF, EF, ch>& adaptor)
        : Parent(adaptor) {}
      NodeMap(const SubGraphBase<GR, NF, EF, ch>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

    template <typename V>
    class ArcMap
      : public SubMapExtender<SubGraphBase<GR, NF, EF, ch>,
          LEMON_SCOPE_FIX(GraphAdaptorBase<GR>, ArcMap<V>)> {
      typedef SubMapExtender<SubGraphBase<GR, NF, EF, ch>,
        LEMON_SCOPE_FIX(GraphAdaptorBase<GR>, ArcMap<V>)> Parent;

    public:
      typedef V Value;

      ArcMap(const SubGraphBase<GR, NF, EF, ch>& adaptor)
        : Parent(adaptor) {}
      ArcMap(const SubGraphBase<GR, NF, EF, ch>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

    template <typename V>
    class EdgeMap
      : public SubMapExtender<SubGraphBase<GR, NF, EF, ch>,
        LEMON_SCOPE_FIX(GraphAdaptorBase<GR>, EdgeMap<V>)> {
      typedef SubMapExtender<SubGraphBase<GR, NF, EF, ch>,
        LEMON_SCOPE_FIX(GraphAdaptorBase<GR>, EdgeMap<V>)> Parent;

    public:
      typedef V Value;

      EdgeMap(const SubGraphBase<GR, NF, EF, ch>& adaptor)
        : Parent(adaptor) {}

      EdgeMap(const SubGraphBase<GR, NF, EF, ch>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

  };

  template <typename GR, typename NF, typename EF>
  class SubGraphBase<GR, NF, EF, false>
    : public GraphAdaptorBase<GR> {
    typedef GraphAdaptorBase<GR> Parent;
  public:
    typedef GR Graph;
    typedef NF NodeFilterMap;
    typedef EF EdgeFilterMap;

    typedef SubGraphBase Adaptor;
  protected:
    NF* _node_filter;
    EF* _edge_filter;
    SubGraphBase()
          : Parent(), _node_filter(0), _edge_filter(0) { }

    void initialize(GR& graph, NF& node_filter, EF& edge_filter) {
      Parent::initialize(graph);
      _node_filter = &node_filter;
      _edge_filter = &edge_filter;
    }

  public:

    typedef typename Parent::Node Node;
    typedef typename Parent::Arc Arc;
    typedef typename Parent::Edge Edge;

    void first(Node& i) const {
      Parent::first(i);
      while (i!=INVALID && !(*_node_filter)[i]) Parent::next(i);
    }

    void first(Arc& i) const {
      Parent::first(i);
      while (i!=INVALID && !(*_edge_filter)[i]) Parent::next(i);
    }

    void first(Edge& i) const {
      Parent::first(i);
      while (i!=INVALID && !(*_edge_filter)[i]) Parent::next(i);
    }

    void firstIn(Arc& i, const Node& n) const {
      Parent::firstIn(i, n);
      while (i!=INVALID && !(*_edge_filter)[i]) Parent::nextIn(i);
    }

    void firstOut(Arc& i, const Node& n) const {
      Parent::firstOut(i, n);
      while (i!=INVALID && !(*_edge_filter)[i]) Parent::nextOut(i);
    }

    void firstInc(Edge& i, bool& d, const Node& n) const {
      Parent::firstInc(i, d, n);
      while (i!=INVALID && !(*_edge_filter)[i]) Parent::nextInc(i, d);
    }

    void next(Node& i) const {
      Parent::next(i);
      while (i!=INVALID && !(*_node_filter)[i]) Parent::next(i);
    }
    void next(Arc& i) const {
      Parent::next(i);
      while (i!=INVALID && !(*_edge_filter)[i]) Parent::next(i);
    }
    void next(Edge& i) const {
      Parent::next(i);
      while (i!=INVALID && !(*_edge_filter)[i]) Parent::next(i);
    }
    void nextIn(Arc& i) const {
      Parent::nextIn(i);
      while (i!=INVALID && !(*_edge_filter)[i]) Parent::nextIn(i);
    }

    void nextOut(Arc& i) const {
      Parent::nextOut(i);
      while (i!=INVALID && !(*_edge_filter)[i]) Parent::nextOut(i);
    }
    void nextInc(Edge& i, bool& d) const {
      Parent::nextInc(i, d);
      while (i!=INVALID && !(*_edge_filter)[i]) Parent::nextInc(i, d);
    }

    void status(const Node& n, bool v) const { _node_filter->set(n, v); }
    void status(const Edge& e, bool v) const { _edge_filter->set(e, v); }

    bool status(const Node& n) const { return (*_node_filter)[n]; }
    bool status(const Edge& e) const { return (*_edge_filter)[e]; }

    typedef False NodeNumTag;
    typedef False ArcNumTag;
    typedef False EdgeNumTag;

    typedef FindArcTagIndicator<Graph> FindArcTag;
    Arc findArc(const Node& u, const Node& v,
                const Arc& prev = INVALID) const {
      Arc arc = Parent::findArc(u, v, prev);
      while (arc != INVALID && !(*_edge_filter)[arc]) {
        arc = Parent::findArc(u, v, arc);
      }
      return arc;
    }

    typedef FindEdgeTagIndicator<Graph> FindEdgeTag;
    Edge findEdge(const Node& u, const Node& v,
                  const Edge& prev = INVALID) const {
      Edge edge = Parent::findEdge(u, v, prev);
      while (edge != INVALID && !(*_edge_filter)[edge]) {
        edge = Parent::findEdge(u, v, edge);
      }
      return edge;
    }

    template <typename V>
    class NodeMap
      : public SubMapExtender<SubGraphBase<GR, NF, EF, false>,
          LEMON_SCOPE_FIX(GraphAdaptorBase<GR>, NodeMap<V>)> {
      typedef SubMapExtender<SubGraphBase<GR, NF, EF, false>,
        LEMON_SCOPE_FIX(GraphAdaptorBase<GR>, NodeMap<V>)> Parent;

    public:
      typedef V Value;

      NodeMap(const SubGraphBase<GR, NF, EF, false>& adaptor)
        : Parent(adaptor) {}
      NodeMap(const SubGraphBase<GR, NF, EF, false>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

    template <typename V>
    class ArcMap
      : public SubMapExtender<SubGraphBase<GR, NF, EF, false>,
          LEMON_SCOPE_FIX(GraphAdaptorBase<GR>, ArcMap<V>)> {
      typedef SubMapExtender<SubGraphBase<GR, NF, EF, false>,
        LEMON_SCOPE_FIX(GraphAdaptorBase<GR>, ArcMap<V>)> Parent;

    public:
      typedef V Value;

      ArcMap(const SubGraphBase<GR, NF, EF, false>& adaptor)
        : Parent(adaptor) {}
      ArcMap(const SubGraphBase<GR, NF, EF, false>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

    template <typename V>
    class EdgeMap
      : public SubMapExtender<SubGraphBase<GR, NF, EF, false>,
        LEMON_SCOPE_FIX(GraphAdaptorBase<GR>, EdgeMap<V>)> {
      typedef SubMapExtender<SubGraphBase<GR, NF, EF, false>,
        LEMON_SCOPE_FIX(GraphAdaptorBase<GR>, EdgeMap<V>)> Parent;

    public:
      typedef V Value;

      EdgeMap(const SubGraphBase<GR, NF, EF, false>& adaptor)
        : Parent(adaptor) {}

      EdgeMap(const SubGraphBase<GR, NF, EF, false>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

  };

  /// \ingroup graph_adaptors
  ///
  /// \brief Adaptor class for hiding nodes and edges in an undirected
  /// graph.
  ///
  /// SubGraph can be used for hiding nodes and edges in a graph.
  /// A \c bool node map and a \c bool edge map must be specified, which
  /// define the filters for nodes and edges.
  /// Only the nodes and edges with \c true filter value are
  /// shown in the subgraph. The edges that are incident to hidden
  /// nodes are also filtered out.
  /// This adaptor conforms to the \ref concepts::Graph "Graph" concept.
  ///
  /// The adapted graph can also be modified through this adaptor
  /// by adding or removing nodes or edges, unless the \c GR template
  /// parameter is set to be \c const.
  ///
  /// This class provides only linear time counting for nodes, edges and arcs.
  ///
  /// \tparam GR The type of the adapted graph.
  /// It must conform to the \ref concepts::Graph "Graph" concept.
  /// It can also be specified to be \c const.
  /// \tparam NF The type of the node filter map.
  /// It must be a \c bool (or convertible) node map of the
  /// adapted graph. The default type is
  /// \ref concepts::Graph::NodeMap "GR::NodeMap<bool>".
  /// \tparam EF The type of the edge filter map.
  /// It must be a \c bool (or convertible) edge map of the
  /// adapted graph. The default type is
  /// \ref concepts::Graph::EdgeMap "GR::EdgeMap<bool>".
  ///
  /// \note The \c Node, \c Edge and \c Arc types of this adaptor and the
  /// adapted graph are convertible to each other.
  ///
  /// \see FilterNodes
  /// \see FilterEdges
#ifdef DOXYGEN
  template<typename GR, typename NF, typename EF>
  class SubGraph {
#else
  template<typename GR,
           typename NF = typename GR::template NodeMap<bool>,
           typename EF = typename GR::template EdgeMap<bool> >
  class SubGraph :
    public GraphAdaptorExtender<SubGraphBase<GR, NF, EF, true> > {
#endif
  public:
    /// The type of the adapted graph.
    typedef GR Graph;
    /// The type of the node filter map.
    typedef NF NodeFilterMap;
    /// The type of the edge filter map.
    typedef EF EdgeFilterMap;

    typedef GraphAdaptorExtender<SubGraphBase<GR, NF, EF, true> >
      Parent;

    typedef typename Parent::Node Node;
    typedef typename Parent::Edge Edge;

  protected:
    SubGraph() { }
  public:

    /// \brief Constructor
    ///
    /// Creates a subgraph for the given graph with the given node
    /// and edge filter maps.
    SubGraph(GR& graph, NF& node_filter, EF& edge_filter) {
      this->initialize(graph, node_filter, edge_filter);
    }

    /// \brief Sets the status of the given node
    ///
    /// This function sets the status of the given node.
    /// It is done by simply setting the assigned value of \c n
    /// to \c v in the node filter map.
    void status(const Node& n, bool v) const { Parent::status(n, v); }

    /// \brief Sets the status of the given edge
    ///
    /// This function sets the status of the given edge.
    /// It is done by simply setting the assigned value of \c e
    /// to \c v in the edge filter map.
    void status(const Edge& e, bool v) const { Parent::status(e, v); }

    /// \brief Returns the status of the given node
    ///
    /// This function returns the status of the given node.
    /// It is \c true if the given node is enabled (i.e. not hidden).
    bool status(const Node& n) const { return Parent::status(n); }

    /// \brief Returns the status of the given edge
    ///
    /// This function returns the status of the given edge.
    /// It is \c true if the given edge is enabled (i.e. not hidden).
    bool status(const Edge& e) const { return Parent::status(e); }

    /// \brief Disables the given node
    ///
    /// This function disables the given node in the subdigraph,
    /// so the iteration jumps over it.
    /// It is the same as \ref status() "status(n, false)".
    void disable(const Node& n) const { Parent::status(n, false); }

    /// \brief Disables the given edge
    ///
    /// This function disables the given edge in the subgraph,
    /// so the iteration jumps over it.
    /// It is the same as \ref status() "status(e, false)".
    void disable(const Edge& e) const { Parent::status(e, false); }

    /// \brief Enables the given node
    ///
    /// This function enables the given node in the subdigraph.
    /// It is the same as \ref status() "status(n, true)".
    void enable(const Node& n) const { Parent::status(n, true); }

    /// \brief Enables the given edge
    ///
    /// This function enables the given edge in the subgraph.
    /// It is the same as \ref status() "status(e, true)".
    void enable(const Edge& e) const { Parent::status(e, true); }

  };

  /// \brief Returns a read-only SubGraph adaptor
  ///
  /// This function just returns a read-only \ref SubGraph adaptor.
  /// \ingroup graph_adaptors
  /// \relates SubGraph
  template<typename GR, typename NF, typename EF>
  SubGraph<const GR, NF, EF>
  subGraph(const GR& graph, NF& node_filter, EF& edge_filter) {
    return SubGraph<const GR, NF, EF>
      (graph, node_filter, edge_filter);
  }

  template<typename GR, typename NF, typename EF>
  SubGraph<const GR, const NF, EF>
  subGraph(const GR& graph, const NF& node_filter, EF& edge_filter) {
    return SubGraph<const GR, const NF, EF>
      (graph, node_filter, edge_filter);
  }

  template<typename GR, typename NF, typename EF>
  SubGraph<const GR, NF, const EF>
  subGraph(const GR& graph, NF& node_filter, const EF& edge_filter) {
    return SubGraph<const GR, NF, const EF>
      (graph, node_filter, edge_filter);
  }

  template<typename GR, typename NF, typename EF>
  SubGraph<const GR, const NF, const EF>
  subGraph(const GR& graph, const NF& node_filter, const EF& edge_filter) {
    return SubGraph<const GR, const NF, const EF>
      (graph, node_filter, edge_filter);
  }


  /// \ingroup graph_adaptors
  ///
  /// \brief Adaptor class for hiding nodes in a digraph or a graph.
  ///
  /// FilterNodes adaptor can be used for hiding nodes in a digraph or a
  /// graph. A \c bool node map must be specified, which defines the filter
  /// for the nodes. Only the nodes with \c true filter value and the
  /// arcs/edges incident to nodes both with \c true filter value are shown
  /// in the subgraph. This adaptor conforms to the \ref concepts::Digraph
  /// "Digraph" concept or the \ref concepts::Graph "Graph" concept
  /// depending on the \c GR template parameter.
  ///
  /// The adapted (di)graph can also be modified through this adaptor
  /// by adding or removing nodes or arcs/edges, unless the \c GR template
  /// parameter is set to be \c const.
  ///
  /// This class provides only linear time item counting.
  ///
  /// \tparam GR The type of the adapted digraph or graph.
  /// It must conform to the \ref concepts::Digraph "Digraph" concept
  /// or the \ref concepts::Graph "Graph" concept.
  /// It can also be specified to be \c const.
  /// \tparam NF The type of the node filter map.
  /// It must be a \c bool (or convertible) node map of the
  /// adapted (di)graph. The default type is
  /// \ref concepts::Graph::NodeMap "GR::NodeMap<bool>".
  ///
  /// \note The \c Node and <tt>Arc/Edge</tt> types of this adaptor and the
  /// adapted (di)graph are convertible to each other.
#ifdef DOXYGEN
  template<typename GR, typename NF>
  class FilterNodes {
#else
  template<typename GR,
           typename NF = typename GR::template NodeMap<bool>,
           typename Enable = void>
  class FilterNodes :
    public DigraphAdaptorExtender<
      SubDigraphBase<GR, NF, ConstMap<typename GR::Arc, Const<bool, true> >,
                     true> > {
#endif
    typedef DigraphAdaptorExtender<
      SubDigraphBase<GR, NF, ConstMap<typename GR::Arc, Const<bool, true> >,
                     true> > Parent;

  public:

    typedef GR Digraph;
    typedef NF NodeFilterMap;

    typedef typename Parent::Node Node;

  protected:
    ConstMap<typename Digraph::Arc, Const<bool, true> > const_true_map;

    FilterNodes() : const_true_map() {}

  public:

    /// \brief Constructor
    ///
    /// Creates a subgraph for the given digraph or graph with the
    /// given node filter map.
    FilterNodes(GR& graph, NF& node_filter)
      : Parent(), const_true_map()
    {
      Parent::initialize(graph, node_filter, const_true_map);
    }

    /// \brief Sets the status of the given node
    ///
    /// This function sets the status of the given node.
    /// It is done by simply setting the assigned value of \c n
    /// to \c v in the node filter map.
    void status(const Node& n, bool v) const { Parent::status(n, v); }

    /// \brief Returns the status of the given node
    ///
    /// This function returns the status of the given node.
    /// It is \c true if the given node is enabled (i.e. not hidden).
    bool status(const Node& n) const { return Parent::status(n); }

    /// \brief Disables the given node
    ///
    /// This function disables the given node, so the iteration
    /// jumps over it.
    /// It is the same as \ref status() "status(n, false)".
    void disable(const Node& n) const { Parent::status(n, false); }

    /// \brief Enables the given node
    ///
    /// This function enables the given node.
    /// It is the same as \ref status() "status(n, true)".
    void enable(const Node& n) const { Parent::status(n, true); }

  };

  template<typename GR, typename NF>
  class FilterNodes<GR, NF,
                    typename enable_if<UndirectedTagIndicator<GR> >::type> :
    public GraphAdaptorExtender<
      SubGraphBase<GR, NF, ConstMap<typename GR::Edge, Const<bool, true> >,
                   true> > {

    typedef GraphAdaptorExtender<
      SubGraphBase<GR, NF, ConstMap<typename GR::Edge, Const<bool, true> >,
                   true> > Parent;

  public:

    typedef GR Graph;
    typedef NF NodeFilterMap;

    typedef typename Parent::Node Node;

  protected:
    ConstMap<typename GR::Edge, Const<bool, true> > const_true_map;

    FilterNodes() : const_true_map() {}

  public:

    FilterNodes(GR& graph, NodeFilterMap& node_filter) :
      Parent(), const_true_map() {
      Parent::initialize(graph, node_filter, const_true_map);
    }

    void status(const Node& n, bool v) const { Parent::status(n, v); }
    bool status(const Node& n) const { return Parent::status(n); }
    void disable(const Node& n) const { Parent::status(n, false); }
    void enable(const Node& n) const { Parent::status(n, true); }

  };


  /// \brief Returns a read-only FilterNodes adaptor
  ///
  /// This function just returns a read-only \ref FilterNodes adaptor.
  /// \ingroup graph_adaptors
  /// \relates FilterNodes
  template<typename GR, typename NF>
  FilterNodes<const GR, NF>
  filterNodes(const GR& graph, NF& node_filter) {
    return FilterNodes<const GR, NF>(graph, node_filter);
  }

  template<typename GR, typename NF>
  FilterNodes<const GR, const NF>
  filterNodes(const GR& graph, const NF& node_filter) {
    return FilterNodes<const GR, const NF>(graph, node_filter);
  }

  /// \ingroup graph_adaptors
  ///
  /// \brief Adaptor class for hiding arcs in a digraph.
  ///
  /// FilterArcs adaptor can be used for hiding arcs in a digraph.
  /// A \c bool arc map must be specified, which defines the filter for
  /// the arcs. Only the arcs with \c true filter value are shown in the
  /// subdigraph. This adaptor conforms to the \ref concepts::Digraph
  /// "Digraph" concept.
  ///
  /// The adapted digraph can also be modified through this adaptor
  /// by adding or removing nodes or arcs, unless the \c GR template
  /// parameter is set to be \c const.
  ///
  /// This class provides only linear time counting for nodes and arcs.
  ///
  /// \tparam DGR The type of the adapted digraph.
  /// It must conform to the \ref concepts::Digraph "Digraph" concept.
  /// It can also be specified to be \c const.
  /// \tparam AF The type of the arc filter map.
  /// It must be a \c bool (or convertible) arc map of the
  /// adapted digraph. The default type is
  /// \ref concepts::Digraph::ArcMap "DGR::ArcMap<bool>".
  ///
  /// \note The \c Node and \c Arc types of this adaptor and the adapted
  /// digraph are convertible to each other.
#ifdef DOXYGEN
  template<typename DGR,
           typename AF>
  class FilterArcs {
#else
  template<typename DGR,
           typename AF = typename DGR::template ArcMap<bool> >
  class FilterArcs :
    public DigraphAdaptorExtender<
      SubDigraphBase<DGR, ConstMap<typename DGR::Node, Const<bool, true> >,
                     AF, false> > {
#endif
    typedef DigraphAdaptorExtender<
      SubDigraphBase<DGR, ConstMap<typename DGR::Node, Const<bool, true> >,
                     AF, false> > Parent;

  public:

    /// The type of the adapted digraph.
    typedef DGR Digraph;
    /// The type of the arc filter map.
    typedef AF ArcFilterMap;

    typedef typename Parent::Arc Arc;

  protected:
    ConstMap<typename DGR::Node, Const<bool, true> > const_true_map;

    FilterArcs() : const_true_map() {}

  public:

    /// \brief Constructor
    ///
    /// Creates a subdigraph for the given digraph with the given arc
    /// filter map.
    FilterArcs(DGR& digraph, ArcFilterMap& arc_filter)
      : Parent(), const_true_map() {
      Parent::initialize(digraph, const_true_map, arc_filter);
    }

    /// \brief Sets the status of the given arc
    ///
    /// This function sets the status of the given arc.
    /// It is done by simply setting the assigned value of \c a
    /// to \c v in the arc filter map.
    void status(const Arc& a, bool v) const { Parent::status(a, v); }

    /// \brief Returns the status of the given arc
    ///
    /// This function returns the status of the given arc.
    /// It is \c true if the given arc is enabled (i.e. not hidden).
    bool status(const Arc& a) const { return Parent::status(a); }

    /// \brief Disables the given arc
    ///
    /// This function disables the given arc in the subdigraph,
    /// so the iteration jumps over it.
    /// It is the same as \ref status() "status(a, false)".
    void disable(const Arc& a) const { Parent::status(a, false); }

    /// \brief Enables the given arc
    ///
    /// This function enables the given arc in the subdigraph.
    /// It is the same as \ref status() "status(a, true)".
    void enable(const Arc& a) const { Parent::status(a, true); }

  };

  /// \brief Returns a read-only FilterArcs adaptor
  ///
  /// This function just returns a read-only \ref FilterArcs adaptor.
  /// \ingroup graph_adaptors
  /// \relates FilterArcs
  template<typename DGR, typename AF>
  FilterArcs<const DGR, AF>
  filterArcs(const DGR& digraph, AF& arc_filter) {
    return FilterArcs<const DGR, AF>(digraph, arc_filter);
  }

  template<typename DGR, typename AF>
  FilterArcs<const DGR, const AF>
  filterArcs(const DGR& digraph, const AF& arc_filter) {
    return FilterArcs<const DGR, const AF>(digraph, arc_filter);
  }

  /// \ingroup graph_adaptors
  ///
  /// \brief Adaptor class for hiding edges in a graph.
  ///
  /// FilterEdges adaptor can be used for hiding edges in a graph.
  /// A \c bool edge map must be specified, which defines the filter for
  /// the edges. Only the edges with \c true filter value are shown in the
  /// subgraph. This adaptor conforms to the \ref concepts::Graph
  /// "Graph" concept.
  ///
  /// The adapted graph can also be modified through this adaptor
  /// by adding or removing nodes or edges, unless the \c GR template
  /// parameter is set to be \c const.
  ///
  /// This class provides only linear time counting for nodes, edges and arcs.
  ///
  /// \tparam GR The type of the adapted graph.
  /// It must conform to the \ref concepts::Graph "Graph" concept.
  /// It can also be specified to be \c const.
  /// \tparam EF The type of the edge filter map.
  /// It must be a \c bool (or convertible) edge map of the
  /// adapted graph. The default type is
  /// \ref concepts::Graph::EdgeMap "GR::EdgeMap<bool>".
  ///
  /// \note The \c Node, \c Edge and \c Arc types of this adaptor and the
  /// adapted graph are convertible to each other.
#ifdef DOXYGEN
  template<typename GR,
           typename EF>
  class FilterEdges {
#else
  template<typename GR,
           typename EF = typename GR::template EdgeMap<bool> >
  class FilterEdges :
    public GraphAdaptorExtender<
      SubGraphBase<GR, ConstMap<typename GR::Node, Const<bool, true> >,
                   EF, false> > {
#endif
    typedef GraphAdaptorExtender<
      SubGraphBase<GR, ConstMap<typename GR::Node, Const<bool, true > >,
                   EF, false> > Parent;

  public:

    /// The type of the adapted graph.
    typedef GR Graph;
    /// The type of the edge filter map.
    typedef EF EdgeFilterMap;

    typedef typename Parent::Edge Edge;

  protected:
    ConstMap<typename GR::Node, Const<bool, true> > const_true_map;

    FilterEdges() : const_true_map(true) {
      Parent::setNodeFilterMap(const_true_map);
    }

  public:

    /// \brief Constructor
    ///
    /// Creates a subgraph for the given graph with the given edge
    /// filter map.
    FilterEdges(GR& graph, EF& edge_filter)
      : Parent(), const_true_map() {
      Parent::initialize(graph, const_true_map, edge_filter);
    }

    /// \brief Sets the status of the given edge
    ///
    /// This function sets the status of the given edge.
    /// It is done by simply setting the assigned value of \c e
    /// to \c v in the edge filter map.
    void status(const Edge& e, bool v) const { Parent::status(e, v); }

    /// \brief Returns the status of the given edge
    ///
    /// This function returns the status of the given edge.
    /// It is \c true if the given edge is enabled (i.e. not hidden).
    bool status(const Edge& e) const { return Parent::status(e); }

    /// \brief Disables the given edge
    ///
    /// This function disables the given edge in the subgraph,
    /// so the iteration jumps over it.
    /// It is the same as \ref status() "status(e, false)".
    void disable(const Edge& e) const { Parent::status(e, false); }

    /// \brief Enables the given edge
    ///
    /// This function enables the given edge in the subgraph.
    /// It is the same as \ref status() "status(e, true)".
    void enable(const Edge& e) const { Parent::status(e, true); }

  };

  /// \brief Returns a read-only FilterEdges adaptor
  ///
  /// This function just returns a read-only \ref FilterEdges adaptor.
  /// \ingroup graph_adaptors
  /// \relates FilterEdges
  template<typename GR, typename EF>
  FilterEdges<const GR, EF>
  filterEdges(const GR& graph, EF& edge_filter) {
    return FilterEdges<const GR, EF>(graph, edge_filter);
  }

  template<typename GR, typename EF>
  FilterEdges<const GR, const EF>
  filterEdges(const GR& graph, const EF& edge_filter) {
    return FilterEdges<const GR, const EF>(graph, edge_filter);
  }


  template <typename DGR>
  class UndirectorBase {
  public:
    typedef DGR Digraph;
    typedef UndirectorBase Adaptor;

    typedef True UndirectedTag;

    typedef typename Digraph::Arc Edge;
    typedef typename Digraph::Node Node;

    class Arc {
      friend class UndirectorBase;
    protected:
      Edge _edge;
      bool _forward;

      Arc(const Edge& edge, bool forward)
        : _edge(edge), _forward(forward) {}

    public:
      Arc() {}

      Arc(Invalid) : _edge(INVALID), _forward(true) {}

      operator const Edge&() const { return _edge; }

      bool operator==(const Arc &other) const {
        return _forward == other._forward && _edge == other._edge;
      }
      bool operator!=(const Arc &other) const {
        return _forward != other._forward || _edge != other._edge;
      }
      bool operator<(const Arc &other) const {
        return _forward < other._forward ||
          (_forward == other._forward && _edge < other._edge);
      }
    };

    void first(Node& n) const {
      _digraph->first(n);
    }

    void next(Node& n) const {
      _digraph->next(n);
    }

    void first(Arc& a) const {
      _digraph->first(a._edge);
      a._forward = true;
    }

    void next(Arc& a) const {
      if (a._forward) {
        a._forward = false;
      } else {
        _digraph->next(a._edge);
        a._forward = true;
      }
    }

    void first(Edge& e) const {
      _digraph->first(e);
    }

    void next(Edge& e) const {
      _digraph->next(e);
    }

    void firstOut(Arc& a, const Node& n) const {
      _digraph->firstIn(a._edge, n);
      if (a._edge != INVALID ) {
        a._forward = false;
      } else {
        _digraph->firstOut(a._edge, n);
        a._forward = true;
      }
    }
    void nextOut(Arc &a) const {
      if (!a._forward) {
        Node n = _digraph->target(a._edge);
        _digraph->nextIn(a._edge);
        if (a._edge == INVALID) {
          _digraph->firstOut(a._edge, n);
          a._forward = true;
        }
      }
      else {
        _digraph->nextOut(a._edge);
      }
    }

    void firstIn(Arc &a, const Node &n) const {
      _digraph->firstOut(a._edge, n);
      if (a._edge != INVALID ) {
        a._forward = false;
      } else {
        _digraph->firstIn(a._edge, n);
        a._forward = true;
      }
    }
    void nextIn(Arc &a) const {
      if (!a._forward) {
        Node n = _digraph->source(a._edge);
        _digraph->nextOut(a._edge);
        if (a._edge == INVALID ) {
          _digraph->firstIn(a._edge, n);
          a._forward = true;
        }
      }
      else {
        _digraph->nextIn(a._edge);
      }
    }

    void firstInc(Edge &e, bool &d, const Node &n) const {
      d = true;
      _digraph->firstOut(e, n);
      if (e != INVALID) return;
      d = false;
      _digraph->firstIn(e, n);
    }

    void nextInc(Edge &e, bool &d) const {
      if (d) {
        Node s = _digraph->source(e);
        _digraph->nextOut(e);
        if (e != INVALID) return;
        d = false;
        _digraph->firstIn(e, s);
      } else {
        _digraph->nextIn(e);
      }
    }

    Node u(const Edge& e) const {
      return _digraph->source(e);
    }

    Node v(const Edge& e) const {
      return _digraph->target(e);
    }

    Node source(const Arc &a) const {
      return a._forward ? _digraph->source(a._edge) : _digraph->target(a._edge);
    }

    Node target(const Arc &a) const {
      return a._forward ? _digraph->target(a._edge) : _digraph->source(a._edge);
    }

    static Arc direct(const Edge &e, bool d) {
      return Arc(e, d);
    }

    static bool direction(const Arc &a) { return a._forward; }

    Node nodeFromId(int ix) const { return _digraph->nodeFromId(ix); }
    Arc arcFromId(int ix) const {
      return direct(_digraph->arcFromId(ix >> 1), bool(ix & 1));
    }
    Edge edgeFromId(int ix) const { return _digraph->arcFromId(ix); }

    int id(const Node &n) const { return _digraph->id(n); }
    int id(const Arc &a) const {
      return  (_digraph->id(a) << 1) | (a._forward ? 1 : 0);
    }
    int id(const Edge &e) const { return _digraph->id(e); }

    int maxNodeId() const { return _digraph->maxNodeId(); }
    int maxArcId() const { return (_digraph->maxArcId() << 1) | 1; }
    int maxEdgeId() const { return _digraph->maxArcId(); }

    Node addNode() { return _digraph->addNode(); }
    Edge addEdge(const Node& u, const Node& v) {
      return _digraph->addArc(u, v);
    }

    void erase(const Node& i) { _digraph->erase(i); }
    void erase(const Edge& i) { _digraph->erase(i); }

    void clear() { _digraph->clear(); }

    typedef NodeNumTagIndicator<Digraph> NodeNumTag;
    int nodeNum() const { return _digraph->nodeNum(); }

    typedef ArcNumTagIndicator<Digraph> ArcNumTag;
    int arcNum() const { return 2 * _digraph->arcNum(); }

    typedef ArcNumTag EdgeNumTag;
    int edgeNum() const { return _digraph->arcNum(); }

    typedef FindArcTagIndicator<Digraph> FindArcTag;
    Arc findArc(Node s, Node t, Arc p = INVALID) const {
      if (p == INVALID) {
        Edge arc = _digraph->findArc(s, t);
        if (arc != INVALID) return direct(arc, true);
        arc = _digraph->findArc(t, s);
        if (arc != INVALID) return direct(arc, false);
      } else if (direction(p)) {
        Edge arc = _digraph->findArc(s, t, p);
        if (arc != INVALID) return direct(arc, true);
        arc = _digraph->findArc(t, s);
        if (arc != INVALID) return direct(arc, false);
      } else {
        Edge arc = _digraph->findArc(t, s, p);
        if (arc != INVALID) return direct(arc, false);
      }
      return INVALID;
    }

    typedef FindArcTag FindEdgeTag;
    Edge findEdge(Node s, Node t, Edge p = INVALID) const {
      if (s != t) {
        if (p == INVALID) {
          Edge arc = _digraph->findArc(s, t);
          if (arc != INVALID) return arc;
          arc = _digraph->findArc(t, s);
          if (arc != INVALID) return arc;
        } else if (_digraph->source(p) == s) {
          Edge arc = _digraph->findArc(s, t, p);
          if (arc != INVALID) return arc;
          arc = _digraph->findArc(t, s);
          if (arc != INVALID) return arc;
        } else {
          Edge arc = _digraph->findArc(t, s, p);
          if (arc != INVALID) return arc;
        }
      } else {
        return _digraph->findArc(s, t, p);
      }
      return INVALID;
    }

  private:

    template <typename V>
    class ArcMapBase {
    private:

      typedef typename DGR::template ArcMap<V> MapImpl;

    public:

      typedef typename MapTraits<MapImpl>::ReferenceMapTag ReferenceMapTag;

      typedef V Value;
      typedef Arc Key;
      typedef typename MapTraits<MapImpl>::ConstReturnValue ConstReturnValue;
      typedef typename MapTraits<MapImpl>::ReturnValue ReturnValue;
      typedef typename MapTraits<MapImpl>::ConstReturnValue ConstReference;
      typedef typename MapTraits<MapImpl>::ReturnValue Reference;

      ArcMapBase(const UndirectorBase<DGR>& adaptor) :
        _forward(*adaptor._digraph), _backward(*adaptor._digraph) {}

      ArcMapBase(const UndirectorBase<DGR>& adaptor, const V& value)
        : _forward(*adaptor._digraph, value),
          _backward(*adaptor._digraph, value) {}

      void set(const Arc& a, const V& value) {
        if (direction(a)) {
          _forward.set(a, value);
        } else {
          _backward.set(a, value);
        }
      }

      ConstReturnValue operator[](const Arc& a) const {
        if (direction(a)) {
          return _forward[a];
        } else {
          return _backward[a];
        }
      }

      ReturnValue operator[](const Arc& a) {
        if (direction(a)) {
          return _forward[a];
        } else {
          return _backward[a];
        }
      }

    protected:

      MapImpl _forward, _backward;

    };

  public:

    template <typename V>
    class NodeMap : public DGR::template NodeMap<V> {
      typedef typename DGR::template NodeMap<V> Parent;

    public:
      typedef V Value;

      explicit NodeMap(const UndirectorBase<DGR>& adaptor)
        : Parent(*adaptor._digraph) {}

      NodeMap(const UndirectorBase<DGR>& adaptor, const V& value)
        : Parent(*adaptor._digraph, value) { }

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

    template <typename V>
    class ArcMap
      : public SubMapExtender<UndirectorBase<DGR>, ArcMapBase<V> > {
      typedef SubMapExtender<UndirectorBase<DGR>, ArcMapBase<V> > Parent;

    public:
      typedef V Value;

      explicit ArcMap(const UndirectorBase<DGR>& adaptor)
        : Parent(adaptor) {}

      ArcMap(const UndirectorBase<DGR>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

    template <typename V>
    class EdgeMap : public Digraph::template ArcMap<V> {
      typedef typename Digraph::template ArcMap<V> Parent;

    public:
      typedef V Value;

      explicit EdgeMap(const UndirectorBase<DGR>& adaptor)
        : Parent(*adaptor._digraph) {}

      EdgeMap(const UndirectorBase<DGR>& adaptor, const V& value)
        : Parent(*adaptor._digraph, value) {}

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

    typedef typename ItemSetTraits<DGR, Node>::ItemNotifier NodeNotifier;
    NodeNotifier& notifier(Node) const { return _digraph->notifier(Node()); }

    typedef typename ItemSetTraits<DGR, Edge>::ItemNotifier EdgeNotifier;
    EdgeNotifier& notifier(Edge) const { return _digraph->notifier(Edge()); }

    typedef EdgeNotifier ArcNotifier;
    ArcNotifier& notifier(Arc) const { return _digraph->notifier(Edge()); }

  protected:

    UndirectorBase() : _digraph(0) {}

    DGR* _digraph;

    void initialize(DGR& digraph) {
      _digraph = &digraph;
    }

  };

  /// \ingroup graph_adaptors
  ///
  /// \brief Adaptor class for viewing a digraph as an undirected graph.
  ///
  /// Undirector adaptor can be used for viewing a digraph as an undirected
  /// graph. All arcs of the underlying digraph are showed in the
  /// adaptor as an edge (and also as a pair of arcs, of course).
  /// This adaptor conforms to the \ref concepts::Graph "Graph" concept.
  ///
  /// The adapted digraph can also be modified through this adaptor
  /// by adding or removing nodes or edges, unless the \c GR template
  /// parameter is set to be \c const.
  ///
  /// This class provides item counting in the same time as the adapted
  /// digraph structure.
  ///
  /// \tparam DGR The type of the adapted digraph.
  /// It must conform to the \ref concepts::Digraph "Digraph" concept.
  /// It can also be specified to be \c const.
  ///
  /// \note The \c Node type of this adaptor and the adapted digraph are
  /// convertible to each other, moreover the \c Edge type of the adaptor
  /// and the \c Arc type of the adapted digraph are also convertible to
  /// each other.
  /// (Thus the \c Arc type of the adaptor is convertible to the \c Arc type
  /// of the adapted digraph.)
  template<typename DGR>
#ifdef DOXYGEN
  class Undirector {
#else
  class Undirector :
    public GraphAdaptorExtender<UndirectorBase<DGR> > {
#endif
    typedef GraphAdaptorExtender<UndirectorBase<DGR> > Parent;
  public:
    /// The type of the adapted digraph.
    typedef DGR Digraph;
  protected:
    Undirector() { }
  public:

    /// \brief Constructor
    ///
    /// Creates an undirected graph from the given digraph.
    Undirector(DGR& digraph) {
      this->initialize(digraph);
    }

    /// \brief Arc map combined from two original arc maps
    ///
    /// This map adaptor class adapts two arc maps of the underlying
    /// digraph to get an arc map of the undirected graph.
    /// Its value type is inherited from the first arc map type (\c FW).
    /// \tparam FW The type of the "foward" arc map.
    /// \tparam BK The type of the "backward" arc map.
    template <typename FW, typename BK>
    class CombinedArcMap {
    public:

      /// The key type of the map
      typedef typename Parent::Arc Key;
      /// The value type of the map
      typedef typename FW::Value Value;

      typedef typename MapTraits<FW>::ReferenceMapTag ReferenceMapTag;

      typedef typename MapTraits<FW>::ReturnValue ReturnValue;
      typedef typename MapTraits<FW>::ConstReturnValue ConstReturnValue;
      typedef typename MapTraits<FW>::ReturnValue Reference;
      typedef typename MapTraits<FW>::ConstReturnValue ConstReference;

      /// Constructor
      CombinedArcMap(FW& forward, BK& backward)
        : _forward(&forward), _backward(&backward) {}

      /// Sets the value associated with the given key.
      void set(const Key& e, const Value& a) {
        if (Parent::direction(e)) {
          _forward->set(e, a);
        } else {
          _backward->set(e, a);
        }
      }

      /// Returns the value associated with the given key.
      ConstReturnValue operator[](const Key& e) const {
        if (Parent::direction(e)) {
          return (*_forward)[e];
        } else {
          return (*_backward)[e];
        }
      }

      /// Returns a reference to the value associated with the given key.
      ReturnValue operator[](const Key& e) {
        if (Parent::direction(e)) {
          return (*_forward)[e];
        } else {
          return (*_backward)[e];
        }
      }

    protected:

      FW* _forward;
      BK* _backward;

    };

    /// \brief Returns a combined arc map
    ///
    /// This function just returns a combined arc map.
    template <typename FW, typename BK>
    static CombinedArcMap<FW, BK>
    combinedArcMap(FW& forward, BK& backward) {
      return CombinedArcMap<FW, BK>(forward, backward);
    }

    template <typename FW, typename BK>
    static CombinedArcMap<const FW, BK>
    combinedArcMap(const FW& forward, BK& backward) {
      return CombinedArcMap<const FW, BK>(forward, backward);
    }

    template <typename FW, typename BK>
    static CombinedArcMap<FW, const BK>
    combinedArcMap(FW& forward, const BK& backward) {
      return CombinedArcMap<FW, const BK>(forward, backward);
    }

    template <typename FW, typename BK>
    static CombinedArcMap<const FW, const BK>
    combinedArcMap(const FW& forward, const BK& backward) {
      return CombinedArcMap<const FW, const BK>(forward, backward);
    }

  };

  /// \brief Returns a read-only Undirector adaptor
  ///
  /// This function just returns a read-only \ref Undirector adaptor.
  /// \ingroup graph_adaptors
  /// \relates Undirector
  template<typename DGR>
  Undirector<const DGR> undirector(const DGR& digraph) {
    return Undirector<const DGR>(digraph);
  }


  template <typename GR, typename DM>
  class OrienterBase {
  public:

    typedef GR Graph;
    typedef DM DirectionMap;

    typedef typename GR::Node Node;
    typedef typename GR::Edge Arc;

    void reverseArc(const Arc& arc) {
      _direction->set(arc, !(*_direction)[arc]);
    }

    void first(Node& i) const { _graph->first(i); }
    void first(Arc& i) const { _graph->first(i); }
    void firstIn(Arc& i, const Node& n) const {
      bool d = true;
      _graph->firstInc(i, d, n);
      while (i != INVALID && d == (*_direction)[i]) _graph->nextInc(i, d);
    }
    void firstOut(Arc& i, const Node& n ) const {
      bool d = true;
      _graph->firstInc(i, d, n);
      while (i != INVALID && d != (*_direction)[i]) _graph->nextInc(i, d);
    }

    void next(Node& i) const { _graph->next(i); }
    void next(Arc& i) const { _graph->next(i); }
    void nextIn(Arc& i) const {
      bool d = !(*_direction)[i];
      _graph->nextInc(i, d);
      while (i != INVALID && d == (*_direction)[i]) _graph->nextInc(i, d);
    }
    void nextOut(Arc& i) const {
      bool d = (*_direction)[i];
      _graph->nextInc(i, d);
      while (i != INVALID && d != (*_direction)[i]) _graph->nextInc(i, d);
    }

    Node source(const Arc& e) const {
      return (*_direction)[e] ? _graph->u(e) : _graph->v(e);
    }
    Node target(const Arc& e) const {
      return (*_direction)[e] ? _graph->v(e) : _graph->u(e);
    }

    typedef NodeNumTagIndicator<Graph> NodeNumTag;
    int nodeNum() const { return _graph->nodeNum(); }

    typedef EdgeNumTagIndicator<Graph> ArcNumTag;
    int arcNum() const { return _graph->edgeNum(); }

    typedef FindEdgeTagIndicator<Graph> FindArcTag;
    Arc findArc(const Node& u, const Node& v,
                const Arc& prev = INVALID) const {
      Arc arc = _graph->findEdge(u, v, prev);
      while (arc != INVALID && source(arc) != u) {
        arc = _graph->findEdge(u, v, arc);
      }
      return arc;
    }

    Node addNode() {
      return Node(_graph->addNode());
    }

    Arc addArc(const Node& u, const Node& v) {
      Arc arc = _graph->addEdge(u, v);
      _direction->set(arc, _graph->u(arc) == u);
      return arc;
    }

    void erase(const Node& i) { _graph->erase(i); }
    void erase(const Arc& i) { _graph->erase(i); }

    void clear() { _graph->clear(); }

    int id(const Node& v) const { return _graph->id(v); }
    int id(const Arc& e) const { return _graph->id(e); }

    Node nodeFromId(int idx) const { return _graph->nodeFromId(idx); }
    Arc arcFromId(int idx) const { return _graph->edgeFromId(idx); }

    int maxNodeId() const { return _graph->maxNodeId(); }
    int maxArcId() const { return _graph->maxEdgeId(); }

    typedef typename ItemSetTraits<GR, Node>::ItemNotifier NodeNotifier;
    NodeNotifier& notifier(Node) const { return _graph->notifier(Node()); }

    typedef typename ItemSetTraits<GR, Arc>::ItemNotifier ArcNotifier;
    ArcNotifier& notifier(Arc) const { return _graph->notifier(Arc()); }

    template <typename V>
    class NodeMap : public GR::template NodeMap<V> {
      typedef typename GR::template NodeMap<V> Parent;

    public:

      explicit NodeMap(const OrienterBase<GR, DM>& adapter)
        : Parent(*adapter._graph) {}

      NodeMap(const OrienterBase<GR, DM>& adapter, const V& value)
        : Parent(*adapter._graph, value) {}

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

    template <typename V>
    class ArcMap : public GR::template EdgeMap<V> {
      typedef typename Graph::template EdgeMap<V> Parent;

    public:

      explicit ArcMap(const OrienterBase<GR, DM>& adapter)
        : Parent(*adapter._graph) { }

      ArcMap(const OrienterBase<GR, DM>& adapter, const V& value)
        : Parent(*adapter._graph, value) { }

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



  protected:
    Graph* _graph;
    DM* _direction;

    void initialize(GR& graph, DM& direction) {
      _graph = &graph;
      _direction = &direction;
    }

  };

  /// \ingroup graph_adaptors
  ///
  /// \brief Adaptor class for orienting the edges of a graph to get a digraph
  ///
  /// Orienter adaptor can be used for orienting the edges of a graph to
  /// get a digraph. A \c bool edge map of the underlying graph must be
  /// specified, which define the direction of the arcs in the adaptor.
  /// The arcs can be easily reversed by the \c reverseArc() member function
  /// of the adaptor.
  /// This class conforms to the \ref concepts::Digraph "Digraph" concept.
  ///
  /// The adapted graph can also be modified through this adaptor
  /// by adding or removing nodes or arcs, unless the \c GR template
  /// parameter is set to be \c const.
  ///
  /// This class provides item counting in the same time as the adapted
  /// graph structure.
  ///
  /// \tparam GR The type of the adapted graph.
  /// It must conform to the \ref concepts::Graph "Graph" concept.
  /// It can also be specified to be \c const.
  /// \tparam DM The type of the direction map.
  /// It must be a \c bool (or convertible) edge map of the
  /// adapted graph. The default type is
  /// \ref concepts::Graph::EdgeMap "GR::EdgeMap<bool>".
  ///
  /// \note The \c Node type of this adaptor and the adapted graph are
  /// convertible to each other, moreover the \c Arc type of the adaptor
  /// and the \c Edge type of the adapted graph are also convertible to
  /// each other.
#ifdef DOXYGEN
  template<typename GR,
           typename DM>
  class Orienter {
#else
  template<typename GR,
           typename DM = typename GR::template EdgeMap<bool> >
  class Orienter :
    public DigraphAdaptorExtender<OrienterBase<GR, DM> > {
#endif
    typedef DigraphAdaptorExtender<OrienterBase<GR, DM> > Parent;
  public:

    /// The type of the adapted graph.
    typedef GR Graph;
    /// The type of the direction edge map.
    typedef DM DirectionMap;

    typedef typename Parent::Arc Arc;

  protected:
    Orienter() { }

  public:

    /// \brief Constructor
    ///
    /// Constructor of the adaptor.
    Orienter(GR& graph, DM& direction) {
      Parent::initialize(graph, direction);
    }

    /// \brief Reverses the given arc
    ///
    /// This function reverses the given arc.
    /// It is done by simply negate the assigned value of \c a
    /// in the direction map.
    void reverseArc(const Arc& a) {
      Parent::reverseArc(a);
    }
  };

  /// \brief Returns a read-only Orienter adaptor
  ///
  /// This function just returns a read-only \ref Orienter adaptor.
  /// \ingroup graph_adaptors
  /// \relates Orienter
  template<typename GR, typename DM>
  Orienter<const GR, DM>
  orienter(const GR& graph, DM& direction) {
    return Orienter<const GR, DM>(graph, direction);
  }

  template<typename GR, typename DM>
  Orienter<const GR, const DM>
  orienter(const GR& graph, const DM& direction) {
    return Orienter<const GR, const DM>(graph, direction);
  }

  namespace _adaptor_bits {

    template <typename DGR, typename CM, typename FM, typename TL>
    class ResForwardFilter {
    public:

      typedef typename DGR::Arc Key;
      typedef bool Value;

    private:

      const CM* _capacity;
      const FM* _flow;
      TL _tolerance;

    public:

      ResForwardFilter(const CM& capacity, const FM& flow,
                       const TL& tolerance = TL())
        : _capacity(&capacity), _flow(&flow), _tolerance(tolerance) { }

      bool operator[](const typename DGR::Arc& a) const {
        return _tolerance.positive((*_capacity)[a] - (*_flow)[a]);
      }
    };

    template<typename DGR,typename CM, typename FM, typename TL>
    class ResBackwardFilter {
    public:

      typedef typename DGR::Arc Key;
      typedef bool Value;

    private:

      const CM* _capacity;
      const FM* _flow;
      TL _tolerance;

    public:

      ResBackwardFilter(const CM& capacity, const FM& flow,
                        const TL& tolerance = TL())
        : _capacity(&capacity), _flow(&flow), _tolerance(tolerance) { }

      bool operator[](const typename DGR::Arc& a) const {
        return _tolerance.positive((*_flow)[a]);
      }
    };

  }

  /// \ingroup graph_adaptors
  ///
  /// \brief Adaptor class for composing the residual digraph for directed
  /// flow and circulation problems.
  ///
  /// ResidualDigraph can be used for composing the \e residual digraph
  /// for directed flow and circulation problems. Let \f$ G=(V, A) \f$
  /// be a directed graph and let \f$ F \f$ be a number type.
  /// Let \f$ flow, cap: A\to F \f$ be functions on the arcs.
  /// This adaptor implements a digraph structure with node set \f$ V \f$
  /// and arc set \f$ A_{forward}\cup A_{backward} \f$,
  /// where \f$ A_{forward}=\{uv : uv\in A, flow(uv)<cap(uv)\} \f$ and
  /// \f$ A_{backward}=\{vu : uv\in A, flow(uv)>0\} \f$, i.e. the so
  /// called residual digraph.
  /// When the union \f$ A_{forward}\cup A_{backward} \f$ is taken,
  /// multiplicities are counted, i.e. the adaptor has exactly
  /// \f$ |A_{forward}| + |A_{backward}|\f$ arcs (it may have parallel
  /// arcs).
  /// This class conforms to the \ref concepts::Digraph "Digraph" concept.
  ///
  /// This class provides only linear time counting for nodes and arcs.
  ///
  /// \tparam DGR The type of the adapted digraph.
  /// It must conform to the \ref concepts::Digraph "Digraph" concept.
  /// It is implicitly \c const.
  /// \tparam CM The type of the capacity map.
  /// It must be an arc map of some numerical type, which defines
  /// the capacities in the flow problem. It is implicitly \c const.
  /// The default type is
  /// \ref concepts::Digraph::ArcMap "GR::ArcMap<int>".
  /// \tparam FM The type of the flow map.
  /// It must be an arc map of some numerical type, which defines
  /// the flow values in the flow problem. The default type is \c CM.
  /// \tparam TL The tolerance type for handling inexact computation.
  /// The default tolerance type depends on the value type of the
  /// capacity map.
  ///
  /// \note This adaptor is implemented using Undirector and FilterArcs
  /// adaptors.
  ///
  /// \note The \c Node type of this adaptor and the adapted digraph are
  /// convertible to each other, moreover the \c Arc type of the adaptor
  /// is convertible to the \c Arc type of the adapted digraph.
#ifdef DOXYGEN
  template<typename DGR, typename CM, typename FM, typename TL>
  class ResidualDigraph
#else
  template<typename DGR,
           typename CM = typename DGR::template ArcMap<int>,
           typename FM = CM,
           typename TL = Tolerance<typename CM::Value> >
  class ResidualDigraph
    : public SubDigraph<
        Undirector<const DGR>,
        ConstMap<typename DGR::Node, Const<bool, true> >,
        typename Undirector<const DGR>::template CombinedArcMap<
          _adaptor_bits::ResForwardFilter<const DGR, CM, FM, TL>,
          _adaptor_bits::ResBackwardFilter<const DGR, CM, FM, TL> > >
#endif
  {
  public:

    /// The type of the underlying digraph.
    typedef DGR Digraph;
    /// The type of the capacity map.
    typedef CM CapacityMap;
    /// The type of the flow map.
    typedef FM FlowMap;
    /// The tolerance type.
    typedef TL Tolerance;

    typedef typename CapacityMap::Value Value;
    typedef ResidualDigraph Adaptor;

  protected:

    typedef Undirector<const Digraph> Undirected;

    typedef ConstMap<typename DGR::Node, Const<bool, true> > NodeFilter;

    typedef _adaptor_bits::ResForwardFilter<const DGR, CM,
                                            FM, TL> ForwardFilter;

    typedef _adaptor_bits::ResBackwardFilter<const DGR, CM,
                                             FM, TL> BackwardFilter;

    typedef typename Undirected::
      template CombinedArcMap<ForwardFilter, BackwardFilter> ArcFilter;

    typedef SubDigraph<Undirected, NodeFilter, ArcFilter> Parent;

    const CapacityMap* _capacity;
    FlowMap* _flow;

    Undirected _graph;
    NodeFilter _node_filter;
    ForwardFilter _forward_filter;
    BackwardFilter _backward_filter;
    ArcFilter _arc_filter;

  public:

    /// \brief Constructor
    ///
    /// Constructor of the residual digraph adaptor. The parameters are the
    /// digraph, the capacity map, the flow map, and a tolerance object.
    ResidualDigraph(const DGR& digraph, const CM& capacity,
                    FM& flow, const TL& tolerance = Tolerance())
      : Parent(), _capacity(&capacity), _flow(&flow),
        _graph(digraph), _node_filter(),
        _forward_filter(capacity, flow, tolerance),
        _backward_filter(capacity, flow, tolerance),
        _arc_filter(_forward_filter, _backward_filter)
    {
      Parent::initialize(_graph, _node_filter, _arc_filter);
    }

    typedef typename Parent::Arc Arc;

    /// \brief Returns the residual capacity of the given arc.
    ///
    /// Returns the residual capacity of the given arc.
    Value residualCapacity(const Arc& a) const {
      if (Undirected::direction(a)) {
        return (*_capacity)[a] - (*_flow)[a];
      } else {
        return (*_flow)[a];
      }
    }

    /// \brief Augments on the given arc in the residual digraph.
    ///
    /// Augments on the given arc in the residual digraph. It increases
    /// or decreases the flow value on the original arc according to the
    /// direction of the residual arc.
    void augment(const Arc& a, const Value& v) const {
      if (Undirected::direction(a)) {
        _flow->set(a, (*_flow)[a] + v);
      } else {
        _flow->set(a, (*_flow)[a] - v);
      }
    }

    /// \brief Returns \c true if the given residual arc is a forward arc.
    ///
    /// Returns \c true if the given residual arc has the same orientation
    /// as the original arc, i.e. it is a so called forward arc.
    static bool forward(const Arc& a) {
      return Undirected::direction(a);
    }

    /// \brief Returns \c true if the given residual arc is a backward arc.
    ///
    /// Returns \c true if the given residual arc has the opposite orientation
    /// than the original arc, i.e. it is a so called backward arc.
    static bool backward(const Arc& a) {
      return !Undirected::direction(a);
    }

    /// \brief Returns the forward oriented residual arc.
    ///
    /// Returns the forward oriented residual arc related to the given
    /// arc of the underlying digraph.
    static Arc forward(const typename Digraph::Arc& a) {
      return Undirected::direct(a, true);
    }

    /// \brief Returns the backward oriented residual arc.
    ///
    /// Returns the backward oriented residual arc related to the given
    /// arc of the underlying digraph.
    static Arc backward(const typename Digraph::Arc& a) {
      return Undirected::direct(a, false);
    }

    /// \brief Residual capacity map.
    ///
    /// This map adaptor class can be used for obtaining the residual
    /// capacities as an arc map of the residual digraph.
    /// Its value type is inherited from the capacity map.
    class ResidualCapacity {
    protected:
      const Adaptor* _adaptor;
    public:
      /// The key type of the map
      typedef Arc Key;
      /// The value type of the map
      typedef typename CapacityMap::Value Value;

      /// Constructor
      ResidualCapacity(const ResidualDigraph<DGR, CM, FM, TL>& adaptor)
        : _adaptor(&adaptor) {}

      /// Returns the value associated with the given residual arc
      Value operator[](const Arc& a) const {
        return _adaptor->residualCapacity(a);
      }

    };

    /// \brief Returns a residual capacity map
    ///
    /// This function just returns a residual capacity map.
    ResidualCapacity residualCapacity() const {
      return ResidualCapacity(*this);
    }

  };

  /// \brief Returns a (read-only) Residual adaptor
  ///
  /// This function just returns a (read-only) \ref ResidualDigraph adaptor.
  /// \ingroup graph_adaptors
  /// \relates ResidualDigraph
    template<typename DGR, typename CM, typename FM>
  ResidualDigraph<DGR, CM, FM>
  residualDigraph(const DGR& digraph, const CM& capacity_map, FM& flow_map) {
    return ResidualDigraph<DGR, CM, FM> (digraph, capacity_map, flow_map);
  }


  template <typename DGR>
  class SplitNodesBase {
    typedef DigraphAdaptorBase<const DGR> Parent;

  public:

    typedef DGR Digraph;
    typedef SplitNodesBase Adaptor;

    typedef typename DGR::Node DigraphNode;
    typedef typename DGR::Arc DigraphArc;

    class Node;
    class Arc;

  private:

    template <typename T> class NodeMapBase;
    template <typename T> class ArcMapBase;

  public:

    class Node : public DigraphNode {
      friend class SplitNodesBase;
      template <typename T> friend class NodeMapBase;
    private:

      bool _in;
      Node(DigraphNode node, bool in)
        : DigraphNode(node), _in(in) {}

    public:

      Node() {}
      Node(Invalid) : DigraphNode(INVALID), _in(true) {}

      bool operator==(const Node& node) const {
        return DigraphNode::operator==(node) && _in == node._in;
      }

      bool operator!=(const Node& node) const {
        return !(*this == node);
      }

      bool operator<(const Node& node) const {
        return DigraphNode::operator<(node) ||
          (DigraphNode::operator==(node) && _in < node._in);
      }
    };

    class Arc {
      friend class SplitNodesBase;
      template <typename T> friend class ArcMapBase;
    private:
      typedef BiVariant<DigraphArc, DigraphNode> ArcImpl;

      explicit Arc(const DigraphArc& arc) : _item(arc) {}
      explicit Arc(const DigraphNode& node) : _item(node) {}

      ArcImpl _item;

    public:
      Arc() {}
      Arc(Invalid) : _item(DigraphArc(INVALID)) {}

      bool operator==(const Arc& arc) const {
        if (_item.firstState()) {
          if (arc._item.firstState()) {
            return _item.first() == arc._item.first();
          }
        } else {
          if (arc._item.secondState()) {
            return _item.second() == arc._item.second();
          }
        }
        return false;
      }

      bool operator!=(const Arc& arc) const {
        return !(*this == arc);
      }

      bool operator<(const Arc& arc) const {
        if (_item.firstState()) {
          if (arc._item.firstState()) {
            return _item.first() < arc._item.first();
          }
          return false;
        } else {
          if (arc._item.secondState()) {
            return _item.second() < arc._item.second();
          }
          return true;
        }
      }

      operator DigraphArc() const { return _item.first(); }
      operator DigraphNode() const { return _item.second(); }

    };

    void first(Node& n) const {
      _digraph->first(n);
      n._in = true;
    }

    void next(Node& n) const {
      if (n._in) {
        n._in = false;
      } else {
        n._in = true;
        _digraph->next(n);
      }
    }

    void first(Arc& e) const {
      e._item.setSecond();
      _digraph->first(e._item.second());
      if (e._item.second() == INVALID) {
        e._item.setFirst();
        _digraph->first(e._item.first());
      }
    }

    void next(Arc& e) const {
      if (e._item.secondState()) {
        _digraph->next(e._item.second());
        if (e._item.second() == INVALID) {
          e._item.setFirst();
          _digraph->first(e._item.first());
        }
      } else {
        _digraph->next(e._item.first());
      }
    }

    void firstOut(Arc& e, const Node& n) const {
      if (n._in) {
        e._item.setSecond(n);
      } else {
        e._item.setFirst();
        _digraph->firstOut(e._item.first(), n);
      }
    }

    void nextOut(Arc& e) const {
      if (!e._item.firstState()) {
        e._item.setFirst(INVALID);
      } else {
        _digraph->nextOut(e._item.first());
      }
    }

    void firstIn(Arc& e, const Node& n) const {
      if (!n._in) {
        e._item.setSecond(n);
      } else {
        e._item.setFirst();
        _digraph->firstIn(e._item.first(), n);
      }
    }

    void nextIn(Arc& e) const {
      if (!e._item.firstState()) {
        e._item.setFirst(INVALID);
      } else {
        _digraph->nextIn(e._item.first());
      }
    }

    Node source(const Arc& e) const {
      if (e._item.firstState()) {
        return Node(_digraph->source(e._item.first()), false);
      } else {
        return Node(e._item.second(), true);
      }
    }

    Node target(const Arc& e) const {
      if (e._item.firstState()) {
        return Node(_digraph->target(e._item.first()), true);
      } else {
        return Node(e._item.second(), false);
      }
    }

    int id(const Node& n) const {
      return (_digraph->id(n) << 1) | (n._in ? 0 : 1);
    }
    Node nodeFromId(int ix) const {
      return Node(_digraph->nodeFromId(ix >> 1), (ix & 1) == 0);
    }
    int maxNodeId() const {
      return 2 * _digraph->maxNodeId() + 1;
    }

    int id(const Arc& e) const {
      if (e._item.firstState()) {
        return _digraph->id(e._item.first()) << 1;
      } else {
        return (_digraph->id(e._item.second()) << 1) | 1;
      }
    }
    Arc arcFromId(int ix) const {
      if ((ix & 1) == 0) {
        return Arc(_digraph->arcFromId(ix >> 1));
      } else {
        return Arc(_digraph->nodeFromId(ix >> 1));
      }
    }
    int maxArcId() const {
      return std::max(_digraph->maxNodeId() << 1,
                      (_digraph->maxArcId() << 1) | 1);
    }

    static bool inNode(const Node& n) {
      return n._in;
    }

    static bool outNode(const Node& n) {
      return !n._in;
    }

    static bool origArc(const Arc& e) {
      return e._item.firstState();
    }

    static bool bindArc(const Arc& e) {
      return e._item.secondState();
    }

    static Node inNode(const DigraphNode& n) {
      return Node(n, true);
    }

    static Node outNode(const DigraphNode& n) {
      return Node(n, false);
    }

    static Arc arc(const DigraphNode& n) {
      return Arc(n);
    }

    static Arc arc(const DigraphArc& e) {
      return Arc(e);
    }

    typedef True NodeNumTag;
    int nodeNum() const {
      return  2 * countNodes(*_digraph);
    }

    typedef True ArcNumTag;
    int arcNum() const {
      return countArcs(*_digraph) + countNodes(*_digraph);
    }

    typedef True FindArcTag;
    Arc findArc(const Node& u, const Node& v,
                const Arc& prev = INVALID) const {
      if (inNode(u) && outNode(v)) {
        if (static_cast<const DigraphNode&>(u) ==
            static_cast<const DigraphNode&>(v) && prev == INVALID) {
          return Arc(u);
        }
      }
      else if (outNode(u) && inNode(v)) {
        return Arc(::lemon::findArc(*_digraph, u, v, prev));
      }
      return INVALID;
    }

  private:

    template <typename V>
    class NodeMapBase
      : public MapTraits<typename Parent::template NodeMap<V> > {
      typedef typename Parent::template NodeMap<V> NodeImpl;
    public:
      typedef Node Key;
      typedef V Value;
      typedef typename MapTraits<NodeImpl>::ReferenceMapTag ReferenceMapTag;
      typedef typename MapTraits<NodeImpl>::ReturnValue ReturnValue;
      typedef typename MapTraits<NodeImpl>::ConstReturnValue ConstReturnValue;
      typedef typename MapTraits<NodeImpl>::ReturnValue Reference;
      typedef typename MapTraits<NodeImpl>::ConstReturnValue ConstReference;

      NodeMapBase(const SplitNodesBase<DGR>& adaptor)
        : _in_map(*adaptor._digraph), _out_map(*adaptor._digraph) {}
      NodeMapBase(const SplitNodesBase<DGR>& adaptor, const V& value)
        : _in_map(*adaptor._digraph, value),
          _out_map(*adaptor._digraph, value) {}

      void set(const Node& key, const V& val) {
        if (SplitNodesBase<DGR>::inNode(key)) { _in_map.set(key, val); }
        else {_out_map.set(key, val); }
      }

      ReturnValue operator[](const Node& key) {
        if (SplitNodesBase<DGR>::inNode(key)) { return _in_map[key]; }
        else { return _out_map[key]; }
      }

      ConstReturnValue operator[](const Node& key) const {
        if (Adaptor::inNode(key)) { return _in_map[key]; }
        else { return _out_map[key]; }
      }

    private:
      NodeImpl _in_map, _out_map;
    };

    template <typename V>
    class ArcMapBase
      : public MapTraits<typename Parent::template ArcMap<V> > {
      typedef typename Parent::template ArcMap<V> ArcImpl;
      typedef typename Parent::template NodeMap<V> NodeImpl;
    public:
      typedef Arc Key;
      typedef V Value;
      typedef typename MapTraits<ArcImpl>::ReferenceMapTag ReferenceMapTag;
      typedef typename MapTraits<ArcImpl>::ReturnValue ReturnValue;
      typedef typename MapTraits<ArcImpl>::ConstReturnValue ConstReturnValue;
      typedef typename MapTraits<ArcImpl>::ReturnValue Reference;
      typedef typename MapTraits<ArcImpl>::ConstReturnValue ConstReference;

      ArcMapBase(const SplitNodesBase<DGR>& adaptor)
        : _arc_map(*adaptor._digraph), _node_map(*adaptor._digraph) {}
      ArcMapBase(const SplitNodesBase<DGR>& adaptor, const V& value)
        : _arc_map(*adaptor._digraph, value),
          _node_map(*adaptor._digraph, value) {}

      void set(const Arc& key, const V& val) {
        if (SplitNodesBase<DGR>::origArc(key)) {
          _arc_map.set(static_cast<const DigraphArc&>(key), val);
        } else {
          _node_map.set(static_cast<const DigraphNode&>(key), val);
        }
      }

      ReturnValue operator[](const Arc& key) {
        if (SplitNodesBase<DGR>::origArc(key)) {
          return _arc_map[static_cast<const DigraphArc&>(key)];
        } else {
          return _node_map[static_cast<const DigraphNode&>(key)];
        }
      }

      ConstReturnValue operator[](const Arc& key) const {
        if (SplitNodesBase<DGR>::origArc(key)) {
          return _arc_map[static_cast<const DigraphArc&>(key)];
        } else {
          return _node_map[static_cast<const DigraphNode&>(key)];
        }
      }

    private:
      ArcImpl _arc_map;
      NodeImpl _node_map;
    };

  public:

    template <typename V>
    class NodeMap
      : public SubMapExtender<SplitNodesBase<DGR>, NodeMapBase<V> > {
      typedef SubMapExtender<SplitNodesBase<DGR>, NodeMapBase<V> > Parent;

    public:
      typedef V Value;

      NodeMap(const SplitNodesBase<DGR>& adaptor)
        : Parent(adaptor) {}

      NodeMap(const SplitNodesBase<DGR>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

    template <typename V>
    class ArcMap
      : public SubMapExtender<SplitNodesBase<DGR>, ArcMapBase<V> > {
      typedef SubMapExtender<SplitNodesBase<DGR>, ArcMapBase<V> > Parent;

    public:
      typedef V Value;

      ArcMap(const SplitNodesBase<DGR>& adaptor)
        : Parent(adaptor) {}

      ArcMap(const SplitNodesBase<DGR>& adaptor, const V& value)
        : Parent(adaptor, value) {}

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

  protected:

    SplitNodesBase() : _digraph(0) {}

    DGR* _digraph;

    void initialize(Digraph& digraph) {
      _digraph = &digraph;
    }

  };

  /// \ingroup graph_adaptors
  ///
  /// \brief Adaptor class for splitting the nodes of a digraph.
  ///
  /// SplitNodes adaptor can be used for splitting each node into an
  /// \e in-node and an \e out-node in a digraph. Formaly, the adaptor
  /// replaces each node \f$ u \f$ in the digraph with two nodes,
  /// namely node \f$ u_{in} \f$ and node \f$ u_{out} \f$.
  /// If there is a \f$ (v, u) \f$ arc in the original digraph, then the
  /// new target of the arc will be \f$ u_{in} \f$ and similarly the
  /// source of each original \f$ (u, v) \f$ arc will be \f$ u_{out} \f$.
  /// The adaptor adds an additional \e bind \e arc from \f$ u_{in} \f$
  /// to \f$ u_{out} \f$ for each node \f$ u \f$ of the original digraph.
  ///
  /// The aim of this class is running an algorithm with respect to node
  /// costs or capacities if the algorithm considers only arc costs or
  /// capacities directly.
  /// In this case you can use \c SplitNodes adaptor, and set the node
  /// costs/capacities of the original digraph to the \e bind \e arcs
  /// in the adaptor.
  ///
  /// This class provides item counting in the same time as the adapted
  /// digraph structure.
  ///
  /// \tparam DGR The type of the adapted digraph.
  /// It must conform to the \ref concepts::Digraph "Digraph" concept.
  /// It is implicitly \c const.
  ///
  /// \note The \c Node type of this adaptor is converible to the \c Node
  /// type of the adapted digraph.
  template <typename DGR>
#ifdef DOXYGEN
  class SplitNodes {
#else
  class SplitNodes
    : public DigraphAdaptorExtender<SplitNodesBase<const DGR> > {
#endif
    typedef DigraphAdaptorExtender<SplitNodesBase<const DGR> > Parent;

  public:
    typedef DGR Digraph;

    typedef typename DGR::Node DigraphNode;
    typedef typename DGR::Arc DigraphArc;

    typedef typename Parent::Node Node;
    typedef typename Parent::Arc Arc;

    /// \brief Constructor
    ///
    /// Constructor of the adaptor.
    SplitNodes(const DGR& g) {
      Parent::initialize(g);
    }

    /// \brief Returns \c true if the given node is an in-node.
    ///
    /// Returns \c true if the given node is an in-node.
    static bool inNode(const Node& n) {
      return Parent::inNode(n);
    }

    /// \brief Returns \c true if the given node is an out-node.
    ///
    /// Returns \c true if the given node is an out-node.
    static bool outNode(const Node& n) {
      return Parent::outNode(n);
    }

    /// \brief Returns \c true if the given arc is an original arc.
    ///
    /// Returns \c true if the given arc is one of the arcs in the
    /// original digraph.
    static bool origArc(const Arc& a) {
      return Parent::origArc(a);
    }

    /// \brief Returns \c true if the given arc is a bind arc.
    ///
    /// Returns \c true if the given arc is a bind arc, i.e. it connects
    /// an in-node and an out-node.
    static bool bindArc(const Arc& a) {
      return Parent::bindArc(a);
    }

    /// \brief Returns the in-node created from the given original node.
    ///
    /// Returns the in-node created from the given original node.
    static Node inNode(const DigraphNode& n) {
      return Parent::inNode(n);
    }

    /// \brief Returns the out-node created from the given original node.
    ///
    /// Returns the out-node created from the given original node.
    static Node outNode(const DigraphNode& n) {
      return Parent::outNode(n);
    }

    /// \brief Returns the bind arc that corresponds to the given
    /// original node.
    ///
    /// Returns the bind arc in the adaptor that corresponds to the given
    /// original node, i.e. the arc connecting the in-node and out-node
    /// of \c n.
    static Arc arc(const DigraphNode& n) {
      return Parent::arc(n);
    }

    /// \brief Returns the arc that corresponds to the given original arc.
    ///
    /// Returns the arc in the adaptor that corresponds to the given
    /// original arc.
    static Arc arc(const DigraphArc& a) {
      return Parent::arc(a);
    }

    /// \brief Node map combined from two original node maps
    ///
    /// This map adaptor class adapts two node maps of the original digraph
    /// to get a node map of the split digraph.
    /// Its value type is inherited from the first node map type (\c IN).
    /// \tparam IN The type of the node map for the in-nodes.
    /// \tparam OUT The type of the node map for the out-nodes.
    template <typename IN, typename OUT>
    class CombinedNodeMap {
    public:

      /// The key type of the map
      typedef Node Key;
      /// The value type of the map
      typedef typename IN::Value Value;

      typedef typename MapTraits<IN>::ReferenceMapTag ReferenceMapTag;
      typedef typename MapTraits<IN>::ReturnValue ReturnValue;
      typedef typename MapTraits<IN>::ConstReturnValue ConstReturnValue;
      typedef typename MapTraits<IN>::ReturnValue Reference;
      typedef typename MapTraits<IN>::ConstReturnValue ConstReference;

      /// Constructor
      CombinedNodeMap(IN& in_map, OUT& out_map)
        : _in_map(in_map), _out_map(out_map) {}

      /// Returns the value associated with the given key.
      Value operator[](const Key& key) const {
        if (SplitNodesBase<const DGR>::inNode(key)) {
          return _in_map[key];
        } else {
          return _out_map[key];
        }
      }

      /// Returns a reference to the value associated with the given key.
      Value& operator[](const Key& key) {
        if (SplitNodesBase<const DGR>::inNode(key)) {
          return _in_map[key];
        } else {
          return _out_map[key];
        }
      }

      /// Sets the value associated with the given key.
      void set(const Key& key, const Value& value) {
        if (SplitNodesBase<const DGR>::inNode(key)) {
          _in_map.set(key, value);
        } else {
          _out_map.set(key, value);
        }
      }

    private:

      IN& _in_map;
      OUT& _out_map;

    };


    /// \brief Returns a combined node map
    ///
    /// This function just returns a combined node map.
    template <typename IN, typename OUT>
    static CombinedNodeMap<IN, OUT>
    combinedNodeMap(IN& in_map, OUT& out_map) {
      return CombinedNodeMap<IN, OUT>(in_map, out_map);
    }

    template <typename IN, typename OUT>
    static CombinedNodeMap<const IN, OUT>
    combinedNodeMap(const IN& in_map, OUT& out_map) {
      return CombinedNodeMap<const IN, OUT>(in_map, out_map);
    }

    template <typename IN, typename OUT>
    static CombinedNodeMap<IN, const OUT>
    combinedNodeMap(IN& in_map, const OUT& out_map) {
      return CombinedNodeMap<IN, const OUT>(in_map, out_map);
    }

    template <typename IN, typename OUT>
    static CombinedNodeMap<const IN, const OUT>
    combinedNodeMap(const IN& in_map, const OUT& out_map) {
      return CombinedNodeMap<const IN, const OUT>(in_map, out_map);
    }

    /// \brief Arc map combined from an arc map and a node map of the
    /// original digraph.
    ///
    /// This map adaptor class adapts an arc map and a node map of the
    /// original digraph to get an arc map of the split digraph.
    /// Its value type is inherited from the original arc map type (\c AM).
    /// \tparam AM The type of the arc map.
    /// \tparam NM the type of the node map.
    template <typename AM, typename NM>
    class CombinedArcMap {
    public:

      /// The key type of the map
      typedef Arc Key;
      /// The value type of the map
      typedef typename AM::Value Value;

      typedef typename MapTraits<AM>::ReferenceMapTag ReferenceMapTag;
      typedef typename MapTraits<AM>::ReturnValue ReturnValue;
      typedef typename MapTraits<AM>::ConstReturnValue ConstReturnValue;
      typedef typename MapTraits<AM>::ReturnValue Reference;
      typedef typename MapTraits<AM>::ConstReturnValue ConstReference;

      /// Constructor
      CombinedArcMap(AM& arc_map, NM& node_map)
        : _arc_map(arc_map), _node_map(node_map) {}

      /// Returns the value associated with the given key.
      Value operator[](const Key& arc) const {
        if (SplitNodesBase<const DGR>::origArc(arc)) {
          return _arc_map[arc];
        } else {
          return _node_map[arc];
        }
      }

      /// Returns a reference to the value associated with the given key.
      Value& operator[](const Key& arc) {
        if (SplitNodesBase<const DGR>::origArc(arc)) {
          return _arc_map[arc];
        } else {
          return _node_map[arc];
        }
      }

      /// Sets the value associated with the given key.
      void set(const Arc& arc, const Value& val) {
        if (SplitNodesBase<const DGR>::origArc(arc)) {
          _arc_map.set(arc, val);
        } else {
          _node_map.set(arc, val);
        }
      }

    private:

      AM& _arc_map;
      NM& _node_map;

    };

    /// \brief Returns a combined arc map
    ///
    /// This function just returns a combined arc map.
    template <typename ArcMap, typename NodeMap>
    static CombinedArcMap<ArcMap, NodeMap>
    combinedArcMap(ArcMap& arc_map, NodeMap& node_map) {
      return CombinedArcMap<ArcMap, NodeMap>(arc_map, node_map);
    }

    template <typename ArcMap, typename NodeMap>
    static CombinedArcMap<const ArcMap, NodeMap>
    combinedArcMap(const ArcMap& arc_map, NodeMap& node_map) {
      return CombinedArcMap<const ArcMap, NodeMap>(arc_map, node_map);
    }

    template <typename ArcMap, typename NodeMap>
    static CombinedArcMap<ArcMap, const NodeMap>
    combinedArcMap(ArcMap& arc_map, const NodeMap& node_map) {
      return CombinedArcMap<ArcMap, const NodeMap>(arc_map, node_map);
    }

    template <typename ArcMap, typename NodeMap>
    static CombinedArcMap<const ArcMap, const NodeMap>
    combinedArcMap(const ArcMap& arc_map, const NodeMap& node_map) {
      return CombinedArcMap<const ArcMap, const NodeMap>(arc_map, node_map);
    }

  };

  /// \brief Returns a (read-only) SplitNodes adaptor
  ///
  /// This function just returns a (read-only) \ref SplitNodes adaptor.
  /// \ingroup graph_adaptors
  /// \relates SplitNodes
  template<typename DGR>
  SplitNodes<DGR>
  splitNodes(const DGR& digraph) {
    return SplitNodes<DGR>(digraph);
  }

#undef LEMON_SCOPE_FIX

} //namespace lemon

#endif //LEMON_ADAPTORS_H
