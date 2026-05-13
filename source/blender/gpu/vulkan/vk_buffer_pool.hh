/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_string_ref.hh"

#include "vk_buffer.hh"
#include "vk_common.hh"

namespace blender::gpu {
/**
 * \brief A pool of VkBuffers.
 */
class VKBufferPool {
  /**
   * All allocated buffers in the pool
   */
  Vector<std::unique_ptr<VKBuffer>> buffers_;
  StringRefNull name_;
  VkDeviceSize buffer_offset_;
  VkDeviceSize default_buffer_size_;
  VkDeviceSize alignment_;
  Vector<uint8_t> data_;
  VkBufferUsageFlags vk_buffer_usage_;
  VmaMemoryUsage vma_memory_usage_;
  VmaAllocationCreateFlags vma_allocation_create_flags_;
  float priority_;

 public:
  VKBufferPool(VKBufferPool &&other) = default;
  VKBufferPool(StringRefNull name,
               VkDeviceSize default_buffer_size,
               VkDeviceSize alignment,
               VkBufferUsageFlags vk_buffer_usage,
               VmaMemoryUsage vma_memory_usage,
               VmaAllocationCreateFlags vma_allocation_create_flags,
               float priority = 1.0f)
      : name_(name),
        default_buffer_size_(default_buffer_size),
        alignment_(alignment),
        vk_buffer_usage_(vk_buffer_usage),
        vma_memory_usage_(vma_memory_usage),
        vma_allocation_create_flags_(vma_allocation_create_flags),
        priority_(priority)
  {
    data_.resize(default_buffer_size_);
  }
  virtual ~VKBufferPool();

  /**
   * \brief Append the given data.
   *
   * \returns the buffer handle and offset in the buffer where the data is stored.
   * \note data might not be available in the buffer yet. Use `ensure_uploaded` to ensure that all
   * data is available by the GPU.
   */
  VKBufferWithOffset append(Span<uint8_t> data);

  /**
   * \brief Ensure that all the data is uploaded.
   */
  void ensure_uploaded();

  /**
   * \brief Discard the current buffers to the discard pool.
   */
  void discard();

 private:
  void finalize_active_buffer();
};
}  // namespace blender::gpu
