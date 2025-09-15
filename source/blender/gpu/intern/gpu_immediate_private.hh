/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Mimics old style opengl immediate mode drawing.
 */

#pragma once

#include <optional>

#include "GPU_batch.hh"
#include "GPU_primitive.hh"
#include "GPU_shader.hh"
#include "GPU_vertex_format.hh"

namespace blender::gpu {

class Immediate {
 public:
  /** Pointer to the mapped buffer data for the current vertex. */
  uchar *vertex_data = nullptr;
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
  gpu::Shader *shader = nullptr;
  /** Enforce strict vertex count (disabled when using #immBeginAtMost). */
  bool strict_vertex_len = true;

  /** Batch in construction when using #immBeginBatch. */
  Batch *batch = nullptr;

  /** Wide Line workaround. */

  /** Previously bound shader to restore after drawing. */
  std::optional<GPUBuiltinShader> prev_builtin_shader;
  /** Builtin shader index. Used to test if the line width workaround can be done. */
  std::optional<GPUBuiltinShader> builtin_shader_bound;
  /** Uniform color: Kept here to update the wide-line shader just before #immBegin. */
  float uniform_color[4];

  Immediate() = default;
  virtual ~Immediate() = default;

  virtual uchar *begin() = 0;
  virtual void end() = 0;

  /* To be called after polyline SSBO binding. */
  void polyline_draw_workaround(uint64_t offset);
};

}  // namespace blender::gpu

void immActivate();
void immDeactivate();
