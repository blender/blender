/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_resource_pool.hh"
#include "vk_backend.hh"
#include "vk_context.hh"

namespace blender::gpu {

void VKDiscardPool::deinit(VKDevice &device)
{
  destroy_discarded_resources(device, UINT64_MAX);
}

void VKDiscardPool::move_data(VKDiscardPool &src_pool, TimelineValue timeline)
{
  src_pool.buffer_views_.update_timeline(timeline);
  src_pool.buffers_.update_timeline(timeline);
  src_pool.image_views_.update_timeline(timeline);
  src_pool.images_.update_timeline(timeline);
  src_pool.shader_modules_.update_timeline(timeline);
  src_pool.pipelines_.update_timeline(timeline);
  src_pool.pipeline_layouts_.update_timeline(timeline);
  src_pool.descriptor_pools_.update_timeline(timeline);
  buffer_views_.extend(std::move(src_pool.buffer_views_));
  buffers_.extend(std::move(src_pool.buffers_));
  image_views_.extend(std::move(src_pool.image_views_));
  images_.extend(std::move(src_pool.images_));
  shader_modules_.extend(std::move(src_pool.shader_modules_));
  pipelines_.extend(std::move(src_pool.pipelines_));
  pipeline_layouts_.extend(std::move(src_pool.pipeline_layouts_));
  descriptor_pools_.extend(std::move(src_pool.descriptor_pools_));
}

void VKDiscardPool::discard_image(VkImage vk_image, VmaAllocation vma_allocation)
{
  std::scoped_lock mutex(mutex_);
  images_.append_timeline(timeline_, std::pair(vk_image, vma_allocation));
}

void VKDiscardPool::discard_image_view(VkImageView vk_image_view)
{
  std::scoped_lock mutex(mutex_);
  image_views_.append_timeline(timeline_, vk_image_view);
}

void VKDiscardPool::discard_buffer(VkBuffer vk_buffer, VmaAllocation vma_allocation)
{
  std::scoped_lock mutex(mutex_);
  buffers_.append_timeline(timeline_, std::pair(vk_buffer, vma_allocation));
}

void VKDiscardPool::discard_buffer_view(VkBufferView vk_buffer_view)
{
  std::scoped_lock mutex(mutex_);
  buffer_views_.append_timeline(timeline_, vk_buffer_view);
}

void VKDiscardPool::discard_shader_module(VkShaderModule vk_shader_module)
{
  std::scoped_lock mutex(mutex_);
  shader_modules_.append_timeline(timeline_, vk_shader_module);
}
void VKDiscardPool::discard_pipeline(VkPipeline vk_pipeline)
{
  std::scoped_lock mutex(mutex_);
  pipelines_.append_timeline(timeline_, vk_pipeline);
}
void VKDiscardPool::discard_pipeline_layout(VkPipelineLayout vk_pipeline_layout)
{
  std::scoped_lock mutex(mutex_);
  pipeline_layouts_.append_timeline(timeline_, vk_pipeline_layout);
}

void VKDiscardPool::discard_descriptor_pool_for_reuse(VkDescriptorPool vk_descriptor_pool,
                                                      VKDescriptorPools *descriptor_pools)
{
  std::scoped_lock mutex(mutex_);
  descriptor_pools_.append_timeline(timeline_, std::pair(vk_descriptor_pool, descriptor_pools));
}

void VKDiscardPool::destroy_discarded_resources(VKDevice &device, TimelineValue current_timeline)
{
  std::scoped_lock mutex(mutex_);

  image_views_.remove_old(current_timeline, [&](VkImageView vk_image_view) {
    vkDestroyImageView(device.vk_handle(), vk_image_view, nullptr);
  });

  images_.remove_old(current_timeline, [&](std::pair<VkImage, VmaAllocation> image_allocation) {
    device.resources.remove_image(image_allocation.first);
    vmaDestroyImage(device.mem_allocator_get(), image_allocation.first, image_allocation.second);
  });
  buffer_views_.remove_old(current_timeline, [&](VkBufferView vk_buffer_view) {
    vkDestroyBufferView(device.vk_handle(), vk_buffer_view, nullptr);
  });

  buffers_.remove_old(current_timeline, [&](std::pair<VkBuffer, VmaAllocation> buffer_allocation) {
    device.resources.remove_buffer(buffer_allocation.first);
    vmaDestroyBuffer(
        device.mem_allocator_get(), buffer_allocation.first, buffer_allocation.second);
  });

  pipelines_.remove_old(current_timeline, [&](VkPipeline vk_pipeline) {
    vkDestroyPipeline(device.vk_handle(), vk_pipeline, nullptr);
  });

  pipeline_layouts_.remove_old(current_timeline, [&](VkPipelineLayout vk_pipeline_layout) {
    vkDestroyPipelineLayout(device.vk_handle(), vk_pipeline_layout, nullptr);
  });

  shader_modules_.remove_old(current_timeline, [&](VkShaderModule vk_shader_module) {
    vkDestroyShaderModule(device.vk_handle(), vk_shader_module, nullptr);
  });

  descriptor_pools_.remove_old(
      current_timeline, [&](std::pair<VkDescriptorPool, VKDescriptorPools *> descriptor_pool) {
        descriptor_pool.second->recycle(descriptor_pool.first);
      });
}

VKDiscardPool &VKDiscardPool::discard_pool_get()
{
  VKContext *context = VKContext::get();
  if (context != nullptr) {
    return context->discard_pool;
  }

  VKDevice &device = VKBackend::get().device;
  if (G.is_rendering) {
    return device.orphaned_data_render;
  }
  else {
    return device.orphaned_data;
  }
}

}  // namespace blender::gpu
