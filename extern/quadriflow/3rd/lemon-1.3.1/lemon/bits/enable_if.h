/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2009
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

// This file contains a modified version of the enable_if library from BOOST.
// See the appropriate copyright notice below.

// Boost enable_if library

// Copyright 2003 (c) The Trustees of Indiana University.

// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//    Authors: Jaakko Jarvi (jajarvi at osl.iu.edu)
//             Jeremiah Willcock (jewillco at osl.iu.edu)
//             Andrew Lumsdaine (lums at osl.iu.edu)


#ifndef LEMON_BITS_ENABLE_IF_H
#define LEMON_BITS_ENABLE_IF_H

//\file
//\brief Miscellaneous basic utilities

namespace lemon
{

  // Basic type for defining "tags". A "YES" condition for \c enable_if.

  // Basic type for defining "tags". A "YES" condition for \c enable_if.
  //
  //\sa False
  struct True {
    //\e
    static const bool value = true;
  };

  // Basic type for defining "tags". A "NO" condition for \c enable_if.

  // Basic type for defining "tags". A "NO" condition for \c enable_if.
  //
  //\sa True
  struct False {
    //\e
    static const bool value = false;
  };



  template <typename T>
  struct Wrap {
    const T &value;
    Wrap(const T &t) : value(t) {}
  };

  /**************** dummy class to avoid ambiguity ****************/

  template<int T> struct dummy { dummy(int) {} };

  /**************** enable_if from BOOST ****************/

  template <typename Type, typename T = void>
  struct exists {
    typedef T type;
  };


  template <bool B, class T = void>
  struct enable_if_c {
    typedef T type;
  };

  template <class T>
  struct enable_if_c<false, T> {};

  template <class Cond, class T = void>
  struct enable_if : public enable_if_c<Cond::value, T> {};

  template <bool B, class T>
  struct lazy_enable_if_c {
    typedef typename T::type type;
  };

  template <class T>
  struct lazy_enable_if_c<false, T> {};

  template <class Cond, class T>
  struct lazy_enable_if : public lazy_enable_if_c<Cond::value, T> {};


  template <bool B, class T = void>
  struct disable_if_c {
    typedef T type;
  };

  template <class T>
  struct disable_if_c<true, T> {};

  template <class Cond, class T = void>
  struct disable_if : public disable_if_c<Cond::value, T> {};

  template <bool B, class T>
  struct lazy_disable_if_c {
    typedef typename T::type type;
  };

  template <class T>
  struct lazy_disable_if_c<true, T> {};

  template <class Cond, class T>
  struct lazy_disable_if : public lazy_disable_if_c<Cond::value, T> {};

} // namespace lemon

#endif
