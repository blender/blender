/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 */
#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_nodetree)
FRAGMENT_SHADER_CREATE_INFO(eevee_geom_mesh)
FRAGMENT_SHADER_CREATE_INFO(eevee_volume_lib)

#include "draw_curves_lib.glsl" /* IWYU pragma: export. For nodetree functions. */
#include "draw_view_lib.glsl"   /* IWYU pragma: export. For nodetree functions. */
#include "eevee_forward_lib.glsl"
#include "eevee_nodetree_frag_lib.glsl"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_lib.glsl"
#include "eevee_surf_lib.glsl"
#include "eevee_volume_lib.bsl.hh"

/* Global thickness because it is needed for closure_to_rgba. */
Thickness g_thickness_forward;

float4 closure_to_rgba_forward(Closure /*cl_unused*/)
{
  float3 radiance, transmittance;
  forward_lighting_eval(g_thickness_forward, radiance, transmittance);

  /* Reset for the next closure tree. */
  float noise = utility_tx_fetch(utility_tx, gl_FragCoord.xy, UTIL_BLUE_NOISE_LAYER).r;
  float closure_rand = fract(noise + sampling_rng_1D_get(SAMPLING_CLOSURE));
  closure_weights_reset(closure_rand);

#if defined(MAT_TRANSPARENT) && defined(MAT_SHADER_TO_RGBA)
  float3 V = -drw_world_incident_vector(g_data.P);
  LightProbeSample samp = lightprobe_load(gl_FragCoord.xy, g_data.P, g_data.Ng, V);
  float3 radiance_behind = lightprobe_spherical_sample_normalized_with_parallax(
      samp, g_data.P, V, 0.0);

#  ifndef MAT_FIRST_LAYER
  int2 texel = int2(gl_FragCoord.xy);
  if (texelFetchExtend(hiz_prev_tx, texel, 0).x != 1.0f) {
    radiance_behind = texelFetch(previous_layer_radiance_tx, texel, 0).xyz;
  }
#  endif

  radiance += radiance_behind * saturate(transmittance);
#endif

  return float4(radiance, saturate(1.0f - average(transmittance)));
}

namespace eevee {

struct SurfaceForward {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_light_data;
  [[legacy_info]] ShaderCreateInfo eevee_lightprobe_data;
  [[legacy_info]] ShaderCreateInfo eevee_utility_texture;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_shadow_data;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
  [[legacy_info]] ShaderCreateInfo eevee_volume_lib;

  [[legacy_info]] ShaderCreateInfo draw_view_culling;

  /* Optionally added depending on the material. */
  // [[legacy_info]] ShaderCreateInfo eevee_render_pass_out;
  // [[legacy_info]] ShaderCreateInfo eevee_cryptomatte_out;
  // [[legacy_info]] ShaderCreateInfo eevee_hiz_prev_data;
  // [[legacy_info]] ShaderCreateInfo eevee_previous_layer_radiance;
};

struct SurfaceForwardFragOut {
  /* Splitting RGB components into different target to overcome the lack of dual source blending
   * with multiple render targets. */
  [[frag_color(0)]] float4 combined_r;
  [[frag_color(1)]] float4 combined_g;
  [[frag_color(2)]] float4 combined_b;
  [[frag_color(3)]] float4 combined_a;
};

/* Early fragment test is needed for render passes support for forward surfaces. */
/* NOTE: This removes the possibility of using gl_FragDepth. */
[[fragment]] [[early_fragment_tests]]
void surf_forward([[resource_table]] SurfaceForward & /*srt*/,
                  [[frag_coord]] const float4 frag_co,
                  [[out]] SurfaceForwardFragOut &frag_out)
{
  init_globals();

  float noise = utility_tx_fetch(utility_tx, gl_FragCoord.xy, UTIL_BLUE_NOISE_LAYER).r;
  float closure_rand = fract(noise + sampling_rng_1D_get(SAMPLING_CLOSURE));

  fragment_displacement();

  g_thickness_forward = Thickness::from(nodetree_thickness(), thickness_mode);

  nodetree_surface(closure_rand);

  float3 radiance, transmittance;
  forward_lighting_eval(g_thickness_forward, radiance, transmittance);

  /* Volumetric resolve and compositing. */
  float2 uvs = gl_FragCoord.xy * uniform_buf.volumes.main_view_extent_inv;
  VolumeResolveSample vol = volume_resolve(
      float3(uvs, reverse_z::read(frag_co.z)), volume_transmittance_tx, volume_scattering_tx);
  /* Removes the part of the volume scattering that has
   * already been added to the destination pixels by the opaque resolve.
   * Since we do that using the blending pipeline we need to account for material transmittance. */
  vol.scattering -= vol.scattering * g_transmittance;
  radiance = radiance * vol.transmittance + vol.scattering;

  eObjectInfoFlag ob_flag = drw_object_infos().flag;
  if (flag_test(ob_flag, OBJECT_HOLDOUT)) {
    g_holdout = 1.0f - average(g_transmittance);
    radiance *= 0.0f;
  }

  g_holdout = saturate(g_holdout);

  radiance *= 1.0f - g_holdout;

  /* There can be 2 frame-buffer layout for forward transparency:
   * - Combined RGB radiance with Monochromatic transmittance.
   * - Channel split RGB radiance & RGB transmittance + Dedicated average alpha with holdout. */
  if (pipeline_buf.use_monochromatic_transmittance) {
    frag_out.combined_r = float4(radiance.rgb, transmittance.r);
  }
  else {
    frag_out.combined_r = float4(radiance.r, 0.0f, 0.0f, transmittance.r);
    frag_out.combined_g = float4(radiance.g, 0.0f, 0.0f, transmittance.g);
    frag_out.combined_b = float4(radiance.b, 0.0f, 0.0f, transmittance.b);
    frag_out.combined_a = float4(g_holdout, 0.0f, 0.0f, average(transmittance));
  }
}

}  // namespace eevee
