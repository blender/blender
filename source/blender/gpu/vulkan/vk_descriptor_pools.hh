/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_vector.hh"

#include "vk_descriptor_set.hh"

namespace blender::gpu {

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
   * Better defaults should be set later on, when we know more about our resource usage.
   */
  static constexpr uint32_t POOL_SIZE_STORAGE_BUFFER = 1000;
  static constexpr uint32_t POOL_SIZE_DESCRIPTOR_SETS = 1000;
  static constexpr uint32_t POOL_SIZE_STORAGE_IMAGE = 1000;
  static constexpr uint32_t POOL_SIZE_COMBINED_IMAGE_SAMPLER = 1000;
  static constexpr uint32_t POOL_SIZE_UNIFORM_BUFFER = 1000;
  static constexpr uint32_t POOL_SIZE_UNIFORM_TEXEL_BUFFER = 1000;

  VkDevice vk_device_ = VK_NULL_HANDLE;
  Vector<VkDescriptorPool> pools_;
  int64_t active_pool_index_ = 0;

 public:
  VKDescriptorPools();
  ~VKDescriptorPools();

  void init(const VkDevice vk_device);

  std::unique_ptr<VKDescriptorSet> allocate(const VkDescriptorSetLayout &descriptor_set_layout);
  void free(VKDescriptorSet &descriptor_set);

  /**
   * Reset the pools to start looking for free space from the first descriptor pool.
   */
  void reset();

 private:
  VkDescriptorPool active_pool_get();
  void activate_next_pool();
  void activate_last_pool();
  bool is_last_pool_active();
  void add_new_pool();
};
}  // namespace blender::gpu
