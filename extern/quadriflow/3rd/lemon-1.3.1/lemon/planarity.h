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

#ifndef LEMON_PLANARITY_H
#define LEMON_PLANARITY_H

/// \ingroup planar
/// \file
/// \brief Planarity checking, embedding, drawing and coloring

#include <vector>
#include <list>

#include <lemon/dfs.h>
#include <lemon/bfs.h>
#include <lemon/radix_sort.h>
#include <lemon/maps.h>
#include <lemon/path.h>
#include <lemon/bucket_heap.h>
#include <lemon/adaptors.h>
#include <lemon/edge_set.h>
#include <lemon/color.h>
#include <lemon/dim2.h>

namespace lemon {

  namespace _planarity_bits {

    template <typename Graph>
    struct PlanarityVisitor : DfsVisitor<Graph> {

      TEMPLATE_GRAPH_TYPEDEFS(Graph);

      typedef typename Graph::template NodeMap<Arc> PredMap;

      typedef typename Graph::template EdgeMap<bool> TreeMap;

      typedef typename Graph::template NodeMap<int> OrderMap;
      typedef std::vector<Node> OrderList;

      typedef typename Graph::template NodeMap<int> LowMap;
      typedef typename Graph::template NodeMap<int> AncestorMap;

      PlanarityVisitor(const Graph& graph,
                       PredMap& pred_map, TreeMap& tree_map,
                       OrderMap& order_map, OrderList& order_list,
                       AncestorMap& ancestor_map, LowMap& low_map)
        : _graph(graph), _pred_map(pred_map), _tree_map(tree_map),
          _order_map(order_map), _order_list(order_list),
          _ancestor_map(ancestor_map), _low_map(low_map) {}

      void reach(const Node& node) {
        _order_map[node] = _order_list.size();
        _low_map[node] = _order_list.size();
        _ancestor_map[node] = _order_list.size();
        _order_list.push_back(node);
      }

      void discover(const Arc& arc) {
        Node target = _graph.target(arc);

        _tree_map[arc] = true;
        _pred_map[target] = arc;
      }

      void examine(const Arc& arc) {
        Node source = _graph.source(arc);
        Node target = _graph.target(arc);

        if (_order_map[target] < _order_map[source] && !_tree_map[arc]) {
          if (_low_map[source] > _order_map[target]) {
            _low_map[source] = _order_map[target];
          }
          if (_ancestor_map[source] > _order_map[target]) {
            _ancestor_map[source] = _order_map[target];
          }
        }
      }

      void backtrack(const Arc& arc) {
        Node source = _graph.source(arc);
        Node target = _graph.target(arc);

        if (_low_map[source] > _low_map[target]) {
          _low_map[source] = _low_map[target];
        }
      }

      const Graph& _graph;
      PredMap& _pred_map;
      TreeMap& _tree_map;
      OrderMap& _order_map;
      OrderList& _order_list;
      AncestorMap& _ancestor_map;
      LowMap& _low_map;
    };

    template <typename Graph, bool embedding = true>
    struct NodeDataNode {
      int prev, next;
      int visited;
      typename Graph::Arc first;
      bool inverted;
    };

    template <typename Graph>
    struct NodeDataNode<Graph, false> {
      int prev, next;
      int visited;
    };

    template <typename Graph>
    struct ChildListNode {
      typedef typename Graph::Node Node;
      Node first;
      Node prev, next;
    };

    template <typename Graph>
    struct ArcListNode {
      typename Graph::Arc prev, next;
    };

    template <typename Graph>
    class PlanarityChecking {
    private:

      TEMPLATE_GRAPH_TYPEDEFS(Graph);

      const Graph& _graph;

    private:

      typedef typename Graph::template NodeMap<Arc> PredMap;

      typedef typename Graph::template EdgeMap<bool> TreeMap;

      typedef typename Graph::template NodeMap<int> OrderMap;
      typedef std::vector<Node> OrderList;

      typedef typename Graph::template NodeMap<int> LowMap;
      typedef typename Graph::template NodeMap<int> AncestorMap;

      typedef _planarity_bits::NodeDataNode<Graph> NodeDataNode;
      typedef std::vector<NodeDataNode> NodeData;

      typedef _planarity_bits::ChildListNode<Graph> ChildListNode;
      typedef typename Graph::template NodeMap<ChildListNode> ChildLists;

      typedef typename Graph::template NodeMap<std::list<int> > MergeRoots;

      typedef typename Graph::template NodeMap<bool> EmbedArc;

    public:

      PlanarityChecking(const Graph& graph) : _graph(graph) {}

      bool run() {
        typedef _planarity_bits::PlanarityVisitor<Graph> Visitor;

        PredMap pred_map(_graph, INVALID);
        TreeMap tree_map(_graph, false);

        OrderMap order_map(_graph, -1);
        OrderList order_list;

        AncestorMap ancestor_map(_graph, -1);
        LowMap low_map(_graph, -1);

        Visitor visitor(_graph, pred_map, tree_map,
                        order_map, order_list, ancestor_map, low_map);
        DfsVisit<Graph, Visitor> visit(_graph, visitor);
        visit.run();

        ChildLists child_lists(_graph);
        createChildLists(tree_map, order_map, low_map, child_lists);

        NodeData node_data(2 * order_list.size());

        EmbedArc embed_arc(_graph, false);

        MergeRoots merge_roots(_graph);

        for (int i = order_list.size() - 1; i >= 0; --i) {

          Node node = order_list[i];

          Node source = node;
          for (OutArcIt e(_graph, node); e != INVALID; ++e) {
            Node target = _graph.target(e);

            if (order_map[source] < order_map[target] && tree_map[e]) {
              initFace(target, node_data, order_map, order_list);
            }
          }

          for (OutArcIt e(_graph, node); e != INVALID; ++e) {
            Node target = _graph.target(e);

            if (order_map[source] < order_map[target] && !tree_map[e]) {
              embed_arc[target] = true;
              walkUp(target, source, i, pred_map, low_map,
                     order_map, order_list, node_data, merge_roots);
            }
          }

          for (typename MergeRoots::Value::iterator it =
                 merge_roots[node].begin();
               it != merge_roots[node].end(); ++it) {
            int rn = *it;
            walkDown(rn, i, node_data, order_list, child_lists,
                     ancestor_map, low_map, embed_arc, merge_roots);
          }
          merge_roots[node].clear();

          for (OutArcIt e(_graph, node); e != INVALID; ++e) {
            Node target = _graph.target(e);

            if (order_map[source] < order_map[target] && !tree_map[e]) {
              if (embed_arc[target]) {
                return false;
              }
            }
          }
        }

        return true;
      }

    private:

      void createChildLists(const TreeMap& tree_map, const OrderMap& order_map,
                            const LowMap& low_map, ChildLists& child_lists) {

        for (NodeIt n(_graph); n != INVALID; ++n) {
          Node source = n;

          std::vector<Node> targets;
          for (OutArcIt e(_graph, n); e != INVALID; ++e) {
            Node target = _graph.target(e);

            if (order_map[source] < order_map[target] && tree_map[e]) {
              targets.push_back(target);
            }
          }

          if (targets.size() == 0) {
            child_lists[source].first = INVALID;
          } else if (targets.size() == 1) {
            child_lists[source].first = targets[0];
            child_lists[targets[0]].prev = INVALID;
            child_lists[targets[0]].next = INVALID;
          } else {
            radixSort(targets.begin(), targets.end(), mapToFunctor(low_map));
            for (int i = 1; i < int(targets.size()); ++i) {
              child_lists[targets[i]].prev = targets[i - 1];
              child_lists[targets[i - 1]].next = targets[i];
            }
            child_lists[targets.back()].next = INVALID;
            child_lists[targets.front()].prev = INVALID;
            child_lists[source].first = targets.front();
          }
        }
      }

      void walkUp(const Node& node, Node root, int rorder,
                  const PredMap& pred_map, const LowMap& low_map,
                  const OrderMap& order_map, const OrderList& order_list,
                  NodeData& node_data, MergeRoots& merge_roots) {

        int na, nb;
        bool da, db;

        na = nb = order_map[node];
        da = true; db = false;

        while (true) {

          if (node_data[na].visited == rorder) break;
          if (node_data[nb].visited == rorder) break;

          node_data[na].visited = rorder;
          node_data[nb].visited = rorder;

          int rn = -1;

          if (na >= int(order_list.size())) {
            rn = na;
          } else if (nb >= int(order_list.size())) {
            rn = nb;
          }

          if (rn == -1) {
            int nn;

            nn = da ? node_data[na].prev : node_data[na].next;
            da = node_data[nn].prev != na;
            na = nn;

            nn = db ? node_data[nb].prev : node_data[nb].next;
            db = node_data[nn].prev != nb;
            nb = nn;

          } else {

            Node rep = order_list[rn - order_list.size()];
            Node parent = _graph.source(pred_map[rep]);

            if (low_map[rep] < rorder) {
              merge_roots[parent].push_back(rn);
            } else {
              merge_roots[parent].push_front(rn);
            }

            if (parent != root) {
              na = nb = order_map[parent];
              da = true; db = false;
            } else {
              break;
            }
          }
        }
      }

