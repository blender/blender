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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Mimics old style opengl immediate mode drawing.
 */

#pragma once

#include "GPU_batch.h"
#include "GPU_primitive.h"
#include "GPU_shader.h"
#include "GPU_vertex_format.h"

namespace blender::gpu {

class Immediate {
 public:
  /** Pointer to the mapped buffer data for the current vertex. */
  uchar *vertex_data = NULL;
  /** Current vertex index. */
  uint vertex_idx = 0;
  /** Length of the buffer in vertices. */
  uint vertex_len = 0;
  /** Which attributes of current vertex have not been given values? */
  uint16_t unassigned_attr_bits = 0;
  /** Attributes that needs to be set. One bit per attribute. */
  uint16_t enabled_attr_bits = 0;

  /** Current draw call specification. */
  GPUPrimType prim_type = GPU_PRIM_NONE;
  GPUVertFormat vertex_format = {};
  GPUShader *shader = NULL;
  /** Enforce strict vertex count (disabled when using #immBeginAtMost). */
  bool strict_vertex_len = true;

  /** Batch in construction when using #immBeginBatch. */
  GPUBatch *batch = NULL;

  /** Wide Line workaround. */

  /** Previously bound shader to restore after drawing. */
  GPUShader *prev_shader = NULL;
  /** Builtin shader index. Used to test if the workaround can be done. */
  eGPUBuiltinShader builtin_shader_bound = GPU_SHADER_TEXT;
  /** Uniform color: Kept here to update the wide-line shader just before #immBegin. */
  float uniform_color[4];

 public:
  Immediate(){};
  virtual ~Immediate(){};

  virtual uchar *begin(void) = 0;
  virtual void end(void) = 0;
};

}  // namespace blender::gpu

void immActivate(void);
void immDeactivate(void);
