/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_IContext interface class.
 */

#pragma once

#include "GHOST_Types.h"

/**
 * Interface for GHOST context.
 *
 * You can create a off-screen context (windowless) with the system's
 * #GHOST_ISystem::createOffscreenContext method.
 * \see GHOST_ISystem#createOffscreenContext
 */
class GHOST_IContext {
 public:
  /**
   * Destructor.
   */
  virtual ~GHOST_IContext()
  {
  }

  /**
   * Activates the drawing context.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess activateDrawingContext() = 0;

  /**
   * Release the drawing context of the calling thread.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess releaseDrawingContext() = 0;

  virtual unsigned int getDefaultFramebuffer() = 0;

  virtual GHOST_TSuccess swapBuffers() = 0;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_IContext")
#endif
};
