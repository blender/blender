/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/noise.h"

CCL_NAMESPACE_BEGIN

/* Fractal Brownian motion. */

ccl_device_noinline float noise_fbm(
    float p, const float detail, const float roughness, const float lacunarity, bool normalize)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;

  for (int i = 0; i <= float_to_int(detail); i++) {
    const float t = snoise_1d(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= roughness;
    fscale *= lacunarity;
  }
  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    const float t = snoise_1d(fscale * p);
    const float sum2 = sum + t * amp;
    return normalize ? mix(0.5f * sum / maxamp + 0.5f, 0.5f * sum2 / (maxamp + amp) + 0.5f, rmd) :
                       mix(sum, sum2, rmd);
  }
  return normalize ? 0.5f * sum / maxamp + 0.5f : sum;
}

ccl_device_noinline float noise_fbm(
    float2 p, const float detail, const float roughness, const float lacunarity, bool normalize)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;

  for (int i = 0; i <= float_to_int(detail); i++) {
    const float t = snoise_2d(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= roughness;
    fscale *= lacunarity;
  }
  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    const float t = snoise_2d(fscale * p);
    const float sum2 = sum + t * amp;
    return normalize ? mix(0.5f * sum / maxamp + 0.5f, 0.5f * sum2 / (maxamp + amp) + 0.5f, rmd) :
                       mix(sum, sum2, rmd);
  }
  return normalize ? 0.5f * sum / maxamp + 0.5f : sum;
}

ccl_device_noinline float noise_fbm(
    float3 p, const float detail, const float roughness, const float lacunarity, bool normalize)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;

  for (int i = 0; i <= float_to_int(detail); i++) {
    const float t = snoise_3d(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= roughness;
    fscale *= lacunarity;
  }
  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    const float t = snoise_3d(fscale * p);
    const float sum2 = sum + t * amp;
    return normalize ? mix(0.5f * sum / maxamp + 0.5f, 0.5f * sum2 / (maxamp + amp) + 0.5f, rmd) :
                       mix(sum, sum2, rmd);
  }
  return normalize ? 0.5f * sum / maxamp + 0.5f : sum;
}

ccl_device_noinline float noise_fbm(
    float4 p, const float detail, const float roughness, const float lacunarity, bool normalize)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;

  for (int i = 0; i <= float_to_int(detail); i++) {
    const float t = snoise_4d(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= roughness;
    fscale *= lacunarity;
  }
  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    const float t = snoise_4d(fscale * p);
    const float sum2 = sum + t * amp;
    return normalize ? mix(0.5f * sum / maxamp + 0.5f, 0.5f * sum2 / (maxamp + amp) + 0.5f, rmd) :
                       mix(sum, sum2, rmd);
  }
  return normalize ? 0.5f * sum / maxamp + 0.5f : sum;
}

/* Multifractal */

ccl_device_noinline float noise_multi_fractal(float p,
                                              const float detail,
                                              const float roughness,
                                              const float lacunarity)
{
  float value = 1.0f;
  float pwr = 1.0f;

  for (int i = 0; i <= float_to_int(detail); i++) {
    value *= (pwr * snoise_1d(p) + 1.0f);
    pwr *= roughness;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * snoise_1d(p) + 1.0f); /* correct? */
  }

  return value;
}

ccl_device_noinline float noise_multi_fractal(float2 p,
                                              const float detail,
                                              const float roughness,
                                              const float lacunarity)
{
  float value = 1.0f;
  float pwr = 1.0f;

  for (int i = 0; i <= float_to_int(detail); i++) {
    value *= (pwr * snoise_2d(p) + 1.0f);
    pwr *= roughness;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * snoise_2d(p) + 1.0f); /* correct? */
  }

  return value;
}

ccl_device_noinline float noise_multi_fractal(float3 p,
                                              const float detail,
                                              const float roughness,
                                              const float lacunarity)
{
  float value = 1.0f;
  float pwr = 1.0f;

  for (int i = 0; i <= float_to_int(detail); i++) {
    value *= (pwr * snoise_3d(p) + 1.0f);
    pwr *= roughness;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * snoise_3d(p) + 1.0f); /* correct? */
  }

  return value;
}

ccl_device_noinline float noise_multi_fractal(float4 p,
                                              const float detail,
                                              const float roughness,
                                              const float lacunarity)
{
  float value = 1.0f;
  float pwr = 1.0f;

  for (int i = 0; i <= float_to_int(detail); i++) {
    value *= (pwr * snoise_4d(p) + 1.0f);
    pwr *= roughness;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    value *= (rmd * pwr * snoise_4d(p) + 1.0f); /* correct? */
  }

  return value;
}

/* Heterogeneous Terrain */

