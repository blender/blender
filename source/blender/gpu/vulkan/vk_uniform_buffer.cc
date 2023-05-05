/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_uniform_buffer.hh"
#include "vk_context.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"

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
  const VKDescriptorSet::Location location = shader_interface.descriptor_set_location(bind_type,
                                                                                      slot);
  VKDescriptorSetTracker &descriptor_set = shader->pipeline_get().descriptor_set_get();
  descriptor_set.bind(*this, location);
}

void VKUniformBuffer::bind(int slot)
{
  bind(slot, shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER);
}

void VKUniformBuffer::bind_as_ssbo(int slot)
{
  bind(slot, shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER);
}

void VKUniformBuffer::unbind() {}

}  // namespace blender::gpu
