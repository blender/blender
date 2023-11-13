/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

void main()
{
  /* Extract pixel motion vector from camera movement. */
  ivec2 texel = ivec2(gl_FragCoord.xy);
  vec2 uv_curr = gl_FragCoord.xy / vec2(textureSize(depthBuffer, 0).xy);

  float depth = texelFetch(depthBuffer, texel, 0).r;

  uv_curr = uv_curr * 2.0 - 1.0;
  depth = depth * 2.0 - 1.0;

  vec3 world_position = project_point(currViewProjMatrixInv, vec3(uv_curr, depth));
  vec2 uv_prev = project_point(prevViewProjMatrix, world_position).xy;
  vec2 uv_next = project_point(nextViewProjMatrix, world_position).xy;

  outData.xy = uv_prev - uv_curr;
  outData.zw = uv_next - uv_curr;

  /* Encode to unsigned normalized 16bit texture. */
  outData = outData * 0.5 + 0.5;
}
