/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

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

namespace spherical_harmonics {

float L0_M0_coef(float3 /*v*/)
{
  return 0.282094792f;
}

float L1_Mn1_coef(float3 v)
{
  return -0.488602512f * v.y;
}
float L1_M0_coef(float3 v)
{
  return 0.488602512f * v.z;
}
float L1_Mp1_coef(float3 v)
{
  return -0.488602512f * v.x;
}

float L2_Mn2_coef(float3 v)
{
  return 1.092548431f * (v.x * v.y);
}
float L2_Mn1_coef(float3 v)
{
  return -1.092548431f * (v.y * v.z);
}
float L2_M0_coef(float3 v)
{
  return 0.315391565f * (3.0f * v.z * v.z - 1.0f);
}
float L2_Mp1_coef(float3 v)
{
  return -1.092548431f * (v.x * v.z);
}
float L2_Mp2_coef(float3 v)
{
  return 0.546274215f * (v.x * v.x - v.y * v.y);
}

template<typename T> struct BandL0 {
  T M0;

  void encode_signal_sample(float3 direction, T amplitude)
  {
    M0 += L0_M0_coef(direction) * amplitude;
  }

  T evaluate(float3 direction) const
  {
    return L0_M0_coef(direction) * M0;
  }

  static BandL0<T> madd(BandL0<T> a, float b, BandL0<T> c)
  {
    BandL0<T> result;
    result.M0 = a.M0 * b + c.M0;
    return result;
  }

  static BandL0<T> mul_scalar(BandL0<T> a, float b)
  {
    BandL0<T> result;
    result.M0 = a.M0 * b;
    return result;
  }

  static BandL0<T> mul(BandL0<T> a, T b)
  {
    BandL0<T> result;
    result.M0 = a.M0 * b;
    return result;
  }

  static BandL0<T> add(BandL0<T> a, BandL0<T> b)
  {
    BandL0<T> result;
    result.M0 = a.M0 + b.M0;
    return result;
  }
};

template<typename T> struct BandL1 {
  T Mn1, M0, Mp1;

  void encode_signal_sample(float3 direction, T amplitude)
  {
    Mn1 += L1_Mn1_coef(direction) * amplitude;
    M0 += L1_M0_coef(direction) * amplitude;
    Mp1 += L1_Mp1_coef(direction) * amplitude;
  }

  T evaluate(float3 direction) const
  {
    return L1_Mn1_coef(direction) * Mn1 + L1_M0_coef(direction) * M0 +
           L1_Mp1_coef(direction) * Mp1;
  }

  static BandL1<T> madd(BandL1<T> a, float b, BandL1<T> c)
  {
    BandL1<T> result;
    result.Mn1 = a.Mn1 * b + c.Mn1;
    result.M0 = a.M0 * b + c.M0;
    result.Mp1 = a.Mp1 * b + c.Mp1;
    return result;
  }

  static BandL1<T> mul_scalar(BandL1<T> a, float b)
  {
    BandL1<T> result;
    result.Mn1 = a.Mn1 * b;
    result.M0 = a.M0 * b;
    result.Mp1 = a.Mp1 * b;
    return result;
  }

  static BandL1<T> mul(BandL1<T> a, T b)
  {
    BandL1<T> result;
    result.Mn1 = a.Mn1 * b;
    result.M0 = a.M0 * b;
    result.Mp1 = a.Mp1 * b;
    return result;
  }

  static BandL1<T> add(BandL1<T> a, BandL1<T> b)
  {
    BandL1<T> result;
    result.Mn1 = a.Mn1 + b.Mn1;
    result.M0 = a.M0 + b.M0;
    result.Mp1 = a.Mp1 + b.Mp1;
    return result;
  }
};

template<typename T> struct BandL2 {
  T Mn2, Mn1, M0, Mp1, Mp2;

  void encode_signal_sample(float3 direction, T amplitude)
  {
    Mn2 += L2_Mn2_coef(direction) * amplitude;
    Mn1 += L2_Mn1_coef(direction) * amplitude;
    M0 += L2_M0_coef(direction) * amplitude;
    Mp1 += L2_Mp1_coef(direction) * amplitude;
    Mp2 += L2_Mp2_coef(direction) * amplitude;
  }

  T evaluate(float3 direction) const
  {
    return L2_Mn2_coef(direction) * Mn2 + L2_Mn1_coef(direction) * Mn1 +
           L2_M0_coef(direction) * M0 + L2_Mp1_coef(direction) * Mp1 +
           L2_Mp2_coef(direction) * Mp2;
  }

  static BandL2 madd(BandL2 a, float b, BandL2 c)
  {
    BandL2 result;
    result.Mn2 = a.Mn2 * b + c.Mn2;
    result.Mn1 = a.Mn1 * b + c.Mn1;
    result.M0 = a.M0 * b + c.M0;
    result.Mp1 = a.Mp1 * b + c.Mp1;
    result.Mp2 = a.Mp2 * b + c.Mp2;
    return result;
  }

  static BandL2 mul(BandL2 a, float b)
  {
    BandL2 result;
    result.Mn2 = a.Mn2 * b;
    result.Mn1 = a.Mn1 * b;
    result.M0 = a.M0 * b;
    result.Mp1 = a.Mp1 * b;
    result.Mp2 = a.Mp2 * b;
    return result;
  }

  static BandL2 add(BandL2 a, BandL2 b)
  {
    BandL2 result;
    result.Mn2 = a.Mn2 + b.Mn2;
    result.Mn1 = a.Mn1 + b.Mn1;
    result.M0 = a.M0 + b.M0;
    result.Mp1 = a.Mp1 + b.Mp1;
    result.Mp2 = a.Mp2 + b.Mp2;
    return result;
  }
};

template struct BandL0<float>;
template struct BandL0<float4>;
template struct BandL1<float>;
template struct BandL1<float4>;

}  // namespace spherical_harmonics

