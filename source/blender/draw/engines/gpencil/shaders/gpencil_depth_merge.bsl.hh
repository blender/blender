/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view.bsl.hh"

namespace gpencil::depth_merge {

struct Resources {
  [[push_constant]] float4x4 gp_model_matrix;
  [[push_constant]] float4 clipping_rect;
  [[push_constant]] bool stroke_order3d;

  [[sampler(0)]] sampler2DDepth depth_buf;
};

[[vertex]]
void vert_main([[resource_table]] const Resources &srt,
               [[resource_table]] const draw::View &views,
               [[vertex_id]] const int vert_id,
               [[position]] float4 &out_pos)
{
  const ViewMatrices view = views.get(0);

  int v = vert_id % 3;
  float x = -1.0f + float((v & 1) << 2);
  float y = -1.0f + float((v & 2) << 1);
  out_pos = view.winmat * (view.viewmat * (srt.gp_model_matrix * float4(x, y, 0.0f, 1.0f)));
}

[[fragment]]
void frag_main([[resource_table]] const Resources &srt,
               [[frag_coord]] const float4 frag_co,
               [[frag_depth(any)]] float &out_depth)
{
  if (any(lessThan(frag_co.xy, srt.clipping_rect.xy)) ||
      any(greaterThan(frag_co.xy, srt.clipping_rect.zw)))
  {
    gpu_discard_fragment();
    return;
  }

  float depth = textureLod(srt.depth_buf, frag_co.xy / float2(textureSize(srt.depth_buf, 0)), 0).r;
  if (srt.stroke_order3d) {
    out_depth = depth;
  }
  else {
    out_depth = (depth != 0.0f) ? frag_co.z : 1.0f;
  }
}

struct DepthPass {
  [[image(0, read_write, SFLOAT_32)]] image2D depth_pass_img;
};

/* Used to merge the object's depth to the viewport compositor depth pass. This is essentially the
 * same as the regular frag_main but writes to the pass as an image output
 * instead of a depth frame buffer. However, it actually writes scene linear depth with manual
 * depth comparison. */
[[fragment]]
void frag_main_pass([[resource_table]] const Resources &srt,
                    [[resource_table]] const draw::View &views,
                    [[resource_table]] DepthPass &pass,
                    [[frag_coord]] const float4 frag_co)
{
  const ViewMatrices view = views.get(0);

  const int2 texel = int2(frag_co.xy);
  const float2 normalized_coordinates = frag_co.xy / float2(textureSize(srt.depth_buf, 0));

  const float depth_3d = textureLod(srt.depth_buf, normalized_coordinates, 0).x;
  const float depth_2d = frag_co.z;
  const float depth = srt.stroke_order3d ? depth_3d : depth_2d;
  const float view_depth = -view.depth_screen_to_view(depth);

  /* Match the background depth in render_layer_allocate_pass. */
  constexpr float background_depth = 1e10f;
  const bool is_background = srt.stroke_order3d ? depth_3d == 1.0f : depth_3d == 0.0f;
  const float object_depth = is_background ? background_depth : view_depth;

  const float scene_depth = imageLoad(pass.depth_pass_img, texel).x;

  const float combined_depth = min(scene_depth, object_depth);
  imageStore(pass.depth_pass_img, texel, float4(combined_depth));
}

}  // namespace gpencil::depth_merge

PipelineGraphic gpencil_depth_merge(gpencil::depth_merge::vert_main,
                                    gpencil::depth_merge::frag_main);
PipelineGraphic gpencil_depth_pass_merge(gpencil::depth_merge::vert_main,
                                         gpencil::depth_merge::frag_main_pass);