      void walkDown(int rn, int rorder, NodeData& node_data,
                    OrderList& order_list, ChildLists& child_lists,
                    AncestorMap& ancestor_map, LowMap& low_map,
                    EmbedArc& embed_arc, MergeRoots& merge_roots) {

        std::vector<std::pair<int, bool> > merge_stack;

        for (int di = 0; di < 2; ++di) {
          bool rd = di == 0;
          int pn = rn;
          int n = rd ? node_data[rn].next : node_data[rn].prev;

          while (n != rn) {

            Node node = order_list[n];

            if (embed_arc[node]) {

              // Merging components on the critical path
              while (!merge_stack.empty()) {

                // Component root
                int cn = merge_stack.back().first;
                bool cd = merge_stack.back().second;
                merge_stack.pop_back();

                // Parent of component
                int dn = merge_stack.back().first;
                bool dd = merge_stack.back().second;
                merge_stack.pop_back();

                Node parent = order_list[dn];

                // Erasing from merge_roots
                merge_roots[parent].pop_front();

                Node child = order_list[cn - order_list.size()];

                // Erasing from child_lists
                if (child_lists[child].prev != INVALID) {
                  child_lists[child_lists[child].prev].next =
                    child_lists[child].next;
                } else {
                  child_lists[parent].first = child_lists[child].next;
                }

                if (child_lists[child].next != INVALID) {
                  child_lists[child_lists[child].next].prev =
                    child_lists[child].prev;
                }

                // Merging external faces
                {
                  int en = cn;
                  cn = cd ? node_data[cn].prev : node_data[cn].next;
                  cd = node_data[cn].next == en;

                }

                if (cd) node_data[cn].next = dn; else node_data[cn].prev = dn;
                if (dd) node_data[dn].prev = cn; else node_data[dn].next = cn;

              }

              bool d = pn == node_data[n].prev;

              if (node_data[n].prev == node_data[n].next &&
                  node_data[n].inverted) {
                d = !d;
              }

              // Embedding arc into external face
              if (rd) node_data[rn].next = n; else node_data[rn].prev = n;
              if (d) node_data[n].prev = rn; else node_data[n].next = rn;
              pn = rn;

              embed_arc[order_list[n]] = false;
            }

            if (!merge_roots[node].empty()) {

              bool d = pn == node_data[n].prev;

              merge_stack.push_back(std::make_pair(n, d));

              int rn = merge_roots[node].front();

              int xn = node_data[rn].next;
              Node xnode = order_list[xn];

              int yn = node_data[rn].prev;
              Node ynode = order_list[yn];

              bool rd;
              if (!external(xnode, rorder, child_lists,
                            ancestor_map, low_map)) {
                rd = true;
              } else if (!external(ynode, rorder, child_lists,
                                   ancestor_map, low_map)) {
                rd = false;
              } else if (pertinent(xnode, embed_arc, merge_roots)) {
                rd = true;
              } else {
                rd = false;
              }

              merge_stack.push_back(std::make_pair(rn, rd));

              pn = rn;
              n = rd ? xn : yn;

            } else if (!external(node, rorder, child_lists,
                                 ancestor_map, low_map)) {
              int nn = (node_data[n].next != pn ?
                        node_data[n].next : node_data[n].prev);

              bool nd = n == node_data[nn].prev;

              if (nd) node_data[nn].prev = pn;
              else node_data[nn].next = pn;

              if (n == node_data[pn].prev) node_data[pn].prev = nn;
              else node_data[pn].next = nn;

              node_data[nn].inverted =
                (node_data[nn].prev == node_data[nn].next && nd != rd);

              n = nn;
            }
            else break;

          }

          if (!merge_stack.empty() || n == rn) {
            break;
          }
        }
      }

      void initFace(const Node& node, NodeData& node_data,
                    const OrderMap& order_map, const OrderList& order_list) {
        int n = order_map[node];
        int rn = n + order_list.size();

        node_data[n].next = node_data[n].prev = rn;
        node_data[rn].next = node_data[rn].prev = n;

        node_data[n].visited = order_list.size();
        node_data[rn].visited = order_list.size();

      }

      bool external(const Node& node, int rorder,
                    ChildLists& child_lists, AncestorMap& ancestor_map,
                    LowMap& low_map) {
        Node child = child_lists[node].first;

        if (child != INVALID) {
          if (low_map[child] < rorder) return true;
        }

        if (ancestor_map[node] < rorder) return true;

        return false;
      }

      bool pertinent(const Node& node, const EmbedArc& embed_arc,
                     const MergeRoots& merge_roots) {
        return !merge_roots[node].empty() || embed_arc[node];
      }

    };

  }

  /// \ingroup planar
  ///
  /// \brief Planarity checking of an undirected simple graph
  ///
  /// This function implements the Boyer-Myrvold algorithm for
  /// planarity checking of an undirected simple graph. It is a simplified
  /// version of the PlanarEmbedding algorithm class because neither
  /// the embedding nor the Kuratowski subdivisons are computed.
  template <typename GR>
  bool checkPlanarity(const GR& graph) {
    _planarity_bits::PlanarityChecking<GR> pc(graph);
    return pc.run();
  }

  /// \ingroup planar
  ///
  /// \brief Planar embedding of an undirected simple graph
  ///
  /// This class implements the Boyer-Myrvold algorithm for planar
  /// embedding of an undirected simple graph. The planar embedding is an
  /// ordering of the outgoing edges of the nodes, which is a possible
  /// configuration to draw the graph in the plane. If there is not
  /// such ordering then the graph contains a K<sub>5</sub> (full graph
  /// with 5 nodes) or a K<sub>3,3</sub> (complete bipartite graph on
  /// 3 Red and 3 Blue nodes) subdivision.
  ///
  /// The current implementation calculates either an embedding or a
  /// Kuratowski subdivision. The running time of the algorithm is O(n).
  ///
  /// \see PlanarDrawing, checkPlanarity()
  template <typename Graph>
  class PlanarEmbedding {
  private:

    TEMPLATE_GRAPH_TYPEDEFS(Graph);

    const Graph& _graph;
    typename Graph::template ArcMap<Arc> _embedding;

    typename Graph::template EdgeMap<bool> _kuratowski;

  private:

    typedef typename Graph::template NodeMap<Arc> PredMap;

    typedef typename Graph::template EdgeMap<bool> TreeMap;

    typedef typename Graph::template NodeMap<int> OrderMap;
    typedef std::vector<Node> OrderList;

    typedef typename Graph::template NodeMap<int> LowMap;
    typedef typename Graph::template NodeMap<int> AncestorMap;

    typedef _planarity_bits::NodeDataNode<Graph> NodeDataNode;
    typedef std::vector<NodeDataNode> NodeData;

    typedef _planarity_bits::ChildListNode<Graph> ChildListNode;
    typedef typename Graph::template NodeMap<ChildListNode> ChildLists;

    typedef typename Graph::template NodeMap<std::list<int> > MergeRoots;

    typedef typename Graph::template NodeMap<Arc> EmbedArc;

    typedef _planarity_bits::ArcListNode<Graph> ArcListNode;
    typedef typename Graph::template ArcMap<ArcListNode> ArcLists;

    typedef typename Graph::template NodeMap<bool> FlipMap;

    typedef typename Graph::template NodeMap<int> TypeMap;

    enum IsolatorNodeType {
      HIGHX = 6, LOWX = 7,
      HIGHY = 8, LOWY = 9,
      ROOT = 10, PERTINENT = 11,
      INTERNAL = 12
    };

  public:

    /// \brief The map type for storing the embedding
    ///
    /// The map type for storing the embedding.
    /// \see embeddingMap()
    typedef typename Graph::template ArcMap<Arc> EmbeddingMap;

    /// \brief Constructor
    ///
    /// Constructor.
    /// \pre The graph must be simple, i.e. it should not
    /// contain parallel or loop arcs.
    PlanarEmbedding(const Graph& graph)
      : _graph(graph), _embedding(_graph), _kuratowski(graph, false) {}

    /// \brief Run the algorithm.
    ///
    /// This function runs the algorithm.
    /// \param kuratowski If this parameter is set to \c false, then the
    /// algorithm does not compute a Kuratowski subdivision.
    /// \return \c true if the graph is planar.
    bool run(bool kuratowski = true) {
      typedef _planarity_bits::PlanarityVisitor<Graph> Visitor;

      PredMap pred_map(_graph, INVALID);
      TreeMap tree_map(_graph, false);

      OrderMap order_map(_graph, -1);
      OrderList order_list;

      AncestorMap ancestor_map(_graph, -1);
      LowMap low_map(_graph, -1);

      Visitor visitor(_graph, pred_map, tree_map,
                      order_map, order_list, ancestor_map, low_map);
      DfsVisit<Graph, Visitor> visit(_graph, visitor);
      visit.run();

      ChildLists child_lists(_graph);
      createChildLists(tree_map, order_map, low_map, child_lists);

      NodeData node_data(2 * order_list.size());

      EmbedArc embed_arc(_graph, INVALID);

      MergeRoots merge_roots(_graph);

      ArcLists arc_lists(_graph);

      FlipMap flip_map(_graph, false);

      for (int i = order_list.size() - 1; i >= 0; --i) {

        Node node = order_list[i];

        node_data[i].first = INVALID;

        Node source = node;
        for (OutArcIt e(_graph, node); e != INVALID; ++e) {
          Node target = _graph.target(e);

          if (order_map[source] < order_map[target] && tree_map[e]) {
            initFace(target, arc_lists, node_data,
                     pred_map, order_map, order_list);
          }
        }

        for (OutArcIt e(_graph, node); e != INVALID; ++e) {
          Node target = _graph.target(e);

          if (order_map[source] < order_map[target] && !tree_map[e]) {
            embed_arc[target] = e;
            walkUp(target, source, i, pred_map, low_map,
                   order_map, order_list, node_data, merge_roots);
          }
        }

        for (typename MergeRoots::Value::iterator it =
               merge_roots[node].begin(); it != merge_roots[node].end(); ++it) {
          int rn = *it;
          walkDown(rn, i, node_data, arc_lists, flip_map, order_list,
                   child_lists, ancestor_map, low_map, embed_arc, merge_roots);
        }
        merge_roots[node].clear();

        for (OutArcIt e(_graph, node); e != INVALID; ++e) {
          Node target = _graph.target(e);

          if (order_map[source] < order_map[target] && !tree_map[e]) {
            if (embed_arc[target] != INVALID) {
              if (kuratowski) {
                isolateKuratowski(e, node_data, arc_lists, flip_map,
                                  order_map, order_list, pred_map, child_lists,
                                  ancestor_map, low_map,
                                  embed_arc, merge_roots);
              }
              return false;
            }
          }
        }
      }

      for (int i = 0; i < int(order_list.size()); ++i) {

        mergeRemainingFaces(order_list[i], node_data, order_list, order_map,
                            child_lists, arc_lists);
        storeEmbedding(order_list[i], node_data, order_map, pred_map,
                       arc_lists, flip_map);
      }

      return true;
    }

    /// \brief Give back the successor of an arc
    ///
    /// This function gives back the successor of an arc. It makes
    /// possible to query the cyclic order of the outgoing arcs from
    /// a node.
    Arc next(const Arc& arc) const {
      return _embedding[arc];
    }

    /// \brief Give back the calculated embedding map
    ///
    /// This function gives back the calculated embedding map, which
    /// contains the successor of each arc in the cyclic order of the
    /// outgoing arcs of its source node.
    const EmbeddingMap& embeddingMap() const {
      return _embedding;
    }

