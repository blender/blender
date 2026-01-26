/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_texture_pool_private.hh"

namespace blender::gpu {

class VKTexturePool : public TexturePool {
  /* Defer deallocation enough cycles to avoid interleaved calls to different viewport render
   * functions (selection / display) causing constant allocation / deallocation (See #113024). */
  static constexpr int max_unused_cycles_ = 8;

  /* Struct to store a memory allocation. */
  struct AllocationHandle {
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info = {};

    /* Counter to track the number of unused cycles before deallocation in `pool_`. */
    int unused_cycles_count = 0;

    /* Allocate/deallocate the handle internals. */
    bool alloc(VkMemoryRequirements memory_requirements);
    void free();
  };

  /* Struct to store an acquired texture and its backing allocation. */
  struct TextureHandle {
    VKTexture *texture = nullptr;
    AllocationHandle allocation_handle = {};

    /* Counter to track texture acquire/retain mismatches in `acquire_`.  */
    int users_count = 1;

    /* Create/destroy the VKTexture+VkImage backing the internal pointer. */
    bool alloc(int2 extent, TextureFormat format, eGPUTextureUsage usage, const char *name);
    void free();

    /* We use the pointer as hash/comparator, as a TextureHandle cannot be acquired twice.
     * This means we can find the handle without knowing the internal counter. */
    uint64_t hash() const
    {
      return get_default_hash(texture);
    }

    bool operator==(const TextureHandle &o) const
    {
      return texture == o.texture;
    }
  };

  Vector<AllocationHandle> pool_;
  Set<TextureHandle> acquired_;

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
