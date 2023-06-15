
/**
 * The resources expected to be defined are:
 * - light_buf
 * - light_zbin_buf
 * - light_cull_buf
 * - light_tile_buf
 * - shadow_atlas_tx
 * - shadow_tilemaps_tx
 * - sss_transmittance_tx
 * - utility_tx
 */

#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)

/* TODO(fclem): We could reduce register pressure by only having static branches for sun lights. */
void light_eval_ex(ClosureDiffuse diffuse,
                   ClosureReflection reflection,
                   const bool is_directional,
                   vec3 P,
                   vec3 Ng,
                   vec3 V,
                   float vP_z,
                   float thickness,
                   vec4 ltc_mat,
                   uint l_idx,
                   inout vec3 out_diffuse,
                   inout vec3 out_specular,
                   inout float out_shadow)
{
  LightData light = light_buf[l_idx];
  vec3 L;
  float dist;
  light_vector_get(light, P, L, dist);

  float visibility = light_attenuation(light, L, dist);

  if (light.tilemap_index != LIGHT_NO_SHADOW && (visibility > 0.0)) {
    vec3 lL = light_world_to_local(light, -L) * dist;
    vec3 lNg = light_world_to_local(light, Ng);

    ShadowSample samp = shadow_sample(
        is_directional, shadow_atlas_tx, shadow_tilemaps_tx, light, lL, lNg, P);

#ifdef SSS_TRANSMITTANCE
    /* Transmittance evaluation first to use initial visibility without shadow. */
    if (diffuse.sss_id != 0u && light.diffuse_power > 0.0) {
      float delta = max(thickness, -(samp.occluder_delta + samp.bias));

      vec3 intensity = visibility * light.transmit_power *
                       light_translucent(sss_transmittance_tx,
                                         is_directional,
                                         light,
                                         diffuse.N,
                                         L,
                                         dist,
                                         diffuse.sss_radius,
                                         delta);
      out_diffuse += light.color * intensity;
    }
#endif
    visibility *= float(samp.occluder_delta + samp.bias >= 0.0);
    out_shadow *= float(samp.occluder_delta + samp.bias >= 0.0);
  }

  if (visibility < 1e-6) {
    return;
  }

  if (light.diffuse_power > 0.0) {
    float intensity = visibility * light.diffuse_power *
                      light_diffuse(utility_tx, is_directional, light, diffuse.N, V, L, dist);
    out_diffuse += light.color * intensity;
  }

  if (light.specular_power > 0.0) {
    float intensity = visibility * light.specular_power *
                      light_ltc(
                          utility_tx, is_directional, light, reflection.N, V, L, dist, ltc_mat);
    out_specular += light.color * intensity;
  }
}

void light_eval(ClosureDiffuse diffuse,
                ClosureReflection reflection,
                vec3 P,
                vec3 Ng,
                vec3 V,
                float vP_z,
                float thickness,
                inout vec3 out_diffuse,
                inout vec3 out_specular,
                inout float out_shadow)
{
  vec2 uv = vec2(reflection.roughness, safe_sqrt(1.0 - dot(reflection.N, V)));
  uv = uv * UTIL_TEX_UV_SCALE + UTIL_TEX_UV_BIAS;
  vec4 ltc_mat = utility_tx_sample(utility_tx, uv, UTIL_LTC_MAT_LAYER);

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    light_eval_ex(diffuse,
                  reflection,
                  true,
                  P,
                  Ng,
                  V,
                  vP_z,
                  thickness,
                  ltc_mat,
                  l_idx,
                  out_diffuse,
                  out_specular,
                  out_shadow);
  }
  LIGHT_FOREACH_END

  vec2 px = gl_FragCoord.xy;
  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, px, vP_z, l_idx) {
    light_eval_ex(diffuse,
                  reflection,
                  false,
                  P,
                  Ng,
                  V,
                  vP_z,
                  thickness,
                  ltc_mat,
                  l_idx,
                  out_diffuse,
                  out_specular,
                  out_shadow);
  }
  LIGHT_FOREACH_END
}
