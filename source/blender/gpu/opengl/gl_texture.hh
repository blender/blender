
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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GPU Framebuffer
 * - this is a wrapper for an OpenGL framebuffer object (FBO). in practice
 *   multiple FBO's may be created.
 * - actual FBO creation & config is deferred until GPU_framebuffer_bind or
 *   GPU_framebuffer_check_valid to allow creation & config while another
 *   opengl context is bound (since FBOs are not shared between ogl contexts).
 */

#pragma once

#include "BLI_assert.h"

#include "glew-mx.h"

namespace blender {
namespace gpu {

static GLenum to_gl(eGPUDataFormat format)
{
  switch (format) {
    case GPU_DATA_FLOAT:
      return GL_FLOAT;
    case GPU_DATA_INT:
      return GL_INT;
    case GPU_DATA_UNSIGNED_INT:
      return GL_UNSIGNED_INT;
    case GPU_DATA_UNSIGNED_BYTE:
      return GL_UNSIGNED_BYTE;
    case GPU_DATA_UNSIGNED_INT_24_8:
      return GL_UNSIGNED_INT_24_8;
    case GPU_DATA_10_11_11_REV:
      return GL_UNSIGNED_INT_10F_11F_11F_REV;
    default:
      BLI_assert(!"Unhandled data format");
      return GL_FLOAT;
  }
}

/* Assume Unorm / Float target. Used with glReadPixels. */
static GLenum channel_len_to_gl(int channel_len)
{
  switch (channel_len) {
    case 1:
      return GL_RED;
    case 2:
      return GL_RG;
    case 3:
      return GL_RGB;
    case 4:
      return GL_RGBA;
    default:
      BLI_assert(!"Wrong number of texture channels");
      return GL_RED;
  }
}

}  // namespace gpu
}  // namespace blender
