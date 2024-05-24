/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_index_buffer.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_staging_buffer.hh"
#include "vk_state_manager.hh"

namespace blender::gpu {

void VKIndexBuffer::ensure_updated()
{
  if (is_subrange_) {
    src_->upload_data();
    return;
  }

  if (!buffer_.is_allocated()) {
    allocate();
  }

  if (data_ == nullptr) {
    return;
  }

  VKContext &context = *VKContext::get();
  VKStagingBuffer staging_buffer(buffer_, VKStagingBuffer::Direction::HostToDevice);
  staging_buffer.host_buffer_get().update(data_);
  staging_buffer.copy_to_device(context);
  MEM_SAFE_FREE(data_);
}

void VKIndexBuffer::upload_data()
{
  ensure_updated();
}

void VKIndexBuffer::bind(VKContext &context)
{
  context.command_buffers_get().bind(buffer_get(), to_vk_index_type(index_type_));
}

void VKIndexBuffer::bind_as_ssbo(uint binding)
{
  VKContext::get()->state_manager_get().storage_buffer_bind(*this, binding);
}

void VKIndexBuffer::add_to_descriptor_set(AddToDescriptorSetContext &data,
                                          int binding,
                                          shader::ShaderCreateInfo::Resource::BindType bind_type,
                                          const GPUSamplerState /*sampler_state*/)
{
  BLI_assert(bind_type == shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER);
  ensure_updated();

  const std::optional<VKDescriptorSet::Location> location =
      data.shader_interface.descriptor_set_location(bind_type, binding);
  if (location) {
    data.descriptor_set.bind_as_ssbo(*this, *location);
    render_graph::VKBufferAccess buffer_access = {};
    buffer_access.vk_buffer = buffer_.vk_handle();
    buffer_access.vk_access_flags = data.shader_interface.access_mask(bind_type, binding);
    data.resource_access_info.buffers.append(buffer_access);
  }
}

void VKIndexBuffer::read(uint32_t *data) const
{
  VKContext &context = *VKContext::get();
  VKStagingBuffer staging_buffer(buffer_, VKStagingBuffer::Direction::DeviceToHost);
  staging_buffer.copy_from_device(context);
  staging_buffer.host_buffer_get().read(context, data);
}

void VKIndexBuffer::update_sub(uint /*start*/, uint /*len*/, const void * /*data*/)
{
  NOT_YET_IMPLEMENTED
}

void VKIndexBuffer::strip_restart_indices()
{
  NOT_YET_IMPLEMENTED
}

void VKIndexBuffer::allocate()
{
  GPUUsageType usage = data_ == nullptr ? GPU_USAGE_DEVICE_ONLY : GPU_USAGE_STATIC;
  buffer_.create(size_get(),
                 usage,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 false);
  debug::object_label(buffer_.vk_handle(), "IndexBuffer");
}

VKBuffer &VKIndexBuffer::buffer_get()
{
  return is_subrange_ ? unwrap(src_)->buffer_ : buffer_;
}

}  // namespace blender::gpu
