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

#ifndef LEMON_LIST_GRAPH_H
#define LEMON_LIST_GRAPH_H

///\ingroup graphs
///\file
///\brief ListDigraph and ListGraph classes.

#include <lemon/core.h>
#include <lemon/error.h>
#include <lemon/bits/graph_extender.h>

#include <vector>
#include <list>

namespace lemon {

  class ListDigraph;

  class ListDigraphBase {

  protected:
    struct NodeT {
      int first_in, first_out;
      int prev, next;
    };

    struct ArcT {
      int target, source;
      int prev_in, prev_out;
      int next_in, next_out;
    };

    std::vector<NodeT> nodes;

    int first_node;

    int first_free_node;

    std::vector<ArcT> arcs;

    int first_free_arc;

  public:

    typedef ListDigraphBase Digraph;

    class Node {
      friend class ListDigraphBase;
      friend class ListDigraph;
    protected:

      int id;
      explicit Node(int pid) { id = pid;}

    public:
      Node() {}
      Node (Invalid) { id = -1; }
      bool operator==(const Node& node) const {return id == node.id;}
      bool operator!=(const Node& node) const {return id != node.id;}
      bool operator<(const Node& node) const {return id < node.id;}
    };

    class Arc {
      friend class ListDigraphBase;
      friend class ListDigraph;
    protected:

      int id;
      explicit Arc(int pid) { id = pid;}

    public:
      Arc() {}
      Arc (Invalid) { id = -1; }
      bool operator==(const Arc& arc) const {return id == arc.id;}
      bool operator!=(const Arc& arc) const {return id != arc.id;}
      bool operator<(const Arc& arc) const {return id < arc.id;}
    };



    ListDigraphBase()
      : nodes(), first_node(-1),
        first_free_node(-1), arcs(), first_free_arc(-1) {}


    int maxNodeId() const { return nodes.size()-1; }
    int maxArcId() const { return arcs.size()-1; }

    Node source(Arc e) const { return Node(arcs[e.id].source); }
    Node target(Arc e) const { return Node(arcs[e.id].target); }


    void first(Node& node) const {
      node.id = first_node;
    }

    void next(Node& node) const {
      node.id = nodes[node.id].next;
    }


    void first(Arc& arc) const {
      int n;
      for(n = first_node;
          n != -1 && nodes[n].first_out == -1;
          n = nodes[n].next) {}
      arc.id = (n == -1) ? -1 : nodes[n].first_out;
    }

    void next(Arc& arc) const {
      if (arcs[arc.id].next_out != -1) {
        arc.id = arcs[arc.id].next_out;
      } else {
        int n;
        for(n = nodes[arcs[arc.id].source].next;
            n != -1 && nodes[n].first_out == -1;
            n = nodes[n].next) {}
        arc.id = (n == -1) ? -1 : nodes[n].first_out;
      }
    }

    void firstOut(Arc &e, const Node& v) const {
      e.id = nodes[v.id].first_out;
    }
    void nextOut(Arc &e) const {
      e.id=arcs[e.id].next_out;
    }

    void firstIn(Arc &e, const Node& v) const {
      e.id = nodes[v.id].first_in;
    }
    void nextIn(Arc &e) const {
      e.id=arcs[e.id].next_in;
    }


    static int id(Node v) { return v.id; }
    static int id(Arc e) { return e.id; }

    static Node nodeFromId(int id) { return Node(id);}
    static Arc arcFromId(int id) { return Arc(id);}

    bool valid(Node n) const {
      return n.id >= 0 && n.id < static_cast<int>(nodes.size()) &&
        nodes[n.id].prev != -2;
    }

    bool valid(Arc a) const {
      return a.id >= 0 && a.id < static_cast<int>(arcs.size()) &&
        arcs[a.id].prev_in != -2;
    }

    Node addNode() {
      int n;

      if(first_free_node==-1) {
        n = nodes.size();
        nodes.push_back(NodeT());
      } else {
        n = first_free_node;
        first_free_node = nodes[n].next;
      }

      nodes[n].next = first_node;
      if(first_node != -1) nodes[first_node].prev = n;
      first_node = n;
      nodes[n].prev = -1;

      nodes[n].first_in = nodes[n].first_out = -1;

      return Node(n);
    }

    Arc addArc(Node u, Node v) {
      int n;

      if (first_free_arc == -1) {
        n = arcs.size();
        arcs.push_back(ArcT());
      } else {
        n = first_free_arc;
        first_free_arc = arcs[n].next_in;
      }

      arcs[n].source = u.id;
      arcs[n].target = v.id;

      arcs[n].next_out = nodes[u.id].first_out;
      if(nodes[u.id].first_out != -1) {
        arcs[nodes[u.id].first_out].prev_out = n;
      }

      arcs[n].next_in = nodes[v.id].first_in;
      if(nodes[v.id].first_in != -1) {
        arcs[nodes[v.id].first_in].prev_in = n;
      }

      arcs[n].prev_in = arcs[n].prev_out = -1;

      nodes[u.id].first_out = nodes[v.id].first_in = n;

      return Arc(n);
    }

    void erase(const Node& node) {
      int n = node.id;

      if(nodes[n].next != -1) {
        nodes[nodes[n].next].prev = nodes[n].prev;
      }

      if(nodes[n].prev != -1) {
        nodes[nodes[n].prev].next = nodes[n].next;
      } else {
        first_node = nodes[n].next;
      }

      nodes[n].next = first_free_node;
      first_free_node = n;
      nodes[n].prev = -2;

    }

    void erase(const Arc& arc) {
      int n = arc.id;

      if(arcs[n].next_in!=-1) {
        arcs[arcs[n].next_in].prev_in = arcs[n].prev_in;
      }

      if(arcs[n].prev_in!=-1) {
        arcs[arcs[n].prev_in].next_in = arcs[n].next_in;
      } else {
        nodes[arcs[n].target].first_in = arcs[n].next_in;
      }


      if(arcs[n].next_out!=-1) {
        arcs[arcs[n].next_out].prev_out = arcs[n].prev_out;
      }

      if(arcs[n].prev_out!=-1) {
        arcs[arcs[n].prev_out].next_out = arcs[n].next_out;
      } else {
        nodes[arcs[n].source].first_out = arcs[n].next_out;
      }

      arcs[n].next_in = first_free_arc;
      first_free_arc = n;
      arcs[n].prev_in = -2;
    }

    void clear() {
      arcs.clear();
      nodes.clear();
      first_node = first_free_node = first_free_arc = -1;
    }

  protected:
    void changeTarget(Arc e, Node n)
    {
      if(arcs[e.id].next_in != -1)
        arcs[arcs[e.id].next_in].prev_in = arcs[e.id].prev_in;
      if(arcs[e.id].prev_in != -1)
        arcs[arcs[e.id].prev_in].next_in = arcs[e.id].next_in;
      else nodes[arcs[e.id].target].first_in = arcs[e.id].next_in;
      if (nodes[n.id].first_in != -1) {
        arcs[nodes[n.id].first_in].prev_in = e.id;
      }
      arcs[e.id].target = n.id;
      arcs[e.id].prev_in = -1;
      arcs[e.id].next_in = nodes[n.id].first_in;
      nodes[n.id].first_in = e.id;
    }
    void changeSource(Arc e, Node n)
    {
      if(arcs[e.id].next_out != -1)
        arcs[arcs[e.id].next_out].prev_out = arcs[e.id].prev_out;
      if(arcs[e.id].prev_out != -1)
        arcs[arcs[e.id].prev_out].next_out = arcs[e.id].next_out;
      else nodes[arcs[e.id].source].first_out = arcs[e.id].next_out;
      if (nodes[n.id].first_out != -1) {
        arcs[nodes[n.id].first_out].prev_out = e.id;
      }
      arcs[e.id].source = n.id;
      arcs[e.id].prev_out = -1;
      arcs[e.id].next_out = nodes[n.id].first_out;
      nodes[n.id].first_out = e.id;
    }

  };

  typedef DigraphExtender<ListDigraphBase> ExtendedListDigraphBase;

  /// \addtogroup graphs
  /// @{

  ///A general directed graph structure.

  ///\ref ListDigraph is a versatile and fast directed graph
  ///implementation based on linked lists that are stored in
  ///\c std::vector structures.
  ///
  ///This type fully conforms to the \ref concepts::Digraph "Digraph concept"
  ///and it also provides several useful additional functionalities.
  ///Most of its member functions and nested classes are documented
  ///only in the concept class.
  ///
  ///This class provides only linear time counting for nodes and arcs.
  ///
  ///\sa concepts::Digraph
  ///\sa ListGraph
  class ListDigraph : public ExtendedListDigraphBase {
    typedef ExtendedListDigraphBase Parent;

  private:
    /// Digraphs are \e not copy constructible. Use DigraphCopy instead.
    ListDigraph(const ListDigraph &) :ExtendedListDigraphBase() {};
    /// \brief Assignment of a digraph to another one is \e not allowed.
    /// Use DigraphCopy instead.
    void operator=(const ListDigraph &) {}
  public:

    /// Constructor

