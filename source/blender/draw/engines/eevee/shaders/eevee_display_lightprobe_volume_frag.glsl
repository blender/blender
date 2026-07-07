/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_lightprobe_volume_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_display_lightprobe_volume)

#include "draw_view_lib.glsl"
#include "eevee_spherical_harmonics_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"

void main()
{
  float dist_sqr = dot(lP, lP);

  /* Discard outside the circle. */
  if (dist_sqr > 1.0f) {
    gpu_discard_fragment();
    return;
  }

  SphericalHarmonicL1 sh;
  sh.L0.M0 = texelFetch(irradiance_a_tx, cell, 0);
  sh.L1.Mn1 = texelFetch(irradiance_b_tx, cell, 0);
  sh.L1.M0 = texelFetch(irradiance_c_tx, cell, 0);
  sh.L1.Mp1 = texelFetch(irradiance_d_tx, cell, 0);
  float validity = texelFetch(validity_tx, cell, 0).r;

  float3 vN = float3(lP, sqrt(max(0.0f, 1.0f - dist_sqr)));
  float3 N = drw_normal_view_to_world(vN);
  float3 lN = transform_direction(world_to_grid, N);

  float3 irradiance = spherical_harmonics_evaluate_lambert(lN, sh);

  if (display_validity) {
    out_color = float4(mix(float3(1.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), validity), 0.0f);
  }
  else {
    out_color = float4(irradiance, 0.0f);
  }
}
