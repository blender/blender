/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

#pragma BLENDER_REQUIRE(volumetric_lib.glsl)

/* Step 4 : Apply final integration on top of the scene color. */

void main()
{
  vec2 uvs = gl_FragCoord.xy / vec2(textureSize(inSceneDepth, 0));
  float scene_depth = texture(inSceneDepth, uvs).r;

  vec3 transmittance, scattering;
  volumetric_resolve(uvs, scene_depth, transmittance, scattering);

  /* Approximate volume alpha by using a monochromatic transmittance
   * and adding it to the scene alpha. */
  float alpha = dot(transmittance, vec3(1.0 / 3.0));

  FragColor0 = vec4(scattering, 1.0 - alpha);
  FragColor1 = vec4(transmittance, alpha);
}
