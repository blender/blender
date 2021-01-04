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
  Batch(){};
  virtual ~Batch(){};

  virtual void draw(int v_first, int v_count, int i_first, int i_count) = 0;

  /* Convenience casts. */
  IndexBuf *elem_(void) const
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
