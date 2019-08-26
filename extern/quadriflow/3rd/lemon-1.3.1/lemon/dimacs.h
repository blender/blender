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

#ifndef LEMON_DIMACS_H
#define LEMON_DIMACS_H

#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <lemon/maps.h>
#include <lemon/error.h>
/// \ingroup dimacs_group
/// \file
/// \brief DIMACS file format reader.

namespace lemon {

  /// \addtogroup dimacs_group
  /// @{

  /// DIMACS file type descriptor.
  struct DimacsDescriptor
  {
    ///\brief DIMACS file type enum
    ///
    ///DIMACS file type enum.
    enum Type {
      NONE,  ///< Undefined type.
      MIN,   ///< DIMACS file type for minimum cost flow problems.
      MAX,   ///< DIMACS file type for maximum flow problems.
      SP,    ///< DIMACS file type for shostest path problems.
      MAT    ///< DIMACS file type for plain graphs and matching problems.
    };
    ///The file type
    Type type;
    ///The number of nodes in the graph
    int nodeNum;
    ///The number of edges in the graph
    int edgeNum;
    int lineShift;
    ///Constructor. It sets the type to \c NONE.
    DimacsDescriptor() : type(NONE) {}
  };

  ///Discover the type of a DIMACS file

  ///This function starts seeking the beginning of the given file for the
  ///problem type and size info.
  ///The found data is returned in a special struct that can be evaluated
  ///and passed to the appropriate reader function.
  DimacsDescriptor dimacsType(std::istream& is)
  {
    DimacsDescriptor r;
    std::string problem,str;
    char c;
    r.lineShift=0;
    while (is >> c)
      switch(c)
        {
        case 'p':
          if(is >> problem >> r.nodeNum >> r.edgeNum)
            {
              getline(is, str);
              r.lineShift++;
              if(problem=="min") r.type=DimacsDescriptor::MIN;
              else if(problem=="max") r.type=DimacsDescriptor::MAX;
              else if(problem=="sp") r.type=DimacsDescriptor::SP;
              else if(problem=="mat") r.type=DimacsDescriptor::MAT;
              else throw FormatError("Unknown problem type");
              return r;
            }
          else
            {
              throw FormatError("Missing or wrong problem type declaration.");
            }
          break;
        case 'c':
          getline(is, str);
          r.lineShift++;
          break;
        default:
          throw FormatError("Unknown DIMACS declaration.");
        }
    throw FormatError("Missing problem type declaration.");
  }


  /// \brief DIMACS minimum cost flow reader function.
  ///
  /// This function reads a minimum cost flow instance from DIMACS format,
  /// i.e. from a DIMACS file having a line starting with
  /// \code
  ///   p min
  /// \endcode
  /// At the beginning, \c g is cleared by \c g.clear(). The supply
  /// amount of the nodes are written to the \c supply node map
  /// (they are signed values). The lower bounds, capacities and costs
  /// of the arcs are written to the \c lower, \c capacity and \c cost
  /// arc maps.
  ///
  /// If the capacity of an arc is less than the lower bound, it will
  /// be set to "infinite" instead. The actual value of "infinite" is
  /// contolled by the \c infty parameter. If it is 0 (the default value),
  /// \c std::numeric_limits<Capacity>::infinity() will be used if available,
  /// \c std::numeric_limits<Capacity>::max() otherwise. If \c infty is set to
  /// a non-zero value, that value will be used as "infinite".
  ///
  /// If the file type was previously evaluated by dimacsType(), then
  /// the descriptor struct should be given by the \c dest parameter.
  template <typename Digraph, typename LowerMap,
            typename CapacityMap, typename CostMap,
            typename SupplyMap>
  void readDimacsMin(std::istream& is,
                     Digraph &g,
                     LowerMap& lower,
                     CapacityMap& capacity,
                     CostMap& cost,
                     SupplyMap& supply,
                     typename CapacityMap::Value infty = 0,
                     DimacsDescriptor desc=DimacsDescriptor())
  {
    g.clear();
    std::vector<typename Digraph::Node> nodes;
    typename Digraph::Arc e;
    std::string problem, str;
    char c;
    int i, j;
    if(desc.type==DimacsDescriptor::NONE) desc=dimacsType(is);
    if(desc.type!=DimacsDescriptor::MIN)
      throw FormatError("Problem type mismatch");

    nodes.resize(desc.nodeNum + 1);
    for (int k = 1; k <= desc.nodeNum; ++k) {
      nodes[k] = g.addNode();
      supply.set(nodes[k], 0);
    }

    typename SupplyMap::Value sup;
    typename CapacityMap::Value low;
    typename CapacityMap::Value cap;
    typename CostMap::Value co;
    typedef typename CapacityMap::Value Capacity;
    if(infty==0)
      infty = std::numeric_limits<Capacity>::has_infinity ?
        std::numeric_limits<Capacity>::infinity() :
        std::numeric_limits<Capacity>::max();

    while (is >> c) {
      switch (c) {
      case 'c': // comment line
        getline(is, str);
        break;
      case 'n': // node definition line
        is >> i >> sup;
        getline(is, str);
        supply.set(nodes[i], sup);
        break;
      case 'a': // arc definition line
        is >> i >> j >> low >> cap >> co;
        getline(is, str);
        e = g.addArc(nodes[i], nodes[j]);
        lower.set(e, low);
        if (cap >= low)
          capacity.set(e, cap);
        else
          capacity.set(e, infty);
        cost.set(e, co);
        break;
      }
    }
  }

