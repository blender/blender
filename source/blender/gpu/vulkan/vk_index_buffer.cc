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

  if (!data_uploaded_ && buffer_.is_mapped()) {
    buffer_.update_immediately(data_);
    MEM_SAFE_FREE(data_);
  }
  else {
    VKContext &context = *VKContext::get();
    VKStagingBuffer staging_buffer(buffer_, VKStagingBuffer::Direction::HostToDevice);
    staging_buffer.host_buffer_get().update_immediately(data_);
    staging_buffer.copy_to_device(context);
    MEM_SAFE_FREE(data_);
  }

  data_uploaded_ = true;
}

void VKIndexBuffer::upload_data()
{
  ensure_updated();
}

void VKIndexBuffer::bind_as_ssbo(uint binding)
{
  if (is_subrange_) {
    src_->bind_as_ssbo(binding);
    return;
  }

  VKContext::get()->state_manager_get().storage_buffer_bind(
      BindSpaceStorageBuffers::Type::IndexBuffer, this, binding);
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
  buffer_.create(size_get(),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 VkMemoryPropertyFlags(0),
                 VmaAllocationCreateFlags(0));
  debug::object_label(buffer_.vk_handle(), "IndexBuffer");
}

const VKBuffer &VKIndexBuffer::buffer_get() const
{
  return is_subrange_ ? unwrap(src_)->buffer_ : buffer_;
}
VKBuffer &VKIndexBuffer::buffer_get()
{
  return is_subrange_ ? unwrap(src_)->buffer_ : buffer_;
}

}  // namespace blender::gpu
