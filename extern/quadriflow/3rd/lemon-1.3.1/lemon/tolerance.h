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

#ifndef LEMON_TOLERANCE_H
#define LEMON_TOLERANCE_H

///\ingroup misc
///\file
///\brief A basic tool to handle the anomalies of calculation with
///floating point numbers.
///

namespace lemon {

  /// \addtogroup misc
  /// @{

  ///\brief A class to provide a basic way to
  ///handle the comparison of numbers that are obtained
  ///as a result of a probably inexact computation.
  ///
  ///\ref Tolerance is a class to provide a basic way to
  ///handle the comparison of numbers that are obtained
  ///as a result of a probably inexact computation.
  ///
  ///The general implementation is suitable only if the data type is exact,
  ///like the integer types, otherwise a specialized version must be
  ///implemented. These specialized classes like
  ///Tolerance<double> may offer additional tuning parameters.
  ///
  ///\sa Tolerance<float>
  ///\sa Tolerance<double>
  ///\sa Tolerance<long double>

  template<class T>
  class Tolerance
  {
  public:
    typedef T Value;

    ///\name Comparisons
    ///The concept is that these bool functions return \c true only if
    ///the related comparisons hold even if some numerical error appeared
    ///during the computations.

    ///@{

    ///Returns \c true if \c a is \e surely strictly less than \c b
    static bool less(Value a,Value b) {return a<b;}
    ///Returns \c true if \c a is \e surely different from \c b
    static bool different(Value a,Value b) {return a!=b;}
    ///Returns \c true if \c a is \e surely positive
    static bool positive(Value a) {return static_cast<Value>(0) < a;}
    ///Returns \c true if \c a is \e surely negative
    static bool negative(Value a) {return a < static_cast<Value>(0);}
    ///Returns \c true if \c a is \e surely non-zero
    static bool nonZero(Value a) {return a != static_cast<Value>(0);}

    ///@}

    ///Returns the zero value.
    static Value zero() {return static_cast<Value>(0);}

    //   static bool finite(Value a) {}
    //   static Value big() {}
    //   static Value negativeBig() {}
  };


  ///Float specialization of Tolerance.

  ///Float specialization of Tolerance.
  ///\sa Tolerance
  ///\relates Tolerance
  template<>
  class Tolerance<float>
  {
    static float def_epsilon;
    float _epsilon;
  public:
    ///\e
    typedef float Value;

    ///Constructor setting the epsilon tolerance to the default value.
    Tolerance() : _epsilon(def_epsilon) {}
    ///Constructor setting the epsilon tolerance to the given value.
    Tolerance(float e) : _epsilon(e) {}

    ///Returns the epsilon value.
    Value epsilon() const {return _epsilon;}
    ///Sets the epsilon value.
    void epsilon(Value e) {_epsilon=e;}

    ///Returns the default epsilon value.
    static Value defaultEpsilon() {return def_epsilon;}
    ///Sets the default epsilon value.
    static void defaultEpsilon(Value e) {def_epsilon=e;}

    ///\name Comparisons
    ///See \ref lemon::Tolerance "Tolerance" for more details.

    ///@{

    ///Returns \c true if \c a is \e surely strictly less than \c b
    bool less(Value a,Value b) const {return a+_epsilon<b;}
    ///Returns \c true if \c a is \e surely different from \c b
    bool different(Value a,Value b) const { return less(a,b)||less(b,a); }
    ///Returns \c true if \c a is \e surely positive
    bool positive(Value a) const { return _epsilon<a; }
    ///Returns \c true if \c a is \e surely negative
    bool negative(Value a) const { return -_epsilon>a; }
    ///Returns \c true if \c a is \e surely non-zero
    bool nonZero(Value a) const { return positive(a)||negative(a); }

    ///@}

    ///Returns zero
    static Value zero() {return 0;}
  };

  ///Double specialization of Tolerance.

  ///Double specialization of Tolerance.
  ///\sa Tolerance
  ///\relates Tolerance
  template<>
  class Tolerance<double>
  {
    static double def_epsilon;
    double _epsilon;
  public:
    ///\e
    typedef double Value;

    ///Constructor setting the epsilon tolerance to the default value.
    Tolerance() : _epsilon(def_epsilon) {}
    ///Constructor setting the epsilon tolerance to the given value.
    Tolerance(double e) : _epsilon(e) {}

    ///Returns the epsilon value.
    Value epsilon() const {return _epsilon;}
    ///Sets the epsilon value.
    void epsilon(Value e) {_epsilon=e;}

    ///Returns the default epsilon value.
    static Value defaultEpsilon() {return def_epsilon;}
    ///Sets the default epsilon value.
    static void defaultEpsilon(Value e) {def_epsilon=e;}

    ///\name Comparisons
    ///See \ref lemon::Tolerance "Tolerance" for more details.

    ///@{

    ///Returns \c true if \c a is \e surely strictly less than \c b
    bool less(Value a,Value b) const {return a+_epsilon<b;}
    ///Returns \c true if \c a is \e surely different from \c b
    bool different(Value a,Value b) const { return less(a,b)||less(b,a); }
    ///Returns \c true if \c a is \e surely positive
    bool positive(Value a) const { return _epsilon<a; }
    ///Returns \c true if \c a is \e surely negative
    bool negative(Value a) const { return -_epsilon>a; }
    ///Returns \c true if \c a is \e surely non-zero
    bool nonZero(Value a) const { return positive(a)||negative(a); }

    ///@}

    ///Returns zero
    static Value zero() {return 0;}
  };

  ///Long double specialization of Tolerance.

  ///Long double specialization of Tolerance.
  ///\sa Tolerance
  ///\relates Tolerance
  template<>
  class Tolerance<long double>
  {
    static long double def_epsilon;
    long double _epsilon;
  public:
    ///\e
    typedef long double Value;

    ///Constructor setting the epsilon tolerance to the default value.
    Tolerance() : _epsilon(def_epsilon) {}
    ///Constructor setting the epsilon tolerance to the given value.
    Tolerance(long double e) : _epsilon(e) {}

    ///Returns the epsilon value.
    Value epsilon() const {return _epsilon;}
    ///Sets the epsilon value.
    void epsilon(Value e) {_epsilon=e;}

    ///Returns the default epsilon value.
    static Value defaultEpsilon() {return def_epsilon;}
    ///Sets the default epsilon value.
    static void defaultEpsilon(Value e) {def_epsilon=e;}

    ///\name Comparisons
    ///See \ref lemon::Tolerance "Tolerance" for more details.

    ///@{

    ///Returns \c true if \c a is \e surely strictly less than \c b
    bool less(Value a,Value b) const {return a+_epsilon<b;}
    ///Returns \c true if \c a is \e surely different from \c b
    bool different(Value a,Value b) const { return less(a,b)||less(b,a); }
    ///Returns \c true if \c a is \e surely positive
    bool positive(Value a) const { return _epsilon<a; }
    ///Returns \c true if \c a is \e surely negative
    bool negative(Value a) const { return -_epsilon>a; }
    ///Returns \c true if \c a is \e surely non-zero
    bool nonZero(Value a) const { return positive(a)||negative(a); }

    ///@}

    ///Returns zero
    static Value zero() {return 0;}
  };

  /// @}

} //namespace lemon

#endif //LEMON_TOLERANCE_H
