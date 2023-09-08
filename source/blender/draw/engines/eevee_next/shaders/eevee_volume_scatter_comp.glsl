/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Step 2 : Evaluate all light scattering for each froxels.
 * Also do the temporal reprojection to fight aliasing artifacts. */

#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

/* Included here to avoid requiring lightprobe resources for all volume lib users. */
#pragma BLENDER_REQUIRE(eevee_lightprobe_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_volume_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

#pragma BLENDER_REQUIRE(eevee_volume_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

#ifdef VOLUME_LIGHTING

vec3 volume_scatter_light_eval(vec3 P, vec3 V, uint l_idx, float s_anisotropy)
{
  LightData ld = light_buf[l_idx];

  if (ld.volume_power == 0.0) {
    return vec3(0);
  }

  vec3 L;
  float l_dist;
  light_shape_vector_get(ld, P, L, l_dist);

  float visibility = light_attenuation(ld, L, l_dist);
  LightData light = light_buf[l_idx];
  if (light.tilemap_index != LIGHT_NO_SHADOW && (visibility > 0.0)) {
    vec3 lL = light_world_to_local(light, -L) * l_dist;
    vec3 lNg = vec3(0);
    ShadowSample samp = shadow_sample(
        is_sun_light(light.type), shadow_atlas_tx, shadow_tilemaps_tx, light, lL, lNg, P);
    visibility *= float(samp.occluder_delta + samp.bias >= 0.0);
  }

  if (visibility < 1e-4) {
    return vec3(0);
  }

  vec3 Li = volume_light(ld, L, l_dist) * volume_shadow(ld, P, L, l_dist, extinction_tx);
  return Li * visibility * volume_phase_function(-V, L, s_anisotropy);
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

  vec3 jitter = sampling_rng_3D_get(SAMPLING_VOLUME_U);
  vec3 volume_ndc = volume_to_ndc((vec3(froxel) + jitter) * uniform_buf.volumes.inv_tex_size);
  vec3 vP = get_view_space_from_depth(volume_ndc.xy, volume_ndc.z);
  vec3 P = point_view_to_world(vP);
  vec3 V = cameraVec(P);

  vec2 phase = imageLoad(in_phase_img, froxel).rg;
  /* Divide by phase total weight, to compute the mean anisotropy. */
  float s_anisotropy = phase.x / max(1.0, phase.y);

  scattering += volume_irradiance(P) * s_scattering * volume_phase_function_isotropic();

#ifdef VOLUME_LIGHTING

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    scattering += volume_scatter_light_eval(P, V, l_idx, s_anisotropy) * s_scattering;
  }
  LIGHT_FOREACH_END

  vec2 pixel = (vec2(froxel.xy) + vec2(0.5)) / vec2(uniform_buf.volumes.tex_size.xy) /
               uniform_buf.volumes.viewport_size_inv;

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, pixel, vP.z, l_idx) {
    scattering += volume_scatter_light_eval(P, V, l_idx, s_anisotropy) * s_scattering;
  }
  LIGHT_FOREACH_END

#endif

  /* Catch NaNs. */
  if (any(isnan(scattering)) || any(isnan(extinction))) {
    scattering = vec3(0.0);
    extinction = vec3(0.0);
  }

  imageStore(out_scattering_img, froxel, vec4(scattering, 1.0));
  imageStore(out_extinction_img, froxel, vec4(extinction, 1.0));
}
