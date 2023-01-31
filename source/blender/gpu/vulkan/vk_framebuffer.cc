/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_framebuffer.hh"

namespace blender::gpu {

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
