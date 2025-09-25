/* SPDX-FileCopyrightText: 2016-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_point_uniform_size_uniform_color_aa_infos.hh"

#include "gpu_shader_colorspace_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_point_uniform_size_uniform_color_aa)

void main()
{
  float dist = length(gl_PointCoord - float2(0.5f));

  /* transparent outside of point
   * --- 0 ---
   * smooth transition
   * --- 1 ---
   * pure point color
   * ...
   * dist = 0 at center of point */

  fragColor = blender_srgb_to_framebuffer_space(color);
  fragColor.a = mix(color.a, 0.0f, smoothstep(radii[1], radii[0], dist));

  if (fragColor.a == 0.0f) {
    gpu_discard_fragment();
  }
}
