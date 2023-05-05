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
  ghost_window_ = ghost_window;
  if (ghost_window) {
    ghost_context = GHOST_GetDrawingContext((GHOST_WindowHandle)ghost_window);
  }
  ghost_context_ = ghost_context;
  VKDevice &device = VKBackend::get().device_;
  if (!device.is_initialized()) {
    device.init(ghost_context);
  }

  state_manager = new VKStateManager();

  /* For off-screen contexts. Default frame-buffer is empty. */
  back_left = new VKFrameBuffer("back_left");
}

VKContext::~VKContext() {}

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
  VKDevice &device = VKBackend::get().device_;
  command_buffer_.init(device.device_get(), device.queue_get(), command_buffer);
  command_buffer_.begin_recording();
  device.descriptor_pools_get().reset();
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

/* -------------------------------------------------------------------- */
/** \name State manager
 * \{ */

const VKStateManager &VKContext::state_manager_get() const
{
  return *static_cast<const VKStateManager *>(state_manager);
}

/** \} */

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
