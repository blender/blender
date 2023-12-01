/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Deferred lighting evaluation: Lighting is evaluated in a separate pass.
 *
 * Outputs shading parameter per pixel using a randomized set of BSDFs.
 * Some render-pass are written during this pass.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(common_hair_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ambient_occlusion_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

vec4 closure_to_rgba(Closure cl)
{
  vec4 out_color;
  out_color.rgb = g_emission;
  out_color.a = saturate(1.0 - average(g_transmittance));

  /* Reset for the next closure tree. */
  closure_weights_reset();

  return out_color;
}

void main()
{
  /* Clear AOVs first. In case the material renders to them. */
  clear_aovs();

  init_globals();

  float noise = utility_tx_fetch(utility_tx, gl_FragCoord.xy, UTIL_BLUE_NOISE_LAYER).r;
  g_closure_rand = fract(noise + sampling_rng_1D_get(SAMPLING_CLOSURE));

  fragment_displacement();

  nodetree_surface();

  g_holdout = saturate(g_holdout);

  float thickness = nodetree_thickness();

  g_diffuse_data.color *= g_diffuse_data.weight;
  g_reflection_data.color *= g_reflection_data.weight;
  g_refraction_data.color *= g_refraction_data.weight;

  /* TODO(fclem): This feels way too complex for what is it. */
  bool has_any_bsdf_weight = g_diffuse_data.weight != 0.0 || g_reflection_data.weight != 0.0 ||
                             g_refraction_data.weight != 0.0;
  vec3 out_normal = has_any_bsdf_weight ? vec3(0.0) : g_data.N;
  out_normal += g_diffuse_data.N * g_diffuse_data.weight;
  out_normal += g_reflection_data.N * g_reflection_data.weight;
  out_normal += g_refraction_data.N * g_refraction_data.weight;
  out_normal = safe_normalize(out_normal);

  vec3 specular_color = g_reflection_data.color + g_refraction_data.color;

  /* ----- Render Passes output ----- */

  ivec2 out_texel = ivec2(gl_FragCoord.xy);
#ifdef MAT_RENDER_PASS_SUPPORT /* Needed because node_tree isn't present in test shaders. */
  /* Some render pass can be written during the gbuffer pass. Light passes are written later. */
  if (imageSize(rp_cryptomatte_img).x > 1) {
    vec4 cryptomatte_output = vec4(
        cryptomatte_object_buf[resource_id], node_tree.crypto_hash, 0.0);
    imageStore(rp_cryptomatte_img, out_texel, cryptomatte_output);
  }
  output_renderpass_color(uniform_buf.render_pass.normal_id, vec4(out_normal, 1.0));
  output_renderpass_color(uniform_buf.render_pass.position_id, vec4(g_data.P, 1.0));
  output_renderpass_color(uniform_buf.render_pass.diffuse_color_id,
                          vec4(g_diffuse_data.color, 1.0));
  output_renderpass_color(uniform_buf.render_pass.specular_color_id, vec4(specular_color, 1.0));
  output_renderpass_color(uniform_buf.render_pass.emission_id, vec4(g_emission, 1.0));
#endif

  /* ----- GBuffer output ----- */

  GBufferDataPacked gbuf = gbuffer_pack(
      g_diffuse_data, g_reflection_data, g_refraction_data, out_normal, thickness);

  /* Output header and first closure using framebuffer attachment. */
  out_gbuf_header = gbuf.header;
  out_gbuf_color = gbuf.color[0];
  out_gbuf_closure = gbuf.closure[0];

  /* Output remaining closures using image store. */
  /* NOTE: The image view start at layer 1 so all destination layer is `closure_index - 1`. */
  if (gbuffer_header_unpack(gbuf.header, 1) != GBUF_NONE) {
    imageStore(out_gbuf_color_img, ivec3(out_texel, 1 - 1), gbuf.color[1]);
    imageStore(out_gbuf_closure_img, ivec3(out_texel, 1 - 1), gbuf.closure[1]);
  }
  if (gbuffer_header_unpack(gbuf.header, 2) != GBUF_NONE) {
    imageStore(out_gbuf_color_img, ivec3(out_texel, 2 - 1), gbuf.color[2]);
    imageStore(out_gbuf_closure_img, ivec3(out_texel, 2 - 1), gbuf.closure[2]);
  }
  if (gbuffer_header_unpack(gbuf.header, 3) != GBUF_NONE) {
    /* No color for SSS. */
    imageStore(out_gbuf_closure_img, ivec3(out_texel, 3 - 1), gbuf.closure[3]);
  }

  /* ----- Radiance output ----- */

  /* Only output emission during the gbuffer pass. */
  out_radiance = vec4(g_emission, 0.0);
  out_radiance.rgb *= 1.0 - g_holdout;
  out_radiance.a = g_holdout;
}
