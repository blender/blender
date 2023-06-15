/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_uniform_buffer.hh"
#include "vk_context.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_state_manager.hh"

namespace blender::gpu {

void VKUniformBuffer::update(const void *data)
{
  if (!buffer_.is_allocated()) {
    allocate();
  }
  buffer_.update(data);
}

void VKUniformBuffer::allocate()
{
  buffer_.create(size_in_bytes_, GPU_USAGE_STATIC, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  debug::object_label(buffer_.vk_handle(), name_);
}

void VKUniformBuffer::clear_to_zero()
{
  if (!buffer_.is_allocated()) {
    allocate();
  }
  VKContext &context = *VKContext::get();
  buffer_.clear(context, 0);
}

void VKUniformBuffer::bind(int slot, shader::ShaderCreateInfo::Resource::BindType bind_type)
{
  if (!buffer_.is_allocated()) {
    allocate();
  }

  VKContext &context = *VKContext::get();
  VKShader *shader = static_cast<VKShader *>(context.shader);
  const VKShaderInterface &shader_interface = shader->interface_get();
  const std::optional<VKDescriptorSet::Location> location =
      shader_interface.descriptor_set_location(bind_type, slot);
  if (location) {
    VKDescriptorSetTracker &descriptor_set = shader->pipeline_get().descriptor_set_get();
    /* TODO: move to descriptor set. */
    if (bind_type == shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER) {
      descriptor_set.bind(*this, *location);
    }
    else {
      descriptor_set.bind_as_ssbo(*this, *location);
    }
  }
}

void VKUniformBuffer::bind(int slot)
{
  VKContext &context = *VKContext::get();
  context.state_manager_get().uniform_buffer_bind(this, slot);
}

void VKUniformBuffer::bind_as_ssbo(int slot)
{
  VKContext &context = *VKContext::get();
  context.state_manager_get().storage_buffer_bind(*this, slot);
}

void VKUniformBuffer::unbind()
{
  const VKContext *context = VKContext::get();
  if (context != nullptr) {
    VKStateManager &state_manager = context->state_manager_get();
    state_manager.uniform_buffer_unbind(this);
    state_manager.storage_buffer_unbind(*this);
  }
}

}  // namespace blender::gpu
