/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_colorspace.bsl.hh"

#include "GPU_shader_shared.hh"

namespace builtin {

struct FragOut {
  [[frag_color(0)]] float4 color;
};

struct ClipPlanes {
  [[uniform(1)]] const GPUClipPlanes &clip_planes;
  [[push_constant]] const float4x4 ClipModelMatrix;
};

struct Simple {
  [[compilation_constant]] const bool use_uniform_color;
  [[compilation_constant]] const bool use_smooth_color;
  [[compilation_constant]] const bool use_clipping;
  [[compilation_constant]] const bool use_dashed;
  [[compilation_constant]] const bool use_lighting;

  [[push_constant]] const float4x4 ModelViewProjectionMatrix;
  [[push_constant, condition(use_lighting)]] const float3x3 NormalMatrix;
  [[push_constant, condition(use_dashed)]] const float2 viewport_size;

  [[push_constant, condition(use_uniform_color)]] const float4 color;
  [[push_constant, condition(use_uniform_point_size &&use_point_size)]] const float size;
};

/* Vertex attributes. Unused ones will be optimized out. */
struct VertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1), condition(!use_uniform_color)]] float4 color;
  [[attribute(2), condition(use_lighting)]] float3 nor;
};

struct VertOutFlat {
  [[flat]] float4 color;
};

struct VertOutSmooth {
  [[smooth]] float4 color;
};

struct VertOutLit {
  [[smooth]] float3 normal;
};

struct VertOutDashed {
  /* We leverage hardware interpolation to compute distance along the line. */
  [[flat]] float2 stipple_start; /* In screen space */
  [[no_perspective]] float2 stipple_pos;
};

[[vertex]] void simple_vert(
    [[resource_table]] const Simple &srt,
    [[resource_table]] [[condition(use_clipping)]] const ClipPlanes &clip,
    [[in]] const VertIn &v_in,
    [[position]] float4 &out_pos,
    [[out]] [[condition(!use_smooth_color)]] VertOutFlat &flat_out,
    [[out]] [[condition(use_smooth_color)]] VertOutSmooth &smooth_out,
    [[out]] [[condition(use_dashed)]] VertOutDashed &dashed_out,
    [[out]] [[condition(use_lighting)]] VertOutLit &lit_out,
    [[clip_distance]] [[condition(use_clipping)]] float (&clip_distance)[6])
{
  out_pos = srt.ModelViewProjectionMatrix * float4(v_in.pos, 1.0f);

  if (srt.use_uniform_color) [[static_branch]] {
    flat_out.color = srt.color;
  }
  else if (srt.use_smooth_color) [[static_branch]] {
    smooth_out.color = v_in.color;
  }
  else {
    flat_out.color = v_in.color;
  }

  if (srt.use_lighting) [[static_branch]] {
    lit_out.normal = normalize(srt.NormalMatrix * v_in.nor);
  }

  if (srt.use_dashed) [[static_branch]] {
    dashed_out.stipple_pos = srt.viewport_size * 0.5f * (out_pos.xy / out_pos.w);
    dashed_out.stipple_start = dashed_out.stipple_pos;
  }

  if (srt.use_clipping) [[static_branch]] {
    float4 ws_pos = clip.ClipModelMatrix * float4(v_in.pos, 1.0f);
    clip_distance[0] = dot(clip.clip_planes.world[0], ws_pos);
    clip_distance[1] = dot(clip.clip_planes.world[1], ws_pos);
    clip_distance[2] = dot(clip.clip_planes.world[2], ws_pos);
    clip_distance[3] = dot(clip.clip_planes.world[3], ws_pos);
    clip_distance[4] = dot(clip.clip_planes.world[4], ws_pos);
    clip_distance[5] = dot(clip.clip_planes.world[5], ws_pos);
  }
}

enum PointStyle : int {
  POINT_SQUARE = 1,
  POINT_CIRCLE = 2,
  POINT_CIRCLE_AA = 3,
  POINT_CIRCLE_AA_OUTLINE = 4,
};

struct SimplePoint {
  [[compilation_constant]] const int point_style;
  [[compilation_constant]] const bool use_uniform_color;
  [[compilation_constant]] const bool use_uniform_point_size;
  [[compilation_constant]] const bool use_clipping;

  [[push_constant]] const float4x4 ModelViewProjectionMatrix;

  [[push_constant, condition(use_uniform_color)]] const float4 color;
  [[push_constant, condition(use_uniform_point_size)]] const float size;
  [[push_constant, condition(point_style == 4)]] const float4 outlineColor;
  [[push_constant, condition(point_style == 4)]] const float outlineWidth;
};

