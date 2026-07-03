/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_index_buffer.hh"
#include "vk_storage_buffer.hh"
#include "vk_vertex_attribute_object_cache.hh"
#include "vk_vertex_buffer.hh"

#include "GPU_batch.hh"

namespace blender::gpu {
class VKBatch : public Batch {
 public:
  void draw(int vertex_first, int vertex_count, int instance_first, int instance_count) override;
  void draw_indirect(StorageBuf *indirect_buf, intptr_t offset) override;
  void multi_draw_indirect(StorageBuf *indirect_buf,
                           int count,
                           intptr_t offset,
                           intptr_t stride) override;
  void multi_draw_indirect(const VKStorageBuffer &indirect_buf,
                           int count,
                           intptr_t offset,
                           intptr_t stride);

  /** \brief Ensure that the index and vertex buffers are uploaded.  */
  void ensure_data_uploaded() const;

  VKVertexBuffer *vertex_buffer_get(int index) const;
  VKIndexBuffer *index_buffer_get() const;

 private:
  /**
   * \brief Get or create the vertex attribute object for the current shader.
   *
   * Checks GPU_BATCH_DIRTY, clears the cache if needed, and returns a cached or freshly
   * created VKVertexAttributeObject.
   */
  const VKVertexAttributeObject &get_vertex_attribute_object(VKContext &context);

  VKVertexAttributeObjectCache vao_cache_;
};

inline VKBatch *unwrap(Batch *batch)
{
  return static_cast<VKBatch *>(batch);
}

inline VKVertexBuffer *VKBatch::vertex_buffer_get(int index) const
{
  return unwrap(verts_(index));
}

inline VKIndexBuffer *VKBatch::index_buffer_get() const
{
  return unwrap(unwrap(elem_()));
}

}  // namespace blender::gpu
