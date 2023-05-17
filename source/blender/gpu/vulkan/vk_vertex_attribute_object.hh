/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

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

  void bind(VKContext &context);

  // Copy assignment operator.
  VKVertexAttributeObject &operator=(const VKVertexAttributeObject &other);

  void update_bindings(const VKContext &context, VKBatch &batch);
  void update_bindings(VKImmediate &immediate);

 private:
  void update_bindings(const GPUVertFormat &vertex_format,
                       VKVertexBuffer *vertex_buffer,
                       VKBufferWithOffset *immediate_vertex_buffer,
                       const int64_t vertex_len,
                       const VKShaderInterface &interface,
                       AttributeMask &r_occupied_attributes,
                       const bool use_instancing);
};

}  // namespace blender::gpu
