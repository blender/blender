/* SPDX-FileCopyrightText: 2019-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Overlay anti-aliasing:
 *
 * Single sample per pixel screen-space AA pass for wires and wireframe
 * overlays, refer to `overlay_antialiasing.hh` for a breakdown. This
 * can be toggled in `Settings > Viewport > Smooth Wires > Overlay`.
 */

#pragma once

#include "gpu_shader_compat.hh"
#include "gpu_shader_fullscreen_lib.glsl"
#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_vector_compare_lib.glsl"
#include "infos/overlay_common_infos.hh"
#include "overlay_common_lib.glsl"
#include "overlay_shader_shared.hh"

SHADER_LIBRARY_CREATE_INFO(draw_globals)

namespace overlay::antialiasing {

/* Per-pixel line data, unpacked as its perpendicular direction, and distance to origin. */
struct Line {
  float2 dir;
  float dist;
  float dist_raw;

  /**
   * Create a default invalid line.
   */
  static Line zero()
  {
    return {.dir = float2(0.0f), .dist = 0.0f, .dist_raw = 0.0f};
  }

  static Line unpack(float2 data)
  {
    /* If the data indicates a blocked pixel, store 0.0f instead of 1.0f,
     * specifically for the small line fix at the bottom.. */
    float dist_raw = data.y == 1.0f ? 0.0f : data.y;

    Line line = {.dist_raw = dist_raw};
    unpack_line_data(data, line.dir, line.dist);
    return line;
  }

  bool is_valid() const
  {
    /* If 0.0: likely no write was performed to this pixel.
     * If 1.0: a line overlay wrote to this pixel indicating it does its own AA. */
    return dist_raw != 0.0f && dist_raw != 1.0f;
  }

  /**
   * Coverage of a line based on its stored distance to origin. Here, `kernel_size` is
   * the inner size of the line with 100% coverage.
   */
  float coverage(float kernel_size, bool do_smooth_lines) const
  {
    if (do_smooth_lines) {
      return smoothstep(LINE_SMOOTH_END, LINE_SMOOTH_START, abs(dist) - kernel_size);
    }
    return step(-0.5f, kernel_size - abs(dist));
  }

  /**
   * Update the stored line distance so it represents a neighboring pixel;
   * if the neighbor should not have influence, distance is set to a maximal value.
   */
  void offset_to_neighbor(int2 offset)
  {
    bool is_dir_horizontal = abs(dir.x) > abs(dir.y);
    bool is_ofs_horizontal = offset.x != 0;

    if (!is_valid() || is_ofs_horizontal != is_dir_horizontal) {
      /* No line. */
      dist = 1e10f;
      return;
    }

    /* Add projection of the line direction onto a unit vector. */
    dist = dist + dot(float2(offset), -dir);
  }
};

struct TexelData {
  float4 color;
  float depth;
  Line line;

  static TexelData zero()
  {
    return {.color = float4(0.0f), .depth = 1.0f, .line = Line::zero()};
  }
};

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_globals;

  [[sampler(0)]] const sampler2DDepth depth_tx;
  [[sampler(1)]] const sampler2D color_tx;
  [[sampler(2)]] const sampler2D line_tx;

  [[push_constant]] const bool do_smooth_lines;
  [[push_constant]] const bool do_background_fetch;

  TexelData fetch_texel(int2 texel, int2 offset)
  {
    int2 texel_actual = texel + offset;
    TexelData data = {
        .color = texelFetch(color_tx, texel_actual, 0),
        .depth = texelFetch(depth_tx, texel_actual, 0).r,
        .line = Line::unpack(texelFetch(line_tx, texel_actual, 0).rg),
    };
    if (any(notEqual(offset, int2(0)))) {
      data.line.offset_to_neighbor(offset);
    }
    return data;
  }
};

/**
 * Return the furthest non-line texel in the neighboring crosshair, or the
 * center if there is none.
 */
TexelData furthest_texel(TexelData center, TexelData neighbors[4], bool do_background_fetch)
{
  if (!do_background_fetch) {
    return TexelData::zero();
  }

  TexelData furthest = center;
  for (int i = 0; i < 4; i++) [[unroll]] {
    if (neighbors[i].depth > furthest.depth) {
      furthest = neighbors[i];
    }
  }
  return furthest;
}

/**
 * Compute the center pixel's line coverage, and blend it over the current background.
 */
