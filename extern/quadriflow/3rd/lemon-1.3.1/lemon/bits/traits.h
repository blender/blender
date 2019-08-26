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

#ifndef LEMON_BITS_TRAITS_H
#define LEMON_BITS_TRAITS_H

//\file
//\brief Traits for graphs and maps
//

#include <lemon/bits/enable_if.h>

namespace lemon {

  struct InvalidType {};

  template <typename GR, typename _Item>
  class ItemSetTraits {};


  template <typename GR, typename Enable = void>
  struct NodeNotifierIndicator {
    typedef InvalidType Type;
  };
  template <typename GR>
  struct NodeNotifierIndicator<
    GR,
    typename enable_if<typename GR::NodeNotifier::Notifier, void>::type
  > {
    typedef typename GR::NodeNotifier Type;
  };

  template <typename GR>
  class ItemSetTraits<GR, typename GR::Node> {
  public:

    typedef GR Graph;
    typedef GR Digraph;

    typedef typename GR::Node Item;
    typedef typename GR::NodeIt ItemIt;

    typedef typename NodeNotifierIndicator<GR>::Type ItemNotifier;

    template <typename V>
    class Map : public GR::template NodeMap<V> {
      typedef typename GR::template NodeMap<V> Parent;

    public:
      typedef typename GR::template NodeMap<V> Type;
      typedef typename Parent::Value Value;

      Map(const GR& _digraph) : Parent(_digraph) {}
      Map(const GR& _digraph, const Value& _value)
        : Parent(_digraph, _value) {}

     };

  };

  template <typename GR, typename Enable = void>
  struct ArcNotifierIndicator {
    typedef InvalidType Type;
  };
  template <typename GR>
  struct ArcNotifierIndicator<
    GR,
    typename enable_if<typename GR::ArcNotifier::Notifier, void>::type
  > {
    typedef typename GR::ArcNotifier Type;
  };

  template <typename GR>
  class ItemSetTraits<GR, typename GR::Arc> {
  public:

    typedef GR Graph;
    typedef GR Digraph;

    typedef typename GR::Arc Item;
    typedef typename GR::ArcIt ItemIt;

    typedef typename ArcNotifierIndicator<GR>::Type ItemNotifier;

    template <typename V>
    class Map : public GR::template ArcMap<V> {
      typedef typename GR::template ArcMap<V> Parent;

    public:
      typedef typename GR::template ArcMap<V> Type;
      typedef typename Parent::Value Value;

      Map(const GR& _digraph) : Parent(_digraph) {}
      Map(const GR& _digraph, const Value& _value)
        : Parent(_digraph, _value) {}
    };

  };

  template <typename GR, typename Enable = void>
  struct EdgeNotifierIndicator {
    typedef InvalidType Type;
  };
  template <typename GR>
  struct EdgeNotifierIndicator<
    GR,
    typename enable_if<typename GR::EdgeNotifier::Notifier, void>::type
  > {
    typedef typename GR::EdgeNotifier Type;
  };

  template <typename GR>
  class ItemSetTraits<GR, typename GR::Edge> {
  public:

    typedef GR Graph;
    typedef GR Digraph;

    typedef typename GR::Edge Item;
    typedef typename GR::EdgeIt ItemIt;

    typedef typename EdgeNotifierIndicator<GR>::Type ItemNotifier;

    template <typename V>
    class Map : public GR::template EdgeMap<V> {
      typedef typename GR::template EdgeMap<V> Parent;

    public:
      typedef typename GR::template EdgeMap<V> Type;
      typedef typename Parent::Value Value;

      Map(const GR& _digraph) : Parent(_digraph) {}
      Map(const GR& _digraph, const Value& _value)
        : Parent(_digraph, _value) {}
    };

  };

  template <typename GR, typename Enable = void>
  struct RedNodeNotifierIndicator {
    typedef InvalidType Type;
  };
  template <typename GR>
  struct RedNodeNotifierIndicator<
    GR,
    typename enable_if<typename GR::RedNodeNotifier::Notifier, void>::type
  > {
    typedef typename GR::RedNodeNotifier Type;
  };

  template <typename GR>
  class ItemSetTraits<GR, typename GR::RedNode> {
  public:

    typedef GR BpGraph;
    typedef GR Graph;
    typedef GR Digraph;

    typedef typename GR::RedNode Item;
    typedef typename GR::RedNodeIt ItemIt;

    typedef typename RedNodeNotifierIndicator<GR>::Type ItemNotifier;

    template <typename V>
    class Map : public GR::template RedNodeMap<V> {
      typedef typename GR::template RedNodeMap<V> Parent;

    public:
      typedef typename GR::template RedNodeMap<V> Type;
      typedef typename Parent::Value Value;

      Map(const GR& _bpgraph) : Parent(_bpgraph) {}
      Map(const GR& _bpgraph, const Value& _value)
        : Parent(_bpgraph, _value) {}

     };

  };

