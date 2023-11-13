/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * We want to know how much a pixel is covered by a line.
 * We replace the square pixel with a circle of the same area and try to find the intersection
 * area. The area we search is the circular segment. https://en.wikipedia.org/wiki/Circular_segment
 * The formula for the area uses inverse trig function and is quite complex. Instead,
 * we approximate it by using the smooth-step function and a 1.05 factor to the disc radius.
 */

#define M_1_SQRTPI 0.5641895835477563 /* `1/sqrt(pi)`. */

#define DISC_RADIUS (M_1_SQRTPI * 1.05)
#define GRID_LINE_SMOOTH_START (0.5 - DISC_RADIUS)
#define GRID_LINE_SMOOTH_END (0.5 + DISC_RADIUS)

bool test_occlusion()
{
  return gl_FragCoord.z > texelFetch(depthTex, ivec2(gl_FragCoord.xy), 0).r;
}

float edge_step(float dist)
{
  if (do_smooth_wire) {
    return smoothstep(GRID_LINE_SMOOTH_START, GRID_LINE_SMOOTH_END, dist);
  }
  else {
    return step(0.5, dist);
  }
}

void main()
{
  float dist = abs(geometry_noperspective_out.edgeCoord) - max(sizeEdge - 0.5, 0.0);
  float dist_outer = dist - max(sizeEdge, 1.0);
  float mix_w = edge_step(dist);
  float mix_w_outer = edge_step(dist_outer);
  /* Line color & alpha. */
  fragColor = mix(geometry_flat_out.finalColorOuter,
                  geometry_out.finalColor,
                  1.0 - mix_w * geometry_flat_out.finalColorOuter.a);
  /* Line edges shape. */
  fragColor.a *= 1.0 - (geometry_flat_out.finalColorOuter.a > 0.0 ? mix_w_outer : mix_w);

  fragColor.a *= test_occlusion() ? alpha : 1.0;
}
