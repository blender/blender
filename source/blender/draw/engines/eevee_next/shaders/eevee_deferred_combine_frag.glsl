/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Combine light passes to the combined color target and apply surface colors.
 * This also fills the different render passes.
 */

#pragma BLENDER_REQUIRE(eevee_deferred_combine_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  DeferredCombine dc = deferred_combine(texel);

  if (use_radiance_feedback) {
    /* Output unmodified radiance for indirect lighting. */
    vec3 out_radiance = imageLoad(radiance_feedback_img, texel).rgb;
    out_radiance += dc.out_direct + dc.out_indirect;
    imageStore(radiance_feedback_img, texel, vec4(out_radiance, 0.0));
  }

  deferred_combine_clamp(dc);

  /* Light passes. */
  if (render_pass_diffuse_light_enabled) {
    vec3 diffuse_light = dc.diffuse_direct + dc.diffuse_indirect;
    output_renderpass_color(uniform_buf.render_pass.diffuse_color_id, vec4(dc.diffuse_color, 1.0));
    output_renderpass_color(uniform_buf.render_pass.diffuse_light_id, vec4(diffuse_light, 1.0));
  }
  if (render_pass_specular_light_enabled) {
    vec3 specular_light = dc.specular_direct + dc.specular_indirect;
    output_renderpass_color(uniform_buf.render_pass.specular_color_id,
                            vec4(dc.specular_color, 1.0));
    output_renderpass_color(uniform_buf.render_pass.specular_light_id, vec4(specular_light, 1.0));
  }
  if (render_pass_normal_enabled) {
    output_renderpass_color(uniform_buf.render_pass.normal_id, vec4(dc.average_normal, 1.0));
  }

  out_combined = deferred_combine_final_output(dc);
}