    /// Constructor.
    ///
    ListDigraph() {}

    ///Add a new node to the digraph.

    ///This function adds a new node to the digraph.
    ///\return The new node.
    Node addNode() { return Parent::addNode(); }

    ///Add a new arc to the digraph.

    ///This function adds a new arc to the digraph with source node \c s
    ///and target node \c t.
    ///\return The new arc.
    Arc addArc(Node s, Node t) {
      return Parent::addArc(s, t);
    }

    ///\brief Erase a node from the digraph.
    ///
    ///This function erases the given node along with its outgoing and
    ///incoming arcs from the digraph.
    ///
    ///\note All iterators referencing the removed node or the connected
    ///arcs are invalidated, of course.
    void erase(Node n) { Parent::erase(n); }

    ///\brief Erase an arc from the digraph.
    ///
    ///This function erases the given arc from the digraph.
    ///
    ///\note All iterators referencing the removed arc are invalidated,
    ///of course.
    void erase(Arc a) { Parent::erase(a); }

    /// Node validity check

    /// This function gives back \c true if the given node is valid,
    /// i.e. it is a real node of the digraph.
    ///
    /// \warning A removed node could become valid again if new nodes are
    /// added to the digraph.
    bool valid(Node n) const { return Parent::valid(n); }

    /// Arc validity check

    /// This function gives back \c true if the given arc is valid,
    /// i.e. it is a real arc of the digraph.
    ///
    /// \warning A removed arc could become valid again if new arcs are
    /// added to the digraph.
    bool valid(Arc a) const { return Parent::valid(a); }

    /// Change the target node of an arc

    /// This function changes the target node of the given arc \c a to \c n.
    ///
    ///\note \c ArcIt and \c OutArcIt iterators referencing the changed
    ///arc remain valid, but \c InArcIt iterators are invalidated.
    ///
    ///\warning This functionality cannot be used together with the Snapshot
    ///feature.
    void changeTarget(Arc a, Node n) {
      Parent::changeTarget(a,n);
    }
    /// Change the source node of an arc

    /// This function changes the source node of the given arc \c a to \c n.
    ///
    ///\note \c InArcIt iterators referencing the changed arc remain
    ///valid, but \c ArcIt and \c OutArcIt iterators are invalidated.
    ///
    ///\warning This functionality cannot be used together with the Snapshot
    ///feature.
    void changeSource(Arc a, Node n) {
      Parent::changeSource(a,n);
    }

    /// Reverse the direction of an arc.

    /// This function reverses the direction of the given arc.
    ///\note \c ArcIt, \c OutArcIt and \c InArcIt iterators referencing
    ///the changed arc are invalidated.
    ///
    ///\warning This functionality cannot be used together with the Snapshot
    ///feature.
    void reverseArc(Arc a) {
      Node t=target(a);
      changeTarget(a,source(a));
      changeSource(a,t);
    }

    ///Contract two nodes.

    ///This function contracts the given two nodes.
    ///Node \c v is removed, but instead of deleting its
    ///incident arcs, they are joined to node \c u.
    ///If the last parameter \c r is \c true (this is the default value),
    ///then the newly created loops are removed.
    ///
    ///\note The moved arcs are joined to node \c u using changeSource()
    ///or changeTarget(), thus \c ArcIt and \c OutArcIt iterators are
    ///invalidated for the outgoing arcs of node \c v and \c InArcIt
    ///iterators are invalidated for the incoming arcs of \c v.
    ///Moreover all iterators referencing node \c v or the removed
    ///loops are also invalidated. Other iterators remain valid.
    ///
    ///\warning This functionality cannot be used together with the Snapshot
    ///feature.
    void contract(Node u, Node v, bool r = true)
    {
      for(OutArcIt e(*this,v);e!=INVALID;) {
        OutArcIt f=e;
        ++f;
        if(r && target(e)==u) erase(e);
        else changeSource(e,u);
        e=f;
      }
      for(InArcIt e(*this,v);e!=INVALID;) {
        InArcIt f=e;
        ++f;
        if(r && source(e)==u) erase(e);
        else changeTarget(e,u);
        e=f;
      }
      erase(v);
    }

    ///Split a node.

    ///This function splits the given node. First, a new node is added
    ///to the digraph, then the source of each outgoing arc of node \c n
    ///is moved to this new node.
    ///If the second parameter \c connect is \c true (this is the default
    ///value), then a new arc from node \c n to the newly created node
    ///is also added.
    ///\return The newly created node.
    ///
    ///\note All iterators remain valid.
    ///
    ///\warning This functionality cannot be used together with the
    ///Snapshot feature.
    Node split(Node n, bool connect = true) {
      Node b = addNode();
      nodes[b.id].first_out=nodes[n.id].first_out;
      nodes[n.id].first_out=-1;
      for(int i=nodes[b.id].first_out; i!=-1; i=arcs[i].next_out) {
        arcs[i].source=b.id;
      }
      if (connect) addArc(n,b);
      return b;
    }

    ///Split an arc.

    ///This function splits the given arc. First, a new node \c v is
    ///added to the digraph, then the target node of the original arc
    ///is set to \c v. Finally, an arc from \c v to the original target
    ///is added.
    ///\return The newly created node.
    ///
    ///\note \c InArcIt iterators referencing the original arc are
    ///invalidated. Other iterators remain valid.
    ///
    ///\warning This functionality cannot be used together with the
    ///Snapshot feature.
    Node split(Arc a) {
      Node v = addNode();
      addArc(v,target(a));
      changeTarget(a,v);
      return v;
    }

    ///Clear the digraph.

    ///This function erases all nodes and arcs from the digraph.
    ///
    ///\note All iterators of the digraph are invalidated, of course.
    void clear() {
      Parent::clear();
    }

    /// Reserve memory for nodes.

    /// Using this function, it is possible to avoid superfluous memory
    /// allocation: if you know that the digraph you want to build will
    /// be large (e.g. it will contain millions of nodes and/or arcs),
    /// then it is worth reserving space for this amount before starting
    /// to build the digraph.
    /// \sa reserveArc()
    void reserveNode(int n) { nodes.reserve(n); };

    /// Reserve memory for arcs.

    /// Using this function, it is possible to avoid superfluous memory
    /// allocation: if you know that the digraph you want to build will
    /// be large (e.g. it will contain millions of nodes and/or arcs),
    /// then it is worth reserving space for this amount before starting
    /// to build the digraph.
    /// \sa reserveNode()
    void reserveArc(int m) { arcs.reserve(m); };

    /// \brief Class to make a snapshot of the digraph and restore
    /// it later.
    ///
    /// Class to make a snapshot of the digraph and restore it later.
    ///
    /// The newly added nodes and arcs can be removed using the
    /// restore() function.
    ///
    /// \note After a state is restored, you cannot restore a later state,
    /// i.e. you cannot add the removed nodes and arcs again using
    /// another Snapshot instance.
    ///
    /// \warning Node and arc deletions and other modifications (e.g.
    /// reversing, contracting, splitting arcs or nodes) cannot be
    /// restored. These events invalidate the snapshot.
    /// However, the arcs and nodes that were added to the digraph after
    /// making the current snapshot can be removed without invalidating it.
    class Snapshot {
    protected:

      typedef Parent::NodeNotifier NodeNotifier;

      class NodeObserverProxy : public NodeNotifier::ObserverBase {
      public:

        NodeObserverProxy(Snapshot& _snapshot)
          : snapshot(_snapshot) {}

        using NodeNotifier::ObserverBase::attach;
        using NodeNotifier::ObserverBase::detach;
        using NodeNotifier::ObserverBase::attached;

      protected:

        virtual void add(const Node& node) {
          snapshot.addNode(node);
        }
        virtual void add(const std::vector<Node>& nodes) {
          for (int i = nodes.size() - 1; i >= 0; ++i) {
            snapshot.addNode(nodes[i]);
          }
        }
        virtual void erase(const Node& node) {
          snapshot.eraseNode(node);
        }
        virtual void erase(const std::vector<Node>& nodes) {
          for (int i = 0; i < int(nodes.size()); ++i) {
            snapshot.eraseNode(nodes[i]);
          }
        }
        virtual void build() {
          Node node;
          std::vector<Node> nodes;
          for (notifier()->first(node); node != INVALID;
               notifier()->next(node)) {
            nodes.push_back(node);
          }
          for (int i = nodes.size() - 1; i >= 0; --i) {
            snapshot.addNode(nodes[i]);
          }
        }
        virtual void clear() {
          Node node;
          for (notifier()->first(node); node != INVALID;
               notifier()->next(node)) {
            snapshot.eraseNode(node);
          }
        }

        Snapshot& snapshot;
      };

      class ArcObserverProxy : public ArcNotifier::ObserverBase {
      public:

        ArcObserverProxy(Snapshot& _snapshot)
          : snapshot(_snapshot) {}

        using ArcNotifier::ObserverBase::attach;
        using ArcNotifier::ObserverBase::detach;
        using ArcNotifier::ObserverBase::attached;

      protected:

