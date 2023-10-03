/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * The resources expected to be defined are:
 * - light_buf
 * - light_zbin_buf
 * - light_cull_buf
 * - light_tile_buf
 * - shadow_atlas_tx
 * - shadow_tilemaps_tx
 * - utility_tx
 */

#pragma BLENDER_REQUIRE(eevee_shadow_tracing_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)

void light_eval_ex(ClosureDiffuse diffuse,
                   ClosureReflection reflection,
                   const bool is_directional,
                   vec3 P,
                   vec3 Ng,
                   vec3 V,
                   float vP_z, /* TODO(fclem): Remove, is unused. */
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

  float visibility = is_directional ? 1.0 : light_attenuation(light, L, dist);

  if (light.tilemap_index != LIGHT_NO_SHADOW && (visibility > 0.0)) {
#ifdef SURFEL_LIGHT
    ShadowEvalResult shadow = shadow_eval(light, is_directional, P, Ng, 16, 8);
#else
    ShadowEvalResult shadow = shadow_eval(
        light, is_directional, P, Ng, uniform_buf.shadow.ray_count, uniform_buf.shadow.step_count);
#endif

#ifdef SSS_TRANSMITTANCE
    /* Transmittance evaluation first to use initial visibility without shadow. */
    if (diffuse.sss_id != 0u && light.diffuse_power > 0.0) {
      float delta = max(thickness, shadow.subsurface_occluder_distance);

      vec3 intensity = visibility * light.transmit_power *
                       light_translucent(
                           is_directional, light, diffuse.N, L, dist, diffuse.sss_radius, delta);
      out_diffuse += light.color * intensity;
    }
#endif
    visibility *= shadow.surface_light_visibilty;
    out_shadow *= shadow.surface_light_visibilty;
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
  vec4 ltc_mat = utility_tx_sample_lut(
      utility_tx, dot(reflection.N, V), reflection.roughness, UTIL_LTC_MAT_LAYER);

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

#ifdef GPU_FRAGMENT_SHADER
  vec2 px = gl_FragCoord.xy;
#else
  vec2 px = vec2(0.0);
#endif
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
