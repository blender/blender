/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 */

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_surf_forward_infos.hh"

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

  radiance *= 1.0f - saturate(g_holdout);

  out_radiance = float4(radiance, g_holdout);
  out_transmittance = float4(transmittance, saturate(average(transmittance)));
}
