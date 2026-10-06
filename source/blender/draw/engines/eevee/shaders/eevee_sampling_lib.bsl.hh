/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Sampling data accessors, noise functions and hash sequences. Also contains
 * several sample mapping functions.
 *
 * References:
 *
 *  [damm2012]  Holger Dammertz
 *              Hammersley Points on Spheres
 *              https://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
 *
 *  [onei2014]  Melissa O'Neill
 *              PCG: A Family of Simple Fast Space-Efficient Good Algorithms for
 *              Random Number Generation.
 *              2014
 *              https://www.pcg-random.org/pdf/hmc-cs-2014-0905.pdf
 *
 *  [jorg2014]  Jorge Jimenez
 *              Next Generation Post-processing in COD Advanced Warfare
 *              SIGGRAPH 2014 Advances in Real-Time Rendering
 *              https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare/
 *
 *  [wymn2017]  Chris Wyman and Morgan McGuire
 *              Hashed Alpha Testing
 *              ACM I3D 2017
 *              https://research.nvidia.com/labs/rtr/publication/wyman2017hashed/
 *
 *  [jarz2020]  Mark Jarzynski and Marc Olano
 *              Hash Functions for GPU Rendering
 *              JCGT 2020
 *              https://www.jcgt.org/published/0009/03/02/
 */

#pragma once

#include "eevee_sampling_shared.hh"
#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_math_constants.bsl.hh"
#include "gpu_shader_math_safe.bsl.hh"

/* -------------------------------------------------------------------- */
/** \name Sampling data.
 *
 * Return a random values from Low Discrepancy Sequence in [0..1) range.
 * This value is uniform (constant) for the whole scene sample.
 * You might want to couple it with a noise function.
 * \{ */

namespace eevee {

struct Sampling {
  [[storage(SAMPLING_BUF_SLOT, read)]] const SamplingData &sampling_buf;

  float rng_1D_get(const eSamplingDimension dim) const
  {
    return sampling_buf.dimensions[dim];
  }

  float2 rng_2D_get(const eSamplingDimension dim) const
  {
    return float2(sampling_buf.dimensions[dim], sampling_buf.dimensions[dim + 1u]);
  }

  float3 rng_3D_get(const eSamplingDimension dim) const
  {
    return float3(sampling_buf.dimensions[dim],
                  sampling_buf.dimensions[dim + 1u],
                  sampling_buf.dimensions[dim + 2u]);
  }
};

}  // namespace eevee

/** \} */

/* -------------------------------------------------------------------- */
/** \name Random number generators: noise functions and hash sequences
 * \{ */

namespace random {

namespace detail {

/** Refer to Sec. 4 in [damm2012]. */
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
  return float(bits) * 2.3283064365386963e-10f;
}

/**
 * Interleaved gradient backing noise function [jorg2014]. Somewhat temporally coherent
 * by producing visibly interleaved gradients. Not very random as a result.
 */
float interleaved_gradient(float2 v, float offset = 0.0f)
{
  return fract(offset + 52.9829189f * fract(0.06711056f * v.x + 0.00583715f * v.y));
}

/**
 * Permutated congruential generator (PCG) integer sequence [onei2014].
 * Good and fast.
 */
uint pcg(uint u)
{
  uint state = u * 747796405u + 2891336453u;
  uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

/* Reciprocal of unsigned integer maximum used in PCG functions below. */
static constexpr float pcg_rcp = 1.0f / float(0xffffffffU);

}  // namespace detail

/** Seeded 1D interleaved gradient noise [jorg2014], with seeding found by Epic Games. */
float interleaved_gradient(float2 pixel, float seed, float offset)
{
  pixel += seed * (float2(47, 17) * 0.695f);
  return detail::interleaved_gradient(pixel, offset);
}

/** Seeded 2D interleaved gradient noise [jorg2014]. */
float2 interleaved_gradient_2d(float2 pixel, float2 seed, float2 offset)
{
  return float2(interleaved_gradient(pixel, seed.x, offset.x),
                interleaved_gradient(pixel, seed.y, offset.y));
}

/** Seeded 3D interleaved gradient noise [jorg2014]. */
float3 interleaved_gradient_3d(float2 pixel, float3 seed, float3 offset)
{
  return float3(interleaved_gradient(pixel, seed.x, offset.x),
                interleaved_gradient(pixel, seed.y, offset.y),
                interleaved_gradient(pixel, seed.z, offset.z));
}

/** 1D PCG hash on arbitrary floats [jarz2020]. */
float pcg(float v)
{
  return detail::pcg(floatBitsToUint(v)) * detail::pcg_rcp;
}

/** 1D PCG nested variant on 2D input [jarz2020]. */
float pcg(float2 v)
{
  uint2 u = floatBitsToUint(v);
  return detail::pcg(detail::pcg(u.x) + u.y) * detail::pcg_rcp;
}

/** 1D PCG nested variant on 3D input [jarz2020]. */
float pcg(float3 v)
{
  uint3 u = floatBitsToUint(v);
  return detail::pcg(detail::pcg(detail::pcg(u.x) + u.y) + u.z) * detail::pcg_rcp;
}

/** 3D PCG variant detailed in [jarz2020]. */
float3 pcg_3d(float3 v)
{
  uint3 u = floatBitsToUint(v);

  u = u * 1664525u + 1013904223u;

  u.x += u.y * u.z;
  u.y += u.z * u.x;
  u.z += u.x * u.y;

  u ^= u >> 16u;

  u.x += u.y * u.z;
  u.y += u.z * u.x;
  u.z += u.x * u.y;

  return float3(u) * detail::pcg_rcp;
}

/** 4D PCG variant detailed in [jarz2020]. */
float4 pcg_4d(float4 v)
{
  uint4 u = floatBitsToUint(v);

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

  return float4(u) * detail::pcg_rcp;
}

/* 2D Noise function using the Hammersley point set, following [damm2012]. */
template<typename T> float2 hammersley_2d(T i, T sample_count)
{
  float2 rand;
  rand.x = float(i) / float(sample_count);
  rand.y = detail::van_der_corput_radical_inverse(uint(i));
  return rand;
}
template float2 hammersley_2d<float>(float i, float sample_count);
template float2 hammersley_2d<uint>(uint i, uint sample_count);
template float2 hammersley_2d<int>(int i, int sample_count);

}  // namespace random

