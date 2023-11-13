/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_colormanagement_lib.glsl)

void main()
{
  vec4 mask = vec4(texture_read_as_srgb(maskImage, maskImagePremultiplied, uv_interp).rgb, 1.0);
  if (maskInvertStencil) {
    mask.rgb = 1.0 - mask.rgb;
  }
  float mask_step = smoothstep(0.0, 3.0, mask.r + mask.g + mask.b);
  mask.rgb *= maskColor;
  mask.a = mask_step * opacity;

  fragColor = mask;
}
