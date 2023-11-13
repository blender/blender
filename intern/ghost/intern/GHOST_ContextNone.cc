/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Definition of GHOST_ContextNone class.
 */

#include "GHOST_ContextNone.hh"

GHOST_TSuccess GHOST_ContextNone::swapBuffers()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::activateDrawingContext()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::releaseDrawingContext()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::updateDrawingContext()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::initializeDrawingContext()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::releaseNativeHandles()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::setSwapInterval(int interval)
{
  m_swapInterval = interval;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextNone::getSwapInterval(int &intervalOut)
{
  intervalOut = m_swapInterval;
  return GHOST_kSuccess;
}