    /// \brief Give back \c true if the given edge is in the Kuratowski
    /// subdivision
    ///
    /// This function gives back \c true if the given edge is in the found
    /// Kuratowski subdivision.
    /// \pre The \c run() function must be called with \c true parameter
    /// before using this function.
    bool kuratowski(const Edge& edge) const {
      return _kuratowski[edge];
    }

  private:

    void createChildLists(const TreeMap& tree_map, const OrderMap& order_map,
                          const LowMap& low_map, ChildLists& child_lists) {

      for (NodeIt n(_graph); n != INVALID; ++n) {
        Node source = n;

        std::vector<Node> targets;
        for (OutArcIt e(_graph, n); e != INVALID; ++e) {
          Node target = _graph.target(e);

          if (order_map[source] < order_map[target] && tree_map[e]) {
            targets.push_back(target);
          }
        }

        if (targets.size() == 0) {
          child_lists[source].first = INVALID;
        } else if (targets.size() == 1) {
          child_lists[source].first = targets[0];
          child_lists[targets[0]].prev = INVALID;
          child_lists[targets[0]].next = INVALID;
        } else {
          radixSort(targets.begin(), targets.end(), mapToFunctor(low_map));
          for (int i = 1; i < int(targets.size()); ++i) {
            child_lists[targets[i]].prev = targets[i - 1];
            child_lists[targets[i - 1]].next = targets[i];
          }
          child_lists[targets.back()].next = INVALID;
          child_lists[targets.front()].prev = INVALID;
          child_lists[source].first = targets.front();
        }
      }
    }

    void walkUp(const Node& node, Node root, int rorder,
                const PredMap& pred_map, const LowMap& low_map,
                const OrderMap& order_map, const OrderList& order_list,
                NodeData& node_data, MergeRoots& merge_roots) {

      int na, nb;
      bool da, db;

      na = nb = order_map[node];
      da = true; db = false;

      while (true) {

        if (node_data[na].visited == rorder) break;
        if (node_data[nb].visited == rorder) break;

        node_data[na].visited = rorder;
        node_data[nb].visited = rorder;

        int rn = -1;

        if (na >= int(order_list.size())) {
          rn = na;
        } else if (nb >= int(order_list.size())) {
          rn = nb;
        }

        if (rn == -1) {
          int nn;

          nn = da ? node_data[na].prev : node_data[na].next;
          da = node_data[nn].prev != na;
          na = nn;

          nn = db ? node_data[nb].prev : node_data[nb].next;
          db = node_data[nn].prev != nb;
          nb = nn;

        } else {

          Node rep = order_list[rn - order_list.size()];
          Node parent = _graph.source(pred_map[rep]);

          if (low_map[rep] < rorder) {
            merge_roots[parent].push_back(rn);
          } else {
            merge_roots[parent].push_front(rn);
          }

          if (parent != root) {
            na = nb = order_map[parent];
            da = true; db = false;
          } else {
            break;
          }
        }
      }
    }

    void walkDown(int rn, int rorder, NodeData& node_data,
                  ArcLists& arc_lists, FlipMap& flip_map,
                  OrderList& order_list, ChildLists& child_lists,
                  AncestorMap& ancestor_map, LowMap& low_map,
                  EmbedArc& embed_arc, MergeRoots& merge_roots) {

      std::vector<std::pair<int, bool> > merge_stack;

      for (int di = 0; di < 2; ++di) {
        bool rd = di == 0;
        int pn = rn;
        int n = rd ? node_data[rn].next : node_data[rn].prev;

        while (n != rn) {

          Node node = order_list[n];

          if (embed_arc[node] != INVALID) {

            // Merging components on the critical path
            while (!merge_stack.empty()) {

              // Component root
              int cn = merge_stack.back().first;
              bool cd = merge_stack.back().second;
              merge_stack.pop_back();

              // Parent of component
              int dn = merge_stack.back().first;
              bool dd = merge_stack.back().second;
              merge_stack.pop_back();

              Node parent = order_list[dn];

              // Erasing from merge_roots
              merge_roots[parent].pop_front();

              Node child = order_list[cn - order_list.size()];

              // Erasing from child_lists
              if (child_lists[child].prev != INVALID) {
                child_lists[child_lists[child].prev].next =
                  child_lists[child].next;
              } else {
                child_lists[parent].first = child_lists[child].next;
              }

              if (child_lists[child].next != INVALID) {
                child_lists[child_lists[child].next].prev =
                  child_lists[child].prev;
              }

              // Merging arcs + flipping
              Arc de = node_data[dn].first;
              Arc ce = node_data[cn].first;

              flip_map[order_list[cn - order_list.size()]] = cd != dd;
              if (cd != dd) {
                std::swap(arc_lists[ce].prev, arc_lists[ce].next);
                ce = arc_lists[ce].prev;
                std::swap(arc_lists[ce].prev, arc_lists[ce].next);
              }

              {
                Arc dne = arc_lists[de].next;
                Arc cne = arc_lists[ce].next;

                arc_lists[de].next = cne;
                arc_lists[ce].next = dne;

                arc_lists[dne].prev = ce;
                arc_lists[cne].prev = de;
              }

              if (dd) {
                node_data[dn].first = ce;
              }

              // Merging external faces
              {
                int en = cn;
                cn = cd ? node_data[cn].prev : node_data[cn].next;
                cd = node_data[cn].next == en;

                 if (node_data[cn].prev == node_data[cn].next &&
                    node_data[cn].inverted) {
                   cd = !cd;
                 }
              }

              if (cd) node_data[cn].next = dn; else node_data[cn].prev = dn;
              if (dd) node_data[dn].prev = cn; else node_data[dn].next = cn;

            }

            bool d = pn == node_data[n].prev;

            if (node_data[n].prev == node_data[n].next &&
                node_data[n].inverted) {
              d = !d;
            }

            // Add new arc
            {
              Arc arc = embed_arc[node];
              Arc re = node_data[rn].first;

              arc_lists[arc_lists[re].next].prev = arc;
              arc_lists[arc].next = arc_lists[re].next;
              arc_lists[arc].prev = re;
              arc_lists[re].next = arc;

              if (!rd) {
                node_data[rn].first = arc;
              }

              Arc rev = _graph.oppositeArc(arc);
              Arc e = node_data[n].first;

              arc_lists[arc_lists[e].next].prev = rev;
              arc_lists[rev].next = arc_lists[e].next;
              arc_lists[rev].prev = e;
              arc_lists[e].next = rev;

              if (d) {
                node_data[n].first = rev;
              }

            }

            // Embedding arc into external face
            if (rd) node_data[rn].next = n; else node_data[rn].prev = n;
            if (d) node_data[n].prev = rn; else node_data[n].next = rn;
            pn = rn;

            embed_arc[order_list[n]] = INVALID;
          }

          if (!merge_roots[node].empty()) {

            bool d = pn == node_data[n].prev;
            if (node_data[n].prev == node_data[n].next &&
                node_data[n].inverted) {
              d = !d;
            }

            merge_stack.push_back(std::make_pair(n, d));

            int rn = merge_roots[node].front();

            int xn = node_data[rn].next;
            Node xnode = order_list[xn];

            int yn = node_data[rn].prev;
            Node ynode = order_list[yn];

            bool rd;
            if (!external(xnode, rorder, child_lists, ancestor_map, low_map)) {
              rd = true;
            } else if (!external(ynode, rorder, child_lists,
                                 ancestor_map, low_map)) {
              rd = false;
            } else if (pertinent(xnode, embed_arc, merge_roots)) {
              rd = true;
            } else {
              rd = false;
            }

            merge_stack.push_back(std::make_pair(rn, rd));

            pn = rn;
            n = rd ? xn : yn;

          } else if (!external(node, rorder, child_lists,
                               ancestor_map, low_map)) {
            int nn = (node_data[n].next != pn ?
                      node_data[n].next : node_data[n].prev);

            bool nd = n == node_data[nn].prev;

            if (nd) node_data[nn].prev = pn;
            else node_data[nn].next = pn;

            if (n == node_data[pn].prev) node_data[pn].prev = nn;
            else node_data[pn].next = nn;

            node_data[nn].inverted =
              (node_data[nn].prev == node_data[nn].next && nd != rd);

            n = nn;
          }
          else break;

        }

        if (!merge_stack.empty() || n == rn) {
          break;
        }
      }
    }

    void initFace(const Node& node, ArcLists& arc_lists,
                  NodeData& node_data, const PredMap& pred_map,
                  const OrderMap& order_map, const OrderList& order_list) {
      int n = order_map[node];
      int rn = n + order_list.size();

      node_data[n].next = node_data[n].prev = rn;
      node_data[rn].next = node_data[rn].prev = n;

      node_data[n].visited = order_list.size();
      node_data[rn].visited = order_list.size();

      node_data[n].inverted = false;
      node_data[rn].inverted = false;

      Arc arc = pred_map[node];
      Arc rev = _graph.oppositeArc(arc);

      node_data[rn].first = arc;
      node_data[n].first = rev;

      arc_lists[arc].prev = arc;
      arc_lists[arc].next = arc;

      arc_lists[rev].prev = rev;
      arc_lists[rev].next = rev;

    }

    void mergeRemainingFaces(const Node& node, NodeData& node_data,
                             OrderList& order_list, OrderMap& order_map,
                             ChildLists& child_lists, ArcLists& arc_lists) {
      while (child_lists[node].first != INVALID) {
        int dd = order_map[node];
        Node child = child_lists[node].first;
        int cd = order_map[child] + order_list.size();
        child_lists[node].first = child_lists[child].next;

        Arc de = node_data[dd].first;
        Arc ce = node_data[cd].first;

        if (de != INVALID) {
          Arc dne = arc_lists[de].next;
          Arc cne = arc_lists[ce].next;

          arc_lists[de].next = cne;
          arc_lists[ce].next = dne;

          arc_lists[dne].prev = ce;
          arc_lists[cne].prev = de;
        }

        node_data[dd].first = ce;

      }
    }

    void storeEmbedding(const Node& node, NodeData& node_data,
                        OrderMap& order_map, PredMap& pred_map,
                        ArcLists& arc_lists, FlipMap& flip_map) {

      if (node_data[order_map[node]].first == INVALID) return;

      if (pred_map[node] != INVALID) {
        Node source = _graph.source(pred_map[node]);
        flip_map[node] = flip_map[node] != flip_map[source];
      }

      Arc first = node_data[order_map[node]].first;
      Arc prev = first;

      Arc arc = flip_map[node] ?
        arc_lists[prev].prev : arc_lists[prev].next;

      _embedding[prev] = arc;

      while (arc != first) {
        Arc next = arc_lists[arc].prev == prev ?
          arc_lists[arc].next : arc_lists[arc].prev;
        prev = arc; arc = next;
        _embedding[prev] = arc;
      }
    }


