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
class VKContext;
class VKDevice;

/**
 * List of VkDescriptorPools.
 *
 * In Vulkan a pool is constructed with a fixed size per resource type. When more resources are
 * needed it a next pool should be created. VKDescriptorPools will keep track of those pools and
 * construct new pools when the previous one is exhausted.
 *
 * Each context allocates from its own active pool, as pools must be externally synchronized.
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
  static constexpr uint32_t POOL_SIZE_ACCELERATION_STRUCTURE = 100;

  /**
   * Unused pools, shared by all contexts.
   *
   * When the active pool of a context is full it is discarded. After all descriptor sets of the
   * pool are unused it is reset and added here for reuse by any context.
   */
  Vector<VkDescriptorPool> recycled_pools_;
  Mutex mutex_;

 public:
  void deinit(const VKDevice &device);

  /**
   * Allocate a new descriptor set from the active descriptor pool of the context.
   *
   * When the active descriptor pool is full it is discarded and another descriptor pool is
   * ensured.
   */
  VkDescriptorSet allocate(VKContext &context, const VkDescriptorSetLayout descriptor_set_layout);

  /**
   * Discard the active descriptor pool of the context, for reuse once its descriptor sets are
   * unused.
   */
  void discard_active_pool(VKContext &context);

  /**
   * Recycle a previous discarded descriptor pool.
   */
  void recycle(VkDescriptorPool vk_descriptor_pool);

 private:
  VkDescriptorPool acquire_pool(const VKDevice &device);
};
}  // namespace blender::gpu
