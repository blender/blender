/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_common.hh"

#include "vk_descriptor_pools.hh"
#include "vk_immediate.hh"

namespace blender::gpu {
class VKDevice;

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

 private:
  Vector<std::pair<VkImage, VmaAllocation>> images_;
  Vector<std::pair<VkBuffer, VmaAllocation>> buffers_;
  Vector<VkImageView> image_views_;
  Vector<VkShaderModule> shader_modules_;
  Vector<VkPipelineLayout> pipeline_layouts_;
  Vector<VkRenderPass> render_passes_;
  Vector<VkFramebuffer> framebuffers_;
  Map<VkCommandPool, Vector<VkCommandBuffer>> command_buffers_;

  std::mutex mutex_;

  /**
   * Free command buffers generated from `vk_command_pool`.
   *
   * Command buffers are freed in `destroy_discarded_resources`, however if a `vk_command_pool` is
   * going to be destroyed, commands buffers generated from this command pool needs to be freed at
   * forehand.
   */
  void free_command_pool_buffers(VkCommandPool vk_command_pool, VKDevice &device);

 public:
  void deinit(VKDevice &device);

  void discard_image(VkImage vk_image, VmaAllocation vma_allocation);
  void discard_command_buffer(VkCommandBuffer vk_command_buffer, VkCommandPool vk_command_pool);
  void discard_image_view(VkImageView vk_image_view);
  void discard_buffer(VkBuffer vk_buffer, VmaAllocation vma_allocation);
  void discard_shader_module(VkShaderModule vk_shader_module);
  void discard_pipeline_layout(VkPipelineLayout vk_pipeline_layout);
  void discard_framebuffer(VkFramebuffer vk_framebuffer);
  void discard_render_pass(VkRenderPass vk_render_pass);

  /**
   * Move discarded resources from src_pool into this.
   *
   * GPU resources that are discarded from the dependency graph are stored in the device orphaned
   * data. When a swap chain context list is made active the orphaned data can be merged into a
   * swap chain discard pool.
   */
  void move_data(VKDiscardPool &src_pool);
  void destroy_discarded_resources(VKDevice &device);
};

class VKResourcePool {

 public:
  VKDescriptorPools descriptor_pools;
  VKDescriptorSetTracker descriptor_set;
  VKDiscardPool discard_pool;
  VKImmediate immediate;

  void init(VKDevice &device);
  void deinit(VKDevice &device);
  void reset();
};
}  // namespace blender::gpu
