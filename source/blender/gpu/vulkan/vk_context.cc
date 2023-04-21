/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */
#include "vk_context.hh"
#include "vk_debug.hh"

#include "vk_backend.hh"
#include "vk_framebuffer.hh"
#include "vk_memory.hh"
#include "vk_state_manager.hh"

#include "GHOST_C-api.h"

namespace blender::gpu {

VKContext::VKContext(void *ghost_window, void *ghost_context)
{
  VK_ALLOCATION_CALLBACKS;
  ghost_window_ = ghost_window;
  if (ghost_window) {
    ghost_context = GHOST_GetDrawingContext((GHOST_WindowHandle)ghost_window);
  }
  ghost_context_ = ghost_context;

  GHOST_GetVulkanHandles((GHOST_ContextHandle)ghost_context,
                         &vk_instance_,
                         &vk_physical_device_,
                         &vk_device_,
                         &vk_queue_family_,
                         &vk_queue_);
  debug::init_callbacks(this, vkGetInstanceProcAddr);
  init_physical_device_limits();

  debug::object_label(this, vk_device_, "LogicalDevice");
  debug::object_label(this, vk_queue_, "GenericQueue");

  /* Initialize the memory allocator. */
  VmaAllocatorCreateInfo info = {};
  /* Should use same vulkan version as GHOST (1.2), but set to 1.0 as 1.2 requires
   * correct extensions and functions to be found by VMA, which isn't working as expected and
   * requires more research. To continue development we lower the API to version 1.0. */
  info.vulkanApiVersion = VK_API_VERSION_1_0;
  info.physicalDevice = vk_physical_device_;
  info.device = vk_device_;
  info.instance = vk_instance_;
  info.pAllocationCallbacks = vk_allocation_callbacks;
  vmaCreateAllocator(&info, &mem_allocator_);
  descriptor_pools_.init(vk_device_);

  state_manager = new VKStateManager();

  VKBackend::capabilities_init(*this);

  /* For off-screen contexts. Default frame-buffer is empty. */
  back_left = new VKFrameBuffer("back_left");
}

VKContext::~VKContext()
{
  vmaDestroyAllocator(mem_allocator_);
  debug::destroy_callbacks(this);
}

void VKContext::init_physical_device_limits()
{
  BLI_assert(vk_physical_device_ != VK_NULL_HANDLE);
  VkPhysicalDeviceProperties properties = {};
  vkGetPhysicalDeviceProperties(vk_physical_device_, &properties);
  vk_physical_device_limits_ = properties.limits;
}

void VKContext::activate()
{
  if (ghost_window_) {
    VkImage image; /* TODO will be used for reading later... */
    VkFramebuffer vk_framebuffer;
    VkRenderPass render_pass;
    VkExtent2D extent;
    uint32_t fb_id;

    GHOST_GetVulkanBackbuffer(
        (GHOST_WindowHandle)ghost_window_, &image, &vk_framebuffer, &render_pass, &extent, &fb_id);

    /* Recreate the gpu::VKFrameBuffer wrapper after every swap. */
    if (has_active_framebuffer()) {
      deactivate_framebuffer();
    }
    delete back_left;

    VKFrameBuffer *framebuffer = new VKFrameBuffer(
        "back_left", vk_framebuffer, render_pass, extent);
    back_left = framebuffer;
    framebuffer->bind(false);
  }
}

void VKContext::deactivate() {}

void VKContext::begin_frame()
{
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  GHOST_GetVulkanCommandBuffer(static_cast<GHOST_ContextHandle>(ghost_context_), &command_buffer);
  command_buffer_.init(vk_device_, vk_queue_, command_buffer);
  command_buffer_.begin_recording();

  descriptor_pools_.reset();
}

void VKContext::end_frame()
{
  command_buffer_.end_recording();
}

void VKContext::flush()
{
  command_buffer_.submit();
}

void VKContext::finish()
{
  if (has_active_framebuffer()) {
    deactivate_framebuffer();
  }
  command_buffer_.submit();
}

void VKContext::memory_statistics_get(int * /*total_mem*/, int * /*free_mem*/) {}

void VKContext::activate_framebuffer(VKFrameBuffer &framebuffer)
{
  if (has_active_framebuffer()) {
    deactivate_framebuffer();
  }

  BLI_assert(active_fb == nullptr);
  active_fb = &framebuffer;
  command_buffer_.begin_render_pass(framebuffer);
}

bool VKContext::has_active_framebuffer() const
{
  return active_fb != nullptr;
}

void VKContext::deactivate_framebuffer()
{
  BLI_assert(active_fb != nullptr);
  VKFrameBuffer *framebuffer = unwrap(active_fb);
  command_buffer_.end_render_pass(*framebuffer);
  active_fb = nullptr;
}

}  // namespace blender::gpu