struct VertInPoint {
  [[attribute(0)]] float3 pos;
  [[attribute(1), condition(!use_uniform_color)]] float4 color;
  [[attribute(2), condition(!use_uniform_point_size)]] float size;
};

struct VertOutPoint {
  [[flat]] float4 color;
  [[flat]] float4 radii;
};

[[vertex]] void simple_point_vert(
    [[resource_table]] const SimplePoint &srt,
    [[resource_table]] [[condition(use_clipping)]] const ClipPlanes &clip,
    [[in]] const VertInPoint &v_in,
    [[position]] float4 &out_pos,
    [[out]] VertOutPoint &v_out,
    [[point_size]] [[condition(point_style != 0)]] float &out_point_size,
    [[clip_distance]] [[condition(use_clipping)]] float (&clip_distance)[6])
{
  out_pos = srt.ModelViewProjectionMatrix * float4(v_in.pos, 1.0f);

  if (srt.use_uniform_color) [[static_branch]] {
    v_out.color = srt.color;
  }
  else {
    v_out.color = v_in.color;
  }

  if (srt.use_uniform_point_size) [[static_branch]] {
    out_point_size = srt.size;
  }
  else {
    out_point_size = v_in.size;
  }

  /* Calculate concentric radii in pixels. */
  float radius = 0.5f * out_point_size;
  /* Start at the outside and progress toward the center. */
  v_out.radii = float2(radius, radius - 1.0f).xyxy;
  if (srt.point_style == 4 /* POINT_CIRCLE_AA_OUTLINE */) [[static_branch]] {
    v_out.radii.zw -= srt.outlineWidth;
  }
  /* Convert to PointCoord units. */
  v_out.radii /= out_point_size;

  if (srt.use_clipping) [[static_branch]] {
    float4 ws_pos = clip.ClipModelMatrix * float4(v_in.pos, 1.0f);
    clip_distance[0] = dot(clip.clip_planes.world[0], ws_pos);
    clip_distance[1] = dot(clip.clip_planes.world[1], ws_pos);
    clip_distance[2] = dot(clip.clip_planes.world[2], ws_pos);
    clip_distance[3] = dot(clip.clip_planes.world[3], ws_pos);
    clip_distance[4] = dot(clip.clip_planes.world[4], ws_pos);
    clip_distance[5] = dot(clip.clip_planes.world[5], ws_pos);
  }
}

[[fragment]] void simple_point_frag(
    [[resource_table]] const SimplePoint &srt,
    [[resource_table]] const ColorSpace &colorspace,
    [[point_coord]] const float2 pt_coord,
    [[in]] [[condition(!use_smooth_color)]] const VertOutPoint &v_out,
    [[in]] [[condition(use_smooth_color)]] const VertOutSmooth &smooth_out,
    [[out]] FragOut &frag_out)
{
  /* transparent outside of point
   * --- 0 ---
   * smooth transition
   * --- 1 ---
   * pure outline color
   * --- 2 ---
   * smooth transition
   * --- 3 ---
   * pure point color
   * ...
   * dist = 0 at center of point */
  float dist = length(pt_coord - float2(0.5f));

  if (srt.point_style == 4 /* POINT_CIRCLE_AA_OUTLINE */) [[static_branch]] {
    float fac = smoothstep(v_out.radii[3], v_out.radii[2], dist);
    frag_out.color = mix(v_out.color, srt.outlineColor, fac);
  }
  else {
    frag_out.color = v_out.color;
  }

  if (srt.point_style > 1 /* POINT_SQUARE */) [[static_branch]] {
    if (srt.point_style >= 3 /* POINT_CIRCLE_AA */) [[static_branch]] {
      /* Soft transition to 0 alpha. */
      frag_out.color.a *= mix(1.0f, 0.0f, smoothstep(v_out.radii[1], v_out.radii[0], dist));
    }

    if (frag_out.color.a == 0.0f) {
      gpu_discard_fragment();
    }
  }
}

[[fragment]] void simple_frag(
    [[resource_table]] const Simple &srt,
    [[resource_table]] const ColorSpace &colorspace,
    [[in]] [[condition(!use_smooth_color)]] const VertOutFlat &flat_out,
    [[in]] [[condition(use_smooth_color)]] const VertOutSmooth &smooth_out,
    [[out]] FragOut &frag_out)
{
  if (srt.use_smooth_color) [[static_branch]] {
    frag_out.color = smooth_out.color;
  }
  else {
    frag_out.color = flat_out.color;
  }

  frag_out.color = colorspace.rec709_srgb_to_output_space(frag_out.color);
}

