/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_texture_pool_private.hh"

#include "vk_texture.hh"

#include <list>

namespace blender::gpu {

/* Hashable struct describing a segment of allocated memory.. */
struct VKMemorySegment {
  VkDeviceSize offset;
  VkDeviceSize size;

  uint64_t hash() const;
  bool operator==(const VKMemorySegment &) const = default;
};

/* Hashable struct containing key information to identify or create a VkImage handle. */
struct VKImageInfo {
  VkImageCreateInfo create_info;
  VmaAllocation allocation;
  VKMemorySegment segment;

  uint64_t hash() const;
  bool operator==(const VKImageInfo &) const;
};

/* Map to VkImage handles bound to segments of specific allocations. */
class VKImageCache {
  /* Unused VkImages are eventually passed to discard pool, unless they are reused. */
  static constexpr int max_unused_cycles_ = 8;

  struct VKImageHandle {
    VkImage image = VK_NULL_HANDLE;
    int unused_cycles_count = 0;
  };

  Map<VKImageInfo, VKImageHandle> cache_;

 public:
  /* Given key information identifying a particular VkImage, get or create it. */
  VkImage get_or_create(const VKImageInfo &info);

  /* Remove unused images  and send them to the discard pool.
   * If `force_free` is true, removes all images in the cache. */
  void reset(bool force_reset = false);

  /* Reset VKImageHandle internal counter. */
  void reset_unused_cycles_count(const VKImageInfo &info);

  /* Discard cached images bound to an allocation. */
  void discard_all_of(VmaAllocation allocation);

  uint64_t size() const
  {
    return cache_.size();
  }
};

class VKTexturePool : public TexturePool {
  /* Performed allocation size, current is 64mb. */
  static constexpr VkDeviceSize allocation_size = 1 << 26;

  /* Defer deallocation enough cycles to avoid interleaved calls to different viewport render
   * functions (selection / display) causing constant allocation / deallocation (See #113024). */
  static constexpr int max_unused_cycles_ = 8;

  /* Struct to manage a VmaAllocation. The allocation is segmented
   * for binding to multiple images at a time, and allows aliasing. */
  struct AllocationHandle {
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info = {};

    /* Counter to track the number of unused cycles before deallocation in `pool_`. */
    int unused_cycles_count = 0;

    /* Linked list of unused segments of the allocation. */
    std::list<VKMemorySegment> segments;

    /* Allocate/deallocate the handle internals. */
    void alloc(VkMemoryRequirements memory_requirements);
    void free();

    /* Extract a segment of the allocation for binding, if compatible. */
    std::optional<VKMemorySegment> acquire(VkMemoryRequirements memory_requirements);

    /* Return a segment to the allocation for reuse. */
    void release(VKMemorySegment segment);

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

  /* Struct to manage an acquired texture. The texture has a backing image in
   * the VKimageCache, which is bound to a segment of an allocation. */
  struct TextureHandle {
    VKTexture *texture = nullptr;
    VKImageInfo image_info;

    /* Counter to track texture acquire/retain mismatches in `acquire_`.  */
    int users_count = 1;

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

  /* Cache of VkImage handles to avoid repeated memory binding. */
  VKImageCache image_cache_;
  /* Allocated memory on which images are bound. */
  Set<AllocationHandle> allocations_;
  /* Texture handles currently in use. */
  Set<TextureHandle> acquired_;

  /* Debug storage to log memory usage. Log is only output
   * if values have changed since the last `::reset()`. */
  struct LogUsageData {
    int64_t allocation_count = 0;
    int64_t image_cache_size = 0;
    VkDeviceSize acquired_segment_size = 0;
    VkDeviceSize acquired_segment_size_max = 0;

    bool operator==(const LogUsageData &o) const
    {
      return allocation_count == o.allocation_count && image_cache_size == o.image_cache_size &&
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
