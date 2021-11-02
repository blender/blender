/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "kernel/svm/noise.h"

CCL_NAMESPACE_BEGIN

/* The fractal_noise_[1-4] functions are all exactly the same except for the input type. */
ccl_device_noinline float fractal_noise_1d(float p, float octaves, float roughness)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;
  octaves = clamp(octaves, 0.0f, 15.0f);
  int n = float_to_int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = noise_1d(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0f, 1.0f);
    fscale *= 2.0f;
  }
  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float t = noise_1d(fscale * p);
    float sum2 = sum + t * amp;
    sum /= maxamp;
    sum2 /= maxamp + amp;
    return (1.0f - rmd) * sum + rmd * sum2;
  }
  else {
    return sum / maxamp;
  }
}

/* The fractal_noise_[1-4] functions are all exactly the same except for the input type. */
ccl_device_noinline float fractal_noise_2d(float2 p, float octaves, float roughness)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;
  octaves = clamp(octaves, 0.0f, 15.0f);
  int n = float_to_int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = noise_2d(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0f, 1.0f);
    fscale *= 2.0f;
  }
  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float t = noise_2d(fscale * p);
    float sum2 = sum + t * amp;
    sum /= maxamp;
    sum2 /= maxamp + amp;
    return (1.0f - rmd) * sum + rmd * sum2;
  }
  else {
    return sum / maxamp;
  }
}

/* The fractal_noise_[1-4] functions are all exactly the same except for the input type. */
ccl_device_noinline float fractal_noise_3d(float3 p, float octaves, float roughness)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;
  octaves = clamp(octaves, 0.0f, 15.0f);
  int n = float_to_int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = noise_3d(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0f, 1.0f);
    fscale *= 2.0f;
  }
  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float t = noise_3d(fscale * p);
    float sum2 = sum + t * amp;
    sum /= maxamp;
    sum2 /= maxamp + amp;
    return (1.0f - rmd) * sum + rmd * sum2;
  }
  else {
    return sum / maxamp;
  }
}

/* The fractal_noise_[1-4] functions are all exactly the same except for the input type. */
ccl_device_noinline float fractal_noise_4d(float4 p, float octaves, float roughness)
{
  float fscale = 1.0f;
  float amp = 1.0f;
  float maxamp = 0.0f;
  float sum = 0.0f;
  octaves = clamp(octaves, 0.0f, 15.0f);
  int n = float_to_int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = noise_4d(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0f, 1.0f);
    fscale *= 2.0f;
  }
  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float t = noise_4d(fscale * p);
    float sum2 = sum + t * amp;
    sum /= maxamp;
    sum2 /= maxamp + amp;
    return (1.0f - rmd) * sum + rmd * sum2;
  }
  else {
    return sum / maxamp;
  }
}

CCL_NAMESPACE_END