    bool external(const Node& node, int rorder,
                  ChildLists& child_lists, AncestorMap& ancestor_map,
                  LowMap& low_map) {
      Node child = child_lists[node].first;

      if (child != INVALID) {
        if (low_map[child] < rorder) return true;
      }

      if (ancestor_map[node] < rorder) return true;

      return false;
    }

    bool pertinent(const Node& node, const EmbedArc& embed_arc,
                   const MergeRoots& merge_roots) {
      return !merge_roots[node].empty() || embed_arc[node] != INVALID;
    }

    int lowPoint(const Node& node, OrderMap& order_map, ChildLists& child_lists,
                 AncestorMap& ancestor_map, LowMap& low_map) {
      int low_point;

      Node child = child_lists[node].first;

      if (child != INVALID) {
        low_point = low_map[child];
      } else {
        low_point = order_map[node];
      }

      if (low_point > ancestor_map[node]) {
        low_point = ancestor_map[node];
      }

      return low_point;
    }

    int findComponentRoot(Node root, Node node, ChildLists& child_lists,
                          OrderMap& order_map, OrderList& order_list) {

      int order = order_map[root];
      int norder = order_map[node];

      Node child = child_lists[root].first;
      while (child != INVALID) {
        int corder = order_map[child];
        if (corder > order && corder < norder) {
          order = corder;
        }
        child = child_lists[child].next;
      }
      return order + order_list.size();
    }

    Node findPertinent(Node node, OrderMap& order_map, NodeData& node_data,
                       EmbedArc& embed_arc, MergeRoots& merge_roots) {
      Node wnode =_graph.target(node_data[order_map[node]].first);
      while (!pertinent(wnode, embed_arc, merge_roots)) {
        wnode = _graph.target(node_data[order_map[wnode]].first);
      }
      return wnode;
    }


    Node findExternal(Node node, int rorder, OrderMap& order_map,
                      ChildLists& child_lists, AncestorMap& ancestor_map,
                      LowMap& low_map, NodeData& node_data) {
      Node wnode =_graph.target(node_data[order_map[node]].first);
      while (!external(wnode, rorder, child_lists, ancestor_map, low_map)) {
        wnode = _graph.target(node_data[order_map[wnode]].first);
      }
      return wnode;
    }

    void markCommonPath(Node node, int rorder, Node& wnode, Node& znode,
                        OrderList& order_list, OrderMap& order_map,
                        NodeData& node_data, ArcLists& arc_lists,
                        EmbedArc& embed_arc, MergeRoots& merge_roots,
                        ChildLists& child_lists, AncestorMap& ancestor_map,
                        LowMap& low_map) {

      Node cnode = node;
      Node pred = INVALID;

      while (true) {

        bool pert = pertinent(cnode, embed_arc, merge_roots);
        bool ext = external(cnode, rorder, child_lists, ancestor_map, low_map);

        if (pert && ext) {
          if (!merge_roots[cnode].empty()) {
            int cn = merge_roots[cnode].back();

            if (low_map[order_list[cn - order_list.size()]] < rorder) {
              Arc arc = node_data[cn].first;
              _kuratowski.set(arc, true);

              pred = cnode;
              cnode = _graph.target(arc);

              continue;
            }
          }
          wnode = znode = cnode;
          return;

        } else if (pert) {
          wnode = cnode;

          while (!external(cnode, rorder, child_lists, ancestor_map, low_map)) {
            Arc arc = node_data[order_map[cnode]].first;

            if (_graph.target(arc) == pred) {
              arc = arc_lists[arc].next;
            }
            _kuratowski.set(arc, true);

            Node next = _graph.target(arc);
            pred = cnode; cnode = next;
          }

          znode = cnode;
          return;

        } else if (ext) {
          znode = cnode;

          while (!pertinent(cnode, embed_arc, merge_roots)) {
            Arc arc = node_data[order_map[cnode]].first;

            if (_graph.target(arc) == pred) {
              arc = arc_lists[arc].next;
            }
            _kuratowski.set(arc, true);

            Node next = _graph.target(arc);
            pred = cnode; cnode = next;
          }

          wnode = cnode;
          return;

        } else {
          Arc arc = node_data[order_map[cnode]].first;

          if (_graph.target(arc) == pred) {
            arc = arc_lists[arc].next;
          }
          _kuratowski.set(arc, true);

          Node next = _graph.target(arc);
          pred = cnode; cnode = next;
        }

      }

    }

    void orientComponent(Node root, int rn, OrderMap& order_map,
                         PredMap& pred_map, NodeData& node_data,
                         ArcLists& arc_lists, FlipMap& flip_map,
                         TypeMap& type_map) {
      node_data[order_map[root]].first = node_data[rn].first;
      type_map[root] = 1;

      std::vector<Node> st, qu;

      st.push_back(root);
      while (!st.empty()) {
        Node node = st.back();
        st.pop_back();
        qu.push_back(node);

        Arc arc = node_data[order_map[node]].first;

        if (type_map[_graph.target(arc)] == 0) {
          st.push_back(_graph.target(arc));
          type_map[_graph.target(arc)] = 1;
        }

        Arc last = arc, pred = arc;
        arc = arc_lists[arc].next;
        while (arc != last) {

          if (type_map[_graph.target(arc)] == 0) {
            st.push_back(_graph.target(arc));
            type_map[_graph.target(arc)] = 1;
          }

          Arc next = arc_lists[arc].next != pred ?
            arc_lists[arc].next : arc_lists[arc].prev;
          pred = arc; arc = next;
        }

      }

      type_map[root] = 2;
      flip_map[root] = false;

      for (int i = 1; i < int(qu.size()); ++i) {

        Node node = qu[i];

        while (type_map[node] != 2) {
          st.push_back(node);
          type_map[node] = 2;
          node = _graph.source(pred_map[node]);
        }

        bool flip = flip_map[node];

        while (!st.empty()) {
          node = st.back();
          st.pop_back();

          flip_map[node] = flip != flip_map[node];
          flip = flip_map[node];

          if (flip) {
            Arc arc = node_data[order_map[node]].first;
            std::swap(arc_lists[arc].prev, arc_lists[arc].next);
            arc = arc_lists[arc].prev;
            std::swap(arc_lists[arc].prev, arc_lists[arc].next);
            node_data[order_map[node]].first = arc;
          }
        }
      }

      for (int i = 0; i < int(qu.size()); ++i) {

        Arc arc = node_data[order_map[qu[i]]].first;
        Arc last = arc, pred = arc;

        arc = arc_lists[arc].next;
        while (arc != last) {

          if (arc_lists[arc].next == pred) {
            std::swap(arc_lists[arc].next, arc_lists[arc].prev);
          }
          pred = arc; arc = arc_lists[arc].next;
        }

      }
    }

    void setFaceFlags(Node root, Node wnode, Node ynode, Node xnode,
                      OrderMap& order_map, NodeData& node_data,
                      TypeMap& type_map) {
      Node node = _graph.target(node_data[order_map[root]].first);

      while (node != ynode) {
        type_map[node] = HIGHY;
        node = _graph.target(node_data[order_map[node]].first);
      }

      while (node != wnode) {
        type_map[node] = LOWY;
        node = _graph.target(node_data[order_map[node]].first);
      }

      node = _graph.target(node_data[order_map[wnode]].first);

      while (node != xnode) {
        type_map[node] = LOWX;
        node = _graph.target(node_data[order_map[node]].first);
      }
      type_map[node] = LOWX;

      node = _graph.target(node_data[order_map[xnode]].first);
      while (node != root) {
        type_map[node] = HIGHX;
        node = _graph.target(node_data[order_map[node]].first);
      }

      type_map[wnode] = PERTINENT;
      type_map[root] = ROOT;
    }

    void findInternalPath(std::vector<Arc>& ipath,
                          Node wnode, Node root, TypeMap& type_map,
                          OrderMap& order_map, NodeData& node_data,
                          ArcLists& arc_lists) {
      std::vector<Arc> st;

      Node node = wnode;

      while (node != root) {
        Arc arc = arc_lists[node_data[order_map[node]].first].next;
        st.push_back(arc);
        node = _graph.target(arc);
      }

      while (true) {
        Arc arc = st.back();
        if (type_map[_graph.target(arc)] == LOWX ||
            type_map[_graph.target(arc)] == HIGHX) {
          break;
        }
        if (type_map[_graph.target(arc)] == 2) {
          type_map[_graph.target(arc)] = 3;

          arc = arc_lists[_graph.oppositeArc(arc)].next;
          st.push_back(arc);
        } else {
          st.pop_back();
          arc = arc_lists[arc].next;

          while (_graph.oppositeArc(arc) == st.back()) {
            arc = st.back();
            st.pop_back();
            arc = arc_lists[arc].next;
          }
          st.push_back(arc);
        }
      }

      for (int i = 0; i < int(st.size()); ++i) {
        if (type_map[_graph.target(st[i])] != LOWY &&
            type_map[_graph.target(st[i])] != HIGHY) {
          for (; i < int(st.size()); ++i) {
            ipath.push_back(st[i]);
          }
        }
      }
    }

    void setInternalFlags(std::vector<Arc>& ipath, TypeMap& type_map) {
      for (int i = 1; i < int(ipath.size()); ++i) {
        type_map[_graph.source(ipath[i])] = INTERNAL;
      }
    }

    void findPilePath(std::vector<Arc>& ppath,
                      Node root, TypeMap& type_map, OrderMap& order_map,
                      NodeData& node_data, ArcLists& arc_lists) {
      std::vector<Arc> st;

      st.push_back(_graph.oppositeArc(node_data[order_map[root]].first));
      st.push_back(node_data[order_map[root]].first);

      while (st.size() > 1) {
        Arc arc = st.back();
        if (type_map[_graph.target(arc)] == INTERNAL) {
          break;
        }
        if (type_map[_graph.target(arc)] == 3) {
          type_map[_graph.target(arc)] = 4;

          arc = arc_lists[_graph.oppositeArc(arc)].next;
          st.push_back(arc);
        } else {
          st.pop_back();
          arc = arc_lists[arc].next;

          while (!st.empty() && _graph.oppositeArc(arc) == st.back()) {
            arc = st.back();
            st.pop_back();
            arc = arc_lists[arc].next;
          }
          st.push_back(arc);
        }
      }

      for (int i = 1; i < int(st.size()); ++i) {
        ppath.push_back(st[i]);
      }
    }


