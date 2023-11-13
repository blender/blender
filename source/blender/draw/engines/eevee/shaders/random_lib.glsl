/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Random numbers and low discrepancy sequences utilities.
 */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

/* From: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html */
float van_der_corput_radical_inverse(uint bits)
{
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  /* Same as dividing by 0x100000000. */
  return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley_2d(float i, float sample_count)
{
  vec2 rand;
  rand.x = i / sample_count;
  rand.y = van_der_corput_radical_inverse(uint(i));
  return rand;
}

/* This transform a 2d random sample (in [0..1] range) to a sample located on a cylinder of the
 * same range. This is because the sampling functions expect such a random sample which is
 * normally precomputed. */
vec3 rand2d_to_cylinder(vec2 rand)
{
  float theta = rand.x;
  float phi = (rand.y - 0.5) * M_2PI;
  float cos_phi = cos(phi);
  float sin_phi = sqrt(1.0 - sqr(cos_phi)) * sign(phi);
  return vec3(theta, cos_phi, sin_phi);
}
