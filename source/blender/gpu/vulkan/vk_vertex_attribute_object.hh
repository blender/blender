/* SPDX-FileCopyrightText: 2023 Blender Authors All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "render_graph/vk_render_graph.hh"
#include "vk_buffer.hh"
#include "vk_common.hh"

#include "BLI_vector.hh"

#pragma once

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
  bool is_valid = false;
  VkPipelineVertexInputStateCreateInfo info = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL};

  Vector<VkVertexInputBindingDescription> bindings;
  Vector<VkVertexInputAttributeDescription> attributes;
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

  void debug_print() const;

 private:
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
