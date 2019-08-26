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

#ifndef LEMON_BFS_H
#define LEMON_BFS_H

///\ingroup search
///\file
///\brief BFS algorithm.

#include <lemon/list_graph.h>
#include <lemon/bits/path_dump.h>
#include <lemon/core.h>
#include <lemon/error.h>
#include <lemon/maps.h>
#include <lemon/path.h>

namespace lemon {

  ///Default traits class of Bfs class.

  ///Default traits class of Bfs class.
  ///\tparam GR Digraph type.
  template<class GR>
  struct BfsDefaultTraits
  {
    ///The type of the digraph the algorithm runs on.
    typedef GR Digraph;

    ///\brief The type of the map that stores the predecessor
    ///arcs of the shortest paths.
    ///
    ///The type of the map that stores the predecessor
    ///arcs of the shortest paths.
    ///It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    typedef typename Digraph::template NodeMap<typename Digraph::Arc> PredMap;
    ///Instantiates a \c PredMap.

    ///This function instantiates a \ref PredMap.
    ///\param g is the digraph, to which we would like to define the
    ///\ref PredMap.
    static PredMap *createPredMap(const Digraph &g)
    {
      return new PredMap(g);
    }

    ///The type of the map that indicates which nodes are processed.

    ///The type of the map that indicates which nodes are processed.
    ///It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    ///By default, it is a NullMap.
    typedef NullMap<typename Digraph::Node,bool> ProcessedMap;
    ///Instantiates a \c ProcessedMap.

    ///This function instantiates a \ref ProcessedMap.
    ///\param g is the digraph, to which
    ///we would like to define the \ref ProcessedMap
#ifdef DOXYGEN
    static ProcessedMap *createProcessedMap(const Digraph &g)
#else
    static ProcessedMap *createProcessedMap(const Digraph &)
#endif
    {
      return new ProcessedMap();
    }

    ///The type of the map that indicates which nodes are reached.

    ///The type of the map that indicates which nodes are reached.
    ///It must conform to
    ///the \ref concepts::ReadWriteMap "ReadWriteMap" concept.
    typedef typename Digraph::template NodeMap<bool> ReachedMap;
    ///Instantiates a \c ReachedMap.

    ///This function instantiates a \ref ReachedMap.
    ///\param g is the digraph, to which
    ///we would like to define the \ref ReachedMap.
    static ReachedMap *createReachedMap(const Digraph &g)
    {
      return new ReachedMap(g);
    }

    ///The type of the map that stores the distances of the nodes.

    ///The type of the map that stores the distances of the nodes.
    ///It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    typedef typename Digraph::template NodeMap<int> DistMap;
    ///Instantiates a \c DistMap.

    ///This function instantiates a \ref DistMap.
    ///\param g is the digraph, to which we would like to define the
    ///\ref DistMap.
    static DistMap *createDistMap(const Digraph &g)
    {
      return new DistMap(g);
    }
  };

  ///%BFS algorithm class.

  ///\ingroup search
  ///This class provides an efficient implementation of the %BFS algorithm.
  ///
  ///There is also a \ref bfs() "function-type interface" for the BFS
  ///algorithm, which is convenient in the simplier cases and it can be
  ///used easier.
  ///
  ///\tparam GR The type of the digraph the algorithm runs on.
  ///The default type is \ref ListDigraph.
  ///\tparam TR The traits class that defines various types used by the
  ///algorithm. By default, it is \ref BfsDefaultTraits
  ///"BfsDefaultTraits<GR>".
  ///In most cases, this parameter should not be set directly,
  ///consider to use the named template parameters instead.
#ifdef DOXYGEN
  template <typename GR,
            typename TR>
#else
  template <typename GR=ListDigraph,
            typename TR=BfsDefaultTraits<GR> >
#endif
  class Bfs {
  public:

    ///The type of the digraph the algorithm runs on.
    typedef typename TR::Digraph Digraph;

    ///\brief The type of the map that stores the predecessor arcs of the
    ///shortest paths.
    typedef typename TR::PredMap PredMap;
    ///The type of the map that stores the distances of the nodes.
    typedef typename TR::DistMap DistMap;
    ///The type of the map that indicates which nodes are reached.
    typedef typename TR::ReachedMap ReachedMap;
    ///The type of the map that indicates which nodes are processed.
    typedef typename TR::ProcessedMap ProcessedMap;
    ///The type of the paths.
    typedef PredMapPath<Digraph, PredMap> Path;

    ///The \ref lemon::BfsDefaultTraits "traits class" of the algorithm.
    typedef TR Traits;

  private:

    typedef typename Digraph::Node Node;
    typedef typename Digraph::NodeIt NodeIt;
    typedef typename Digraph::Arc Arc;
    typedef typename Digraph::OutArcIt OutArcIt;

    //Pointer to the underlying digraph.
    const Digraph *G;
    //Pointer to the map of predecessor arcs.
    PredMap *_pred;
    //Indicates if _pred is locally allocated (true) or not.
    bool local_pred;
    //Pointer to the map of distances.
    DistMap *_dist;
    //Indicates if _dist is locally allocated (true) or not.
    bool local_dist;
    //Pointer to the map of reached status of the nodes.
    ReachedMap *_reached;
    //Indicates if _reached is locally allocated (true) or not.
    bool local_reached;
    //Pointer to the map of processed status of the nodes.
    ProcessedMap *_processed;
    //Indicates if _processed is locally allocated (true) or not.
    bool local_processed;

    std::vector<typename Digraph::Node> _queue;
    int _queue_head,_queue_tail,_queue_next_dist;
    int _curr_dist;

    //Creates the maps if necessary.
    void create_maps()
    {
      if(!_pred) {
        local_pred = true;
        _pred = Traits::createPredMap(*G);
      }
      if(!_dist) {
        local_dist = true;
        _dist = Traits::createDistMap(*G);
      }
      if(!_reached) {
        local_reached = true;
        _reached = Traits::createReachedMap(*G);
      }
      if(!_processed) {
        local_processed = true;
        _processed = Traits::createProcessedMap(*G);
      }
    }

  protected:

    Bfs() {}

  public:

    typedef Bfs Create;

    ///\name Named Template Parameters

    ///@{

    template <class T>
    struct SetPredMapTraits : public Traits {
      typedef T PredMap;
      static PredMap *createPredMap(const Digraph &)
      {
        LEMON_ASSERT(false, "PredMap is not initialized");
        return 0; // ignore warnings
      }
    };
    ///\brief \ref named-templ-param "Named parameter" for setting
    ///\c PredMap type.
    ///
    ///\ref named-templ-param "Named parameter" for setting
    ///\c PredMap type.
    ///It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    template <class T>
    struct SetPredMap : public Bfs< Digraph, SetPredMapTraits<T> > {
      typedef Bfs< Digraph, SetPredMapTraits<T> > Create;
    };

