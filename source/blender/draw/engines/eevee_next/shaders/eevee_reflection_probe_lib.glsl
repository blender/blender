/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_octahedron_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_reflection_probe_mapping_lib.glsl)

#ifdef SPHERE_PROBE
vec4 reflection_probes_sample(vec3 L, float lod, SphereProbeUvArea uv_area)
{
  float lod_min = floor(lod);
  float lod_max = ceil(lod);
  float mix_fac = lod - lod_min;

  vec2 altas_uv_min, altas_uv_max;
  sphere_probe_direction_to_uv(L, lod_min, lod_max, uv_area, altas_uv_min, altas_uv_max);

  vec4 color_min = textureLod(reflection_probes_tx, vec3(altas_uv_min, uv_area.layer), lod_min);
  vec4 color_max = textureLod(reflection_probes_tx, vec3(altas_uv_max, uv_area.layer), lod_max);
  return mix(color_min, color_max, mix_fac);
}
#endif

ReflectionProbeLowFreqLight reflection_probes_extract_low_freq(SphericalHarmonicL1 sh)
{
  /* To avoid color shift and negative values, we reduce saturation and directionality. */
  ReflectionProbeLowFreqLight result;
  result.ambient = sh.L0.M0.r + sh.L0.M0.g + sh.L0.M0.b;

  mat3x4 L1_per_band;
  L1_per_band[0] = sh.L1.Mn1;
  L1_per_band[1] = sh.L1.M0;
  L1_per_band[2] = sh.L1.Mp1;

  mat4x3 L1_per_comp = transpose(L1_per_band);
  result.direction = L1_per_comp[0] + L1_per_comp[1] + L1_per_comp[2];

  return result;
}

vec3 reflection_probes_normalization_eval(vec3 L,
                                          ReflectionProbeLowFreqLight numerator,
                                          ReflectionProbeLowFreqLight denominator)
{
  /* TODO(fclem): Adjusting directionality is tricky.
   * Needs to be revisited later on. For now only use the ambient term. */
  return vec3(numerator.ambient * safe_rcp(denominator.ambient));
}
