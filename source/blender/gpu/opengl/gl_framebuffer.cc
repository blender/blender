/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_string.h"

#include "BKE_global.hh"

#include "gl_backend.hh"
#include "gl_debug.hh"
#include "gl_state.hh"
#include "gl_texture.hh"

#include "gl_framebuffer.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

GLFrameBuffer::GLFrameBuffer(const char *name) : FrameBuffer(name)
{
  /* Just-In-Time init. See #GLFrameBuffer::init(). */
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
  set_color_attachment_bit(GPU_FB_COLOR_ATTACHMENT0, true);
  /* Never update an internal frame-buffer. */
  dirty_attachments_ = false;
  width_ = w;
  height_ = h;
  srgb_ = false;

  viewport_[0][0] = scissor_[0] = 0;
  viewport_[0][1] = scissor_[1] = 0;
  viewport_[0][2] = scissor_[2] = w;
  viewport_[0][3] = scissor_[3] = h;

  if (fbo_id_) {
    debug::object_label(GL_FRAMEBUFFER, fbo_id_, name_);
  }
}

GLFrameBuffer::~GLFrameBuffer()
{
  if (context_ == nullptr) {
    return;
  }

  /* Context might be partially freed. This happens when destroying the window frame-buffers. */
  if (context_ == Context::get()) {
    glDeleteFramebuffers(1, &fbo_id_);
  }
  else {
    context_->fbo_free(fbo_id_);
  }
  /* Restore default frame-buffer if this frame-buffer was bound. */
  if (context_->active_fb == this && context_->back_left != this) {
    /* If this assert triggers it means the frame-buffer is being freed while in use by another
     * context which, by the way, is TOTALLY UNSAFE! */
    BLI_assert(context_ == Context::get());
    GPU_framebuffer_restore();
  }
}

void GLFrameBuffer::init()
{
  context_ = GLContext::get();
  state_manager_ = static_cast<GLStateManager *>(context_->state_manager);
  glGenFramebuffers(1, &fbo_id_);
  /* Binding before setting the label is needed on some drivers.
   * This is not an issue since we call this function only before binding. */
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_id_);

  debug::object_label(GL_FRAMEBUFFER, fbo_id_, name_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Config
 * \{ */

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

  const char *format = "gpu::FrameBuffer: %s status %s\n";

  if (err_out) {
    BLI_snprintf(err_out, 256, format, this->name_, err);
  }
  else {
    fprintf(stderr, format, this->name_, err);
  }

  return false;
}

void GLFrameBuffer::update_attachments()
{
  /* Default frame-buffers cannot have attachments. */
  BLI_assert(immutable_ == false);

  /* First color texture OR the depth texture if no color is attached.
   * Used to determine frame-buffer color-space and dimensions. */
  GPUAttachmentType first_attachment = GPU_FB_MAX_ATTACHMENT;
  /* NOTE: Inverse iteration to get the first color texture. */
  for (GPUAttachmentType type = GPU_FB_MAX_ATTACHMENT - 1; type >= 0; --type) {
    GPUAttachment &attach = attachments_[type];
    GLenum gl_attachment = to_gl(type);

    if (type >= GPU_FB_COLOR_ATTACHMENT0) {
      gl_attachments_[type - GPU_FB_COLOR_ATTACHMENT0] = (attach.tex) ? gl_attachment : GL_NONE;
      first_attachment = (attach.tex) ? type : first_attachment;
    }
    else if (first_attachment == GPU_FB_MAX_ATTACHMENT) {
      /* Only use depth texture to get information if there is no color attachment. */
      first_attachment = (attach.tex) ? type : first_attachment;
    }

    if (attach.tex == nullptr) {
      glFramebufferTexture(GL_FRAMEBUFFER, gl_attachment, 0, 0);
      continue;
    }
    GLuint gl_tex = static_cast<GLTexture *>(attach.tex)->tex_id_;
    if (attach.layer > -1 && GPU_texture_is_cube(attach.tex) && !GPU_texture_is_array(attach.tex))
    {
      /* Could be avoided if ARB_direct_state_access is required. In this case
       * #glFramebufferTextureLayer would bind the correct face. */
      GLenum gl_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + attach.layer;
      glFramebufferTexture2D(GL_FRAMEBUFFER, gl_attachment, gl_target, gl_tex, attach.mip);
    }
    else if (attach.layer > -1) {
      glFramebufferTextureLayer(GL_FRAMEBUFFER, gl_attachment, gl_tex, attach.mip, attach.layer);
    }
    else {
      /* The whole texture level is attached. The frame-buffer is potentially layered. */
      glFramebufferTexture(GL_FRAMEBUFFER, gl_attachment, gl_tex, attach.mip);
    }
    /* We found one depth buffer type. Stop here, otherwise we would
     * override it by setting GPU_FB_DEPTH_ATTACHMENT */
    if (type == GPU_FB_DEPTH_STENCIL_ATTACHMENT) {
      break;
    }
  }

  if (GLContext::unused_fb_slot_workaround) {
    /* Fill normally un-occupied slots to avoid rendering artifacts on some hardware. */
    GLuint gl_tex = 0;
    /* NOTE: Inverse iteration to get the first color texture. */
    for (int i = ARRAY_SIZE(gl_attachments_) - 1; i >= 0; --i) {
      GPUAttachmentType type = GPU_FB_COLOR_ATTACHMENT0 + i;
      GPUAttachment &attach = attachments_[type];
      if (attach.tex != nullptr) {
        gl_tex = static_cast<GLTexture *>(attach.tex)->tex_id_;
      }
      else if (gl_tex != 0) {
        GLenum gl_attachment = to_gl(type);
        gl_attachments_[i] = gl_attachment;
        glFramebufferTexture(GL_FRAMEBUFFER, gl_attachment, gl_tex, 0);
      }
    }
  }

  if (first_attachment != GPU_FB_MAX_ATTACHMENT) {
    GPUAttachment &attach = attachments_[first_attachment];
    int size[3];
    GPU_texture_get_mipmap_size(attach.tex, attach.mip, size);
    this->size_set(size[0], size[1]);
    srgb_ = (GPU_texture_format(attach.tex) == TextureFormat::SRGBA_8_8_8_8);
  }
  else {
    /* Empty frame-buffer. */
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, width_);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, height_);
  }

  dirty_attachments_ = false;

  glDrawBuffers(ARRAY_SIZE(gl_attachments_), gl_attachments_);

  if (G.debug & G_DEBUG_GPU) {
    BLI_assert(this->check(nullptr));
  }
}

