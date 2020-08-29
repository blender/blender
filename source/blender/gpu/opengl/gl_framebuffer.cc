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
 */

#include "BKE_global.h"

#include "GPU_extensions.h"

#include "gl_backend.hh"
#include "gl_framebuffer.hh"
#include "gl_state.hh"
#include "gl_texture.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

GLFrameBuffer::GLFrameBuffer(const char *name) : FrameBuffer(name)
{
  /* Just-In-Time init. See GLFrameBuffer::init(). */
  immutable_ = false;
  fbo_id_ = 0;
}

GLFrameBuffer::GLFrameBuffer(
    const char *name, GLContext *ctx, GLenum target, GLuint fbo, int w, int h)
    : FrameBuffer(name)
{
  context_ = ctx;
  state_manager_ = static_cast<GLStateManager *>(ctx->state_manager);
  immutable_ = true;
  fbo_id_ = fbo;
  gl_attachments_[0] = target;
  /* Never update an internal framebuffer. */
  dirty_attachments_ = false;
  width_ = w;
  height_ = h;
  srgb_ = false;

  viewport_[0] = scissor_[0] = 0;
  viewport_[1] = scissor_[1] = 0;
  viewport_[2] = scissor_[2] = w;
  viewport_[3] = scissor_[3] = h;

#ifndef __APPLE__
  if (fbo_id_ && (G.debug & G_DEBUG_GPU) && (GLEW_VERSION_4_3 || GLEW_KHR_debug)) {
    char sh_name[32];
    SNPRINTF(sh_name, "FrameBuffer-%s", name);
    glObjectLabel(GL_FRAMEBUFFER, fbo_id_, -1, sh_name);
  }
#endif
}

GLFrameBuffer::~GLFrameBuffer()
{
  if (context_ != NULL) {
    context_->fbo_free(fbo_id_);
    /* Restore default framebuffer if this framebuffer was bound. */
    if (context_->active_fb == this && context_->back_left != this) {
      /* If this assert triggers it means the framebuffer is being freed while in use by another
       * context which, by the way, is TOTALLY UNSAFE!!!  */
      BLI_assert(context_ == GPU_context_active_get());
      GPU_framebuffer_restore();
    }
  }
}

