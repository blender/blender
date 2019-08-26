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

#ifndef LEMON_EULER_H
#define LEMON_EULER_H

#include<lemon/core.h>
#include<lemon/adaptors.h>
#include<lemon/connectivity.h>
#include <list>

/// \ingroup graph_properties
/// \file
/// \brief Euler tour iterators and a function for checking the \e Eulerian
/// property.
///
///This file provides Euler tour iterators and a function to check
///if a (di)graph is \e Eulerian.

namespace lemon {

  ///Euler tour iterator for digraphs.

  /// \ingroup graph_properties
  ///This iterator provides an Euler tour (Eulerian circuit) of a \e directed
  ///graph (if there exists) and it converts to the \c Arc type of the digraph.
  ///
  ///For example, if the given digraph has an Euler tour (i.e it has only one
  ///non-trivial component and the in-degree is equal to the out-degree
  ///for all nodes), then the following code will put the arcs of \c g
  ///to the vector \c et according to an Euler tour of \c g.
  ///\code
  ///  std::vector<ListDigraph::Arc> et;
  ///  for(DiEulerIt<ListDigraph> e(g); e!=INVALID; ++e)
  ///    et.push_back(e);
  ///\endcode
  ///If \c g has no Euler tour, then the resulted walk will not be closed
  ///or not contain all arcs.
  ///\sa EulerIt
  template<typename GR>
  class DiEulerIt
  {
    typedef typename GR::Node Node;
    typedef typename GR::NodeIt NodeIt;
    typedef typename GR::Arc Arc;
    typedef typename GR::ArcIt ArcIt;
    typedef typename GR::OutArcIt OutArcIt;
    typedef typename GR::InArcIt InArcIt;

    const GR &g;
    typename GR::template NodeMap<OutArcIt> narc;
    std::list<Arc> euler;

  public:

    ///Constructor

    ///Constructor.
    ///\param gr A digraph.
    ///\param start The starting point of the tour. If it is not given,
    ///the tour will start from the first node that has an outgoing arc.
    DiEulerIt(const GR &gr, typename GR::Node start = INVALID)
      : g(gr), narc(g)
    {
      if (start==INVALID) {
        NodeIt n(g);
        while (n!=INVALID && OutArcIt(g,n)==INVALID) ++n;
        start=n;
      }
      if (start!=INVALID) {
        for (NodeIt n(g); n!=INVALID; ++n) narc[n]=OutArcIt(g,n);
        while (narc[start]!=INVALID) {
          euler.push_back(narc[start]);
          Node next=g.target(narc[start]);
          ++narc[start];
          start=next;
        }
      }
    }

    ///Arc conversion
    operator Arc() { return euler.empty()?INVALID:euler.front(); }
    ///Compare with \c INVALID
    bool operator==(Invalid) { return euler.empty(); }
    ///Compare with \c INVALID
    bool operator!=(Invalid) { return !euler.empty(); }

    ///Next arc of the tour

    ///Next arc of the tour
    ///
    DiEulerIt &operator++() {
      Node s=g.target(euler.front());
      euler.pop_front();
      typename std::list<Arc>::iterator next=euler.begin();
      while(narc[s]!=INVALID) {
        euler.insert(next,narc[s]);
        Node n=g.target(narc[s]);
        ++narc[s];
        s=n;
      }
      return *this;
    }
    ///Postfix incrementation

    /// Postfix incrementation.
    ///
    ///\warning This incrementation
    ///returns an \c Arc, not a \ref DiEulerIt, as one may
    ///expect.
    Arc operator++(int)
    {
      Arc e=*this;
      ++(*this);
      return e;
    }
  };

  ///Euler tour iterator for graphs.

  /// \ingroup graph_properties
  ///This iterator provides an Euler tour (Eulerian circuit) of an
  ///\e undirected graph (if there exists) and it converts to the \c Arc
  ///and \c Edge types of the graph.
  ///
  ///For example, if the given graph has an Euler tour (i.e it has only one
  ///non-trivial component and the degree of each node is even),
  ///the following code will print the arc IDs according to an
  ///Euler tour of \c g.
  ///\code
  ///  for(EulerIt<ListGraph> e(g); e!=INVALID; ++e) {
  ///    std::cout << g.id(Edge(e)) << std::eol;
  ///  }
  ///\endcode
  ///Although this iterator is for undirected graphs, it still returns
  ///arcs in order to indicate the direction of the tour.
  ///(But arcs convert to edges, of course.)
  ///
  ///If \c g has no Euler tour, then the resulted walk will not be closed
  ///or not contain all edges.
  template<typename GR>
  class EulerIt
  {
    typedef typename GR::Node Node;
    typedef typename GR::NodeIt NodeIt;
    typedef typename GR::Arc Arc;
    typedef typename GR::Edge Edge;
    typedef typename GR::ArcIt ArcIt;
    typedef typename GR::OutArcIt OutArcIt;
    typedef typename GR::InArcIt InArcIt;