        virtual void add(const Arc& arc) {
          snapshot.addArc(arc);
        }
        virtual void add(const std::vector<Arc>& arcs) {
          for (int i = arcs.size() - 1; i >= 0; ++i) {
            snapshot.addArc(arcs[i]);
          }
        }
        virtual void erase(const Arc& arc) {
          snapshot.eraseArc(arc);
        }
        virtual void erase(const std::vector<Arc>& arcs) {
          for (int i = 0; i < int(arcs.size()); ++i) {
            snapshot.eraseArc(arcs[i]);
          }
        }
        virtual void build() {
          Arc arc;
          std::vector<Arc> arcs;
          for (notifier()->first(arc); arc != INVALID;
               notifier()->next(arc)) {
            arcs.push_back(arc);
          }
          for (int i = arcs.size() - 1; i >= 0; --i) {
            snapshot.addArc(arcs[i]);
          }
        }
        virtual void clear() {
          Arc arc;
          for (notifier()->first(arc); arc != INVALID;
               notifier()->next(arc)) {
            snapshot.eraseArc(arc);
          }
        }

        Snapshot& snapshot;
      };

      ListDigraph *digraph;

      NodeObserverProxy node_observer_proxy;
      ArcObserverProxy arc_observer_proxy;

      std::list<Node> added_nodes;
      std::list<Arc> added_arcs;


      void addNode(const Node& node) {
        added_nodes.push_front(node);
      }
      void eraseNode(const Node& node) {
        std::list<Node>::iterator it =
          std::find(added_nodes.begin(), added_nodes.end(), node);
        if (it == added_nodes.end()) {
          clear();
          arc_observer_proxy.detach();
          throw NodeNotifier::ImmediateDetach();
        } else {
          added_nodes.erase(it);
        }
      }

      void addArc(const Arc& arc) {
        added_arcs.push_front(arc);
      }
      void eraseArc(const Arc& arc) {
        std::list<Arc>::iterator it =
          std::find(added_arcs.begin(), added_arcs.end(), arc);
        if (it == added_arcs.end()) {
          clear();
          node_observer_proxy.detach();
          throw ArcNotifier::ImmediateDetach();
        } else {
          added_arcs.erase(it);
        }
      }

      void attach(ListDigraph &_digraph) {
        digraph = &_digraph;
        node_observer_proxy.attach(digraph->notifier(Node()));
        arc_observer_proxy.attach(digraph->notifier(Arc()));
      }

      void detach() {
        node_observer_proxy.detach();
        arc_observer_proxy.detach();
      }

      bool attached() const {
        return node_observer_proxy.attached();
      }

      void clear() {
        added_nodes.clear();
        added_arcs.clear();
      }

    public:

      /// \brief Default constructor.
      ///
      /// Default constructor.
      /// You have to call save() to actually make a snapshot.
      Snapshot()
        : digraph(0), node_observer_proxy(*this),
          arc_observer_proxy(*this) {}

      /// \brief Constructor that immediately makes a snapshot.
      ///
      /// This constructor immediately makes a snapshot of the given digraph.
      Snapshot(ListDigraph &gr)
        : node_observer_proxy(*this),
          arc_observer_proxy(*this) {
        attach(gr);
      }

      /// \brief Make a snapshot.
      ///
      /// This function makes a snapshot of the given digraph.
      /// It can be called more than once. In case of a repeated
      /// call, the previous snapshot gets lost.
      void save(ListDigraph &gr) {
        if (attached()) {
          detach();
          clear();
        }
        attach(gr);
      }

      /// \brief Undo the changes until the last snapshot.
      ///
      /// This function undos the changes until the last snapshot
      /// created by save() or Snapshot(ListDigraph&).
      ///
      /// \warning This method invalidates the snapshot, i.e. repeated
      /// restoring is not supported unless you call save() again.
      void restore() {
        detach();
        for(std::list<Arc>::iterator it = added_arcs.begin();
            it != added_arcs.end(); ++it) {
          digraph->erase(*it);
        }
        for(std::list<Node>::iterator it = added_nodes.begin();
            it != added_nodes.end(); ++it) {
          digraph->erase(*it);
        }
        clear();
      }

      /// \brief Returns \c true if the snapshot is valid.
      ///
      /// This function returns \c true if the snapshot is valid.
      bool valid() const {
        return attached();
      }
    };

  };

  ///@}

  class ListGraphBase {

  protected:

    struct NodeT {
      int first_out;
      int prev, next;
    };

    struct ArcT {
      int target;
      int prev_out, next_out;
    };

    std::vector<NodeT> nodes;

    int first_node;

    int first_free_node;

    std::vector<ArcT> arcs;

    int first_free_arc;

  public:

    typedef ListGraphBase Graph;

    class Node {
      friend class ListGraphBase;
    protected:

      int id;
      explicit Node(int pid) { id = pid;}

    public:
      Node() {}
      Node (Invalid) { id = -1; }
      bool operator==(const Node& node) const {return id == node.id;}
      bool operator!=(const Node& node) const {return id != node.id;}
      bool operator<(const Node& node) const {return id < node.id;}
    };

    class Edge {
      friend class ListGraphBase;
    protected:

      int id;
      explicit Edge(int pid) { id = pid;}

    public:
      Edge() {}
      Edge (Invalid) { id = -1; }
      bool operator==(const Edge& edge) const {return id == edge.id;}
      bool operator!=(const Edge& edge) const {return id != edge.id;}
      bool operator<(const Edge& edge) const {return id < edge.id;}
    };

    class Arc {
      friend class ListGraphBase;
    protected:

      int id;
      explicit Arc(int pid) { id = pid;}

    public:
      operator Edge() const {
        return id != -1 ? edgeFromId(id / 2) : INVALID;
      }

      Arc() {}
      Arc (Invalid) { id = -1; }
      bool operator==(const Arc& arc) const {return id == arc.id;}
      bool operator!=(const Arc& arc) const {return id != arc.id;}
      bool operator<(const Arc& arc) const {return id < arc.id;}
    };

    ListGraphBase()
      : nodes(), first_node(-1),
        first_free_node(-1), arcs(), first_free_arc(-1) {}


    int maxNodeId() const { return nodes.size()-1; }
    int maxEdgeId() const { return arcs.size() / 2 - 1; }
    int maxArcId() const { return arcs.size()-1; }

    Node source(Arc e) const { return Node(arcs[e.id ^ 1].target); }
    Node target(Arc e) const { return Node(arcs[e.id].target); }

    Node u(Edge e) const { return Node(arcs[2 * e.id].target); }
    Node v(Edge e) const { return Node(arcs[2 * e.id + 1].target); }

    static bool direction(Arc e) {
      return (e.id & 1) == 1;
    }

    static Arc direct(Edge e, bool d) {
      return Arc(e.id * 2 + (d ? 1 : 0));
    }

    void first(Node& node) const {
      node.id = first_node;
    }

    void next(Node& node) const {
      node.id = nodes[node.id].next;
    }

    void first(Arc& e) const {
      int n = first_node;
      while (n != -1 && nodes[n].first_out == -1) {
        n = nodes[n].next;
      }
      e.id = (n == -1) ? -1 : nodes[n].first_out;
    }

    void next(Arc& e) const {
      if (arcs[e.id].next_out != -1) {
        e.id = arcs[e.id].next_out;
      } else {
        int n = nodes[arcs[e.id ^ 1].target].next;
        while(n != -1 && nodes[n].first_out == -1) {
          n = nodes[n].next;
        }
        e.id = (n == -1) ? -1 : nodes[n].first_out;
      }
    }

    void first(Edge& e) const {
      int n = first_node;
      while (n != -1) {
        e.id = nodes[n].first_out;
        while ((e.id & 1) != 1) {
          e.id = arcs[e.id].next_out;
        }
        if (e.id != -1) {
          e.id /= 2;
          return;
        }
        n = nodes[n].next;
      }
      e.id = -1;
    }

    void next(Edge& e) const {
      int n = arcs[e.id * 2].target;
      e.id = arcs[(e.id * 2) | 1].next_out;
      while ((e.id & 1) != 1) {
        e.id = arcs[e.id].next_out;
      }
      if (e.id != -1) {
        e.id /= 2;
        return;
      }
      n = nodes[n].next;
      while (n != -1) {
        e.id = nodes[n].first_out;
        while ((e.id & 1) != 1) {
          e.id = arcs[e.id].next_out;
        }
        if (e.id != -1) {
          e.id /= 2;
          return;
        }
        n = nodes[n].next;
      }
      e.id = -1;
    }

    void firstOut(Arc &e, const Node& v) const {
      e.id = nodes[v.id].first_out;
    }
    void nextOut(Arc &e) const {
      e.id = arcs[e.id].next_out;
    }

    void firstIn(Arc &e, const Node& v) const {
      e.id = ((nodes[v.id].first_out) ^ 1);
      if (e.id == -2) e.id = -1;
    }
    void nextIn(Arc &e) const {
      e.id = ((arcs[e.id ^ 1].next_out) ^ 1);
      if (e.id == -2) e.id = -1;
    }