    template <class T>
    struct SetDistMapTraits : public Traits {
      typedef T DistMap;
      static DistMap *createDistMap(const Digraph &)
      {
        LEMON_ASSERT(false, "DistMap is not initialized");
        return 0; // ignore warnings
      }
    };
    ///\brief \ref named-templ-param "Named parameter" for setting
    ///\c DistMap type.
    ///
    ///\ref named-templ-param "Named parameter" for setting
    ///\c DistMap type.
    ///It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    template <class T>
    struct SetDistMap : public Bfs< Digraph, SetDistMapTraits<T> > {
      typedef Bfs< Digraph, SetDistMapTraits<T> > Create;
    };

    template <class T>
    struct SetReachedMapTraits : public Traits {
      typedef T ReachedMap;
      static ReachedMap *createReachedMap(const Digraph &)
      {
        LEMON_ASSERT(false, "ReachedMap is not initialized");
        return 0; // ignore warnings
      }
    };
    ///\brief \ref named-templ-param "Named parameter" for setting
    ///\c ReachedMap type.
    ///
    ///\ref named-templ-param "Named parameter" for setting
    ///\c ReachedMap type.
    ///It must conform to
    ///the \ref concepts::ReadWriteMap "ReadWriteMap" concept.
    template <class T>
    struct SetReachedMap : public Bfs< Digraph, SetReachedMapTraits<T> > {
      typedef Bfs< Digraph, SetReachedMapTraits<T> > Create;
    };

    template <class T>
    struct SetProcessedMapTraits : public Traits {
      typedef T ProcessedMap;
      static ProcessedMap *createProcessedMap(const Digraph &)
      {
        LEMON_ASSERT(false, "ProcessedMap is not initialized");
        return 0; // ignore warnings
      }
    };
    ///\brief \ref named-templ-param "Named parameter" for setting
    ///\c ProcessedMap type.
    ///
    ///\ref named-templ-param "Named parameter" for setting
    ///\c ProcessedMap type.
    ///It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    template <class T>
    struct SetProcessedMap : public Bfs< Digraph, SetProcessedMapTraits<T> > {
      typedef Bfs< Digraph, SetProcessedMapTraits<T> > Create;
    };

    struct SetStandardProcessedMapTraits : public Traits {
      typedef typename Digraph::template NodeMap<bool> ProcessedMap;
      static ProcessedMap *createProcessedMap(const Digraph &g)
      {
        return new ProcessedMap(g);
        return 0; // ignore warnings
      }
    };
    ///\brief \ref named-templ-param "Named parameter" for setting
    ///\c ProcessedMap type to be <tt>Digraph::NodeMap<bool></tt>.
    ///
    ///\ref named-templ-param "Named parameter" for setting
    ///\c ProcessedMap type to be <tt>Digraph::NodeMap<bool></tt>.
    ///If you don't set it explicitly, it will be automatically allocated.
    struct SetStandardProcessedMap :
      public Bfs< Digraph, SetStandardProcessedMapTraits > {
      typedef Bfs< Digraph, SetStandardProcessedMapTraits > Create;
    };

    ///@}

  public:

    ///Constructor.

    ///Constructor.
    ///\param g The digraph the algorithm runs on.
    Bfs(const Digraph &g) :
      G(&g),
      _pred(NULL), local_pred(false),
      _dist(NULL), local_dist(false),
      _reached(NULL), local_reached(false),
      _processed(NULL), local_processed(false)
    { }

    ///Destructor.
    ~Bfs()
    {
      if(local_pred) delete _pred;
      if(local_dist) delete _dist;
      if(local_reached) delete _reached;
      if(local_processed) delete _processed;
    }

    ///Sets the map that stores the predecessor arcs.

    ///Sets the map that stores the predecessor arcs.
    ///If you don't use this function before calling \ref run(Node) "run()"
    ///or \ref init(), an instance will be allocated automatically.
    ///The destructor deallocates this automatically allocated map,
    ///of course.
    ///\return <tt> (*this) </tt>
    Bfs &predMap(PredMap &m)
    {
      if(local_pred) {
        delete _pred;
        local_pred=false;
      }
      _pred = &m;
      return *this;
    }

    ///Sets the map that indicates which nodes are reached.

    ///Sets the map that indicates which nodes are reached.
    ///If you don't use this function before calling \ref run(Node) "run()"
    ///or \ref init(), an instance will be allocated automatically.
    ///The destructor deallocates this automatically allocated map,
    ///of course.
    ///\return <tt> (*this) </tt>
    Bfs &reachedMap(ReachedMap &m)
    {
      if(local_reached) {
        delete _reached;
        local_reached=false;
      }
      _reached = &m;
      return *this;
    }

    ///Sets the map that indicates which nodes are processed.

    ///Sets the map that indicates which nodes are processed.
    ///If you don't use this function before calling \ref run(Node) "run()"
    ///or \ref init(), an instance will be allocated automatically.
    ///The destructor deallocates this automatically allocated map,
    ///of course.
    ///\return <tt> (*this) </tt>
    Bfs &processedMap(ProcessedMap &m)
    {
      if(local_processed) {
        delete _processed;
        local_processed=false;
      }
      _processed = &m;
      return *this;
    }

    ///Sets the map that stores the distances of the nodes.

    ///Sets the map that stores the distances of the nodes calculated by
    ///the algorithm.
    ///If you don't use this function before calling \ref run(Node) "run()"
    ///or \ref init(), an instance will be allocated automatically.
    ///The destructor deallocates this automatically allocated map,
    ///of course.
    ///\return <tt> (*this) </tt>
    Bfs &distMap(DistMap &m)
    {
      if(local_dist) {
        delete _dist;
        local_dist=false;
      }
      _dist = &m;
      return *this;
    }

  public:

    ///\name Execution Control
    ///The simplest way to execute the BFS algorithm is to use one of the
    ///member functions called \ref run(Node) "run()".\n
    ///If you need better control on the execution, you have to call
    ///\ref init() first, then you can add several source nodes with
    ///\ref addSource(). Finally the actual path computation can be
    ///performed with one of the \ref start() functions.

    ///@{

    ///\brief Initializes the internal data structures.
    ///
    ///Initializes the internal data structures.
    void init()
    {
      create_maps();
      _queue.resize(countNodes(*G));
      _queue_head=_queue_tail=0;
      _curr_dist=1;
      for ( NodeIt u(*G) ; u!=INVALID ; ++u ) {
        _pred->set(u,INVALID);
        _reached->set(u,false);
        _processed->set(u,false);
      }
    }

    ///Adds a new source node.

    ///Adds a new source node to the set of nodes to be processed.
    ///
    void addSource(Node s)
    {
      if(!(*_reached)[s])
        {
          _reached->set(s,true);
          _pred->set(s,INVALID);
          _dist->set(s,0);
          _queue[_queue_head++]=s;
          _queue_next_dist=_queue_head;
        }
    }

