/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#if defined(WIN32) || defined(__APPLE__)

#  ifdef WIN32
#    include <GL/gl.h>
#    include <windows.h>
#  else  // WIN32
// __APPLE__ is defined
#    include <AGL/gl.h>
#  endif  // WIN32
#else     // defined(WIN32) || defined(__APPLE__)
#  include <GL/gl.h>
#endif  // defined(WIN32) || defined(__APPLE__)
