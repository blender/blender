/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_string.h"

#include "BKE_global.hh"

#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_framebuffer.hh"
#include "mtl_texture.hh"
#import <Availability.h>

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

MTLFrameBuffer::MTLFrameBuffer(MTLContext *ctx, const char *name) : FrameBuffer(name)
{

  context_ = ctx;
  is_dirty_ = true;
  is_loadstore_dirty_ = true;
  dirty_state_ctx_ = nullptr;
  has_pending_clear_ = false;
  colour_attachment_count_ = 0;
  enabled_srgb_ = false;
  srgb_ = false;

  for (int i = 0; i < GPU_FB_MAX_COLOR_ATTACHMENT; i++) {
    mtl_color_attachments_[i].used = false;
  }
  mtl_depth_attachment_.used = false;
  mtl_stencil_attachment_.used = false;

  for (int i = 0; i < MTL_FB_CONFIG_MAX; i++) {
    framebuffer_descriptor_[i] = [[MTLRenderPassDescriptor alloc] init];
    descriptor_dirty_[i] = true;
  }

  for (int i = 0; i < GPU_FB_MAX_COLOR_ATTACHMENT; i++) {
    colour_attachment_descriptors_[i] = [[MTLRenderPassColorAttachmentDescriptor alloc] init];
  }

  /* Initial state. */
  this->size_set(0, 0);
  this->viewport_reset();
  this->scissor_reset();
}

MTLFrameBuffer::~MTLFrameBuffer()
{
  /* If FrameBuffer is associated with a currently open RenderPass, end. */
  if (context_->main_command_buffer.get_active_framebuffer() == this) {
    context_->main_command_buffer.end_active_command_encoder();
  }

  /* Restore default frame-buffer if this frame-buffer was bound. */
  if (context_->active_fb == this && context_->back_left != this) {
    /* If this assert triggers it means the frame-buffer is being freed while in use by another
     * context which, by the way, is TOTALLY UNSAFE!!!  (Copy from GL behavior). */
    BLI_assert(context_ == MTLContext::get());
    GPU_framebuffer_restore();
  }

  /* Free Render Pass Descriptors. */
  for (int config = 0; config < MTL_FB_CONFIG_MAX; config++) {
    if (framebuffer_descriptor_[config] != nil) {
      [framebuffer_descriptor_[config] release];
      framebuffer_descriptor_[config] = nil;
    }
  }

  /* Free color attachment descriptors. */
  for (int i = 0; i < GPU_FB_MAX_COLOR_ATTACHMENT; i++) {
    if (colour_attachment_descriptors_[i] != nil) {
      [colour_attachment_descriptors_[i] release];
      colour_attachment_descriptors_[i] = nil;
    }
  }

  /* Remove attachments - release FB texture references. */
  this->remove_all_attachments();

  if (context_ == nullptr) {
    return;
  }
}

void MTLFrameBuffer::ensure_attachments_and_viewport()
{
  /* Ensure local MTLAttachment data is up to date.
   * NOTE: We only refresh viewport/scissor region when attachments are updated during bind.
   * This is to ensure state is consistent with the OpenGL backend. */
  if (dirty_attachments_) {
    this->update_attachments(true);
    this->viewport_reset();
    this->scissor_reset();
  }
}

void MTLFrameBuffer::bind(bool enabled_srgb)
{

  /* Verify Context is valid. */
  if (context_ != MTLContext::get()) {
    BLI_assert_msg(false, "Trying to use the same frame-buffer in multiple context's.");
    return;
  }

  /* Ensure local MTLAttachment data is up to date.
   * NOTE: We only refresh viewport/scissor region when attachments are updated during bind.
   * This is to ensure state is consistent with the OpenGL backend. */
  this->ensure_attachments_and_viewport();

  /* Ensure SRGB state is up-to-date and valid. */
  bool srgb_state_changed = enabled_srgb_ != enabled_srgb;
  if (context_->active_fb != this || srgb_state_changed) {
    if (srgb_state_changed) {
      this->mark_dirty();
    }
    enabled_srgb_ = enabled_srgb;
    Shader::set_framebuffer_srgb_target(enabled_srgb && srgb_);
  }

  /* Reset clear state on bind -- Clears and load/store ops are set after binding. */
  this->reset_clear_state();

  /* Bind to active context. */
  MTLContext *mtl_context = MTLContext::get();
  if (mtl_context) {
    mtl_context->framebuffer_bind(this);
    dirty_state_ = true;
  }
  else {
    MTL_LOG_WARNING("Attempting to bind FrameBuffer, but no context is active");
  }
}

bool MTLFrameBuffer::check(char err_out[256])
{
  /* Ensure local MTLAttachment data is up to date.
   * NOTE: We only refresh viewport/scissor region when attachments are updated during bind.
   * This is to ensure state is consistent with the OpenGL backend. */
  this->ensure_attachments_and_viewport();

  /* Ensure there is at least one attachment. */
  bool valid = (this->get_attachment_count() > 0 || this->has_depth_attachment() ||
                this->has_stencil_attachment());
  if (!valid) {
    const char *format = "Framebuffer %s does not have any attachments.\n";
    if (err_out) {
      BLI_snprintf(err_out, 256, format, name_);
    }
    else {
      MTL_LOG_ERROR(format, name_);
    }
    return false;
  }

  /* Ensure all attachments have identical dimensions. */
  /* Ensure all attachments are render-targets. */
  bool first = true;
  uint dim_x = 0;
  uint dim_y = 0;
  for (int col_att = 0; col_att < this->get_attachment_count(); col_att++) {
    MTLAttachment att = this->get_color_attachment(col_att);
    if (att.used) {
      if (att.texture->internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_ATTACHMENT) {
        if (first) {
          dim_x = att.texture->width_get();
          dim_y = att.texture->height_get();
          first = false;
        }
        else {
          if (dim_x != att.texture->width_get() || dim_y != att.texture->height_get()) {
            const char *format =
                "Framebuffer %s: Color attachment dimensions do not match those of previous "
                "attachment\n";
            if (err_out) {
              BLI_snprintf(err_out, 256, format, name_);
            }
            else {
              fprintf(stderr, format, name_);
              MTL_LOG_ERROR(format, name_);
            }
            return false;
          }
        }
      }
      else {
        const char *format =
            "Framebuffer %s: Color attachment texture does not have usage flag "
            "'GPU_TEXTURE_USAGE_ATTACHMENT'\n";
        if (err_out) {
          BLI_snprintf(err_out, 256, format, name_);
        }
        else {
          fprintf(stderr, format, name_);
          MTL_LOG_ERROR(format, name_);
        }
        return false;
      }
    }
  }
  MTLAttachment depth_att = this->get_depth_attachment();
  MTLAttachment stencil_att = this->get_stencil_attachment();
  if (depth_att.used) {
    if (first) {
      dim_x = depth_att.texture->width_get();
      dim_y = depth_att.texture->height_get();
      first = false;
      valid = (depth_att.texture->internal_gpu_image_usage_flags_ & GPU_TEXTURE_USAGE_ATTACHMENT);

      if (!valid) {
        const char *format =
            "Framebuffer %n: Depth attachment does not have usage "
            "'GPU_TEXTURE_USAGE_ATTACHMENT'\n";
        if (err_out) {
          BLI_snprintf(err_out, 256, format, name_);
        }
        else {
          fprintf(stderr, format, name_);
          MTL_LOG_ERROR(format, name_);
        }
        return false;
      }
    }
    else {
      if (dim_x != depth_att.texture->width_get() || dim_y != depth_att.texture->height_get()) {
        const char *format =
            "Framebuffer %n: Depth attachment dimensions do not match that of previous "
            "attachment\n";
        if (err_out) {
          BLI_snprintf(err_out, 256, format, name_);
        }
        else {
          fprintf(stderr, format, name_);
          MTL_LOG_ERROR(format, name_);
        }
        return false;
      }
    }
  }
  if (stencil_att.used) {
    if (first) {
      dim_x = stencil_att.texture->width_get();
      dim_y = stencil_att.texture->height_get();
      first = false;
      valid = (stencil_att.texture->internal_gpu_image_usage_flags_ &
               GPU_TEXTURE_USAGE_ATTACHMENT);
      if (!valid) {
        const char *format =
            "Framebuffer %s: Stencil attachment does not have usage "
            "'GPU_TEXTURE_USAGE_ATTACHMENT'\n";
        if (err_out) {
          BLI_snprintf(err_out, 256, format, name_);
        }
        else {
          fprintf(stderr, format, name_);
          MTL_LOG_ERROR(format, name_);
        }
        return false;
      }
    }
    else {
      if (dim_x != stencil_att.texture->width_get() || dim_y != stencil_att.texture->height_get())
      {
        const char *format =
            "Framebuffer %s: Stencil attachment dimensions do not match that of previous "
            "attachment";
        if (err_out) {
          BLI_snprintf(err_out, 256, format, name_);
        }
        else {
          fprintf(stderr, format, name_);
          MTL_LOG_ERROR(format, name_);
        }
        return false;
      }
    }
  }

  BLI_assert(valid);
  return valid;
}

