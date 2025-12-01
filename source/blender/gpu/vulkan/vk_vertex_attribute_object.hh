/* SPDX-FileCopyrightText: 2023 Blender Authors All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "render_graph/vk_render_graph.hh"
#include "vk_buffer.hh"

#include "BLI_map.hh"
#include "BLI_mutex.hh"
#include "BLI_vector.hh"

#include "xxhash.h"

#include <mutex>

#pragma once

namespace blender::gpu {

class VKVertexBuffer;
class VKContext;
class VKBatch;
class VKShaderInterface;
class VKImmediate;

using AttributeMask = uint16_t;

struct VKVertexInputDescription {
  Vector<VkVertexInputBindingDescription> bindings;
  Vector<VkVertexInputAttributeDescription> attributes;

  VKVertexInputDescription() = default;

  VKVertexInputDescription(const gpu::shader::PipelineState &pipeline_state)
  {
    attributes.reserve(pipeline_state.vertex_inputs_.size());
    bindings.reserve(pipeline_state.vertex_inputs_.size());
    uint32_t binding = 0;
    for (const gpu::shader::PipelineState::AttributeBinding &attribute_binding :
         pipeline_state.vertex_inputs_)
    {
      const GPUVertAttr::Type attribute_type = {attribute_binding.type};
      attributes.append({attribute_binding.location,
                         binding,
                         to_vk_format(attribute_type.comp_type(),
                                      attribute_type.size(),
                                      attribute_type.fetch_mode()),
                         attribute_binding.offset});
      bindings.append(
          {attribute_binding.binding, attribute_binding.stride, VK_VERTEX_INPUT_RATE_VERTEX});
      binding++;
    }
  }

  void clear();

  bool operator==(const VKVertexInputDescription &other) const
  {
    return attributes.size() == other.attributes.size() &&
           bindings.size() == other.bindings.size() &&
           memcmp(attributes.data(),
                  other.attributes.data(),
                  attributes.size() * sizeof(VkVertexInputAttributeDescription)) == 0 &&
           memcmp(bindings.data(),
                  other.bindings.data(),
                  bindings.size() * sizeof(VkVertexInputBindingDescription)) == 0;
  }

  uint64_t hash() const
  {
    uint64_t hash = XXH3_64bits(attributes.data(),
                                attributes.size() * sizeof(VkVertexInputAttributeDescription));
    hash = hash * 33 ^
           XXH3_64bits(bindings.data(), bindings.size() * sizeof(VkVertexInputBindingDescription));
    return hash;
  }
};

class VKVertexInputDescriptionPool {
 public:
  using Key = int64_t;

 private:
  Mutex mutex_;

  Vector<std::unique_ptr<VKVertexInputDescription>> vertex_inputs_;
  Map<VKVertexInputDescription, Key> lookup_;

 public:
  Key get_or_insert(VKVertexInputDescription &description)
  {
    std::scoped_lock lock(mutex_);
    Key *result_ptr = lookup_.lookup_ptr(description);
    if (result_ptr != nullptr) {
      return *result_ptr;
    }

    Key result = vertex_inputs_.size();
    lookup_.add(description, result);
    vertex_inputs_.append(std::make_unique<VKVertexInputDescription>(description));
    return result;
  }

  const VKVertexInputDescription &get(Key key)
  {
    std::scoped_lock lock(mutex_);
    return *vertex_inputs_[key].get();
  }
};

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
