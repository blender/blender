/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Encapsulation of Framebuffer states (attached textures, viewport, scissors).
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "glew-mx.h"

#include "gpu_framebuffer_private.hh"

namespace blender::gpu {

class GLStateManager;

/**
 * Implementation of FrameBuffer object using OpenGL.
 */
class GLFrameBuffer : public FrameBuffer {
  /* For debugging purpose. */
  friend class GLTexture;

 private:
  /** OpenGL handle. */
  GLuint fbo_id_ = 0;
  /** Context the handle is from. Framebuffers are not shared accros contexts. */
  GLContext *context_ = NULL;
  /** State Manager of the same contexts. */
  GLStateManager *state_manager_ = NULL;
  /** Copy of the GL state. Contains ONLY color attachments enums for slot binding. */
  GLenum gl_attachments_[GPU_FB_MAX_COLOR_ATTACHMENT];
  /** Internal framebuffers are immutable. */
  bool immutable_;
  /** True is the framebuffer has its first color target using the GPU_SRGB8_A8 format. */
  bool srgb_;
  /** True is the framebuffer has been bound using the GL_FRAMEBUFFER_SRGB feature. */
  bool enabled_srgb_ = false;

 public:
  /**
   * Create a conventional framebuffer to attach texture to.
   */
  GLFrameBuffer(const char *name);

  /**
   * Special frame-buffer encapsulating internal window frame-buffer.
   *  (i.e.: #GL_FRONT_LEFT, #GL_BACK_RIGHT, ...)
   * \param ctx: Context the handle is from.
   * \param target: The internal GL name (i.e: #GL_BACK_LEFT).
   * \param fbo: The (optional) already created object for some implementation. Default is 0.
   * \param w: Buffer width.
   * \param h: Buffer height.
   */
  GLFrameBuffer(const char *name, GLContext *ctx, GLenum target, GLuint fbo, int w, int h);

  ~GLFrameBuffer();

  void bind(bool enabled_srgb) override;

  bool check(char err_out[256]) override;

  void clear(eGPUFrameBufferBits buffers,
             const float clear_col[4],
             float clear_depth,
             uint clear_stencil) override;
  void clear_multi(const float (*clear_cols)[4]) override;
  void clear_attachment(GPUAttachmentType type,
                        eGPUDataFormat data_format,
                        const void *clear_value) override;

  void read(eGPUFrameBufferBits planes,
            eGPUDataFormat format,
            const int area[4],
            int channel_len,
            int slot,
            void *r_data) override;

  void blit_to(eGPUFrameBufferBits planes,
               int src_slot,
               FrameBuffer *dst,
               int dst_slot,
               int dst_offset_x,
               int dst_offset_y) override;

  void apply_state(void);

 private:
  void init(void);
  void update_attachments(void);
  void update_drawbuffers(void);

  MEM_CXX_CLASS_ALLOC_FUNCS("GLFrameBuffer");
};

/* -------------------------------------------------------------------- */
/** \name Enums Conversion
 * \{ */

static inline GLenum to_gl(const GPUAttachmentType type)
{
#define ATTACHMENT(X) \
  case GPU_FB_##X: { \
    return GL_##X; \
  } \
    ((void)0)

  switch (type) {
    ATTACHMENT(DEPTH_ATTACHMENT);
    ATTACHMENT(DEPTH_STENCIL_ATTACHMENT);
    ATTACHMENT(COLOR_ATTACHMENT0);
    ATTACHMENT(COLOR_ATTACHMENT1);
    ATTACHMENT(COLOR_ATTACHMENT2);
    ATTACHMENT(COLOR_ATTACHMENT3);
    ATTACHMENT(COLOR_ATTACHMENT4);
    ATTACHMENT(COLOR_ATTACHMENT5);
    default:
      BLI_assert(0);
      return GL_COLOR_ATTACHMENT0;
  }
#undef ATTACHMENT
}

static inline GLbitfield to_gl(const eGPUFrameBufferBits bits)
{
  GLbitfield mask = 0;
  mask |= (bits & GPU_DEPTH_BIT) ? GL_DEPTH_BUFFER_BIT : 0;
  mask |= (bits & GPU_STENCIL_BIT) ? GL_STENCIL_BUFFER_BIT : 0;
  mask |= (bits & GPU_COLOR_BIT) ? GL_COLOR_BUFFER_BIT : 0;
  return mask;
}

/** \} */

}  // namespace blender::gpu
