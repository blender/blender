/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_octahedron_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)

#ifdef REFLECTION_PROBE
vec4 reflection_probes_sample(vec3 L, float lod, ReflectionProbeCoordinate atlas_coord)
{
  vec2 octahedral_uv = octahedral_uv_from_direction(L) * atlas_coord.scale + atlas_coord.offset;
  return textureLod(reflection_probes_tx, vec3(octahedral_uv, atlas_coord.layer), lod);
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
