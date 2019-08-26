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

#ifndef LEMON_KRUSKAL_H
#define LEMON_KRUSKAL_H

#include <algorithm>
#include <vector>
#include <lemon/unionfind.h>
#include <lemon/maps.h>

#include <lemon/core.h>
#include <lemon/bits/traits.h>

///\ingroup spantree
///\file
///\brief Kruskal's algorithm to compute a minimum cost spanning tree

namespace lemon {

  namespace _kruskal_bits {

    // Kruskal for directed graphs.

    template <typename Digraph, typename In, typename Out>
    typename disable_if<lemon::UndirectedTagIndicator<Digraph>,
                       typename In::value_type::second_type >::type
    kruskal(const Digraph& digraph, const In& in, Out& out,dummy<0> = 0) {
      typedef typename In::value_type::second_type Value;
      typedef typename Digraph::template NodeMap<int> IndexMap;
      typedef typename Digraph::Node Node;

      IndexMap index(digraph);
      UnionFind<IndexMap> uf(index);
      for (typename Digraph::NodeIt it(digraph); it != INVALID; ++it) {
        uf.insert(it);
      }

      Value tree_value = 0;
      for (typename In::const_iterator it = in.begin(); it != in.end(); ++it) {
        if (uf.join(digraph.target(it->first),digraph.source(it->first))) {
          out.set(it->first, true);
          tree_value += it->second;
        }
        else {
          out.set(it->first, false);
        }
      }
      return tree_value;
    }

    // Kruskal for undirected graphs.

    template <typename Graph, typename In, typename Out>
    typename enable_if<lemon::UndirectedTagIndicator<Graph>,
                       typename In::value_type::second_type >::type
    kruskal(const Graph& graph, const In& in, Out& out,dummy<1> = 1) {
      typedef typename In::value_type::second_type Value;
      typedef typename Graph::template NodeMap<int> IndexMap;
      typedef typename Graph::Node Node;

      IndexMap index(graph);
      UnionFind<IndexMap> uf(index);
      for (typename Graph::NodeIt it(graph); it != INVALID; ++it) {
        uf.insert(it);
      }

      Value tree_value = 0;
      for (typename In::const_iterator it = in.begin(); it != in.end(); ++it) {
        if (uf.join(graph.u(it->first),graph.v(it->first))) {
          out.set(it->first, true);
          tree_value += it->second;
        }
        else {
          out.set(it->first, false);
        }
      }
      return tree_value;
    }


    template <typename Sequence>
    struct PairComp {
      typedef typename Sequence::value_type Value;
      bool operator()(const Value& left, const Value& right) {
        return left.second < right.second;
      }
    };

    template <typename In, typename Enable = void>
    struct SequenceInputIndicator {
      static const bool value = false;
    };

    template <typename In>
    struct SequenceInputIndicator<In,
      typename exists<typename In::value_type::first_type>::type> {
      static const bool value = true;
    };

    template <typename In, typename Enable = void>
    struct MapInputIndicator {
      static const bool value = false;
    };

    template <typename In>
    struct MapInputIndicator<In,
      typename exists<typename In::Value>::type> {
      static const bool value = true;
    };

    template <typename In, typename Enable = void>
    struct SequenceOutputIndicator {
      static const bool value = false;
    };

    template <typename Out>
    struct SequenceOutputIndicator<Out,
      typename exists<typename Out::value_type>::type> {
      static const bool value = true;
    };

    template <typename Out, typename Enable = void>
    struct MapOutputIndicator {
      static const bool value = false;
    };

    template <typename Out>
    struct MapOutputIndicator<Out,
      typename exists<typename Out::Value>::type> {
      static const bool value = true;
    };

    template <typename In, typename InEnable = void>
    struct KruskalValueSelector {};

    template <typename In>
    struct KruskalValueSelector<In,
      typename enable_if<SequenceInputIndicator<In>, void>::type>
    {
      typedef typename In::value_type::second_type Value;
    };

    template <typename In>
    struct KruskalValueSelector<In,
      typename enable_if<MapInputIndicator<In>, void>::type>
    {
      typedef typename In::Value Value;
    };

    template <typename Graph, typename In, typename Out,
              typename InEnable = void>
    struct KruskalInputSelector {};

    template <typename Graph, typename In, typename Out,
              typename InEnable = void>
    struct KruskalOutputSelector {};

    template <typename Graph, typename In, typename Out>
    struct KruskalInputSelector<Graph, In, Out,
      typename enable_if<SequenceInputIndicator<In>, void>::type >
    {
      typedef typename In::value_type::second_type Value;

      static Value kruskal(const Graph& graph, const In& in, Out& out) {
        return KruskalOutputSelector<Graph, In, Out>::
          kruskal(graph, in, out);
      }

    };

