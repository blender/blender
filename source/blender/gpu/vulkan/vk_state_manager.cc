/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_state_manager.hh"
#include "vk_context.hh"
#include "vk_index_buffer.hh"
#include "vk_pipeline.hh"
#include "vk_shader.hh"
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_vertex_buffer.hh"

#include "GPU_capabilities.hh"

namespace blender::gpu {

void VKStateManager::apply_state()
{
  VKContext &context = *VKContext::get();
  if (context.shader) {
    VKShader &shader = unwrap(*context.shader);
    VKPipeline &pipeline = shader.pipeline_get();
    pipeline.state_manager_get().set_state(state, mutable_state);
  }
}

void VKStateManager::apply_bindings()
{
  VKContext &context = *VKContext::get();
  if (context.shader) {
    textures_.apply_bindings();
    images_.apply_bindings();
    uniform_buffers_.apply_bindings();
    storage_buffers_.apply_bindings();
  }
}

void VKStateManager::force_state()
{
  VKContext &context = *VKContext::get();
  BLI_assert(context.shader);
  VKShader &shader = unwrap(*context.shader);
  VKPipeline &pipeline = shader.pipeline_get();
  pipeline.state_manager_get().force_state(state, mutable_state);
}

void VKStateManager::issue_barrier(eGPUBarrier /*barrier_bits*/)
{
  VKContext &context = *VKContext::get();
  /* TODO: Pipeline barriers should be added. We might be able to extract it from
   * the actual pipeline, later on, but for now we submit the work as barrier. */
  context.flush();
}

void VKStateManager::texture_bind(Texture *tex, GPUSamplerState sampler, int unit)
{
  VKTexture *texture = unwrap(tex);
  textures_.bind(unit, *texture, sampler);
}

void VKStateManager::texture_unbind(Texture *tex)
{
  VKTexture *texture = unwrap(tex);
  textures_.unbind(*texture);
}

void VKStateManager::texture_unbind_all()
{
  textures_.unbind_all();
}

void VKStateManager::image_bind(Texture *tex, int binding)
{
  VKTexture *texture = unwrap(tex);
  images_.bind(binding, *texture);
}

void VKStateManager::image_unbind(Texture *tex)
{
  VKTexture *texture = unwrap(tex);
  images_.unbind(*texture);
}

void VKStateManager::image_unbind_all()
{
  images_.unbind_all();
}

void VKStateManager::uniform_buffer_bind(VKUniformBuffer *uniform_buffer, int slot)
{
  uniform_buffers_.bind(slot, *uniform_buffer);
}

void VKStateManager::uniform_buffer_unbind(VKUniformBuffer *uniform_buffer)
{
  uniform_buffers_.unbind(*uniform_buffer);
}

void VKStateManager::uniform_buffer_unbind_all()
{
  uniform_buffers_.unbind_all();
}

void VKStateManager::unbind_from_all_namespaces(VKBindableResource &resource)
{
  uniform_buffers_.unbind(resource);
  storage_buffers_.unbind(resource);
  images_.unbind(resource);
  textures_.unbind(resource);
}

void VKStateManager::texel_buffer_bind(VKVertexBuffer &vertex_buffer, int slot)
{
  textures_.bind(slot, vertex_buffer);
}

void VKStateManager::texel_buffer_unbind(VKVertexBuffer &vertex_buffer)
{
  textures_.unbind(vertex_buffer);
}

void VKStateManager::storage_buffer_bind(VKBindableResource &resource, int slot)
{
  storage_buffers_.bind(slot, resource);
}

void VKStateManager::storage_buffer_unbind(VKBindableResource &resource)
{
  storage_buffers_.unbind(resource);
}

void VKStateManager::storage_buffer_unbind_all()
{
  storage_buffers_.unbind_all();
}

void VKStateManager::texture_unpack_row_length_set(uint len)
{
  texture_unpack_row_length_ = len;
}

uint VKStateManager::texture_unpack_row_length_get() const
{
  return texture_unpack_row_length_;
}

}  // namespace blender::gpu
