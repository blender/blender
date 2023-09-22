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

  GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel);
  /* Mask invalid radiance. */
  diffuse_light = gbuf.has_diffuse ? diffuse_light : vec3(0.0);
  reflect_light = gbuf.has_reflection ? reflect_light : vec3(0.0);
  refract_light = gbuf.has_refraction ? refract_light : vec3(0.0);
  /* Light passes. */
  vec3 specular_light = reflect_light + refract_light;
  output_renderpass_color(uniform_buf.render_pass.diffuse_light_id, vec4(diffuse_light, 1.0));
  output_renderpass_color(uniform_buf.render_pass.specular_light_id, vec4(specular_light, 1.0));
  /* Combine. */
  out_combined = vec4(0.0);
  out_combined.xyz += diffuse_light * gbuf.diffuse.color;
  out_combined.xyz += reflect_light * gbuf.reflection.color;
  out_combined.xyz += refract_light * gbuf.refraction.color;
}
