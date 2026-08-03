/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_vertex_attribute_object_cache.hh"

#include "vk_batch.hh"
#include "vk_context.hh"

namespace blender::gpu {

const VKVertexAttributeObject &VKVertexAttributeObjectCache::get_or_create(
    VKContext &context,
    VKBatch &batch,
    VKShaderInterface::Key shader_key,
    VKVertexInputDescriptionPool &pool)
{
  for (int index : IndexRange(CACHE_LEN)) {
    if (keys_[index] == shader_key) {
      return values_[index];
    }
  }

  int slot = write_index_;
  write_index_ = (write_index_ + 1) % CACHE_LEN;

  VKVertexAttributeObject &vao = values_[slot];
  keys_[slot] = shader_key;
  vao.update_bindings(context, batch);
  vao.update_vertex_input_key(pool);
  return vao;
}

void VKVertexAttributeObjectCache::clear()
{
  for (int index : IndexRange(CACHE_LEN)) {
    keys_[index] = 0;
    values_[index].clear();
  }
  write_index_ = 0;
}

}  // namespace blender::gpu
