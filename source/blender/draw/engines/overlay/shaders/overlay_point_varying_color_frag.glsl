/* SPDX-FileCopyrightText: 2016-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  vec2 centered = gl_PointCoord - vec2(0.5);
  float dist_squared = dot(centered, centered);
  const float rad_squared = 0.25;

  /* Round point with jaggy edges. */
  if (dist_squared > rad_squared) {
    discard;
    return;
  }

#if defined(VERT)
  fragColor = finalColor;

  float midStroke = 0.5 * rad_squared;
  if (vertexCrease > 0.0 && dist_squared > midStroke) {
    fragColor.rgb = mix(finalColor.rgb, colorEdgeCrease.rgb, vertexCrease);
  }
#else
  fragColor = finalColor;
#endif
}
