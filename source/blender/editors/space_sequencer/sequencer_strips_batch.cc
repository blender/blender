/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "sequencer_strips_batch.hh"

#include "DNA_userdef_types.h"

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

StripsDrawBatch::StripsDrawBatch(float pixelx, float pixely) : strips_(GPU_SEQ_STRIP_DRAW_DATA_LEN)
{
  context_.pixelx = pixelx;
  context_.pixely = pixely;
  context_.inv_pixelx = 1.0f / pixelx;
  context_.inv_pixely = 1.0f / pixely;
  context_.round_radius = calc_strip_round_radius(pixely);
  context_.pixelsize = U.pixelsize;

  uchar col[4];
  UI_GetThemeColor3ubv(TH_BACK, col);
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
  res.content_start = content_start;
  res.content_end = content_end;
  res.top = top;
  res.bottom = bottom;
  res.strip_content_top = content_top;
  res.left_handle = left_handle;
  res.right_handle = right_handle;
  res.handle_width = handle_width;
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
