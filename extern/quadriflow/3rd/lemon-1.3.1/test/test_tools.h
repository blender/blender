/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2010
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

#ifndef LEMON_TEST_TEST_TOOLS_H
#define LEMON_TEST_TEST_TOOLS_H

///\ingroup misc
///\file
///\brief Some utilities to write test programs.

#include <iostream>
#include <stdlib.h>

///If \c rc is fail, writes an error message and exits.

///If \c rc is fail, writes an error message and exits.
///The error message contains the file name and the line number of the
///source code in a standard from, which makes it possible to go there
///using good source browsers like e.g. \c emacs.
///
///For example
///\code check(0==1,"This is obviously false.");\endcode will
///print something like this (and then exits).
///\verbatim file_name.cc:123: error: This is obviously false. \endverbatim
#define check(rc, msg)                                                  \
  {                                                                     \
    if(!(rc)) {                                                         \
      std::cerr << __FILE__ ":" << __LINE__ << ": error: "              \
                << msg << std::endl;                                    \
      abort();                                                          \
    } else { }                                                          \
  }                                                                     \


#endif
