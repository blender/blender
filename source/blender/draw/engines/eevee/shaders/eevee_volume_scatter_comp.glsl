/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Step 2 : Evaluate all light scattering for each froxels.
 * Also do the temporal reprojection to fight aliasing artifacts. */

#include "infos/eevee_volume_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_volume_scatter_with_lights)

#include "eevee_colorspace_lib.glsl"
#include "eevee_light_iter_lib.glsl"
#include "eevee_light_lib.glsl"
#include "eevee_lightprobe_volume_eval_lib.glsl"
#include "eevee_shadow_lib.glsl"
#include "eevee_volume_lib.glsl"

#if defined(VOLUME_LIGHTING) || defined(GLSL_CPP_STUBS)

float3 volume_light(LightData light, const bool is_directional, LightVector lv)
{
  float power = 1.0f;
  if (!is_directional) {
    float light_radius = light_local_data_get(light).shape_radius;
    /**
     * Using "Point Light Attenuation Without Singularity" from Cem Yuksel
     * http://www.cemyuksel.com/research/pointlightattenuation/pointlightattenuation.pdf
     * http://www.cemyuksel.com/research/pointlightattenuation/
     */
    float d = lv.dist;
    float d_sqr = square(d);
    float r_sqr = square(light_radius);

    /* Using reformulation that has better numerical precision. */
    power = 2.0f / (d_sqr + r_sqr + d * sqrt(d_sqr + r_sqr));

    if (light.type == LIGHT_RECT || light.type == LIGHT_ELLIPSE) {
      /* Modulate by light plane orientation / solid angle. */
      power *= saturate(dot(light_z_axis(light), lv.L));
    }
  }
  return light.color * light.power[LIGHT_VOLUME] * power;
}

#  define VOLUMETRIC_SHADOW_MAX_STEP 128.0f

float3 volume_shadow(
    LightData ld, const bool is_directional, float3 P, LightVector lv, sampler3D extinction_tx)
{
#  if defined(VOLUME_SHADOW) || defined(GLSL_CPP_STUBS)
  if (uniform_buf.volumes.shadow_steps == 0) {
    return float3(1.0f);
  }

  /* Heterogeneous volume shadows. */
  float dd = lv.dist / uniform_buf.volumes.shadow_steps;
  float3 L = lv.L * lv.dist / uniform_buf.volumes.shadow_steps;

  if (is_directional) {
    /* For sun light we scan the whole frustum. So we need to get the correct endpoints. */
    float3 ndcP = drw_point_world_to_ndc(P);
    float3 ndcL = drw_point_world_to_ndc(P + lv.L * lv.dist) - ndcP;

    float3 ndc_frustum_isect = ndcP + ndcL * line_unit_box_intersect_dist_safe(ndcP, ndcL);

    L = drw_point_ndc_to_world(ndc_frustum_isect) - P;
    L /= uniform_buf.volumes.shadow_steps;
    dd = length(L);
  }

  /* TODO use shadow maps instead. */
  float3 shadow = float3(1.0f);
  for (float t = 1.0f; t < VOLUMETRIC_SHADOW_MAX_STEP && t <= uniform_buf.volumes.shadow_steps;
       t += 1.0f)
  {
    float3 w_pos = P + L * t;

    float3 v_pos = drw_point_world_to_view(w_pos);
    float3 volume_co = volume_view_to_jitter(v_pos);
    /* Let the texture be clamped to edge. This reduce visual glitches. */
    float3 s_extinction = texture(extinction_tx, volume_co).rgb;

    shadow *= exp(-s_extinction * dd);
  }
  return shadow;
#  else
  return float3(1.0f);
#  endif /* VOLUME_SHADOW */
}

