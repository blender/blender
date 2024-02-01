/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_texture.h"

#include "gpu_storage_buffer_private.hh"
#include "gpu_vertex_buffer_private.hh"

#include "vk_bindable_resource.hh"
#include "vk_buffer.hh"

namespace blender::gpu {
class VertBuf;

class VKStorageBuffer : public StorageBuf, public VKBindableResource {
  GPUUsageType usage_;
  VKBuffer buffer_;

 public:
  VKStorageBuffer(int size, GPUUsageType usage, const char *name);

  void update(const void *data) override;
  void bind(int slot) override;
  void bind(int slot,
            shader::ShaderCreateInfo::Resource::BindType bind_type,
            const GPUSamplerState sampler_state) override;
  void unbind() override;
  void clear(uint32_t clear_value) override;
  void copy_sub(VertBuf *src, uint dst_offset, uint src_offset, uint copy_size) override;
  void read(void *data) override;
  void async_flush_to_host() override;
  void sync_as_indirect_buffer() override{/* No-Op.*/};

  VkBuffer vk_handle() const
  {
    return buffer_.vk_handle();
  }

  int64_t size_in_bytes() const
  {
    return buffer_.size_in_bytes();
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
