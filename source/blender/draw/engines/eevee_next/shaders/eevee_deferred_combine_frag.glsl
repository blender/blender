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

vec3 load_radiance_direct(ivec2 texel, int i)
{
  switch (i) {
    case 0:
      return texelFetch(direct_radiance_1_tx, texel, 0).rgb;
    case 1:
      return texelFetch(direct_radiance_2_tx, texel, 0).rgb;
    case 2:
      return texelFetch(direct_radiance_3_tx, texel, 0).rgb;
    default:
      return vec3(0);
  }
  return vec3(0);
}

vec3 load_radiance_indirect(ivec2 texel, int i)
{
  switch (i) {
    case 0:
      return texelFetch(indirect_radiance_1_tx, texel, 0).rgb;
    case 1:
      return texelFetch(indirect_radiance_2_tx, texel, 0).rgb;
    case 2:
      return texelFetch(indirect_radiance_3_tx, texel, 0).rgb;
    default:
      return vec3(0);
  }
  return vec3(0);
}

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  GBufferReader gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel);

  /* TODO: use stencil buffer to avoid fragment invocations here. */
  out_combined = vec4(0.0, 0.0, 0.0, 0.0);
  if (gbuf.closure_count == 0) {
    return;
  }

  vec3 out_diffuse = vec3(0.0);
  vec3 out_specular = vec3(0.0);

  for (int i = 0; i < GBUFFER_LAYER_MAX && i < gbuf.closure_count; i++) {
    vec3 closure_light = load_radiance_direct(texel, i);

    /* TODO(fclem): Enable for OpenGL and Vulkan once they fully support specialization constants.
     */
#ifndef GPU_METAL
    bool use_combined_lightprobe_eval = uniform_buf.pipeline.use_combined_lightprobe_eval;
#endif
    if (!use_combined_lightprobe_eval) {
      closure_light += load_radiance_indirect(texel, i);
    }

    ClosureUndetermined cl = gbuffer_closure_get(gbuf, i);
    closure_light *= cl.color;
    out_combined.rgb += closure_light;

    switch (cl.type) {
      case CLOSURE_BSDF_TRANSLUCENT_ID:
      case CLOSURE_BSSRDF_BURLEY_ID:
      case CLOSURE_BSDF_DIFFUSE_ID:
        out_diffuse += closure_light;
        break;
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
        out_specular += closure_light;
        break;
      case CLOSURE_NONE_ID:
        /* TODO(fclem): Assert. */
        break;
    }
  }

  /* Light passes. */
  if (render_pass_diffuse_light_enabled) {
    output_renderpass_color(uniform_buf.render_pass.diffuse_light_id, vec4(out_diffuse, 1.0));
  }
  if (render_pass_specular_light_enabled) {
    output_renderpass_color(uniform_buf.render_pass.specular_light_id, vec4(out_specular, 1.0));
  }

  if (any(isnan(out_combined))) {
    out_combined = vec4(1.0, 0.0, 1.0, 0.0);
  }

  out_combined = colorspace_safe_color(out_combined);
}
