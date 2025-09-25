/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Spherical Harmonics Functions
 *
 * `L` denote the row and `M` the column in the spherical harmonics table (1).
 * `p` denote positive column and `n` negative ones.
 *
 * Use precomputed constants to avoid constant folding differences across compilers.
 * Note that (2) doesn't use Condon-Shortley phase whereas our implementation does.
 *
 * Reference:
 * (1) https://en.wikipedia.org/wiki/Spherical_harmonics#/media/File:Sphericalfunctions.svg
 * (2) https://en.wikipedia.org/wiki/Table_of_spherical_harmonics#Real_spherical_harmonics
 * (3) https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
 *
 * \{ */

/* L0 Band. */
float spherical_harmonics_L0_M0(float3 v)
{
  return 0.282094792f;
}

/* L1 Band. */
float spherical_harmonics_L1_Mn1(float3 v)
{
  return -0.488602512f * v.y;
}
float spherical_harmonics_L1_M0(float3 v)
{
  return 0.488602512f * v.z;
}
float spherical_harmonics_L1_Mp1(float3 v)
{
  return -0.488602512f * v.x;
}

/* L2 Band. */
float spherical_harmonics_L2_Mn2(float3 v)
{
  return 1.092548431f * (v.x * v.y);
}
float spherical_harmonics_L2_Mn1(float3 v)
{
  return -1.092548431f * (v.y * v.z);
}
float spherical_harmonics_L2_M0(float3 v)
{
  return 0.315391565f * (3.0f * v.z * v.z - 1.0f);
}
float spherical_harmonics_L2_Mp1(float3 v)
{
  return -1.092548431f * (v.x * v.z);
}
float spherical_harmonics_L2_Mp2(float3 v)
{
  return 0.546274215f * (v.x * v.x - v.y * v.y);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Structure
 * \{ */

struct SphericalHarmonicBandL0 {
  float4 M0;
};

struct SphericalHarmonicBandL1 {
  float4 Mn1;
  float4 M0;
  float4 Mp1;
};

struct SphericalHarmonicBandL2 {
  float4 Mn2;
  float4 Mn1;
  float4 M0;
  float4 Mp1;
  float4 Mp2;
};

struct SphericalHarmonicL0 {
  SphericalHarmonicBandL0 L0;
};

struct SphericalHarmonicL1 {
  SphericalHarmonicBandL0 L0;
  SphericalHarmonicBandL1 L1;
};

struct SphericalHarmonicL2 {
  SphericalHarmonicBandL0 L0;
  SphericalHarmonicBandL1 L1;
  SphericalHarmonicBandL2 L2;
};

SphericalHarmonicBandL0 spherical_harmonics_band_L0_new()
{
  SphericalHarmonicBandL0 L0;
  L0.M0 = float4(0.0f);
  return L0;
}

SphericalHarmonicBandL1 spherical_harmonics_band_L1_new()
{
  SphericalHarmonicBandL1 L1;
  L1.Mn1 = float4(0.0f);
  L1.M0 = float4(0.0f);
  L1.Mp1 = float4(0.0f);
  return L1;
}

SphericalHarmonicBandL2 spherical_harmonics_band_L2_new()
{
  SphericalHarmonicBandL2 L2;
  L2.Mn2 = float4(0.0f);
  L2.Mn1 = float4(0.0f);
  L2.M0 = float4(0.0f);
  L2.Mp1 = float4(0.0f);
  L2.Mp2 = float4(0.0f);
  return L2;
}

SphericalHarmonicL0 spherical_harmonics_L0_new()
{
  SphericalHarmonicL0 sh;
  sh.L0 = spherical_harmonics_band_L0_new();
  return sh;
}

SphericalHarmonicL1 spherical_harmonics_L1_new()
{
  SphericalHarmonicL1 sh;
  sh.L0 = spherical_harmonics_band_L0_new();
  sh.L1 = spherical_harmonics_band_L1_new();
  return sh;
}

SphericalHarmonicL2 spherical_harmonics_L2_new()
{
  SphericalHarmonicL2 sh;
  sh.L0 = spherical_harmonics_band_L0_new();
  sh.L1 = spherical_harmonics_band_L1_new();
  sh.L2 = spherical_harmonics_band_L2_new();
  return sh;
}

SphericalHarmonicBandL0 spherical_harmonics_band_L0_swizzle_wwww(SphericalHarmonicBandL0 L0)
{
  L0.M0 = L0.M0.wwww;
  return L0;
}

SphericalHarmonicBandL1 spherical_harmonics_band_L1_swizzle_wwww(SphericalHarmonicBandL1 L1)
{
  L1.Mn1 = L1.Mn1.wwww;
  L1.M0 = L1.M0.wwww;
  L1.Mp1 = L1.Mp1.wwww;
  return L1;
}

SphericalHarmonicBandL2 spherical_harmonics_band_L2_swizzle_wwww(SphericalHarmonicBandL2 L2)
{
  L2.Mn2 = L2.Mn2.wwww;
  L2.Mn1 = L2.Mn1.wwww;
  L2.M0 = L2.M0.wwww;
  L2.Mp1 = L2.Mp1.wwww;
  L2.Mp2 = L2.Mp2.wwww;
  return L2;
}

SphericalHarmonicL0 spherical_harmonics_swizzle_wwww(SphericalHarmonicL0 sh)
{
  sh.L0 = spherical_harmonics_band_L0_swizzle_wwww(sh.L0);
  return sh;
}

SphericalHarmonicL1 spherical_harmonics_swizzle_wwww(SphericalHarmonicL1 sh)
{
  sh.L0 = spherical_harmonics_band_L0_swizzle_wwww(sh.L0);
  sh.L1 = spherical_harmonics_band_L1_swizzle_wwww(sh.L1);
  return sh;
}

SphericalHarmonicL2 spherical_harmonics_swizzle_wwww(SphericalHarmonicL2 sh)
{
  sh.L0 = spherical_harmonics_band_L0_swizzle_wwww(sh.L0);
  sh.L1 = spherical_harmonics_band_L1_swizzle_wwww(sh.L1);
  sh.L2 = spherical_harmonics_band_L2_swizzle_wwww(sh.L2);
  return sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Encode
 *
 * Decompose an input signal into spherical harmonic coefficients.
 * Note that `amplitude` need to be scaled by solid angle.
 * \{ */

void spherical_harmonics_L0_encode_signal_sample(float3 direction,
                                                 float4 amplitude,
                                                 inout SphericalHarmonicBandL0 r_L0)
{
  r_L0.M0 += spherical_harmonics_L0_M0(direction) * amplitude;
}

void spherical_harmonics_L1_encode_signal_sample(float3 direction,
                                                 float4 amplitude,
                                                 inout SphericalHarmonicBandL1 r_L1)
{
  r_L1.Mn1 += spherical_harmonics_L1_Mn1(direction) * amplitude;
  r_L1.M0 += spherical_harmonics_L1_M0(direction) * amplitude;
  r_L1.Mp1 += spherical_harmonics_L1_Mp1(direction) * amplitude;
}

void spherical_harmonics_L2_encode_signal_sample(float3 direction,
                                                 float4 amplitude,
                                                 inout SphericalHarmonicBandL2 r_L2)
{
  r_L2.Mn2 += spherical_harmonics_L2_Mn2(direction) * amplitude;
  r_L2.Mn1 += spherical_harmonics_L2_Mn1(direction) * amplitude;
  r_L2.M0 += spherical_harmonics_L2_M0(direction) * amplitude;
  r_L2.Mp1 += spherical_harmonics_L2_Mp1(direction) * amplitude;
  r_L2.Mp2 += spherical_harmonics_L2_Mp2(direction) * amplitude;
}

void spherical_harmonics_encode_signal_sample(float3 direction,
                                              float4 amplitude,
                                              inout SphericalHarmonicL0 sh)
{
  spherical_harmonics_L0_encode_signal_sample(direction, amplitude, sh.L0);
}

void spherical_harmonics_encode_signal_sample(float3 direction,
                                              float4 amplitude,
                                              inout SphericalHarmonicL1 sh)
{
  spherical_harmonics_L0_encode_signal_sample(direction, amplitude, sh.L0);
  spherical_harmonics_L1_encode_signal_sample(direction, amplitude, sh.L1);
}

void spherical_harmonics_encode_signal_sample(float3 direction,
                                              float4 amplitude,
                                              inout SphericalHarmonicL2 sh)
{
  spherical_harmonics_L0_encode_signal_sample(direction, amplitude, sh.L0);
  spherical_harmonics_L1_encode_signal_sample(direction, amplitude, sh.L1);
  spherical_harmonics_L2_encode_signal_sample(direction, amplitude, sh.L2);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Decode
 *
 * Evaluate an encoded signal in a given unit vector direction.
 * \{ */

float4 spherical_harmonics_L0_evaluate(float3 direction, SphericalHarmonicBandL0 L0)
{
  return spherical_harmonics_L0_M0(direction) * L0.M0;
}

float4 spherical_harmonics_L1_evaluate(float3 direction, SphericalHarmonicBandL1 L1)
{
  return spherical_harmonics_L1_Mn1(direction) * L1.Mn1 +
         spherical_harmonics_L1_M0(direction) * L1.M0 +
         spherical_harmonics_L1_Mp1(direction) * L1.Mp1;
}

float4 spherical_harmonics_L2_evaluate(float3 direction, SphericalHarmonicBandL2 L2)
{
  return spherical_harmonics_L2_Mn2(direction) * L2.Mn2 +
         spherical_harmonics_L2_Mn1(direction) * L2.Mn1 +
         spherical_harmonics_L2_M0(direction) * L2.M0 +
         spherical_harmonics_L2_Mp1(direction) * L2.Mp1 +
         spherical_harmonics_L2_Mp2(direction) * L2.Mp2;
}

float3 spherical_harmonics_evaluate(float3 direction, SphericalHarmonicL0 sh)
{
  float3 radiance = spherical_harmonics_L0_evaluate(direction, sh.L0).rgb;
  return max(float3(0.0f), radiance);
}

float3 spherical_harmonics_evaluate(float3 direction, SphericalHarmonicL1 sh)
{
  float3 radiance = spherical_harmonics_L0_evaluate(direction, sh.L0).rgb +
                    spherical_harmonics_L1_evaluate(direction, sh.L1).rgb;
  return max(float3(0.0f), radiance);
}

float3 spherical_harmonics_evaluate(float3 direction, SphericalHarmonicL2 sh)
{
  float3 radiance = spherical_harmonics_L0_evaluate(direction, sh.L0).rgb +
                    spherical_harmonics_L1_evaluate(direction, sh.L1).rgb +
                    spherical_harmonics_L2_evaluate(direction, sh.L2).rgb;
  return max(float3(0.0f), radiance);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rotation
 * \{ */

SphericalHarmonicBandL0 spherical_harmonics_L0_rotate(float3x3 rotation,
                                                      SphericalHarmonicBandL0 L0)
{
  /* L0 band being a constant function (i.e: there is no directionality) there is nothing to
   * rotate. This is a no-op. */
  return L0;
}

SphericalHarmonicBandL1 spherical_harmonics_L1_rotate(float3x3 rotation,
                                                      SphericalHarmonicBandL1 L1)
{
  /* Convert L1 coefficients to per channel column.
   * Note the component shuffle to match blender coordinate system. */
  float4x3 per_channel = transpose(float3x4(L1.Mp1, L1.Mn1, -L1.M0));
  /* Rotate each channel. */
  per_channel[0] = rotation * per_channel[0];
  per_channel[1] = rotation * per_channel[1];
  per_channel[2] = rotation * per_channel[2];
  per_channel[3] = rotation * per_channel[3];
  /* Convert back from L1 coefficients to per channel column.
   * Note the component shuffle to match blender coordinate system. */
  float3x4 per_coef = transpose(per_channel);
  L1.Mn1 = per_coef[1];
  L1.M0 = -per_coef[2];
  L1.Mp1 = per_coef[0];
  return L1;
}

SphericalHarmonicL1 spherical_harmonics_rotate(float3x3 rotation, SphericalHarmonicL1 sh)
{
  sh.L0 = spherical_harmonics_L0_rotate(rotation, sh.L0);
  sh.L1 = spherical_harmonics_L1_rotate(rotation, sh.L1);
  return sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation
 * \{ */

/**
 * Convolve a spherical harmonic encoded irradiance signal as a lambertian reflection.
 * Returns the lambertian radiance (cosine lobe divided by PI) so the coefficients simplify to 1,
 * 2/3 and 1/4. See this reference for more explanation:
 * https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
 */
float3 spherical_harmonics_evaluate_lambert(float3 N, SphericalHarmonicL0 sh)
{
  float3 radiance = spherical_harmonics_L0_evaluate(N, sh.L0).rgb;
  return max(float3(0.0f), radiance);
}
float3 spherical_harmonics_evaluate_lambert(float3 N, SphericalHarmonicL1 sh)
{
  float3 radiance = spherical_harmonics_L0_evaluate(N, sh.L0).rgb +
                    spherical_harmonics_L1_evaluate(N, sh.L1).rgb * (2.0f / 3.0f);
  return max(float3(0.0f), radiance);
}
float3 spherical_harmonics_evaluate_lambert(float3 N, SphericalHarmonicL2 sh)
{
  float3 radiance = spherical_harmonics_L0_evaluate(N, sh.L0).rgb +
                    spherical_harmonics_L1_evaluate(N, sh.L1).rgb * (2.0f / 3.0f) +
                    spherical_harmonics_L2_evaluate(N, sh.L2).rgb * (1.0f / 4.0f);
  return max(float3(0.0f), radiance);
}

/**
 * Use non-linear reconstruction method to avoid negative lobe artifacts.
 * See this reference for more explanation:
 * https://grahamhazel.com/blog/2017/12/22/converting-sh-radiance-to-irradiance/
 */
float spherical_harmonics_evaluate_non_linear(float3 N, float R0, float3 R1)
{
  /* No idea why this is needed. */
  R1 /= 2.0f;

  float R1_len;
  float3 R1_dir = normalize_and_get_length(R1, R1_len);
  float rcp_R0 = safe_rcp(R0);

  float q = (1.0f + dot(R1_dir, N)) / 2.0f;
  float p = 1.0f + 2.0f * R1_len * rcp_R0;
  float a = (1.0f - R1_len * rcp_R0) * safe_rcp(1.0f + R1_len * rcp_R0);

  return R0 * (a + (1.0f - a) * (p + 1.0f) * pow(q, p));
}
float3 spherical_harmonics_evaluate_lambert_non_linear(float3 N, SphericalHarmonicL1 sh)
{
  /* Shuffling based on spherical_harmonics_L1_* functions. */
  float3 R1_r = float3(-sh.L1.Mp1.r, -sh.L1.Mn1.r, sh.L1.M0.r);
  float3 R1_g = float3(-sh.L1.Mp1.g, -sh.L1.Mn1.g, sh.L1.M0.g);
  float3 R1_b = float3(-sh.L1.Mp1.b, -sh.L1.Mn1.b, sh.L1.M0.b);

  float3 radiance = float3(spherical_harmonics_evaluate_non_linear(N, sh.L0.M0.r, R1_r),
                           spherical_harmonics_evaluate_non_linear(N, sh.L0.M0.g, R1_g),
                           spherical_harmonics_evaluate_non_linear(N, sh.L0.M0.b, R1_b));
  /* Return lambertian radiance. So divide by PI. */
  return radiance / M_PI;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Load/Store
 *
 * This section define the compression scheme of spherical harmonic data.
 * \{ */

SphericalHarmonicL1 spherical_harmonics_unpack(float4 L0_L1_a,
                                               float4 L0_L1_b,
                                               float4 L0_L1_c,
                                               float4 L0_L1_vis)
{
  SphericalHarmonicL1 sh;
  sh.L0.M0.xyz = L0_L1_a.xyz;
  sh.L1.Mn1.xyz = L0_L1_b.xyz;
  sh.L1.M0.xyz = L0_L1_c.xyz;
  sh.L1.Mp1.xyz = float3(L0_L1_a.w, L0_L1_b.w, L0_L1_c.w);
  sh.L0.M0.w = L0_L1_vis.x;
  sh.L1.Mn1.w = L0_L1_vis.y;
  sh.L1.M0.w = L0_L1_vis.z;
  sh.L1.Mp1.w = L0_L1_vis.w;
  return sh;
}

void spherical_harmonics_pack(SphericalHarmonicL1 sh,
                              out float4 L0_L1_a,
                              out float4 L0_L1_b,
                              out float4 L0_L1_c,
                              out float4 L0_L1_vis)
{
  L0_L1_a.xyz = sh.L0.M0.xyz;
  L0_L1_b.xyz = sh.L1.Mn1.xyz;
  L0_L1_c.xyz = sh.L1.M0.xyz;
  L0_L1_a.w = sh.L1.Mp1.x;
  L0_L1_b.w = sh.L1.Mp1.y;
  L0_L1_c.w = sh.L1.Mp1.z;
  L0_L1_vis = float4(sh.L0.M0.w, sh.L1.Mn1.w, sh.L1.M0.w, sh.L1.Mp1.w);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Triple Product
 * \{ */

SphericalHarmonicL1 spherical_harmonics_triple_product(SphericalHarmonicL1 a,
                                                       SphericalHarmonicL1 b)
{
  /* Adapted from:
   * "Code Generation and Factoring for Fast Evaluation of Low-order Spherical Harmonic Products
   * and Squares" Function "SH_product_3". */
  constexpr float L0_M0_coef = 0.282094792f;
  SphericalHarmonicL1 sh;
  sh.L0.M0 = a.L0.M0 * b.L0.M0;
  sh.L0.M0 += a.L1.Mn1 * b.L1.Mn1;
  sh.L0.M0 += a.L1.M0 * b.L1.M0;
  sh.L0.M0 += a.L1.Mp1 * b.L1.Mp1;
  sh.L0.M0 *= L0_M0_coef;

  sh.L1.Mn1 = L0_M0_coef * (a.L0.M0 * b.L1.Mn1 + b.L0.M0 * a.L1.Mn1);
  sh.L1.M0 = L0_M0_coef * (a.L0.M0 * b.L1.M0 + b.L0.M0 * a.L1.M0);
  sh.L1.Mp1 = L0_M0_coef * (a.L0.M0 * b.L1.Mp1 + b.L0.M0 * a.L1.Mp1);
  return sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Multiply Add
 * \{ */

SphericalHarmonicBandL0 spherical_harmonics_L0_madd(SphericalHarmonicBandL0 a,
                                                    float b,
                                                    SphericalHarmonicBandL0 c)
{
  SphericalHarmonicBandL0 result;
  result.M0 = a.M0 * b + c.M0;
  return result;
}

SphericalHarmonicBandL1 spherical_harmonics_L1_madd(SphericalHarmonicBandL1 a,
                                                    float b,
                                                    SphericalHarmonicBandL1 c)
{
  SphericalHarmonicBandL1 result;
  result.Mn1 = a.Mn1 * b + c.Mn1;
  result.M0 = a.M0 * b + c.M0;
  result.Mp1 = a.Mp1 * b + c.Mp1;
  return result;
}

SphericalHarmonicBandL2 spherical_harmonics_L2_madd(SphericalHarmonicBandL2 a,
                                                    float b,
                                                    SphericalHarmonicBandL2 c)
{
  SphericalHarmonicBandL2 result;
  result.Mn2 = a.Mn2 * b + c.Mn2;
  result.Mn1 = a.Mn1 * b + c.Mn1;
  result.M0 = a.M0 * b + c.M0;
  result.Mp1 = a.Mp1 * b + c.Mp1;
  result.Mp2 = a.Mp2 * b + c.Mp2;
  return result;
}

SphericalHarmonicL0 spherical_harmonics_madd(SphericalHarmonicL0 a, float b, SphericalHarmonicL0 c)
{
  SphericalHarmonicL0 result;
  result.L0 = spherical_harmonics_L0_madd(a.L0, b, c.L0);
  return result;
}

SphericalHarmonicL1 spherical_harmonics_madd(SphericalHarmonicL1 a, float b, SphericalHarmonicL1 c)
{
  SphericalHarmonicL1 result;
  result.L0 = spherical_harmonics_L0_madd(a.L0, b, c.L0);
  result.L1 = spherical_harmonics_L1_madd(a.L1, b, c.L1);
  return result;
}

SphericalHarmonicL2 spherical_harmonics_madd(SphericalHarmonicL2 a, float b, SphericalHarmonicL2 c)
{
  SphericalHarmonicL2 result;
  result.L0 = spherical_harmonics_L0_madd(a.L0, b, c.L0);
  result.L1 = spherical_harmonics_L1_madd(a.L1, b, c.L1);
  result.L2 = spherical_harmonics_L2_madd(a.L2, b, c.L2);
  return result;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Multiply
 * \{ */

SphericalHarmonicBandL0 spherical_harmonics_L0_mul(SphericalHarmonicBandL0 a, float b)
{
  SphericalHarmonicBandL0 result;
  result.M0 = a.M0 * b;
  return result;
}

SphericalHarmonicBandL1 spherical_harmonics_L1_mul(SphericalHarmonicBandL1 a, float b)
{
  SphericalHarmonicBandL1 result;
  result.Mn1 = a.Mn1 * b;
  result.M0 = a.M0 * b;
  result.Mp1 = a.Mp1 * b;
  return result;
}

SphericalHarmonicBandL2 spherical_harmonics_L2_mul(SphericalHarmonicBandL2 a, float b)
{
  SphericalHarmonicBandL2 result;
  result.Mn2 = a.Mn2 * b;
  result.Mn1 = a.Mn1 * b;
  result.M0 = a.M0 * b;
  result.Mp1 = a.Mp1 * b;
  result.Mp2 = a.Mp2 * b;
  return result;
}

SphericalHarmonicL0 spherical_harmonics_mul(SphericalHarmonicL0 a, float b)
{
  SphericalHarmonicL0 result;
  result.L0 = spherical_harmonics_L0_mul(a.L0, b);
  return result;
}

SphericalHarmonicL1 spherical_harmonics_mul(SphericalHarmonicL1 a, float b)
{
  SphericalHarmonicL1 result;
  result.L0 = spherical_harmonics_L0_mul(a.L0, b);
  result.L1 = spherical_harmonics_L1_mul(a.L1, b);
  return result;
}

SphericalHarmonicL2 spherical_harmonics_mul(SphericalHarmonicL2 a, float b)
{
  SphericalHarmonicL2 result;
  result.L0 = spherical_harmonics_L0_mul(a.L0, b);
  result.L1 = spherical_harmonics_L1_mul(a.L1, b);
  result.L2 = spherical_harmonics_L2_mul(a.L2, b);
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add
 * \{ */

SphericalHarmonicBandL0 spherical_harmonics_L0_add(SphericalHarmonicBandL0 a,
                                                   SphericalHarmonicBandL0 b)
{
  SphericalHarmonicBandL0 result;
  result.M0 = a.M0 + b.M0;
  return result;
}

SphericalHarmonicBandL1 spherical_harmonics_L1_add(SphericalHarmonicBandL1 a,
                                                   SphericalHarmonicBandL1 b)
{
  SphericalHarmonicBandL1 result;
  result.Mn1 = a.Mn1 + b.Mn1;
  result.M0 = a.M0 + b.M0;
  result.Mp1 = a.Mp1 + b.Mp1;
  return result;
}

SphericalHarmonicBandL2 spherical_harmonics_L2_add(SphericalHarmonicBandL2 a,
                                                   SphericalHarmonicBandL2 b)
{
  SphericalHarmonicBandL2 result;
  result.Mn2 = a.Mn2 + b.Mn2;
  result.Mn1 = a.Mn1 + b.Mn1;
  result.M0 = a.M0 + b.M0;
  result.Mp1 = a.Mp1 + b.Mp1;
  result.Mp2 = a.Mp2 + b.Mp2;
  return result;
}

SphericalHarmonicL0 spherical_harmonics_add(SphericalHarmonicL0 a, SphericalHarmonicL0 b)
{
  SphericalHarmonicL0 result;
  result.L0 = spherical_harmonics_L0_add(a.L0, b.L0);
  return result;
}

SphericalHarmonicL1 spherical_harmonics_add(SphericalHarmonicL1 a, SphericalHarmonicL1 b)
{
  SphericalHarmonicL1 result;
  result.L0 = spherical_harmonics_L0_add(a.L0, b.L0);
  result.L1 = spherical_harmonics_L1_add(a.L1, b.L1);
  return result;
}

SphericalHarmonicL2 spherical_harmonics_add(SphericalHarmonicL2 a, SphericalHarmonicL2 b)
{
  SphericalHarmonicL2 result;
  result.L0 = spherical_harmonics_L0_add(a.L0, b.L0);
  result.L1 = spherical_harmonics_L1_add(a.L1, b.L1);
  result.L2 = spherical_harmonics_L2_add(a.L2, b.L2);
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dot
 * \{ */

float4 spherical_harmonics_dot(SphericalHarmonicL1 a, SphericalHarmonicL1 b)
{
  /* Convert coefficients to per channel column. */
  float4x4 a_mat = transpose(float4x4(a.L0.M0, a.L1.Mn1, a.L1.M0, a.L1.Mp1));
  float4x4 b_mat = transpose(float4x4(b.L0.M0, b.L1.Mn1, b.L1.M0, b.L1.Mp1));
  float4 result;
  result[0] = dot(a_mat[0], b_mat[0]);
  result[1] = dot(a_mat[1], b_mat[1]);
  result[2] = dot(a_mat[2], b_mat[2]);
  result[3] = dot(a_mat[3], b_mat[3]);
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compression
 *
 * Described by Josh Hobson in "The indirect Lighting Pipeline of God of War" p. 120
 * \{ */

SphericalHarmonicL1 spherical_harmonics_compress(SphericalHarmonicL1 sh)
{
  SphericalHarmonicL1 result;
  result.L0 = sh.L0;
  float4 fac = safe_rcp(sh.L0.M0 * M_SQRT3);
  result.L1.Mn1 = (sh.L1.Mn1 * fac) * 0.5f + 0.5f;
  result.L1.M0 = (sh.L1.M0 * fac) * 0.5f + 0.5f;
  result.L1.Mp1 = (sh.L1.Mp1 * fac) * 0.5f + 0.5f;
  return result;
}

SphericalHarmonicL1 spherical_harmonics_decompress(SphericalHarmonicL1 sh)
{
  SphericalHarmonicL1 result;
  result.L0 = sh.L0;
  float4 fac = sh.L0.M0 * M_SQRT3;
  result.L1.Mn1 = (sh.L1.Mn1 * 2.0f - 1.0f) * fac;
  result.L1.M0 = (sh.L1.M0 * 2.0f - 1.0f) * fac;
  result.L1.Mp1 = (sh.L1.Mp1 * 2.0f - 1.0f) * fac;
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deringing
 *
 * Change the encoded data to avoid negative value during evaluation.
 * \{ */

SphericalHarmonicL1 spherical_harmonics_dering(SphericalHarmonicL1 sh)
{
  /* Convert coefficients to per channel column. */
  float4x4 m = transpose(float4x4(sh.L0.M0, sh.L1.Mn1, sh.L1.M0, sh.L1.Mp1));
  /* Find maximum value the L1 band can contain that doesn't exhibit ringing artifacts. */
  float fac_r = abs(m[0].x) / max(1e-8f, reduce_max(abs(m[0].yzw)));
  float fac_g = abs(m[1].x) / max(1e-8f, reduce_max(abs(m[1].yzw)));
  float fac_b = abs(m[2].x) / max(1e-8f, reduce_max(abs(m[2].yzw)));
  /* Find the factor for the biggest component. We don't want to have color drift. */
  float fac = reduce_min(float3(fac_r, fac_g, fac_b));
  /* Multiply by each band's weight. */
  fac *= 0.282094792f / 0.488602512f;

  if (fac > 1.0f) {
    return sh;
  }

  SphericalHarmonicL1 result = sh;
  result.L1 = spherical_harmonics_L1_mul(result.L1, fac);
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clamping
 *
 * Clamp the total power of the SH function.
 * \{ */

SphericalHarmonicL1 spherical_harmonics_clamp(SphericalHarmonicL1 sh, float clamp_value)
{
  /* Convert coefficients to per channel column. */
  float4x4 per_channel = transpose(float4x4(sh.L0.M0, sh.L1.Mn1, sh.L1.M0, sh.L1.Mp1));
  /* Magnitude per channel. */
  float3 mag_L1;
  mag_L1.r = length(per_channel[0].yzw);
  mag_L1.g = length(per_channel[1].yzw);
  mag_L1.b = length(per_channel[2].yzw);
  /* Find maximum of the sh function over all channels. */
  float3 max_sh = abs(sh.L0.M0.rgb) * 0.282094792f + mag_L1 * 0.488602512f;

  float fac = clamp_value * safe_rcp(reduce_max(max_sh));
  if (fac > 1.0f) {
    return sh;
  }
  return spherical_harmonics_mul(sh, fac);
}

/** \} */