    ///Processes the next node.

    ///Processes the next node.
    ///
    ///\return The processed node.
    ///
    ///\pre The queue must not be empty.
    Node processNextNode()
    {
      if(_queue_tail==_queue_next_dist) {
        _curr_dist++;
        _queue_next_dist=_queue_head;
      }
      Node n=_queue[_queue_tail++];
      _processed->set(n,true);
      Node m;
      for(OutArcIt e(*G,n);e!=INVALID;++e)
        if(!(*_reached)[m=G->target(e)]) {
          _queue[_queue_head++]=m;
          _reached->set(m,true);
          _pred->set(m,e);
          _dist->set(m,_curr_dist);
        }
      return n;
    }

    ///Processes the next node.

    ///Processes the next node and checks if the given target node
    ///is reached. If the target node is reachable from the processed
    ///node, then the \c reach parameter will be set to \c true.
    ///
    ///\param target The target node.
    ///\retval reach Indicates if the target node is reached.
    ///It should be initially \c false.
    ///
    ///\return The processed node.
    ///
    ///\pre The queue must not be empty.
    Node processNextNode(Node target, bool& reach)
    {
      if(_queue_tail==_queue_next_dist) {
        _curr_dist++;
        _queue_next_dist=_queue_head;
      }
      Node n=_queue[_queue_tail++];
      _processed->set(n,true);
      Node m;
      for(OutArcIt e(*G,n);e!=INVALID;++e)
        if(!(*_reached)[m=G->target(e)]) {
          _queue[_queue_head++]=m;
          _reached->set(m,true);
          _pred->set(m,e);
          _dist->set(m,_curr_dist);
          reach = reach || (target == m);
        }
      return n;
    }

    ///Processes the next node.

    ///Processes the next node and checks if at least one of reached
    ///nodes has \c true value in the \c nm node map. If one node
    ///with \c true value is reachable from the processed node, then the
    ///\c rnode parameter will be set to the first of such nodes.
    ///
    ///\param nm A \c bool (or convertible) node map that indicates the
    ///possible targets.
    ///\retval rnode The reached target node.
    ///It should be initially \c INVALID.
    ///
    ///\return The processed node.
    ///
    ///\pre The queue must not be empty.
    template<class NM>
    Node processNextNode(const NM& nm, Node& rnode)
    {
      if(_queue_tail==_queue_next_dist) {
        _curr_dist++;
        _queue_next_dist=_queue_head;
      }
      Node n=_queue[_queue_tail++];
      _processed->set(n,true);
      Node m;
      for(OutArcIt e(*G,n);e!=INVALID;++e)
        if(!(*_reached)[m=G->target(e)]) {
          _queue[_queue_head++]=m;
          _reached->set(m,true);
          _pred->set(m,e);
          _dist->set(m,_curr_dist);
          if (nm[m] && rnode == INVALID) rnode = m;
        }
      return n;
    }

    ///The next node to be processed.

    ///Returns the next node to be processed or \c INVALID if the queue
    ///is empty.
    Node nextNode() const
    {
      return _queue_tail<_queue_head?_queue[_queue_tail]:INVALID;
    }

    ///Returns \c false if there are nodes to be processed.

    ///Returns \c false if there are nodes to be processed
    ///in the queue.
    bool emptyQueue() const { return _queue_tail==_queue_head; }

    ///Returns the number of the nodes to be processed.

    ///Returns the number of the nodes to be processed
    ///in the queue.
    int queueSize() const { return _queue_head-_queue_tail; }

    ///Executes the algorithm.

    ///Executes the algorithm.
    ///
    ///This method runs the %BFS algorithm from the root node(s)
    ///in order to compute the shortest path to each node.
    ///
    ///The algorithm computes
    ///- the shortest path tree (forest),
    ///- the distance of each node from the root(s).
    ///
    ///\pre init() must be called and at least one root node should be
    ///added with addSource() before using this function.
    ///
    ///\note <tt>b.start()</tt> is just a shortcut of the following code.
    ///\code
    ///  while ( !b.emptyQueue() ) {
    ///    b.processNextNode();
    ///  }
    ///\endcode
    void start()
    {
      while ( !emptyQueue() ) processNextNode();
    }

    ///Executes the algorithm until the given target node is reached.

    ///Executes the algorithm until the given target node is reached.
    ///
    ///This method runs the %BFS algorithm from the root node(s)
    ///in order to compute the shortest path to \c t.
    ///
    ///The algorithm computes
    ///- the shortest path to \c t,
    ///- the distance of \c t from the root(s).
    ///
    ///\pre init() must be called and at least one root node should be
    ///added with addSource() before using this function.
    ///
    ///\note <tt>b.start(t)</tt> is just a shortcut of the following code.
    ///\code
    ///  bool reach = false;
    ///  while ( !b.emptyQueue() && !reach ) {
    ///    b.processNextNode(t, reach);
    ///  }
    ///\endcode
    void start(Node t)
    {
      bool reach = false;
      while ( !emptyQueue() && !reach ) processNextNode(t, reach);
    }

    ///Executes the algorithm until a condition is met.

    ///Executes the algorithm until a condition is met.
    ///
    ///This method runs the %BFS algorithm from the root node(s) in
    ///order to compute the shortest path to a node \c v with
    /// <tt>nm[v]</tt> true, if such a node can be found.
    ///
    ///\param nm A \c bool (or convertible) node map. The algorithm
    ///will stop when it reaches a node \c v with <tt>nm[v]</tt> true.
    ///
    ///\return The reached node \c v with <tt>nm[v]</tt> true or
    ///\c INVALID if no such node was found.
    ///
    ///\pre init() must be called and at least one root node should be
    ///added with addSource() before using this function.
    ///
    ///\note <tt>b.start(nm)</tt> is just a shortcut of the following code.
    ///\code
    ///  Node rnode = INVALID;
    ///  while ( !b.emptyQueue() && rnode == INVALID ) {
    ///    b.processNextNode(nm, rnode);
    ///  }
    ///  return rnode;
    ///\endcode
    template<class NodeBoolMap>
    Node start(const NodeBoolMap &nm)
    {
      Node rnode = INVALID;
      while ( !emptyQueue() && rnode == INVALID ) {
        processNextNode(nm, rnode);
      }
      return rnode;
    }

    ///Runs the algorithm from the given source node.

    ///This method runs the %BFS algorithm from node \c s
    ///in order to compute the shortest path to each node.
    ///
    ///The algorithm computes
    ///- the shortest path tree,
    ///- the distance of each node from the root.
    ///
    ///\note <tt>b.run(s)</tt> is just a shortcut of the following code.
    ///\code
    ///  b.init();
    ///  b.addSource(s);
    ///  b.start();
    ///\endcode
    void run(Node s) {
      init();
      addSource(s);
      start();
    }

