/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Private frame buffer API.
 */

#pragma once

#include "BLI_math_vector.h"
#include "BLI_span.hh"

#include "GPU_framebuffer.hh"

namespace blender::gpu {
class Texture;
}

enum GPUAttachmentType : int {
  GPU_FB_DEPTH_ATTACHMENT = 0,
  GPU_FB_DEPTH_STENCIL_ATTACHMENT,
  GPU_FB_COLOR_ATTACHMENT0,
  GPU_FB_COLOR_ATTACHMENT1,
  GPU_FB_COLOR_ATTACHMENT2,
  GPU_FB_COLOR_ATTACHMENT3,
  GPU_FB_COLOR_ATTACHMENT4,
  GPU_FB_COLOR_ATTACHMENT5,
  GPU_FB_COLOR_ATTACHMENT6,
  GPU_FB_COLOR_ATTACHMENT7,
  /* Number of maximum output slots. */
  /* Keep in mind that GL max is GL_MAX_DRAW_BUFFERS and is at least 8, corresponding to
   * the maximum number of COLOR attachments specified by glDrawBuffers. */
  GPU_FB_MAX_ATTACHMENT,

};

#define GPU_FB_MAX_COLOR_ATTACHMENT (GPU_FB_MAX_ATTACHMENT - GPU_FB_COLOR_ATTACHMENT0)

constexpr GPUAttachmentType operator-(GPUAttachmentType a, int b)
{
  return static_cast<GPUAttachmentType>(int(a) - b);
}

constexpr GPUAttachmentType operator+(GPUAttachmentType a, int b)
{
  return static_cast<GPUAttachmentType>(int(a) + b);
}

inline GPUAttachmentType &operator++(GPUAttachmentType &a)
{
  a = a + 1;
  return a;
}

inline GPUAttachmentType &operator--(GPUAttachmentType &a)
{
  a = a - 1;
  return a;
}

namespace blender::gpu {

#ifndef NDEBUG
#  define DEBUG_NAME_LEN 64
#else
#  define DEBUG_NAME_LEN 16
#endif

class FrameBuffer {
 protected:
  /** Set of texture attachments to render to. DEPTH and DEPTH_STENCIL are mutually exclusive. */
  GPUAttachment attachments_[GPU_FB_MAX_ATTACHMENT];
  /** Is true if internal representation need to be updated. */
  bool dirty_attachments_ = true;
  /** Size of attachment textures. */
  int width_ = 0, height_ = 0;
  /** Debug name. */
  char name_[DEBUG_NAME_LEN];
  /** Frame-buffer state. */
  int viewport_[GPU_MAX_VIEWPORTS][4] = {{0}};
  int scissor_[4] = {0};
  bool multi_viewport_ = false;
  bool scissor_test_ = false;
  bool dirty_state_ = true;
  /* Flag specifying the current bind operation should use explicit load-store state. */
  bool use_explicit_load_store_ = false;
  /** Bit-set indicating the color attachments slots in use. */
  uint16_t color_attachments_bits_ = 0;

 public:
#ifndef GPU_NO_USE_PY_REFERENCES
  /**
   * Reference of a pointer that needs to be cleaned when deallocating the frame-buffer.
   * Points to #BPyGPUFrameBuffer.fb
   */
  void **py_ref = nullptr;
#endif

  FrameBuffer(const char *name);
  virtual ~FrameBuffer();

  virtual void bind(bool enabled_srgb) = 0;
  virtual bool check(char err_out[256]) = 0;
  virtual void clear(GPUFrameBufferBits buffers,
                     const float clear_col[4],
                     float clear_depth,
                     uint clear_stencil) = 0;
  virtual void clear_multi(const float (*clear_col)[4]) = 0;
  virtual void clear_attachment(GPUAttachmentType type,
                                eGPUDataFormat data_format,
                                const void *clear_value) = 0;

  virtual void attachment_set_loadstore_op(GPUAttachmentType type, GPULoadStore ls) = 0;

  virtual void read(GPUFrameBufferBits planes,
                    eGPUDataFormat format,
                    const int area[4],
                    int channel_len,
                    int slot,
                    void *r_data) = 0;

