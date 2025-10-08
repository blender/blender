/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_texture.hh"

#include "GPU_vertex_buffer.hh"
#include "gpu_storage_buffer_private.hh"

#include "vk_buffer.hh"
#include "vk_staging_buffer.hh"

namespace blender::gpu {
class VertBuf;

class VKStorageBuffer : public StorageBuf {
  GPUUsageType usage_;
  VKBuffer buffer_;

  /** Staging buffer that is used when doing an async read-back. */
  VKStagingBuffer *async_read_buffer_ = nullptr;
  VkDeviceSize offset_ = 0;

 public:
  VKStorageBuffer(size_t size, GPUUsageType usage, const char *name);
  ~VKStorageBuffer();

  void update(const void *data) override;
  void bind(int slot) override;
  void unbind() override;
  void clear(uint32_t clear_value) override;
  void copy_sub(VertBuf *src, uint dst_offset, uint src_offset, uint copy_size) override;
  void read(void *data) override;
  void async_flush_to_host() override;
  void sync_as_indirect_buffer() override { /* No-Op. */ };

  VkBuffer vk_handle() const
  {
    return buffer_.vk_handle();
  }
  inline VkDeviceAddress device_address_get() const
  {
    return buffer_.device_address_get();
  }

  int64_t size_in_bytes() const
  {
    return buffer_.size_in_bytes();
  }
  VkDeviceSize offset_get() const
  {
    return offset_;
  }

  void ensure_allocated();

 private:
  void allocate();
};

BLI_INLINE VKStorageBuffer *unwrap(StorageBuf *storage_buffer)
{
  return static_cast<VKStorageBuffer *>(storage_buffer);
}

}  // namespace blender::gpu