  template<typename Digraph, typename CapacityMap>
  void _readDimacs(std::istream& is,
                   Digraph &g,
                   CapacityMap& capacity,
                   typename Digraph::Node &s,
                   typename Digraph::Node &t,
                   typename CapacityMap::Value infty = 0,
                   DimacsDescriptor desc=DimacsDescriptor()) {
    g.clear();
    s=t=INVALID;
    std::vector<typename Digraph::Node> nodes;
    typename Digraph::Arc e;
    char c, d;
    int i, j;
    typename CapacityMap::Value _cap;
    std::string str;
    nodes.resize(desc.nodeNum + 1);
    for (int k = 1; k <= desc.nodeNum; ++k) {
      nodes[k] = g.addNode();
    }
    typedef typename CapacityMap::Value Capacity;

    if(infty==0)
      infty = std::numeric_limits<Capacity>::has_infinity ?
        std::numeric_limits<Capacity>::infinity() :
        std::numeric_limits<Capacity>::max();

    while (is >> c) {
      switch (c) {
      case 'c': // comment line
        getline(is, str);
        break;
      case 'n': // node definition line
        if (desc.type==DimacsDescriptor::SP) { // shortest path problem
          is >> i;
          getline(is, str);
          s = nodes[i];
        }
        if (desc.type==DimacsDescriptor::MAX) { // max flow problem
          is >> i >> d;
          getline(is, str);
          if (d == 's') s = nodes[i];
          if (d == 't') t = nodes[i];
        }
        break;
      case 'a': // arc definition line
        if (desc.type==DimacsDescriptor::SP) {
          is >> i >> j >> _cap;
          getline(is, str);
          e = g.addArc(nodes[i], nodes[j]);
          capacity.set(e, _cap);
        }
        else if (desc.type==DimacsDescriptor::MAX) {
          is >> i >> j >> _cap;
          getline(is, str);
          e = g.addArc(nodes[i], nodes[j]);
          if (_cap >= 0)
            capacity.set(e, _cap);
          else
            capacity.set(e, infty);
        }
        else {
          is >> i >> j;
          getline(is, str);
          g.addArc(nodes[i], nodes[j]);
        }
        break;
      }
    }
  }

  /// \brief DIMACS maximum flow reader function.
  ///
  /// This function reads a maximum flow instance from DIMACS format,
  /// i.e. from a DIMACS file having a line starting with
  /// \code
  ///   p max
  /// \endcode
  /// At the beginning, \c g is cleared by \c g.clear(). The arc
  /// capacities are written to the \c capacity arc map and \c s and
  /// \c t are set to the source and the target nodes.
  ///
  /// If the capacity of an arc is negative, it will
  /// be set to "infinite" instead. The actual value of "infinite" is
  /// contolled by the \c infty parameter. If it is 0 (the default value),
  /// \c std::numeric_limits<Capacity>::infinity() will be used if available,
  /// \c std::numeric_limits<Capacity>::max() otherwise. If \c infty is set to
  /// a non-zero value, that value will be used as "infinite".
  ///
  /// If the file type was previously evaluated by dimacsType(), then
  /// the descriptor struct should be given by the \c dest parameter.
  template<typename Digraph, typename CapacityMap>
  void readDimacsMax(std::istream& is,
                     Digraph &g,
                     CapacityMap& capacity,
                     typename Digraph::Node &s,
                     typename Digraph::Node &t,
                     typename CapacityMap::Value infty = 0,
                     DimacsDescriptor desc=DimacsDescriptor()) {
    if(desc.type==DimacsDescriptor::NONE) desc=dimacsType(is);
    if(desc.type!=DimacsDescriptor::MAX)
      throw FormatError("Problem type mismatch");
    _readDimacs(is,g,capacity,s,t,infty,desc);
  }

  /// \brief DIMACS shortest path reader function.
  ///
  /// This function reads a shortest path instance from DIMACS format,
  /// i.e. from a DIMACS file having a line starting with
  /// \code
  ///   p sp
  /// \endcode
  /// At the beginning, \c g is cleared by \c g.clear(). The arc
  /// lengths are written to the \c length arc map and \c s is set to the
  /// source node.
  ///
  /// If the file type was previously evaluated by dimacsType(), then
  /// the descriptor struct should be given by the \c dest parameter.
  template<typename Digraph, typename LengthMap>
  void readDimacsSp(std::istream& is,
                    Digraph &g,
                    LengthMap& length,
                    typename Digraph::Node &s,
                    DimacsDescriptor desc=DimacsDescriptor()) {
    typename Digraph::Node t;
    if(desc.type==DimacsDescriptor::NONE) desc=dimacsType(is);
    if(desc.type!=DimacsDescriptor::SP)
      throw FormatError("Problem type mismatch");
    _readDimacs(is, g, length, s, t, 0, desc);
  }