ccl_device_noinline float noise_hetero_terrain(
    float p, const float detail, const float roughness, const float lacunarity, const float offset)
{
  float pwr = roughness;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + snoise_1d(p);
  p *= lacunarity;

  for (int i = 1; i <= float_to_int(detail); i++) {
    const float increment = (snoise_1d(p) + offset) * pwr * value;
    value += increment;
    pwr *= roughness;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    const float increment = (snoise_1d(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

ccl_device_noinline float noise_hetero_terrain(float2 p,
                                               const float detail,
                                               const float roughness,
                                               const float lacunarity,
                                               const float offset)
{
  float pwr = roughness;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + snoise_2d(p);
  p *= lacunarity;

  for (int i = 1; i <= float_to_int(detail); i++) {
    const float increment = (snoise_2d(p) + offset) * pwr * value;
    value += increment;
    pwr *= roughness;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    const float increment = (snoise_2d(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

ccl_device_noinline float noise_hetero_terrain(float3 p,
                                               const float detail,
                                               const float roughness,
                                               const float lacunarity,
                                               const float offset)
{
  float pwr = roughness;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + snoise_3d(p);
  p *= lacunarity;

  for (int i = 1; i <= float_to_int(detail); i++) {
    const float increment = (snoise_3d(p) + offset) * pwr * value;
    value += increment;
    pwr *= roughness;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    const float increment = (snoise_3d(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

ccl_device_noinline float noise_hetero_terrain(float4 p,
                                               const float detail,
                                               const float roughness,
                                               const float lacunarity,
                                               const float offset)
{
  float pwr = roughness;

  /* first unscaled octave of function; later octaves are scaled */
  float value = offset + snoise_4d(p);
  p *= lacunarity;

  for (int i = 1; i <= float_to_int(detail); i++) {
    const float increment = (snoise_4d(p) + offset) * pwr * value;
    value += increment;
    pwr *= roughness;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if (rmd != 0.0f) {
    const float increment = (snoise_4d(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

/* Hybrid Additive/Multiplicative Multifractal Terrain */

ccl_device_noinline float noise_hybrid_multi_fractal(float p,
                                                     const float detail,
                                                     const float roughness,
                                                     const float lacunarity,
                                                     const float offset,
                                                     const float gain)
{
  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  for (int i = 0; (weight > 0.001f) && (i <= float_to_int(detail)); i++) {
    weight = fminf(weight, 1.0f);

    const float signal = (snoise_1d(p) + offset) * pwr;
    pwr *= roughness;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    weight = fminf(weight, 1.0f);
    const float signal = (snoise_1d(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

ccl_device_noinline float noise_hybrid_multi_fractal(float2 p,
                                                     const float detail,
                                                     const float roughness,
                                                     const float lacunarity,
                                                     const float offset,
                                                     const float gain)
{
  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  for (int i = 0; (weight > 0.001f) && (i <= float_to_int(detail)); i++) {
    weight = fminf(weight, 1.0f);

    const float signal = (snoise_2d(p) + offset) * pwr;
    pwr *= roughness;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    weight = fminf(weight, 1.0f);
    const float signal = (snoise_2d(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

ccl_device_noinline float noise_hybrid_multi_fractal(float3 p,
                                                     const float detail,
                                                     const float roughness,
                                                     const float lacunarity,
                                                     const float offset,
                                                     const float gain)
{
  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  for (int i = 0; (weight > 0.001f) && (i <= float_to_int(detail)); i++) {
    weight = fminf(weight, 1.0f);

    const float signal = (snoise_3d(p) + offset) * pwr;
    pwr *= roughness;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    weight = fminf(weight, 1.0f);
    const float signal = (snoise_3d(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

ccl_device_noinline float noise_hybrid_multi_fractal(float4 p,
                                                     const float detail,
                                                     const float roughness,
                                                     const float lacunarity,
                                                     const float offset,
                                                     const float gain)
{
  float pwr = 1.0f;
  float value = 0.0f;
  float weight = 1.0f;

  for (int i = 0; (weight > 0.001f) && (i <= float_to_int(detail)); i++) {
    weight = fminf(weight, 1.0f);

    const float signal = (snoise_4d(p) + offset) * pwr;
    pwr *= roughness;
    value += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  const float rmd = detail - floorf(detail);
  if ((rmd != 0.0f) && (weight > 0.001f)) {
    weight = fminf(weight, 1.0f);
    const float signal = (snoise_4d(p) + offset) * pwr;
    value += rmd * weight * signal;
  }

  return value;
}

/* Ridged Multifractal Terrain */

ccl_device_noinline float noise_ridged_multi_fractal(float p,
                                                     const float detail,
                                                     const float roughness,
                                                     const float lacunarity,
                                                     const float offset,
                                                     const float gain)
{
  float pwr = roughness;

  float signal = offset - fabsf(snoise_1d(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  for (int i = 1; i <= float_to_int(detail); i++) {
    p *= lacunarity;
    weight = saturatef(signal * gain);
    signal = offset - fabsf(snoise_1d(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= roughness;
  }

  return value;
}

ccl_device_noinline float noise_ridged_multi_fractal(float2 p,
                                                     const float detail,
                                                     const float roughness,
                                                     const float lacunarity,
                                                     const float offset,
                                                     const float gain)
{
  float pwr = roughness;

  float signal = offset - fabsf(snoise_2d(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  for (int i = 1; i <= float_to_int(detail); i++) {
    p *= lacunarity;
    weight = saturatef(signal * gain);
    signal = offset - fabsf(snoise_2d(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= roughness;
  }

  return value;
}

ccl_device_noinline float noise_ridged_multi_fractal(float3 p,
                                                     const float detail,
                                                     const float roughness,
                                                     const float lacunarity,
                                                     const float offset,
                                                     const float gain)
{
  float pwr = roughness;

  float signal = offset - fabsf(snoise_3d(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  for (int i = 1; i <= float_to_int(detail); i++) {
    p *= lacunarity;
    weight = saturatef(signal * gain);
    signal = offset - fabsf(snoise_3d(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= roughness;
  }

  return value;
}

ccl_device_noinline float noise_ridged_multi_fractal(float4 p,
                                                     const float detail,
                                                     const float roughness,
                                                     const float lacunarity,
                                                     const float offset,
                                                     const float gain)
{
  float pwr = roughness;

  float signal = offset - fabsf(snoise_4d(p));
  signal *= signal;
  float value = signal;
  float weight = 1.0f;

  for (int i = 1; i <= float_to_int(detail); i++) {
    p *= lacunarity;
    weight = saturatef(signal * gain);
    signal = offset - fabsf(snoise_4d(p));
    signal *= signal;
    signal *= weight;
    value += signal * pwr;
    pwr *= roughness;
  }

  return value;
}

CCL_NAMESPACE_END
