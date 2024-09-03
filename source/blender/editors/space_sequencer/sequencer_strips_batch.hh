/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "GPU_shader_shared.hh"

struct GPUShader;
struct GPUUniformBuf;
struct View2D;

namespace blender::gpu {
class Batch;
}

namespace blender::ed::seq {

/* Utility to draw VSE timeline strip widgets in batches, with a dedicated
 * shader. Internally, strip data for drawing is encoded into a uniform
 * buffer. Strip coordinates are converted into pixel space, to avoid
 * precision issues at large frames. Drawing assumes that a pixel space
 * projection matrix is set. */
class StripsDrawBatch {
  SeqContextDrawData context_;
  Array<SeqStripDrawData> strips_;
  GPUUniformBuf *ubo_context_ = nullptr;
  GPUUniformBuf *ubo_strips_ = nullptr;
  GPUShader *shader_ = nullptr;
  gpu::Batch *batch_ = nullptr;
  int binding_context_ = 0;
  int binding_strips_ = 0;
  int strips_count_ = 0;

  float2 view_mask_min_;
  float2 view_mask_size_;
  float2 view_cur_min_;
  float2 view_cur_inv_size_;

 public:
  StripsDrawBatch(const View2D *v2d);
  ~StripsDrawBatch();

  SeqStripDrawData &add_strip(float content_start,
                              float content_end,
                              float top,
                              float bottom,
                              float content_top,
                              float left_handle,
                              float right_handle,
                              float handle_width,
                              bool single_image);

  void flush_batch();

  /* Same math as `UI_view2d_view_to_region_*` but avoiding divisions,
   * and without relying on View2D data type. */
  inline float pos_to_pixel_space_x(float x) const
  {
    return (view_mask_min_.x + (x - view_cur_min_.x) * view_cur_inv_size_.x) * view_mask_size_.x;
  }
  inline float pos_to_pixel_space_y(float y) const
  {
    return (view_mask_min_.y + (y - view_cur_min_.y) * view_cur_inv_size_.y) * view_mask_size_.y;
  }
  inline float size_to_pixel_space_x(float x) const
  {
    return x * view_cur_inv_size_.x * view_mask_size_.x;
  }

  GPUUniformBuf *get_ubo_context() const
  {
    return ubo_context_;
  }
};

uint color_pack(const uchar rgba[4]);
float calc_strip_round_radius(float pixely);

}  // namespace blender::ed::seq
