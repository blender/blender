/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * We want to know how much a pixel is covered by a line.
 * We replace the square pixel with a circle of the same area and try to find the intersection
 * area. The area we search is the circular segment. https://en.wikipedia.org/wiki/Circular_segment
 * The formula for the area uses inverse trig function and is quite complex. Instead,
 * we approximate it by using the smooth-step function and a 1.05 factor to the disc radius.
 */
#define M_1_SQRTPI 0.5641895835477563 /* 1/sqrt(pi) */
#define DISC_RADIUS (M_1_SQRTPI * 1.05)
#define GRID_LINE_SMOOTH_START (0.5 - DISC_RADIUS)
#define GRID_LINE_SMOOTH_END (0.5 + DISC_RADIUS)

#pragma BLENDER_REQUIRE(overlay_common_lib.glsl)

void main()
{
  vec4 inner_color = vec4(vec3(0.0), 1.0);
  vec4 outer_color = vec4(0.0);

  vec2 dd = fwidth(geom_noperspective_out.stipplePos);
  float line_distance = distance(geom_noperspective_out.stipplePos, geom_flat_out.stippleStart) /
                        max(dd.x, dd.y);

  if (lineStyle == OVERLAY_UV_LINE_STYLE_OUTLINE) {
#ifdef USE_EDGE_SELECT
    /* TODO(@ideasman42): The current wire-edit color contrast enough against the selection.
     * Look into changing the default theme color instead of reducing contrast with edge-select. */
    inner_color = (geom_out.selectionFac != 0.0) ? colorEdgeSelect : (colorWireEdit * 0.5);
#else
    inner_color = mix(colorWireEdit, colorEdgeSelect, geom_out.selectionFac);
#endif
    outer_color = vec4(vec3(0.0), 1.0);
  }
  else if (lineStyle == OVERLAY_UV_LINE_STYLE_DASH) {
    if (fract(line_distance / dashLength) < 0.5) {
      inner_color = mix(vec4(vec3(0.35), 1.0), colorEdgeSelect, geom_out.selectionFac);
    }
  }
  else if (lineStyle == OVERLAY_UV_LINE_STYLE_BLACK) {
    vec4 base_color = vec4(vec3(0.0), 1.0);
    inner_color = mix(base_color, colorEdgeSelect, geom_out.selectionFac);
  }
  else if (lineStyle == OVERLAY_UV_LINE_STYLE_WHITE) {
    vec4 base_color = vec4(1.0);
    inner_color = mix(base_color, colorEdgeSelect, geom_out.selectionFac);
  }
  else if (lineStyle == OVERLAY_UV_LINE_STYLE_SHADOW) {
    inner_color = colorUVShadow;
  }

  float dist = abs(geom_noperspective_out.edgeCoord) - max(sizeEdge - 0.5, 0.0);
  float dist_outer = dist - max(sizeEdge, 1.0);
  float mix_w;
  float mix_w_outer;

  if (doSmoothWire) {
    mix_w = smoothstep(GRID_LINE_SMOOTH_START, GRID_LINE_SMOOTH_END, dist);
    mix_w_outer = smoothstep(GRID_LINE_SMOOTH_START, GRID_LINE_SMOOTH_END, dist_outer);
  }
  else {
    mix_w = step(0.5, dist);
    mix_w_outer = step(0.5, dist_outer);
  }

  vec4 final_color = mix(outer_color, inner_color, 1.0 - mix_w * outer_color.a);
  final_color.a *= 1.0 - (outer_color.a > 0.0 ? mix_w_outer : mix_w);
  final_color.a *= alpha;

  fragColor = final_color;
}
