/* SPDX-FileCopyrightText: 2023 Blender Authors All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "render_graph/vk_render_graph.hh"
#include "vk_buffer.hh"
#include "vk_vertex_input_description.hh"

namespace blender::gpu {

class VKVertexBuffer;
class VKContext;
class VKBatch;
class VKShaderInterface;
class VKImmediate;

using AttributeMask = uint16_t;

/* TODO: VKVertexAttributeObject should not contain any reference to VBO's. This should make the
 * API be compatible with both #VKBatch and #VKImmediate. */
/* TODO: In steam of storing the bindings/attributes we should add a data structure that can store
 * them. Building the bindings/attributes should be done inside #VKPipelinePool. */
class VKVertexAttributeObject {
 public:
  VKVertexInputDescription vertex_input;

  /* Used for batches. */
  Vector<VKVertexBuffer *> vbos;
  /* Used for immediate mode. */
  Vector<VKBufferWithOffset> buffers;

  VKVertexAttributeObject();
  void clear();

  void bind(render_graph::VKVertexBufferBindings &r_vertex_buffer_bindings) const;

  /** Copy assignment operator. */
  VKVertexAttributeObject &operator=(const VKVertexAttributeObject &other);

  void update_bindings(const VKContext &context, VKBatch &batch);
  void update_bindings(VKImmediate &immediate);

  /**
   * \brief Returns the VKVertexInputDescriptionPool key for the current vertex input
   * description.
   *
   * The key is cached internally and only recomputed when the description actually changes (i.e.,
   * after clear() is called by update_bindings()).
   */
  VKVertexInputDescriptionPool::Key ensure_vertex_input_key(VKVertexInputDescriptionPool &pool);

  void debug_print() const;

 private:
  /**
   * \brief Cached key from VKVertexInputDescriptionPool, to avoid repeated hash/lookup when
   * bindings haven't changed.
   *
   * Invalidated in clear().
   *
   * Note currently it only saves a single recalc of the key, but when VKVertexAttributeObject
   * caching is added the savings will be more.
   */
  VKVertexInputDescriptionPool::Key cached_vertex_input_key_ =
      VKVertexInputDescriptionPool::invalid_key;

  /** Update unused bindings with a dummy binding. */
  void fill_unused_bindings(const VKShaderInterface &interface,
                            const AttributeMask occupied_attributes);
  void update_bindings(const GPUVertFormat &vertex_format,
                       VKVertexBuffer *vertex_buffer,
                       VKBufferWithOffset *immediate_vertex_buffer,
                       const int64_t vertex_len,
                       const VKShaderInterface &interface,
                       AttributeMask &r_occupied_attributes);
};

}  // namespace blender::gpu