void GLFrameBuffer::subpass_transition_impl(const GPUAttachmentState depth_attachment_state,
                                            Span<GPUAttachmentState> color_attachment_states)
{
  GPU_depth_mask(depth_attachment_state == GPU_ATTACHMENT_WRITE);

  bool any_read = false;
  for (auto attachment : color_attachment_states.index_range()) {
    if (attachment == GPU_ATTACHMENT_READ) {
      any_read = true;
      break;
    }
  }

  if (GLContext::framebuffer_fetch_support) {
    if (any_read) {
      glFramebufferFetchBarrierEXT();
    }
  }
  else if (GLContext::texture_barrier_support) {
    if (any_read) {
      glTextureBarrier();
    }

    GLenum attachments[GPU_FB_MAX_COLOR_ATTACHMENT] = {GL_NONE};
    for (int i : color_attachment_states.index_range()) {
      GPUAttachmentType type = GPU_FB_COLOR_ATTACHMENT0 + i;
      gpu::Texture *attach_tex = this->attachments_[type].tex;
      if (color_attachment_states[i] == GPU_ATTACHMENT_READ) {
        tmp_detached_[type] = this->attachments_[type]; /* Bypass feedback loop check. */
        GPU_texture_bind_ex(attach_tex, GPUSamplerState::default_sampler(), i);
      }
      else {
        tmp_detached_[type] = GPU_ATTACHMENT_NONE;
      }
      bool attach_write = color_attachment_states[i] == GPU_ATTACHMENT_WRITE;
      attachments[i] = (attach_tex && attach_write) ? to_gl(type) : GL_NONE;
    }
    /* We have to use `glDrawBuffers` instead of `glColorMaski` because the later is overwritten
     * by the `GLStateManager`. */
    /* WATCH(fclem): This modifies the frame-buffer state without setting `dirty_attachments_`. */
    glDrawBuffers(ARRAY_SIZE(attachments), attachments);
  }
  else {
    /* The only way to have correct visibility without extensions and ensure defined behavior, is
     * to unbind the textures and update the frame-buffer. This is a slow operation but that's all
     * we can do to emulate the sub-pass input. */
    /* TODO(@fclem): Could avoid the frame-buffer reconfiguration by creating multiple
     * frame-buffers internally. */
    for (int i : color_attachment_states.index_range()) {
      GPUAttachmentType type = GPU_FB_COLOR_ATTACHMENT0 + i;

      if (color_attachment_states[i] == GPU_ATTACHMENT_WRITE) {
        if (tmp_detached_[type].tex != nullptr) {
          /* Re-attach previous read attachments. */
          this->attachment_set(type, tmp_detached_[type]);
          tmp_detached_[type] = GPU_ATTACHMENT_NONE;
        }
      }
      else if (color_attachment_states[i] == GPU_ATTACHMENT_READ) {
        tmp_detached_[type] = this->attachments_[type];
        tmp_detached_[type].tex->detach_from(this);
        GPU_texture_bind_ex(tmp_detached_[type].tex, GPUSamplerState::default_sampler(), i);
      }
    }
    if (dirty_attachments_) {
      this->update_attachments();
    }
  }
}

