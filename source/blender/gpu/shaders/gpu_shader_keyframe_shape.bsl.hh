/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_shader_shared.hh"
#include "gpu_shader_utildefines_lib.glsl"

namespace builtin::keyframe {

struct VertIn {
  [[attribute(0)]] float4 color;
  [[attribute(1)]] float4 outlineColor;
  [[attribute(2)]] float2 pos;
  [[attribute(3)]] float size;
  [[attribute(4)]] uint flags;
};

struct VertOut {
  [[flat]] float4 finalColor;
  [[flat]] float4 finalOutlineColor;
  [[flat]] float4 radii;
  [[flat]] float4 thresholds;
  [[flat]] uint finalFlags;
};

struct FragOut {
  [[frag_color(0)]] float4 color;
};

struct Resources {
  [[push_constant]] float4x4 ModelViewProjectionMatrix;
  [[push_constant]] float2 ViewportSize;
  [[push_constant]] float outline_scale;
};

#define line_falloff 1.0f
#define circle_scale sqrt(2.0f / 3.1416f)
#define square_scale sqrt(0.5f)
#define diagonal_scale sqrt(0.5f)

#define minmax_bias 0.7f
#define minmax_scale sqrt(1.0f / (1.0f + 1.0f / minmax_bias))

float2 line_thresholds(float width)
{
  return float2(max(0.0f, width - line_falloff), width);
}

[[vertex]] void vert_main([[resource_table]] const Resources &srt,
                          [[in]] const VertIn &v_in,
                          [[out]] VertOut &v_out,
                          [[position]] float4 &out_pos,
                          [[point_size]] float &out_pt_size)
{
  out_pos = srt.ModelViewProjectionMatrix * float4(v_in.pos, 0.0f, 1.0f);

  /* Align to pixel grid if the viewport size is known. */
  if (srt.ViewportSize.x > 0) {
    float2 scale = srt.ViewportSize * 0.5f;
    float2 px_pos = (out_pos.xy + 1) * scale;
    float2 adj_pos = round(px_pos - 0.5f) + 0.5f;
    out_pos.xy = adj_pos / scale - 1;
  }

  /* Pass through parameters. */
  v_out.finalColor = v_in.color;
  v_out.finalOutlineColor = v_in.outlineColor;
  v_out.finalFlags = v_in.flags;

  if (!flag_test(v_in.flags,
                 GPU_KEYFRAME_SHAPE_DIAMOND | GPU_KEYFRAME_SHAPE_CIRCLE |
                     GPU_KEYFRAME_SHAPE_CLIPPED_VERTICAL | GPU_KEYFRAME_SHAPE_CLIPPED_HORIZONTAL))
  {
    v_out.finalFlags |= GPU_KEYFRAME_SHAPE_DIAMOND;
  }

  /* Size-dependent line thickness. */
  float half_width = (0.06f + (v_in.size - 10) * 0.04f);
  float line_width = half_width + line_falloff;

  /* Outline thresholds. */
  v_out.thresholds.xy = line_thresholds(line_width * srt.outline_scale);

  /* Inner dot thresholds. */
  v_out.thresholds.zw = line_thresholds(line_width * 1.6f);

  /* Extend the primitive size by half line width on either side; odd for symmetry. */
  float ext_radius = round(0.5f * v_in.size) + v_out.thresholds.x;

  out_pt_size = ceil(ext_radius + v_out.thresholds.y) * 2 + 1;

  /* Diamond radius. */
  v_out.radii[0] = ext_radius * diagonal_scale;
  /* Circle radius. */
  v_out.radii[1] = ext_radius * circle_scale;
  /* Square radius. */
  v_out.radii[2] = round(ext_radius * square_scale);
  /* Min/max cutout offset. */
  v_out.radii[3] = -line_falloff;
  /* Convert to PointCoord units. */
  v_out.radii /= out_pt_size;
  v_out.thresholds /= out_pt_size;
}

[[fragment]] void frag_main([[point_coord]] const float2 pt_coord,
                            [[in]] const VertOut &v_out,
                            [[out]] FragOut &frag_out)
{
  float2 pos = pt_coord - float2(0.5f);
  float2 absPos = abs(pos);
  float radius = (absPos.x + absPos.y) * diagonal_scale;

  float outline_dist = -1.0f;

  /* Diamond outline */
  if (flag_test(v_out.finalFlags, GPU_KEYFRAME_SHAPE_DIAMOND)) {
    outline_dist = max(outline_dist, radius - v_out.radii[0]);
  }

  /* Circle outline */
  if (flag_test(v_out.finalFlags, GPU_KEYFRAME_SHAPE_CIRCLE)) {
    radius = length(absPos);

    outline_dist = max(outline_dist, radius - v_out.radii[1]);
  }

  /* Top & Bottom clamp */
  if (flag_test(v_out.finalFlags, GPU_KEYFRAME_SHAPE_CLIPPED_VERTICAL)) {
    outline_dist = max(outline_dist, absPos.y - v_out.radii[2]);
  }

  /* Left & Right clamp */
  if (flag_test(v_out.finalFlags, GPU_KEYFRAME_SHAPE_CLIPPED_HORIZONTAL)) {
    outline_dist = max(outline_dist, absPos.x - v_out.radii[2]);
  }

  float alpha = 1 - smoothstep(v_out.thresholds[0], v_out.thresholds[1], abs(outline_dist));

  /* Inside the outline. */
  if (outline_dist < 0) {
    /* Middle dot */
    if (flag_test(v_out.finalFlags, GPU_KEYFRAME_SHAPE_INNER_DOT)) {
      alpha = max(alpha, 1 - smoothstep(v_out.thresholds[2], v_out.thresholds[3], length(absPos)));
    }

    /* Up and down arrow-like shading. */
    if (flag_test(v_out.finalFlags,
                  GPU_KEYFRAME_SHAPE_ARROW_END_MAX | GPU_KEYFRAME_SHAPE_ARROW_END_MIN))
    {
      float ypos = -1.0f;

      /* Up arrow (maximum) */
      if (flag_test(v_out.finalFlags, GPU_KEYFRAME_SHAPE_ARROW_END_MAX)) {
        ypos = max(ypos, pos.y);
      }
      /* Down arrow (minimum) */
      if (flag_test(v_out.finalFlags, GPU_KEYFRAME_SHAPE_ARROW_END_MIN)) {
        ypos = max(ypos, -pos.y);
      }

      /* Arrow shape threshold. */
      float minmax_dist = (ypos - v_out.radii[3]) - absPos.x * minmax_bias;
      float minmax_step = smoothstep(
          v_out.thresholds[0], v_out.thresholds[1], minmax_dist * minmax_scale);

      /* Reduced alpha for uncertain extremes. */
      float minmax_alpha = flag_test(v_out.finalFlags, GPU_KEYFRAME_SHAPE_ARROW_END_MIXED) ?
                               0.55f :
                               0.85f;

      alpha = max(alpha, minmax_step * minmax_alpha);
    }

    frag_out.color = mix(v_out.finalColor, v_out.finalOutlineColor, alpha);
  }
  /* Outside the outline. */
  else {
    frag_out.color = float4(v_out.finalOutlineColor.rgb, v_out.finalOutlineColor.a * alpha);
  }
}
}  // namespace builtin::keyframe

PipelineGraphic gpu_shader_keyframe_shape(builtin::keyframe::vert_main,
                                          builtin::keyframe::frag_main);