/* Not random but still useful. sample_count should be an even. */
float2 regular_grid_2d(int i, int sample_count)
{
  int sample_per_dim = int(sqrt(float(sample_count)));
  return (float2(i % sample_per_dim, i / sample_per_dim) + 0.5f) / float(sample_per_dim);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Distribution mappings.
 *
 * Functions mapping input random numbers in [0..1] to sampling shapes (i.e: hemisphere).
 * \{ */

/* Given 1 random number in [0..1] range, return a random unit circle sample. */
float2 sample_circle(float rand)
{
  float phi = (rand - 0.5f) * M_TAU;
  float cos_phi = cos(phi);
  float sin_phi = sqrt(1.0f - square(cos_phi)) * sign(phi);
  return float2(cos_phi, sin_phi);
}

/* Given 2 random number in [0..1] range, return a random unit disk sample. */
float2 sample_disk(float2 rand)
{
  return sample_circle(rand.y) * sqrt(rand.x);
}

/* This transform a 2d random sample (in [0..1] range) to a sample located on a cylinder of the
 * same range. This is because the sampling functions expect such a random sample which is
 * normally precomputed. */
float3 sample_cylinder(float2 rand)
{
  return float3(rand.x, sample_circle(rand.y));
}

/**
 * Uniform sphere distribution.
 * \a rand is 2 random float in the [0..1] range.
 * Returns point on a Z positive hemisphere of radius 1 and centered on the origin.
 * PDF = 1 / (4 * pi)
 */
float3 sample_sphere(float2 rand)
{
  float cos_theta = rand.x * 2.0f - 1.0f;
  float sin_theta = safe_sqrt(1.0f - cos_theta * cos_theta);
  return float3(sin_theta * sample_circle(rand.y), cos_theta);
}

/**
 * Returns a point in a ball that is "uniformly" distributed after projection along any axis.
 * PDF = unknown
 */
float3 sample_ball(float3 rand)
{
  /* Completely ad-hoc, but works well in practice and is fast. */
  return sample_sphere(rand.xy) * sqrt(sqrt(rand.z));
}

/**
 * Uniform hemisphere distribution.
 * \a rand is 2 random float in the [0..1] range.
 * Returns point on a Z positive hemisphere of radius 1 and centered on the origin.
 * PDF = 1 / (2 * pi)
 */
float3 sample_hemisphere(float2 rand)
{
  float cos_theta = rand.x;
  float sin_theta = safe_sqrt(1.0f - square(cos_theta));
  return float3(sin_theta * sample_circle(rand.y), cos_theta);
}

/**
 * Uniform cone distribution.
 * \a rand is 2 random float in the [0..1] range.
 * \a cos_angle is the cosine of the half angle.
 * Returns point on a Z positive hemisphere of radius 1 and centered on the origin.
 * PDF = 1 / (2 * pi * (1 - cos_angle))
 */
float3 sample_uniform_cone(float2 rand, float cos_angle)
{
  float cos_theta = mix(cos_angle, 1.0f, rand.x);
  float sin_theta = safe_sqrt(1.0f - square(cos_theta));
  return float3(sin_theta * sample_circle(rand.y), cos_theta);
}

/**
 * Cosine-weighted direction in hemisphere
 * \a rand contains 2 random floats in the [0..1] range
 * PDF = cos_angle / pi
 */
float3 sample_cos_hemisphere(const float2 rand)
{
  const float cos_theta = safe_sqrt(rand.x);
  const float sin_theta = safe_sqrt(1.0f - rand.x);
  return float3(sin_theta * sample_circle(rand.y), cos_theta);
}

/**
 * Cosine-weighted direction in hemisphere
 * \a rand is a random point on a unit cylinder
 * PDF = cos_angle / pi
 */
float3 sample_cos_hemisphere(const float3 rand_cylinder)
{
  const float cos_theta = safe_sqrt(rand_cylinder.x);
  return float3(rand_cylinder.yz * sin_from_cos(cos_theta), cos_theta);
}

/** \} */