    void firstInc(Edge &e, bool& d, const Node& v) const {
      int a = nodes[v.id].first_out;
      if (a != -1 ) {
        e.id = a / 2;
        d = ((a & 1) == 1);
      } else {
        e.id = -1;
        d = true;
      }
    }
    void nextInc(Edge &e, bool& d) const {
      int a = (arcs[(e.id * 2) | (d ? 1 : 0)].next_out);
      if (a != -1 ) {
        e.id = a / 2;
        d = ((a & 1) == 1);
      } else {
        e.id = -1;
        d = true;
      }
    }

    static int id(Node v) { return v.id; }
    static int id(Arc e) { return e.id; }
    static int id(Edge e) { return e.id; }

    static Node nodeFromId(int id) { return Node(id);}
    static Arc arcFromId(int id) { return Arc(id);}
    static Edge edgeFromId(int id) { return Edge(id);}

    bool valid(Node n) const {
      return n.id >= 0 && n.id < static_cast<int>(nodes.size()) &&
        nodes[n.id].prev != -2;
    }

    bool valid(Arc a) const {
      return a.id >= 0 && a.id < static_cast<int>(arcs.size()) &&
        arcs[a.id].prev_out != -2;
    }

    bool valid(Edge e) const {
      return e.id >= 0 && 2 * e.id < static_cast<int>(arcs.size()) &&
        arcs[2 * e.id].prev_out != -2;
    }

    Node addNode() {
      int n;

      if(first_free_node==-1) {
        n = nodes.size();
        nodes.push_back(NodeT());
      } else {
        n = first_free_node;
        first_free_node = nodes[n].next;
      }

      nodes[n].next = first_node;
      if (first_node != -1) nodes[first_node].prev = n;
      first_node = n;
      nodes[n].prev = -1;

      nodes[n].first_out = -1;

      return Node(n);
    }

    Edge addEdge(Node u, Node v) {
      int n;

      if (first_free_arc == -1) {
        n = arcs.size();
        arcs.push_back(ArcT());
        arcs.push_back(ArcT());
      } else {
        n = first_free_arc;
        first_free_arc = arcs[n].next_out;
      }

      arcs[n].target = u.id;
      arcs[n | 1].target = v.id;

      arcs[n].next_out = nodes[v.id].first_out;
      if (nodes[v.id].first_out != -1) {
        arcs[nodes[v.id].first_out].prev_out = n;
      }
      arcs[n].prev_out = -1;
      nodes[v.id].first_out = n;

      arcs[n | 1].next_out = nodes[u.id].first_out;
      if (nodes[u.id].first_out != -1) {
        arcs[nodes[u.id].first_out].prev_out = (n | 1);
      }
      arcs[n | 1].prev_out = -1;
      nodes[u.id].first_out = (n | 1);

      return Edge(n / 2);
    }

    void erase(const Node& node) {
      int n = node.id;

      if(nodes[n].next != -1) {
        nodes[nodes[n].next].prev = nodes[n].prev;
      }

      if(nodes[n].prev != -1) {
        nodes[nodes[n].prev].next = nodes[n].next;
      } else {
        first_node = nodes[n].next;
      }

      nodes[n].next = first_free_node;
      first_free_node = n;
      nodes[n].prev = -2;
    }

    void erase(const Edge& edge) {
      int n = edge.id * 2;

      if (arcs[n].next_out != -1) {
        arcs[arcs[n].next_out].prev_out = arcs[n].prev_out;
      }

      if (arcs[n].prev_out != -1) {
        arcs[arcs[n].prev_out].next_out = arcs[n].next_out;
      } else {
        nodes[arcs[n | 1].target].first_out = arcs[n].next_out;
      }

      if (arcs[n | 1].next_out != -1) {
        arcs[arcs[n | 1].next_out].prev_out = arcs[n | 1].prev_out;
      }

      if (arcs[n | 1].prev_out != -1) {
        arcs[arcs[n | 1].prev_out].next_out = arcs[n | 1].next_out;
      } else {
        nodes[arcs[n].target].first_out = arcs[n | 1].next_out;
      }

      arcs[n].next_out = first_free_arc;
      first_free_arc = n;
      arcs[n].prev_out = -2;
      arcs[n | 1].prev_out = -2;

    }

    void clear() {
      arcs.clear();
      nodes.clear();
      first_node = first_free_node = first_free_arc = -1;
    }

  protected:

    void changeV(Edge e, Node n) {
      if(arcs[2 * e.id].next_out != -1) {
        arcs[arcs[2 * e.id].next_out].prev_out = arcs[2 * e.id].prev_out;
      }
      if(arcs[2 * e.id].prev_out != -1) {
        arcs[arcs[2 * e.id].prev_out].next_out =
          arcs[2 * e.id].next_out;
      } else {
        nodes[arcs[(2 * e.id) | 1].target].first_out =
          arcs[2 * e.id].next_out;
      }

      if (nodes[n.id].first_out != -1) {
        arcs[nodes[n.id].first_out].prev_out = 2 * e.id;
      }
      arcs[(2 * e.id) | 1].target = n.id;
      arcs[2 * e.id].prev_out = -1;
      arcs[2 * e.id].next_out = nodes[n.id].first_out;
      nodes[n.id].first_out = 2 * e.id;
    }

    void changeU(Edge e, Node n) {
      if(arcs[(2 * e.id) | 1].next_out != -1) {
        arcs[arcs[(2 * e.id) | 1].next_out].prev_out =
          arcs[(2 * e.id) | 1].prev_out;
      }
      if(arcs[(2 * e.id) | 1].prev_out != -1) {
        arcs[arcs[(2 * e.id) | 1].prev_out].next_out =
          arcs[(2 * e.id) | 1].next_out;
      } else {
        nodes[arcs[2 * e.id].target].first_out =
          arcs[(2 * e.id) | 1].next_out;
      }

      if (nodes[n.id].first_out != -1) {
        arcs[nodes[n.id].first_out].prev_out = ((2 * e.id) | 1);
      }
      arcs[2 * e.id].target = n.id;
      arcs[(2 * e.id) | 1].prev_out = -1;
      arcs[(2 * e.id) | 1].next_out = nodes[n.id].first_out;
      nodes[n.id].first_out = ((2 * e.id) | 1);
    }

  };

  typedef GraphExtender<ListGraphBase> ExtendedListGraphBase;


  /// \addtogroup graphs
  /// @{

  ///A general undirected graph structure.

  ///\ref ListGraph is a versatile and fast undirected graph
  ///implementation based on linked lists that are stored in
  ///\c std::vector structures.
  ///
  ///This type fully conforms to the \ref concepts::Graph "Graph concept"
  ///and it also provides several useful additional functionalities.
  ///Most of its member functions and nested classes are documented
  ///only in the concept class.
  ///
  ///This class provides only linear time counting for nodes, edges and arcs.
  ///
  ///\sa concepts::Graph
  ///\sa ListDigraph
  class ListGraph : public ExtendedListGraphBase {
    typedef ExtendedListGraphBase Parent;

  private:
    /// Graphs are \e not copy constructible. Use GraphCopy instead.
    ListGraph(const ListGraph &) :ExtendedListGraphBase()  {};
    /// \brief Assignment of a graph to another one is \e not allowed.
    /// Use GraphCopy instead.
    void operator=(const ListGraph &) {}
  public:
    /// Constructor

    /// Constructor.
    ///
    ListGraph() {}

    typedef Parent::OutArcIt IncEdgeIt;

    /// \brief Add a new node to the graph.
    ///
    /// This function adds a new node to the graph.
    /// \return The new node.
    Node addNode() { return Parent::addNode(); }

    /// \brief Add a new edge to the graph.
    ///
    /// This function adds a new edge to the graph between nodes
    /// \c u and \c v with inherent orientation from node \c u to
    /// node \c v.
    /// \return The new edge.
    Edge addEdge(Node u, Node v) {
      return Parent::addEdge(u, v);
    }

    ///\brief Erase a node from the graph.
    ///
    /// This function erases the given node along with its incident arcs
    /// from the graph.
    ///
    /// \note All iterators referencing the removed node or the incident
    /// edges are invalidated, of course.
    void erase(Node n) { Parent::erase(n); }

    ///\brief Erase an edge from the graph.
    ///
    /// This function erases the given edge from the graph.
    ///
    /// \note All iterators referencing the removed edge are invalidated,
    /// of course.
    void erase(Edge e) { Parent::erase(e); }
    /// Node validity check

    /// This function gives back \c true if the given node is valid,
    /// i.e. it is a real node of the graph.
    ///
    /// \warning A removed node could become valid again if new nodes are
    /// added to the graph.
    bool valid(Node n) const { return Parent::valid(n); }
    /// Edge validity check

    /// This function gives back \c true if the given edge is valid,
    /// i.e. it is a real edge of the graph.
    ///
    /// \warning A removed edge could become valid again if new edges are
    /// added to the graph.
    bool valid(Edge e) const { return Parent::valid(e); }
    /// Arc validity check

    /// This function gives back \c true if the given arc is valid,
    /// i.e. it is a real arc of the graph.
    ///
    /// \warning A removed arc could become valid again if new edges are
    /// added to the graph.
    bool valid(Arc a) const { return Parent::valid(a); }

