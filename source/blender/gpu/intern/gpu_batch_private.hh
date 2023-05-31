/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU geometry batch
 * Contains VAOs + VBOs + Shader representing a drawable entity.
 */

#pragma once

#include "GPU_batch.h"
#include "GPU_context.h"

#include "gpu_index_buffer_private.hh"
#include "gpu_vertex_buffer_private.hh"

namespace blender {
namespace gpu {

/**
 * Base class which is then specialized for each implementation (GL, VK, ...).
 *
 * \note Extends #GPUBatch as we still needs to expose some of the internals to the outside C code.
 */
class Batch : public GPUBatch {
 public:
  virtual ~Batch() = default;

  virtual void draw(int v_first, int v_count, int i_first, int i_count) = 0;
  virtual void draw_indirect(GPUStorageBuf *indirect_buf, intptr_t offset) = 0;
  virtual void multi_draw_indirect(GPUStorageBuf *indirect_buf,
                                   int count,
                                   intptr_t offset,
                                   intptr_t stride) = 0;

  /* Convenience casts. */
  IndexBuf *elem_() const
  {
    return unwrap(elem);
  }
  VertBuf *verts_(const int index) const
  {
    return unwrap(verts[index]);
  }
  VertBuf *inst_(const int index) const
  {
    return unwrap(inst[index]);
  }
};

}  // namespace gpu
}  // namespace blender
