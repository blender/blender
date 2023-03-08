/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_framebuffer.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

VKFrameBuffer::VKFrameBuffer(const char *name) : FrameBuffer(name)
{
  immutable_ = false;
}

VKFrameBuffer::VKFrameBuffer(const char *name,
                             VkFramebuffer vk_framebuffer,
                             VkRenderPass /*vk_render_pass*/,
                             VkExtent2D vk_extent)
    : FrameBuffer(name)
{
  immutable_ = true;
  /* Never update an internal frame-buffer. */
  dirty_attachments_ = false;
  width_ = vk_extent.width;
  height_ = vk_extent.height;
  vk_framebuffer_ = vk_framebuffer;

  viewport_[0] = scissor_[0] = 0;
  viewport_[1] = scissor_[1] = 0;
  viewport_[2] = scissor_[2] = width_;
  viewport_[3] = scissor_[3] = height_;
}

VKFrameBuffer::~VKFrameBuffer()
{
  if (!immutable_ && vk_framebuffer_ != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(vk_device_, vk_framebuffer_, NULL);
  }
}

/** \} */

void VKFrameBuffer::bind(bool /*enabled_srgb*/)
{
}

bool VKFrameBuffer::check(char /*err_out*/[256])
{
  return false;
}

void VKFrameBuffer::clear(eGPUFrameBufferBits /*buffers*/,
                          const float /*clear_col*/[4],
                          float /*clear_depth*/,
                          uint /*clear_stencil*/)
{
}

void VKFrameBuffer::clear_multi(const float (*/*clear_col*/)[4])
{
}

void VKFrameBuffer::clear_attachment(GPUAttachmentType /*type*/,
                                     eGPUDataFormat /*data_format*/,
                                     const void * /*clear_value*/)
{
}

void VKFrameBuffer::attachment_set_loadstore_op(GPUAttachmentType /*type*/,
                                                eGPULoadOp /*load_action*/,
                                                eGPUStoreOp /*store_action*/)
{
}

void VKFrameBuffer::read(eGPUFrameBufferBits /*planes*/,
                         eGPUDataFormat /*format*/,
                         const int /*area*/[4],
                         int /*channel_len*/,
                         int /*slot*/,
                         void * /*r_data*/)
{
}

void VKFrameBuffer::blit_to(eGPUFrameBufferBits /*planes*/,
                            int /*src_slot*/,
                            FrameBuffer * /*dst*/,
                            int /*dst_slot*/,
                            int /*dst_offset_x*/,
                            int /*dst_offset_y*/)
{
}

}  // namespace blender::gpu