  /// \brief DIMACS capacitated digraph reader function.
  ///
  /// This function reads an arc capacitated digraph instance from
  /// DIMACS 'max' or 'sp' format.
  /// At the beginning, \c g is cleared by \c g.clear()
  /// and the arc capacities/lengths are written to the \c capacity
  /// arc map.
  ///
  /// In case of the 'max' format, if the capacity of an arc is negative,
  /// it will
  /// be set to "infinite" instead. The actual value of "infinite" is
  /// contolled by the \c infty parameter. If it is 0 (the default value),
  /// \c std::numeric_limits<Capacity>::infinity() will be used if available,
  /// \c std::numeric_limits<Capacity>::max() otherwise. If \c infty is set to
  /// a non-zero value, that value will be used as "infinite".
  ///
  /// If the file type was previously evaluated by dimacsType(), then
  /// the descriptor struct should be given by the \c dest parameter.
  template<typename Digraph, typename CapacityMap>
  void readDimacsCap(std::istream& is,
                     Digraph &g,
                     CapacityMap& capacity,
                     typename CapacityMap::Value infty = 0,
                     DimacsDescriptor desc=DimacsDescriptor()) {
    typename Digraph::Node u,v;
    if(desc.type==DimacsDescriptor::NONE) desc=dimacsType(is);
    if(desc.type!=DimacsDescriptor::MAX || desc.type!=DimacsDescriptor::SP)
      throw FormatError("Problem type mismatch");
    _readDimacs(is, g, capacity, u, v, infty, desc);
  }

  template<typename Graph>
  typename enable_if<lemon::UndirectedTagIndicator<Graph>,void>::type
  _addArcEdge(Graph &g, typename Graph::Node s, typename Graph::Node t,
              dummy<0> = 0)
  {
    g.addEdge(s,t);
  }
  template<typename Graph>
  typename disable_if<lemon::UndirectedTagIndicator<Graph>,void>::type
  _addArcEdge(Graph &g, typename Graph::Node s, typename Graph::Node t,
              dummy<1> = 1)
  {
    g.addArc(s,t);
  }

  /// \brief DIMACS plain (di)graph reader function.
  ///
  /// This function reads a plain (di)graph without any designated nodes
  /// and maps (e.g. a matching instance) from DIMACS format, i.e. from
  /// DIMACS files having a line starting with
  /// \code
  ///   p mat
  /// \endcode
  /// At the beginning, \c g is cleared by \c g.clear().
  ///
  /// If the file type was previously evaluated by dimacsType(), then
  /// the descriptor struct should be given by the \c dest parameter.
  template<typename Graph>
  void readDimacsMat(std::istream& is, Graph &g,
                     DimacsDescriptor desc=DimacsDescriptor())
  {
    if(desc.type==DimacsDescriptor::NONE) desc=dimacsType(is);
    if(desc.type!=DimacsDescriptor::MAT)
      throw FormatError("Problem type mismatch");

    g.clear();
    std::vector<typename Graph::Node> nodes;
    char c;
    int i, j;
    std::string str;
    nodes.resize(desc.nodeNum + 1);
    for (int k = 1; k <= desc.nodeNum; ++k) {
      nodes[k] = g.addNode();
    }

    while (is >> c) {
      switch (c) {
      case 'c': // comment line
        getline(is, str);
        break;
      case 'n': // node definition line
        break;
      case 'a': // arc definition line
        is >> i >> j;
        getline(is, str);
        _addArcEdge(g,nodes[i], nodes[j]);
        break;
      }
    }
  }

  /// DIMACS plain digraph writer function.
  ///
  /// This function writes a digraph without any designated nodes and
  /// maps into DIMACS format, i.e. into DIMACS file having a line
  /// starting with
  /// \code
  ///   p mat
  /// \endcode
  /// If \c comment is not empty, then it will be printed in the first line
  /// prefixed by 'c'.
  template<typename Digraph>
  void writeDimacsMat(std::ostream& os, const Digraph &g,
                      std::string comment="") {
    typedef typename Digraph::NodeIt NodeIt;
    typedef typename Digraph::ArcIt ArcIt;

    if(!comment.empty())
      os << "c " << comment << std::endl;
    os << "p mat " << g.nodeNum() << " " << g.arcNum() << std::endl;

    typename Digraph::template NodeMap<int> nodes(g);
    int i = 1;
    for(NodeIt v(g); v != INVALID; ++v) {
      nodes.set(v, i);
      ++i;
    }
    for(ArcIt e(g); e != INVALID; ++e) {
      os << "a " << nodes[g.source(e)] << " " << nodes[g.target(e)]
         << std::endl;
    }
  }

  /// @}

} //namespace lemon

#endif //LEMON_DIMACS_H
