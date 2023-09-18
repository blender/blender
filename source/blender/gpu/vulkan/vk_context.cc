/* SPDX-FileCopyrightText: 2022 Blender Authors
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
  if (surface_texture_) {
    GPU_texture_free(surface_texture_);
    surface_texture_ = nullptr;
  }
  VKBackend::get().device_.context_unregister(*this);

  delete imm;
  imm = nullptr;
}

void VKContext::sync_backbuffer()
{
  if (ghost_context_) {
    VKDevice &device = VKBackend::get().device_;
    if (!command_buffer_.is_initialized()) {
      command_buffer_.init(device);
      command_buffer_.begin_recording();
      device.init_dummy_buffer(*this);
    }
    device.descriptor_pools_get().reset();
  }

  if (ghost_window_) {
    GHOST_VulkanSwapChainData swap_chain_data = {};
    GHOST_GetVulkanSwapChainFormat((GHOST_WindowHandle)ghost_window_, &swap_chain_data);

    const bool reset_framebuffer = swap_chain_format_ != swap_chain_data.format ||
                                   vk_extent_.width != swap_chain_data.extent.width ||
                                   vk_extent_.height != swap_chain_data.extent.height;
    if (reset_framebuffer) {
      if (has_active_framebuffer()) {
        deactivate_framebuffer();
      }
      if (surface_texture_) {
        GPU_texture_free(surface_texture_);
        surface_texture_ = nullptr;
      }
      surface_texture_ = GPU_texture_create_2d("back-left",
                                               swap_chain_data.extent.width,
                                               swap_chain_data.extent.height,
                                               1,
                                               to_gpu_format(swap_chain_data.format),
                                               GPU_TEXTURE_USAGE_ATTACHMENT,
                                               nullptr);

      back_left->attachment_set(GPU_FB_COLOR_ATTACHMENT0,
                                GPU_ATTACHMENT_TEXTURE(surface_texture_));

      back_left->bind(false);

      swap_chain_format_ = swap_chain_data.format;
      vk_extent_ = swap_chain_data.extent;
    }
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

void VKContext::begin_frame() {}

void VKContext::end_frame() {}

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
  framebuffer.update_size();
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
  command_buffer_.end_render_pass(*framebuffer);
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

/* -------------------------------------------------------------------- */
/** \name Graphics pipeline
 * \{ */

void VKContext::swap_buffers_pre_callback(const GHOST_VulkanSwapChainData *swap_chain_data)
{
  VKContext *context = VKContext::get();
  BLI_assert(context);
  context->swap_buffers_pre_handler(*swap_chain_data);
}

void VKContext::swap_buffers_post_callback()
{
  VKContext *context = VKContext::get();
  BLI_assert(context);
  context->swap_buffers_post_handler();
}

void VKContext::swap_buffers_pre_handler(const GHOST_VulkanSwapChainData &swap_chain_data)
{
  VKFrameBuffer &framebuffer = *unwrap(back_left);

  VKTexture wrapper("display_texture");
  wrapper.init(swap_chain_data.image,
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               to_gpu_format(swap_chain_data.format));
  wrapper.layout_ensure(*this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  framebuffer.color_attachment_layout_ensure(*this, 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  VKTexture *color_attachment = unwrap(unwrap(framebuffer.color_tex(0)));
  color_attachment->layout_ensure(*this, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  VkImageBlit image_blit = {};
  image_blit.srcOffsets[0] = {0, int32_t(swap_chain_data.extent.height) - 1, 0};
  image_blit.srcOffsets[1] = {int32_t(swap_chain_data.extent.width), 0, 1};
  image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_blit.srcSubresource.mipLevel = 0;
  image_blit.srcSubresource.baseArrayLayer = 0;
  image_blit.srcSubresource.layerCount = 1;

  image_blit.dstOffsets[0] = {0, 0, 0};
  image_blit.dstOffsets[1] = {
      int32_t(swap_chain_data.extent.width), int32_t(swap_chain_data.extent.height), 1};
  image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_blit.dstSubresource.mipLevel = 0;
  image_blit.dstSubresource.baseArrayLayer = 0;
  image_blit.dstSubresource.layerCount = 1;

  command_buffer_.blit(wrapper,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       *color_attachment,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       Span<VkImageBlit>(&image_blit, 1));
  wrapper.layout_ensure(*this, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  command_buffer_.submit();
}

void VKContext::swap_buffers_post_handler()
{
  sync_backbuffer();
}

/** \} */

}  // namespace blender::gpu