float3 volume_light_eval(
    const bool is_directional, float3 P, float3 V, uint l_idx, float s_anisotropy)
{
  LightData light = light_buf[l_idx];

  /* TODO(fclem): Own light list for volume without lights that have 0 volume influence. */
  if (light.power[LIGHT_VOLUME] == 0.0f) {
    return float3(0);
  }

  LightVector lv = light_shape_vector_get(light, is_directional, P);

  float attenuation = light_attenuation_volume(light, is_directional, lv);
  if (attenuation < LIGHT_ATTENUATION_THRESHOLD) {
    return float3(0);
  }

  float visibility = attenuation;
  if (light.tilemap_index != LIGHT_NO_SHADOW) {
    float delta = shadow_sample(is_directional, shadow_atlas_tx, shadow_tilemaps_tx, light, P);
    if (delta > 0.0f) {
      return float3(0);
    }
  }
  visibility *= volume_phase_function(-V, lv.L, s_anisotropy);
  if (visibility < LIGHT_ATTENUATION_THRESHOLD) {
    return float3(0);
  }

  float3 Li = volume_light(light, is_directional, lv) * visibility;

  if (light.tilemap_index != LIGHT_NO_SHADOW) {
    Li *= volume_shadow(light, is_directional, P, lv, extinction_tx);
  }

  return Li;
}

float3 volume_lightprobe_eval(float3 P, float3 V, float s_anisotropy)
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
  int3 froxel = int3(gl_GlobalInvocationID);

  if (any(greaterThanEqual(froxel, uniform_buf.volumes.tex_size))) {
    return;
  }

  /* Emission. */
  float3 scattering = imageLoadFast(in_emission_img, froxel).rgb;
  float3 extinction = imageLoadFast(in_extinction_img, froxel).rgb;

#if defined(VOLUME_LIGHTING) || defined(GLSL_CPP_STUBS)
  float3 s_scattering = imageLoadFast(in_scattering_img, froxel).rgb;

  float offset = sampling_rng_1D_get(SAMPLING_VOLUME_W);
  float jitter = volume_froxel_jitter(froxel.xy, offset);
  float3 uvw = (float3(froxel) + float3(0.5f, 0.5f, 0.5f - jitter)) *
               uniform_buf.volumes.inv_tex_size;
  float3 vP = volume_jitter_to_view(uvw);

  float3 P = drw_point_view_to_world(vP);
  float3 V = drw_world_incident_vector(P);

  float phase = imageLoadFast(in_phase_img, froxel).r;
  float phase_weight = imageLoadFast(in_phase_weight_img, froxel).r;
  /* Divide by phase total weight, to compute the mean anisotropy. */
  float s_anisotropy = phase / max(1.0f, phase_weight);

  float3 direct_radiance = float3(0.0f);

  if (reduce_max(s_scattering) > 0.0f) {
    LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
      direct_radiance += volume_light_eval(true, P, V, l_idx, s_anisotropy);
    }
    LIGHT_FOREACH_END

    float2 pixel = ((float2(froxel.xy) + 0.5f) * uniform_buf.volumes.inv_tex_size.xy) *
                   uniform_buf.volumes.main_view_extent;

    LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, pixel, vP.z, l_idx)
    {
      direct_radiance += volume_light_eval(false, P, V, l_idx, s_anisotropy);
    }
    LIGHT_FOREACH_END
  }

  float3 indirect_radiance = volume_lightprobe_eval(P, V, s_anisotropy).xyz;

  direct_radiance *= s_scattering;
  indirect_radiance *= s_scattering;

  float clamp_direct = uniform_buf.clamp.volume_direct;
  float clamp_indirect = uniform_buf.clamp.volume_indirect;
  direct_radiance = colorspace_brightness_clamp_max(direct_radiance, clamp_direct);
  indirect_radiance = colorspace_brightness_clamp_max(indirect_radiance, clamp_indirect);

  scattering += direct_radiance + indirect_radiance;
#endif

  if (uniform_buf.volumes.history_opacity > 0.0f) {
    /* Temporal reprojection. */
    float3 uvw_history = volume_history_uvw_get(froxel);
    if (uvw_history.x != -1.0f) {
      float3 scattering_history = texture(scattering_history_tx, uvw_history).rgb;
      float3 extinction_history = texture(extinction_history_tx, uvw_history).rgb;
      scattering = mix(scattering, scattering_history, uniform_buf.volumes.history_opacity);
      extinction = mix(extinction, extinction_history, uniform_buf.volumes.history_opacity);
    }
  }

  /* Catch NaNs. */
  if (any(isnan(scattering)) || any(isnan(extinction))) {
    scattering = float3(0.0f);
    extinction = float3(0.0f);
  }

  imageStoreFast(out_scattering_img, froxel, float4(scattering, 1.0f));
  imageStoreFast(out_extinction_img, froxel, float4(extinction, 1.0f));
}
