/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Encapsulation of Frame-buffer states (attached textures, viewport, scissors).
 */

#pragma once

#include "BLI_assert.h"

#include "GPU_primitive.h"

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
