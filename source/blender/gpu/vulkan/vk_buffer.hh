/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_context_private.hh"

#include "BLI_utility_mixins.hh"
#include "vk_common.hh"

namespace blender::gpu {
class VKContext;
class VKDevice;

/**
 * Class for handing vulkan buffers (allocation/updating/binding).
 */
class VKBuffer : public NonCopyable {
  size_t size_in_bytes_ = 0;
  VkBuffer vk_buffer_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = VK_NULL_HANDLE;
  /* Pointer to the virtually mapped memory. */
  void *mapped_memory_ = nullptr;

 public:
  VKBuffer() = default;
  virtual ~VKBuffer();

  /** Has this buffer been allocated? */
  bool is_allocated() const;
  bool create(size_t size,
              GPUUsageType usage,
              VkBufferUsageFlags buffer_usage,
              bool is_host_visible = true);
  void clear(VKContext &context, uint32_t clear_value);
  void update_immediately(const void *data) const;

  /**
   * Update the buffer as part of the render graph evaluation. The ownership of data will be
   * transferred to the render graph and should have been allocated using guarded alloc.
   */
  void update_render_graph(VKContext &context, void *data) const;
  void flush() const;
  void read(VKContext &context, void *data) const;

  /**
   * Free the buffer.
   *
   * Discards the buffer so it can be destroyed safely later. Buffers can still be used when
   * rendering so we can only destroy them after the rendering is completed.
   */
  bool free();

  /**
   * Destroy the buffer immediately.
   */
  void free_immediately(VKDevice &device);

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

  /**
   * Is this buffer mapped (visible on host)
   */
  bool is_mapped() const;

 private:
  /** Check if this buffer is mapped. */
  bool map();
  void unmap();
};

/**
 * Helper struct to enable buffers to be bound with an offset.
 *
 * Used for de-interleaved vertex input buffers and immediate mode buffers.
 */
struct VKBufferWithOffset {
  const VkBuffer buffer;
  VkDeviceSize offset;
};

}  // namespace blender::gpu