    ///Finds the shortest path between \c s and \c t.

    ///This method runs the %BFS algorithm from node \c s
    ///in order to compute the shortest path to node \c t
    ///(it stops searching when \c t is processed).
    ///
    ///\return \c true if \c t is reachable form \c s.
    ///
    ///\note Apart from the return value, <tt>b.run(s,t)</tt> is just a
    ///shortcut of the following code.
    ///\code
    ///  b.init();
    ///  b.addSource(s);
    ///  b.start(t);
    ///\endcode
    bool run(Node s,Node t) {
      init();
      addSource(s);
      start(t);
      return reached(t);
    }

    ///Runs the algorithm to visit all nodes in the digraph.

    ///This method runs the %BFS algorithm in order to visit all nodes
    ///in the digraph.
    ///
    ///\note <tt>b.run(s)</tt> is just a shortcut of the following code.
    ///\code
    ///  b.init();
    ///  for (NodeIt n(gr); n != INVALID; ++n) {
    ///    if (!b.reached(n)) {
    ///      b.addSource(n);
    ///      b.start();
    ///    }
    ///  }
    ///\endcode
    void run() {
      init();
      for (NodeIt n(*G); n != INVALID; ++n) {
        if (!reached(n)) {
          addSource(n);
          start();
        }
      }
    }

    ///@}

    ///\name Query Functions
    ///The results of the BFS algorithm can be obtained using these
    ///functions.\n
    ///Either \ref run(Node) "run()" or \ref start() should be called
    ///before using them.

    ///@{

    ///The shortest path to the given node.

    ///Returns the shortest path to the given node from the root(s).
    ///
    ///\warning \c t should be reached from the root(s).
    ///
    ///\pre Either \ref run(Node) "run()" or \ref init()
    ///must be called before using this function.
    Path path(Node t) const { return Path(*G, *_pred, t); }

    ///The distance of the given node from the root(s).

    ///Returns the distance of the given node from the root(s).
    ///
    ///\warning If node \c v is not reached from the root(s), then
    ///the return value of this function is undefined.
    ///
    ///\pre Either \ref run(Node) "run()" or \ref init()
    ///must be called before using this function.
    int dist(Node v) const { return (*_dist)[v]; }

    ///\brief Returns the 'previous arc' of the shortest path tree for
    ///the given node.
    ///
    ///This function returns the 'previous arc' of the shortest path
    ///tree for the node \c v, i.e. it returns the last arc of a
    ///shortest path from a root to \c v. It is \c INVALID if \c v
    ///is not reached from the root(s) or if \c v is a root.
    ///
    ///The shortest path tree used here is equal to the shortest path
    ///tree used in \ref predNode() and \ref predMap().
    ///
    ///\pre Either \ref run(Node) "run()" or \ref init()
    ///must be called before using this function.
    Arc predArc(Node v) const { return (*_pred)[v];}

    ///\brief Returns the 'previous node' of the shortest path tree for
    ///the given node.
    ///
    ///This function returns the 'previous node' of the shortest path
    ///tree for the node \c v, i.e. it returns the last but one node
    ///of a shortest path from a root to \c v. It is \c INVALID
    ///if \c v is not reached from the root(s) or if \c v is a root.
    ///
    ///The shortest path tree used here is equal to the shortest path
    ///tree used in \ref predArc() and \ref predMap().
    ///
    ///\pre Either \ref run(Node) "run()" or \ref init()
    ///must be called before using this function.
    Node predNode(Node v) const { return (*_pred)[v]==INVALID ? INVALID:
                                  G->source((*_pred)[v]); }

    ///\brief Returns a const reference to the node map that stores the
    /// distances of the nodes.
    ///
    ///Returns a const reference to the node map that stores the distances
    ///of the nodes calculated by the algorithm.
    ///
    ///\pre Either \ref run(Node) "run()" or \ref init()
    ///must be called before using this function.
    const DistMap &distMap() const { return *_dist;}

    ///\brief Returns a const reference to the node map that stores the
    ///predecessor arcs.
    ///
    ///Returns a const reference to the node map that stores the predecessor
    ///arcs, which form the shortest path tree (forest).
    ///
    ///\pre Either \ref run(Node) "run()" or \ref init()
    ///must be called before using this function.
    const PredMap &predMap() const { return *_pred;}

    ///Checks if the given node is reached from the root(s).

    ///Returns \c true if \c v is reached from the root(s).
    ///
    ///\pre Either \ref run(Node) "run()" or \ref init()
    ///must be called before using this function.
    bool reached(Node v) const { return (*_reached)[v]; }

    ///@}
  };

  ///Default traits class of bfs() function.

  ///Default traits class of bfs() function.
  ///\tparam GR Digraph type.
  template<class GR>
  struct BfsWizardDefaultTraits
  {
    ///The type of the digraph the algorithm runs on.
    typedef GR Digraph;

    ///\brief The type of the map that stores the predecessor
    ///arcs of the shortest paths.
    ///
    ///The type of the map that stores the predecessor
    ///arcs of the shortest paths.
    ///It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    typedef typename Digraph::template NodeMap<typename Digraph::Arc> PredMap;
    ///Instantiates a PredMap.

    ///This function instantiates a PredMap.
    ///\param g is the digraph, to which we would like to define the
    ///PredMap.
    static PredMap *createPredMap(const Digraph &g)
    {
      return new PredMap(g);
    }

    ///The type of the map that indicates which nodes are processed.

    ///The type of the map that indicates which nodes are processed.
    ///It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    ///By default, it is a NullMap.
    typedef NullMap<typename Digraph::Node,bool> ProcessedMap;
    ///Instantiates a ProcessedMap.

    ///This function instantiates a ProcessedMap.
    ///\param g is the digraph, to which
    ///we would like to define the ProcessedMap.
#ifdef DOXYGEN
    static ProcessedMap *createProcessedMap(const Digraph &g)
#else
    static ProcessedMap *createProcessedMap(const Digraph &)
#endif
    {
      return new ProcessedMap();
    }

    ///The type of the map that indicates which nodes are reached.

    ///The type of the map that indicates which nodes are reached.
    ///It must conform to
    ///the \ref concepts::ReadWriteMap "ReadWriteMap" concept.
    typedef typename Digraph::template NodeMap<bool> ReachedMap;
    ///Instantiates a ReachedMap.

    ///This function instantiates a ReachedMap.
    ///\param g is the digraph, to which
    ///we would like to define the ReachedMap.
    static ReachedMap *createReachedMap(const Digraph &g)
    {
      return new ReachedMap(g);
    }

    ///The type of the map that stores the distances of the nodes.

    ///The type of the map that stores the distances of the nodes.
    ///It must conform to the \ref concepts::WriteMap "WriteMap" concept.
    typedef typename Digraph::template NodeMap<int> DistMap;
    ///Instantiates a DistMap.