    int markExternalPath(Node node, OrderMap& order_map,
                         ChildLists& child_lists, PredMap& pred_map,
                         AncestorMap& ancestor_map, LowMap& low_map) {
      int lp = lowPoint(node, order_map, child_lists,
                        ancestor_map, low_map);

      if (ancestor_map[node] != lp) {
        node = child_lists[node].first;
        _kuratowski[pred_map[node]] = true;

        while (ancestor_map[node] != lp) {
          for (OutArcIt e(_graph, node); e != INVALID; ++e) {
            Node tnode = _graph.target(e);
            if (order_map[tnode] > order_map[node] && low_map[tnode] == lp) {
              node = tnode;
              _kuratowski[e] = true;
              break;
            }
          }
        }
      }

      for (OutArcIt e(_graph, node); e != INVALID; ++e) {
        if (order_map[_graph.target(e)] == lp) {
          _kuratowski[e] = true;
          break;
        }
      }

      return lp;
    }

    void markPertinentPath(Node node, OrderMap& order_map,
                           NodeData& node_data, ArcLists& arc_lists,
                           EmbedArc& embed_arc, MergeRoots& merge_roots) {
      while (embed_arc[node] == INVALID) {
        int n = merge_roots[node].front();
        Arc arc = node_data[n].first;

        _kuratowski.set(arc, true);

        Node pred = node;
        node = _graph.target(arc);
        while (!pertinent(node, embed_arc, merge_roots)) {
          arc = node_data[order_map[node]].first;
          if (_graph.target(arc) == pred) {
            arc = arc_lists[arc].next;
          }
          _kuratowski.set(arc, true);
          pred = node;
          node = _graph.target(arc);
        }
      }
      _kuratowski.set(embed_arc[node], true);
    }

    void markPredPath(Node node, Node snode, PredMap& pred_map) {
      while (node != snode) {
        _kuratowski.set(pred_map[node], true);
        node = _graph.source(pred_map[node]);
      }
    }

    void markFacePath(Node ynode, Node xnode,
                      OrderMap& order_map, NodeData& node_data) {
      Arc arc = node_data[order_map[ynode]].first;
      Node node = _graph.target(arc);
      _kuratowski.set(arc, true);

      while (node != xnode) {
        arc = node_data[order_map[node]].first;
        _kuratowski.set(arc, true);
        node = _graph.target(arc);
      }
    }

    void markInternalPath(std::vector<Arc>& path) {
      for (int i = 0; i < int(path.size()); ++i) {
        _kuratowski.set(path[i], true);
      }
    }

    void markPilePath(std::vector<Arc>& path) {
      for (int i = 0; i < int(path.size()); ++i) {
        _kuratowski.set(path[i], true);
      }
    }

    void isolateKuratowski(Arc arc, NodeData& node_data,
                           ArcLists& arc_lists, FlipMap& flip_map,
                           OrderMap& order_map, OrderList& order_list,
                           PredMap& pred_map, ChildLists& child_lists,
                           AncestorMap& ancestor_map, LowMap& low_map,
                           EmbedArc& embed_arc, MergeRoots& merge_roots) {

      Node root = _graph.source(arc);
      Node enode = _graph.target(arc);

      int rorder = order_map[root];

      TypeMap type_map(_graph, 0);

      int rn = findComponentRoot(root, enode, child_lists,
                                 order_map, order_list);

      Node xnode = order_list[node_data[rn].next];
      Node ynode = order_list[node_data[rn].prev];

      // Minor-A
      {
        while (!merge_roots[xnode].empty() || !merge_roots[ynode].empty()) {

          if (!merge_roots[xnode].empty()) {
            root = xnode;
            rn = merge_roots[xnode].front();
          } else {
            root = ynode;
            rn = merge_roots[ynode].front();
          }

          xnode = order_list[node_data[rn].next];
          ynode = order_list[node_data[rn].prev];
        }

        if (root != _graph.source(arc)) {
          orientComponent(root, rn, order_map, pred_map,
                          node_data, arc_lists, flip_map, type_map);
          markFacePath(root, root, order_map, node_data);
          int xlp = markExternalPath(xnode, order_map, child_lists,
                                     pred_map, ancestor_map, low_map);
          int ylp = markExternalPath(ynode, order_map, child_lists,
                                     pred_map, ancestor_map, low_map);
          markPredPath(root, order_list[xlp < ylp ? xlp : ylp], pred_map);
          Node lwnode = findPertinent(ynode, order_map, node_data,
                                      embed_arc, merge_roots);

          markPertinentPath(lwnode, order_map, node_data, arc_lists,
                            embed_arc, merge_roots);

          return;
        }
      }

      orientComponent(root, rn, order_map, pred_map,
                      node_data, arc_lists, flip_map, type_map);

      Node wnode = findPertinent(ynode, order_map, node_data,
                                 embed_arc, merge_roots);
      setFaceFlags(root, wnode, ynode, xnode, order_map, node_data, type_map);


      //Minor-B
      if (!merge_roots[wnode].empty()) {
        int cn = merge_roots[wnode].back();
        Node rep = order_list[cn - order_list.size()];
        if (low_map[rep] < rorder) {
          markFacePath(root, root, order_map, node_data);
          int xlp = markExternalPath(xnode, order_map, child_lists,
                                     pred_map, ancestor_map, low_map);
          int ylp = markExternalPath(ynode, order_map, child_lists,
                                     pred_map, ancestor_map, low_map);

          Node lwnode, lznode;
          markCommonPath(wnode, rorder, lwnode, lznode, order_list,
                         order_map, node_data, arc_lists, embed_arc,
                         merge_roots, child_lists, ancestor_map, low_map);

          markPertinentPath(lwnode, order_map, node_data, arc_lists,
                            embed_arc, merge_roots);
          int zlp = markExternalPath(lznode, order_map, child_lists,
                                     pred_map, ancestor_map, low_map);

          int minlp = xlp < ylp ? xlp : ylp;
          if (zlp < minlp) minlp = zlp;

          int maxlp = xlp > ylp ? xlp : ylp;
          if (zlp > maxlp) maxlp = zlp;

          markPredPath(order_list[maxlp], order_list[minlp], pred_map);

          return;
        }
      }

      Node pxnode, pynode;
      std::vector<Arc> ipath;
      findInternalPath(ipath, wnode, root, type_map, order_map,
                       node_data, arc_lists);
      setInternalFlags(ipath, type_map);
      pynode = _graph.source(ipath.front());
      pxnode = _graph.target(ipath.back());

      wnode = findPertinent(pynode, order_map, node_data,
                            embed_arc, merge_roots);

      // Minor-C
      {
        if (type_map[_graph.source(ipath.front())] == HIGHY) {
          if (type_map[_graph.target(ipath.back())] == HIGHX) {
            markFacePath(xnode, pxnode, order_map, node_data);
          }
          markFacePath(root, xnode, order_map, node_data);
          markPertinentPath(wnode, order_map, node_data, arc_lists,
                            embed_arc, merge_roots);
          markInternalPath(ipath);
          int xlp = markExternalPath(xnode, order_map, child_lists,
                                     pred_map, ancestor_map, low_map);
          int ylp = markExternalPath(ynode, order_map, child_lists,
                                     pred_map, ancestor_map, low_map);
          markPredPath(root, order_list[xlp < ylp ? xlp : ylp], pred_map);
          return;
        }

        if (type_map[_graph.target(ipath.back())] == HIGHX) {
          markFacePath(ynode, root, order_map, node_data);
          markPertinentPath(wnode, order_map, node_data, arc_lists,
                            embed_arc, merge_roots);
          markInternalPath(ipath);
          int xlp = markExternalPath(xnode, order_map, child_lists,
                                     pred_map, ancestor_map, low_map);
          int ylp = markExternalPath(ynode, order_map, child_lists,
                                     pred_map, ancestor_map, low_map);
          markPredPath(root, order_list[xlp < ylp ? xlp : ylp], pred_map);
          return;
        }
      }

      std::vector<Arc> ppath;
      findPilePath(ppath, root, type_map, order_map, node_data, arc_lists);

      // Minor-D
      if (!ppath.empty()) {
        markFacePath(ynode, xnode, order_map, node_data);
        markPertinentPath(wnode, order_map, node_data, arc_lists,
                          embed_arc, merge_roots);
        markPilePath(ppath);
        markInternalPath(ipath);
        int xlp = markExternalPath(xnode, order_map, child_lists,
                                   pred_map, ancestor_map, low_map);
        int ylp = markExternalPath(ynode, order_map, child_lists,
                                   pred_map, ancestor_map, low_map);
        markPredPath(root, order_list[xlp < ylp ? xlp : ylp], pred_map);
        return;
      }

      // Minor-E*
      {

        if (!external(wnode, rorder, child_lists, ancestor_map, low_map)) {
          Node znode = findExternal(pynode, rorder, order_map,
                                    child_lists, ancestor_map,
                                    low_map, node_data);

          if (type_map[znode] == LOWY) {
            markFacePath(root, xnode, order_map, node_data);
            markPertinentPath(wnode, order_map, node_data, arc_lists,
                              embed_arc, merge_roots);
            markInternalPath(ipath);
            int xlp = markExternalPath(xnode, order_map, child_lists,
                                       pred_map, ancestor_map, low_map);
            int zlp = markExternalPath(znode, order_map, child_lists,
                                       pred_map, ancestor_map, low_map);
            markPredPath(root, order_list[xlp < zlp ? xlp : zlp], pred_map);
          } else {
            markFacePath(ynode, root, order_map, node_data);
            markPertinentPath(wnode, order_map, node_data, arc_lists,
                              embed_arc, merge_roots);
            markInternalPath(ipath);
            int ylp = markExternalPath(ynode, order_map, child_lists,
                                       pred_map, ancestor_map, low_map);
            int zlp = markExternalPath(znode, order_map, child_lists,
                                       pred_map, ancestor_map, low_map);
            markPredPath(root, order_list[ylp < zlp ? ylp : zlp], pred_map);
          }
          return;
        }

        int xlp = markExternalPath(xnode, order_map, child_lists,
                                   pred_map, ancestor_map, low_map);
        int ylp = markExternalPath(ynode, order_map, child_lists,
                                   pred_map, ancestor_map, low_map);
        int wlp = markExternalPath(wnode, order_map, child_lists,
                                   pred_map, ancestor_map, low_map);

        if (wlp > xlp && wlp > ylp) {
          markFacePath(root, root, order_map, node_data);
          markPredPath(root, order_list[xlp < ylp ? xlp : ylp], pred_map);
          return;
        }

        markInternalPath(ipath);
        markPertinentPath(wnode, order_map, node_data, arc_lists,
                          embed_arc, merge_roots);

        if (xlp > ylp && xlp > wlp) {
          markFacePath(root, pynode, order_map, node_data);
          markFacePath(wnode, xnode, order_map, node_data);
          markPredPath(root, order_list[ylp < wlp ? ylp : wlp], pred_map);
          return;
        }

        if (ylp > xlp && ylp > wlp) {
          markFacePath(pxnode, root, order_map, node_data);
          markFacePath(ynode, wnode, order_map, node_data);
          markPredPath(root, order_list[xlp < wlp ? xlp : wlp], pred_map);
          return;
        }

        if (pynode != ynode) {
          markFacePath(pxnode, wnode, order_map, node_data);

          int minlp = xlp < ylp ? xlp : ylp;
          if (wlp < minlp) minlp = wlp;

          int maxlp = xlp > ylp ? xlp : ylp;
          if (wlp > maxlp) maxlp = wlp;

          markPredPath(order_list[maxlp], order_list[minlp], pred_map);
          return;
        }

        if (pxnode != xnode) {
          markFacePath(wnode, pynode, order_map, node_data);

          int minlp = xlp < ylp ? xlp : ylp;
          if (wlp < minlp) minlp = wlp;

          int maxlp = xlp > ylp ? xlp : ylp;
          if (wlp > maxlp) maxlp = wlp;

          markPredPath(order_list[maxlp], order_list[minlp], pred_map);
          return;
        }

        markFacePath(root, root, order_map, node_data);
        int minlp = xlp < ylp ? xlp : ylp;
        if (wlp < minlp) minlp = wlp;
        markPredPath(root, order_list[minlp], pred_map);
        return;
      }

    }

  };