    /// \brief Change the first node of an edge.
    ///
    /// This function changes the first node of the given edge \c e to \c n.
    ///
    ///\note \c EdgeIt and \c ArcIt iterators referencing the
    ///changed edge are invalidated and all other iterators whose
    ///base node is the changed node are also invalidated.
    ///
    ///\warning This functionality cannot be used together with the
    ///Snapshot feature.
    void changeU(Edge e, Node n) {
      Parent::changeU(e,n);
    }
    /// \brief Change the second node of an edge.
    ///
    /// This function changes the second node of the given edge \c e to \c n.
    ///
    ///\note \c EdgeIt iterators referencing the changed edge remain
    ///valid, but \c ArcIt iterators referencing the changed edge and
    ///all other iterators whose base node is the changed node are also
    ///invalidated.
    ///
    ///\warning This functionality cannot be used together with the
    ///Snapshot feature.
    void changeV(Edge e, Node n) {
      Parent::changeV(e,n);
    }

    /// \brief Contract two nodes.
    ///
    /// This function contracts the given two nodes.
    /// Node \c b is removed, but instead of deleting
    /// its incident edges, they are joined to node \c a.
    /// If the last parameter \c r is \c true (this is the default value),
    /// then the newly created loops are removed.
    ///
    /// \note The moved edges are joined to node \c a using changeU()
    /// or changeV(), thus all edge and arc iterators whose base node is
    /// \c b are invalidated.
    /// Moreover all iterators referencing node \c b or the removed
    /// loops are also invalidated. Other iterators remain valid.
    ///
    ///\warning This functionality cannot be used together with the
    ///Snapshot feature.
    void contract(Node a, Node b, bool r = true) {
      for(IncEdgeIt e(*this, b); e!=INVALID;) {
        IncEdgeIt f = e; ++f;
        if (r && runningNode(e) == a) {
          erase(e);
        } else if (u(e) == b) {
          changeU(e, a);
        } else {
          changeV(e, a);
        }
        e = f;
      }
      erase(b);
    }

    ///Clear the graph.

    ///This function erases all nodes and arcs from the graph.
    ///
    ///\note All iterators of the graph are invalidated, of course.
    void clear() {
      Parent::clear();
    }

    /// Reserve memory for nodes.

    /// Using this function, it is possible to avoid superfluous memory
    /// allocation: if you know that the graph you want to build will
    /// be large (e.g. it will contain millions of nodes and/or edges),
    /// then it is worth reserving space for this amount before starting
    /// to build the graph.
    /// \sa reserveEdge()
    void reserveNode(int n) { nodes.reserve(n); };

    /// Reserve memory for edges.

    /// Using this function, it is possible to avoid superfluous memory
    /// allocation: if you know that the graph you want to build will
    /// be large (e.g. it will contain millions of nodes and/or edges),
    /// then it is worth reserving space for this amount before starting
    /// to build the graph.
    /// \sa reserveNode()
    void reserveEdge(int m) { arcs.reserve(2 * m); };

    /// \brief Class to make a snapshot of the graph and restore
    /// it later.
    ///
    /// Class to make a snapshot of the graph and restore it later.
    ///
    /// The newly added nodes and edges can be removed
    /// using the restore() function.
    ///
    /// \note After a state is restored, you cannot restore a later state,
    /// i.e. you cannot add the removed nodes and edges again using
    /// another Snapshot instance.
    ///
    /// \warning Node and edge deletions and other modifications
    /// (e.g. changing the end-nodes of edges or contracting nodes)
    /// cannot be restored. These events invalidate the snapshot.
    /// However, the edges and nodes that were added to the graph after
    /// making the current snapshot can be removed without invalidating it.
    class Snapshot {
    protected:

      typedef Parent::NodeNotifier NodeNotifier;

      class NodeObserverProxy : public NodeNotifier::ObserverBase {
      public:

        NodeObserverProxy(Snapshot& _snapshot)
          : snapshot(_snapshot) {}

        using NodeNotifier::ObserverBase::attach;
        using NodeNotifier::ObserverBase::detach;
        using NodeNotifier::ObserverBase::attached;

      protected:

        virtual void add(const Node& node) {
          snapshot.addNode(node);
        }
        virtual void add(const std::vector<Node>& nodes) {
          for (int i = nodes.size() - 1; i >= 0; ++i) {
            snapshot.addNode(nodes[i]);
          }
        }
        virtual void erase(const Node& node) {
          snapshot.eraseNode(node);
        }
        virtual void erase(const std::vector<Node>& nodes) {
          for (int i = 0; i < int(nodes.size()); ++i) {
            snapshot.eraseNode(nodes[i]);
          }
        }
        virtual void build() {
          Node node;
          std::vector<Node> nodes;
          for (notifier()->first(node); node != INVALID;
               notifier()->next(node)) {
            nodes.push_back(node);
          }
          for (int i = nodes.size() - 1; i >= 0; --i) {
            snapshot.addNode(nodes[i]);
          }
        }
        virtual void clear() {
          Node node;
          for (notifier()->first(node); node != INVALID;
               notifier()->next(node)) {
            snapshot.eraseNode(node);
          }
        }

        Snapshot& snapshot;
      };

      class EdgeObserverProxy : public EdgeNotifier::ObserverBase {
      public:

        EdgeObserverProxy(Snapshot& _snapshot)
          : snapshot(_snapshot) {}

        using EdgeNotifier::ObserverBase::attach;
        using EdgeNotifier::ObserverBase::detach;
        using EdgeNotifier::ObserverBase::attached;

      protected:

        virtual void add(const Edge& edge) {
          snapshot.addEdge(edge);
        }
        virtual void add(const std::vector<Edge>& edges) {
          for (int i = edges.size() - 1; i >= 0; ++i) {
            snapshot.addEdge(edges[i]);
          }
        }
        virtual void erase(const Edge& edge) {
          snapshot.eraseEdge(edge);
        }
        virtual void erase(const std::vector<Edge>& edges) {
          for (int i = 0; i < int(edges.size()); ++i) {
            snapshot.eraseEdge(edges[i]);
          }
        }
        virtual void build() {
          Edge edge;
          std::vector<Edge> edges;
          for (notifier()->first(edge); edge != INVALID;
               notifier()->next(edge)) {
            edges.push_back(edge);
          }
          for (int i = edges.size() - 1; i >= 0; --i) {
            snapshot.addEdge(edges[i]);
          }
        }
        virtual void clear() {
          Edge edge;
          for (notifier()->first(edge); edge != INVALID;
               notifier()->next(edge)) {
            snapshot.eraseEdge(edge);
          }
        }

        Snapshot& snapshot;
      };

      ListGraph *graph;

      NodeObserverProxy node_observer_proxy;
      EdgeObserverProxy edge_observer_proxy;

      std::list<Node> added_nodes;
      std::list<Edge> added_edges;


      void addNode(const Node& node) {
        added_nodes.push_front(node);
      }
      void eraseNode(const Node& node) {
        std::list<Node>::iterator it =
          std::find(added_nodes.begin(), added_nodes.end(), node);
        if (it == added_nodes.end()) {
          clear();
          edge_observer_proxy.detach();
          throw NodeNotifier::ImmediateDetach();
        } else {
          added_nodes.erase(it);
        }
      }

      void addEdge(const Edge& edge) {
        added_edges.push_front(edge);
      }
      void eraseEdge(const Edge& edge) {
        std::list<Edge>::iterator it =
          std::find(added_edges.begin(), added_edges.end(), edge);
        if (it == added_edges.end()) {
          clear();
          node_observer_proxy.detach();
          throw EdgeNotifier::ImmediateDetach();
        } else {
          added_edges.erase(it);
        }
      }

      void attach(ListGraph &_graph) {
        graph = &_graph;
        node_observer_proxy.attach(graph->notifier(Node()));
        edge_observer_proxy.attach(graph->notifier(Edge()));
      }

      void detach() {
        node_observer_proxy.detach();
        edge_observer_proxy.detach();
      }

      bool attached() const {
        return node_observer_proxy.attached();
      }

      void clear() {
        added_nodes.clear();
        added_edges.clear();
      }

    public:

      /// \brief Default constructor.
      ///
      /// Default constructor.
      /// You have to call save() to actually make a snapshot.
      Snapshot()
        : graph(0), node_observer_proxy(*this),
          edge_observer_proxy(*this) {}

      /// \brief Constructor that immediately makes a snapshot.
      ///
      /// This constructor immediately makes a snapshot of the given graph.
      Snapshot(ListGraph &gr)
        : node_observer_proxy(*this),
          edge_observer_proxy(*this) {
        attach(gr);
      }

      /// \brief Make a snapshot.
      ///
      /// This function makes a snapshot of the given graph.
      /// It can be called more than once. In case of a repeated
      /// call, the previous snapshot gets lost.
      void save(ListGraph &gr) {
        if (attached()) {
          detach();
          clear();
        }
        attach(gr);
      }

