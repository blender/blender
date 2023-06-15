/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Declaration of GHOST_Context class.
 */

#pragma once

#include "GHOST_Context.hh"

class GHOST_ContextNone : public GHOST_Context {
 public:
  GHOST_ContextNone(bool stereoVisual) : GHOST_Context(stereoVisual), m_swapInterval(1) {}

  /**
   * Dummy function
   * \return Always succeeds
   */
  GHOST_TSuccess swapBuffers() override;

  /**
   * Dummy function
   * \return Always succeeds.
   */
  GHOST_TSuccess activateDrawingContext() override;

  /**
   * Dummy function
   * \return Always succeeds.
   */
  GHOST_TSuccess releaseDrawingContext() override;

  /**
   * Dummy function
   * \return Always succeeds.
   */
  GHOST_TSuccess updateDrawingContext() override;

  /**
   * Dummy function
   * \return Always succeeds.
   */
  GHOST_TSuccess initializeDrawingContext() override;

  /**
   * Dummy function
   * \return Always succeeds.
   */
  GHOST_TSuccess releaseNativeHandles() override;

  /**
   * Dummy function
   * \return Always succeeds.
   */
  GHOST_TSuccess setSwapInterval(int interval) override;

  /**
   * Dummy function
   * \param intervalOut: Gets whatever was set by #setSwapInterval.
   * \return Always succeeds.
   */
  GHOST_TSuccess getSwapInterval(int &intervalOut) override;

 private:
  int m_swapInterval;
};