void center_kernel(TexelData &target,
                   const TexelData background,
                   float kernel_size,
                   bool do_smooth_lines)
{
  if (!target.line.is_valid()) {
    return;
  }

  float coverage = target.line.coverage(kernel_size, do_smooth_lines);
  target.color = mix(background.color, target.color, coverage);
}

/**
 * Compute a neighbor pixel's line coverage, and alpha-over/alpha-under blend it with
 * current center. The target pixel tracks the closest depth of center/neighbor.
 */
void neighbor_kernel(TexelData &target,
                     TexelData neighbor,
                     TexelData background,
                     float kernel_size,
                     bool do_smooth_lines)
{
  if (!neighbor.line.is_valid()) {
    return;
  }

  float coverage = neighbor.line.coverage(kernel_size, do_smooth_lines);
  if (coverage == 0.0f) {
    return;
  }

  /* Blend neighbor over background based on their respective line coverages. */
  neighbor.color = mix(background.color, neighbor.color, coverage);

  /* Select over/under for alpha blending, based on relative depth. */
  bool target_over_neighbor = target.depth < neighbor.depth;
  float4 over = target_over_neighbor ? target.color : neighbor.color;
  float4 under = target_over_neighbor ? neighbor.color : target.color;

  if (background.color.a == 0.0) {
    /* Without background, we do additive alpha on the current frame-buffer. */
    target.color = over + under * (1.0f - over.a);
  }
  else {
    /* With background, we have to avoid pre-multiplied alpha. */
    under.a *= (1.0f - over.a);
    target.color = (over * over.a + under * under.a) / (over.a + under.a);
  }

  if (!target_over_neighbor) {
    target.depth = neighbor.depth;
  }
}

[[vertex]] void vert_main([[vertex_id]] const int &vert_id, [[position]] float4 &vert)
{
  fullscreen_vertex(vert_id, vert);
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]] void frag_main([[frag_coord]] const float4 &frag_coord,
                            [[resource_table]] Resources &srt,
                            [[out]] FragOut &frag)
{
  const int2 texel = int2(frag_coord.xy);

  /* Fetch center pixel. */
  TexelData center = srt.fetch_texel(texel, int2(0));

  /* Guard; check if no expansion or AA should be applied. */
  if (!srt.do_smooth_lines && center.line.dist <= 1.0f) {
    frag.color = center.color;
    return;
  }

  /* Fetch a cross of neighboring pixels. */
  TexelData neighbors[] = {srt.fetch_texel(texel, int2(1, 0)),
                           srt.fetch_texel(texel, int2(-1, 0)),
                           srt.fetch_texel(texel, int2(0, 1)),
                           srt.fetch_texel(texel, int2(0, -1))};

  /* Find the furthest non-line texel among neighbors. */
  TexelData background = furthest_texel(center, neighbors, srt.do_background_fetch);

  /* Store until after blend, does the center pixel have alpha? */
  const bool original_center_has_alpha = center.color.a < 1.0f;

  /* Blend center color over background. */
  const float kernel_size = theme.sizes.pixel * 0.5f - 0.5f;
  center_kernel(center, background, kernel_size, srt.do_smooth_lines);
  /* We don't order fragments; instead blending neighbors alpha-over/alpha-under based on
   * a tracked depth for each neighbor, using the center pixel as starting reference. */
  neighbor_kernel(center, neighbors[0], background, kernel_size, srt.do_smooth_lines);
  neighbor_kernel(center, neighbors[1], background, kernel_size, srt.do_smooth_lines);
  neighbor_kernel(center, neighbors[2], background, kernel_size, srt.do_smooth_lines);
  neighbor_kernel(center, neighbors[3], background, kernel_size, srt.do_smooth_lines);

#if 1
  /* Fix aliasing issue with really dense meshes and 1 pixel sized lines. */
  if (!original_center_has_alpha && center.line.is_valid() && kernel_size < 0.45f) {
    float4 lines_raw = float4(neighbors[0].line.dist_raw,
                              neighbors[1].line.dist_raw,
                              neighbors[2].line.dist_raw,
                              neighbors[3].line.dist_raw);
    float blend = dot(float4(0.25f), step(1e-2f, lines_raw));

    /* Only do blend if there are more than 2 neighbors, to avoid losing too much AA. */
    blend = clamp(blend * 2.0f - 1.0f, 0.0f, 1.0f);
    center.color = mix(center.color, center.color / center.color.a, blend);
  }
#endif

  frag.color = center.color;
}

PipelineGraphic pipeline(vert_main, frag_main);

}  // namespace overlay::antialiasing
