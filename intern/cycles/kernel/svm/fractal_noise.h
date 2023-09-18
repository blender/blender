/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/noise.h"

CCL_NAMESPACE_BEGIN

/* The fractal_noise_[1-4] functions are all exactly the same except for the input type. */
ccl_device_noinline float fractal_noise_1d(
    float p, float octaves, float roughness, float lacunarity, bool normalize)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;
  octaves = clamp(octaves, 0.0f, 15.0f);
  int n = float_to_int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = snoise_1d(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0f, 1.0f);
    fscale *= lacunarity;
  }
  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float t = snoise_1d(fscale * p);
    float sum2 = sum + t * amp;
    return normalize ? mix(0.5f * sum / maxamp + 0.5f, 0.5f * sum2 / (maxamp + amp) + 0.5f, rmd) :
                       mix(sum, sum2, rmd);
  }
  else {
    return normalize ? 0.5f * sum / maxamp + 0.5f : sum;
  }
}

/* The fractal_noise_[1-4] functions are all exactly the same except for the input type. */
ccl_device_noinline float fractal_noise_2d(
    float2 p, float octaves, float roughness, float lacunarity, bool normalize)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;
  octaves = clamp(octaves, 0.0f, 15.0f);
  int n = float_to_int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = snoise_2d(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0f, 1.0f);
    fscale *= lacunarity;
  }
  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float t = snoise_2d(fscale * p);
    float sum2 = sum + t * amp;
    return normalize ? mix(0.5f * sum / maxamp + 0.5f, 0.5f * sum2 / (maxamp + amp) + 0.5f, rmd) :
                       mix(sum, sum2, rmd);
  }
  else {
    return normalize ? 0.5f * sum / maxamp + 0.5f : sum;
  }
}

/* The fractal_noise_[1-4] functions are all exactly the same except for the input type. */
ccl_device_noinline float fractal_noise_3d(
    float3 p, float octaves, float roughness, float lacunarity, bool normalize)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;
  octaves = clamp(octaves, 0.0f, 15.0f);
  int n = float_to_int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = snoise_3d(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0f, 1.0f);
    fscale *= lacunarity;
  }
  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float t = snoise_3d(fscale * p);
    float sum2 = sum + t * amp;
    return normalize ? mix(0.5f * sum / maxamp + 0.5f, 0.5f * sum2 / (maxamp + amp) + 0.5f, rmd) :
                       mix(sum, sum2, rmd);
  }
  else {
    return normalize ? 0.5f * sum / maxamp + 0.5f : sum;
  }
}

/* The fractal_noise_[1-4] functions are all exactly the same except for the input type. */
ccl_device_noinline float fractal_noise_4d(
    float4 p, float octaves, float roughness, float lacunarity, bool normalize)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;
  octaves = clamp(octaves, 0.0f, 15.0f);
  int n = float_to_int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = snoise_4d(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0f, 1.0f);
    fscale *= lacunarity;
  }
  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float t = snoise_4d(fscale * p);
    float sum2 = sum + t * amp;
    return normalize ? mix(0.5f * sum / maxamp + 0.5f, 0.5f * sum2 / (maxamp + amp) + 0.5f, rmd) :
                       mix(sum, sum2, rmd);
  }
  else {
    return normalize ? 0.5f * sum / maxamp + 0.5f : sum;
  }
}

CCL_NAMESPACE_END
