/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Simple down-sample shader. Takes the average of the 4 texels of lower mip.
 */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

void main()
{
#if 0
  /* Reconstructing Target uvs like this avoid missing pixels if NPO2 */
  vec2 uvs = gl_FragCoord.xy * 2.0 / vec2(textureSize(source, 0).xy);

  FragColor = textureLod(source, vec3(uvs, lightprobe_geom_iface.layer), 0.0);
#else
  vec2 texel_size = 1.0 / vec2(textureSize(source, 0).xy);
  vec2 uvs = gl_FragCoord.xy * 2.0 * texel_size;
  vec4 ofs = texel_size.xyxy * vec4(0.75, 0.75, -0.75, -0.75);

  FragColor = textureLod(source, vec3(uvs + ofs.xy, lightprobe_geom_iface.layer), 0.0);
  FragColor += textureLod(source, vec3(uvs + ofs.xw, lightprobe_geom_iface.layer), 0.0);
  FragColor += textureLod(source, vec3(uvs + ofs.zy, lightprobe_geom_iface.layer), 0.0);
  FragColor += textureLod(source, vec3(uvs + ofs.zw, lightprobe_geom_iface.layer), 0.0);
  FragColor *= 0.25;

  /* Clamped brightness. */
  float luma = max(1e-8, max_v3(FragColor.rgb));
  FragColor *= 1.0 - max(0.0, luma - fireflyFactor) / luma;
#endif
}
