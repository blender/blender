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

#ifndef LEMON_COUNTER_H
#define LEMON_COUNTER_H

#include <string>
#include <iostream>

///\ingroup timecount
///\file
///\brief Tools for counting steps and events

namespace lemon
{

  template<class P> class _NoSubCounter;

  template<class P>
  class _SubCounter
  {
    P &_parent;
    std::string _title;
    std::ostream &_os;
    int count;
  public:

    typedef _SubCounter<_SubCounter<P> > SubCounter;
    typedef _NoSubCounter<_SubCounter<P> > NoSubCounter;

    _SubCounter(P &parent)
      : _parent(parent), _title(), _os(std::cerr), count(0) {}
    _SubCounter(P &parent,std::string title,std::ostream &os=std::cerr)
      : _parent(parent), _title(title), _os(os), count(0) {}
    _SubCounter(P &parent,const char *title,std::ostream &os=std::cerr)
      : _parent(parent), _title(title), _os(os), count(0) {}
    ~_SubCounter() {
      _os << _title << count <<std::endl;
      _parent+=count;
    }
    _SubCounter &operator++() { count++; return *this;}
    int operator++(int) { return count++; }
    _SubCounter &operator--() { count--; return *this;}
    int operator--(int) { return count--; }
    _SubCounter &operator+=(int c) { count+=c; return *this;}
    _SubCounter &operator-=(int c) { count-=c; return *this;}
    operator int() {return count;}
  };

  template<class P>
  class _NoSubCounter
  {
    P &_parent;
  public:
    typedef _NoSubCounter<_NoSubCounter<P> > SubCounter;
    typedef _NoSubCounter<_NoSubCounter<P> > NoSubCounter;

    _NoSubCounter(P &parent) :_parent(parent) {}
    _NoSubCounter(P &parent,std::string,std::ostream &)
      :_parent(parent) {}
    _NoSubCounter(P &parent,std::string)
      :_parent(parent) {}
    _NoSubCounter(P &parent,const char *,std::ostream &)
      :_parent(parent) {}
    _NoSubCounter(P &parent,const char *)
      :_parent(parent) {}
    ~_NoSubCounter() {}
    _NoSubCounter &operator++() { ++_parent; return *this;}
    int operator++(int) { _parent++; return 0;}
    _NoSubCounter &operator--() { --_parent; return *this;}
    int operator--(int) { _parent--; return 0;}
    _NoSubCounter &operator+=(int c) { _parent+=c; return *this;}
    _NoSubCounter &operator-=(int c) { _parent-=c; return *this;}
    operator int() {return 0;}
  };


  /// \addtogroup timecount
  /// @{

  /// A counter class

  /// This class makes it easier to count certain events (e.g. for debug
  /// reasons).
  /// You can increment or decrement the counter using \c operator++,
  /// \c operator--, \c operator+= and \c operator-=. You can also
  /// define subcounters for the different phases of the algorithm or
  /// for different types of operations.
  /// A report containing the given title and the value of the counter
  /// is automatically printed on destruction.
  ///
  /// The following example shows the usage of counters and subcounters.
  /// \code
  /// // Bubble sort
  /// std::vector<T> v;
  /// ...
  /// Counter op("Operations: ");
  /// Counter::SubCounter as(op, "Assignments: ");
  /// Counter::SubCounter co(op, "Comparisons: ");
  /// for (int i = v.size()-1; i > 0; --i) {
  ///   for (int j = 0; j < i; ++j) {
  ///     if (v[j] > v[j+1]) {
  ///       T tmp = v[j];
  ///       v[j] = v[j+1];
  ///       v[j+1] = tmp;
  ///       as += 3;          // three assignments
  ///     }
  ///     ++co;               // one comparison
  ///   }
  /// }
  /// \endcode
  ///
  /// This code prints out something like that:
  /// \code
  /// Comparisons: 45
  /// Assignments: 57
  /// Operations: 102
  /// \endcode
  ///
  /// \sa NoCounter
  class Counter
  {
    std::string _title;
    std::ostream &_os;
    int count;
  public:

