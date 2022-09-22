/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU geometry batch
 * Contains VAOs + VBOs + Shader representing a drawable entity.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_batch_private.hh"

namespace blender {
namespace gpu {


/* Pass-through MTLBatch. TODO(Metal): Implement. */
class MTLBatch : public Batch {
 public:
  void draw(int v_first, int v_count, int i_first, int i_count) override {

  }

  void draw_indirect(GPUStorageBuf *indirect_buf, intptr_t offset) override {

  }
  
  void multi_draw_indirect(GPUStorageBuf *indirect_buf,
                           int count,
                           intptr_t offset,
                           intptr_t stride) override {
                               
                           }
  MEM_CXX_CLASS_ALLOC_FUNCS("MTLBatch");
};

}  // namespace gpu
}  // namespace blender
