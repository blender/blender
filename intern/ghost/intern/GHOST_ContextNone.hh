/* SPDX-FileCopyrightText: 2013 Blender Authors
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
  GHOST_ContextNone(const GHOST_ContextParams &context_params) : GHOST_Context(context_params) {}

  /** \copydoc #GHOST_IContext::swapBuffersAcquire */
  GHOST_TSuccess swapBufferAcquire() override
  {
    return GHOST_kSuccess;
  }

  /**
   * Dummy function
   * \return Always succeeds
   */
  GHOST_TSuccess swapBufferRelease() override;

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
   * \param interval_out: Gets whatever was set by #setSwapInterval.
   * \return Always succeeds.
   */
  GHOST_TSuccess getSwapInterval(int &interval_out) override;

 private:
  int swap_interval_ = 1;
};
