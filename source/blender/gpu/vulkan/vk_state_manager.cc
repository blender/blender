/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_state_manager.hh"
#include "vk_context.hh"
#include "vk_pipeline.hh"
#include "vk_shader.hh"
#include "vk_texture.hh"
#include "vk_vertex_buffer.hh"

#include "GPU_capabilities.h"

namespace blender::gpu {

VKStateManager::VKStateManager()
{
  sampler_.create();
  constexpr int max_bindings = 16;
  image_bindings_ = Array<ImageBinding>(max_bindings);
  image_bindings_.fill(ImageBinding());
  texture_bindings_ = Array<TextureBinding>(max_bindings);
  texture_bindings_.fill(TextureBinding());
  uniform_buffer_bindings_ = Array<UniformBufferBinding>(max_bindings);
  uniform_buffer_bindings_.fill(UniformBufferBinding());
}

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
    for (int binding : IndexRange(image_bindings_.size())) {
      if (image_bindings_[binding].texture) {
        image_bindings_[binding].texture->image_bind(binding);
      }
    }

    for (int binding : IndexRange(texture_bindings_.size())) {
      if (texture_bindings_[binding].texture) {
        texture_bindings_[binding].texture->bind(binding, sampler_);
      }
      else if (texture_bindings_[binding].vertex_buffer) {
        texture_bindings_[binding].vertex_buffer->bind(binding);
      }
    }

    for (int binding : IndexRange(uniform_buffer_bindings_.size())) {
      if (uniform_buffer_bindings_[binding].buffer) {
        uniform_buffer_bindings_[binding].buffer->bind(
            binding, shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER);
      }
    }
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
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  /* TODO: Pipeline barriers should be added. We might be able to extract it from
   * the actual pipeline, later on, but for now we submit the work as barrier. */
  command_buffer.submit();
}

void VKStateManager::texture_bind(Texture *tex, GPUSamplerState /*sampler*/, int unit)
{
  VKTexture *texture = unwrap(tex);
  texture_bindings_[unit].texture = texture;
  texture_bindings_[unit].vertex_buffer = nullptr;
}

void VKStateManager::texture_unbind(Texture *tex)
{
  VKTexture *texture = unwrap(tex);
  for (TextureBinding &binding : texture_bindings_) {
    if (binding.texture == texture) {
      binding.texture = nullptr;
    }
  }
}

void VKStateManager::texture_unbind_all()
{
  for (TextureBinding &binding : texture_bindings_) {
    binding.texture = nullptr;
  }
}

void VKStateManager::image_bind(Texture *tex, int binding)
{
  VKTexture *texture = unwrap(tex);
  image_bindings_[binding].texture = texture;
}

void VKStateManager::image_unbind(Texture *tex)
{
  VKTexture *texture = unwrap(tex);
  for (ImageBinding &binding : image_bindings_) {
    if (binding.texture == texture) {
      binding.texture = nullptr;
    }
  }
}

void VKStateManager::image_unbind_all()
{
  for (TextureBinding &binding : image_bindings_) {
    binding.texture = nullptr;
  }
}

void VKStateManager::uniform_buffer_bind(VKUniformBuffer *uniform_buffer, int slot)
{
  uniform_buffer_bindings_[slot].buffer = uniform_buffer;
}

void VKStateManager::uniform_buffer_unbind(VKUniformBuffer *uniform_buffer)
{
  for (UniformBufferBinding &binding : uniform_buffer_bindings_) {
    if (binding.buffer == uniform_buffer) {
      binding.buffer = nullptr;
    }
  }
}

void VKStateManager::texel_buffer_bind(VKVertexBuffer *vertex_buffer, int slot)
{
  texture_bindings_[slot].vertex_buffer = vertex_buffer;
  texture_bindings_[slot].texture = nullptr;
}

void VKStateManager::texel_buffer_unbind(VKVertexBuffer *vertex_buffer)
{
  for (TextureBinding &binding : texture_bindings_) {
    if (binding.vertex_buffer == vertex_buffer) {
      binding.vertex_buffer = nullptr;
      binding.texture = nullptr;
    }
  }
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