  namespace _planarity_bits {

    template <typename Graph, typename EmbeddingMap>
    void makeConnected(Graph& graph, EmbeddingMap& embedding) {
      DfsVisitor<Graph> null_visitor;
      DfsVisit<Graph, DfsVisitor<Graph> > dfs(graph, null_visitor);
      dfs.init();

      typename Graph::Node u = INVALID;
      for (typename Graph::NodeIt n(graph); n != INVALID; ++n) {
        if (!dfs.reached(n)) {
          dfs.addSource(n);
          dfs.start();
          if (u == INVALID) {
            u = n;
          } else {
            typename Graph::Node v = n;

            typename Graph::Arc ue = typename Graph::OutArcIt(graph, u);
            typename Graph::Arc ve = typename Graph::OutArcIt(graph, v);

            typename Graph::Arc e = graph.direct(graph.addEdge(u, v), true);

            if (ue != INVALID) {
              embedding[e] = embedding[ue];
              embedding[ue] = e;
            } else {
              embedding[e] = e;
            }

            if (ve != INVALID) {
              embedding[graph.oppositeArc(e)] = embedding[ve];
              embedding[ve] = graph.oppositeArc(e);
            } else {
              embedding[graph.oppositeArc(e)] = graph.oppositeArc(e);
            }
          }
        }
      }
    }

    template <typename Graph, typename EmbeddingMap>
    void makeBiNodeConnected(Graph& graph, EmbeddingMap& embedding) {
      typename Graph::template ArcMap<bool> processed(graph);

      std::vector<typename Graph::Arc> arcs;
      for (typename Graph::ArcIt e(graph); e != INVALID; ++e) {
        arcs.push_back(e);
      }

      IterableBoolMap<Graph, typename Graph::Node> visited(graph, false);

      for (int i = 0; i < int(arcs.size()); ++i) {
        typename Graph::Arc pp = arcs[i];
        if (processed[pp]) continue;

        typename Graph::Arc e = embedding[graph.oppositeArc(pp)];
        processed[e] = true;
        visited.set(graph.source(e), true);

        typename Graph::Arc p = e, l = e;
        e = embedding[graph.oppositeArc(e)];

        while (e != l) {
          processed[e] = true;

          if (visited[graph.source(e)]) {

            typename Graph::Arc n =
              graph.direct(graph.addEdge(graph.source(p),
                                           graph.target(e)), true);
            embedding[n] = p;
            embedding[graph.oppositeArc(pp)] = n;

            embedding[graph.oppositeArc(n)] =
              embedding[graph.oppositeArc(e)];
            embedding[graph.oppositeArc(e)] =
              graph.oppositeArc(n);

            p = n;
            e = embedding[graph.oppositeArc(n)];
          } else {
            visited.set(graph.source(e), true);
            pp = p;
            p = e;
            e = embedding[graph.oppositeArc(e)];
          }
        }
        visited.setAll(false);
      }
    }


    template <typename Graph, typename EmbeddingMap>
    void makeMaxPlanar(Graph& graph, EmbeddingMap& embedding) {

      typename Graph::template NodeMap<int> degree(graph);

      for (typename Graph::NodeIt n(graph); n != INVALID; ++n) {
        degree[n] = countIncEdges(graph, n);
      }

      typename Graph::template ArcMap<bool> processed(graph);
      IterableBoolMap<Graph, typename Graph::Node> visited(graph, false);

      std::vector<typename Graph::Arc> arcs;
      for (typename Graph::ArcIt e(graph); e != INVALID; ++e) {
        arcs.push_back(e);
      }

      for (int i = 0; i < int(arcs.size()); ++i) {
        typename Graph::Arc e = arcs[i];

        if (processed[e]) continue;
        processed[e] = true;

        typename Graph::Arc mine = e;
        int mind = degree[graph.source(e)];

        int face_size = 1;

        typename Graph::Arc l = e;
        e = embedding[graph.oppositeArc(e)];
        while (l != e) {
          processed[e] = true;

          ++face_size;

          if (degree[graph.source(e)] < mind) {
            mine = e;
            mind = degree[graph.source(e)];
          }

          e = embedding[graph.oppositeArc(e)];
        }

        if (face_size < 4) {
          continue;
        }

        typename Graph::Node s = graph.source(mine);
        for (typename Graph::OutArcIt e(graph, s); e != INVALID; ++e) {
          visited.set(graph.target(e), true);
        }

        typename Graph::Arc oppe = INVALID;

        e = embedding[graph.oppositeArc(mine)];
        e = embedding[graph.oppositeArc(e)];
        while (graph.target(e) != s) {
          if (visited[graph.source(e)]) {
            oppe = e;
            break;
          }
          e = embedding[graph.oppositeArc(e)];
        }
        visited.setAll(false);

        if (oppe == INVALID) {

          e = embedding[graph.oppositeArc(mine)];
          typename Graph::Arc pn = mine, p = e;

          e = embedding[graph.oppositeArc(e)];
          while (graph.target(e) != s) {
            typename Graph::Arc n =
              graph.direct(graph.addEdge(s, graph.source(e)), true);

            embedding[n] = pn;
            embedding[graph.oppositeArc(n)] = e;
            embedding[graph.oppositeArc(p)] = graph.oppositeArc(n);

            pn = n;

            p = e;
            e = embedding[graph.oppositeArc(e)];
          }

          embedding[graph.oppositeArc(e)] = pn;

        } else {

          mine = embedding[graph.oppositeArc(mine)];
          s = graph.source(mine);
          oppe = embedding[graph.oppositeArc(oppe)];
          typename Graph::Node t = graph.source(oppe);

          typename Graph::Arc ce = graph.direct(graph.addEdge(s, t), true);
          embedding[ce] = mine;
          embedding[graph.oppositeArc(ce)] = oppe;

          typename Graph::Arc pn = ce, p = oppe;
          e = embedding[graph.oppositeArc(oppe)];
          while (graph.target(e) != s) {
            typename Graph::Arc n =
              graph.direct(graph.addEdge(s, graph.source(e)), true);

            embedding[n] = pn;
            embedding[graph.oppositeArc(n)] = e;
            embedding[graph.oppositeArc(p)] = graph.oppositeArc(n);

            pn = n;

            p = e;
            e = embedding[graph.oppositeArc(e)];

          }
          embedding[graph.oppositeArc(e)] = pn;

          pn = graph.oppositeArc(ce), p = mine;
          e = embedding[graph.oppositeArc(mine)];
          while (graph.target(e) != t) {
            typename Graph::Arc n =
              graph.direct(graph.addEdge(t, graph.source(e)), true);

            embedding[n] = pn;
            embedding[graph.oppositeArc(n)] = e;
            embedding[graph.oppositeArc(p)] = graph.oppositeArc(n);

            pn = n;

            p = e;
            e = embedding[graph.oppositeArc(e)];

          }
          embedding[graph.oppositeArc(e)] = pn;
        }
      }
    }

  }

  /// \ingroup planar
  ///
  /// \brief Schnyder's planar drawing algorithm
  ///
  /// The planar drawing algorithm calculates positions for the nodes
  /// in the plane. These coordinates satisfy that if the edges are
  /// represented with straight lines, then they will not intersect
  /// each other.
  ///
  /// Scnyder's algorithm embeds the graph on an \c (n-2)x(n-2) size grid,
  /// i.e. each node will be located in the \c [0..n-2]x[0..n-2] square.
  /// The time complexity of the algorithm is O(n).
  ///
  /// \see PlanarEmbedding
  template <typename Graph>
  class PlanarDrawing {
  public:

    TEMPLATE_GRAPH_TYPEDEFS(Graph);

    /// \brief The point type for storing coordinates
    typedef dim2::Point<int> Point;
    /// \brief The map type for storing the coordinates of the nodes
    typedef typename Graph::template NodeMap<Point> PointMap;


    /// \brief Constructor
    ///
    /// Constructor
    /// \pre The graph must be simple, i.e. it should not
    /// contain parallel or loop arcs.
    PlanarDrawing(const Graph& graph)
      : _graph(graph), _point_map(graph) {}

  private:

