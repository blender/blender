/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_state_manager.hh"
#include "vk_vertex_buffer.hh"

#include "vk_storage_buffer.hh"

namespace blender::gpu {

VKStorageBuffer::VKStorageBuffer(int size, GPUUsageType usage, const char *name)
    : StorageBuf(size, name), usage_(usage)
{
}

void VKStorageBuffer::update(const void *data)
{
  ensure_allocated();
  buffer_.update(data);
}

void VKStorageBuffer::ensure_allocated()
{
  if (!buffer_.is_allocated()) {
    allocate();
  }
}

void VKStorageBuffer::allocate()
{
  buffer_.create(size_in_bytes_,
                 usage_,
                 static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT));
  debug::object_label(buffer_.vk_handle(), name_);
}

void VKStorageBuffer::bind(int slot)
{
  VKContext &context = *VKContext::get();
  context.state_manager_get().storage_buffer_bind(*this, slot);
}

void VKStorageBuffer::bind(int slot, shader::ShaderCreateInfo::Resource::BindType bind_type)
{
  VKContext &context = *VKContext::get();
  VKShader *shader = static_cast<VKShader *>(context.shader);
  ensure_allocated();
  const VKShaderInterface &shader_interface = shader->interface_get();
  const std::optional<VKDescriptorSet::Location> location =
      shader_interface.descriptor_set_location(bind_type, slot);
  if (location) {
    shader->pipeline_get().descriptor_set_get().bind(*this, *location);
  }
}

void VKStorageBuffer::unbind()
{
  unbind_from_active_context();
}

void VKStorageBuffer::clear(uint32_t clear_value)
{
  ensure_allocated();
  VKContext &context = *VKContext::get();
  buffer_.clear(context, clear_value);
}

void VKStorageBuffer::copy_sub(VertBuf * /*src*/,
                               uint /*dst_offset*/,
                               uint /*src_offset*/,
                               uint /*copy_size*/)
{
  NOT_YET_IMPLEMENTED;
}

void VKStorageBuffer::read(void *data)
{
  ensure_allocated();
  VKContext &context = *VKContext::get();
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.submit();

  buffer_.read(data);
}

}  // namespace blender::gpu