      /// \brief Undo the changes until the last snapshot.
      ///
      /// This function undos the changes until the last snapshot
      /// created by save() or Snapshot(ListGraph&).
      ///
      /// \warning This method invalidates the snapshot, i.e. repeated
      /// restoring is not supported unless you call save() again.
      void restore() {
        detach();
        for(std::list<Edge>::iterator it = added_edges.begin();
            it != added_edges.end(); ++it) {
          graph->erase(*it);
        }
        for(std::list<Node>::iterator it = added_nodes.begin();
            it != added_nodes.end(); ++it) {
          graph->erase(*it);
        }
        clear();
      }

      /// \brief Returns \c true if the snapshot is valid.
      ///
      /// This function returns \c true if the snapshot is valid.
      bool valid() const {
        return attached();
      }
    };
  };

  /// @}

  class ListBpGraphBase {

  protected:

    struct NodeT {
      int first_out;
      int prev, next;
      int partition_prev, partition_next;
      int partition_index;
      bool red;
    };

    struct ArcT {
      int target;
      int prev_out, next_out;
    };

    std::vector<NodeT> nodes;

    int first_node, first_red, first_blue;
    int max_red, max_blue;

    int first_free_red, first_free_blue;

    std::vector<ArcT> arcs;

    int first_free_arc;

  public:

    typedef ListBpGraphBase BpGraph;

    class Node {
      friend class ListBpGraphBase;
    protected:

      int id;
      explicit Node(int pid) { id = pid;}

    public:
      Node() {}
      Node (Invalid) { id = -1; }
      bool operator==(const Node& node) const {return id == node.id;}
      bool operator!=(const Node& node) const {return id != node.id;}
      bool operator<(const Node& node) const {return id < node.id;}
    };

    class RedNode : public Node {
      friend class ListBpGraphBase;
    protected:

      explicit RedNode(int pid) : Node(pid) {}

    public:
      RedNode() {}
      RedNode(const RedNode& node) : Node(node) {}
      RedNode(Invalid) : Node(INVALID){}
    };

    class BlueNode : public Node {
      friend class ListBpGraphBase;
    protected:

      explicit BlueNode(int pid) : Node(pid) {}

    public:
      BlueNode() {}
      BlueNode(const BlueNode& node) : Node(node) {}
      BlueNode(Invalid) : Node(INVALID){}
    };

    class Edge {
      friend class ListBpGraphBase;
    protected:

      int id;
      explicit Edge(int pid) { id = pid;}

    public:
      Edge() {}
      Edge (Invalid) { id = -1; }
      bool operator==(const Edge& edge) const {return id == edge.id;}
      bool operator!=(const Edge& edge) const {return id != edge.id;}
      bool operator<(const Edge& edge) const {return id < edge.id;}
    };

    class Arc {
      friend class ListBpGraphBase;
    protected:

      int id;
      explicit Arc(int pid) { id = pid;}

    public:
      operator Edge() const {
        return id != -1 ? edgeFromId(id / 2) : INVALID;
      }

      Arc() {}
      Arc (Invalid) { id = -1; }
      bool operator==(const Arc& arc) const {return id == arc.id;}
      bool operator!=(const Arc& arc) const {return id != arc.id;}
      bool operator<(const Arc& arc) const {return id < arc.id;}
    };

    ListBpGraphBase()
      : nodes(), first_node(-1),
        first_red(-1), first_blue(-1),
        max_red(-1), max_blue(-1),
        first_free_red(-1), first_free_blue(-1),
        arcs(), first_free_arc(-1) {}


    bool red(Node n) const { return nodes[n.id].red; }
    bool blue(Node n) const { return !nodes[n.id].red; }

    static RedNode asRedNodeUnsafe(Node n) { return RedNode(n.id); }
    static BlueNode asBlueNodeUnsafe(Node n) { return BlueNode(n.id); }

    int maxNodeId() const { return nodes.size()-1; }
    int maxRedId() const { return max_red; }
    int maxBlueId() const { return max_blue; }
    int maxEdgeId() const { return arcs.size() / 2 - 1; }
    int maxArcId() const { return arcs.size()-1; }

    Node source(Arc e) const { return Node(arcs[e.id ^ 1].target); }
    Node target(Arc e) const { return Node(arcs[e.id].target); }

    RedNode redNode(Edge e) const {
      return RedNode(arcs[2 * e.id].target);
    }
    BlueNode blueNode(Edge e) const {
      return BlueNode(arcs[2 * e.id + 1].target);
    }

    static bool direction(Arc e) {
      return (e.id & 1) == 1;
    }

    static Arc direct(Edge e, bool d) {
      return Arc(e.id * 2 + (d ? 1 : 0));
    }

    void first(Node& node) const {
      node.id = first_node;
    }

    void next(Node& node) const {
      node.id = nodes[node.id].next;
    }

    void first(RedNode& node) const {
      node.id = first_red;
    }

    void next(RedNode& node) const {
      node.id = nodes[node.id].partition_next;
    }

    void first(BlueNode& node) const {
      node.id = first_blue;
    }

    void next(BlueNode& node) const {
      node.id = nodes[node.id].partition_next;
    }

    void first(Arc& e) const {
      int n = first_node;
      while (n != -1 && nodes[n].first_out == -1) {
        n = nodes[n].next;
      }
      e.id = (n == -1) ? -1 : nodes[n].first_out;
    }

    void next(Arc& e) const {
      if (arcs[e.id].next_out != -1) {
        e.id = arcs[e.id].next_out;
      } else {
        int n = nodes[arcs[e.id ^ 1].target].next;
        while(n != -1 && nodes[n].first_out == -1) {
          n = nodes[n].next;
        }
        e.id = (n == -1) ? -1 : nodes[n].first_out;
      }
    }

    void first(Edge& e) const {
      int n = first_node;
      while (n != -1) {
        e.id = nodes[n].first_out;
        while ((e.id & 1) != 1) {
          e.id = arcs[e.id].next_out;
        }
        if (e.id != -1) {
          e.id /= 2;
          return;
        }
        n = nodes[n].next;
      }
      e.id = -1;
    }

    void next(Edge& e) const {
      int n = arcs[e.id * 2].target;
      e.id = arcs[(e.id * 2) | 1].next_out;
      while ((e.id & 1) != 1) {
        e.id = arcs[e.id].next_out;
      }
      if (e.id != -1) {
        e.id /= 2;
        return;
      }
      n = nodes[n].next;
      while (n != -1) {
        e.id = nodes[n].first_out;
        while ((e.id & 1) != 1) {
          e.id = arcs[e.id].next_out;
        }
        if (e.id != -1) {
          e.id /= 2;
          return;
        }
        n = nodes[n].next;
      }
      e.id = -1;
    }

    void firstOut(Arc &e, const Node& v) const {
      e.id = nodes[v.id].first_out;
    }
    void nextOut(Arc &e) const {
      e.id = arcs[e.id].next_out;
    }

    void firstIn(Arc &e, const Node& v) const {
      e.id = ((nodes[v.id].first_out) ^ 1);
      if (e.id == -2) e.id = -1;
    }
    void nextIn(Arc &e) const {
      e.id = ((arcs[e.id ^ 1].next_out) ^ 1);
      if (e.id == -2) e.id = -1;
    }

    void firstInc(Edge &e, bool& d, const Node& v) const {
      int a = nodes[v.id].first_out;
      if (a != -1 ) {
        e.id = a / 2;
        d = ((a & 1) == 1);
      } else {
        e.id = -1;
        d = true;
      }
    }
    void nextInc(Edge &e, bool& d) const {
      int a = (arcs[(e.id * 2) | (d ? 1 : 0)].next_out);
      if (a != -1 ) {
        e.id = a / 2;
        d = ((a & 1) == 1);
      } else {
        e.id = -1;
        d = true;
      }
    }

    static int id(Node v) { return v.id; }
    int id(RedNode v) const { return nodes[v.id].partition_index; }
    int id(BlueNode v) const { return nodes[v.id].partition_index; }
    static int id(Arc e) { return e.id; }
    static int id(Edge e) { return e.id; }

    static Node nodeFromId(int id) { return Node(id);}
    static Arc arcFromId(int id) { return Arc(id);}
    static Edge edgeFromId(int id) { return Edge(id);}

    bool valid(Node n) const {
      return n.id >= 0 && n.id < static_cast<int>(nodes.size()) &&
        nodes[n.id].prev != -2;
    }

    bool valid(Arc a) const {
      return a.id >= 0 && a.id < static_cast<int>(arcs.size()) &&
        arcs[a.id].prev_out != -2;
    }

    bool valid(Edge e) const {
      return e.id >= 0 && 2 * e.id < static_cast<int>(arcs.size()) &&
        arcs[2 * e.id].prev_out != -2;
    }

    RedNode addRedNode() {
      int n;

      if(first_free_red==-1) {
        n = nodes.size();
        nodes.push_back(NodeT());
        nodes[n].partition_index = ++max_red;
        nodes[n].red = true;
      } else {
        n = first_free_red;
        first_free_red = nodes[n].next;
      }

      nodes[n].next = first_node;
      if (first_node != -1) nodes[first_node].prev = n;
      first_node = n;
      nodes[n].prev = -1;

      nodes[n].partition_next = first_red;
      if (first_red != -1) nodes[first_red].partition_prev = n;
      first_red = n;
      nodes[n].partition_prev = -1;

      nodes[n].first_out = -1;

      return RedNode(n);
    }