struct SimpleClip {
  [[push_constant]] const float4x4 ModelViewProjectionMatrix;
  [[push_constant]] const float4x4 ModelMatrix;
  [[push_constant]] const float4 ClipPlane;
  [[push_constant]] const float4 color;
};

[[vertex]] void simple_clip([[resource_table]] const SimpleClip &srt,
                            [[in]] const VertIn &v_in,
                            [[position]] float4 &out_pos,
                            [[out]] VertOutFlat &flat_out,
                            [[clip_distance]] float (&clip_distance)[1])
{
  out_pos = srt.ModelViewProjectionMatrix * float4(v_in.pos, 1.0f);
  clip_distance[0] = dot(srt.ModelMatrix * float4(v_in.pos, 1.0f), srt.ClipPlane);
  flat_out.color = srt.color;
}

struct Checker {
  [[push_constant]] const float4 color1;
  [[push_constant]] const float4 color2;
  [[push_constant]] const int size;
};

[[fragment]] void checker_frag([[resource_table]] const Checker &srt,
                               [[frag_coord]] const float4 frag_co,
                               [[out]] FragOut &frag_out)
{
  float2 phase = mod(frag_co.xy, (srt.size * 2)) / float(srt.size);
  frag_out.color = ((phase.x > 1.0f) != (phase.y > 1.0f)) ? srt.color1 : srt.color2;
}

struct DiagStripes {
  [[push_constant]] const float4 color1;
  [[push_constant]] const float4 color2;
  [[push_constant]] const int size1;
  [[push_constant]] const int size2;
};

[[fragment]] void diag_stripes_frag([[resource_table]] const DiagStripes &srt,
                                    [[frag_coord]] const float4 frag_co,
                                    [[out]] FragOut &frag_out)
{
  float phase = mod((frag_co.x + frag_co.y), float(srt.size1 + srt.size2)) / float(srt.size1);
  frag_out.color = (phase < 1.0f) ? srt.color1 : srt.color2;
}

struct Dashed {
  [[push_constant]] const float4 color;
  [[push_constant]] const float4 color2;
  /* TODO(fclem): Remove this. And decide to discard if color2 alpha is 0. */
  [[push_constant]] const int colors_len; /* Enabled if > 0, 1 for solid line. */
  [[push_constant]] const float dash_width;
  [[push_constant]] const float udash_factor; /* if > 1.0f, solid line. */
};

[[fragment]] void dashed_line_frag([[resource_table]] const Dashed &srt,
                                   [[frag_coord]] const float4 frag_co,
                                   [[in]] const VertOutDashed &v_out,
                                   [[out]] FragOut &frag_out)
{
  float distance_along_line = distance(v_out.stipple_pos, v_out.stipple_start);
  /* Solid line case, simple. */
  if (srt.udash_factor >= 1.0f) {
    frag_out.color = srt.color;
  }
  /* Actually dashed line... */
  else {
    float normalized_distance = fract(distance_along_line / srt.dash_width);
    if (normalized_distance <= srt.udash_factor) {
      frag_out.color = srt.color;
    }
    else if (srt.colors_len > 0) {
      frag_out.color = srt.color2;
    }
    else {
      gpu_discard_fragment();
    }
  }
}

struct SimpleLit {
  [[uniform(0), frequency(PASS)]] SimpleLightingData &simple_lighting_data;
};

[[fragment]] void simple_lit_frag([[resource_table]] const SimpleLit &srt,
                                  [[frag_coord]] const float4 frag_co,
                                  [[in]] const VertOutLit &v_out,
                                  [[out]] FragOut &frag_out)
{
  frag_out.color = srt.simple_lighting_data.l_color;
  frag_out.color.xyz *= clamp(
      dot(normalize(v_out.normal), srt.simple_lighting_data.light), 0.0f, 1.0f);
}

}  // namespace builtin

/* clang-format off */
PipelineGraphic gpu_shader_3D_uniform_color(builtin::simple_vert, builtin::simple_frag, builtin::Simple{
  .use_uniform_color = true,
  .use_smooth_color = false,
  .use_clipping = false,
  .use_dashed = false,
  .use_lighting = false});
PipelineGraphic gpu_shader_3D_flat_color(builtin::simple_vert, builtin::simple_frag, builtin::Simple{
  .use_uniform_color = false,
  .use_smooth_color = false,
  .use_clipping = false,
  .use_dashed = false,
  .use_lighting = false});
PipelineGraphic gpu_shader_3D_smooth_color(builtin::simple_vert, builtin::simple_frag, builtin::Simple{
  .use_uniform_color = false,
  .use_smooth_color = true,
  .use_clipping = false,
  .use_dashed = false,
  .use_lighting = false});