    ///This function instantiates a DistMap.
    ///\param g is the digraph, to which we would like to define
    ///the DistMap
    static DistMap *createDistMap(const Digraph &g)
    {
      return new DistMap(g);
    }

    ///The type of the shortest paths.

    ///The type of the shortest paths.
    ///It must conform to the \ref concepts::Path "Path" concept.
    typedef lemon::Path<Digraph> Path;
  };

  /// Default traits class used by BfsWizard

  /// Default traits class used by BfsWizard.
  /// \tparam GR The type of the digraph.
  template<class GR>
  class BfsWizardBase : public BfsWizardDefaultTraits<GR>
  {

    typedef BfsWizardDefaultTraits<GR> Base;
  protected:
    //The type of the nodes in the digraph.
    typedef typename Base::Digraph::Node Node;

    //Pointer to the digraph the algorithm runs on.
    void *_g;
    //Pointer to the map of reached nodes.
    void *_reached;
    //Pointer to the map of processed nodes.
    void *_processed;
    //Pointer to the map of predecessors arcs.
    void *_pred;
    //Pointer to the map of distances.
    void *_dist;
    //Pointer to the shortest path to the target node.
    void *_path;
    //Pointer to the distance of the target node.
    int *_di;

    public:
    /// Constructor.

    /// This constructor does not require parameters, it initiates
    /// all of the attributes to \c 0.
    BfsWizardBase() : _g(0), _reached(0), _processed(0), _pred(0),
                      _dist(0), _path(0), _di(0) {}

    /// Constructor.

    /// This constructor requires one parameter,
    /// others are initiated to \c 0.
    /// \param g The digraph the algorithm runs on.
    BfsWizardBase(const GR &g) :
      _g(reinterpret_cast<void*>(const_cast<GR*>(&g))),
      _reached(0), _processed(0), _pred(0), _dist(0),  _path(0), _di(0) {}

  };

  /// Auxiliary class for the function-type interface of BFS algorithm.

  /// This auxiliary class is created to implement the
  /// \ref bfs() "function-type interface" of \ref Bfs algorithm.
  /// It does not have own \ref run(Node) "run()" method, it uses the
  /// functions and features of the plain \ref Bfs.
  ///
  /// This class should only be used through the \ref bfs() function,
  /// which makes it easier to use the algorithm.
  ///
  /// \tparam TR The traits class that defines various types used by the
  /// algorithm.
  template<class TR>
  class BfsWizard : public TR
  {
    typedef TR Base;

    typedef typename TR::Digraph Digraph;

    typedef typename Digraph::Node Node;
    typedef typename Digraph::NodeIt NodeIt;
    typedef typename Digraph::Arc Arc;
    typedef typename Digraph::OutArcIt OutArcIt;

    typedef typename TR::PredMap PredMap;
    typedef typename TR::DistMap DistMap;
    typedef typename TR::ReachedMap ReachedMap;
    typedef typename TR::ProcessedMap ProcessedMap;
    typedef typename TR::Path Path;

  public:

    /// Constructor.
    BfsWizard() : TR() {}

    /// Constructor that requires parameters.

    /// Constructor that requires parameters.
    /// These parameters will be the default values for the traits class.
    /// \param g The digraph the algorithm runs on.
    BfsWizard(const Digraph &g) :
      TR(g) {}

    ///Copy constructor
    BfsWizard(const TR &b) : TR(b) {}

    ~BfsWizard() {}

    ///Runs BFS algorithm from the given source node.

    ///This method runs BFS algorithm from node \c s
    ///in order to compute the shortest path to each node.
    void run(Node s)
    {
      Bfs<Digraph,TR> alg(*reinterpret_cast<const Digraph*>(Base::_g));
      if (Base::_pred)
        alg.predMap(*reinterpret_cast<PredMap*>(Base::_pred));
      if (Base::_dist)
        alg.distMap(*reinterpret_cast<DistMap*>(Base::_dist));
      if (Base::_reached)
        alg.reachedMap(*reinterpret_cast<ReachedMap*>(Base::_reached));
      if (Base::_processed)
        alg.processedMap(*reinterpret_cast<ProcessedMap*>(Base::_processed));
      if (s!=INVALID)
        alg.run(s);
      else
        alg.run();
    }

    ///Finds the shortest path between \c s and \c t.

    ///This method runs BFS algorithm from node \c s
    ///in order to compute the shortest path to node \c t
    ///(it stops searching when \c t is processed).
    ///
    ///\return \c true if \c t is reachable form \c s.
    bool run(Node s, Node t)
    {
      Bfs<Digraph,TR> alg(*reinterpret_cast<const Digraph*>(Base::_g));
      if (Base::_pred)
        alg.predMap(*reinterpret_cast<PredMap*>(Base::_pred));
      if (Base::_dist)
        alg.distMap(*reinterpret_cast<DistMap*>(Base::_dist));
      if (Base::_reached)
        alg.reachedMap(*reinterpret_cast<ReachedMap*>(Base::_reached));
      if (Base::_processed)
        alg.processedMap(*reinterpret_cast<ProcessedMap*>(Base::_processed));
      alg.run(s,t);
      if (Base::_path)
        *reinterpret_cast<Path*>(Base::_path) = alg.path(t);
      if (Base::_di)
        *Base::_di = alg.dist(t);
      return alg.reached(t);
    }

    ///Runs BFS algorithm to visit all nodes in the digraph.

    ///This method runs BFS algorithm in order to visit all nodes
    ///in the digraph.
    void run()
    {
      run(INVALID);
    }

    template<class T>
    struct SetPredMapBase : public Base {
      typedef T PredMap;
      static PredMap *createPredMap(const Digraph &) { return 0; };
      SetPredMapBase(const TR &b) : TR(b) {}
    };

    ///\brief \ref named-templ-param "Named parameter" for setting
    ///the predecessor map.
    ///
    ///\ref named-templ-param "Named parameter" function for setting
    ///the map that stores the predecessor arcs of the nodes.
    template<class T>
    BfsWizard<SetPredMapBase<T> > predMap(const T &t)
    {
      Base::_pred=reinterpret_cast<void*>(const_cast<T*>(&t));
      return BfsWizard<SetPredMapBase<T> >(*this);
    }

    template<class T>
    struct SetReachedMapBase : public Base {
      typedef T ReachedMap;
      static ReachedMap *createReachedMap(const Digraph &) { return 0; };
      SetReachedMapBase(const TR &b) : TR(b) {}
    };

    ///\brief \ref named-templ-param "Named parameter" for setting
    ///the reached map.
    ///
    ///\ref named-templ-param "Named parameter" function for setting
    ///the map that indicates which nodes are reached.
    template<class T>
    BfsWizard<SetReachedMapBase<T> > reachedMap(const T &t)
    {
      Base::_reached=reinterpret_cast<void*>(const_cast<T*>(&t));
      return BfsWizard<SetReachedMapBase<T> >(*this);
    }