  virtual void blit_to(GPUFrameBufferBits planes,
                       int src_slot,
                       FrameBuffer *dst,
                       int dst_slot,
                       int dst_offset_x,
                       int dst_offset_y) = 0;

 protected:
  virtual void subpass_transition_impl(const GPUAttachmentState depth_attachment_state,
                                       Span<GPUAttachmentState> color_attachment_states) = 0;

  void set_color_attachment_bit(GPUAttachmentType type, bool value)
  {
    if (type >= GPU_FB_COLOR_ATTACHMENT0) {
      int color_index = type - GPU_FB_COLOR_ATTACHMENT0;
      SET_FLAG_FROM_TEST(color_attachments_bits_, value, 1u << color_index);
    }
  }

 public:
  void subpass_transition(const GPUAttachmentState depth_attachment_state,
                          Span<GPUAttachmentState> color_attachment_states);

  void load_store_config_array(const GPULoadStore *load_store_actions, uint actions_len);

  void attachment_set(GPUAttachmentType type, const GPUAttachment &new_attachment);
  void attachment_remove(GPUAttachmentType type);

  uint get_bits_per_pixel();

  /* Sets the size after creation. */
  void size_set(int width, int height)
  {
    width_ = width;
    height_ = height;
    dirty_state_ = true;
  }

  /* Sets the size for frame-buffer with no attachments. */
  void default_size_set(int width, int height)
  {
    width_ = width;
    height_ = height;
    dirty_attachments_ = true;
    dirty_state_ = true;
  }

  void viewport_set(const int viewport[4])
  {
    if (!equals_v4v4_int(viewport_[0], viewport)) {
      copy_v4_v4_int(viewport_[0], viewport);
      dirty_state_ = true;
    }
    multi_viewport_ = false;
  }

  void viewport_multi_set(const int viewports[GPU_MAX_VIEWPORTS][4])
  {
    for (size_t i = 0; i < GPU_MAX_VIEWPORTS; i++) {
      if (!equals_v4v4_int(viewport_[i], viewports[i])) {
        copy_v4_v4_int(viewport_[i], viewports[i]);
        dirty_state_ = true;
      }
    }
    multi_viewport_ = true;
  }

  void scissor_set(const int scissor[4])
  {
    if (!equals_v4v4_int(scissor_, scissor)) {
      copy_v4_v4_int(scissor_, scissor);
      dirty_state_ = true;
    }
  }

  void scissor_test_set(bool test)
  {
    scissor_test_ = test;
    dirty_state_ = true;
  }

  void viewport_get(int r_viewport[4]) const
  {
    copy_v4_v4_int(r_viewport, viewport_[0]);
  }

  void scissor_get(int r_scissor[4]) const
  {
    copy_v4_v4_int(r_scissor, scissor_);
  }

  bool scissor_test_get() const
  {
    return scissor_test_;
  }

  void viewport_reset()
  {
    int viewport_rect[4] = {0, 0, width_, height_};
    viewport_set(viewport_rect);
  }

  void scissor_reset()
  {
    int scissor_rect[4] = {0, 0, width_, height_};
    scissor_set(scissor_rect);
  }

  inline const GPUAttachment &depth_attachment() const
  {
    if (attachments_[GPU_FB_DEPTH_ATTACHMENT].tex) {
      return attachments_[GPU_FB_DEPTH_ATTACHMENT];
    }
    return attachments_[GPU_FB_DEPTH_STENCIL_ATTACHMENT];
  }

  blender::gpu::Texture *depth_tex() const
  {
    return depth_attachment().tex;
  };

  blender::gpu::Texture *color_tex(int slot) const
  {
    return attachments_[GPU_FB_COLOR_ATTACHMENT0 + slot].tex;
  };

  const char *name_get() const
  {
    return name_;
  };

  void set_use_explicit_loadstore(bool use_explicit_loadstore)
  {
    use_explicit_load_store_ = use_explicit_loadstore;
  }

  bool get_use_explicit_loadstore() const
  {
    return use_explicit_load_store_;
  }

  uint16_t get_color_attachments_bitset()
  {
    return color_attachments_bits_;
  }
};

#undef DEBUG_NAME_LEN

}  // namespace blender::gpu