void GLFrameBuffer::attachment_set_loadstore_op(GPUAttachmentType type, GPULoadStore ls)
{
  BLI_assert(context_->active_fb == this);

  /* TODO(fclem): Add support for other ops. */
  if (ls.load_action == GPULoadOp::GPU_LOADACTION_CLEAR) {
    if (tmp_detached_[type].tex != nullptr) {
      /* #GPULoadStore is used to define the frame-buffer before it is used for rendering.
       * Binding back unattached attachment makes its state undefined. This is described by the
       * documentation and the user-land code should specify a sub-pass at the start of the drawing
       * to explicitly set attachment state. */
      if (GLContext::framebuffer_fetch_support) {
        /* NOOP. */
      }
      else if (GLContext::texture_barrier_support) {
        /* Reset default attachment state. */
        for (int i : IndexRange(ARRAY_SIZE(tmp_detached_))) {
          tmp_detached_[i] = GPU_ATTACHMENT_NONE;
        }
        glDrawBuffers(ARRAY_SIZE(gl_attachments_), gl_attachments_);
      }
      else {
        tmp_detached_[type] = GPU_ATTACHMENT_NONE;
        this->attachment_set(type, tmp_detached_[type]);
        this->update_attachments();
      }
    }
    clear_attachment(type, GPU_DATA_FLOAT, ls.clear_value);
  }
}

