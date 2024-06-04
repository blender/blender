/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Step 2 : Evaluate all light scattering for each froxels.
 * Also do the temporal reprojection to fight aliasing artifacts. */

#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)

/* Included here to avoid requiring lightprobe resources for all volume lib users. */
#pragma BLENDER_REQUIRE(eevee_lightprobe_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_volume_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_colorspace_lib.glsl)

#pragma BLENDER_REQUIRE(eevee_volume_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

#ifdef VOLUME_LIGHTING

vec3 volume_light_eval(const bool is_directional, vec3 P, vec3 V, uint l_idx, float s_anisotropy)
{
  LightData light = light_buf[l_idx];

  /* TODO(fclem): Own light list for volume without lights that have 0 volume influence. */
  if (light.power[LIGHT_VOLUME] == 0.0) {
    return vec3(0);
  }

  LightVector lv = light_shape_vector_get(light, is_directional, P);

  float attenuation = light_attenuation_volume(light, is_directional, lv);
  if (attenuation < LIGHT_ATTENUATION_THRESHOLD) {
    return vec3(0);
  }

  float visibility = attenuation;
  if (light.tilemap_index != LIGHT_NO_SHADOW) {
    float delta = shadow_sample(is_directional, shadow_atlas_tx, shadow_tilemaps_tx, light, P);
    if (delta > 0.0) {
      return vec3(0);
    }
  }
  visibility *= volume_phase_function(-V, lv.L, s_anisotropy);
  if (visibility < LIGHT_ATTENUATION_THRESHOLD) {
    return vec3(0);
  }

  vec3 Li = volume_light(light, is_directional, lv) * visibility;

  if (light.tilemap_index != LIGHT_NO_SHADOW) {
    Li *= volume_shadow(light, is_directional, P, lv, extinction_tx);
  }

  return Li;
}

vec3 volume_lightprobe_eval(vec3 P, vec3 V, float s_anisotropy)
{
  SphericalHarmonicL1 phase_sh = volume_phase_function_as_sh_L1(V, s_anisotropy);
  SphericalHarmonicL1 volume_radiance_sh = lightprobe_volume_sample(P);

  float clamp_indirect = uniform_buf.clamp.volume_indirect;
  volume_radiance_sh = spherical_harmonics_clamp(volume_radiance_sh, clamp_indirect);

  return spherical_harmonics_dot(volume_radiance_sh, phase_sh).xyz;
}

#endif

void main()
{
  ivec3 froxel = ivec3(gl_GlobalInvocationID);

  if (any(greaterThanEqual(froxel, uniform_buf.volumes.tex_size))) {
    return;
  }

  /* Emission. */
  vec3 scattering = imageLoad(in_emission_img, froxel).rgb;
  vec3 extinction = imageLoad(in_extinction_img, froxel).rgb;
  vec3 s_scattering = imageLoad(in_scattering_img, froxel).rgb;

  float offset = sampling_rng_1D_get(SAMPLING_VOLUME_W);
  float jitter = volume_froxel_jitter(froxel.xy, offset);
  vec3 uvw = (vec3(froxel) + vec3(0.5, 0.5, 0.5 - jitter)) * uniform_buf.volumes.inv_tex_size;
  vec3 vP = volume_jitter_to_view(uvw);
  vec3 P = drw_point_view_to_world(vP);
  vec3 V = drw_world_incident_vector(P);

  vec2 phase = imageLoad(in_phase_img, froxel).rg;
  /* Divide by phase total weight, to compute the mean anisotropy. */
  float s_anisotropy = phase.x / max(1.0, phase.y);

#ifdef VOLUME_LIGHTING
  vec3 direct_radiance = vec3(0.0);

  if (reduce_max(s_scattering) > 0.0) {
    LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
      direct_radiance += volume_light_eval(true, P, V, l_idx, s_anisotropy);
    }
    LIGHT_FOREACH_END

    vec2 pixel = ((vec2(froxel.xy) + 0.5) * uniform_buf.volumes.inv_tex_size.xy) *
                 uniform_buf.volumes.main_view_extent;

    LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, pixel, vP.z, l_idx)
    {
      direct_radiance += volume_light_eval(false, P, V, l_idx, s_anisotropy);
    }
    LIGHT_FOREACH_END
  }

  vec3 indirect_radiance = volume_lightprobe_eval(P, V, s_anisotropy).xyz;

  direct_radiance *= s_scattering;
  indirect_radiance *= s_scattering;

  float clamp_direct = uniform_buf.clamp.volume_direct;
  float clamp_indirect = uniform_buf.clamp.volume_indirect;
  direct_radiance = colorspace_brightness_clamp_max(direct_radiance, clamp_direct);
  indirect_radiance = colorspace_brightness_clamp_max(indirect_radiance, clamp_indirect);

  scattering += direct_radiance + indirect_radiance;
#endif

  if (uniform_buf.volumes.history_opacity > 0.0) {
    /* Temporal reprojection. */
    vec3 uvw_history = volume_history_uvw_get(froxel);
    if (uvw_history.x != -1.0) {
      vec3 scattering_history = texture(scattering_history_tx, uvw_history).rgb;
      vec3 extinction_history = texture(extinction_history_tx, uvw_history).rgb;
      scattering = mix(scattering, scattering_history, uniform_buf.volumes.history_opacity);
      extinction = mix(extinction, extinction_history, uniform_buf.volumes.history_opacity);
    }
  }

  /* Catch NaNs. */
  if (any(isnan(scattering)) || any(isnan(extinction))) {
    scattering = vec3(0.0);
    extinction = vec3(0.0);
  }

  imageStore(out_scattering_img, froxel, vec4(scattering, 1.0));
  imageStore(out_extinction_img, froxel, vec4(extinction, 1.0));
}