void GLFrameBuffer::init(void)
{
  context_ = static_cast<GLContext *>(GPU_context_active_get());
  state_manager_ = static_cast<GLStateManager *>(context_->state_manager);
  glGenFramebuffers(1, &fbo_id_);

#ifndef __APPLE__
  if ((G.debug & G_DEBUG_GPU) && (GLEW_VERSION_4_3 || GLEW_KHR_debug)) {
    char sh_name[64];
    SNPRINTF(sh_name, "FrameBuffer-%s", name_);
    /* Binding before setting the label is needed on some drivers. */
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id_);
    glObjectLabel(GL_FRAMEBUFFER, fbo_id_, -1, sh_name);
  }
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Config
 * \{ */

/* This is a rather slow operation. Don't check in normal cases. */
bool GLFrameBuffer::check(char err_out[256])
{
  this->bind(true);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

#define FORMAT_STATUS(X) \
  case X: { \
    err = #X; \
    break; \
  }

  const char *err;
  switch (status) {
    FORMAT_STATUS(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
    FORMAT_STATUS(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
    FORMAT_STATUS(GL_FRAMEBUFFER_UNSUPPORTED);
    FORMAT_STATUS(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
    FORMAT_STATUS(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER);
    FORMAT_STATUS(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE);
    FORMAT_STATUS(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS);
    FORMAT_STATUS(GL_FRAMEBUFFER_UNDEFINED);
    case GL_FRAMEBUFFER_COMPLETE:
      return true;
    default:
      err = "unknown";
      break;
  }

#undef FORMAT_STATUS

  const char *format = "GPUFrameBuffer: framebuffer status %s\n";

  if (err_out) {
    BLI_snprintf(err_out, 256, format, err);
  }
  else {
    fprintf(stderr, format, err);
  }

  return false;
}

void GLFrameBuffer::update_attachments(void)
{
  /* Default framebuffers cannot have attachements. */
  BLI_assert(immutable_ == false);

  /* First color texture OR the depth texture if no color is attached.
   * Used to determine framebuffer colorspace and dimensions. */
  GPUAttachmentType first_attachment = GPU_FB_MAX_ATTACHEMENT;
  /* NOTE: Inverse iteration to get the first color texture. */
  for (GPUAttachmentType type = GPU_FB_MAX_ATTACHEMENT - 1; type >= 0; --type) {
    GPUAttachment &attach = attachments_[type];
    GLenum gl_attachment = to_gl(type);

    if (type >= GPU_FB_COLOR_ATTACHMENT0) {
      gl_attachments_[type - GPU_FB_COLOR_ATTACHMENT0] = (attach.tex) ? gl_attachment : GL_NONE;
      first_attachment = (attach.tex) ? type : first_attachment;
    }
    else if (first_attachment == GPU_FB_MAX_ATTACHEMENT) {
      /* Only use depth texture to get infos if there is no color attachment. */
      first_attachment = (attach.tex) ? type : first_attachment;
    }

    if (attach.tex == NULL) {
      glFramebufferTexture(GL_FRAMEBUFFER, gl_attachment, 0, 0);
      continue;
    }
    GLuint gl_tex = GPU_texture_opengl_bindcode(attach.tex);
    if (attach.layer > -1 && GPU_texture_cube(attach.tex) && !GPU_texture_array(attach.tex)) {
      /* Could be avoided if ARB_direct_state_access is required. In this case
       * glFramebufferTextureLayer would bind the correct face. */
      GLenum gl_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + attach.layer;
      glFramebufferTexture2D(GL_FRAMEBUFFER, gl_attachment, gl_target, gl_tex, attach.mip);
    }
    else if (attach.layer > -1) {
      glFramebufferTextureLayer(GL_FRAMEBUFFER, gl_attachment, gl_tex, attach.mip, attach.layer);
    }
    else {
      /* The whole texture level is attached. The framebuffer is potentially layered. */
      glFramebufferTexture(GL_FRAMEBUFFER, gl_attachment, gl_tex, attach.mip);
    }
    /* We found one depth buffer type. Stop here, otherwise we would
     * override it by setting GPU_FB_DEPTH_ATTACHMENT */
    if (type == GPU_FB_DEPTH_STENCIL_ATTACHMENT) {
      break;
    }
  }

  if (GPU_unused_fb_slot_workaround()) {
    /* Fill normally un-occupied slots to avoid rendering artifacts on some hardware. */
    GLuint gl_tex = 0;
    /* NOTE: Inverse iteration to get the first color texture. */
    for (int i = ARRAY_SIZE(gl_attachments_) - 1; i >= 0; --i) {
      GPUAttachmentType type = GPU_FB_COLOR_ATTACHMENT0 + i;
      GPUAttachment &attach = attachments_[type];
      if (attach.tex != NULL) {
        gl_tex = GPU_texture_opengl_bindcode(attach.tex);
      }
      else if (gl_tex != 0) {
        GLenum gl_attachment = to_gl(type);
        gl_attachments_[i] = gl_attachment;
        glFramebufferTexture(GL_FRAMEBUFFER, gl_attachment, gl_tex, 0);
      }
    }
  }

  if (first_attachment != GPU_FB_MAX_ATTACHEMENT) {
    GPUAttachment &attach = attachments_[first_attachment];
    int size[3];
    GPU_texture_get_mipmap_size(attach.tex, attach.mip, size);
    this->size_set(size[0], size[1]);
    srgb_ = (GPU_texture_format(attach.tex) == GPU_SRGB8_A8);
  }

  dirty_attachments_ = false;

  glDrawBuffers(ARRAY_SIZE(gl_attachments_), gl_attachments_);

  if (G.debug & G_DEBUG_GPU) {
    BLI_assert(this->check(NULL));
  }
}

void GLFrameBuffer::apply_state(void)
{
  if (dirty_state_ == false) {
    return;
  }

  glViewport(UNPACK4(viewport_));
  glScissor(UNPACK4(scissor_));

  if (scissor_test_) {
    glEnable(GL_SCISSOR_TEST);
  }
  else {
    glDisable(GL_SCISSOR_TEST);
  }

  dirty_state_ = false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

void GLFrameBuffer::bind(bool enabled_srgb)
{
  if (!immutable_ && fbo_id_ == 0) {
    this->init();
  }

  if (context_ != GPU_context_active_get()) {
    BLI_assert(!"Trying to use the same framebuffer in multiple context");
    return;
  }

  if (context_->active_fb != this) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id_);
    /* Internal framebuffers have only one color output and needs to be set everytime. */
    if (immutable_ && fbo_id_ == 0) {
      glDrawBuffer(gl_attachments_[0]);
    }
  }

  if (dirty_attachments_) {
    this->update_attachments();
    this->viewport_reset();
    this->scissor_reset();
  }

  if (context_->active_fb != this) {
    context_->active_fb = this;
    state_manager_->active_fb = this;
    dirty_state_ = true;

    if (enabled_srgb) {
      glEnable(GL_FRAMEBUFFER_SRGB);
    }
    else {
      glDisable(GL_FRAMEBUFFER_SRGB);
    }

    GPU_shader_set_framebuffer_srgb_target(enabled_srgb && srgb_);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operations.
 * \{ */

void GLFrameBuffer::clear(eGPUFrameBufferBits buffers,
                          const float clear_col[4],
                          float clear_depth,
                          uint clear_stencil)
{
  BLI_assert(GPU_context_active_get() == context_);
  BLI_assert(context_->active_fb == this);

  /* Save and restore the state. */
  eGPUWriteMask write_mask = GPU_write_mask_get();
  uint stencil_mask = GPU_stencil_mask_get();
  eGPUStencilTest stencil_test = GPU_stencil_test_get();

  if (buffers & GPU_COLOR_BIT) {
    GPU_color_mask(true, true, true, true);
    glClearColor(clear_col[0], clear_col[1], clear_col[2], clear_col[3]);
  }
  if (buffers & GPU_DEPTH_BIT) {
    GPU_depth_mask(true);
    glClearDepth(clear_depth);
  }
  if (buffers & GPU_STENCIL_BIT) {
    GPU_stencil_write_mask_set(0xFFu);
    GPU_stencil_test(GPU_STENCIL_ALWAYS);
    glClearStencil(clear_stencil);
  }

  context_->state_manager->apply_state();

  GLbitfield mask = to_gl(buffers);
  glClear(mask);

  if (buffers & (GPU_COLOR_BIT | GPU_DEPTH_BIT)) {
    GPU_write_mask(write_mask);
  }
  if (buffers & GPU_STENCIL_BIT) {
    GPU_stencil_write_mask_set(stencil_mask);
    GPU_stencil_test(stencil_test);
  }
}

void GLFrameBuffer::clear_multi(const float (*clear_cols)[4])
{
  BLI_assert(GPU_context_active_get() == context_);
  BLI_assert(context_->active_fb == this);

  /* Save and restore the state. */
  eGPUWriteMask write_mask = GPU_write_mask_get();
  GPU_color_mask(true, true, true, true);

  context_->state_manager->apply_state();

  /* WATCH: This can easilly access clear_cols out of bounds it clear_cols is not big enough for
   * all attachments.
   * TODO(fclem) fix this insecurity? */
  int type = GPU_FB_COLOR_ATTACHMENT0;
  for (int i = 0; type < GPU_FB_MAX_ATTACHEMENT; i++, type++) {
    if (attachments_[type].tex != NULL) {
      glClearBufferfv(GL_COLOR, i, clear_cols[i]);
    }
  }

  GPU_write_mask(write_mask);
}

void GLFrameBuffer::read(eGPUFrameBufferBits plane,
                         eGPUDataFormat data_format,
                         const int area[4],
                         int channel_len,
                         int slot,
                         void *r_data)
{
  GLenum format, type, mode;
  mode = gl_attachments_[slot];
  type = to_gl(data_format);

  switch (plane) {
    case GPU_DEPTH_BIT:
      format = GL_DEPTH_COMPONENT;
      break;
    case GPU_COLOR_BIT:
      format = channel_len_to_gl(channel_len);
      /* TODO: needed for selection buffers to work properly, this should be handled better. */
      if (format == GL_RED && type == GL_UNSIGNED_INT) {
        format = GL_RED_INTEGER;
      }
      break;
    case GPU_STENCIL_BIT:
      fprintf(stderr, "GPUFramebuffer: Error: Trying to read stencil bit. Unsupported.");
      return;
    default:
      fprintf(stderr, "GPUFramebuffer: Error: Trying to read more than one framebuffer plane.");
      return;
  }

  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_id_);
  glReadBuffer(mode);
  glReadPixels(UNPACK4(area), format, type, r_data);
}

/* Copy src at the give offset inside dst. */
void GLFrameBuffer::blit_to(
    eGPUFrameBufferBits planes, int src_slot, FrameBuffer *dst_, int dst_slot, int x, int y)
{
  GLFrameBuffer *src = this;
  GLFrameBuffer *dst = static_cast<GLFrameBuffer *>(dst_);

  /* Framebuffers must be up to date. This simplify this function. */
  if (src->dirty_attachments_) {
    src->bind(true);
  }
  if (dst->dirty_attachments_) {
    dst->bind(true);
  }

  glBindFramebuffer(GL_READ_FRAMEBUFFER, src->fbo_id_);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst->fbo_id_);

  if (planes & GPU_COLOR_BIT) {
    BLI_assert(src->immutable_ == false || src_slot == 0);
    BLI_assert(dst->immutable_ == false || dst_slot == 0);
    BLI_assert(src->gl_attachments_[src_slot] != GL_NONE);
    BLI_assert(dst->gl_attachments_[dst_slot] != GL_NONE);
    glReadBuffer(src->gl_attachments_[src_slot]);
    glDrawBuffer(dst->gl_attachments_[dst_slot]);
  }

  context_->state_manager->apply_state();

  int w = src->width_;
  int h = src->height_;
  GLbitfield mask = to_gl(planes);
  glBlitFramebuffer(0, 0, w, h, x, y, x + w, y + h, mask, GL_NEAREST);

  if (!dst->immutable_) {
    /* Restore the draw buffers. */
    glDrawBuffers(ARRAY_SIZE(dst->gl_attachments_), dst->gl_attachments_);
  }
}

/** \} */

}  // namespace blender::gpu