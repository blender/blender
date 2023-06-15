
/**
 * Deferred lighting evaluation: Lighting is evaluated in a separate pass.
 *
 * Outputs shading parameter per pixel using a randomized set of BSDFs.
 * Some render-pass are written during this pass.
 */

#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_hair_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

vec4 closure_to_rgba(Closure cl)
{
  vec4 out_color;
  out_color.rgb = g_emission;
  out_color.a = saturate(1.0 - avg(g_transmittance));

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

  out_transmittance = vec4(1.0 - g_holdout);
  float transmittance_mono = saturate(avg(g_transmittance));

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
  vec4 cryptomatte_output = vec4(cryptomatte_object_buf[resource_id], node_tree.crypto_hash, 0.0);
  imageStore(rp_cryptomatte_img, out_texel, cryptomatte_output);
  output_renderpass_color(rp_buf.normal_id, vec4(out_normal, 1.0));
  output_renderpass_color(rp_buf.diffuse_color_id, vec4(g_diffuse_data.color, 1.0));
  output_renderpass_color(rp_buf.specular_color_id, vec4(specular_color, 1.0));
  output_renderpass_color(rp_buf.emission_id, vec4(g_emission, 1.0));
#endif

  /* ----- GBuffer output ----- */

  if (true) {
    /* Reflection. */
    vec4 out_reflect = vec4(gbuffer_normal_pack(g_reflection_data.N),
                            g_reflection_data.roughness,
                            g_reflection_data.roughness);
    imageStore(out_gbuff_closure_img, ivec3(out_texel, 0), out_reflect);

    vec4 color = gbuffer_color_pack(g_reflection_data.color);
    imageStore(out_gbuff_color_img, ivec3(out_texel, 0), color);
  }

  /* TODO(fclem) other RNG. */
  float refract_rand = fract(g_closure_rand * 6.1803398875);
  float combined_weight = g_refraction_data.weight + g_diffuse_data.weight;
  bool output_refraction = combined_weight > 0.0 &&
                           (refract_rand * combined_weight) < g_refraction_data.weight;
  if (output_refraction) {
    /* Refraction. */
    vec4 closure;
    closure.xy = gbuffer_normal_pack(g_refraction_data.N);
    closure.z = g_refraction_data.roughness;
    closure.w = gbuffer_ior_pack(g_refraction_data.ior);
    /* Clamp to just bellow 1 to be able to distinguish between refraction and diffuse.
     * Ceiling value is chosen by the storage format (16bit UNORM). */
    closure.w = min(closure.w, float(0xFFFFu - 1u) / float(0xFFFFu));
    imageStore(out_gbuff_closure_img, ivec3(out_texel, 1), closure);

    vec4 color = gbuffer_color_pack(g_refraction_data.color);
    imageStore(out_gbuff_color_img, ivec3(out_texel, 1), color);
  }
  else {
    /* Diffuse. */
    vec4 closure;
    closure.xy = gbuffer_normal_pack(g_diffuse_data.N);
    closure.z = gbuffer_thickness_pack(thickness);
    /* Used to detect the refraction case. Could be used for roughness. */
    closure.w = 1.0;
    imageStore(out_gbuff_closure_img, ivec3(out_texel, 1), closure);

    vec4 color = gbuffer_color_pack(g_diffuse_data.color);
    imageStore(out_gbuff_color_img, ivec3(out_texel, 1), color);
  }

  if (true) {
    /* SubSurface Scattering. */
    vec4 closure;
    closure.xyz = gbuffer_sss_radii_pack(g_diffuse_data.sss_radius);
    closure.w = gbuffer_object_id_unorm16_pack(g_diffuse_data.sss_id > 0 ? uint(resource_id) : 0);
    imageStore(out_gbuff_closure_img, ivec3(out_texel, 2), closure);
  }

  /* ----- Radiance output ----- */

  /* Only output emission during the gbuffer pass. */
  out_radiance = vec4(g_emission, 0.0);
  out_radiance.rgb *= 1.0 - g_holdout;

  out_transmittance.rgb = g_transmittance;
  out_transmittance.a = saturate(avg(g_transmittance));
}
