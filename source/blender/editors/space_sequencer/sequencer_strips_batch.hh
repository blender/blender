/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#pragma once

#include "BLI_array.hh"
#include "GPU_shader_shared.hh"

struct GPUShader;
struct GPUUniformBuf;

namespace blender::gpu {
class Batch;
}

namespace blender::ed::seq {

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

 public:
  StripsDrawBatch(float pixelx, float pixely);
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
};

uint color_pack(const uchar rgba[4]);
float calc_strip_round_radius(float pixely);

}  // namespace blender::ed::seq
