/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_colorspace_lib.glsl)

void main()
{
#ifdef CLIP
  if (interp.clip < 0.0) {
    discard;
  }
#endif
  fragColor = interp.final_color;
  if (lineSmooth) {
    fragColor.a *= clamp(
        (lineWidth + SMOOTH_WIDTH) * 0.5 - abs(interp_noperspective.smoothline), 0.0, 1.0);
  }
  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
