/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_3D_polyline_infos.hh"

#include "gpu_shader_colorspace_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color)

void main()
{
#ifdef CLIP
  if (clip < 0.0f) {
    gpu_discard_fragment();
  }
#endif
  fragColor = final_color;
  if (lineSmooth) {
    fragColor.a *= clamp((lineWidth + SMOOTH_WIDTH) * 0.5f - abs(smoothline), 0.0f, 1.0f);
  }
  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