    /// SubCounter class

    /// This class can be used to setup subcounters for a \ref Counter
    /// to have finer reports. A subcounter provides exactly the same
    /// operations as the main \ref Counter, but it also increments and
    /// decrements the value of its parent.
    /// Subcounters can also have subcounters.
    ///
    /// The parent counter must be given as the first parameter of the
    /// constructor. Apart from that a title and an \c ostream object
    /// can also be given just like for the main \ref Counter.
    ///
    /// A report containing the given title and the value of the
    /// subcounter is automatically printed on destruction. If you
    /// would like to turn off this report, use \ref NoSubCounter
    /// instead.
    ///
    /// \sa NoSubCounter
    typedef _SubCounter<Counter> SubCounter;

    /// SubCounter class without printing report on destruction

    /// This class can be used to setup subcounters for a \ref Counter.
    /// It is the same as \ref SubCounter but it does not print report
    /// on destruction. (It modifies the value of its parent, so 'No'
    /// only means 'do not print'.)
    ///
    /// Replacing \ref SubCounter "SubCounter"s with \ref NoSubCounter
    /// "NoSubCounter"s makes it possible to turn off reporting
    /// subcounter values without actually removing the definitions
    /// and the increment or decrement operators.
    ///
    /// \sa SubCounter
    typedef _NoSubCounter<Counter> NoSubCounter;

    /// Constructor.
    Counter() : _title(), _os(std::cerr), count(0) {}
    /// Constructor.
    Counter(std::string title,std::ostream &os=std::cerr)
      : _title(title), _os(os), count(0) {}
    /// Constructor.
    Counter(const char *title,std::ostream &os=std::cerr)
      : _title(title), _os(os), count(0) {}
    /// Destructor. Prints the given title and the value of the counter.
    ~Counter() {
      _os << _title << count <<std::endl;
    }
    ///\e
    Counter &operator++() { count++; return *this;}
    ///\e
    int operator++(int) { return count++;}
    ///\e
    Counter &operator--() { count--; return *this;}
    ///\e
    int operator--(int) { return count--;}
    ///\e
    Counter &operator+=(int c) { count+=c; return *this;}
    ///\e
    Counter &operator-=(int c) { count-=c; return *this;}
    /// Resets the counter to the given value.

    /// Resets the counter to the given value.
    /// \note This function does not reset the values of
    /// \ref SubCounter "SubCounter"s but it resets \ref NoSubCounter
    /// "NoSubCounter"s along with the main counter.
    void reset(int c=0) {count=c;}
    /// Returns the value of the counter.
    operator int() {return count;}
  };

  /// 'Do nothing' version of Counter.

  /// This class can be used in the same way as \ref Counter, but it
  /// does not count at all and does not print report on destruction.
  ///
  /// Replacing a \ref Counter with a \ref NoCounter makes it possible
  /// to turn off all counting and reporting (SubCounters should also
  /// be replaced with NoSubCounters), so it does not affect the
  /// efficiency of the program at all.
  ///
  /// \sa Counter
  class NoCounter
  {
  public:
    typedef _NoSubCounter<NoCounter> SubCounter;
    typedef _NoSubCounter<NoCounter> NoSubCounter;

    NoCounter() {}
    NoCounter(std::string,std::ostream &) {}
    NoCounter(const char *,std::ostream &) {}
    NoCounter(std::string) {}
    NoCounter(const char *) {}
    NoCounter &operator++() { return *this; }
    int operator++(int) { return 0; }
    NoCounter &operator--() { return *this; }
    int operator--(int) { return 0; }
    NoCounter &operator+=(int) { return *this;}
    NoCounter &operator-=(int) { return *this;}
    void reset(int) {}
    void reset() {}
    operator int() {return 0;}
  };

  ///@}
}

#endif
