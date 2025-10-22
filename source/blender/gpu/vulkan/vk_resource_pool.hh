/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_mutex.hh"

#include "vk_common.hh"

#include "vk_descriptor_pools.hh"
#include "vk_immediate.hh"

namespace blender::gpu {
class VKDevice;
class VKDiscardPool;

template<typename Item> class TimelineResources : Vector<std::pair<TimelineValue, Item>> {
  friend class VKDiscardPool;

 public:
  void append_timeline(TimelineValue timeline, Item item)
  {
    this->append(std::pair(timeline, item));
  }

  void update_timeline(TimelineValue timeline)
  {
    for (std::pair<TimelineValue, Item> &pair : *this) {
      pair.first = timeline;
    }
  }

  int64_t size() const
  {
    return static_cast<const Vector<std::pair<TimelineValue, Item>> &>(*this).size();
  }
  bool is_empty() const
  {
    return static_cast<const Vector<std::pair<TimelineValue, Item>> &>(*this).is_empty();
  }

  /**
   * Remove all items that are used in a timeline before or equal to the current_timeline.
   */
  template<typename Deleter> void remove_old(TimelineValue current_timeline, Deleter deleter)
  {
    int64_t first_index_to_keep = 0;
    for (std::pair<TimelineValue, Item> &item : *this) {
      if (item.first > current_timeline) {
        break;
      }
      deleter(item.second);
      first_index_to_keep++;
    }

    if (first_index_to_keep > 0) {
      this->remove(0, first_index_to_keep);
    }
  }
};

/**
 * Pool of resources that are discarded, but can still be in used and cannot be destroyed.
 *
 * When GPU resources are deleted (`GPU_*_delete`) the GPU handles are kept inside a discard pool.
 * When we are sure that the resource isn't used on the GPU anymore we can safely destroy it.
 *
 * When preparing the next frame, the previous frame can still be rendered. Resources that needs to
 * be destroyed can only be when the previous frame has been completed and being displayed on the
 * screen.
 */
class VKDiscardPool {
  friend class VKDevice;
  friend class VKBackend;

 private:
  TimelineResources<std::pair<VkImage, VmaAllocation>> images_;
  TimelineResources<std::pair<VkBuffer, VmaAllocation>> buffers_;
  TimelineResources<VkImageView> image_views_;
  TimelineResources<VkBufferView> buffer_views_;
  TimelineResources<VkShaderModule> shader_modules_;
  TimelineResources<VkPipeline> pipelines_;
  TimelineResources<VkPipelineLayout> pipeline_layouts_;
  TimelineResources<std::pair<VkDescriptorPool, VKDescriptorPools *>> descriptor_pools_;

  Mutex mutex_;

  TimelineValue timeline_ = UINT64_MAX;

 public:
  void deinit(VKDevice &device);

  void discard_image(VkImage vk_image, VmaAllocation vma_allocation);
  void discard_image_view(VkImageView vk_image_view);
  void discard_buffer(VkBuffer vk_buffer, VmaAllocation vma_allocation);
  void discard_buffer_view(VkBufferView vk_buffer_view);
  void discard_shader_module(VkShaderModule vk_shader_module);
  void discard_pipeline(VkPipeline vk_pipeline);
  void discard_pipeline_layout(VkPipelineLayout vk_pipeline_layout);
  void discard_descriptor_pool_for_reuse(VkDescriptorPool vk_descriptor_pool,
                                         VKDescriptorPools *descriptor_pools);

  /**
   * Move discarded resources from src_pool into this.
   *
   * GPU resources that are discarded from the dependency graph are stored in the device orphaned
   * data. When a swap-chain context list is made active the orphaned data can be merged into a
   * swap-chain discard pool.
   *
   * All moved items will receive a new timeline.
   *
   * Function must be externally synced (
   *
   * <source>
   * {
   *   std::scoped_lock lock(pool.mutex_get()));
   *   pool.move_data(src_pool, timeline);
   * }
   * </source>
   */
  void move_data(VKDiscardPool &src_pool, TimelineValue timeline);
  inline Mutex &mutex_get()
  {
    return mutex_;
  }
  void destroy_discarded_resources(VKDevice &device, TimelineValue current_timeline);

  /**
   * Returns the discard pool for the current thread.
   *
   * When active thread has a context it uses the context discard pool.
   * Otherwise a device discard pool is used.
   */
  static VKDiscardPool &discard_pool_get();
};

}  // namespace blender::gpu
