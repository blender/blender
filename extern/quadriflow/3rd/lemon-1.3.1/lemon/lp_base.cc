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
///\brief The implementation of the LP solver interface.

#include <lemon/lp_base.h>
namespace lemon {

  const LpBase::Value LpBase::INF =
    std::numeric_limits<LpBase::Value>::infinity();
  const LpBase::Value LpBase::NaN =
    std::numeric_limits<LpBase::Value>::quiet_NaN();

} //namespace lemon
