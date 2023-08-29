/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec4 refco = ProjectionMatrix * (ViewMatrix * vec4(worldPosition, 1.0));
  refco.xy /= refco.w;
  FragColor = vec4(
      textureLod(probePlanars, vec3(refco.xy * vec2(-0.5, 0.5) + 0.5, float(probeIdx)), 0.0).rgb,
      1.0);
}