    template<class T>
    struct SetDistMapBase : public Base {
      typedef T DistMap;
      static DistMap *createDistMap(const Digraph &) { return 0; };
      SetDistMapBase(const TR &b) : TR(b) {}
    };

    ///\brief \ref named-templ-param "Named parameter" for setting
    ///the distance map.
    ///
    ///\ref named-templ-param "Named parameter" function for setting
    ///the map that stores the distances of the nodes calculated
    ///by the algorithm.
    template<class T>
    BfsWizard<SetDistMapBase<T> > distMap(const T &t)
    {
      Base::_dist=reinterpret_cast<void*>(const_cast<T*>(&t));
      return BfsWizard<SetDistMapBase<T> >(*this);
    }

    template<class T>
    struct SetProcessedMapBase : public Base {
      typedef T ProcessedMap;
      static ProcessedMap *createProcessedMap(const Digraph &) { return 0; };
      SetProcessedMapBase(const TR &b) : TR(b) {}
    };

    ///\brief \ref named-func-param "Named parameter" for setting
    ///the processed map.
    ///
    ///\ref named-templ-param "Named parameter" function for setting
    ///the map that indicates which nodes are processed.
    template<class T>
    BfsWizard<SetProcessedMapBase<T> > processedMap(const T &t)
    {
      Base::_processed=reinterpret_cast<void*>(const_cast<T*>(&t));
      return BfsWizard<SetProcessedMapBase<T> >(*this);
    }

    template<class T>
    struct SetPathBase : public Base {
      typedef T Path;
      SetPathBase(const TR &b) : TR(b) {}
    };
    ///\brief \ref named-func-param "Named parameter"
    ///for getting the shortest path to the target node.
    ///
    ///\ref named-func-param "Named parameter"
    ///for getting the shortest path to the target node.
    template<class T>
    BfsWizard<SetPathBase<T> > path(const T &t)
    {
      Base::_path=reinterpret_cast<void*>(const_cast<T*>(&t));
      return BfsWizard<SetPathBase<T> >(*this);
    }

    ///\brief \ref named-func-param "Named parameter"
    ///for getting the distance of the target node.
    ///
    ///\ref named-func-param "Named parameter"
    ///for getting the distance of the target node.
    BfsWizard dist(const int &d)
    {
      Base::_di=const_cast<int*>(&d);
      return *this;
    }

  };

  ///Function-type interface for BFS algorithm.

  /// \ingroup search
  ///Function-type interface for BFS algorithm.
  ///
  ///This function also has several \ref named-func-param "named parameters",
  ///they are declared as the members of class \ref BfsWizard.
  ///The following examples show how to use these parameters.
  ///\code
  ///  // Compute shortest path from node s to each node
  ///  bfs(g).predMap(preds).distMap(dists).run(s);
  ///
  ///  // Compute shortest path from s to t
  ///  bool reached = bfs(g).path(p).dist(d).run(s,t);
  ///\endcode
  ///\warning Don't forget to put the \ref BfsWizard::run(Node) "run()"
  ///to the end of the parameter list.
  ///\sa BfsWizard
  ///\sa Bfs
  template<class GR>
  BfsWizard<BfsWizardBase<GR> >
  bfs(const GR &digraph)
  {
    return BfsWizard<BfsWizardBase<GR> >(digraph);
  }

#ifdef DOXYGEN
  /// \brief Visitor class for BFS.
  ///
  /// This class defines the interface of the BfsVisit events, and
  /// it could be the base of a real visitor class.
  template <typename GR>
  struct BfsVisitor {
    typedef GR Digraph;
    typedef typename Digraph::Arc Arc;
    typedef typename Digraph::Node Node;
    /// \brief Called for the source node(s) of the BFS.
    ///
    /// This function is called for the source node(s) of the BFS.
    void start(const Node& node) {}
    /// \brief Called when a node is reached first time.
    ///
    /// This function is called when a node is reached first time.
    void reach(const Node& node) {}
    /// \brief Called when a node is processed.
    ///
    /// This function is called when a node is processed.
    void process(const Node& node) {}
    /// \brief Called when an arc reaches a new node.
    ///
    /// This function is called when the BFS finds an arc whose target node
    /// is not reached yet.
    void discover(const Arc& arc) {}
    /// \brief Called when an arc is examined but its target node is
    /// already discovered.
    ///
    /// This function is called when an arc is examined but its target node is
    /// already discovered.
    void examine(const Arc& arc) {}
  };
#else
  template <typename GR>
  struct BfsVisitor {
    typedef GR Digraph;
    typedef typename Digraph::Arc Arc;
    typedef typename Digraph::Node Node;
    void start(const Node&) {}
    void reach(const Node&) {}
    void process(const Node&) {}
    void discover(const Arc&) {}
    void examine(const Arc&) {}

    template <typename _Visitor>
    struct Constraints {
      void constraints() {
        Arc arc;
        Node node;
        visitor.start(node);
        visitor.reach(node);
        visitor.process(node);
        visitor.discover(arc);
        visitor.examine(arc);
      }
      _Visitor& visitor;
      Constraints() {}
    };
  };
#endif

  /// \brief Default traits class of BfsVisit class.
  ///
  /// Default traits class of BfsVisit class.
  /// \tparam GR The type of the digraph the algorithm runs on.
  template<class GR>
  struct BfsVisitDefaultTraits {

    /// \brief The type of the digraph the algorithm runs on.
    typedef GR Digraph;

    /// \brief The type of the map that indicates which nodes are reached.
    ///
    /// The type of the map that indicates which nodes are reached.
    /// It must conform to
    ///the \ref concepts::ReadWriteMap "ReadWriteMap" concept.
    typedef typename Digraph::template NodeMap<bool> ReachedMap;

    /// \brief Instantiates a ReachedMap.
    ///
    /// This function instantiates a ReachedMap.
    /// \param digraph is the digraph, to which
    /// we would like to define the ReachedMap.
    static ReachedMap *createReachedMap(const Digraph &digraph) {
      return new ReachedMap(digraph);
    }

  };

