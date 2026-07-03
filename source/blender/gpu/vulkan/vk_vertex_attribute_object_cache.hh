/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_shader_interface.hh"
#include "vk_vertex_attribute_object.hh"

namespace blender::gpu {

class VKContext;
class VKBatch;

/**
 * \brief Cache of VKVertexAttributeObjects keyed by VKShaderInterface.
 *
 * When a batch is drawn multiple times with the same shader, the vertex attribute object
 * (bindings, attributes, buffer references) is reused, avoiding the expensive
 * `update_bindings()` step.
 *
 * The cache uses a fixed-size ring buffer. When full, the oldest entry is evicted.
 * Entries are invalidated via `clear()`, typically when `GPU_BATCH_DIRTY` is set.
 */
class VKVertexAttributeObjectCache {
 public:
  static constexpr int CACHE_LEN = 16;

  /**
   * Get or create a cached VKVertexAttributeObject.
   *
   * On cache hit the cached entry is returned and `update_bindings()` is skipped.
   * On cache miss `update_bindings()` is called and the vertex_input_key is populated.
   */
  const VKVertexAttributeObject &get_or_create(VKContext &context,
                                               VKBatch &batch,
                                               VKShaderInterface::Key shader_key,
                                               VKVertexInputDescriptionPool &pool);

  /** Clear all cache entries. */
  void clear();

 private:
  VKShaderInterface::Key keys_[CACHE_LEN];
  VKVertexAttributeObject values_[CACHE_LEN];
  int write_index_ = 0;
};

}  // namespace blender::gpu
