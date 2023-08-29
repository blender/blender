/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#define mistStart mistSettings.x
#define mistInvDistance mistSettings.y
#define mistFalloff mistSettings.z

void main()
{
  vec2 texel_size = 1.0 / vec2(textureSize(depthBuffer, 0)).xy;
  vec2 uvs = gl_FragCoord.xy * texel_size;

  float depth = textureLod(depthBuffer, uvs, 0.0).r;
  vec3 co = get_view_space_from_depth(uvs, depth);

  float zcor = (ProjectionMatrix[3][3] == 0.0) ? length(co) : -co.z;

  /* bring depth into 0..1 range */
  float mist = saturate((zcor - mistStart) * mistInvDistance);

  /* falloff */
  mist = pow(mist, mistFalloff);

  fragColor = vec4(mist);

  // if (mist > 0.999) fragColor = vec4(1.0);
  // else if (mist > 0.0001) fragColor = vec4(0.5);
  // else fragColor = vec4(0.0);
}
