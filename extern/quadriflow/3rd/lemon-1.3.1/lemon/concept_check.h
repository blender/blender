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

// The contents of this file was inspired by the concept checking
// utility of the BOOST library (http://www.boost.org).

///\file
///\brief Basic utilities for concept checking.
///

#ifndef LEMON_CONCEPT_CHECK_H
#define LEMON_CONCEPT_CHECK_H

namespace lemon {

  /*
    "inline" is used for ignore_unused_variable_warning()
    and function_requires() to make sure there is no
    overtarget with g++.
  */

  template <class T> inline void ignore_unused_variable_warning(const T&) { }
  template <class T1, class T2>
  inline void ignore_unused_variable_warning(const T1&, const T2&) { }
  template <class T1, class T2, class T3>
  inline void ignore_unused_variable_warning(const T1&, const T2&,
                                             const T3&) { }
  template <class T1, class T2, class T3, class T4>
  inline void ignore_unused_variable_warning(const T1&, const T2&,
                                             const T3&, const T4&) { }
  template <class T1, class T2, class T3, class T4, class T5>
  inline void ignore_unused_variable_warning(const T1&, const T2&,
                                             const T3&, const T4&,
                                             const T5&) { }
  template <class T1, class T2, class T3, class T4, class T5, class T6>
  inline void ignore_unused_variable_warning(const T1&, const T2&,
                                             const T3&, const T4&,
                                             const T5&, const T6&) { }

  ///\e
  template <class Concept>
  inline void function_requires()
  {
#if !defined(NDEBUG)
    void (Concept::*x)() = & Concept::constraints;
    ::lemon::ignore_unused_variable_warning(x);
#endif
  }

  ///\e
  template <typename Concept, typename Type>
  inline void checkConcept() {
#if !defined(NDEBUG)
    typedef typename Concept::template Constraints<Type> ConceptCheck;
    void (ConceptCheck::*x)() = & ConceptCheck::constraints;
    ::lemon::ignore_unused_variable_warning(x);
#endif
  }

} // namespace lemon

#endif // LEMON_CONCEPT_CHECK_H