    template <typename AuxGraph, typename AuxEmbeddingMap>
    void drawing(const AuxGraph& graph,
                 const AuxEmbeddingMap& next,
                 PointMap& point_map) {
      TEMPLATE_GRAPH_TYPEDEFS(AuxGraph);

      typename AuxGraph::template ArcMap<Arc> prev(graph);

      for (NodeIt n(graph); n != INVALID; ++n) {
        Arc e = OutArcIt(graph, n);

        Arc p = e, l = e;

        e = next[e];
        while (e != l) {
          prev[e] = p;
          p = e;
          e = next[e];
        }
        prev[e] = p;
      }

      Node anode, bnode, cnode;

      {
        Arc e = ArcIt(graph);
        anode = graph.source(e);
        bnode = graph.target(e);
        cnode = graph.target(next[graph.oppositeArc(e)]);
      }

      IterableBoolMap<AuxGraph, Node> proper(graph, false);
      typename AuxGraph::template NodeMap<int> conn(graph, -1);

      conn[anode] = conn[bnode] = -2;
      {
        for (OutArcIt e(graph, anode); e != INVALID; ++e) {
          Node m = graph.target(e);
          if (conn[m] == -1) {
            conn[m] = 1;
          }
        }
        conn[cnode] = 2;

        for (OutArcIt e(graph, bnode); e != INVALID; ++e) {
          Node m = graph.target(e);
          if (conn[m] == -1) {
            conn[m] = 1;
          } else if (conn[m] != -2) {
            conn[m] += 1;
            Arc pe = graph.oppositeArc(e);
            if (conn[graph.target(next[pe])] == -2) {
              conn[m] -= 1;
            }
            if (conn[graph.target(prev[pe])] == -2) {
              conn[m] -= 1;
            }

            proper.set(m, conn[m] == 1);
          }
        }
      }


      typename AuxGraph::template ArcMap<int> angle(graph, -1);

      while (proper.trueNum() != 0) {
        Node n = typename IterableBoolMap<AuxGraph, Node>::TrueIt(proper);
        proper.set(n, false);
        conn[n] = -2;

        for (OutArcIt e(graph, n); e != INVALID; ++e) {
          Node m = graph.target(e);
          if (conn[m] == -1) {
            conn[m] = 1;
          } else if (conn[m] != -2) {
            conn[m] += 1;
            Arc pe = graph.oppositeArc(e);
            if (conn[graph.target(next[pe])] == -2) {
              conn[m] -= 1;
            }
            if (conn[graph.target(prev[pe])] == -2) {
              conn[m] -= 1;
            }

            proper.set(m, conn[m] == 1);
          }
        }

        {
          Arc e = OutArcIt(graph, n);
          Arc p = e, l = e;

          e = next[e];
          while (e != l) {

            if (conn[graph.target(e)] == -2 && conn[graph.target(p)] == -2) {
              Arc f = e;
              angle[f] = 0;
              f = next[graph.oppositeArc(f)];
              angle[f] = 1;
              f = next[graph.oppositeArc(f)];
              angle[f] = 2;
            }

            p = e;
            e = next[e];
          }

          if (conn[graph.target(e)] == -2 && conn[graph.target(p)] == -2) {
            Arc f = e;
            angle[f] = 0;
            f = next[graph.oppositeArc(f)];
            angle[f] = 1;
            f = next[graph.oppositeArc(f)];
            angle[f] = 2;
          }
        }
      }

      typename AuxGraph::template NodeMap<Node> apred(graph, INVALID);
      typename AuxGraph::template NodeMap<Node> bpred(graph, INVALID);
      typename AuxGraph::template NodeMap<Node> cpred(graph, INVALID);

      typename AuxGraph::template NodeMap<int> apredid(graph, -1);
      typename AuxGraph::template NodeMap<int> bpredid(graph, -1);
      typename AuxGraph::template NodeMap<int> cpredid(graph, -1);

      for (ArcIt e(graph); e != INVALID; ++e) {
        if (angle[e] == angle[next[e]]) {
          switch (angle[e]) {
          case 2:
            apred[graph.target(e)] = graph.source(e);
            apredid[graph.target(e)] = graph.id(graph.source(e));
            break;
          case 1:
            bpred[graph.target(e)] = graph.source(e);
            bpredid[graph.target(e)] = graph.id(graph.source(e));
            break;
          case 0:
            cpred[graph.target(e)] = graph.source(e);
            cpredid[graph.target(e)] = graph.id(graph.source(e));
            break;
          }
        }
      }

      cpred[anode] = INVALID;
      cpred[bnode] = INVALID;

      std::vector<Node> aorder, border, corder;

      {
        typename AuxGraph::template NodeMap<bool> processed(graph, false);
        std::vector<Node> st;
        for (NodeIt n(graph); n != INVALID; ++n) {
          if (!processed[n] && n != bnode && n != cnode) {
            st.push_back(n);
            processed[n] = true;
            Node m = apred[n];
            while (m != INVALID && !processed[m]) {
              st.push_back(m);
              processed[m] = true;
              m = apred[m];
            }
            while (!st.empty()) {
              aorder.push_back(st.back());
              st.pop_back();
            }
          }
        }
      }

      {
        typename AuxGraph::template NodeMap<bool> processed(graph, false);
        std::vector<Node> st;
        for (NodeIt n(graph); n != INVALID; ++n) {
          if (!processed[n] && n != cnode && n != anode) {
            st.push_back(n);
            processed[n] = true;
            Node m = bpred[n];
            while (m != INVALID && !processed[m]) {
              st.push_back(m);
              processed[m] = true;
              m = bpred[m];
            }
            while (!st.empty()) {
              border.push_back(st.back());
              st.pop_back();
            }
          }
        }
      }

      {
        typename AuxGraph::template NodeMap<bool> processed(graph, false);
        std::vector<Node> st;
        for (NodeIt n(graph); n != INVALID; ++n) {
          if (!processed[n] && n != anode && n != bnode) {
            st.push_back(n);
            processed[n] = true;
            Node m = cpred[n];
            while (m != INVALID && !processed[m]) {
              st.push_back(m);
              processed[m] = true;
              m = cpred[m];
            }
            while (!st.empty()) {
              corder.push_back(st.back());
              st.pop_back();
            }
          }
        }
      }

      typename AuxGraph::template NodeMap<int> atree(graph, 0);
      for (int i = aorder.size() - 1; i >= 0; --i) {
        Node n = aorder[i];
        atree[n] = 1;
        for (OutArcIt e(graph, n); e != INVALID; ++e) {
          if (apred[graph.target(e)] == n) {
            atree[n] += atree[graph.target(e)];
          }
        }
      }

      typename AuxGraph::template NodeMap<int> btree(graph, 0);
      for (int i = border.size() - 1; i >= 0; --i) {
        Node n = border[i];
        btree[n] = 1;
        for (OutArcIt e(graph, n); e != INVALID; ++e) {
          if (bpred[graph.target(e)] == n) {
            btree[n] += btree[graph.target(e)];
          }
        }
      }

      typename AuxGraph::template NodeMap<int> apath(graph, 0);
      apath[bnode] = apath[cnode] = 1;
      typename AuxGraph::template NodeMap<int> apath_btree(graph, 0);
      apath_btree[bnode] = btree[bnode];
      for (int i = 1; i < int(aorder.size()); ++i) {
        Node n = aorder[i];
        apath[n] = apath[apred[n]] + 1;
        apath_btree[n] = btree[n] + apath_btree[apred[n]];
      }

      typename AuxGraph::template NodeMap<int> bpath_atree(graph, 0);
      bpath_atree[anode] = atree[anode];
      for (int i = 1; i < int(border.size()); ++i) {
        Node n = border[i];
        bpath_atree[n] = atree[n] + bpath_atree[bpred[n]];
      }

      typename AuxGraph::template NodeMap<int> cpath(graph, 0);
      cpath[anode] = cpath[bnode] = 1;
      typename AuxGraph::template NodeMap<int> cpath_atree(graph, 0);
      cpath_atree[anode] = atree[anode];
      typename AuxGraph::template NodeMap<int> cpath_btree(graph, 0);
      cpath_btree[bnode] = btree[bnode];
      for (int i = 1; i < int(corder.size()); ++i) {
        Node n = corder[i];
        cpath[n] = cpath[cpred[n]] + 1;
        cpath_atree[n] = atree[n] + cpath_atree[cpred[n]];
        cpath_btree[n] = btree[n] + cpath_btree[cpred[n]];
      }

      typename AuxGraph::template NodeMap<int> third(graph);
      for (NodeIt n(graph); n != INVALID; ++n) {
        point_map[n].x =
          bpath_atree[n] + cpath_atree[n] - atree[n] - cpath[n] + 1;
        point_map[n].y =
          cpath_btree[n] + apath_btree[n] - btree[n] - apath[n] + 1;
      }

    }

  public:

    /// \brief Calculate the node positions
    ///
    /// This function calculates the node positions on the plane.
    /// \return \c true if the graph is planar.
    bool run() {
      PlanarEmbedding<Graph> pe(_graph);
      if (!pe.run()) return false;

      run(pe);
      return true;
    }

    /// \brief Calculate the node positions according to a
    /// combinatorical embedding
    ///
    /// This function calculates the node positions on the plane.
    /// The given \c embedding map should contain a valid combinatorical
    /// embedding, i.e. a valid cyclic order of the arcs.
    /// It can be computed using PlanarEmbedding.
    template <typename EmbeddingMap>
    void run(const EmbeddingMap& embedding) {
      typedef SmartEdgeSet<Graph> AuxGraph;

      if (3 * countNodes(_graph) - 6 == countEdges(_graph)) {
        drawing(_graph, embedding, _point_map);
        return;
      }

      AuxGraph aux_graph(_graph);
      typename AuxGraph::template ArcMap<typename AuxGraph::Arc>
        aux_embedding(aux_graph);

      {

        typename Graph::template EdgeMap<typename AuxGraph::Edge>
          ref(_graph);

        for (EdgeIt e(_graph); e != INVALID; ++e) {
          ref[e] = aux_graph.addEdge(_graph.u(e), _graph.v(e));
        }

        for (EdgeIt e(_graph); e != INVALID; ++e) {
          Arc ee = embedding[_graph.direct(e, true)];
          aux_embedding[aux_graph.direct(ref[e], true)] =
            aux_graph.direct(ref[ee], _graph.direction(ee));
          ee = embedding[_graph.direct(e, false)];
          aux_embedding[aux_graph.direct(ref[e], false)] =
            aux_graph.direct(ref[ee], _graph.direction(ee));
        }
      }
      _planarity_bits::makeConnected(aux_graph, aux_embedding);
      _planarity_bits::makeBiNodeConnected(aux_graph, aux_embedding);
      _planarity_bits::makeMaxPlanar(aux_graph, aux_embedding);
      drawing(aux_graph, aux_embedding, _point_map);
    }

