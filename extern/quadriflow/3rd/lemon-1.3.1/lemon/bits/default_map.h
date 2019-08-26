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

#ifndef LEMON_BITS_DEFAULT_MAP_H
#define LEMON_BITS_DEFAULT_MAP_H

#include <lemon/config.h>
#include <lemon/bits/array_map.h>
#include <lemon/bits/vector_map.h>
//#include <lemon/bits/debug_map.h>

//\ingroup graphbits
//\file
//\brief Graph maps that construct and destruct their elements dynamically.

namespace lemon {


  //#ifndef LEMON_USE_DEBUG_MAP

  template <typename _Graph, typename _Item, typename _Value>
  struct DefaultMapSelector {
    typedef ArrayMap<_Graph, _Item, _Value> Map;
  };

  // bool
  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, bool> {
    typedef VectorMap<_Graph, _Item, bool> Map;
  };

  // char
  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, char> {
    typedef VectorMap<_Graph, _Item, char> Map;
  };

  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, signed char> {
    typedef VectorMap<_Graph, _Item, signed char> Map;
  };

  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, unsigned char> {
    typedef VectorMap<_Graph, _Item, unsigned char> Map;
  };


  // int
  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, signed int> {
    typedef VectorMap<_Graph, _Item, signed int> Map;
  };

  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, unsigned int> {
    typedef VectorMap<_Graph, _Item, unsigned int> Map;
  };


  // short
  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, signed short> {
    typedef VectorMap<_Graph, _Item, signed short> Map;
  };

  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, unsigned short> {
    typedef VectorMap<_Graph, _Item, unsigned short> Map;
  };


  // long
  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, signed long> {
    typedef VectorMap<_Graph, _Item, signed long> Map;
  };

  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, unsigned long> {
    typedef VectorMap<_Graph, _Item, unsigned long> Map;
  };


#if defined LEMON_HAVE_LONG_LONG

  // long long
  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, signed long long> {
    typedef VectorMap<_Graph, _Item, signed long long> Map;
  };

  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, unsigned long long> {
    typedef VectorMap<_Graph, _Item, unsigned long long> Map;
  };

#endif


  // float
  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, float> {
    typedef VectorMap<_Graph, _Item, float> Map;
  };


  // double
  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, double> {
    typedef VectorMap<_Graph, _Item,  double> Map;
  };


  // long double
  template <typename _Graph, typename _Item>
  struct DefaultMapSelector<_Graph, _Item, long double> {
    typedef VectorMap<_Graph, _Item, long double> Map;
  };


  // pointer
  template <typename _Graph, typename _Item, typename _Ptr>
  struct DefaultMapSelector<_Graph, _Item, _Ptr*> {
    typedef VectorMap<_Graph, _Item, _Ptr*> Map;
  };

// #else

//   template <typename _Graph, typename _Item, typename _Value>
//   struct DefaultMapSelector {
//     typedef DebugMap<_Graph, _Item, _Value> Map;
//   };

// #endif

  // DefaultMap class
  template <typename _Graph, typename _Item, typename _Value>
  class DefaultMap
    : public DefaultMapSelector<_Graph, _Item, _Value>::Map {
    typedef typename DefaultMapSelector<_Graph, _Item, _Value>::Map Parent;

  public:
    typedef DefaultMap<_Graph, _Item, _Value> Map;

    typedef typename Parent::GraphType GraphType;
    typedef typename Parent::Value Value;

    explicit DefaultMap(const GraphType& graph) : Parent(graph) {}
    DefaultMap(const GraphType& graph, const Value& value)
      : Parent(graph, value) {}

    DefaultMap& operator=(const DefaultMap& cmap) {
      return operator=<DefaultMap>(cmap);
    }

    template <typename CMap>
    DefaultMap& operator=(const CMap& cmap) {
      Parent::operator=(cmap);
      return *this;
    }

  };

}

#endif
