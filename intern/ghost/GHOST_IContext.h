/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_IContext interface class.
 */

#ifndef __GHOST_IContext_H__
#define __GHOST_IContext_H__

#include "GHOST_Types.h"
#include "STR_String.h"

/**
 * Interface for GHOST context.
 *
 * You can create a offscreen context (windowless) with the system's
 * GHOST_ISystem::createOffscreenContext method.
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
   * \return  A boolean success indicator.
   */
  virtual GHOST_TSuccess activateDrawingContext() = 0;

  /**
   * Release the drawing context of the calling thread.
   * \return  A boolean success indicator.
   */
  virtual GHOST_TSuccess releaseDrawingContext() = 0;

  virtual unsigned int getDefaultFramebuffer() = 0;

  virtual GHOST_TSuccess swapBuffers() = 0;

  /**
   * Returns if the window is rendered upside down compared to OpenGL.
   */
  virtual bool isUpsideDown() const = 0;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_IContext")
#endif
};

#endif  // __GHOST_IContext_H__
