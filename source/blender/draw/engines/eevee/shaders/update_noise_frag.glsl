/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

void main(void)
{
  vec3 blue_noise = texelFetch(blueNoise, ivec2(gl_FragCoord.xy), 0).xyz;

  float noise = fract(blue_noise.y + offsets.z);
  FragColor.x = fract(blue_noise.x + offsets.x);
  FragColor.y = fract(blue_noise.z + offsets.y);
  FragColor.z = cos(noise * M_2PI);
  FragColor.w = sin(noise * M_2PI);
}