  /// \ingroup search
  ///
  /// \brief BFS algorithm class with visitor interface.
  ///
  /// This class provides an efficient implementation of the BFS algorithm
  /// with visitor interface.
  ///
  /// The BfsVisit class provides an alternative interface to the Bfs
  /// class. It works with callback mechanism, the BfsVisit object calls
  /// the member functions of the \c Visitor class on every BFS event.
  ///
  /// This interface of the BFS algorithm should be used in special cases
  /// when extra actions have to be performed in connection with certain
  /// events of the BFS algorithm. Otherwise consider to use Bfs or bfs()
  /// instead.
  ///
  /// \tparam GR The type of the digraph the algorithm runs on.
  /// The default type is \ref ListDigraph.
  /// The value of GR is not used directly by \ref BfsVisit,
  /// it is only passed to \ref BfsVisitDefaultTraits.
  /// \tparam VS The Visitor type that is used by the algorithm.
  /// \ref BfsVisitor "BfsVisitor<GR>" is an empty visitor, which
  /// does not observe the BFS events. If you want to observe the BFS
  /// events, you should implement your own visitor class.
  /// \tparam TR The traits class that defines various types used by the
  /// algorithm. By default, it is \ref BfsVisitDefaultTraits
  /// "BfsVisitDefaultTraits<GR>".
  /// In most cases, this parameter should not be set directly,
  /// consider to use the named template parameters instead.
#ifdef DOXYGEN
  template <typename GR, typename VS, typename TR>
#else
  template <typename GR = ListDigraph,
            typename VS = BfsVisitor<GR>,
            typename TR = BfsVisitDefaultTraits<GR> >
#endif
  class BfsVisit {
  public:

    ///The traits class.
    typedef TR Traits;

    ///The type of the digraph the algorithm runs on.
    typedef typename Traits::Digraph Digraph;

    ///The visitor type used by the algorithm.
    typedef VS Visitor;

    ///The type of the map that indicates which nodes are reached.
    typedef typename Traits::ReachedMap ReachedMap;

  private:

    typedef typename Digraph::Node Node;
    typedef typename Digraph::NodeIt NodeIt;
    typedef typename Digraph::Arc Arc;
    typedef typename Digraph::OutArcIt OutArcIt;

    //Pointer to the underlying digraph.
    const Digraph *_digraph;
    //Pointer to the visitor object.
    Visitor *_visitor;
    //Pointer to the map of reached status of the nodes.
    ReachedMap *_reached;
    //Indicates if _reached is locally allocated (true) or not.
    bool local_reached;

    std::vector<typename Digraph::Node> _list;
    int _list_front, _list_back;

    //Creates the maps if necessary.
    void create_maps() {
      if(!_reached) {
        local_reached = true;
        _reached = Traits::createReachedMap(*_digraph);
      }
    }

  protected:

    BfsVisit() {}

  public:

    typedef BfsVisit Create;

    /// \name Named Template Parameters

    ///@{
    template <class T>
    struct SetReachedMapTraits : public Traits {
      typedef T ReachedMap;
      static ReachedMap *createReachedMap(const Digraph &digraph) {
        LEMON_ASSERT(false, "ReachedMap is not initialized");
        return 0; // ignore warnings
      }
    };
    /// \brief \ref named-templ-param "Named parameter" for setting
    /// ReachedMap type.
    ///
    /// \ref named-templ-param "Named parameter" for setting ReachedMap type.
    template <class T>
    struct SetReachedMap : public BfsVisit< Digraph, Visitor,
                                            SetReachedMapTraits<T> > {
      typedef BfsVisit< Digraph, Visitor, SetReachedMapTraits<T> > Create;
    };
    ///@}

  public:

    /// \brief Constructor.
    ///
    /// Constructor.
    ///
    /// \param digraph The digraph the algorithm runs on.
    /// \param visitor The visitor object of the algorithm.
    BfsVisit(const Digraph& digraph, Visitor& visitor)
      : _digraph(&digraph), _visitor(&visitor),
        _reached(0), local_reached(false) {}

    /// \brief Destructor.
    ~BfsVisit() {
      if(local_reached) delete _reached;
    }

    /// \brief Sets the map that indicates which nodes are reached.
    ///
    /// Sets the map that indicates which nodes are reached.
    /// If you don't use this function before calling \ref run(Node) "run()"
    /// or \ref init(), an instance will be allocated automatically.
    /// The destructor deallocates this automatically allocated map,
    /// of course.
    /// \return <tt> (*this) </tt>
    BfsVisit &reachedMap(ReachedMap &m) {
      if(local_reached) {
        delete _reached;
        local_reached = false;
      }
      _reached = &m;
      return *this;
    }

  public:

    /// \name Execution Control
    /// The simplest way to execute the BFS algorithm is to use one of the
    /// member functions called \ref run(Node) "run()".\n
    /// If you need better control on the execution, you have to call
    /// \ref init() first, then you can add several source nodes with
    /// \ref addSource(). Finally the actual path computation can be
    /// performed with one of the \ref start() functions.

    /// @{

    /// \brief Initializes the internal data structures.
    ///
    /// Initializes the internal data structures.
    void init() {
      create_maps();
      _list.resize(countNodes(*_digraph));
      _list_front = _list_back = -1;
      for (NodeIt u(*_digraph) ; u != INVALID ; ++u) {
        _reached->set(u, false);
      }
    }

    /// \brief Adds a new source node.
    ///
    /// Adds a new source node to the set of nodes to be processed.
    void addSource(Node s) {
      if(!(*_reached)[s]) {
          _reached->set(s,true);
          _visitor->start(s);
          _visitor->reach(s);
          _list[++_list_back] = s;
        }
    }

    /// \brief Processes the next node.
    ///
    /// Processes the next node.
    ///
    /// \return The processed node.
    ///
    /// \pre The queue must not be empty.
    Node processNextNode() {
      Node n = _list[++_list_front];
      _visitor->process(n);
      Arc e;
      for (_digraph->firstOut(e, n); e != INVALID; _digraph->nextOut(e)) {
        Node m = _digraph->target(e);
        if (!(*_reached)[m]) {
          _visitor->discover(e);
          _visitor->reach(m);
          _reached->set(m, true);
          _list[++_list_back] = m;
        } else {
          _visitor->examine(e);
        }
      }
      return n;
    }

    /// \brief Processes the next node.
    ///
    /// Processes the next node and checks if the given target node
    /// is reached. If the target node is reachable from the processed
    /// node, then the \c reach parameter will be set to \c true.
    ///
    /// \param target The target node.
    /// \retval reach Indicates if the target node is reached.
    /// It should be initially \c false.
    ///
    /// \return The processed node.
    ///
    /// \pre The queue must not be empty.
    Node processNextNode(Node target, bool& reach) {
      Node n = _list[++_list_front];
      _visitor->process(n);
      Arc e;
      for (_digraph->firstOut(e, n); e != INVALID; _digraph->nextOut(e)) {
        Node m = _digraph->target(e);
        if (!(*_reached)[m]) {
          _visitor->discover(e);
          _visitor->reach(m);
          _reached->set(m, true);
          _list[++_list_back] = m;
          reach = reach || (target == m);
        } else {
          _visitor->examine(e);
        }
      }
      return n;
    }

