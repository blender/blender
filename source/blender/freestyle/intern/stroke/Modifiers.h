/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief modifiers...
 */

#include "TimeStamp.h"

#include "MEM_guardedalloc.h"

namespace Freestyle {

/* ----------------------------------------- *
 *                                           *
 *              modifiers                    *
 *                                           *
 * ----------------------------------------- */

/** Base class for modifiers.
 *  Modifiers are used in the Operators in order to "mark" the processed Interface1D.
 */
template<class Edge> struct EdgeModifier : public unary_function<Edge, void> {
  /** Default construction */
  EdgeModifier() : unary_function<Edge, void>() {}

  /** the () operator */
  virtual void operator()(Edge &iEdge) {}

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:EdgeModifier")
};

/** Modifier that sets the time stamp of an Interface1D to the time stamp of the system. */
template<class Edge> struct TimestampModifier : public EdgeModifier<Edge> {
  /** Default constructor */
  TimestampModifier() : EdgeModifier<Edge>() {}

  /** The () operator. */
  virtual void operator()(Edge &iEdge)
  {
    TimeStamp *timestamp = TimeStamp::instance();
    iEdge.setTimeStamp(timestamp->getTimeStamp());
  }
};

} /* namespace Freestyle */
