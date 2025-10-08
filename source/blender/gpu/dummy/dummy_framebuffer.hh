/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_framebuffer_private.hh"

namespace blender::gpu {

class DummyFrameBuffer : public FrameBuffer {
 public:
  DummyFrameBuffer(const char *name) : FrameBuffer(name) {}
  void bind(bool /*enabled_srgb*/) override {}
  bool check(char /*err_out*/[256]) override
  {
    return true;
  }
  void clear(GPUFrameBufferBits /*buffers*/,
             const float /*clear_color*/[4],
             float /*clear_depth*/,
             uint /*clear_stencil*/) override
  {
  }
  void clear_multi(const float (* /*clear_color*/)[4]) override {}
  void clear_attachment(GPUAttachmentType /*type*/,
                        eGPUDataFormat /*data_format*/,
                        const void * /*clear_value*/) override
  {
  }

  void attachment_set_loadstore_op(GPUAttachmentType /*type*/, GPULoadStore /*ls*/) override {}

  void subpass_transition_impl(const GPUAttachmentState /*depth_attachment_state*/,
                               Span<GPUAttachmentState> /*color_attachment_states*/) override {};

  void read(GPUFrameBufferBits /*planes*/,
            eGPUDataFormat /*format*/,
            const int /*area*/[4],
            int /*channel_len*/,
            int /*slot*/,
            void * /*r_data*/) override
  {
  }

  void blit_to(GPUFrameBufferBits /*planes*/,
               int /*src_slot*/,
               FrameBuffer * /*dst*/,
               int /*dst_slot*/,
               int /*dst_offset_x*/,
               int /*dst_offset_y*/) override
  {
  }
};

}  // namespace blender::gpu