    /// \brief Processes the next node.
    ///
    /// Processes the next node and checks if at least one of reached
    /// nodes has \c true value in the \c nm node map. If one node
    /// with \c true value is reachable from the processed node, then the
    /// \c rnode parameter will be set to the first of such nodes.
    ///
    /// \param nm A \c bool (or convertible) node map that indicates the
    /// possible targets.
    /// \retval rnode The reached target node.
    /// It should be initially \c INVALID.
    ///
    /// \return The processed node.
    ///
    /// \pre The queue must not be empty.
    template <typename NM>
    Node processNextNode(const NM& nm, Node& rnode) {
      Node n = _list[++_list_front];
      _visitor->process(n);
      Arc e;
      for (_digraph->firstOut(e, n); e != INVALID; _digraph->nextOut(e)) {
        Node m = _digraph->target(e);
        if (!(*_reached)[m]) {
          _visitor->discover(e);
          _visitor->reach(m);
          _reached->set(m, true);
          _list[++_list_back] = m;
          if (nm[m] && rnode == INVALID) rnode = m;
        } else {
          _visitor->examine(e);
        }
      }
      return n;
    }

    /// \brief The next node to be processed.
    ///
    /// Returns the next node to be processed or \c INVALID if the queue
    /// is empty.
    Node nextNode() const {
      return _list_front != _list_back ? _list[_list_front + 1] : INVALID;
    }

    /// \brief Returns \c false if there are nodes
    /// to be processed.
    ///
    /// Returns \c false if there are nodes
    /// to be processed in the queue.
    bool emptyQueue() const { return _list_front == _list_back; }

    /// \brief Returns the number of the nodes to be processed.
    ///
    /// Returns the number of the nodes to be processed in the queue.
    int queueSize() const { return _list_back - _list_front; }

    /// \brief Executes the algorithm.
    ///
    /// Executes the algorithm.
    ///
    /// This method runs the %BFS algorithm from the root node(s)
    /// in order to compute the shortest path to each node.
    ///
    /// The algorithm computes
    /// - the shortest path tree (forest),
    /// - the distance of each node from the root(s).
    ///
    /// \pre init() must be called and at least one root node should be added
    /// with addSource() before using this function.
    ///
    /// \note <tt>b.start()</tt> is just a shortcut of the following code.
    /// \code
    ///   while ( !b.emptyQueue() ) {
    ///     b.processNextNode();
    ///   }
    /// \endcode
    void start() {
      while ( !emptyQueue() ) processNextNode();
    }

    /// \brief Executes the algorithm until the given target node is reached.
    ///
    /// Executes the algorithm until the given target node is reached.
    ///
    /// This method runs the %BFS algorithm from the root node(s)
    /// in order to compute the shortest path to \c t.
    ///
    /// The algorithm computes
    /// - the shortest path to \c t,
    /// - the distance of \c t from the root(s).
    ///
    /// \pre init() must be called and at least one root node should be
    /// added with addSource() before using this function.
    ///
    /// \note <tt>b.start(t)</tt> is just a shortcut of the following code.
    /// \code
    ///   bool reach = false;
    ///   while ( !b.emptyQueue() && !reach ) {
    ///     b.processNextNode(t, reach);
    ///   }
    /// \endcode
    void start(Node t) {
      bool reach = false;
      while ( !emptyQueue() && !reach ) processNextNode(t, reach);
    }

    /// \brief Executes the algorithm until a condition is met.
    ///
    /// Executes the algorithm until a condition is met.
    ///
    /// This method runs the %BFS algorithm from the root node(s) in
    /// order to compute the shortest path to a node \c v with
    /// <tt>nm[v]</tt> true, if such a node can be found.
    ///
    /// \param nm must be a bool (or convertible) node map. The
    /// algorithm will stop when it reaches a node \c v with
    /// <tt>nm[v]</tt> true.
    ///
    /// \return The reached node \c v with <tt>nm[v]</tt> true or
    /// \c INVALID if no such node was found.
    ///
    /// \pre init() must be called and at least one root node should be
    /// added with addSource() before using this function.
    ///
    /// \note <tt>b.start(nm)</tt> is just a shortcut of the following code.
    /// \code
    ///   Node rnode = INVALID;
    ///   while ( !b.emptyQueue() && rnode == INVALID ) {
    ///     b.processNextNode(nm, rnode);
    ///   }
    ///   return rnode;
    /// \endcode
    template <typename NM>
    Node start(const NM &nm) {
      Node rnode = INVALID;
      while ( !emptyQueue() && rnode == INVALID ) {
        processNextNode(nm, rnode);
      }
      return rnode;
    }

    /// \brief Runs the algorithm from the given source node.
    ///
    /// This method runs the %BFS algorithm from node \c s
    /// in order to compute the shortest path to each node.
    ///
    /// The algorithm computes
    /// - the shortest path tree,
    /// - the distance of each node from the root.
    ///
    /// \note <tt>b.run(s)</tt> is just a shortcut of the following code.
    ///\code
    ///   b.init();
    ///   b.addSource(s);
    ///   b.start();
    ///\endcode
    void run(Node s) {
      init();
      addSource(s);
      start();
    }

    /// \brief Finds the shortest path between \c s and \c t.
    ///
    /// This method runs the %BFS algorithm from node \c s
    /// in order to compute the shortest path to node \c t
    /// (it stops searching when \c t is processed).
    ///
    /// \return \c true if \c t is reachable form \c s.
    ///
    /// \note Apart from the return value, <tt>b.run(s,t)</tt> is just a
    /// shortcut of the following code.
    ///\code
    ///   b.init();
    ///   b.addSource(s);
    ///   b.start(t);
    ///\endcode
    bool run(Node s,Node t) {
      init();
      addSource(s);
      start(t);
      return reached(t);
    }

    /// \brief Runs the algorithm to visit all nodes in the digraph.
    ///
    /// This method runs the %BFS algorithm in order to visit all nodes
    /// in the digraph.
    ///
    /// \note <tt>b.run(s)</tt> is just a shortcut of the following code.
    ///\code
    ///  b.init();
    ///  for (NodeIt n(gr); n != INVALID; ++n) {
    ///    if (!b.reached(n)) {
    ///      b.addSource(n);
    ///      b.start();
    ///    }
    ///  }
    ///\endcode
    void run() {
      init();
      for (NodeIt it(*_digraph); it != INVALID; ++it) {
        if (!reached(it)) {
          addSource(it);
          start();
        }
      }
    }

    ///@}

    /// \name Query Functions
    /// The results of the BFS algorithm can be obtained using these
    /// functions.\n
    /// Either \ref run(Node) "run()" or \ref start() should be called
    /// before using them.

    ///@{

    /// \brief Checks if the given node is reached from the root(s).
    ///
    /// Returns \c true if \c v is reached from the root(s).
    ///
    /// \pre Either \ref run(Node) "run()" or \ref init()
    /// must be called before using this function.
    bool reached(Node v) const { return (*_reached)[v]; }

    ///@}

  };

} //END OF NAMESPACE LEMON

#endif
