/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute light objects lighting contribution using captured Gbuffer data.
 */

#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_eval_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;
  vec3 P = get_world_space_from_depth(uvcoordsvar.xy, depth);

  vec3 V = cameraVec(P);
  float vP_z = dot(cameraForward, P) - dot(cameraForward, cameraPos);

  GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel);

  vec3 Ng = gbuf.diffuse.N;

  vec3 diffuse_light = vec3(0.0);
  vec3 unused_reflection_light = vec3(0.0);
  vec3 unused_refraction_light = vec3(0.0);
  float unused_shadow = 1.0;

  light_eval(gbuf.diffuse,
             gbuf.reflection,
             P,
             Ng,
             V,
             vP_z,
             gbuf.thickness,
             diffuse_light,
             unused_reflection_light,
             unused_shadow);

  vec3 albedo = gbuf.diffuse.color + gbuf.reflection.color + gbuf.refraction.color;

  out_radiance = vec4(diffuse_light * albedo, 0.0);
}
