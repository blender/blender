/* SPDX-FileCopyrightText: 2016-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_colorspace_lib.glsl)

void main()
{
  float dist = length(gl_PointCoord - vec2(0.5));

  /* transparent outside of point
   * --- 0 ---
   * smooth transition
   * --- 1 ---
   * pure point color
   * ...
   * dist = 0 at center of point */

  fragColor = blender_srgb_to_framebuffer_space(color);
  fragColor.a = mix(color.a, 0.0, smoothstep(radii[1], radii[0], dist));

  if (fragColor.a == 0.0) {
    discard;
  }
}
