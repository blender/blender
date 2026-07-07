/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Encapsulation of Frame-buffer states (attached textures, viewport, scissors).
 */

#pragma once

#include "GPU_common_types.hh"
#include "MEM_guardedalloc.h"

#include "gpu_framebuffer_private.hh"
#include "mtl_texture.hh"

#include <Metal/Metal.h>

namespace blender::gpu {

class MTLContext;

struct MTLAttachment {
  bool used = false;
  gpu::MTLTexture *texture = nullptr;
  union {
    float color[4];
    float depth;
    uint stencil;
  } clear_value;

  GPULoadOp load_action = GPU_LOADACTION_DONT_CARE;
  GPUStoreOp store_action = GPU_STOREACTION_DONT_CARE;
  uint mip = 0;
  uint slice = 0;
  uint depth_plane = 0;

  /* If Array Length is larger than zero, use multilayered rendering. */
  uint render_target_array_length = 0;
};

/**
 * Implementation of FrameBuffer object using Metal.
 */
class MTLFrameBuffer : public FrameBuffer {
 private:
  /* Context Handle. */
  MTLContext *context_;

  /* Metal Attachment properties. */
  uint colour_attachment_count_;
  MTLAttachment mtl_color_attachments_[GPU_FB_MAX_COLOR_ATTACHMENT];
  MTLAttachment mtl_depth_attachment_;
  MTLAttachment mtl_stencil_attachment_;
  bool use_multilayered_rendering_ = false;

  /* State. */

  /**
   * Whether global frame-buffer properties have changed and require
   * re-generation of #MTLRenderPassDescriptor / #RenderCommandEncoders.
   */
  bool is_dirty_;

  /** Whether `loadstore` properties have changed (only affects certain cached configurations). */
  bool is_loadstore_dirty_;

  /**
   * Context that the latest modified state was last applied to.
   * If this does not match current ctx, re-apply state.
   */
  MTLContext *dirty_state_ctx_;

  /**
   * Whether a clear is pending -- Used to toggle between clear and load FB configurations
   * (without dirtying the state) - Frame-buffer load configuration is used if no `GPU_clear_*`
   * command was issued after binding the #FrameBuffer.
   */
  bool has_pending_clear_;

  /**
   * Render Pass Descriptors:
   * There are 3 #MTLRenderPassDescriptors for different ways in which a frame-buffer
   * can be configured:
   * [0] = CLEAR CONFIG -- Used when a GPU_framebuffer_clear_* command has been issued.
   * [1] = LOAD CONFIG -- Used if bound, but no clear is required.
   * [2] = CUSTOM CONFIG -- When using GPU_framebuffer_bind_ex to manually specify
   * load-store configuration for optimal bandwidth utilization.
   * -- We cache these different configurations to avoid re-generation --
   */
  enum {
    MTL_FB_CONFIG_CLEAR = 0,
    MTL_FB_CONFIG_LOAD = 1,
    MTL_FB_CONFIG_CUSTOM = 2,

    MTL_FB_CONFIG_MAX = (MTL_FB_CONFIG_CUSTOM + 1),
  };

  MTLRenderPassDescriptor *framebuffer_descriptor_[MTL_FB_CONFIG_MAX];
  MTLRenderPassColorAttachmentDescriptor
      *colour_attachment_descriptors_[GPU_FB_MAX_COLOR_ATTACHMENT];
  /** Whether `MTLRenderPassDescriptor[N]` requires updating with latest state. */
  bool descriptor_dirty_[MTL_FB_CONFIG_MAX];
  /** Whether SRGB is enabled for this frame-buffer configuration. */
  bool enabled_srgb_;
  /** Whether the primary Frame-buffer attachment is an SRGB target or not. */
  bool srgb_;

  /** Default width/height represent raw size of active frame-buffer attachments.
   * For consistency with OpenGL backend, as width_/height_ can affect viewport and scissor
   * size, we need to track this differently to ensure viewport state does not get reset.
   * This size is only used to reset viewport/scissor regions when viewports and scissor are
   * disabled, as Metal does not provide a utility to fully disable either without manually
   * specifying the size.
   */
  int default_width_ = 0;
  int default_height_ = 0;

 public:
  /**
   * Create a conventional frame-buffer to attach texture to.
   */
  MTLFrameBuffer(MTLContext *ctx, const char *name);

  ~MTLFrameBuffer() override;

  void bind(bool enabled_srgb) override;

