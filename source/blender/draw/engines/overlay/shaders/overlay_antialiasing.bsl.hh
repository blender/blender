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
#pragma create_info

#include "gpu_shader_compat.hh"
#include "gpu_shader_fullscreen_lib.glsl"
#include "infos/overlay_common_infos.hh"
#include "overlay_shader_shared.hh"

SHADER_LIBRARY_CREATE_INFO(draw_globals)

namespace overlay::antialiasing {

/* Per-pixel line data, unpacked as a direction and covered distance. */
struct Line {
  float2 dir;
  float dist;
  float dist_raw;

  static Line decode(float3 data)
  {
    return {
        .dir = data.xy * 2.0f - 1.0f,
        .dist = (data.z - 0.1f) * 4.0f - 2.0f,
        .dist_raw = data.z,
    };
  }

  bool is_valid() const
  {
    return dist_raw != 0.0f;
  }
};

struct TexelData {
  float4 color;
  float depth;
  Line line;
};

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_globals;

  [[sampler(0)]] const sampler2DDepth depth_tx;
  [[sampler(1)]] const sampler2D color_tx;
  [[sampler(2)]] const sampler2D line_tx;

  [[push_constant]] bool do_smooth_lines;

  TexelData fetch_texel(int2 texel, int2 offset)
  {
    int2 texel_actual = texel + offset;
    return {
        .color = texelFetch(color_tx, texel_actual, 0),
        .depth = texelFetch(depth_tx, texel_actual, 0).r,
        .line = Line::decode(texelFetch(line_tx, texel_actual, 0).rgb),
    };
  }
};

/**
 * Coverage of a line onto a sample that is `distance_to_Line` pixels from the line.
 * Here, `line_kernel_size` is the inner size of the line with 100% coverage.
 */
template<typename T>
T line_coverage(T distance_to_line, float line_kernel_size, bool do_smooth_lines)
{
  if (do_smooth_lines) {
    return smoothstep(
        LINE_SMOOTH_END, LINE_SMOOTH_START, abs(distance_to_line) - line_kernel_size);
  }
  return step(-0.5f, line_kernel_size - abs(distance_to_line));
}

template float line_coverage<float>(float, float, bool);
template float4 line_coverage<float4>(float4, float, bool);

/**
 * Compute distance-to-line for one of the neighboring crosshair pixels, dependent
 * on whether that pixel has influence or not; in which case distance is set to
 * a maximal value.
 */
float neighbor_dist(const TexelData &neighbor, int2 offset)
{
  bool is_dir_horizontal = abs(neighbor.line.dir.x) > abs(neighbor.line.dir.y);
  bool is_ofs_horizontal = offset.x != 0;

  if (!neighbor.line.is_valid() || is_ofs_horizontal != is_dir_horizontal) {
    return 1e10f; /* No line. */
  }

  /* Add projection of the line direction onto a unit vector. */
  return neighbor.line.dist + dot(float2(offset), -neighbor.line.dir);
}

/**
 * Blend the neighbor pixel onto the target pixel, based on the pixels'
 * relative depths doing alpha-over or alpha-under. The resulting pixel's
 * depth is then adjusted to the closest depth.
 */
void neighbor_blend(TexelData neighbor, TexelData &target, float line_coverage)
{
  neighbor.color *= line_coverage;
  if (line_coverage > 0.0f && neighbor.depth < target.depth) {
    /* Alpha over blending, and update to new closest target depth. */
    target.color = neighbor.color + target.color * (1.0f - neighbor.color.a);
    target.depth = neighbor.depth;
  }
  else {
    /* Alpha under blending */
    target.color = target.color + neighbor.color * (1.0f - target.color.a);
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
  int2 texel = int2(frag_coord.xy);

  /* Fetch data at the center pixel. */
  TexelData center = srt.fetch_texel(texel, int2(0));

  /* Store until end of function; does the center pixel have alpha? */
  bool original_center_has_alpha = center.color.a < 1.0f;

  /* Guard; check if no expansion or AA should be applied. */
  if (!srt.do_smooth_lines && center.line.dist <= 1.0f) {
    frag.color = center.color;
    return;
  }

  /* Fetch data for a cross of neighboring pixels. */
  TexelData neighbors[] = {srt.fetch_texel(texel, int2(1, 0)),
                           srt.fetch_texel(texel, int2(-1, 0)),
                           srt.fetch_texel(texel, int2(0, 1)),
                           srt.fetch_texel(texel, int2(0, -1))};

  float4 neighbor_dists = float4(neighbor_dist(neighbors[0], int2(1, 0)),
                                 neighbor_dist(neighbors[1], int2(-1, 0)),
                                 neighbor_dist(neighbors[2], int2(0, 1)),
                                 neighbor_dist(neighbors[3], int2(0, -1)));

  /* Compute per-neighbor line coverage */
  float line_kernel = theme.sizes.pixel * 0.5f - 0.5f;
  float4 coverage = line_coverage(neighbor_dists, line_kernel, srt.do_smooth_lines);

  /* Multiply current output color by center pixel's line coverage. */
  if (center.line.is_valid()) {
    float coverage = line_coverage(center.line.dist, line_kernel, srt.do_smooth_lines);
    center.color *= coverage;
  }

  /* We don't order fragments; instead, we blend using alpha-over/alpha-under
   * based on the tracked depth of each neighbor pixel, using the center pixel
   * as reference input and tracked value. */
  neighbor_blend(neighbors[0], center, coverage.x);
  neighbor_blend(neighbors[1], center, coverage.y);
  neighbor_blend(neighbors[2], center, coverage.z);
  neighbor_blend(neighbors[3], center, coverage.w);

#if 1
  /* Fix aliasing issue with really dense meshes and 1 pixel sized lines. */
  if (!original_center_has_alpha && center.line.is_valid() && line_kernel < 0.45f) {
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

PipelineGraphic pipeline(vert_main, frag_main, Resources{});

}  // namespace overlay::antialiasing
