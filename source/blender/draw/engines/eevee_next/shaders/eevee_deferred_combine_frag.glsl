/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Combine light passes to the combined color target and apply surface colors.
 * This also fills the different render passes.
 */

#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_renderpass_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_colorspace_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel);

  vec3 glossy_reflect_light = vec3(0.0);
  vec3 glossy_refract_light = vec3(0.0);
  vec3 diffuse_reflect_light = vec3(0.0);
  vec3 diffuse_refract_light = vec3(0.0);

  if (gbuf.has_diffuse) {
    diffuse_reflect_light = imageLoad(direct_radiance_1_img, texel).rgb +
                            imageLoad(indirect_diffuse_img, texel).rgb;
  }

  if (gbuf.has_reflection) {
    glossy_reflect_light = imageLoad(direct_radiance_2_img, texel).rgb +
                           imageLoad(indirect_reflect_img, texel).rgb;
  }

  if (gbuf.has_translucent) {
    /* Indirect radiance not implemented yet. */
    diffuse_refract_light = imageLoad(direct_radiance_3_img, texel).rgb;
  }

  if (gbuf.has_refraction) {
    /* Direct radiance not implemented yet. */
    glossy_refract_light = imageLoad(indirect_refract_img, texel).rgb;
  }

  /* Light passes. */
  vec3 diffuse_light = diffuse_reflect_light + diffuse_refract_light;
  vec3 specular_light = glossy_reflect_light + glossy_refract_light;
  output_renderpass_color(uniform_buf.render_pass.diffuse_light_id, vec4(diffuse_light, 1.0));
  output_renderpass_color(uniform_buf.render_pass.specular_light_id, vec4(specular_light, 1.0));
  /* Combine. */
  out_combined = vec4(0.0);
  out_combined.xyz += diffuse_reflect_light * gbuf.diffuse.color;
  out_combined.xyz += diffuse_refract_light * gbuf.translucent.color;
  out_combined.xyz += glossy_reflect_light * gbuf.reflection.color;
  out_combined.xyz += glossy_refract_light * gbuf.refraction.color;

  if (any(isnan(out_combined))) {
    out_combined = vec4(1.0, 0.0, 1.0, 0.0);
  }

  out_combined = colorspace_safe_color(out_combined);
}
