/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_context_private.hh"

#include "vk_common.hh"
#include "vk_context.hh"

namespace blender::gpu {

/**
 * Class for handing vulkan buffers (allocation/updating/binding).
 */
class VKBuffer {
  int64_t size_in_bytes_;
  VkBuffer vk_buffer_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = VK_NULL_HANDLE;
  /* Pointer to the virtually mapped memory. */
  void *mapped_memory_ = nullptr;

 public:
  VKBuffer() = default;
  virtual ~VKBuffer();

  /** Has this buffer been allocated? */
  bool is_allocated() const;

  bool create(VKContext &context,
              int64_t size,
              GPUUsageType usage,
              VkBufferUsageFlagBits buffer_usage);
  void update(const void *data) const;
  void read(void *data) const;
  bool free(VKContext &context);

  int64_t size_in_bytes() const
  {
    return size_in_bytes_;
  }

  VkBuffer vk_handle() const
  {
    return vk_buffer_;
  }

  /**
   * Get the reference to the mapped memory.
   *
   * Can only be called when the buffer is (still) mapped.
   */
  void *mapped_memory_get() const;

 private:
  /** Check if this buffer is mapped. */
  bool is_mapped() const;
  bool map(VKContext &context);
  void unmap(VKContext &context);
};
}  // namespace blender::gpu
