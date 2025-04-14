/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_info.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_edit_uv_edges)

/**
 * We want to know how much a pixel is covered by a line.
 * We replace the square pixel with a circle of the same area and try to find the intersection
 * area. The area we search is the circular segment. https://en.wikipedia.org/wiki/Circular_segment
 * The formula for the area uses inverse trig function and is quite complex. Instead,
 * we approximate it by using the smooth-step function and a 1.05 factor to the disc radius.
 */
#define M_1_SQRTPI 0.5641895835477563f /* `1/sqrt(pi)`. */
#define DISC_RADIUS (M_1_SQRTPI * 1.05f)
#define GRID_LINE_SMOOTH_START (0.5f - DISC_RADIUS)
#define GRID_LINE_SMOOTH_END (0.5f + DISC_RADIUS)

#include "draw_object_infos_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "overlay_common_lib.glsl"

void main()
{
  float4 inner_color = float4(float3(0.0f), 1.0f);
  float4 outer_color = float4(0.0f);

  float2 dd = fwidth(stipplePos);
  float line_distance = distance(stipplePos, stippleStart) / max(dd.x, dd.y);

  if (lineStyle == OVERLAY_UV_LINE_STYLE_OUTLINE) {
    if (use_edge_select) {
      /* TODO(@ideasman42): The current wire-edit color contrast enough against the selection.
       * Look into changing the default theme color instead of reducing contrast with edge-select.
       */
      inner_color = (selectionFac != 0.0f) ? colorEdgeSelect : (colorWireEdit * 0.5f);
    }
    else {
      inner_color = mix(colorWireEdit, colorEdgeSelect, selectionFac);
    }
    outer_color = float4(float3(0.0f), 1.0f);
  }
  else if (lineStyle == OVERLAY_UV_LINE_STYLE_DASH) {
    if (fract(line_distance / dashLength) < 0.5f) {
      inner_color = mix(float4(float3(0.35f), 1.0f), colorEdgeSelect, selectionFac);
    }
  }
  else if (lineStyle == OVERLAY_UV_LINE_STYLE_BLACK) {
    float4 base_color = float4(float3(0.0f), 1.0f);
    inner_color = mix(base_color, colorEdgeSelect, selectionFac);
  }
  else if (lineStyle == OVERLAY_UV_LINE_STYLE_WHITE) {
    float4 base_color = float4(1.0f);
    inner_color = mix(base_color, colorEdgeSelect, selectionFac);
  }
  else if (lineStyle == OVERLAY_UV_LINE_STYLE_SHADOW) {
    inner_color = colorUVShadow;
  }

  float dist = abs(edgeCoord) - max(sizeEdge - 0.5f, 0.0f);
  float dist_outer = dist - max(sizeEdge, 1.0f);
  float mix_w;
  float mix_w_outer;

  if (doSmoothWire) {
    mix_w = smoothstep(GRID_LINE_SMOOTH_START, GRID_LINE_SMOOTH_END, dist);
    mix_w_outer = smoothstep(GRID_LINE_SMOOTH_START, GRID_LINE_SMOOTH_END, dist_outer);
  }
  else {
    mix_w = step(0.5f, dist);
    mix_w_outer = step(0.5f, dist_outer);
  }

  float4 final_color = mix(outer_color, inner_color, 1.0f - mix_w * outer_color.a);
  final_color.a *= 1.0f - (outer_color.a > 0.0f ? mix_w_outer : mix_w);

  eObjectInfoFlag ob_flag = drw_object_infos().flag;
  bool is_active = flag_test(ob_flag, OBJECT_ACTIVE);
  final_color.a *= is_active ? alpha : (alpha * 0.25f);

  fragColor = final_color;
}