void MTLFrameBuffer::force_clear()
{
  /* Perform clear by ending current and starting a new render pass. */
  MTLContext *mtl_context = MTLContext::get();
  MTLFrameBuffer *current_framebuffer = mtl_context->get_current_framebuffer();
  if (current_framebuffer) {
    BLI_assert(current_framebuffer == this);
    /* End current render-pass. */
    if (mtl_context->main_command_buffer.is_inside_render_pass()) {
      mtl_context->main_command_buffer.end_active_command_encoder();
    }
    mtl_context->ensure_begin_render_pass();
    BLI_assert(has_pending_clear_ == false);
  }
}

void MTLFrameBuffer::clear(GPUFrameBufferBits buffers,
                           const float clear_col[4],
                           float clear_depth,
                           uint clear_stencil)
{

  BLI_assert(MTLContext::get() == context_);
  BLI_assert(context_->active_fb == this);

  /* Ensure attachments are up to date. */
  this->update_attachments(true);

  /* If we had no previous clear pending, reset clear state. */
  if (!has_pending_clear_) {
    this->reset_clear_state();
  }

  /* Ensure we only clear if attachments exist for given buffer bits. */
  bool do_clear = false;
  if (buffers & GPU_COLOR_BIT) {
    for (int i = 0; i < colour_attachment_count_; i++) {
      this->set_color_attachment_clear_color(i, clear_col);
      do_clear = true;
    }
  }

  if (buffers & GPU_DEPTH_BIT) {
    this->set_depth_attachment_clear_value(clear_depth);
    do_clear = do_clear || this->has_depth_attachment();
  }
  if (buffers & GPU_STENCIL_BIT) {
    this->set_stencil_attachment_clear_value(clear_stencil);
    do_clear = do_clear || this->has_stencil_attachment();
  }

  if (do_clear) {
    has_pending_clear_ = true;

    /* Apply state before clear. */
    this->apply_state();

    /* TODO(Metal): Optimize - Currently force-clear always used. Consider moving clear state to
     * MTLTexture instead. */
    /* Force clear if RP is not yet active -- not the most efficient, but there is no distinction
     * between clears where no draws occur. Can optimize at the high-level by using explicit
     * load-store flags. */
    this->force_clear();
  }
}

void MTLFrameBuffer::clear_multi(const float (*clear_cols)[4])
{
  /* If we had no previous clear pending, reset clear state. */
  if (!has_pending_clear_) {
    this->reset_clear_state();
  }

  bool do_clear = false;
  for (int i = 0; i < this->get_attachment_limit(); i++) {
    if (this->has_attachment_at_slot(i)) {
      this->set_color_attachment_clear_color(i, clear_cols[i]);
      do_clear = true;
    }
  }

  if (do_clear) {
    has_pending_clear_ = true;

    /* Apply state before clear. */
    this->apply_state();

    /* TODO(Metal): Optimize - Currently force-clear always used. Consider moving clear state to
     * MTLTexture instead. */
    /* Force clear if RP is not yet active -- not the most efficient, but there is no distinction
     * between clears where no draws occur. Can optimize at the high-level by using explicit
     * load-store flags. */
    this->force_clear();
  }
}

void MTLFrameBuffer::clear_attachment(GPUAttachmentType type,
                                      eGPUDataFormat data_format,
                                      const void *clear_value)
{
  BLI_assert(MTLContext::get() == context_);
  BLI_assert(context_->active_fb == this);

  /* If we had no previous clear pending, reset clear state. */
  if (!has_pending_clear_) {
    this->reset_clear_state();
  }

  bool do_clear = false;

  if (type == GPU_FB_DEPTH_STENCIL_ATTACHMENT) {
    if (this->has_depth_attachment() || this->has_stencil_attachment()) {
      BLI_assert(data_format == GPU_DATA_UINT_24_8_DEPRECATED);
      float depth = ((*(uint32_t *)clear_value) & 0x00FFFFFFu) / (float)0x00FFFFFFu;
      int stencil = ((*(uint32_t *)clear_value) >> 24);
      this->set_depth_attachment_clear_value(depth);
      this->set_stencil_attachment_clear_value(stencil);
      do_clear = true;
    }
  }
  else if (type == GPU_FB_DEPTH_ATTACHMENT) {
    if (this->has_depth_attachment()) {
      if (data_format == GPU_DATA_FLOAT) {
        this->set_depth_attachment_clear_value(*(float *)clear_value);
      }
      else {
        float depth = *(uint32_t *)clear_value / (float)0xFFFFFFFFu;
        this->set_depth_attachment_clear_value(depth);
      }
      do_clear = true;
    }
  }
  else {
    int slot = type - GPU_FB_COLOR_ATTACHMENT0;
    if (this->has_attachment_at_slot(slot)) {
      float col_clear_val[4] = {0.0};
      switch (data_format) {
        case GPU_DATA_FLOAT: {
          const float *vals = (float *)clear_value;
          col_clear_val[0] = vals[0];
          col_clear_val[1] = vals[1];
          col_clear_val[2] = vals[2];
          col_clear_val[3] = vals[3];
        } break;
        case GPU_DATA_UINT: {
          const uint *vals = (uint *)clear_value;
          col_clear_val[0] = (float)(vals[0]);
          col_clear_val[1] = (float)(vals[1]);
          col_clear_val[2] = (float)(vals[2]);
          col_clear_val[3] = (float)(vals[3]);
        } break;
        case GPU_DATA_INT: {
          const int *vals = (int *)clear_value;
          col_clear_val[0] = (float)(vals[0]);
          col_clear_val[1] = (float)(vals[1]);
          col_clear_val[2] = (float)(vals[2]);
          col_clear_val[3] = (float)(vals[3]);
        } break;
        default:
          BLI_assert_msg(0, "Unhandled data format");
          break;
      }
      this->set_color_attachment_clear_color(slot, col_clear_val);
      do_clear = true;
    }
  }

  if (do_clear) {
    has_pending_clear_ = true;

    /* Apply state before clear. */
    this->apply_state();

    /* TODO(Metal): Optimize - Currently force-clear always used. Consider moving clear state to
     * MTLTexture instead. */
    /* Force clear if RP is not yet active -- not the most efficient, but there is no distinction
     * between clears where no draws occur. Can optimize at the high-level by using explicit
     * load-store flags. */
    this->force_clear();
  }
}
void MTLFrameBuffer::subpass_transition_impl(const GPUAttachmentState /*depth_attachment_state*/,
                                             Span<GPUAttachmentState> color_attachment_states)
{
  if (!MTLBackend::capabilities.supports_native_tile_inputs) {
    /* Break render-pass if tile memory is unsupported to ensure current frame-buffer results are
     * stored. */
    context_->main_command_buffer.end_active_command_encoder(true);

    /* Bind frame-buffer attachments as textures.
     * NOTE: Follows behavior of gl_framebuffer. However, shaders utilizing subpass_in will
     * need to avoid bind-point collisions for image/texture resources. */
    for (int i : color_attachment_states.index_range()) {
      GPUAttachmentType type = GPU_FB_COLOR_ATTACHMENT0 + i;
      gpu::Texture *attach_tex = this->attachments_[type].tex;
      if (color_attachment_states[i] == GPU_ATTACHMENT_READ) {
        GPU_texture_image_bind(attach_tex, i);
      }
    }
  }
}

void MTLFrameBuffer::read(GPUFrameBufferBits planes,
                          eGPUDataFormat format,
                          const int area[4],
                          int channel_len,
                          int slot,
                          void *r_data)
{

  BLI_assert((planes & GPU_STENCIL_BIT) == 0);
  BLI_assert(area[2] > 0);
  BLI_assert(area[3] > 0);

  /* Early exit if requested read region area is zero. */
  if (area[2] <= 0 || area[3] <= 0) {
    return;
  }

  switch (planes) {
    case GPU_DEPTH_BIT: {
      if (this->has_depth_attachment()) {
        MTLAttachment depth = this->get_depth_attachment();
        gpu::MTLTexture *tex = depth.texture;
        if (tex) {
          size_t sample_len = area[2] * area[3];
          size_t sample_size = to_bytesize(tex->format_, format);
          size_t debug_data_size = sample_len * sample_size;
          tex->read_internal(0,
                             area[0],
                             area[1],
                             0,
                             area[2],
                             area[3],
                             1,
                             format,
                             channel_len,
                             debug_data_size,
                             r_data);
        }
      }
      else {
        MTL_LOG_ERROR(
            "Attempting to read depth from a framebuffer which does not have a depth "
            "attachment!");
      }
    }
      return;

    case GPU_COLOR_BIT: {
      if (this->has_attachment_at_slot(slot)) {
        MTLAttachment color = this->get_color_attachment(slot);
        gpu::MTLTexture *tex = color.texture;
        if (tex) {
          size_t sample_len = area[2] * area[3];
          size_t sample_size = to_bytesize(tex->format_, format);
          size_t debug_data_size = sample_len * sample_size * channel_len;
          tex->read_internal(0,
                             area[0],
                             area[1],
                             0,
                             area[2],
                             area[3],
                             1,
                             format,
                             channel_len,
                             debug_data_size,
                             r_data);
        }
      }
    }
      return;

    case GPU_STENCIL_BIT:
      MTL_LOG_ERROR("Framebuffer: Trying to read stencil bit. Unsupported.");
      return;
  }
}