PipelineGraphic gpu_shader_3D_line_dashed_uniform_color(builtin::simple_vert, builtin::dashed_line_frag, builtin::Simple{
  .use_uniform_color = true,
  .use_smooth_color = false,
  .use_clipping = false,
  .use_dashed = true});
PipelineGraphic gpu_shader_simple_lighting(builtin::simple_vert, builtin::simple_lit_frag, builtin::Simple{
  .use_uniform_color = true,
  .use_smooth_color = false,
  .use_clipping = false,
  .use_dashed = false,
  .use_lighting = true});

PipelineGraphic gpu_shader_3D_uniform_color_clipped(builtin::simple_vert, builtin::simple_frag, builtin::Simple{
  .use_uniform_color = true,
  .use_smooth_color = false,
  .use_clipping = true,
  .use_dashed = false,
  .use_lighting = false});
PipelineGraphic gpu_shader_3D_flat_color_clipped(builtin::simple_vert, builtin::simple_frag, builtin::Simple{
  .use_uniform_color = false,
  .use_smooth_color = false,
  .use_clipping = true,
  .use_dashed = false,
  .use_lighting = false});
PipelineGraphic gpu_shader_3D_smooth_color_clipped(builtin::simple_vert, builtin::simple_frag, builtin::Simple{
  .use_uniform_color = false,
  .use_smooth_color = true,
  .use_clipping = true,
  .use_dashed = false,
  .use_lighting = false});
PipelineGraphic gpu_shader_3D_line_dashed_uniform_color_clipped(builtin::simple_vert, builtin::dashed_line_frag, builtin::Simple{
  .use_uniform_color = true,
  .use_smooth_color = false,
  .use_clipping = true,
  .use_dashed = true,
  .use_lighting = false});

/* Confusing naming convention. But this is a version with only one local clip plane. */
PipelineGraphic gpu_shader_3D_clipped_uniform_color(builtin::simple_vert, builtin::simple_frag, builtin::Simple{
  .use_uniform_color = true,
  .use_smooth_color = false,
  .use_clipping = false,
  .use_dashed = false,
  .use_lighting = false});

PipelineGraphic gpu_shader_3D_point_uniform_color(builtin::simple_point_vert, builtin::simple_point_frag, builtin::SimplePoint{
  .point_style = 1 /* SQUARE */,
  .use_uniform_color = true,
  .use_uniform_point_size = true,
  .use_clipping = false});
PipelineGraphic gpu_shader_3D_point_flat_color(builtin::simple_point_vert, builtin::simple_point_frag, builtin::SimplePoint{
  .point_style = 1 /* SQUARE */,
  .use_uniform_color = false,
  .use_uniform_point_size = true,
  .use_clipping = false});
PipelineGraphic gpu_shader_3D_point_varying_size_varying_color(builtin::simple_point_vert, builtin::simple_point_frag, builtin::SimplePoint{
  .point_style = 1 /* SQUARE */,
  .use_uniform_color = false,
  .use_uniform_point_size = false,
  .use_clipping = false});
PipelineGraphic gpu_shader_3D_point_uniform_size_uniform_color_aa(builtin::simple_point_vert, builtin::simple_point_frag, builtin::SimplePoint{
  .point_style = 3 /* POINT_CIRCLE_AA */,
  .use_uniform_color = true,
  .use_uniform_point_size = true,
  .use_clipping = false});
PipelineGraphic gpu_shader_3D_point_uniform_size_uniform_color_outline_aa(builtin::simple_point_vert, builtin::simple_point_frag, builtin::SimplePoint{
  .point_style = 4 /* POINT_CIRCLE_AA_OUTLINE */,
  .use_uniform_color = true,
  .use_uniform_point_size = true,
  .use_clipping = false});

PipelineGraphic gpu_shader_3D_point_uniform_size_uniform_color_aa_clipped(builtin::simple_point_vert, builtin::simple_point_frag, builtin::SimplePoint{
  .point_style = 3 /* POINT_CIRCLE_AA */,
  .use_uniform_color = true,
  .use_uniform_point_size = true,
  .use_clipping = true});

PipelineGraphic gpu_shader_2D_checker(builtin::simple_vert, builtin::checker_frag, builtin::Simple{
  .use_uniform_color = true,
  .use_smooth_color = false,
  .use_clipping = false,
  .use_dashed = false,
  .use_lighting = false});

PipelineGraphic gpu_shader_2D_diag_stripes(builtin::simple_vert, builtin::diag_stripes_frag, builtin::Simple{
  .use_uniform_color = true,
  .use_smooth_color = false,
  .use_clipping = false,
  .use_dashed = false,
  .use_lighting = false});
/* clang-format on */
