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

///\file
///\brief Some basic non-inline functions and static global data.

#include<lemon/tolerance.h>
#include<lemon/core.h>
#include<lemon/time_measure.h>
namespace lemon {

  float Tolerance<float>::def_epsilon = static_cast<float>(1e-4);
  double Tolerance<double>::def_epsilon = 1e-10;
  long double Tolerance<long double>::def_epsilon = 1e-14;

#ifndef LEMON_ONLY_TEMPLATES
  const Invalid INVALID = Invalid();
#endif

  TimeStamp::Format TimeStamp::_format = TimeStamp::NORMAL;

} //namespace lemon