template<typename T> struct SphericalHarmonicL1 {
  spherical_harmonics::BandL0<T> L0;
  spherical_harmonics::BandL1<T> L1;

  /* Decompose an input signal into spherical harmonic coefficients.
   * Note that `amplitude` need to be scaled by solid angle. */
  void encode_signal_sample(float3 direction, T amplitude)
  {
    L0.encode_signal_sample(direction, amplitude);
    L1.encode_signal_sample(direction, amplitude);
  }

  /* Evaluate an encoded signal in a given unit vector direction. */
  T evaluate(float3 direction) const
  {
    T eval = L0.evaluate(direction) + L1.evaluate(direction);
    return max(T(0.0f), eval);
  }

  /**
   * Convolve a spherical harmonic encoded irradiance signal as a lambertian reflection.
   * Returns the lambertian radiance (cosine lobe divided by PI) so the coefficients simplify to 1,
   * 2/3 and 1/4. See this reference for more explanation:
   * https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
   */
  T evaluate_lambert(float3 N) const
  {
    T radiance = L0.evaluate(N) + L1.evaluate(N) * (2.0f / 3.0f);
    return max(T(0.0f), radiance);
  }

#if 0 /* Not used. */
  float3 evaluate_lambert_non_linear(float3 N) const
  {
    /* Shuffling based on L1_* functions. */
    float3 R1_r = float3(-L1.Mp1.r, -L1.Mn1.r, L1.M0.r);
    float3 R1_g = float3(-L1.Mp1.g, -L1.Mn1.g, L1.M0.g);
    float3 R1_b = float3(-L1.Mp1.b, -L1.Mn1.b, L1.M0.b);

    float3 radiance = float3(evaluate_non_linear(N, L0.M0.r, R1_r),
                             evaluate_non_linear(N, L0.M0.g, R1_g),
                             evaluate_non_linear(N, L0.M0.b, R1_b));
    /* Return lambertian radiance. So divide by PI. */
    return radiance / M_PI;
  }

 private:
  /**
   * Use non-linear reconstruction method to avoid negative lobe artifacts.
   * See this reference for more explanation:
   * https://grahamhazel.com/blog/2017/12/22/converting-sh-radiance-to-irradiance/
   */
  static float evaluate_non_linear(float3 N, float R0, float3 R1)
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
#endif
};

template struct SphericalHarmonicL1<float>;
template struct SphericalHarmonicL1<float4>;

/** \} */

