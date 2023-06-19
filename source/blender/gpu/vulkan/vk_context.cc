/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#include "vk_context.hh"
#include "vk_debug.hh"

#include "vk_backend.hh"
#include "vk_framebuffer.hh"
#include "vk_immediate.hh"
#include "vk_memory.hh"
#include "vk_shader.hh"
#include "vk_state_manager.hh"
#include "vk_texture.hh"

#include "GHOST_C-api.h"

namespace blender::gpu {

VKContext::VKContext(void *ghost_window, void *ghost_context)
{
  ghost_window_ = ghost_window;
  ghost_context_ = ghost_context;

  state_manager = new VKStateManager();
  imm = new VKImmediate();

  /* For off-screen contexts. Default frame-buffer is empty. */
  VKFrameBuffer *framebuffer = new VKFrameBuffer("back_left");
  back_left = framebuffer;
  active_fb = framebuffer;
}

VKContext::~VKContext()
{
  VKBackend::get().device_.context_unregister(*this);

  delete imm;
  imm = nullptr;
}

void VKContext::sync_backbuffer()
{
  if (ghost_window_) {
    VkImage vk_image;
    VkFramebuffer vk_framebuffer;
    VkRenderPass render_pass;
    VkExtent2D extent;
    uint32_t fb_id;

    GHOST_GetVulkanBackbuffer((GHOST_WindowHandle)ghost_window_,
                              &vk_image,
                              &vk_framebuffer,
                              &render_pass,
                              &extent,
                              &fb_id);

    /* Recreate the gpu::VKFrameBuffer wrapper after every swap. */
    if (has_active_framebuffer()) {
      deactivate_framebuffer();
    }
    delete back_left;

    VKFrameBuffer *framebuffer = new VKFrameBuffer(
        "back_left", vk_image, vk_framebuffer, render_pass, extent);
    back_left = framebuffer;
    back_left->bind(false);
  }

  if (ghost_context_) {
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    GHOST_GetVulkanCommandBuffer(static_cast<GHOST_ContextHandle>(ghost_context_),
                                 &command_buffer);
    VKDevice &device = VKBackend::get().device_;
    command_buffer_.init(device.device_get(), device.queue_get(), command_buffer);
    command_buffer_.begin_recording();
    device.descriptor_pools_get().reset();
  }
}

void VKContext::activate()
{
  /* Make sure no other context is already bound to this thread. */
  BLI_assert(is_active_ == false);

  is_active_ = true;

  sync_backbuffer();

  immActivate();
}

void VKContext::deactivate()
{
  immDeactivate();
  is_active_ = false;
}

void VKContext::begin_frame()
{
  sync_backbuffer();
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
  command_buffer_.submit();
}

void VKContext::memory_statistics_get(int * /*total_mem*/, int * /*free_mem*/) {}

/* -------------------------------------------------------------------- */
/** \name State manager
 * \{ */

VKStateManager &VKContext::state_manager_get() const
{
  return *static_cast<VKStateManager *>(state_manager);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame-buffer
 * \{ */

void VKContext::activate_framebuffer(VKFrameBuffer &framebuffer)
{
  if (has_active_framebuffer()) {
    deactivate_framebuffer();
  }

  BLI_assert(active_fb == nullptr);
  active_fb = &framebuffer;
  command_buffer_.begin_render_pass(framebuffer);
}

VKFrameBuffer *VKContext::active_framebuffer_get() const
{
  return unwrap(active_fb);
}

bool VKContext::has_active_framebuffer() const
{
  return active_framebuffer_get() != nullptr;
}

void VKContext::deactivate_framebuffer()
{
  VKFrameBuffer *framebuffer = active_framebuffer_get();
  BLI_assert(framebuffer != nullptr);
  if (framebuffer->is_valid()) {
    command_buffer_.end_render_pass(*framebuffer);
  }
  active_fb = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compute pipeline
 * \{ */

void VKContext::bind_compute_pipeline()
{
  VKShader *shader = unwrap(this->shader);
  BLI_assert(shader);
  VKPipeline &pipeline = shader->pipeline_get();
  pipeline.update_and_bind(
      *this, shader->vk_pipeline_layout_get(), VK_PIPELINE_BIND_POINT_COMPUTE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Graphics pipeline
 * \{ */

void VKContext::bind_graphics_pipeline(const GPUPrimType prim_type,
                                       const VKVertexAttributeObject &vertex_attribute_object)
{
  VKShader *shader = unwrap(this->shader);
  BLI_assert(shader);
  shader->update_graphics_pipeline(*this, prim_type, vertex_attribute_object);

  VKPipeline &pipeline = shader->pipeline_get();
  pipeline.update_and_bind(
      *this, shader->vk_pipeline_layout_get(), VK_PIPELINE_BIND_POINT_GRAPHICS);
}

/** \} */

}  // namespace blender::gpu