void MTLFrameBuffer::blit_to(GPUFrameBufferBits planes,
                             int src_slot,
                             FrameBuffer *dst,
                             int dst_slot,
                             int dst_offset_x,
                             int dst_offset_y)
{
  this->ensure_attachments_and_viewport();
  static_cast<MTLFrameBuffer *>(dst)->ensure_attachments_and_viewport();

  BLI_assert(planes != 0);

  MTLFrameBuffer *metal_fb_write = static_cast<MTLFrameBuffer *>(dst);

  BLI_assert(this);
  BLI_assert(metal_fb_write);

  /* Get width/height from attachment. */
  MTLAttachment src_attachment;
  const bool do_color = (planes & GPU_COLOR_BIT);
  const bool do_depth = (planes & GPU_DEPTH_BIT);
  const bool do_stencil = (planes & GPU_STENCIL_BIT);

  if (do_color) {
    BLI_assert(!do_depth && !do_stencil);
    src_attachment = this->get_color_attachment(src_slot);
  }
  else if (do_depth) {
    BLI_assert(!do_color && !do_stencil);
    src_attachment = this->get_depth_attachment();
  }
  else if (do_stencil) {
    BLI_assert(!do_color && !do_depth);
    src_attachment = this->get_stencil_attachment();
  }

  BLI_assert(src_attachment.used);
  this->blit(src_slot,
             0,
             0,
             metal_fb_write,
             dst_slot,
             dst_offset_x,
             dst_offset_y,
             src_attachment.texture->width_get(),
             src_attachment.texture->height_get(),
             planes);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \ Private METAL implementation functions
 * \{ */

void MTLFrameBuffer::mark_dirty()
{
  is_dirty_ = true;
  is_loadstore_dirty_ = true;
}

void MTLFrameBuffer::mark_loadstore_dirty()
{
  is_loadstore_dirty_ = true;
}

void MTLFrameBuffer::mark_cleared()
{
  has_pending_clear_ = false;
}

void MTLFrameBuffer::mark_do_clear()
{
  has_pending_clear_ = true;
}

void MTLFrameBuffer::update_attachments(bool /*update_viewport*/)
{
  if (!dirty_attachments_) {
    return;
  }
  /* Clear current attachments state. */
  this->remove_all_attachments();

  /* Reset framebuffer options. */
  use_multilayered_rendering_ = false;

  /* Track first attachment for SRGB property extraction. */
  GPUAttachmentType first_attachment = GPU_FB_MAX_ATTACHMENT;
  MTLAttachment first_attachment_mtl;

  /* Scan through changes to attachments and populate local structures. */
  bool depth_added = false;
  for (GPUAttachmentType type = GPU_FB_MAX_ATTACHMENT - 1; type >= 0; --type) {
    GPUAttachment &attach = attachments_[type];

    switch (type) {
      case GPU_FB_DEPTH_ATTACHMENT:
      case GPU_FB_DEPTH_STENCIL_ATTACHMENT: {
        /* If one of the DEPTH types has added a texture, we avoid running this again, as it would
         * only remove the target. */
        if (depth_added) {
          break;
        }
        if (attach.tex) {
          /* If we already had a depth attachment, preserve load/clear-state parameters,
           * but remove existing and add new attachment. */
          if (this->has_depth_attachment()) {
            MTLAttachment depth_attachment_prev = this->get_depth_attachment();
            this->remove_depth_attachment();
            this->add_depth_attachment(
                static_cast<gpu::MTLTexture *>(attach.tex), attach.mip, attach.layer);
            this->set_depth_attachment_clear_value(depth_attachment_prev.clear_value.depth);
            this->set_depth_loadstore_op(depth_attachment_prev.load_action,
                                         depth_attachment_prev.store_action);
          }
          else {
            this->add_depth_attachment(
                static_cast<gpu::MTLTexture *>(attach.tex), attach.mip, attach.layer);
          }

          /* Check stencil component -- if supplied texture format supports stencil. */
          TextureFormat format = GPU_texture_format(attach.tex);
          bool use_stencil = (type == GPU_FB_DEPTH_STENCIL_ATTACHMENT) &&
                             (format == TextureFormat::SFLOAT_32_DEPTH_UINT_8);
          if (use_stencil) {
            if (this->has_stencil_attachment()) {
              MTLAttachment stencil_attachment_prev = this->get_stencil_attachment();
              this->remove_stencil_attachment();
              this->add_stencil_attachment(
                  static_cast<gpu::MTLTexture *>(attach.tex), attach.mip, attach.layer);
              this->set_stencil_attachment_clear_value(
                  stencil_attachment_prev.clear_value.stencil);
              this->set_stencil_loadstore_op(stencil_attachment_prev.load_action,
                                             stencil_attachment_prev.store_action);
            }
            else {
              this->add_stencil_attachment(
                  static_cast<gpu::MTLTexture *>(attach.tex), attach.mip, attach.layer);
            }
          }

          /* Flag depth as added -- mirrors the behavior in gl_framebuffer.cc to exit the for-loop
           * after GPU_FB_DEPTH_STENCIL_ATTACHMENT has executed. */
          depth_added = true;

          if (first_attachment == GPU_FB_MAX_ATTACHMENT) {
            /* Only use depth texture to get information if there is no color attachment. */
            first_attachment = type;
            first_attachment_mtl = this->get_depth_attachment();
          }
        }
        else {
          this->remove_depth_attachment();
          if (type == GPU_FB_DEPTH_STENCIL_ATTACHMENT && this->has_stencil_attachment()) {
            this->remove_stencil_attachment();
          }
        }
      } break;
      case GPU_FB_COLOR_ATTACHMENT0:
      case GPU_FB_COLOR_ATTACHMENT1:
      case GPU_FB_COLOR_ATTACHMENT2:
      case GPU_FB_COLOR_ATTACHMENT3:
      case GPU_FB_COLOR_ATTACHMENT4:
      case GPU_FB_COLOR_ATTACHMENT5:
      case GPU_FB_COLOR_ATTACHMENT6:
      case GPU_FB_COLOR_ATTACHMENT7: {
        int color_slot_ind = type - GPU_FB_COLOR_ATTACHMENT0;
        if (attach.tex) {
          /* If we already had a color attachment, preserve load/clear-state parameters,
           * but remove existing and add new attachment. */
          if (this->has_attachment_at_slot(color_slot_ind)) {
            MTLAttachment color_attachment_prev = this->get_color_attachment(color_slot_ind);

            this->remove_color_attachment(color_slot_ind);
            this->add_color_attachment(static_cast<gpu::MTLTexture *>(attach.tex),
                                       color_slot_ind,
                                       attach.mip,
                                       attach.layer);
            this->set_color_attachment_clear_color(color_slot_ind,
                                                   color_attachment_prev.clear_value.color);
            this->set_color_loadstore_op(color_slot_ind,
                                         color_attachment_prev.load_action,
                                         color_attachment_prev.store_action);
          }
          else {
            this->add_color_attachment(static_cast<gpu::MTLTexture *>(attach.tex),
                                       color_slot_ind,
                                       attach.mip,
                                       attach.layer);
          }
          first_attachment = type;
          first_attachment_mtl = this->get_color_attachment(color_slot_ind);
        }
        else {
          this->remove_color_attachment(color_slot_ind);
        }
      } break;
      default:
        /* Non-attachment parameters. */
        break;
    }
  }

  /* Extract attachment size and determine if framebuffer is SRGB. */
  if (first_attachment != GPU_FB_MAX_ATTACHMENT) {
    /* Ensure size is correctly assigned. */
    GPUAttachment &attach = attachments_[first_attachment];
    int size[3];
    GPU_texture_get_mipmap_size(attach.tex, attach.mip, size);
    this->size_set(size[0], size[1]);
    srgb_ = (GPU_texture_format(attach.tex) == TextureFormat::SRGBA_8_8_8_8);
  }

  /* We have now updated our internal structures. */
  dirty_attachments_ = false;
}

void MTLFrameBuffer::apply_state()
{
  MTLContext *mtl_ctx = MTLContext::get();
  BLI_assert(mtl_ctx);
  if (mtl_ctx->active_fb == this) {
    if (dirty_state_ == false && dirty_state_ctx_ == mtl_ctx) {
      return;
    }

    /* Ensure viewport has been set. NOTE: This should no longer happen, but kept for safety to
     * track bugs. If viewport size is zero, use framebuffer size. */
    int viewport_w = viewport_[0][2];
    int viewport_h = viewport_[0][3];
    if (viewport_w == 0 || viewport_h == 0) {
      MTL_LOG_WARNING("Viewport had width and height of (0,0) -- Updating -- DEBUG Safety check");
      viewport_w = default_width_;
      viewport_h = default_height_;
    }

    /* Update Context State. */
    if (multi_viewport_) {
      mtl_ctx->set_viewports(GPU_MAX_VIEWPORTS, viewport_);
    }
    else {
      mtl_ctx->set_viewport(viewport_[0][0], viewport_[0][1], viewport_w, viewport_h);
    }
    mtl_ctx->set_scissor(scissor_[0], scissor_[1], scissor_[2], scissor_[3]);
    mtl_ctx->set_scissor_enabled(scissor_test_);

    dirty_state_ = false;
    dirty_state_ctx_ = mtl_ctx;
  }
  else {
    MTL_LOG_ERROR(
        "Attempting to set FrameBuffer State (VIEWPORT, SCISSOR), But FrameBuffer is not bound to "
        "current Context.");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \ Adding and Removing attachments
 * \{ */

bool MTLFrameBuffer::add_color_attachment(gpu::MTLTexture *texture,
                                          uint slot,
                                          int miplevel,
                                          int layer)
{
  BLI_assert(this);
  BLI_assert(slot >= 0 && slot < this->get_attachment_limit());
  set_color_attachment_bit(GPU_FB_COLOR_ATTACHMENT0 + int(slot), true);

  if (texture) {
    if (miplevel < 0 || miplevel >= MTL_MAX_MIPMAP_COUNT) {
      MTL_LOG_WARNING("Attachment specified with invalid mip level %u", miplevel);
      miplevel = 0;
    }

    /* Check if slot is in-use. */
    /* Assume attachment load by default. */
    colour_attachment_count_ += (!mtl_color_attachments_[slot].used) ? 1 : 0;
    mtl_color_attachments_[slot].used = true;
    mtl_color_attachments_[slot].texture = texture;
    mtl_color_attachments_[slot].mip = miplevel;
    mtl_color_attachments_[slot].load_action = GPU_LOADACTION_LOAD;
    mtl_color_attachments_[slot].store_action = GPU_STOREACTION_STORE;
    mtl_color_attachments_[slot].render_target_array_length = 0;

    /* Determine whether array slice or depth plane based on texture type. */
    switch (texture->type_) {
      case GPU_TEXTURE_1D:
      case GPU_TEXTURE_2D:
        BLI_assert(layer <= 0);
        mtl_color_attachments_[slot].slice = 0;
        mtl_color_attachments_[slot].depth_plane = 0;
        break;
      case GPU_TEXTURE_1D_ARRAY:
        if (layer < 0) {
          layer = 0;
          MTL_LOG_WARNING("TODO: Support layered rendering for 1D array textures, if needed.");
        }
        BLI_assert(layer < texture->h_);
        mtl_color_attachments_[slot].slice = layer;
        mtl_color_attachments_[slot].depth_plane = 0;
        break;
      case GPU_TEXTURE_2D_ARRAY:
        BLI_assert(layer < texture->d_);
        mtl_color_attachments_[slot].slice = layer;
        mtl_color_attachments_[slot].depth_plane = 0;
        if (layer == -1) {
          mtl_color_attachments_[slot].slice = 0;
          mtl_color_attachments_[slot].render_target_array_length = texture->d_;
          use_multilayered_rendering_ = true;
        }
        break;
      case GPU_TEXTURE_3D:
        BLI_assert(layer < texture->d_);
        mtl_color_attachments_[slot].slice = 0;
        mtl_color_attachments_[slot].depth_plane = layer;
        if (layer == -1) {
          mtl_color_attachments_[slot].depth_plane = 0;
          mtl_color_attachments_[slot].render_target_array_length = texture->d_;
          use_multilayered_rendering_ = true;
        }
        break;
      case GPU_TEXTURE_CUBE:
        BLI_assert(layer < 6);
        mtl_color_attachments_[slot].slice = layer;
        mtl_color_attachments_[slot].depth_plane = 0;
        if (layer == -1) {
          mtl_color_attachments_[slot].slice = 0;
          mtl_color_attachments_[slot].depth_plane = 0;
          mtl_color_attachments_[slot].render_target_array_length = 6;
          use_multilayered_rendering_ = true;
        }
        break;
      case GPU_TEXTURE_CUBE_ARRAY:
        BLI_assert(layer < 6 * texture->d_);
        /* TODO(Metal): Verify multilayered rendering for Cube arrays. */
        mtl_color_attachments_[slot].slice = layer;
        mtl_color_attachments_[slot].depth_plane = 0;
        if (layer == -1) {
          mtl_color_attachments_[slot].slice = 0;
          mtl_color_attachments_[slot].depth_plane = 0;
          mtl_color_attachments_[slot].render_target_array_length = texture->d_;
          use_multilayered_rendering_ = true;
        }
        break;
      case GPU_TEXTURE_BUFFER:
        mtl_color_attachments_[slot].slice = 0;
        mtl_color_attachments_[slot].depth_plane = 0;
        break;
      default:
        MTL_LOG_ERROR("MTLFrameBuffer::add_color_attachment Unrecognized texture type %u",
                      texture->type_);
        break;
    }

    /* Update default attachment size and ensure future attachments match the same size. */
    int width_of_miplayer, height_of_miplayer;
    if (miplevel <= 0) {
      width_of_miplayer = texture->width_get();
      height_of_miplayer = texture->height_get();
    }
    else {
      width_of_miplayer = max_ii(texture->width_get() >> miplevel, 1);
      height_of_miplayer = max_ii(texture->height_get() >> miplevel, 1);
    }

    if (default_width_ == 0 || default_height_ == 0) {
      this->default_size_set(width_of_miplayer, height_of_miplayer);
      BLI_assert(default_width_ > 0);
      BLI_assert(default_height_ > 0);
    }
    else {
      BLI_assert(default_width_ == width_of_miplayer);
      BLI_assert(default_height_ == height_of_miplayer);
    }

    /* Flag as dirty. */
    this->mark_dirty();
  }
  else {
    MTL_LOG_ERROR(
        "Passing in null texture to MTLFrameBuffer::add_color_attachment (This could be due to "
        "not all texture types being supported).");
  }
  return true;
}

bool MTLFrameBuffer::add_depth_attachment(gpu::MTLTexture *texture, int miplevel, int layer)
{
  BLI_assert(this);

  if (texture) {
    if (miplevel < 0 || miplevel >= MTL_MAX_MIPMAP_COUNT) {
      MTL_LOG_WARNING("Attachment specified with invalid mip level %u", miplevel);
      miplevel = 0;
    }

    /* Assume attachment load by default. */
    mtl_depth_attachment_.used = true;
    mtl_depth_attachment_.texture = texture;
    mtl_depth_attachment_.mip = miplevel;
    mtl_depth_attachment_.load_action = GPU_LOADACTION_LOAD;
    mtl_depth_attachment_.store_action = GPU_STOREACTION_STORE;
    mtl_depth_attachment_.render_target_array_length = 0;

    /* Determine whether array slice or depth plane based on texture type. */
    switch (texture->type_) {
      case GPU_TEXTURE_1D:
      case GPU_TEXTURE_2D:
        BLI_assert(layer <= 0);
        mtl_depth_attachment_.slice = 0;
        mtl_depth_attachment_.depth_plane = 0;
        break;
      case GPU_TEXTURE_1D_ARRAY:
        if (layer < 0) {
          layer = 0;
          MTL_LOG_WARNING("TODO: Support layered rendering for 1D array textures, if needed");
        }
        BLI_assert(layer < texture->h_);
        mtl_depth_attachment_.slice = layer;
        mtl_depth_attachment_.depth_plane = 0;
        break;
      case GPU_TEXTURE_2D_ARRAY:
        BLI_assert(layer < texture->d_);
        mtl_depth_attachment_.slice = layer;
        mtl_depth_attachment_.depth_plane = 0;
        if (layer == -1) {
          mtl_depth_attachment_.slice = 0;
          mtl_depth_attachment_.render_target_array_length = texture->d_;
          use_multilayered_rendering_ = true;
        }
        break;
      case GPU_TEXTURE_3D:
        BLI_assert(layer < texture->d_);
        mtl_depth_attachment_.slice = 0;
        mtl_depth_attachment_.depth_plane = layer;
        if (layer == -1) {
          mtl_depth_attachment_.depth_plane = 0;
          mtl_depth_attachment_.render_target_array_length = texture->d_;
          use_multilayered_rendering_ = true;
        }
        break;
      case GPU_TEXTURE_CUBE:
        BLI_assert(layer < 6);
        mtl_depth_attachment_.slice = layer;
        mtl_depth_attachment_.depth_plane = 0;
        if (layer == -1) {
          mtl_depth_attachment_.slice = 0;
          mtl_depth_attachment_.depth_plane = 0;
          mtl_depth_attachment_.render_target_array_length = 6;
          use_multilayered_rendering_ = true;
        }
        break;
      case GPU_TEXTURE_CUBE_ARRAY:
        /* TODO(Metal): Verify multilayered rendering for Cube arrays. */
        BLI_assert(layer < 6 * texture->d_);
        mtl_depth_attachment_.slice = layer;
        mtl_depth_attachment_.depth_plane = 0;
        if (layer == -1) {
          mtl_depth_attachment_.slice = 0;
          mtl_depth_attachment_.depth_plane = 0;
          mtl_depth_attachment_.render_target_array_length = texture->d_;
          use_multilayered_rendering_ = true;
        }
        break;
      case GPU_TEXTURE_BUFFER:
        mtl_depth_attachment_.slice = 0;
        mtl_depth_attachment_.depth_plane = 0;
        break;
      default:
        BLI_assert_msg(false, "Unrecognized texture type");
        break;
    }

    /* Update default attachment size and ensure future attachments match the same size. */
    int width_of_miplayer, height_of_miplayer;
    if (miplevel <= 0) {
      width_of_miplayer = texture->width_get();
      height_of_miplayer = texture->height_get();
    }
    else {
      width_of_miplayer = max_ii(texture->width_get() >> miplevel, 1);
      height_of_miplayer = max_ii(texture->height_get() >> miplevel, 1);
    }

    if (default_width_ == 0 || default_height_ == 0) {
      this->default_size_set(width_of_miplayer, height_of_miplayer);
      BLI_assert(default_width_ > 0);
      BLI_assert(default_height_ > 0);
    }
    else {
      BLI_assert(default_width_ == width_of_miplayer);
      BLI_assert(default_height_ == height_of_miplayer);
    }

    /* Flag as dirty after attachments changed. */
    this->mark_dirty();
  }
  else {
    MTL_LOG_ERROR(
        "Passing in null texture to MTLFrameBuffer::addDepthAttachment (This could be due to not "
        "all texture types being supported).");
  }
  return true;
}

bool MTLFrameBuffer::add_stencil_attachment(gpu::MTLTexture *texture, int miplevel, int layer)
{
  BLI_assert(this);

  if (texture) {
    if (miplevel < 0 || miplevel >= MTL_MAX_MIPMAP_COUNT) {
      MTL_LOG_WARNING("Attachment specified with invalid mip level %u", miplevel);
      miplevel = 0;
    }

    /* Assume attachment load by default. */
    mtl_stencil_attachment_.used = true;
    mtl_stencil_attachment_.texture = texture;
    mtl_stencil_attachment_.mip = miplevel;
    mtl_stencil_attachment_.load_action = GPU_LOADACTION_LOAD;
    mtl_stencil_attachment_.store_action = GPU_STOREACTION_STORE;
    mtl_stencil_attachment_.render_target_array_length = 0;

    /* Determine whether array slice or depth plane based on texture type. */
    switch (texture->type_) {
      case GPU_TEXTURE_1D:
      case GPU_TEXTURE_2D:
        BLI_assert(layer <= 0);
        mtl_stencil_attachment_.slice = 0;
        mtl_stencil_attachment_.depth_plane = 0;
        break;
      case GPU_TEXTURE_1D_ARRAY:
        if (layer < 0) {
          layer = 0;
          MTL_LOG_WARNING("TODO: Support layered rendering for 1D array textures, if needed");
        }
        BLI_assert(layer < texture->h_);
        mtl_stencil_attachment_.slice = layer;
        mtl_stencil_attachment_.depth_plane = 0;
        break;
      case GPU_TEXTURE_2D_ARRAY:
        BLI_assert(layer < texture->d_);
        mtl_stencil_attachment_.slice = layer;
        mtl_stencil_attachment_.depth_plane = 0;
        if (layer == -1) {
          mtl_stencil_attachment_.slice = 0;
          mtl_stencil_attachment_.render_target_array_length = texture->d_;
          use_multilayered_rendering_ = true;
        }
        break;
      case GPU_TEXTURE_3D:
        BLI_assert(layer < texture->d_);
        mtl_stencil_attachment_.slice = 0;
        mtl_stencil_attachment_.depth_plane = layer;
        if (layer == -1) {
          mtl_stencil_attachment_.depth_plane = 0;
          mtl_stencil_attachment_.render_target_array_length = texture->d_;
          use_multilayered_rendering_ = true;
        }
        break;
      case GPU_TEXTURE_CUBE:
        BLI_assert(layer < 6);
        mtl_stencil_attachment_.slice = layer;
        mtl_stencil_attachment_.depth_plane = 0;
        if (layer == -1) {
          mtl_stencil_attachment_.slice = 0;
          mtl_stencil_attachment_.depth_plane = 0;
          mtl_stencil_attachment_.render_target_array_length = 6;
          use_multilayered_rendering_ = true;
        }
        break;
      case GPU_TEXTURE_CUBE_ARRAY:
        /* TODO(Metal): Verify multilayered rendering for Cube arrays. */
        BLI_assert(layer < 6 * texture->d_);
        mtl_stencil_attachment_.slice = layer;
        mtl_stencil_attachment_.depth_plane = 0;
        if (layer == -1) {
          mtl_stencil_attachment_.slice = 0;
          mtl_stencil_attachment_.depth_plane = 0;
          mtl_stencil_attachment_.render_target_array_length = texture->d_;
          use_multilayered_rendering_ = true;
        }
        break;
      case GPU_TEXTURE_BUFFER:
        mtl_stencil_attachment_.slice = 0;
        mtl_stencil_attachment_.depth_plane = 0;
        break;
      default:
        BLI_assert_msg(false, "Unrecognized texture type");
        break;
    }

    /* Update default attachment size and ensure future attachments match the same size. */
    int width_of_miplayer, height_of_miplayer;
    if (miplevel <= 0) {
      width_of_miplayer = texture->width_get();
      height_of_miplayer = texture->height_get();
    }
    else {
      width_of_miplayer = max_ii(texture->width_get() >> miplevel, 1);
      height_of_miplayer = max_ii(texture->height_get() >> miplevel, 1);
    }

    if (default_width_ == 0 || default_height_ == 0) {
      this->default_size_set(width_of_miplayer, height_of_miplayer);
      BLI_assert(default_width_ > 0);
      BLI_assert(default_height_ > 0);
    }
    else {
      BLI_assert(default_width_ == width_of_miplayer);
      BLI_assert(default_height_ == height_of_miplayer);
    }

    /* Flag as dirty after attachments changed. */
    this->mark_dirty();
  }
  else {
    MTL_LOG_ERROR(
        "Passing in null texture to MTLFrameBuffer::addStencilAttachment (This could be due to "
        "not all texture types being supported).");
  }
  return true;
}

bool MTLFrameBuffer::remove_color_attachment(uint slot)
{
  BLI_assert(this);
  BLI_assert(slot >= 0 && slot < this->get_attachment_limit());
  set_color_attachment_bit(GPU_FB_COLOR_ATTACHMENT0 + int(slot), false);

  if (this->has_attachment_at_slot(slot)) {
    colour_attachment_count_ -= (mtl_color_attachments_[slot].used) ? 1 : 0;
    mtl_color_attachments_[slot].used = false;
    this->ensure_render_target_size();
    this->mark_dirty();
    return true;
  }

  return false;
}

bool MTLFrameBuffer::remove_depth_attachment()
{
  BLI_assert(this);

  mtl_depth_attachment_.used = false;
  mtl_depth_attachment_.texture = nullptr;
  this->ensure_render_target_size();
  this->mark_dirty();

  return true;
}

bool MTLFrameBuffer::remove_stencil_attachment()
{
  BLI_assert(this);

  mtl_stencil_attachment_.used = false;
  mtl_stencil_attachment_.texture = nullptr;
  this->ensure_render_target_size();
  this->mark_dirty();

  return true;
}

void MTLFrameBuffer::remove_all_attachments()
{
  BLI_assert(this);

  for (int attachment = 0; attachment < GPU_FB_MAX_COLOR_ATTACHMENT; attachment++) {
    this->remove_color_attachment(attachment);
  }
  this->remove_depth_attachment();
  this->remove_stencil_attachment();
  colour_attachment_count_ = 0;
  this->mark_dirty();

  /* Verify height. */
  this->ensure_render_target_size();

  /* Flag attachments as no longer being dirty. */
  dirty_attachments_ = false;
}

void MTLFrameBuffer::ensure_render_target_size()
{
  /* If we have no attachments, reset width and height to zero. */
  if (colour_attachment_count_ == 0 && !this->has_depth_attachment() &&
      !this->has_stencil_attachment())
  {
    /* Reset default size for empty framebuffer. */
    this->default_size_set(0, 0);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \ Clear values and Load-store actions
 * \{ */

void MTLFrameBuffer::attachment_set_loadstore_op(GPUAttachmentType type, GPULoadStore ls)
{
  if (type >= GPU_FB_COLOR_ATTACHMENT0) {
    int slot = type - GPU_FB_COLOR_ATTACHMENT0;
    if (ls.load_action == GPU_LOADACTION_CLEAR) {
      this->set_color_attachment_clear_color(slot, ls.clear_value);
    }
    this->set_color_loadstore_op(slot, ls.load_action, ls.store_action);
  }
  else if (type == GPU_FB_DEPTH_STENCIL_ATTACHMENT) {
    if (ls.load_action == GPU_LOADACTION_CLEAR) {
      this->set_depth_attachment_clear_value(ls.clear_value[0]);
    }
    this->set_depth_loadstore_op(ls.load_action, ls.store_action);
    this->set_stencil_loadstore_op(ls.load_action, ls.store_action);
  }
  else if (type == GPU_FB_DEPTH_ATTACHMENT) {
    if (ls.load_action == GPU_LOADACTION_CLEAR) {
      this->set_depth_attachment_clear_value(ls.clear_value[0]);
    }
    this->set_depth_loadstore_op(ls.load_action, ls.store_action);
  }
}

bool MTLFrameBuffer::set_color_attachment_clear_color(uint slot, const float clear_color[4])
{
  BLI_assert(this);
  BLI_assert(slot >= 0 && slot < this->get_attachment_limit());

  /* Only mark as dirty if values have changed. */
  bool changed = mtl_color_attachments_[slot].load_action != GPU_LOADACTION_CLEAR;
  float *attachment_clear_color = mtl_color_attachments_[slot].clear_value.color;
  changed = changed || (attachment_clear_color[0] != clear_color[0] ||
                        attachment_clear_color[1] != clear_color[1] ||
                        attachment_clear_color[2] != clear_color[2] ||
                        attachment_clear_color[3] != clear_color[3]);
  if (changed) {
    attachment_clear_color[0] = clear_color[0];
    attachment_clear_color[1] = clear_color[1];
    attachment_clear_color[2] = clear_color[2];
    attachment_clear_color[3] = clear_color[3];
  }
  mtl_color_attachments_[slot].load_action = GPU_LOADACTION_CLEAR;

  if (changed) {
    this->mark_loadstore_dirty();
  }
  return true;
}

bool MTLFrameBuffer::set_depth_attachment_clear_value(float depth_clear)
{
  BLI_assert(this);

  if (mtl_depth_attachment_.clear_value.depth != depth_clear ||
      mtl_depth_attachment_.load_action != GPU_LOADACTION_CLEAR)
  {
    mtl_depth_attachment_.clear_value.depth = depth_clear;
    mtl_depth_attachment_.load_action = GPU_LOADACTION_CLEAR;
    this->mark_loadstore_dirty();
  }
  return true;
}

bool MTLFrameBuffer::set_stencil_attachment_clear_value(uint stencil_clear)
{
  BLI_assert(this);

  if (mtl_stencil_attachment_.clear_value.stencil != stencil_clear ||
      mtl_stencil_attachment_.load_action != GPU_LOADACTION_CLEAR)
  {
    mtl_stencil_attachment_.clear_value.stencil = stencil_clear;
    mtl_stencil_attachment_.load_action = GPU_LOADACTION_CLEAR;
    this->mark_loadstore_dirty();
  }
  return true;
}

bool MTLFrameBuffer::set_color_loadstore_op(uint slot,
                                            GPULoadOp load_action,
                                            GPUStoreOp store_action)
{
  BLI_assert(this);
  GPULoadOp prev_load_action = mtl_color_attachments_[slot].load_action;
  GPUStoreOp prev_store_action = mtl_color_attachments_[slot].store_action;
  mtl_color_attachments_[slot].load_action = load_action;
  mtl_color_attachments_[slot].store_action = store_action;

  bool changed = (mtl_color_attachments_[slot].load_action != prev_load_action ||
                  mtl_color_attachments_[slot].store_action != prev_store_action);
  if (changed) {
    this->mark_loadstore_dirty();
  }

  return changed;
}

bool MTLFrameBuffer::set_depth_loadstore_op(GPULoadOp load_action, GPUStoreOp store_action)
{
  BLI_assert(this);
  GPULoadOp prev_load_action = mtl_depth_attachment_.load_action;
  GPUStoreOp prev_store_action = mtl_depth_attachment_.store_action;
  mtl_depth_attachment_.load_action = load_action;
  mtl_depth_attachment_.store_action = store_action;

  bool changed = (mtl_depth_attachment_.load_action != prev_load_action ||
                  mtl_depth_attachment_.store_action != prev_store_action);
  if (changed) {
    this->mark_loadstore_dirty();
  }

  return changed;
}

bool MTLFrameBuffer::set_stencil_loadstore_op(GPULoadOp load_action, GPUStoreOp store_action)
{
  BLI_assert(this);
  GPULoadOp prev_load_action = mtl_stencil_attachment_.load_action;
  GPUStoreOp prev_store_action = mtl_stencil_attachment_.store_action;
  mtl_stencil_attachment_.load_action = load_action;
  mtl_stencil_attachment_.store_action = store_action;

  bool changed = (mtl_stencil_attachment_.load_action != prev_load_action ||
                  mtl_stencil_attachment_.store_action != prev_store_action);
  if (changed) {
    this->mark_loadstore_dirty();
  }

  return changed;
}

bool MTLFrameBuffer::reset_clear_state()
{
  for (int slot = 0; slot < colour_attachment_count_; slot++) {
    this->set_color_loadstore_op(slot, GPU_LOADACTION_LOAD, GPU_STOREACTION_STORE);
  }
  this->set_depth_loadstore_op(GPU_LOADACTION_LOAD, GPU_STOREACTION_STORE);
  this->set_stencil_loadstore_op(GPU_LOADACTION_LOAD, GPU_STOREACTION_STORE);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \ Fetch values and Frame-buffer status
 * \{ */

bool MTLFrameBuffer::has_attachment_at_slot(uint slot)
{
  BLI_assert(this);

  if (slot >= 0 && slot < this->get_attachment_limit()) {
    return mtl_color_attachments_[slot].used;
  }
  return false;
}

bool MTLFrameBuffer::has_color_attachment_with_texture(gpu::MTLTexture *texture)
{
  BLI_assert(this);

  for (int attachment = 0; attachment < this->get_attachment_limit(); attachment++) {
    if (mtl_color_attachments_[attachment].used &&
        mtl_color_attachments_[attachment].texture == texture)
    {
      return true;
    }
  }
  return false;
}

bool MTLFrameBuffer::has_depth_attachment()
{
  BLI_assert(this);
  return mtl_depth_attachment_.used;
}

bool MTLFrameBuffer::has_stencil_attachment()
{
  BLI_assert(this);
  return mtl_stencil_attachment_.used;
}

int MTLFrameBuffer::get_color_attachment_slot_from_texture(gpu::MTLTexture *texture)
{
  BLI_assert(this);
  BLI_assert(texture);

  for (int attachment = 0; attachment < this->get_attachment_limit(); attachment++) {
    if (mtl_color_attachments_[attachment].used &&
        (mtl_color_attachments_[attachment].texture == texture))
    {
      return attachment;
    }
  }
  return -1;
}

uint MTLFrameBuffer::get_attachment_count()
{
  BLI_assert(this);
  return colour_attachment_count_;
}

MTLAttachment MTLFrameBuffer::get_color_attachment(uint slot)
{
  BLI_assert(this);
  if (slot >= 0 && slot < GPU_FB_MAX_COLOR_ATTACHMENT) {
    return mtl_color_attachments_[slot];
  }
  MTLAttachment null_attachment;
  null_attachment.used = false;
  return null_attachment;
}

MTLAttachment MTLFrameBuffer::get_depth_attachment()
{
  BLI_assert(this);
  return mtl_depth_attachment_;
}

MTLAttachment MTLFrameBuffer::get_stencil_attachment()
{
  BLI_assert(this);
  return mtl_stencil_attachment_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \ METAL API Resources and Validation
 * \{ */
bool MTLFrameBuffer::validate_render_pass()
{
  BLI_assert(this);

  /* First update attachments if dirty. */
  if (dirty_attachments_) {
    this->update_attachments(true);
  }

  /* NOTE: Attachment-less render targets now supported so we do not need to validate attachment
   * counts. Keeping this function in place if other parameters need to be validate. */
  return true;
}

MTLLoadAction mtl_load_action_from_gpu(GPULoadOp action)
{
  return (action == GPU_LOADACTION_LOAD) ?
             MTLLoadActionLoad :
             ((action == GPU_LOADACTION_CLEAR) ? MTLLoadActionClear : MTLLoadActionDontCare);
}

MTLStoreAction mtl_store_action_from_gpu(GPUStoreOp action)
{
  return (action == GPU_STOREACTION_STORE) ? MTLStoreActionStore : MTLStoreActionDontCare;
}

MTLRenderPassDescriptor *MTLFrameBuffer::bake_render_pass_descriptor(bool load_contents)
{
  BLI_assert(this);
  if (load_contents) {
    /* Only force-load contents if there is no clear pending. */
    BLI_assert(!has_pending_clear_);
  }

  /* Ensure we are inside a frame boundary. */
  MTLContext *metal_ctx = MTLContext::get();
  BLI_assert(metal_ctx && metal_ctx->get_inside_frame());
  UNUSED_VARS_NDEBUG(metal_ctx);

  /* If Frame-buffer has been modified, regenerate descriptor. */
  if (is_dirty_) {
    /* Clear all configurations. */
    for (int config = 0; config < 3; config++) {
      descriptor_dirty_[config] = true;
    }
  }
  else if (is_loadstore_dirty_) {
    /* Load config always has load ops, so we only need to re-generate custom and clear state. */
    descriptor_dirty_[MTL_FB_CONFIG_CLEAR] = true;
    descriptor_dirty_[MTL_FB_CONFIG_CUSTOM] = true;
  }

  /* If we need to populate descriptor" */
  /* Select config based on FrameBuffer state:
   * [0] {MTL_FB_CONFIG_CLEAR} = Clear config -- we have a pending clear so should perform our
   * configured clear.
   * [1] {MTL_FB_CONFIG_LOAD} = Load config -- We need to re-load ALL attachments,
   * used for re-binding/pass-breaks.
   * [2] {MTL_FB_CONFIG_CUSTOM} = Custom config -- Use this when a custom binding config is
   * specified.
   */
  uint descriptor_config = (load_contents) ? MTL_FB_CONFIG_LOAD :
                                             ((this->get_pending_clear()) ? MTL_FB_CONFIG_CLEAR :
                                                                            MTL_FB_CONFIG_CUSTOM);

  /* NOTE: If `GPU_framebuffer_bind_loadstore` is used, the `use_explicit_load_store_` flag will be
   * set in which case, we should always use the custom configuration. Calls with this flag will
   * only happen for the first render pass after binding. */
  if (use_explicit_load_store_) {
    descriptor_config = MTL_FB_CONFIG_CUSTOM;
    descriptor_dirty_[descriptor_config] = true;
  }

  if (descriptor_dirty_[descriptor_config] || framebuffer_descriptor_[descriptor_config] == nil) {

    /* Create descriptor if it does not exist. */
    if (framebuffer_descriptor_[descriptor_config] == nil) {
      framebuffer_descriptor_[descriptor_config] = [[MTLRenderPassDescriptor alloc] init];
    }

    /* Configure multilayered rendering. */
    if (use_multilayered_rendering_) {
      /* Ensure all targets have the same length. */
      int len = 0;
      bool valid = true;

      for (int attachment_ind = 0; attachment_ind < GPU_FB_MAX_COLOR_ATTACHMENT; attachment_ind++)
      {
        if (mtl_color_attachments_[attachment_ind].used) {
          if (len == 0) {
            len = mtl_color_attachments_[attachment_ind].render_target_array_length;
          }
          else {
            valid = valid &&
                    (len == mtl_color_attachments_[attachment_ind].render_target_array_length);
          }
        }
      }

      if (mtl_depth_attachment_.used) {
        if (len == 0) {
          len = mtl_depth_attachment_.render_target_array_length;
        }
        else {
          valid = valid && (len == mtl_depth_attachment_.render_target_array_length);
        }
      }

      if (mtl_stencil_attachment_.used) {
        if (len == 0) {
          len = mtl_stencil_attachment_.render_target_array_length;
        }
        else {
          valid = valid && (len == mtl_stencil_attachment_.render_target_array_length);
        }
      }

      BLI_assert(len > 0);
      BLI_assert(valid);
      framebuffer_descriptor_[descriptor_config].renderTargetArrayLength = len;
    }
    else {
      framebuffer_descriptor_[descriptor_config].renderTargetArrayLength = 0;
    }

    /* Color attachments. */
    int colour_attachments = 0;
    for (int attachment_ind = 0; attachment_ind < GPU_FB_MAX_COLOR_ATTACHMENT; attachment_ind++) {
      MTLAttachment &attachment_config = mtl_color_attachments_[attachment_ind];

      if (attachment_config.used) {
        id<MTLTexture> texture = attachment_config.texture->get_metal_handle_base();
        if (texture == nil) {
          MTL_LOG_ERROR("Attempting to assign invalid texture as attachment");
        }

        bool texture_is_memoryless = (attachment_config.texture->usage_get() &
                                      GPU_TEXTURE_USAGE_MEMORYLESS);

        /* IF SRGB is enabled, but we are rendering with SRGB disabled, sample texture view. */
        id<MTLTexture> source_color_texture = texture;
        if (this->get_is_srgb() && attachment_config.texture->is_format_srgb() &&
            !this->get_srgb_enabled())
        {
          source_color_texture = attachment_config.texture->get_non_srgb_handle();
          BLI_assert(source_color_texture != nil);
        }

        /* Resolve appropriate load action -- IF force load, perform load.
         * If clear but framebuffer has no pending clear, also load. */
        GPULoadOp load_action = attachment_config.load_action;
        if (descriptor_config == MTL_FB_CONFIG_LOAD) {
          /* MTL_FB_CONFIG_LOAD must always load. */
          load_action = GPU_LOADACTION_LOAD;
        }
        else if (descriptor_config == MTL_FB_CONFIG_CUSTOM && load_action == GPU_LOADACTION_CLEAR)
        {
          /* If Custom load config is used, fallback to loading if explicit bind state flag is
           * unset. This is to ensure attachments are loaded by default in the case a framebuffer
           * is unbound and rebound, or, if a render pass breaks mid-pass for compute or blit
           * operations. */
          if (!use_explicit_load_store_ && !texture_is_memoryless) {
            load_action = GPU_LOADACTION_LOAD;
          }
        }

        /* Ensure memoryless attachment cannot load or store results. */
        GPUStoreOp store_action = attachment_config.store_action;
        if (texture_is_memoryless && load_action == GPU_LOADACTION_LOAD) {
          load_action = GPU_LOADACTION_DONT_CARE;
        }
        if (texture_is_memoryless && store_action == GPU_STOREACTION_STORE) {
          store_action = GPU_STOREACTION_DONT_CARE;
        }

        /* Create attachment descriptor. */
        MTLRenderPassColorAttachmentDescriptor *attachment =
            colour_attachment_descriptors_[attachment_ind];
        BLI_assert(attachment != nil);

        attachment.texture = source_color_texture;
        attachment.loadAction = mtl_load_action_from_gpu(load_action);
        attachment.clearColor = (load_action == GPU_LOADACTION_CLEAR) ?
                                    MTLClearColorMake(
                                        UNPACK4(attachment_config.clear_value.color)) :
                                    MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
        attachment.storeAction = mtl_store_action_from_gpu(store_action);
        attachment.level = attachment_config.mip;
        attachment.slice = attachment_config.slice;
        attachment.depthPlane = attachment_config.depth_plane;
        colour_attachments++;

        /* Copy attachment info back in. */
        [framebuffer_descriptor_[descriptor_config].colorAttachments setObject:attachment
                                                            atIndexedSubscript:attachment_ind];
      }
      else {
        /* Disable color attachment. */
        [framebuffer_descriptor_[descriptor_config].colorAttachments setObject:nil
                                                            atIndexedSubscript:attachment_ind];
      }
    }
    BLI_assert(colour_attachments == colour_attachment_count_);
    UNUSED_VARS_NDEBUG(colour_attachments);

    /* Depth attachment. */
    if (mtl_depth_attachment_.used) {
      framebuffer_descriptor_[descriptor_config].depthAttachment.texture =
          (id<MTLTexture>)mtl_depth_attachment_.texture->get_metal_handle_base();

      bool texture_is_memoryless = (mtl_depth_attachment_.texture->usage_get() &
                                    GPU_TEXTURE_USAGE_MEMORYLESS);

      /* Resolve appropriate load action -- IF force load, perform load.
       * If clear but framebuffer has no pending clear, also load. */
      GPULoadOp load_action = mtl_depth_attachment_.load_action;
      if (descriptor_config == MTL_FB_CONFIG_LOAD) {
        /* MTL_FB_CONFIG_LOAD must always load. */
        load_action = GPU_LOADACTION_LOAD;
      }
      else if (descriptor_config == MTL_FB_CONFIG_CUSTOM && load_action == GPU_LOADACTION_CLEAR) {
        /* If Custom load config is used, fallback to loading if explicit bind state flag is unset.
         * This is to ensure attachments are loaded by default in the case a framebuffer is unbound
         * and rebound, or, if a render pass breaks mid-pass for compute or blit operations. */
        if (!use_explicit_load_store_ && !texture_is_memoryless) {
          load_action = GPU_LOADACTION_LOAD;
        }
      }

      /* Ensure memoryless attachment cannot load or store results. */
      GPUStoreOp store_action = mtl_depth_attachment_.store_action;
      if (texture_is_memoryless && load_action == GPU_LOADACTION_LOAD) {
        load_action = GPU_LOADACTION_DONT_CARE;
      }
      if (texture_is_memoryless && store_action == GPU_STOREACTION_STORE) {
        store_action = GPU_STOREACTION_DONT_CARE;
      }

      framebuffer_descriptor_[descriptor_config].depthAttachment.loadAction =
          mtl_load_action_from_gpu(load_action);
      framebuffer_descriptor_[descriptor_config].depthAttachment.clearDepth =
          (load_action == GPU_LOADACTION_CLEAR) ? mtl_depth_attachment_.clear_value.depth : 0;
      framebuffer_descriptor_[descriptor_config].depthAttachment.storeAction =
          mtl_store_action_from_gpu(store_action);
      framebuffer_descriptor_[descriptor_config].depthAttachment.level = mtl_depth_attachment_.mip;
      framebuffer_descriptor_[descriptor_config].depthAttachment.slice =
          mtl_depth_attachment_.slice;
      framebuffer_descriptor_[descriptor_config].depthAttachment.depthPlane =
          mtl_depth_attachment_.depth_plane;
    }
    else {
      framebuffer_descriptor_[descriptor_config].depthAttachment.texture = nil;
    }

    /* Stencil attachment. */
    if (mtl_stencil_attachment_.used) {
      framebuffer_descriptor_[descriptor_config].stencilAttachment.texture =
          (id<MTLTexture>)mtl_stencil_attachment_.texture->get_metal_handle_base();

      bool texture_is_memoryless = (mtl_stencil_attachment_.texture->usage_get() &
                                    GPU_TEXTURE_USAGE_MEMORYLESS);

      /* Resolve appropriate load action -- IF force load, perform load.
       * If clear but framebuffer has no pending clear, also load. */
      GPULoadOp load_action = mtl_stencil_attachment_.load_action;
      if (descriptor_config == MTL_FB_CONFIG_LOAD) {
        /* MTL_FB_CONFIG_LOAD must always load. */
        load_action = GPU_LOADACTION_LOAD;
      }
      else if (descriptor_config == MTL_FB_CONFIG_CUSTOM && load_action == GPU_LOADACTION_CLEAR) {
        /* If Custom load config is used, fallback to loading if explicit bind state flag is unset.
         * This is to ensure attachments are loaded by default in the case a framebuffer is unbound
         * and rebound, or, if a render pass breaks mid-pass for compute or blit operations. */
        if (!use_explicit_load_store_ && !texture_is_memoryless) {
          load_action = GPU_LOADACTION_LOAD;
        }
      }

      /* Ensure memoryless attachment cannot load or store results. */
      GPUStoreOp store_action = mtl_stencil_attachment_.store_action;
      if (texture_is_memoryless && load_action == GPU_LOADACTION_LOAD) {
        load_action = GPU_LOADACTION_DONT_CARE;
      }
      if (texture_is_memoryless && store_action == GPU_STOREACTION_STORE) {
        store_action = GPU_STOREACTION_DONT_CARE;
      }

      framebuffer_descriptor_[descriptor_config].stencilAttachment.loadAction =
          mtl_load_action_from_gpu(load_action);
      framebuffer_descriptor_[descriptor_config].stencilAttachment.clearStencil =
          (load_action == GPU_LOADACTION_CLEAR) ? mtl_stencil_attachment_.clear_value.stencil : 0;
      framebuffer_descriptor_[descriptor_config].stencilAttachment.storeAction =
          mtl_store_action_from_gpu(store_action);
      framebuffer_descriptor_[descriptor_config].stencilAttachment.level =
          mtl_stencil_attachment_.mip;
      framebuffer_descriptor_[descriptor_config].stencilAttachment.slice =
          mtl_stencil_attachment_.slice;
      framebuffer_descriptor_[descriptor_config].stencilAttachment.depthPlane =
          mtl_stencil_attachment_.depth_plane;
    }
    else {
      framebuffer_descriptor_[descriptor_config].stencilAttachment.texture = nil;
    }

    /* Attachmentless render support. */
    int total_num_attachments = colour_attachment_count_ + (mtl_depth_attachment_.used ? 1 : 0) +
                                (mtl_stencil_attachment_.used ? 1 : 0);
    if (total_num_attachments == 0) {
      BLI_assert(width_ > 0 && height_ > 0);
      framebuffer_descriptor_[descriptor_config].renderTargetWidth = width_;
      framebuffer_descriptor_[descriptor_config].renderTargetHeight = height_;
      framebuffer_descriptor_[descriptor_config].defaultRasterSampleCount = 1;
    }

    descriptor_dirty_[descriptor_config] = false;
  }
  /* Clear dirty state flags. */
  is_dirty_ = false;
  is_loadstore_dirty_ = false;

  /* Clear explicit bind flag. */
  use_explicit_load_store_ = false;

  return framebuffer_descriptor_[descriptor_config];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \ Blitting
 * \{ */

void MTLFrameBuffer::blit(uint read_slot,
                          uint src_x_offset,
                          uint src_y_offset,
                          MTLFrameBuffer *metal_fb_write,
                          uint write_slot,
                          uint dst_x_offset,
                          uint dst_y_offset,
                          uint width,
                          uint height,
                          GPUFrameBufferBits blit_buffers)
{
  BLI_assert(metal_fb_write);
  if (!metal_fb_write) {
    return;
  }
  MTLContext *mtl_context = MTLContext::get();

  const bool do_color = (blit_buffers & GPU_COLOR_BIT);
  const bool do_depth = (blit_buffers & GPU_DEPTH_BIT);
  const bool do_stencil = (blit_buffers & GPU_STENCIL_BIT);

  /* Early exit if there is no blit to do. */
  if (!(do_color || do_depth || do_stencil)) {
    MTL_LOG_WARNING("FrameBuffer: requested blit but no color, depth or stencil flag was set");
    return;
  }

  id<MTLBlitCommandEncoder> blit_encoder = nil;

  /* If the color format is not the same, we cannot use the BlitCommandEncoder, and instead use
   * a Graphics-based blit. */
  if (do_color && (this->get_color_attachment(read_slot).texture->format_get() !=
                   metal_fb_write->get_color_attachment(read_slot).texture->format_get()))
  {

    MTLAttachment src_attachment = this->get_color_attachment(read_slot);
    MTLAttachment dst_attachment = metal_fb_write->get_color_attachment(write_slot);
    assert(src_attachment.slice == 0 &&
           "currently only supporting slice 0 for graphics framebuffer blit");

    src_attachment.texture->blit(dst_attachment.texture,
                                 src_x_offset,
                                 src_y_offset,
                                 dst_x_offset,
                                 dst_y_offset,
                                 src_attachment.mip,
                                 dst_attachment.mip,
                                 dst_attachment.slice,
                                 width,
                                 height);
  }
  else {

    /* Setup blit encoder. */
    blit_encoder = mtl_context->main_command_buffer.ensure_begin_blit_encoder();

    if (do_color) {
      MTLAttachment src_attachment = this->get_color_attachment(read_slot);
      MTLAttachment dst_attachment = metal_fb_write->get_color_attachment(write_slot);

      if (src_attachment.used && dst_attachment.used) {

        /* TODO(Metal): Support depth(z) offset in blit if needed. */
        src_attachment.texture->blit(blit_encoder,
                                     src_x_offset,
                                     src_y_offset,
                                     0,
                                     src_attachment.slice,
                                     src_attachment.mip,
                                     dst_attachment.texture,
                                     dst_x_offset,
                                     dst_y_offset,
                                     0,
                                     dst_attachment.slice,
                                     dst_attachment.mip,
                                     width,
                                     height,
                                     1);
      }
      else {
        MTL_LOG_ERROR("Failed performing colour blit");
      }
    }
  }
  if ((do_depth || do_stencil) && blit_encoder == nil) {
    blit_encoder = mtl_context->main_command_buffer.ensure_begin_blit_encoder();
  }

  if (do_depth) {
    MTLAttachment src_attachment = this->get_depth_attachment();
    MTLAttachment dst_attachment = metal_fb_write->get_depth_attachment();

    if (src_attachment.used && dst_attachment.used) {

      /* TODO(Metal): Support depth(z) offset in blit if needed. */
      src_attachment.texture->blit(blit_encoder,
                                   src_x_offset,
                                   src_y_offset,
                                   0,
                                   src_attachment.slice,
                                   src_attachment.mip,
                                   dst_attachment.texture,
                                   dst_x_offset,
                                   dst_y_offset,
                                   0,
                                   dst_attachment.slice,
                                   dst_attachment.mip,
                                   width,
                                   height,
                                   1);
    }
    else {
      MTL_LOG_ERROR("Failed performing depth blit");
    }
  }

  /* Stencil attachment blit. */
  if (do_stencil) {
    MTLAttachment src_attachment = this->get_stencil_attachment();
    MTLAttachment dst_attachment = metal_fb_write->get_stencil_attachment();

    if (src_attachment.used && dst_attachment.used) {

      /* TODO(Metal): Support depth(z) offset in blit if needed. */
      src_attachment.texture->blit(blit_encoder,
                                   src_x_offset,
                                   src_y_offset,
                                   0,
                                   src_attachment.slice,
                                   src_attachment.mip,
                                   dst_attachment.texture,
                                   dst_x_offset,
                                   dst_y_offset,
                                   0,
                                   dst_attachment.slice,
                                   dst_attachment.mip,
                                   width,
                                   height,
                                   1);
    }
    else {
      MTL_LOG_ERROR("Failed performing Stencil blit");
    }
  }
}

int MTLFrameBuffer::get_width()
{
  return width_;
}
int MTLFrameBuffer::get_height()
{
  return height_;
}

int MTLFrameBuffer::get_default_width()
{
  return default_width_;
}
int MTLFrameBuffer::get_default_height()
{
  return default_height_;
}

}  // namespace blender::gpu
