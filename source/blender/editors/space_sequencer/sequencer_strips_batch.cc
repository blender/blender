/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "sequencer_strips_batch.hh"

#include "BLI_rect.h"

#include "DNA_userdef_types.h"
#include "DNA_view2d_types.h"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_shader_shared.hh"
#include "GPU_uniform_buffer.hh"

#include "UI_resources.hh"

namespace blender::ed::seq {

uint color_pack(const uchar rgba[4])
{
  return rgba[0] | (rgba[1] << 8u) | (rgba[2] << 16u) | (rgba[3] << 24u);
}

float calc_strip_round_radius(float pixely)
{
  float height_pixels = 1.0f / pixely;
  if (height_pixels < 16.0f) {
    return 0.0f;
  }
  if (height_pixels < 64.0f) {
    return 4.0f;
  }
  if (height_pixels < 128.0f) {
    return 6.0f;
  }
  return 8.0f;
}

StripsDrawBatch::StripsDrawBatch(const View2D *v2d) : strips_(GPU_SEQ_STRIP_DRAW_DATA_LEN)
{
  view_mask_min_ = float2(v2d->mask.xmin, v2d->mask.ymin);
  view_mask_size_ = float2(BLI_rcti_size_x(&v2d->mask), BLI_rcti_size_y(&v2d->mask));
  view_cur_min_ = float2(v2d->cur.xmin, v2d->cur.ymin);
  float2 view_cur_size = float2(BLI_rctf_size_x(&v2d->cur), BLI_rctf_size_y(&v2d->cur));
  view_cur_inv_size_ = 1.0f / view_cur_size;

  float pixely = view_cur_size.y / view_mask_size_.y;
  context_.round_radius = calc_strip_round_radius(pixely);
  context_.pixelsize = U.pixelsize;

  uchar col[4];
  UI_GetThemeColorShade3ubv(TH_BACK, -40, col);
  col[3] = 255;
  context_.col_back = color_pack(col);

  shader_ = GPU_shader_get_builtin_shader(GPU_SHADER_SEQUENCER_STRIPS);
  binding_strips_ = GPU_shader_get_ubo_binding(shader_, "strip_data");
  binding_context_ = GPU_shader_get_ubo_binding(shader_, "context_data");

  ubo_context_ = GPU_uniformbuf_create_ex(sizeof(SeqContextDrawData), &context_, __func__);
  ubo_strips_ = GPU_uniformbuf_create(sizeof(SeqStripDrawData) * GPU_SEQ_STRIP_DRAW_DATA_LEN);

  batch_ = GPU_batch_preset_quad();
}

StripsDrawBatch::~StripsDrawBatch()
{
  flush_batch();

  GPU_uniformbuf_unbind(ubo_strips_);
  GPU_uniformbuf_free(ubo_strips_);
  GPU_uniformbuf_unbind(ubo_context_);
  GPU_uniformbuf_free(ubo_context_);
}

SeqStripDrawData &StripsDrawBatch::add_strip(float content_start,
                                             float content_end,
                                             float top,
                                             float bottom,
                                             float content_top,
                                             float left_handle,
                                             float right_handle,
                                             float handle_width,
                                             bool single_image)
{
  if (strips_count_ == GPU_SEQ_STRIP_DRAW_DATA_LEN) {
    flush_batch();
  }

  SeqStripDrawData &res = strips_[strips_count_];
  strips_count_++;

  memset(&res, 0, sizeof(res));
  res.content_start = pos_to_pixel_space_x(content_start);
  res.content_end = pos_to_pixel_space_x(content_end);
  res.top = pos_to_pixel_space_y(top);
  res.bottom = pos_to_pixel_space_y(bottom);
  res.strip_content_top = pos_to_pixel_space_y(content_top);
  res.left_handle = pos_to_pixel_space_x(left_handle);
  res.right_handle = pos_to_pixel_space_x(right_handle);
  res.handle_width = size_to_pixel_space_x(handle_width);
  if (single_image) {
    res.flags |= GPU_SEQ_FLAG_SINGLE_IMAGE;
  }
  return res;
}

void StripsDrawBatch::flush_batch()
{
  if (strips_count_ == 0) {
    return;
  }

  GPU_uniformbuf_update(ubo_strips_, strips_.data());

  GPU_shader_bind(shader_);
  GPU_uniformbuf_bind(ubo_strips_, binding_strips_);
  GPU_uniformbuf_bind(ubo_context_, binding_context_);

  GPU_batch_set_shader(batch_, shader_);
  GPU_batch_draw_instance_range(batch_, 0, strips_count_);
  strips_count_ = 0;
}

}  // namespace blender::ed::seq
