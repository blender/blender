/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_lightprobe_sphere_infos.hh"

/* TODO(fclem): Pass the lightprobe_sphere_buf around and avoid relying on interface.
 * Currently in conflict with eevee_lightprobe_volume_load. */
#ifndef SPHERE_PROBE_SELECT
SHADER_LIBRARY_CREATE_INFO(eevee_lightprobe_sphere_data)
#endif

#include "eevee_lightprobe_sphere_mapping_lib.glsl"
#include "eevee_octahedron_lib.glsl"
#include "eevee_spherical_harmonics_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

#ifdef SPHERE_PROBE
float4 lightprobe_spheres_sample(float3 L, float lod, SphereProbeUvArea uv_area)
{
  float lod_min = floor(lod);
  float lod_max = ceil(lod);
  float mix_fac = lod - lod_min;

  float2 altas_uv_min, altas_uv_max;
  sphere_probe_direction_to_uv(L, lod_min, lod_max, uv_area, altas_uv_min, altas_uv_max);

  float4 color_min = textureLod(
      lightprobe_spheres_tx, float3(altas_uv_min, uv_area.layer), lod_min);
  float4 color_max = textureLod(
      lightprobe_spheres_tx, float3(altas_uv_max, uv_area.layer), lod_max);
  return mix(color_min, color_max, mix_fac);
}
#endif

ReflectionProbeLowFreqLight lightprobe_spheres_extract_low_freq(SphericalHarmonicL1 sh)
{
  /* To avoid color shift and negative values, we reduce saturation and directionality. */
  ReflectionProbeLowFreqLight result;
  result.ambient = sh.L0.M0.r + sh.L0.M0.g + sh.L0.M0.b;
  /* Bias to avoid division by zero. */
  result.ambient += 1e-6f;

  float3x4 L1_per_band;
  L1_per_band[0] = sh.L1.Mn1;
  L1_per_band[1] = sh.L1.M0;
  L1_per_band[2] = sh.L1.Mp1;

  float4x3 L1_per_comp = transpose(L1_per_band);
  result.direction = L1_per_comp[0] + L1_per_comp[1] + L1_per_comp[2];

  return result;
}

float lightprobe_spheres_normalization_eval(float3 L,
                                            ReflectionProbeLowFreqLight numerator,
                                            ReflectionProbeLowFreqLight denominator)
{
  /* TODO(fclem): Adjusting directionality is tricky.
   * Needs to be revisited later on. For now only use the ambient term. */
  return saturate(numerator.ambient / denominator.ambient);
}
