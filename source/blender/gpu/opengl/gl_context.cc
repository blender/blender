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
 * Copyright 2020, Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#include "BLI_assert.h"
#include "BLI_utildefines.h"

#include "GPU_framebuffer.h"

#include "GHOST_C-api.h"

#include "gpu_context_private.h"

#include "gl_context.hh"

// TODO(fclem) this requires too much refactor for now.
// using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Constructor / Destructor
 * \{ */

GLContext::GLContext(void *ghost_window) : GPUContext()
{
  default_framebuffer_ = ghost_window ?
                             GHOST_GetDefaultOpenGLFramebuffer((GHOST_WindowHandle)ghost_window) :
                             0;
}

GLContext::~GLContext()
{
}

/** \} */