    BlueNode addBlueNode() {
      int n;

      if(first_free_blue==-1) {
        n = nodes.size();
        nodes.push_back(NodeT());
        nodes[n].partition_index = ++max_blue;
        nodes[n].red = false;
      } else {
        n = first_free_blue;
        first_free_blue = nodes[n].next;
      }

      nodes[n].next = first_node;
      if (first_node != -1) nodes[first_node].prev = n;
      first_node = n;
      nodes[n].prev = -1;

      nodes[n].partition_next = first_blue;
      if (first_blue != -1) nodes[first_blue].partition_prev = n;
      first_blue = n;
      nodes[n].partition_prev = -1;

      nodes[n].first_out = -1;

      return BlueNode(n);
    }

    Edge addEdge(Node u, Node v) {
      int n;

      if (first_free_arc == -1) {
        n = arcs.size();
        arcs.push_back(ArcT());
        arcs.push_back(ArcT());
      } else {
        n = first_free_arc;
        first_free_arc = arcs[n].next_out;
      }

      arcs[n].target = u.id;
      arcs[n | 1].target = v.id;

      arcs[n].next_out = nodes[v.id].first_out;
      if (nodes[v.id].first_out != -1) {
        arcs[nodes[v.id].first_out].prev_out = n;
      }
      arcs[n].prev_out = -1;
      nodes[v.id].first_out = n;

      arcs[n | 1].next_out = nodes[u.id].first_out;
      if (nodes[u.id].first_out != -1) {
        arcs[nodes[u.id].first_out].prev_out = (n | 1);
      }
      arcs[n | 1].prev_out = -1;
      nodes[u.id].first_out = (n | 1);

      return Edge(n / 2);
    }

    void erase(const Node& node) {
      int n = node.id;

      if(nodes[n].next != -1) {
        nodes[nodes[n].next].prev = nodes[n].prev;
      }

      if(nodes[n].prev != -1) {
        nodes[nodes[n].prev].next = nodes[n].next;
      } else {
        first_node = nodes[n].next;
      }

      if (nodes[n].partition_next != -1) {
        nodes[nodes[n].partition_next].partition_prev = nodes[n].partition_prev;
      }

      if (nodes[n].partition_prev != -1) {
        nodes[nodes[n].partition_prev].partition_next = nodes[n].partition_next;
      } else {
        if (nodes[n].red) {
          first_red = nodes[n].partition_next;
        } else {
          first_blue = nodes[n].partition_next;
        }
      }

      if (nodes[n].red) {
        nodes[n].next = first_free_red;
        first_free_red = n;
      } else {
        nodes[n].next = first_free_blue;
        first_free_blue = n;
      }
      nodes[n].prev = -2;
    }

    void erase(const Edge& edge) {
      int n = edge.id * 2;

      if (arcs[n].next_out != -1) {
        arcs[arcs[n].next_out].prev_out = arcs[n].prev_out;
      }

      if (arcs[n].prev_out != -1) {
        arcs[arcs[n].prev_out].next_out = arcs[n].next_out;
      } else {
        nodes[arcs[n | 1].target].first_out = arcs[n].next_out;
      }

      if (arcs[n | 1].next_out != -1) {
        arcs[arcs[n | 1].next_out].prev_out = arcs[n | 1].prev_out;
      }

      if (arcs[n | 1].prev_out != -1) {
        arcs[arcs[n | 1].prev_out].next_out = arcs[n | 1].next_out;
      } else {
        nodes[arcs[n].target].first_out = arcs[n | 1].next_out;
      }

      arcs[n].next_out = first_free_arc;
      first_free_arc = n;
      arcs[n].prev_out = -2;
      arcs[n | 1].prev_out = -2;

    }

    void clear() {
      arcs.clear();
      nodes.clear();
      first_node = first_free_arc = first_red = first_blue =
        max_red = max_blue = first_free_red = first_free_blue = -1;
    }

  protected:

    void changeRed(Edge e, RedNode n) {
      if(arcs[(2 * e.id) | 1].next_out != -1) {
        arcs[arcs[(2 * e.id) | 1].next_out].prev_out =
          arcs[(2 * e.id) | 1].prev_out;
      }
      if(arcs[(2 * e.id) | 1].prev_out != -1) {
        arcs[arcs[(2 * e.id) | 1].prev_out].next_out =
          arcs[(2 * e.id) | 1].next_out;
      } else {
        nodes[arcs[2 * e.id].target].first_out =
          arcs[(2 * e.id) | 1].next_out;
      }

      if (nodes[n.id].first_out != -1) {
        arcs[nodes[n.id].first_out].prev_out = ((2 * e.id) | 1);
      }
      arcs[2 * e.id].target = n.id;
      arcs[(2 * e.id) | 1].prev_out = -1;
      arcs[(2 * e.id) | 1].next_out = nodes[n.id].first_out;
      nodes[n.id].first_out = ((2 * e.id) | 1);
    }

    void changeBlue(Edge e, BlueNode n) {
       if(arcs[2 * e.id].next_out != -1) {
        arcs[arcs[2 * e.id].next_out].prev_out = arcs[2 * e.id].prev_out;
      }
      if(arcs[2 * e.id].prev_out != -1) {
        arcs[arcs[2 * e.id].prev_out].next_out =
          arcs[2 * e.id].next_out;
      } else {
        nodes[arcs[(2 * e.id) | 1].target].first_out =
          arcs[2 * e.id].next_out;
      }

      if (nodes[n.id].first_out != -1) {
        arcs[nodes[n.id].first_out].prev_out = 2 * e.id;
      }
      arcs[(2 * e.id) | 1].target = n.id;
      arcs[2 * e.id].prev_out = -1;
      arcs[2 * e.id].next_out = nodes[n.id].first_out;
      nodes[n.id].first_out = 2 * e.id;
    }

  };

  typedef BpGraphExtender<ListBpGraphBase> ExtendedListBpGraphBase;


  /// \addtogroup graphs
  /// @{

  ///A general undirected graph structure.

  ///\ref ListBpGraph is a versatile and fast undirected graph
  ///implementation based on linked lists that are stored in
  ///\c std::vector structures.
  ///
  ///This type fully conforms to the \ref concepts::BpGraph "BpGraph concept"
  ///and it also provides several useful additional functionalities.
  ///Most of its member functions and nested classes are documented
  ///only in the concept class.
  ///
  ///This class provides only linear time counting for nodes, edges and arcs.
  ///
  ///\sa concepts::BpGraph
  ///\sa ListDigraph
  class ListBpGraph : public ExtendedListBpGraphBase {
    typedef ExtendedListBpGraphBase Parent;

  private:
    /// BpGraphs are \e not copy constructible. Use BpGraphCopy instead.
    ListBpGraph(const ListBpGraph &) :ExtendedListBpGraphBase()  {};
    /// \brief Assignment of a graph to another one is \e not allowed.
    /// Use BpGraphCopy instead.
    void operator=(const ListBpGraph &) {}
  public:
    /// Constructor

    /// Constructor.
    ///
    ListBpGraph() {}

    typedef Parent::OutArcIt IncEdgeIt;

    /// \brief Add a new red node to the graph.
    ///
    /// This function adds a red new node to the graph.
    /// \return The new node.
    RedNode addRedNode() { return Parent::addRedNode(); }

    /// \brief Add a new blue node to the graph.
    ///
    /// This function adds a blue new node to the graph.
    /// \return The new node.
    BlueNode addBlueNode() { return Parent::addBlueNode(); }

    /// \brief Add a new edge to the graph.
    ///
    /// This function adds a new edge to the graph between nodes
    /// \c u and \c v with inherent orientation from node \c u to
    /// node \c v.
    /// \return The new edge.
    Edge addEdge(RedNode u, BlueNode v) {
      return Parent::addEdge(u, v);
    }
    Edge addEdge(BlueNode v, RedNode u) {
      return Parent::addEdge(u, v);
    }

    ///\brief Erase a node from the graph.
    ///
    /// This function erases the given node along with its incident arcs
    /// from the graph.
    ///
    /// \note All iterators referencing the removed node or the incident
    /// edges are invalidated, of course.
    void erase(Node n) { Parent::erase(n); }

    ///\brief Erase an edge from the graph.
    ///
    /// This function erases the given edge from the graph.
    ///
    /// \note All iterators referencing the removed edge are invalidated,
    /// of course.
    void erase(Edge e) { Parent::erase(e); }
    /// Node validity check

    /// This function gives back \c true if the given node is valid,
    /// i.e. it is a real node of the graph.
    ///
    /// \warning A removed node could become valid again if new nodes are
    /// added to the graph.
    bool valid(Node n) const { return Parent::valid(n); }
    /// Edge validity check

    /// This function gives back \c true if the given edge is valid,
    /// i.e. it is a real edge of the graph.
    ///
    /// \warning A removed edge could become valid again if new edges are
    /// added to the graph.
    bool valid(Edge e) const { return Parent::valid(e); }
    /// Arc validity check

