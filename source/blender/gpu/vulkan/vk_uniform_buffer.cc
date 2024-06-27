/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_uniform_buffer.hh"
#include "vk_context.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_staging_buffer.hh"
#include "vk_state_manager.hh"

namespace blender::gpu {

void VKUniformBuffer::update(const void *data)
{
  if (!buffer_.is_allocated()) {
    allocate();
  }
  VKContext &context = *VKContext::get();
  if (buffer_.is_mapped()) {
    buffer_.update(data);
  }
  else {
    VKStagingBuffer staging_buffer(buffer_, VKStagingBuffer::Direction::HostToDevice);
    staging_buffer.host_buffer_get().update(data);
    staging_buffer.copy_to_device(context);
  }
}

void VKUniformBuffer::allocate()
{
  buffer_.create(size_in_bytes_,
                 GPU_USAGE_STATIC,
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 false);
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

void VKUniformBuffer::add_to_descriptor_set(AddToDescriptorSetContext &data,
                                            int binding,
                                            shader::ShaderCreateInfo::Resource::BindType bind_type,
                                            const GPUSamplerState /*sampler_state*/)
{
  if (!buffer_.is_allocated()) {
    allocate();
  }

  /* Upload attached data, during bind time. */
  if (data_) {
    update(data_);
    MEM_SAFE_FREE(data_);
  }

  const std::optional<VKDescriptorSet::Location> location =
      data.shader_interface.descriptor_set_location(bind_type, binding);
  if (location) {
    if (bind_type == shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER) {
      data.descriptor_set.bind(*this, *location);
    }
    else {
      data.descriptor_set.bind_as_ssbo(*this, *location);
    }
    render_graph::VKBufferAccess buffer_access = {};
    buffer_access.vk_buffer = buffer_.vk_handle();
    buffer_access.vk_access_flags = data.shader_interface.access_mask(bind_type, binding);
    data.resource_access_info.buffers.append(buffer_access);
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
