/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

#pragma once

#include "infos/eevee_light_infos.hh"
#include "infos/eevee_lightprobe_infos.hh"
#include "infos/eevee_shadow_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_light_data)
SHADER_LIBRARY_CREATE_INFO(eevee_shadow_data)
SHADER_LIBRARY_CREATE_INFO(eevee_lightprobe_data)

#include "draw_view_lib.glsl"
#include "eevee_colorspace_lib.bsl.hh"
#include "eevee_defines.hh"
#include "eevee_light_iter_lib.glsl"
#include "eevee_light_lib.glsl"
#include "eevee_light_shared.hh"
#include "eevee_lightprobe_volume_eval_lib.glsl"
#include "eevee_shadow_lib.glsl"
#include "eevee_volume_lib.bsl.hh"
#include "eevee_volume_shared.hh"
#include "gpu_shader_fullscreen_lib.glsl"

namespace eevee::volume {

float3 volume_light(LightData light, const bool is_directional, LightVector lv)
{
  float power = 1.0f;
  if (!is_directional) {
    float light_radius = light.local().local.shape_radius;
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

#define VOLUMETRIC_SHADOW_MAX_STEP 128.0f

float3 volume_shadow(
    LightData /*ld*/, const bool is_directional, float3 P, LightVector lv, sampler3D extinction_tx)
{
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
}

float3 volume_light_eval(const bool is_directional,
                         float3 P,
                         float3 V,
                         uint l_idx,
                         float s_anisotropy,
                         sampler3D extinction_tx)
{
  LightData light = buffer_get(eevee_light_data, light_buf)[l_idx];
  auto &shadow_atlas_tx = sampler_get(eevee_shadow_data, shadow_atlas_tx);
  auto &shadow_tilemaps_tx = sampler_get(eevee_shadow_data, shadow_tilemaps_tx);

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
  SphericalHarmonicL1<float4> phase_sh = volume_phase_function_as_sh_L1(V, s_anisotropy);
  SphericalHarmonicL1<float4> volume_radiance_sh = lightprobe_volume_sample(P);

  float clamp_indirect = uniform_buf.clamp.volume_indirect;
  volume_radiance_sh = spherical_harmonics::clamp_energy(volume_radiance_sh, clamp_indirect);

  return spherical_harmonics::dot(volume_radiance_sh, phase_sh).xyz;
}

struct Scatter {
  [[compilation_constant]] const bool use_volume_light;

  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
  [[legacy_info]] ShaderCreateInfo eevee_light_data;
  [[legacy_info]] ShaderCreateInfo eevee_shadow_data;
  [[legacy_info]] ShaderCreateInfo eevee_lightprobe_data;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;

  [[sampler(0)]] sampler3D scattering_history_tx;
  [[sampler(1)]] sampler3D extinction_history_tx;
  /* Alias of UnifiedVolumeProperties::in_extinction_img but as a sampler. */
  [[sampler(9), condition(use_volume_light)]] sampler3D extinction_tx;

  [[image(5, write, UFLOAT_11_11_10)]] image3D out_scattering_img;
  [[image(6, write, UFLOAT_11_11_10)]] image3D out_extinction_img;
};

/* Step 2 : Evaluate all light scattering for each froxels.
 * Also do the temporal reprojection to fight aliasing artifacts. */
[[compute, local_size(VOLUME_GROUP_SIZE, VOLUME_GROUP_SIZE, VOLUME_GROUP_SIZE)]]
void scatter_main([[resource_table]] Scatter &srt,
                  [[resource_table]] UnifiedVolumeProperties &props,
                  [[global_invocation_id]] const uint3 global_id)
{
  int3 froxel = int3(global_id);

  if (any(greaterThanEqual(froxel, uniform_buf.volumes.tex_size))) {
    return;
  }

  /* Emission. */
  float3 scattering = imageLoadFast(props.in_emission_img, froxel).rgb;
  float3 extinction = imageLoadFast(props.in_extinction_img, froxel).rgb;

  float3 s_scattering = imageLoadFast(props.in_scattering_img, froxel).rgb;

  float offset = sampling_rng_1D_get(SAMPLING_VOLUME_W);
  float jitter = volume_froxel_jitter(froxel.xy, offset);
  float3 uvw = (float3(froxel) + float3(0.5f, 0.5f, 0.5f - jitter)) *
               uniform_buf.volumes.inv_tex_size;
  float3 vP = volume_jitter_to_view(uvw);

  float3 P = drw_point_view_to_world(vP);
  float3 V = drw_world_incident_vector(P);

  float phase = imageLoadFast(props.in_phase_img, froxel).r;
  float phase_weight = imageLoadFast(props.in_phase_weight_img, froxel).r;
  /* Divide by phase total weight, to compute the mean anisotropy. */
  float s_anisotropy = phase / max(1.0f, phase_weight);

  float3 direct_radiance = float3(0.0f);

  if (srt.use_volume_light) [[static_branch]] {
    if (reduce_max(s_scattering) > 0.0f) {
      LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
        direct_radiance += volume_light_eval(true, P, V, l_idx, s_anisotropy, srt.extinction_tx);
      }
      LIGHT_FOREACH_END

      float2 pixel = ((float2(froxel.xy) + 0.5f) * uniform_buf.volumes.inv_tex_size.xy) *
                     uniform_buf.volumes.main_view_extent;

      LIGHT_FOREACH_BEGIN_LOCAL (
          light_cull_buf, light_zbin_buf, light_tile_buf, pixel, vP.z, l_idx)
      {
        direct_radiance += volume_light_eval(false, P, V, l_idx, s_anisotropy, srt.extinction_tx);
      }
      LIGHT_FOREACH_END
    }
  }

  float3 indirect_radiance = volume_lightprobe_eval(P, V, s_anisotropy).xyz;

  direct_radiance *= s_scattering;
  indirect_radiance *= s_scattering;

  float clamp_direct = uniform_buf.clamp.volume_direct;
  float clamp_indirect = uniform_buf.clamp.volume_indirect;
  direct_radiance = colorspace::brightness_clamp_max(direct_radiance, clamp_direct);
  indirect_radiance = colorspace::brightness_clamp_max(indirect_radiance, clamp_indirect);

  direct_radiance *= uniform_buf.clamp.direct_scale;
  indirect_radiance *= uniform_buf.clamp.indirect_scale;

  scattering += direct_radiance + indirect_radiance;

  if (uniform_buf.volumes.history_opacity > 0.0f) {
    /* Temporal reprojection. */
    float3 uvw_history = volume_history_uvw_get(froxel);
    if (uvw_history.x != -1.0f) {
      float3 scattering_history = texture(srt.scattering_history_tx, uvw_history).rgb;
      float3 extinction_history = texture(srt.extinction_history_tx, uvw_history).rgb;
      scattering = mix(scattering, scattering_history, uniform_buf.volumes.history_opacity);
      extinction = mix(extinction, extinction_history, uniform_buf.volumes.history_opacity);
    }
  }

  /* Catch NaNs. */
  if (any(isnan(scattering)) || any(isnan(extinction))) {
    scattering = float3(0.0f);
    extinction = float3(0.0f);
  }

  imageStoreFast(srt.out_scattering_img, froxel, float4(scattering, 1.0f));
  imageStoreFast(srt.out_extinction_img, froxel, float4(extinction, 1.0f));
}
struct Integrate {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;

  [[sampler(0)]] sampler3D in_scattering_tx;
  [[sampler(1)]] sampler3D in_extinction_tx;

  [[image(0, write, UFLOAT_11_11_10)]] image3D out_scattering_img;
  [[image(1, write, UFLOAT_11_11_10)]] image3D out_transmittance_img;
};

/* Step 3 : Integrate for each froxel the final amount of light
 * scattered back to the viewer and the amount of transmittance. */
[[compute, local_size(VOLUME_INTEGRATION_GROUP_SIZE, VOLUME_INTEGRATION_GROUP_SIZE, 1)]]
void integration_main([[resource_table]] Integrate &srt,
                      [[global_invocation_id]] const uint3 global_id)
{
  int2 texel = int2(global_id.xy);

  if (any(greaterThanEqual(texel, uniform_buf.volumes.tex_size.xy))) {
    return;
  }

  /* Start with full transmittance and no scattered light. */
  float3 scattering = float3(0.0f);
  float3 transmittance = float3(1.0f);

  /* Compute view ray. Note that jittering the position of the first voxel doesn't bring any
   * benefit here. */
  float3 uvw = (float3(float2(texel), 0.0f) + float3(0.5f, 0.5f, 0.0f)) *
               uniform_buf.volumes.inv_tex_size;
  float3 view_cell = volume_jitter_to_view(uvw);

  float prev_ray_len;
  float orig_ray_len;
  if (drw_view_is_perspective()) {
    prev_ray_len = length(view_cell);
    orig_ray_len = prev_ray_len / view_cell.z;
  }
  else {
    prev_ray_len = view_cell.z;
    orig_ray_len = 1.0f;
  }

  for (int i = 0; i <= uniform_buf.volumes.tex_size.z; i++) {
    int3 froxel = int3(texel, i);

    float3 froxel_scattering = texelFetch(srt.in_scattering_tx, froxel, 0).rgb;
    float3 extinction = texelFetch(srt.in_extinction_tx, froxel, 0).rgb;

    float cell_depth = volume_z_to_view_z((float(i) + 1.0f) * uniform_buf.volumes.inv_tex_size.z);
    float ray_len = orig_ray_len * cell_depth;

    /* Evaluate Scattering. */
    float step_len = abs(ray_len - prev_ray_len);
    prev_ray_len = ray_len;
    float3 froxel_transmittance = exp(-extinction * step_len);
    /** NOTE: Original calculation carries precision issues when compiling for AMD GPUs
     * and running Metal. This version of the equation retains precision well for all
     * macOS HW configurations.
     * Here is the original for reference:
     * `Lscat = (Lscat - Lscat * Tr) / safe_rcp(s_extinction)` */
    float3 froxel_opacity = 1.0f - froxel_transmittance;
    float3 froxel_step_opacity = froxel_opacity * safe_rcp(extinction);

    /* Emission does not work if there is no extinction because
     * `froxel_transmittance` evaluates to 1.0 leading to `froxel_opacity = 0.0 and 0.4 depth.`.
     * (See #65771) To avoid fiddling with numerical values, take the limit of
     * `froxel_step_opacity` as `extinction` approaches zero which is simply `step_len`. */
    bool3 is_invalid_extinction = equal(extinction, float3(0.0f));
    froxel_step_opacity = mix(froxel_step_opacity, float3(step_len), is_invalid_extinction);

    /* Integrate along the current step segment. */
    froxel_scattering = froxel_scattering * froxel_step_opacity;

    /* Accumulate and also take into account the transmittance from previous steps. */
    scattering += transmittance * froxel_scattering;
    transmittance *= froxel_transmittance;

    imageStoreFast(srt.out_scattering_img, froxel, float4(scattering, 1.0f));
    imageStoreFast(srt.out_transmittance_img, froxel, float4(transmittance, 1.0f));
  }
}

struct FragOut {
  [[frag_color(0), index(0)]] float4 radiance;
  [[frag_color(0), index(1)]] float4 transmittance;
};

struct Resolve {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_render_pass_out;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
};

[[vertex]]
void resolve_vert([[vertex_id]] const int vert_id, [[position]] float4 &out_position)
{
  fullscreen_vertex(vert_id, out_position);
}

/* Step 4 : Apply final integration on top of the scene color.
 * This is only for opaque geometry. */
[[fragment]]
void resolve_frag([[resource_table]] const Resolve & /*srt*/,
                  [[resource_table]] const UnifiedVolumeData &volumes,
                  [[frag_coord]] const float4 frag_co,
                  [[out]] FragOut &out_frag)
{
  auto &hiz_tx = sampler_get(eevee_hiz_data, hiz_tx);

  float2 uvs = frag_co.xy * uniform_buf.volumes.main_view_extent_inv;
  float scene_depth = texelFetch(hiz_tx, int2(frag_co.xy), 0).r;

  VolumeResolveSample vol = volume_resolve(
      float3(uvs, scene_depth), volumes.transmittance_tx, volumes.scattering_tx);

  out_frag.radiance = float4(vol.scattering, 0.0f);
  out_frag.transmittance = float4(vol.transmittance, saturate(average(vol.transmittance)));

  if (uniform_buf.render_pass.volume_light_id >= 0) {
    auto &rp_color_img = image_get(eevee_render_pass_out, rp_color_img);
    imageStoreFast(rp_color_img,
                   int3(int2(frag_co.xy), uniform_buf.render_pass.volume_light_id),
                   float4(vol.scattering, 1.0f));
  }
}

PipelineCompute scatter(scatter_main, Scatter{.use_volume_light = false});
PipelineCompute scatter_with_lights(scatter_main, Scatter{.use_volume_light = true});
PipelineCompute integration(integration_main);
PipelineGraphic resolve(resolve_vert, resolve_frag);
}  // namespace eevee::volume
