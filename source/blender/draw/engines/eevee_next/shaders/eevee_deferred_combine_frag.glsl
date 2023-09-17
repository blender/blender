/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Combine light passes to the combined color target and apply surface colors.
 * This also fills the different render passes.
 */

#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_renderpass_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  vec3 diffuse_light = imageLoad(direct_diffuse_img, texel).rgb +
                       imageLoad(indirect_diffuse_img, texel).rgb;
  vec3 reflect_light = imageLoad(direct_reflect_img, texel).rgb +
                       imageLoad(indirect_reflect_img, texel).rgb;
  vec3 refract_light = imageLoad(direct_refract_img, texel).rgb +
                       imageLoad(indirect_refract_img, texel).rgb;

  /* Apply color and output lighting to render-passes. */
  vec4 gbuffer_1_packed = texelFetch(gbuffer_closure_tx, ivec3(texel, 1), 0);
  bool is_refraction = gbuffer_is_refraction(gbuffer_1_packed);

  vec4 color_0_packed = texelFetch(gbuffer_color_tx, ivec3(texel, 0), 0);
  vec4 color_1_packed = texelFetch(gbuffer_color_tx, ivec3(texel, 1), 0);

  vec3 reflection_color = gbuffer_color_unpack(color_0_packed);
  vec3 refraction_color = is_refraction ? gbuffer_color_unpack(color_1_packed) : vec3(0.0);
  vec3 diffuse_color = is_refraction ? vec3(0.0) : gbuffer_color_unpack(color_1_packed);

  /* Light passes. */
  vec3 specular_light = reflect_light + refract_light;
  render_pass_color_out(uniform_buf.render_pass.diffuse_light_id, diffuse_light);
  render_pass_color_out(uniform_buf.render_pass.specular_light_id, specular_light);

  out_combined = vec4(0.0);
  out_combined.xyz += diffuse_light * diffuse_color;
  out_combined.xyz += reflect_light * reflection_color;
  out_combined.xyz += refract_light * refraction_color;
}
