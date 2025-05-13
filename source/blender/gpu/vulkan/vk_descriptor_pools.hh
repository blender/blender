/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_vector.hh"

#include "vk_descriptor_set.hh"

namespace blender::gpu {
class VKDevice;

/**
 * List of VkDescriptorPools.
 *
 * In Vulkan a pool is constructed with a fixed size per resource type. When more resources are
 * needed it a next pool should be created. VKDescriptorPools will keep track of those pools and
 * construct new pools when the previous one is exhausted.
 *
 * At the beginning of a new frame the descriptor pools are reset. This will start allocating
 * again from the first descriptor pool in order to use freed space from previous pools.
 */
class VKDescriptorPools {
  /**
   * Pool sizes to use. When one descriptor pool is requested to allocate a descriptor but isn't
   * able to do so, it will fail.
   *
   * See VKDescriptorSetTracker::upload_descriptor_sets for rebalancing the pool sizes.
   */
  static constexpr uint32_t POOL_SIZE_STORAGE_BUFFER = 1000;
  static constexpr uint32_t POOL_SIZE_DESCRIPTOR_SETS = 250;
  static constexpr uint32_t POOL_SIZE_STORAGE_IMAGE = 250;
  static constexpr uint32_t POOL_SIZE_COMBINED_IMAGE_SAMPLER = 250;
  static constexpr uint32_t POOL_SIZE_UNIFORM_BUFFER = 500;
  static constexpr uint32_t POOL_SIZE_UNIFORM_TEXEL_BUFFER = 100;
  static constexpr uint32_t POOL_SIZE_INPUT_ATTACHMENT = 100;

  Vector<VkDescriptorPool> pools_;
  int64_t active_pool_index_ = 0;

 public:
  VKDescriptorPools();
  ~VKDescriptorPools();

  void init(const VKDevice &vk_device);

  VkDescriptorSet allocate(const VkDescriptorSetLayout descriptor_set_layout);

  /**
   * Discard all existing pools and re-initializes this instance.
   *
   * This is a fix to ensure that resources will not be rewritten. Eventually we should discard the
   * resource pools for reuse.
   */
  void discard(VKContext &vk_context);

 private:
  VkDescriptorPool active_pool_get();
  void activate_next_pool();
  void activate_last_pool();
  bool is_last_pool_active();
  void add_new_pool(const VKDevice &device);
};
}  // namespace blender::gpu
