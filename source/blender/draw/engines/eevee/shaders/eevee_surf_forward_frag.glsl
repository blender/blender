/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 */

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"
#include "infos/eevee_surf_forward_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_nodetree)
FRAGMENT_SHADER_CREATE_INFO(eevee_geom_mesh)
FRAGMENT_SHADER_CREATE_INFO(eevee_surf_forward)

#include "draw_curves_lib.glsl"
#include "draw_view_lib.glsl"
#include "eevee_forward_lib.glsl"
#include "eevee_nodetree_frag_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_surf_lib.glsl"
#include "eevee_volume_lib.glsl"

/* Global thickness because it is needed for closure_to_rgba. */
float g_thickness;

float4 closure_to_rgba(Closure cl_unused)
{
  float3 radiance, transmittance;
  forward_lighting_eval(g_thickness, radiance, transmittance);

  /* Reset for the next closure tree. */
  float noise = utility_tx_fetch(utility_tx, gl_FragCoord.xy, UTIL_BLUE_NOISE_LAYER).r;
  float closure_rand = fract(noise + sampling_rng_1D_get(SAMPLING_CLOSURE));
  closure_weights_reset(closure_rand);

#if defined(MAT_TRANSPARENT) && defined(MAT_SHADER_TO_RGBA)
  float3 V = -drw_world_incident_vector(g_data.P);
  LightProbeSample samp = lightprobe_load(g_data.P, g_data.Ng, V);
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

void main()
{
  init_globals();

  float noise = utility_tx_fetch(utility_tx, gl_FragCoord.xy, UTIL_BLUE_NOISE_LAYER).r;
  float closure_rand = fract(noise + sampling_rng_1D_get(SAMPLING_CLOSURE));

  fragment_displacement();

  g_thickness = nodetree_thickness() * thickness_mode;

  nodetree_surface(closure_rand);

  float3 radiance, transmittance;
  forward_lighting_eval(g_thickness, radiance, transmittance);

  /* Volumetric resolve and compositing. */
  float2 uvs = gl_FragCoord.xy * uniform_buf.volumes.main_view_extent_inv;
  VolumeResolveSample vol = volume_resolve(
      float3(uvs, reverse_z::read(gl_FragCoord.z)), volume_transmittance_tx, volume_scattering_tx);
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
  if (uniform_buf.pipeline.use_monochromatic_transmittance) {
    out_combined_r = float4(radiance.rgb, transmittance.r);
  }
  else {
    out_combined_r = float4(radiance.r, 0.0f, 0.0f, transmittance.r);
    out_combined_g = float4(radiance.g, 0.0f, 0.0f, transmittance.g);
    out_combined_b = float4(radiance.b, 0.0f, 0.0f, transmittance.b);
    out_combined_a = float4(g_holdout, 0.0f, 0.0f, average(transmittance));
  }
}