  bool check(char err_out[256]) override;

  void clear(GPUFrameBufferBits buffers,
             const float clear_col[4],
             float clear_depth,
             uint clear_stencil) override;
  void clear_multi(const float (*clear_cols)[4]) override;
  void clear_attachment(GPUAttachmentType type,
                        eGPUDataFormat data_format,
                        const void *clear_value) override;

  void attachment_set_loadstore_op(GPUAttachmentType type, GPULoadStore ls) override;

  void read(GPUFrameBufferBits planes,
            eGPUDataFormat format,
            const int area[4],
            int channel_len,
            int slot,
            void *r_data) override;

  void blit_to(GPUFrameBufferBits planes,
               int src_slot,
               FrameBuffer *dst,
               int dst_slot,
               int dst_offset_x,
               int dst_offset_y) override;

 protected:
  void subpass_transition_impl(const GPUAttachmentState /*depth_attachment_state*/,
                               Span<GPUAttachmentState> color_attachment_states) override;

 public:
  void apply_state();

  /* State. */
  /* Flag MTLFramebuffer configuration as having changed. */
  void mark_dirty();
  void mark_loadstore_dirty();
  /* Mark that a pending clear has been performed. */
  void mark_cleared();
  /* Mark that we have a pending clear. */
  void mark_do_clear();

  /* Attachment management. */
  /* When dirty_attachments_ is true, we need to reprocess attachments to extract Metal
   * information. */
  void ensure_attachments_and_viewport();
  void update_attachments(bool update_viewport);
  bool add_color_attachment(gpu::MTLTexture *texture, uint slot, int miplevel, int layer);
  bool add_depth_attachment(gpu::MTLTexture *texture, int miplevel, int layer);
  bool add_stencil_attachment(gpu::MTLTexture *texture, int miplevel, int layer);
  bool remove_color_attachment(uint slot);
  bool remove_depth_attachment();
  bool remove_stencil_attachment();
  void remove_all_attachments();
  void ensure_render_target_size();

  /* Clear values -> Load/store actions. */
  bool set_color_attachment_clear_color(uint slot, const float clear_color[4]);
  bool set_depth_attachment_clear_value(float depth_clear);
  bool set_stencil_attachment_clear_value(uint stencil_clear);
  bool set_color_loadstore_op(uint slot, GPULoadOp load_action, GPUStoreOp store_action);
  bool set_depth_loadstore_op(GPULoadOp load_action, GPUStoreOp store_action);
  bool set_stencil_loadstore_op(GPULoadOp load_action, GPUStoreOp store_action);

  /* Remove any pending clears - Ensure "load" configuration is used. */
  bool reset_clear_state();

  /* Fetch values */
  bool has_attachment_at_slot(uint slot);
  bool has_color_attachment_with_texture(gpu::MTLTexture *texture);
  bool has_depth_attachment();
  bool has_stencil_attachment();
  int get_color_attachment_slot_from_texture(gpu::MTLTexture *texture);
  uint get_attachment_count();
  uint get_attachment_limit()
  {
    return GPU_FB_MAX_COLOR_ATTACHMENT;
  };
  MTLAttachment get_color_attachment(uint slot);
  MTLAttachment get_depth_attachment();
  MTLAttachment get_stencil_attachment();

  /* Metal API resources and validation. */
  bool validate_render_pass();
  MTLRenderPassDescriptor *bake_render_pass_descriptor(bool load_contents);

  /* Blitting. */
  void blit(uint read_slot,
            uint src_x_offset,
            uint src_y_offset,
            MTLFrameBuffer *metal_fb_write,
            uint write_slot,
            uint dst_x_offset,
            uint dst_y_offset,
            uint width,
            uint height,
            GPUFrameBufferBits blit_buffers);

  int get_width();
  int get_height();
  int get_default_width();
  int get_default_height();

  bool get_dirty()
  {
    return is_dirty_ || is_loadstore_dirty_;
  }

  bool get_pending_clear()
  {
    return has_pending_clear_;
  }

  bool get_srgb_enabled()
  {
    return enabled_srgb_;
  }

  bool get_is_srgb()
  {
    return srgb_;
  }

  inline void default_size_set(int w, int h)
  {
    default_width_ = w;
    default_height_ = h;
  }

 private:
  /* Clears a render target by force-opening a render pass. */
  void force_clear();

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLFrameBuffer");
};

}  // namespace blender::gpu
