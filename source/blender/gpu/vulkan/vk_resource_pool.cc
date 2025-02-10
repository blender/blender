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

void VKResourcePool::init(VKDevice &device)
{
  descriptor_pools.init(device);
}

void VKResourcePool::deinit(VKDevice &device)
{
  immediate.deinit(device);
}

void VKResourcePool::reset()
{
  descriptor_pools.reset();
  immediate.reset();
}

void VKDiscardPool::deinit(VKDevice &device)
{
  destroy_discarded_resources(device, true);
}

void VKDiscardPool::move_data(VKDiscardPool &src_pool, TimelineValue timeline)
{
  std::scoped_lock mutex(mutex_);
  src_pool.buffer_views_.update_timeline(timeline);
  src_pool.buffers_.update_timeline(timeline);
  src_pool.image_views_.update_timeline(timeline);
  src_pool.images_.update_timeline(timeline);
  src_pool.shader_modules_.update_timeline(timeline);
  src_pool.pipeline_layouts_.update_timeline(timeline);
  src_pool.framebuffers_.update_timeline(timeline);
  src_pool.render_passes_.update_timeline(timeline);
  buffer_views_.extend(std::move(src_pool.buffer_views_));
  buffers_.extend(std::move(src_pool.buffers_));
  image_views_.extend(std::move(src_pool.image_views_));
  images_.extend(std::move(src_pool.images_));
  shader_modules_.extend(std::move(src_pool.shader_modules_));
  pipeline_layouts_.extend(std::move(src_pool.pipeline_layouts_));
  framebuffers_.extend(std::move(src_pool.framebuffers_));
  render_passes_.extend(std::move(src_pool.render_passes_));
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
void VKDiscardPool::discard_pipeline_layout(VkPipelineLayout vk_pipeline_layout)
{
  std::scoped_lock mutex(mutex_);
  pipeline_layouts_.append_timeline(timeline_, vk_pipeline_layout);
}

void VKDiscardPool::discard_framebuffer(VkFramebuffer vk_framebuffer)
{
  std::scoped_lock mutex(mutex_);
  framebuffers_.append_timeline(timeline_, vk_framebuffer);
}

void VKDiscardPool::discard_render_pass(VkRenderPass vk_render_pass)
{
  std::scoped_lock mutex(mutex_);
  render_passes_.append_timeline(timeline_, vk_render_pass);
}

void VKDiscardPool::destroy_discarded_resources(VKDevice &device, bool force)
{
  std::scoped_lock mutex(mutex_);
  TimelineValue current_timeline = force ? UINT64_MAX : device.submission_finished_timeline_get();

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

  pipeline_layouts_.remove_old(current_timeline, [&](VkPipelineLayout vk_pipeline_layout) {
    vkDestroyPipelineLayout(device.vk_handle(), vk_pipeline_layout, nullptr);
  });

  shader_modules_.remove_old(current_timeline, [&](VkShaderModule vk_shader_module) {
    vkDestroyShaderModule(device.vk_handle(), vk_shader_module, nullptr);
  });

  framebuffers_.remove_old(current_timeline, [&](VkFramebuffer vk_framebuffer) {
    vkDestroyFramebuffer(device.vk_handle(), vk_framebuffer, nullptr);
  });

  render_passes_.remove_old(current_timeline, [&](VkRenderPass vk_render_pass) {
    vkDestroyRenderPass(device.vk_handle(), vk_render_pass, nullptr);
  });
}

VKDiscardPool &VKDiscardPool::discard_pool_get()
{
  VKContext *context = VKContext::get();
  if (context != nullptr) {
    return context->discard_pool;
  }

  VKDevice &device = VKBackend::get().device;
  return device.orphaned_data;
}

}  // namespace blender::gpu