  template <typename GR, typename Enable = void>
  struct BlueNodeNotifierIndicator {
    typedef InvalidType Type;
  };
  template <typename GR>
  struct BlueNodeNotifierIndicator<
    GR,
    typename enable_if<typename GR::BlueNodeNotifier::Notifier, void>::type
  > {
    typedef typename GR::BlueNodeNotifier Type;
  };

  template <typename GR>
  class ItemSetTraits<GR, typename GR::BlueNode> {
  public:

    typedef GR BpGraph;
    typedef GR Graph;
    typedef GR Digraph;

    typedef typename GR::BlueNode Item;
    typedef typename GR::BlueNodeIt ItemIt;

    typedef typename BlueNodeNotifierIndicator<GR>::Type ItemNotifier;

    template <typename V>
    class Map : public GR::template BlueNodeMap<V> {
      typedef typename GR::template BlueNodeMap<V> Parent;

    public:
      typedef typename GR::template BlueNodeMap<V> Type;
      typedef typename Parent::Value Value;

      Map(const GR& _bpgraph) : Parent(_bpgraph) {}
      Map(const GR& _bpgraph, const Value& _value)
        : Parent(_bpgraph, _value) {}

     };

  };

  template <typename Map, typename Enable = void>
  struct MapTraits {
    typedef False ReferenceMapTag;

    typedef typename Map::Key Key;
    typedef typename Map::Value Value;

    typedef Value ConstReturnValue;
    typedef Value ReturnValue;
  };

  template <typename Map>
  struct MapTraits<
    Map, typename enable_if<typename Map::ReferenceMapTag, void>::type >
  {
    typedef True ReferenceMapTag;

    typedef typename Map::Key Key;
    typedef typename Map::Value Value;

    typedef typename Map::ConstReference ConstReturnValue;
    typedef typename Map::Reference ReturnValue;

    typedef typename Map::ConstReference ConstReference;
    typedef typename Map::Reference Reference;
 };

  template <typename MatrixMap, typename Enable = void>
  struct MatrixMapTraits {
    typedef False ReferenceMapTag;

    typedef typename MatrixMap::FirstKey FirstKey;
    typedef typename MatrixMap::SecondKey SecondKey;
    typedef typename MatrixMap::Value Value;

    typedef Value ConstReturnValue;
    typedef Value ReturnValue;
  };

  template <typename MatrixMap>
  struct MatrixMapTraits<
    MatrixMap, typename enable_if<typename MatrixMap::ReferenceMapTag,
                                  void>::type >
  {
    typedef True ReferenceMapTag;

    typedef typename MatrixMap::FirstKey FirstKey;
    typedef typename MatrixMap::SecondKey SecondKey;
    typedef typename MatrixMap::Value Value;

    typedef typename MatrixMap::ConstReference ConstReturnValue;
    typedef typename MatrixMap::Reference ReturnValue;

    typedef typename MatrixMap::ConstReference ConstReference;
    typedef typename MatrixMap::Reference Reference;
 };

  // Indicators for the tags

  template <typename GR, typename Enable = void>
  struct NodeNumTagIndicator {
    static const bool value = false;
  };

  template <typename GR>
  struct NodeNumTagIndicator<
    GR,
    typename enable_if<typename GR::NodeNumTag, void>::type
  > {
    static const bool value = true;
  };

  template <typename GR, typename Enable = void>
  struct ArcNumTagIndicator {
    static const bool value = false;
  };

  template <typename GR>
  struct ArcNumTagIndicator<
    GR,
    typename enable_if<typename GR::ArcNumTag, void>::type
  > {
    static const bool value = true;
  };

  template <typename GR, typename Enable = void>
  struct EdgeNumTagIndicator {
    static const bool value = false;
  };

  template <typename GR>
  struct EdgeNumTagIndicator<
    GR,
    typename enable_if<typename GR::EdgeNumTag, void>::type
  > {
    static const bool value = true;
  };

  template <typename GR, typename Enable = void>
  struct FindArcTagIndicator {
    static const bool value = false;
  };

  template <typename GR>
  struct FindArcTagIndicator<
    GR,
    typename enable_if<typename GR::FindArcTag, void>::type
  > {
    static const bool value = true;
  };

  template <typename GR, typename Enable = void>
  struct FindEdgeTagIndicator {
    static const bool value = false;
  };

  template <typename GR>
  struct FindEdgeTagIndicator<
    GR,
    typename enable_if<typename GR::FindEdgeTag, void>::type
  > {
    static const bool value = true;
  };

  template <typename GR, typename Enable = void>
  struct UndirectedTagIndicator {
    static const bool value = false;
  };

  template <typename GR>
  struct UndirectedTagIndicator<
    GR,
    typename enable_if<typename GR::UndirectedTag, void>::type
  > {
    static const bool value = true;
  };

  template <typename GR, typename Enable = void>
  struct BuildTagIndicator {
    static const bool value = false;
  };

  template <typename GR>
  struct BuildTagIndicator<
    GR,
    typename enable_if<typename GR::BuildTag, void>::type
  > {
    static const bool value = true;
  };

}

#endif
