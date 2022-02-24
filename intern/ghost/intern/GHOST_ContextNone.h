/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup GHOST
 *
 * Declaration of GHOST_Context class.
 */

#pragma once

#include "GHOST_Context.h"

class GHOST_ContextNone : public GHOST_Context {
 public:
  GHOST_ContextNone(bool stereoVisual) : GHOST_Context(stereoVisual), m_swapInterval(1)
  {
  }

  /**
   * Dummy function
   * \return Always succeeds
   */
  GHOST_TSuccess swapBuffers();

  /**
   * Dummy function
   * \return Always succeeds.
   */
  GHOST_TSuccess activateDrawingContext();

  /**
   * Dummy function
   * \return Always succeeds.
   */
  GHOST_TSuccess releaseDrawingContext();

  /**
   * Dummy function
   * \return Always succeeds.
   */
  GHOST_TSuccess updateDrawingContext();

  /**
   * Dummy function
   * \return Always succeeds.
   */
  GHOST_TSuccess initializeDrawingContext();

  /**
   * Dummy function
   * \return Always succeeds.
   */
  GHOST_TSuccess releaseNativeHandles();

  /**
   * Dummy function
   * \return Always succeeds.
   */
  GHOST_TSuccess setSwapInterval(int interval);

  /**
   * Dummy function
   * \param intervalOut: Gets whatever was set by #setSwapInterval.
   * \return Always succeeds.
   */
  GHOST_TSuccess getSwapInterval(int &intervalOut);

 private:
  int m_swapInterval;
};
