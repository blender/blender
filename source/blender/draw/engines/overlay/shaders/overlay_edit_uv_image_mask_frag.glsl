/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_colormanagement_lib.glsl)

void main()
{
  vec2 uvs_clamped = clamp(uvs, 0.0, 1.0);
  float mask_value = texture_read_as_linearrgb(imgTexture, true, uvs_clamped).r;
  fragColor = vec4(color.rgb * mask_value, color.a);
}
