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

///\file
///\brief Color constants

#include<lemon/color.h>

namespace lemon {

  const Color WHITE(1,1,1);

  const Color BLACK(0,0,0);
  const Color RED(1,0,0);
  const Color GREEN(0,1,0);
  const Color BLUE(0,0,1);
  const Color YELLOW(1,1,0);
  const Color MAGENTA(1,0,1);
  const Color CYAN(0,1,1);

  const Color GREY(0,0,0);
  const Color DARK_RED(.5,0,0);
  const Color DARK_GREEN(0,.5,0);
  const Color DARK_BLUE(0,0,.5);
  const Color DARK_YELLOW(.5,.5,0);
  const Color DARK_MAGENTA(.5,0,.5);
  const Color DARK_CYAN(0,.5,.5);

} //namespace lemon
