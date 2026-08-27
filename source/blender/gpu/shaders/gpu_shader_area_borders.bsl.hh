/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

namespace builtin {

namespace area_borders {

struct Resources {
  [[push_constant]] float4x4 ModelViewProjectionMatrix;

  [[push_constant]] float4 rect;
  [[push_constant]] float4 color;
  /* Amount of pixels the border can cover. Scales rounded corner radius. */
  [[push_constant]] float scale;
  /* Width of the border relative to the scale. Also affects rounded corner radius. */
  [[push_constant]] float width;
  [[push_constant]] int cornerLen;
};

struct VertIn {
  [[attribute(0)]] float2 pos;
};

struct VertOut {
  [[smooth]] float2 uv;
};

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[vertex]] void main_vert([[resource_table]] const Resources &srt,
                          [[in]] const VertIn &v_in,
                          [[out]] VertOut &v_out,
                          [[vertex_id]] const int vert_id,
                          [[position]] float4 &out_pos)
{
  int corner_id = (vert_id / srt.cornerLen) % 4;
  bool inner = all(lessThan(abs(v_in.pos), float2(1.0f)));

  /* Scale the inner part of the border.
   * Add a sub pixel offset to the outer part to make sure we don't miss a pixel row/column. */
  float2 final_pos = v_in.pos * ((inner) ? (1.0f - srt.width) : 1.05f);

  v_out.uv = final_pos;
  /* Rescale to the corner size and position the corner. */
  if (corner_id == 0) {
    /* top right */
    final_pos = (final_pos - float2(1.0f, 1.0f)) * srt.scale + srt.rect.yw;
  }
  else if (corner_id == 1) {
    /* top left */
    final_pos = (final_pos - float2(-1.0f, 1.0f)) * srt.scale + srt.rect.xw;
  }
  else if (corner_id == 2) {
    /* bottom left */
    final_pos = (final_pos - float2(-1.0f, -1.0f)) * srt.scale + srt.rect.xz;
  }
  else {
    /* bottom right */
    final_pos = (final_pos - float2(1.0f, -1.0f)) * srt.scale + srt.rect.yz;
  }

  out_pos = (srt.ModelViewProjectionMatrix * float4(final_pos, 0.0f, 1.0f));
}

[[fragment]] void main_frag([[resource_table]] const Resources &srt,
                            [[in]] const VertOut &v_out,
                            [[out]] FragOut &frag_out)
{
  /* Should be 1.0f but minimize the AA on the edges. */
  float dist = (length(v_out.uv) - (0.98f - srt.width)) * srt.scale;

  frag_out.color = srt.color;
  frag_out.color.a *= smoothstep(-0.09f, 1.09f, dist);
}

}  // namespace area_borders

namespace window_borders {

struct Resources {
  [[push_constant]] float4x4 ModelViewProjectionMatrix;

  /* The rectangle to round the corners of, as (`xmin`, `xmax`, `ymin`, `ymax`). */
  [[push_constant]] float4 rect;
  [[push_constant]] float4 color;
  /* The radius of each corner in pixels, counter-clockwise from the top right. A radius of zero
   * leaves that corner square. */
  [[push_constant]] float4 radii;
};

struct VertOut {
  /* Position relative to the center of the corner's arc, in pixels, mirrored so that the corner of
   * the rectangle is always in the positive quadrant. */
  [[smooth]] float2 arc_co;
  [[flat]] float arc_radius;
};

struct FragOut {
  [[frag_color(0)]] float4 color;
};

/**
 * Draws the anti-aliased area that a rounded corner cuts away from each corner of a rectangle.
 * Uses one instance per corner.
 */
[[vertex]] void main_vert([[resource_table]] const Resources &srt,
                          [[out]] VertOut &v_out,
                          [[instance_index]] const int inst_index,
                          [[vertex_id]] const int vert_id,
                          [[position]] float4 &out_pos)
{
  /* One quad per corner, counter-clockwise from the top right. */
  int corner_id = inst_index;
  float radius = srt.radii[corner_id];

  float2 corner_sign = float2((corner_id == 0 || corner_id == 3) ? 1.0f : -1.0f,
                              (corner_id == 0 || corner_id == 1) ? 1.0f : -1.0f);
  float2 corner_co = float2((corner_sign.x > 0.0f) ? srt.rect.y : srt.rect.x,
                            (corner_sign.y > 0.0f) ? srt.rect.w : srt.rect.z);

  /* Distance inside of the corner, spanning the square bounding the arc, expanded by a pixel. */
  float2 quad_co = float2(float(vert_id / 2), float(vert_id % 2));
  float2 inset = float2(-1.0f) + (quad_co * (radius + 1.0f));

  v_out.arc_co = float2(radius) - inset;
  v_out.arc_radius = radius;

  float2 final_pos = corner_co - (corner_sign * inset);
  out_pos = srt.ModelViewProjectionMatrix * float4(final_pos, 0.0f, 1.0f);
}

[[fragment]] void main_frag([[resource_table]] const Resources &srt,
                            [[in]] const VertOut &v_out,
                            [[out]] FragOut &frag_out)
{
  /* Distance to the arc, negative inside of the rounded rectangle. */
  float dist = length(v_out.arc_co) - v_out.arc_radius;

  frag_out.color = srt.color;
  /* A one pixel wide linear ramp centered on the arc. */
  frag_out.color.a *= clamp(dist + 0.5f, 0.0f, 1.0f);
}

}  // namespace window_borders

}  // namespace builtin

PipelineGraphic gpu_shader_2D_area_borders(builtin::area_borders::main_vert,
                                           builtin::area_borders::main_frag);
PipelineGraphic gpu_shader_2D_rounded_corner_mask(builtin::window_borders::main_vert,
                                                  builtin::window_borders::main_frag);
