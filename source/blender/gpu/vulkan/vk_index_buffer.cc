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

#include "CLG_log.h"

static CLG_LogRef LOG = {"gpu.vulkan"};

namespace blender::gpu {

void VKIndexBuffer::ensure_updated()
{
  if (is_subrange_) {
    src_->upload_data();
    return;
  }

  if (!buffer_.is_allocated()) {
    allocate();
    if (!buffer_.is_allocated()) {
      CLOG_ERROR(&LOG, "Unable to allocate index buffer. Most likely an out of memory issue.");
      return;
    }
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
    VKBuffer &buffer = staging_buffer.host_buffer_get();
    if (buffer.is_allocated()) {
      staging_buffer.host_buffer_get().update_immediately(data_);
      staging_buffer.copy_to_device(context);
    }
    else {
      buffer_.clear(context, 0u);
      CLOG_ERROR(
          &LOG,
          "Unable to upload data to index buffer via a staging buffer as the staging buffer "
          "could not be allocated. Index buffer will be filled with on zeros to reduce "
          "drawing artifacts due to read from uninitialized memory.");
      buffer_.clear(context, 0u);
    }
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
  VKBuffer &buffer = staging_buffer.host_buffer_get();
  if (buffer.is_mapped()) {
    staging_buffer.copy_from_device(context);
    staging_buffer.host_buffer_get().read(context, data);
  }
  else {
    CLOG_ERROR(&LOG,
               "Unable to read data from index buffer via a staging buffer as the staging buffer "
               "could not be allocated. ");
  }
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
                 VMA_MEMORY_USAGE_AUTO,
                 VmaAllocationCreateFlags(0),
                 0.8f);
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
