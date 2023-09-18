/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_batch_private.hh"

namespace blender::gpu {
class VKVertexBuffer;
class VKIndexBuffer;

class VKBatch : public Batch {
 public:
  void draw(int vertex_first, int vertex_count, int instance_first, int instance_count) override;
  void draw_indirect(GPUStorageBuf *indirect_buf, intptr_t offset) override;
  void multi_draw_indirect(GPUStorageBuf *indirect_buf,
                           int count,
                           intptr_t offset,
                           intptr_t stride) override;

  VKVertexBuffer *vertex_buffer_get(int index);
  VKVertexBuffer *instance_buffer_get(int index);
  VKIndexBuffer *index_buffer_get();

 private:
  void draw_setup();
};

}  // namespace blender::gpu
