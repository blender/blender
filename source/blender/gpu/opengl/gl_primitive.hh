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
 * Encapsulation of Frame-buffer states (attached textures, viewport, scissors).
 */

#pragma once

#include "BLI_assert.h"

#include "GPU_primitive.h"

#include "glew-mx.h"

namespace blender::gpu {

static inline GLenum to_gl(GPUPrimType prim_type)
{
  BLI_assert(prim_type != GPU_PRIM_NONE);
  switch (prim_type) {
    default:
    case GPU_PRIM_POINTS:
      return GL_POINTS;
    case GPU_PRIM_LINES:
      return GL_LINES;
    case GPU_PRIM_LINE_STRIP:
      return GL_LINE_STRIP;
    case GPU_PRIM_LINE_LOOP:
      return GL_LINE_LOOP;
    case GPU_PRIM_TRIS:
      return GL_TRIANGLES;
    case GPU_PRIM_TRI_STRIP:
      return GL_TRIANGLE_STRIP;
    case GPU_PRIM_TRI_FAN:
      return GL_TRIANGLE_FAN;

    case GPU_PRIM_LINES_ADJ:
      return GL_LINES_ADJACENCY;
    case GPU_PRIM_LINE_STRIP_ADJ:
      return GL_LINE_STRIP_ADJACENCY;
    case GPU_PRIM_TRIS_ADJ:
      return GL_TRIANGLES_ADJACENCY;
  };
}

}  // namespace blender::gpu