void GLFrameBuffer::apply_state()
{
  if (dirty_state_ == false) {
    return;
  }

  if (multi_viewport_ == false) {
    glViewport(UNPACK4(viewport_[0]));
  }
  else {
    /* Great API you have there! You have to convert to float values for setting int viewport
     * values. **Audible Facepalm** */
    float viewports_f[GPU_MAX_VIEWPORTS][4];
    for (int i = 0; i < GPU_MAX_VIEWPORTS; i++) {
      for (int j = 0; j < 4; j++) {
        viewports_f[i][j] = viewport_[i][j];
      }
    }
    glViewportArrayv(0, GPU_MAX_VIEWPORTS, viewports_f[0]);
  }
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

  if (context_ != GLContext::get()) {
    BLI_assert_msg(0, "Trying to use the same frame-buffer in multiple context");
    return;
  }

  if (context_->active_fb != this) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id_);
    /* Internal frame-buffers have only one color output and needs to be set every time. */
    if (immutable_ && fbo_id_ == 0) {
      glDrawBuffer(gl_attachments_[0]);
    }
  }

  if (!GLContext::texture_barrier_support && !GLContext::framebuffer_fetch_support) {
    for (int index : IndexRange(GPU_FB_MAX_ATTACHMENT)) {
      tmp_detached_[index] = GPU_ATTACHMENT_NONE;
    }
  }

  if (dirty_attachments_) {
    this->update_attachments();
    this->viewport_reset();
    this->scissor_reset();
  }

  if (context_->active_fb != this || enabled_srgb_ != enabled_srgb) {
    enabled_srgb_ = enabled_srgb;
    if (enabled_srgb && srgb_) {
      glEnable(GL_FRAMEBUFFER_SRGB);
    }
    else {
      glDisable(GL_FRAMEBUFFER_SRGB);
    }
    Shader::set_framebuffer_srgb_target(enabled_srgb && srgb_);
  }

  if (context_->active_fb != this) {
    context_->active_fb = this;
    state_manager_->active_fb = this;
    dirty_state_ = true;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operations.
 * \{ */

void GLFrameBuffer::clear(GPUFrameBufferBits buffers,
                          const float clear_col[4],
                          float clear_depth,
                          uint clear_stencil)
{
  BLI_assert(GLContext::get() == context_);
  BLI_assert(context_->active_fb == this);

  /* Save and restore the state. */
  GPUWriteMask write_mask = GPU_write_mask_get();
  uint stencil_mask = GPU_stencil_mask_get();
  GPUStencilTest stencil_test = GPU_stencil_test_get();

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

void GLFrameBuffer::clear_attachment(GPUAttachmentType type,
                                     eGPUDataFormat data_format,
                                     const void *clear_value)
{
  BLI_assert(GLContext::get() == context_);
  BLI_assert(context_->active_fb == this);

  /* Save and restore the state. */
  GPUWriteMask write_mask = GPU_write_mask_get();
  GPU_color_mask(true, true, true, true);
  bool depth_mask = GPU_depth_mask_get();
  GPU_depth_mask(true);

  context_->state_manager->apply_state();

  if (type == GPU_FB_DEPTH_STENCIL_ATTACHMENT) {
    BLI_assert(data_format == GPU_DATA_UINT_24_8_DEPRECATED);
    float depth = ((*(uint32_t *)clear_value) & 0x00FFFFFFu) / float(0x00FFFFFFu);
    int stencil = ((*(uint32_t *)clear_value) >> 24);
    glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil);
  }
  else if (type == GPU_FB_DEPTH_ATTACHMENT) {
    if (data_format == GPU_DATA_FLOAT) {
      glClearBufferfv(GL_DEPTH, 0, (GLfloat *)clear_value);
    }
    else if (data_format == GPU_DATA_UINT) {
      float depth = *(uint32_t *)clear_value / float(0xFFFFFFFFu);
      glClearBufferfv(GL_DEPTH, 0, &depth);
    }
    else {
      BLI_assert_msg(0, "Unhandled data format");
    }
  }
  else {
    int slot = type - GPU_FB_COLOR_ATTACHMENT0;
    switch (data_format) {
      case GPU_DATA_FLOAT:
        glClearBufferfv(GL_COLOR, slot, (GLfloat *)clear_value);
        break;
      case GPU_DATA_UINT:
        glClearBufferuiv(GL_COLOR, slot, (GLuint *)clear_value);
        break;
      case GPU_DATA_INT:
        glClearBufferiv(GL_COLOR, slot, (GLint *)clear_value);
        break;
      default:
        BLI_assert_msg(0, "Unhandled data format");
        break;
    }
  }

  GPU_write_mask(write_mask);
  GPU_depth_mask(depth_mask);
}

void GLFrameBuffer::clear_multi(const float (*clear_cols)[4])
{
  /* WATCH: This can easily access clear_cols out of bounds it clear_cols is not big enough for
   * all attachments.
   * TODO(fclem): fix this insecurity? */
  int type = GPU_FB_COLOR_ATTACHMENT0;
  for (int i = 0; type < GPU_FB_MAX_ATTACHMENT; i++, type++) {
    if (attachments_[type].tex != nullptr) {
      this->clear_attachment(GPU_FB_COLOR_ATTACHMENT0 + i, GPU_DATA_FLOAT, clear_cols[i]);
    }
  }
}

void GLFrameBuffer::read(GPUFrameBufferBits plane,
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
      BLI_assert_msg(
          this->attachments_[GPU_FB_DEPTH_ATTACHMENT].tex != nullptr ||
              this->attachments_[GPU_FB_DEPTH_STENCIL_ATTACHMENT].tex != nullptr,
          "GPUFramebuffer: Error: Trying to read depth without a depth buffer attached.");
      break;
    case GPU_COLOR_BIT:
      BLI_assert_msg(
          mode != GL_NONE,
          "GPUFramebuffer: Error: Trying to read a color slot without valid attachment.");
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
      fprintf(stderr, "GPUFramebuffer: Error: Trying to read more than one frame-buffer plane.");
      return;
  }

  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_id_);
  glReadBuffer(mode);
  glReadPixels(UNPACK4(area), format, type, r_data);
}

void GLFrameBuffer::blit_to(
    GPUFrameBufferBits planes, int src_slot, FrameBuffer *dst_, int dst_slot, int x, int y)
{
  GLFrameBuffer *src = this;
  GLFrameBuffer *dst = static_cast<GLFrameBuffer *>(dst_);

  /* Frame-buffers must be up to date. This simplify this function. */
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
  /* Ensure previous buffer is restored. */
  context_->active_fb = dst;
}

/** \} */

}  // namespace blender::gpu
