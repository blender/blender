/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Definition of GHOST_ContextNone class.
 */

#include "GHOST_ContextNone.hh"

GHOST_TSuccess GHOST_ContextNone::swapBufferRelease()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::activateDrawingContext()
{
  active_context_ = this;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::releaseDrawingContext()
{
  active_context_ = nullptr;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::updateDrawingContext()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::initializeDrawingContext()
{
  active_context_ = this;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::releaseNativeHandles()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::setSwapInterval(int interval)
{
  swap_interval_ = interval;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::getSwapInterval(int &interval_out)
{
  interval_out = swap_interval_;
  return GHOST_kSuccess;
}
