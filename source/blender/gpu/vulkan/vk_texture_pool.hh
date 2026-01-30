/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_texture_pool_private.hh"
#include <list>

namespace blender::gpu {

class VKTexturePool : public TexturePool {
  /* Performed allocation size, current is 64mb. */
  static constexpr VkDeviceSize allocation_size = 1 << 26;

  /* Defer deallocation enough cycles to avoid interleaved calls to different viewport render
   * functions (selection / display) causing constant allocation / deallocation (See #113024). */
  static constexpr int max_unused_cycles_ = 8;

  struct Segment {
    VkDeviceSize offset;
    VkDeviceSize size;
  };

  /* Struct to manage a memory allocation. This region of memory can be segmented
   * for binding to multiple supported resources at a time. */
  struct AllocationHandle {
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info = {};

    /* Counter to track the number of unused cycles before deallocation in `pool_`. */
    int unused_cycles_count = 0;

    /* Linked list of unused segments of the allocation. */
    std::list<Segment> segments;

    /* Allocate/deallocate the handle internals. */
    void alloc(VkMemoryRequirements memory_requirements);
    void free();

    /* Extract a segment of the allocation for binding, if compatible. */
    std::optional<Segment> acquire(VkMemoryRequirements memory_requirements);

    /* Return a segment to the allocation for reuse. */
    void release(Segment segment);

    /* Check if no part of the allocation is acuired. */
    bool is_unused() const
    {
      return !segments.empty() && segments.front().size == allocation_info.size;
    }

    /* We use the pointer as hash/comparator, as a VmaAllocation is unique.
     * This means we can find the handle without knowing other internals. */
    uint64_t hash() const
    {
      return get_default_hash(allocation);
    }

    bool operator==(const AllocationHandle &o) const
    {
      return allocation == o.allocation;
    }
  };

  /* Struct to store an acquired texture. The texture image has a backing allocation,
   * and is bound to a segment of this allocation. */
  struct TextureHandle {
    VKTexture *texture = nullptr;
    AllocationHandle allocation_handle = {};
    Segment segment = {};

    /* Counter to track texture acquire/retain mismatches in `acquire_`.  */
    int users_count = 1;

    /* Create or destroy the VKTexture+VkImage and handle internals. */
    void alloc(int2 extent, TextureFormat format, eGPUTextureUsage usage, const char *name);
    void free();

    VkDeviceSize allocation_local_offset() const
    {
      return segment.offset - allocation_handle.allocation_info.offset;
    }

    /* We use the pointer as hash/comparator, as a TextureHandle cannot be acquired twice.
     * This means we can find the handle without knowing other internals */
    uint64_t hash() const
    {
      return get_default_hash(texture);
    }

    bool operator==(const TextureHandle &o) const
    {
      return texture == o.texture;
    }
  };

  /* Stores of allocated memory and textures bound to said memory. */
  Set<AllocationHandle> allocations_;
  Set<TextureHandle> acquired_;

  /* Debug storage to log memory usage. Log is only output
   * if values have changed since the last `::reset()`. */
  struct LogUsageData {
    int64_t allocation_count = 0;
    VkDeviceSize acquired_segment_size = 0;
    VkDeviceSize acquired_segment_size_max = 0;

    bool operator==(const LogUsageData &o) const
    {
      return allocation_count == o.allocation_count &&
             acquired_segment_size == o.acquired_segment_size &&
             acquired_segment_size_max == o.acquired_segment_size_max;
    }
  };
  LogUsageData previous_usage_data_ = {};
  LogUsageData current_usage_data_ = {};

  /* Output usage data to debug log. Called on `--debug-gpu` */
  void log_usage_data();

 public:
  ~VKTexturePool();

  Texture *acquire_texture(int2 extent,
                           TextureFormat format,
                           eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL,
                           const char *name = nullptr) override;

  void release_texture(Texture *tex) override;

  void reset(bool force_free = false) override;

  void offset_users_count(Texture *tex, int offset) override;
};

}  // namespace blender::gpu