    const GR &g;
    typename GR::template NodeMap<OutArcIt> narc;
    typename GR::template EdgeMap<bool> visited;
    std::list<Arc> euler;

  public:

    ///Constructor

    ///Constructor.
    ///\param gr A graph.
    ///\param start The starting point of the tour. If it is not given,
    ///the tour will start from the first node that has an incident edge.
    EulerIt(const GR &gr, typename GR::Node start = INVALID)
      : g(gr), narc(g), visited(g, false)
    {
      if (start==INVALID) {
        NodeIt n(g);
        while (n!=INVALID && OutArcIt(g,n)==INVALID) ++n;
        start=n;
      }
      if (start!=INVALID) {
        for (NodeIt n(g); n!=INVALID; ++n) narc[n]=OutArcIt(g,n);
        while(narc[start]!=INVALID) {
          euler.push_back(narc[start]);
          visited[narc[start]]=true;
          Node next=g.target(narc[start]);
          ++narc[start];
          start=next;
          while(narc[start]!=INVALID && visited[narc[start]]) ++narc[start];
        }
      }
    }

    ///Arc conversion
    operator Arc() const { return euler.empty()?INVALID:euler.front(); }
    ///Edge conversion
    operator Edge() const { return euler.empty()?INVALID:euler.front(); }
    ///Compare with \c INVALID
    bool operator==(Invalid) const { return euler.empty(); }
    ///Compare with \c INVALID
    bool operator!=(Invalid) const { return !euler.empty(); }

    ///Next arc of the tour

    ///Next arc of the tour
    ///
    EulerIt &operator++() {
      Node s=g.target(euler.front());
      euler.pop_front();
      typename std::list<Arc>::iterator next=euler.begin();
      while(narc[s]!=INVALID) {
        while(narc[s]!=INVALID && visited[narc[s]]) ++narc[s];
        if(narc[s]==INVALID) break;
        else {
          euler.insert(next,narc[s]);
          visited[narc[s]]=true;
          Node n=g.target(narc[s]);
          ++narc[s];
          s=n;
        }
      }
      return *this;
    }

    ///Postfix incrementation

    /// Postfix incrementation.
    ///
    ///\warning This incrementation returns an \c Arc (which converts to
    ///an \c Edge), not an \ref EulerIt, as one may expect.
    Arc operator++(int)
    {
      Arc e=*this;
      ++(*this);
      return e;
    }
  };


  ///Check if the given graph is Eulerian

  /// \ingroup graph_properties
  ///This function checks if the given graph is Eulerian.
  ///It works for both directed and undirected graphs.
  ///
  ///By definition, a digraph is called \e Eulerian if
  ///and only if it is connected and the number of incoming and outgoing
  ///arcs are the same for each node.
  ///Similarly, an undirected graph is called \e Eulerian if
  ///and only if it is connected and the number of incident edges is even
  ///for each node.
  ///
  ///\note There are (di)graphs that are not Eulerian, but still have an
  /// Euler tour, since they may contain isolated nodes.
  ///
  ///\sa DiEulerIt, EulerIt
  template<typename GR>
#ifdef DOXYGEN
  bool
#else
  typename enable_if<UndirectedTagIndicator<GR>,bool>::type
  eulerian(const GR &g)
  {
    for(typename GR::NodeIt n(g);n!=INVALID;++n)
      if(countIncEdges(g,n)%2) return false;
    return connected(g);
  }
  template<class GR>
  typename disable_if<UndirectedTagIndicator<GR>,bool>::type
#endif
  eulerian(const GR &g)
  {
    for(typename GR::NodeIt n(g);n!=INVALID;++n)
      if(countInArcs(g,n)!=countOutArcs(g,n)) return false;
    return connected(undirector(g));
  }

}

#endif
