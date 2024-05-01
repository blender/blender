/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Sampling data accessors and random number generators.
 * Also contains some sample mapping functions.
 */

#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name Sampling data.
 *
 * Return a random values from Low Discrepancy Sequence in [0..1) range.
 * This value is uniform (constant) for the whole scene sample.
 * You might want to couple it with a noise function.
 * \{ */

#ifdef EEVEE_SAMPLING_DATA

float sampling_rng_1D_get(const eSamplingDimension dimension)
{
  return sampling_buf.dimensions[dimension];
}

vec2 sampling_rng_2D_get(const eSamplingDimension dimension)
{
  return vec2(sampling_buf.dimensions[dimension], sampling_buf.dimensions[dimension + 1u]);
}

vec3 sampling_rng_3D_get(const eSamplingDimension dimension)
{
  return vec3(sampling_buf.dimensions[dimension],
              sampling_buf.dimensions[dimension + 1u],
              sampling_buf.dimensions[dimension + 2u]);
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Random Number Generators.
 * \{ */

/* Interleaved gradient noise by Jorge Jimenez
 * http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
 * Seeding found by Epic Game. */
float interlieved_gradient_noise(vec2 pixel, float seed, float offset)
{
  pixel += seed * (vec2(47, 17) * 0.695);
  return fract(offset + 52.9829189 * fract(0.06711056 * pixel.x + 0.00583715 * pixel.y));
}

vec2 interlieved_gradient_noise(vec2 pixel, vec2 seed, vec2 offset)
{
  return vec2(interlieved_gradient_noise(pixel, seed.x, offset.x),
              interlieved_gradient_noise(pixel, seed.y, offset.y));
}

vec3 interlieved_gradient_noise(vec2 pixel, vec3 seed, vec3 offset)
{
  return vec3(interlieved_gradient_noise(pixel, seed.x, offset.x),
              interlieved_gradient_noise(pixel, seed.y, offset.y),
              interlieved_gradient_noise(pixel, seed.z, offset.z));
}

/* From: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html */
float van_der_corput_radical_inverse(uint bits)
{
#if 0 /* Reference */
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
#else
  bits = bitfieldReverse(bits);
#endif
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

vec2 hammersley_2d(uint i, uint sample_count)
{
  vec2 rand;
  rand.x = float(i) / float(sample_count);
  rand.y = van_der_corput_radical_inverse(i);
  return rand;
}

vec2 hammersley_2d(int i, int sample_count)
{
  return hammersley_2d(uint(i), uint(sample_count));
}

/* Not random but still useful. sample_count should be an even. */
vec2 regular_grid_2d(int i, int sample_count)
{
  int sample_per_dim = int(sqrt(float(sample_count)));
  return (vec2(i % sample_per_dim, i / sample_per_dim) + 0.5) / float(sample_per_dim);
}

/* PCG */

/* https://www.pcg-random.org/ */
uint pcg_uint(uint u)
{
  uint state = u * 747796405u + 2891336453u;
  uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

float pcg(float v)
{
  return pcg_uint(floatBitsToUint(v)) / float(0xffffffffU);
}

float pcg(vec2 v)
{
  /* Nested pcg (faster and better quality that pcg2d). */
  uvec2 u = floatBitsToUint(v);
  return pcg_uint(pcg_uint(u.x) + u.y) / float(0xffffffffU);
}

/* http://www.jcgt.org/published/0009/03/02/ */
vec3 pcg3d(vec3 v)
{
  uvec3 u = floatBitsToUint(v);

  u = u * 1664525u + 1013904223u;

  u.x += u.y * u.z;
  u.y += u.z * u.x;
  u.z += u.x * u.y;

  u ^= u >> 16u;

  u.x += u.y * u.z;
  u.y += u.z * u.x;
  u.z += u.x * u.y;

  return vec3(u) / float(0xffffffffU);
}

/* http://www.jcgt.org/published/0009/03/02/ */
vec4 pcg4d(vec4 v)
{
  uvec4 u = floatBitsToUint(v);

  u = u * 1664525u + 1013904223u;

  u.x += u.y * u.w;
  u.y += u.z * u.x;
  u.z += u.x * u.y;
  u.w += u.y * u.z;

  u ^= u >> 16u;

  u.x += u.y * u.w;
  u.y += u.z * u.x;
  u.z += u.x * u.y;
  u.w += u.y * u.z;

  return vec4(u) / float(0xffffffffU);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Distribution mapping.
 *
 * Functions mapping input random numbers to sampling shapes (i.e: hemisphere).
 * \{ */

/* Given 1 random number in [0..1] range, return a random unit circle sample. */
vec2 sample_circle(float rand)
{
  float phi = (rand - 0.5) * M_TAU;
  float cos_phi = cos(phi);
  float sin_phi = sqrt(1.0 - square(cos_phi)) * sign(phi);
  return vec2(cos_phi, sin_phi);
}

/* Given 2 random number in [0..1] range, return a random unit disk sample. */
vec2 sample_disk(vec2 rand)
{
  return sample_circle(rand.y) * sqrt(rand.x);
}

/* This transform a 2d random sample (in [0..1] range) to a sample located on a cylinder of the
 * same range. This is because the sampling functions expect such a random sample which is
 * normally precomputed. */
vec3 sample_cylinder(vec2 rand)
{
  return vec3(rand.x, sample_circle(rand.y));
}

vec3 sample_sphere(vec2 rand)
{
  float cos_theta = rand.x * 2.0 - 1.0;
  float sin_theta = safe_sqrt(1.0 - cos_theta * cos_theta);
  return vec3(sin_theta * sample_circle(rand.y), cos_theta);
}

/**
 * Uniform hemisphere distribution.
 * \a rand is 2 random float in the [0..1] range.
 * Returns point on a Z positive hemisphere of radius 1 and centered on the origin.
 */
vec3 sample_hemisphere(vec2 rand)
{
  float cos_theta = rand.x;
  float sin_theta = safe_sqrt(1.0 - square(cos_theta));
  return vec3(sin_theta * sample_circle(rand.y), cos_theta);
}

/**
 * Uniform cone distribution.
 * \a rand is 2 random float in the [0..1] range.
 * \a cos_angle is the cosine of the half angle.
 * Returns point on a Z positive hemisphere of radius 1 and centered on the origin.
 */
vec3 sample_uniform_cone(vec2 rand, float cos_angle)
{
  float cos_theta = mix(cos_angle, 1.0, rand.x);
  float sin_theta = safe_sqrt(1.0 - square(cos_theta));
  return vec3(sin_theta * sample_circle(rand.y), cos_theta);
}

/** \} */