namespace spherical_harmonics {

/* -------------------------------------------------------------------- */
/** \name Load/Store
 *
 * This section define the compression scheme of spherical harmonic data.
 * \{ */

SphericalHarmonicL1<float4> unpack(float4 L0_L1_a,
                                   float4 L0_L1_b,
                                   float4 L0_L1_c,
                                   float4 L0_L1_vis)
{
  SphericalHarmonicL1<float4> sh;
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

void pack(SphericalHarmonicL1<float4> sh,
          float4 &L0_L1_a,
          float4 &L0_L1_b,
          float4 &L0_L1_c,
          float4 &L0_L1_vis)
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

SphericalHarmonicL1<float4> triple_product(SphericalHarmonicL1<float4> a,
                                           SphericalHarmonicL1<float4> b)
{
  /* Adapted from:
   * "Code Generation and Factoring for Fast Evaluation of Low-order Spherical Harmonic Products
   * and Squares" Function "SH_product_3". */
  constexpr float L0_M0_coef = 0.282094792f;
  SphericalHarmonicL1<float4> sh;
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
/** \name Operations
 * \{ */

SphericalHarmonicL1<float4> madd(SphericalHarmonicL1<float4> a,
                                 float b,
                                 SphericalHarmonicL1<float4> c)
{
  SphericalHarmonicL1<float4> result;
  result.L0 = BandL0<float4>::madd(a.L0, b, c.L0);
  result.L1 = BandL1<float4>::madd(a.L1, b, c.L1);
  return result;
}

SphericalHarmonicL1<float4> mul(SphericalHarmonicL1<float4> a, float b)
{
  SphericalHarmonicL1<float4> result;
  result.L0 = BandL0<float4>::mul_scalar(a.L0, b);
  result.L1 = BandL1<float4>::mul_scalar(a.L1, b);
  return result;
}

SphericalHarmonicL1<float4> mul(SphericalHarmonicL1<float4> a, float4 b)
{
  SphericalHarmonicL1<float4> result;
  result.L0 = BandL0<float4>::mul(a.L0, b);
  result.L1 = BandL1<float4>::mul(a.L1, b);
  return result;
}

SphericalHarmonicL1<float4> add(SphericalHarmonicL1<float4> a, SphericalHarmonicL1<float4> b)
{
  SphericalHarmonicL1<float4> result;
  result.L0 = BandL0<float4>::add(a.L0, b.L0);
  result.L1 = BandL1<float4>::add(a.L1, b.L1);
  return result;
}

SphericalHarmonicL1<float4> rotate(float3x3 rotation, SphericalHarmonicL1<float4> sh)
{
  /* Convert L1 coefficients to per channel column.
   * Note the component shuffle to match blender coordinate system. */
  float4x3 per_channel = transpose(float3x4(sh.L1.Mp1, sh.L1.Mn1, -sh.L1.M0));
  /* Rotate each channel. */
  per_channel[0] = rotation * per_channel[0];
  per_channel[1] = rotation * per_channel[1];
  per_channel[2] = rotation * per_channel[2];
  per_channel[3] = rotation * per_channel[3];
  /* Convert back from L1 coefficients to per channel column.
   * Note the component shuffle to match blender coordinate system. */
  float3x4 per_coef = transpose(per_channel);
  sh.L1.Mn1 = per_coef[1];
  sh.L1.M0 = -per_coef[2];
  sh.L1.Mp1 = per_coef[0];
  /* NOTE: L0 band being a constant function (i.e: there is no directionality) there is nothing to
   * rotate. */
  return sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dot
 * \{ */

float4 dot(SphericalHarmonicL1<float4> a, SphericalHarmonicL1<float4> b)
{
  /* Convert coefficients to per channel column. */
  float4x4 a_mat = transpose(float4x4(a.L0.M0, a.L1.Mn1, a.L1.M0, a.L1.Mp1));
  float4x4 b_mat = transpose(float4x4(b.L0.M0, b.L1.Mn1, b.L1.M0, b.L1.Mp1));
  float4 result;
  result[0] = ::dot(a_mat[0], b_mat[0]);
  result[1] = ::dot(a_mat[1], b_mat[1]);
  result[2] = ::dot(a_mat[2], b_mat[2]);
  result[3] = ::dot(a_mat[3], b_mat[3]);
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compression
 *
 * Described by Josh Hobson in "The indirect Lighting Pipeline of God of War" p. 120
 * \{ */

SphericalHarmonicL1<float4> compress(SphericalHarmonicL1<float4> sh)
{
  SphericalHarmonicL1<float4> result;
  result.L0 = sh.L0;
  float4 fac = safe_rcp(sh.L0.M0 * M_SQRT3);
  result.L1.Mn1 = (sh.L1.Mn1 * fac) * 0.5f + 0.5f;
  result.L1.M0 = (sh.L1.M0 * fac) * 0.5f + 0.5f;
  result.L1.Mp1 = (sh.L1.Mp1 * fac) * 0.5f + 0.5f;
  return result;
}

SphericalHarmonicL1<float4> decompress(SphericalHarmonicL1<float4> sh)
{
  SphericalHarmonicL1<float4> result;
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

SphericalHarmonicL1<float4> dering(SphericalHarmonicL1<float4> sh)
{
  float L0_weight = 0.282094792f;
  float L1_weight = 0.488602512f;
  /* Multiply by lambert convolution weight (see #evaluate_lambert). */
  L1_weight *= (2.0f / 3.0f);
  /* Add some bias to avoid too much unrealistic contrast. */
  L1_weight += 0.05f;

  /* Convert coefficients to per channel column. */
  const float4x4 m = transpose(float4x4(max(abs(sh.L0.M0) * L0_weight, float4(1e-8f)),
                                        abs(sh.L1.Mn1),
                                        abs(sh.L1.M0),
                                        abs(sh.L1.Mp1)));

  /* Multiply by each band's weight. */
  /* Find maximum value the L1 band can contain that doesn't exhibit ringing artifacts. */
  float fac_r = m[0].x / max(1e-8f, length(m[0].yzw) * L1_weight);
  float fac_g = m[1].x / max(1e-8f, length(m[1].yzw) * L1_weight);
  float fac_b = m[2].x / max(1e-8f, length(m[2].yzw) * L1_weight);
  /* Choose the factor for the component with higher directionality and apply it on all component.
   * Otherwise this can result in color drifting. */
  float fac = reduce_min(float3(fac_r, fac_g, fac_b));
  if (fac < 1.0f) {
    sh.L1 = BandL1<float4>::mul(sh.L1, float4(fac));
  }
  return sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clamping
 *
 * Clamp the total power of the SH function.
 * \{ */

SphericalHarmonicL1<float4> clamp_energy(SphericalHarmonicL1<float4> sh, float clamp_value)
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
  if (fac < 1.0f) {
    sh = spherical_harmonics::mul(sh, fac);
  }
  return sh;
}

/** \} */

}  // namespace spherical_harmonics