    /// \brief The coordinate of the given node
    ///
    /// This function returns the coordinate of the given node.
    Point operator[](const Node& node) const {
      return _point_map[node];
    }

    /// \brief Return the grid embedding in a node map
    ///
    /// This function returns the grid embedding in a node map of
    /// \c dim2::Point<int> coordinates.
    const PointMap& coords() const {
      return _point_map;
    }

  private:

    const Graph& _graph;
    PointMap _point_map;

  };

  namespace _planarity_bits {

    template <typename ColorMap>
    class KempeFilter {
    public:
      typedef typename ColorMap::Key Key;
      typedef bool Value;

      KempeFilter(const ColorMap& color_map,
                  const typename ColorMap::Value& first,
                  const typename ColorMap::Value& second)
        : _color_map(color_map), _first(first), _second(second) {}

      Value operator[](const Key& key) const {
        return _color_map[key] == _first || _color_map[key] == _second;
      }

    private:
      const ColorMap& _color_map;
      typename ColorMap::Value _first, _second;
    };
  }

  /// \ingroup planar
  ///
  /// \brief Coloring planar graphs
  ///
  /// The graph coloring problem is the coloring of the graph nodes
  /// so that there are no adjacent nodes with the same color. The
  /// planar graphs can always be colored with four colors, which is
  /// proved by Appel and Haken. Their proofs provide a quadratic
  /// time algorithm for four coloring, but it could not be used to
  /// implement an efficient algorithm. The five and six coloring can be
  /// made in linear time, but in this class, the five coloring has
  /// quadratic worst case time complexity. The two coloring (if
  /// possible) is solvable with a graph search algorithm and it is
  /// implemented in \ref bipartitePartitions() function in LEMON. To
  /// decide whether a planar graph is three colorable is NP-complete.
  ///
  /// This class contains member functions for calculate colorings
  /// with five and six colors. The six coloring algorithm is a simple
  /// greedy coloring on the backward minimum outgoing order of nodes.
  /// This order can be computed by selecting the node with least
  /// outgoing arcs to unprocessed nodes in each phase. This order
  /// guarantees that when a node is chosen for coloring it has at
  /// most five already colored adjacents. The five coloring algorithm
  /// use the same method, but if the greedy approach fails to color
  /// with five colors, i.e. the node has five already different
  /// colored neighbours, it swaps the colors in one of the connected
  /// two colored sets with the Kempe recoloring method.
  template <typename Graph>
  class PlanarColoring {
  public:

    TEMPLATE_GRAPH_TYPEDEFS(Graph);

    /// \brief The map type for storing color indices
    typedef typename Graph::template NodeMap<int> IndexMap;
    /// \brief The map type for storing colors
    ///
    /// The map type for storing colors.
    /// \see Palette, Color
    typedef ComposeMap<Palette, IndexMap> ColorMap;

    /// \brief Constructor
    ///
    /// Constructor.
    /// \pre The graph must be simple, i.e. it should not
    /// contain parallel or loop arcs.
    PlanarColoring(const Graph& graph)
      : _graph(graph), _color_map(graph), _palette(0) {
      _palette.add(Color(1,0,0));
      _palette.add(Color(0,1,0));
      _palette.add(Color(0,0,1));
      _palette.add(Color(1,1,0));
      _palette.add(Color(1,0,1));
      _palette.add(Color(0,1,1));
    }

    /// \brief Return the node map of color indices
    ///
    /// This function returns the node map of color indices. The values are
    /// in the range \c [0..4] or \c [0..5] according to the coloring method.
    IndexMap colorIndexMap() const {
      return _color_map;
    }

    /// \brief Return the node map of colors
    ///
    /// This function returns the node map of colors. The values are among
    /// five or six distinct \ref lemon::Color "colors".
    ColorMap colorMap() const {
      return composeMap(_palette, _color_map);
    }

    /// \brief Return the color index of the node
    ///
    /// This function returns the color index of the given node. The value is
    /// in the range \c [0..4] or \c [0..5] according to the coloring method.
    int colorIndex(const Node& node) const {
      return _color_map[node];
    }

    /// \brief Return the color of the node
    ///
    /// This function returns the color of the given node. The value is among
    /// five or six distinct \ref lemon::Color "colors".
    Color color(const Node& node) const {
      return _palette[_color_map[node]];
    }


    /// \brief Calculate a coloring with at most six colors
    ///
    /// This function calculates a coloring with at most six colors. The time
    /// complexity of this variant is linear in the size of the graph.
    /// \return \c true if the algorithm could color the graph with six colors.
    /// If the algorithm fails, then the graph is not planar.
    /// \note This function can return \c true if the graph is not
    /// planar, but it can be colored with at most six colors.
    bool runSixColoring() {

      typename Graph::template NodeMap<int> heap_index(_graph, -1);
      BucketHeap<typename Graph::template NodeMap<int> > heap(heap_index);

      for (NodeIt n(_graph); n != INVALID; ++n) {
        _color_map[n] = -2;
        heap.push(n, countOutArcs(_graph, n));
      }

      std::vector<Node> order;

      while (!heap.empty()) {
        Node n = heap.top();
        heap.pop();
        _color_map[n] = -1;
        order.push_back(n);
        for (OutArcIt e(_graph, n); e != INVALID; ++e) {
          Node t = _graph.runningNode(e);
          if (_color_map[t] == -2) {
            heap.decrease(t, heap[t] - 1);
          }
        }
      }

      for (int i = order.size() - 1; i >= 0; --i) {
        std::vector<bool> forbidden(6, false);
        for (OutArcIt e(_graph, order[i]); e != INVALID; ++e) {
          Node t = _graph.runningNode(e);
          if (_color_map[t] != -1) {
            forbidden[_color_map[t]] = true;
          }
        }
               for (int k = 0; k < 6; ++k) {
          if (!forbidden[k]) {
            _color_map[order[i]] = k;
            break;
          }
        }
        if (_color_map[order[i]] == -1) {
          return false;
        }
      }
      return true;
    }

  private:

    bool recolor(const Node& u, const Node& v) {
      int ucolor = _color_map[u];
      int vcolor = _color_map[v];
      typedef _planarity_bits::KempeFilter<IndexMap> KempeFilter;
      KempeFilter filter(_color_map, ucolor, vcolor);

      typedef FilterNodes<const Graph, const KempeFilter> KempeGraph;
      KempeGraph kempe_graph(_graph, filter);

      std::vector<Node> comp;
      Bfs<KempeGraph> bfs(kempe_graph);
      bfs.init();
      bfs.addSource(u);
      while (!bfs.emptyQueue()) {
        Node n = bfs.nextNode();
        if (n == v) return false;
        comp.push_back(n);
        bfs.processNextNode();
      }

      int scolor = ucolor + vcolor;
      for (int i = 0; i < static_cast<int>(comp.size()); ++i) {
        _color_map[comp[i]] = scolor - _color_map[comp[i]];
      }

      return true;
    }

    template <typename EmbeddingMap>
    void kempeRecoloring(const Node& node, const EmbeddingMap& embedding) {
      std::vector<Node> nodes;
      nodes.reserve(4);

      for (Arc e = OutArcIt(_graph, node); e != INVALID; e = embedding[e]) {
        Node t = _graph.target(e);
        if (_color_map[t] != -1) {
          nodes.push_back(t);
          if (nodes.size() == 4) break;
        }
      }

      int color = _color_map[nodes[0]];
      if (recolor(nodes[0], nodes[2])) {
        _color_map[node] = color;
      } else {
        color = _color_map[nodes[1]];
        recolor(nodes[1], nodes[3]);
        _color_map[node] = color;
      }
    }

  public:

    /// \brief Calculate a coloring with at most five colors
    ///
    /// This function calculates a coloring with at most five
    /// colors. The worst case time complexity of this variant is
    /// quadratic in the size of the graph.
    /// \param embedding This map should contain a valid combinatorical
    /// embedding, i.e. a valid cyclic order of the arcs.
    /// It can be computed using PlanarEmbedding.
    template <typename EmbeddingMap>
    void runFiveColoring(const EmbeddingMap& embedding) {

      typename Graph::template NodeMap<int> heap_index(_graph, -1);
      BucketHeap<typename Graph::template NodeMap<int> > heap(heap_index);

      for (NodeIt n(_graph); n != INVALID; ++n) {
        _color_map[n] = -2;
        heap.push(n, countOutArcs(_graph, n));
      }

      std::vector<Node> order;

      while (!heap.empty()) {
        Node n = heap.top();
        heap.pop();
        _color_map[n] = -1;
        order.push_back(n);
        for (OutArcIt e(_graph, n); e != INVALID; ++e) {
          Node t = _graph.runningNode(e);
          if (_color_map[t] == -2) {
            heap.decrease(t, heap[t] - 1);
          }
        }
      }

      for (int i = order.size() - 1; i >= 0; --i) {
        std::vector<bool> forbidden(5, false);
        for (OutArcIt e(_graph, order[i]); e != INVALID; ++e) {
          Node t = _graph.runningNode(e);
          if (_color_map[t] != -1) {
            forbidden[_color_map[t]] = true;
          }
        }
        for (int k = 0; k < 5; ++k) {
          if (!forbidden[k]) {
            _color_map[order[i]] = k;
            break;
          }
        }
        if (_color_map[order[i]] == -1) {
          kempeRecoloring(order[i], embedding);
        }
      }
    }

    /// \brief Calculate a coloring with at most five colors
    ///
    /// This function calculates a coloring with at most five
    /// colors. The worst case time complexity of this variant is
    /// quadratic in the size of the graph.
    /// \return \c true if the graph is planar.
    bool runFiveColoring() {
      PlanarEmbedding<Graph> pe(_graph);
      if (!pe.run()) return false;

      runFiveColoring(pe.embeddingMap());
      return true;
    }

  private:

    const Graph& _graph;
    IndexMap _color_map;
    Palette _palette;
  };

}

#endif