    /// This function gives back \c true if the given arc is valid,
    /// i.e. it is a real arc of the graph.
    ///
    /// \warning A removed arc could become valid again if new edges are
    /// added to the graph.
    bool valid(Arc a) const { return Parent::valid(a); }

    /// \brief Change the red node of an edge.
    ///
    /// This function changes the red node of the given edge \c e to \c n.
    ///
    ///\note \c EdgeIt and \c ArcIt iterators referencing the
    ///changed edge are invalidated and all other iterators whose
    ///base node is the changed node are also invalidated.
    ///
    ///\warning This functionality cannot be used together with the
    ///Snapshot feature.
    void changeRed(Edge e, RedNode n) {
      Parent::changeRed(e, n);
    }
    /// \brief Change the blue node of an edge.
    ///
    /// This function changes the blue node of the given edge \c e to \c n.
    ///
    ///\note \c EdgeIt iterators referencing the changed edge remain
    ///valid, but \c ArcIt iterators referencing the changed edge and
    ///all other iterators whose base node is the changed node are also
    ///invalidated.
    ///
    ///\warning This functionality cannot be used together with the
    ///Snapshot feature.
    void changeBlue(Edge e, BlueNode n) {
      Parent::changeBlue(e, n);
    }

    ///Clear the graph.

    ///This function erases all nodes and arcs from the graph.
    ///
    ///\note All iterators of the graph are invalidated, of course.
    void clear() {
      Parent::clear();
    }

    /// Reserve memory for nodes.

    /// Using this function, it is possible to avoid superfluous memory
    /// allocation: if you know that the graph you want to build will
    /// be large (e.g. it will contain millions of nodes and/or edges),
    /// then it is worth reserving space for this amount before starting
    /// to build the graph.
    /// \sa reserveEdge()
    void reserveNode(int n) { nodes.reserve(n); };

    /// Reserve memory for edges.

    /// Using this function, it is possible to avoid superfluous memory
    /// allocation: if you know that the graph you want to build will
    /// be large (e.g. it will contain millions of nodes and/or edges),
    /// then it is worth reserving space for this amount before starting
    /// to build the graph.
    /// \sa reserveNode()
    void reserveEdge(int m) { arcs.reserve(2 * m); };

    /// \brief Class to make a snapshot of the graph and restore
    /// it later.
    ///
    /// Class to make a snapshot of the graph and restore it later.
    ///
    /// The newly added nodes and edges can be removed
    /// using the restore() function.
    ///
    /// \note After a state is restored, you cannot restore a later state,
    /// i.e. you cannot add the removed nodes and edges again using
    /// another Snapshot instance.
    ///
    /// \warning Node and edge deletions and other modifications
    /// (e.g. changing the end-nodes of edges or contracting nodes)
    /// cannot be restored. These events invalidate the snapshot.
    /// However, the edges and nodes that were added to the graph after
    /// making the current snapshot can be removed without invalidating it.
    class Snapshot {
    protected:

      typedef Parent::NodeNotifier NodeNotifier;

      class NodeObserverProxy : public NodeNotifier::ObserverBase {
      public:

        NodeObserverProxy(Snapshot& _snapshot)
          : snapshot(_snapshot) {}

        using NodeNotifier::ObserverBase::attach;
        using NodeNotifier::ObserverBase::detach;
        using NodeNotifier::ObserverBase::attached;

      protected:

        virtual void add(const Node& node) {
          snapshot.addNode(node);
        }
        virtual void add(const std::vector<Node>& nodes) {
          for (int i = nodes.size() - 1; i >= 0; ++i) {
            snapshot.addNode(nodes[i]);
          }
        }
        virtual void erase(const Node& node) {
          snapshot.eraseNode(node);
        }
        virtual void erase(const std::vector<Node>& nodes) {
          for (int i = 0; i < int(nodes.size()); ++i) {
            snapshot.eraseNode(nodes[i]);
          }
        }
        virtual void build() {
          Node node;
          std::vector<Node> nodes;
          for (notifier()->first(node); node != INVALID;
               notifier()->next(node)) {
            nodes.push_back(node);
          }
          for (int i = nodes.size() - 1; i >= 0; --i) {
            snapshot.addNode(nodes[i]);
          }
        }
        virtual void clear() {
          Node node;
          for (notifier()->first(node); node != INVALID;
               notifier()->next(node)) {
            snapshot.eraseNode(node);
          }
        }

        Snapshot& snapshot;
      };

      class EdgeObserverProxy : public EdgeNotifier::ObserverBase {
      public:

        EdgeObserverProxy(Snapshot& _snapshot)
          : snapshot(_snapshot) {}

        using EdgeNotifier::ObserverBase::attach;
        using EdgeNotifier::ObserverBase::detach;
        using EdgeNotifier::ObserverBase::attached;

      protected:

        virtual void add(const Edge& edge) {
          snapshot.addEdge(edge);
        }
        virtual void add(const std::vector<Edge>& edges) {
          for (int i = edges.size() - 1; i >= 0; ++i) {
            snapshot.addEdge(edges[i]);
          }
        }
        virtual void erase(const Edge& edge) {
          snapshot.eraseEdge(edge);
        }
        virtual void erase(const std::vector<Edge>& edges) {
          for (int i = 0; i < int(edges.size()); ++i) {
            snapshot.eraseEdge(edges[i]);
          }
        }
        virtual void build() {
          Edge edge;
          std::vector<Edge> edges;
          for (notifier()->first(edge); edge != INVALID;
               notifier()->next(edge)) {
            edges.push_back(edge);
          }
          for (int i = edges.size() - 1; i >= 0; --i) {
            snapshot.addEdge(edges[i]);
          }
        }
        virtual void clear() {
          Edge edge;
          for (notifier()->first(edge); edge != INVALID;
               notifier()->next(edge)) {
            snapshot.eraseEdge(edge);
          }
        }

        Snapshot& snapshot;
      };

      ListBpGraph *graph;

      NodeObserverProxy node_observer_proxy;
      EdgeObserverProxy edge_observer_proxy;

      std::list<Node> added_nodes;
      std::list<Edge> added_edges;


      void addNode(const Node& node) {
        added_nodes.push_front(node);
      }
      void eraseNode(const Node& node) {
        std::list<Node>::iterator it =
          std::find(added_nodes.begin(), added_nodes.end(), node);
        if (it == added_nodes.end()) {
          clear();
          edge_observer_proxy.detach();
          throw NodeNotifier::ImmediateDetach();
        } else {
          added_nodes.erase(it);
        }
      }

      void addEdge(const Edge& edge) {
        added_edges.push_front(edge);
      }
      void eraseEdge(const Edge& edge) {
        std::list<Edge>::iterator it =
          std::find(added_edges.begin(), added_edges.end(), edge);
        if (it == added_edges.end()) {
          clear();
          node_observer_proxy.detach();
          throw EdgeNotifier::ImmediateDetach();
        } else {
          added_edges.erase(it);
        }
      }

      void attach(ListBpGraph &_graph) {
        graph = &_graph;
        node_observer_proxy.attach(graph->notifier(Node()));
        edge_observer_proxy.attach(graph->notifier(Edge()));
      }

      void detach() {
        node_observer_proxy.detach();
        edge_observer_proxy.detach();
      }

      bool attached() const {
        return node_observer_proxy.attached();
      }

      void clear() {
        added_nodes.clear();
        added_edges.clear();
      }

    public:

      /// \brief Default constructor.
      ///
      /// Default constructor.
      /// You have to call save() to actually make a snapshot.
      Snapshot()
        : graph(0), node_observer_proxy(*this),
          edge_observer_proxy(*this) {}

      /// \brief Constructor that immediately makes a snapshot.
      ///
      /// This constructor immediately makes a snapshot of the given graph.
      Snapshot(ListBpGraph &gr)
        : node_observer_proxy(*this),
          edge_observer_proxy(*this) {
        attach(gr);
      }

      /// \brief Make a snapshot.
      ///
      /// This function makes a snapshot of the given graph.
      /// It can be called more than once. In case of a repeated
      /// call, the previous snapshot gets lost.
      void save(ListBpGraph &gr) {
        if (attached()) {
          detach();
          clear();
        }
        attach(gr);
      }

      /// \brief Undo the changes until the last snapshot.
      ///
      /// This function undos the changes until the last snapshot
      /// created by save() or Snapshot(ListBpGraph&).
      ///
      /// \warning This method invalidates the snapshot, i.e. repeated
      /// restoring is not supported unless you call save() again.
      void restore() {
        detach();
        for(std::list<Edge>::iterator it = added_edges.begin();
            it != added_edges.end(); ++it) {
          graph->erase(*it);
        }
        for(std::list<Node>::iterator it = added_nodes.begin();
            it != added_nodes.end(); ++it) {
          graph->erase(*it);
        }
        clear();
      }

      /// \brief Returns \c true if the snapshot is valid.
      ///
      /// This function returns \c true if the snapshot is valid.
      bool valid() const {
        return attached();
      }
    };
  };

  /// @}
} //namespace lemon


#endif
