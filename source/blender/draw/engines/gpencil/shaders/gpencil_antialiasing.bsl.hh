/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_fullscreen.bsl.hh"

namespace gpencil::antialiasing {

[[vertex]] void vert_main([[vertex_id]] const int vert_id, [[position]] float4 &out_pos)
{
  fullscreen_vertex(vert_id, out_pos);
}

struct Resources {
  [[image(0, read, SFLOAT_16_16_16_16 /* GPENCIL_RENDER_FORMAT */)]] image2D src_img;
  [[image(1, read_write, SFLOAT_16_16_16_16 /* GPENCIL_ACCUM_FORMAT */)]] image2D dst_img;
  [[push_constant]] float weight_src;
  [[push_constant]] float weight_dst;
};

float4 colorspace_scene_to_perceptual(float4 color)
{
  return float4(log2(color.rgb + 0.5f), color.a);
}

float4 colorspace_perceptual_to_scene(float4 color)
{
  return float4(exp2(color.rgb) - 0.5f, color.a);
}

[[fragment]] void accumulation_frag([[resource_table]] Resources &srt,
                                    [[frag_coord]] const float4 frag_co)
{
  int2 texel = int2(frag_co.xy);
  float4 data_src = colorspace_scene_to_perceptual(
      max(float4(0.0f), imageLoadFast(srt.src_img, texel)));
  float4 data_dst = colorspace_scene_to_perceptual(
      max(float4(0.0f), imageLoadFast(srt.dst_img, texel)));
  float4 result = data_src * srt.weight_src;
  if (srt.weight_dst > 0.0f) {
    /* Avoid uncleared data to mess with the result value. */
    result += data_dst * srt.weight_dst;
  }
  if (data_src.a == 1.0f && data_dst.a == 1.0f) {
    /* Avoid float imprecision leading to non fully opaque renders. */
    result.a = 1.0f;
  }
  imageStoreFast(srt.dst_img, texel, colorspace_perceptual_to_scene(result));
}

}  // namespace gpencil::antialiasing

PipelineGraphic gpencil_antialiasing_accumulation(gpencil::antialiasing::vert_main,
                                                  gpencil::antialiasing::accumulation_frag);