    template <typename Graph, typename In, typename Out>
    struct KruskalInputSelector<Graph, In, Out,
      typename enable_if<MapInputIndicator<In>, void>::type >
    {
      typedef typename In::Value Value;
      static Value kruskal(const Graph& graph, const In& in, Out& out) {
        typedef typename In::Key MapArc;
        typedef typename In::Value Value;
        typedef typename ItemSetTraits<Graph, MapArc>::ItemIt MapArcIt;
        typedef std::vector<std::pair<MapArc, Value> > Sequence;
        Sequence seq;

        for (MapArcIt it(graph); it != INVALID; ++it) {
          seq.push_back(std::make_pair(it, in[it]));
        }

        std::sort(seq.begin(), seq.end(), PairComp<Sequence>());
        return KruskalOutputSelector<Graph, Sequence, Out>::
          kruskal(graph, seq, out);
      }
    };

    template <typename T>
    struct RemoveConst {
      typedef T type;
    };

    template <typename T>
    struct RemoveConst<const T> {
      typedef T type;
    };

    template <typename Graph, typename In, typename Out>
    struct KruskalOutputSelector<Graph, In, Out,
      typename enable_if<SequenceOutputIndicator<Out>, void>::type >
    {
      typedef typename In::value_type::second_type Value;

      static Value kruskal(const Graph& graph, const In& in, Out& out) {
        typedef LoggerBoolMap<typename RemoveConst<Out>::type> Map;
        Map map(out);
        return _kruskal_bits::kruskal(graph, in, map);
      }

    };

    template <typename Graph, typename In, typename Out>
    struct KruskalOutputSelector<Graph, In, Out,
      typename enable_if<MapOutputIndicator<Out>, void>::type >
    {
      typedef typename In::value_type::second_type Value;

      static Value kruskal(const Graph& graph, const In& in, Out& out) {
        return _kruskal_bits::kruskal(graph, in, out);
      }
    };

  }

  /// \ingroup spantree
  ///
  /// \brief Kruskal's algorithm for finding a minimum cost spanning tree of
  /// a graph.
  ///
  /// This function runs Kruskal's algorithm to find a minimum cost
  /// spanning tree of a graph.
  /// Due to some C++ hacking, it accepts various input and output types.
  ///
  /// \param g The graph the algorithm runs on.
  /// It can be either \ref concepts::Digraph "directed" or
  /// \ref concepts::Graph "undirected".
  /// If the graph is directed, the algorithm consider it to be
  /// undirected by disregarding the direction of the arcs.
  ///
  /// \param in This object is used to describe the arc/edge costs.
  /// It can be one of the following choices.
  /// - An STL compatible 'Forward Container' with
  /// <tt>std::pair<GR::Arc,C></tt> or
  /// <tt>std::pair<GR::Edge,C></tt> as its <tt>value_type</tt>, where
  /// \c C is the type of the costs. The pairs indicates the arcs/edges
  /// along with the assigned cost. <em>They must be in a
  /// cost-ascending order.</em>
  /// - Any readable arc/edge map. The values of the map indicate the
  /// arc/edge costs.
  ///
  /// \retval out Here we also have a choice.
  /// - It can be a writable arc/edge map with \c bool value type. After
  /// running the algorithm it will contain the found minimum cost spanning
  /// tree: the value of an arc/edge will be set to \c true if it belongs
  /// to the tree, otherwise it will be set to \c false. The value of
  /// each arc/edge will be set exactly once.
  /// - It can also be an iteraror of an STL Container with
  /// <tt>GR::Arc</tt> or <tt>GR::Edge</tt> as its
  /// <tt>value_type</tt>.  The algorithm copies the elements of the
  /// found tree into this sequence.  For example, if we know that the
  /// spanning tree of the graph \c g has say 53 arcs, then we can
  /// put its arcs into an STL vector \c tree with a code like this.
  ///\code
  /// std::vector<Arc> tree(53);
  /// kruskal(g,cost,tree.begin());
  ///\endcode
  /// Or if we don't know in advance the size of the tree, we can
  /// write this.
  ///\code
  /// std::vector<Arc> tree;
  /// kruskal(g,cost,std::back_inserter(tree));
  ///\endcode
  ///
  /// \return The total cost of the found spanning tree.
  ///
  /// \note If the input graph is not (weakly) connected, a spanning
  /// forest is calculated instead of a spanning tree.

#ifdef DOXYGEN
  template <typename Graph, typename In, typename Out>
  Value kruskal(const Graph& g, const In& in, Out& out)
#else
  template <class Graph, class In, class Out>
  inline typename _kruskal_bits::KruskalValueSelector<In>::Value
  kruskal(const Graph& graph, const In& in, Out& out)
#endif
  {
    return _kruskal_bits::KruskalInputSelector<Graph, In, Out>::
      kruskal(graph, in, out);
  }


  template <class Graph, class In, class Out>
  inline typename _kruskal_bits::KruskalValueSelector<In>::Value
  kruskal(const Graph& graph, const In& in, const Out& out)
  {
    return _kruskal_bits::KruskalInputSelector<Graph, In, const Out>::
      kruskal(graph, in, out);
  }

} //namespace lemon

#endif //LEMON_KRUSKAL_H
