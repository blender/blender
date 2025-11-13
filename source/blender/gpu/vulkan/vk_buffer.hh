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
  size_t alloc_size_in_bytes_ = 0;
  VkBuffer vk_buffer_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = VK_NULL_HANDLE;
  VkMemoryPropertyFlags vk_memory_property_flags_;
  TimelineValue async_timeline_ = 0;
  /** Has a previous allocation failed. Will skip reallocations. */
  bool allocation_failed_ = false;

  /* Pointer to the virtually mapped memory. */
  void *mapped_memory_ = nullptr;

  VkDeviceAddress vk_device_address = 0;

 public:
  VKBuffer() = default;
  virtual ~VKBuffer();

  /** Has this buffer been allocated? */
  bool is_allocated() const;

  /**
   * Allocate the buffer.
   */
  bool create(size_t size,
              VkBufferUsageFlags buffer_usage,
              VmaMemoryUsage vma_memory_usage,
              VmaAllocationCreateFlags vma_allocation_flags,
              float priority,
              bool export_memory = false);
  void clear(VKContext &context, uint32_t clear_value);
  void update_immediately(const void *data) const;
  void update_sub_immediately(size_t start_offset, size_t data_size, const void *data) const;

  /**
   * Update the buffer as part of the render graph evaluation. The ownership of data will be
   * transferred to the render graph and should have been allocated using guarded alloc.
   */
  void update_render_graph(VKContext &context, void *data) const;
  void flush() const;

  /**
   * Read the buffer (synchronously).
   */
  void read(VKContext &context, void *data) const;

  /**
   * Start a async read-back.
   */
  void async_flush_to_host(VKContext &context);

  /**
   * Wait until the async read back is finished and fill the given data with the content of the
   * buffer.
   *
   * Will start a new async read-back when there is no read back in progress.
   */
  void read_async(VKContext &context, void *data);

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

  VkDeviceAddress device_address_get() const
  {
    return vk_device_address;
  }

  /**
   * Is this buffer mapped (visible on host)
   */
  bool is_mapped() const;

  /**
   * Get allocated device memory.
   */
  VkDeviceMemory export_memory_get(size_t &memory_size);

 private:
  /** Check if this buffer is mapped. */
  bool map();
  void unmap();
};

inline void *VKBuffer::mapped_memory_get() const
{
  BLI_assert_msg(this->is_mapped(), "Cannot access a non-mapped buffer.");
  return mapped_memory_;
}

inline bool VKBuffer::is_mapped() const
{
  return mapped_memory_ != nullptr;
}

inline bool VKBuffer::is_allocated() const
{
  return allocation_ != VK_NULL_HANDLE;
}

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
