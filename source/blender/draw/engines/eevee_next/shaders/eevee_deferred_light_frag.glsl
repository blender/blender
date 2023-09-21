/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compute light objects lighting contribution using Gbuffer data.
 *
 * Output light .
 */

#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_renderpass_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_eval_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;
  vec3 P = get_world_space_from_depth(uvcoordsvar.xy, depth);

  /* TODO(fclem): High precision derivative. */
  vec3 Ng = safe_normalize(cross(dFdx(P), dFdy(P)));
  vec3 V = cameraVec(P);
  float vP_z = dot(cameraForward, P) - dot(cameraForward, cameraPos);

  GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel);

  vec3 diffuse_light = vec3(0.0);
  vec3 reflection_light = vec3(0.0);
  vec3 refraction_light = vec3(0.0);
  float shadow = 1.0;

  light_eval(gbuf.diffuse,
             gbuf.reflection,
             P,
             Ng,
             V,
             vP_z,
             gbuf.thickness,
             diffuse_light,
             reflection_light,
             /* TODO(fclem): Implement refraction light. */
             //  refraction_light,
             shadow);

  output_renderpass_value(uniform_buf.render_pass.shadow_id, shadow);

  /* Store lighting for next deferred pass. */
  /* Output object ID for sub-surface screen space processing. */
  float f_sss_id = gbuffer_object_id_f16_pack(gbuf.diffuse.sss_id);

  imageStore(direct_diffuse_img, texel, vec4(diffuse_light, f_sss_id));
  imageStore(direct_reflect_img, texel, vec4(reflection_light, 1.0));
  imageStore(direct_refract_img, texel, vec4(refraction_light, 1.0));
}
