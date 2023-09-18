/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "vector2.h"
#include "vector4.h"

#define vector3 point

float safe_noise(float p)
{
  float f = noise("noise", p);
  if (isinf(f))
    return 0.5;
  return f;
}

float safe_noise(vector2 p)
{
  float f = noise("noise", p.x, p.y);
  if (isinf(f))
    return 0.5;
  return f;
}

float safe_noise(vector3 p)
{
  float f = noise("noise", p);
  if (isinf(f))
    return 0.5;
  return f;
}

float safe_noise(vector4 p)
{
  float f = noise("noise", vector3(p.x, p.y, p.z), p.w);
  if (isinf(f))
    return 0.5;
  return f;
}

float safe_snoise(float p)
{
  float f = noise("snoise", p);
  if (isinf(f))
    return 0.0;
  return f;
}

float safe_snoise(vector2 p)
{
  float f = noise("snoise", p.x, p.y);
  if (isinf(f))
    return 0.0;
  return f;
}

float safe_snoise(vector3 p)
{
  float f = noise("snoise", p);
  if (isinf(f))
    return 0.0;
  return f;
}

float safe_snoise(vector4 p)
{
  float f = noise("snoise", vector3(p.x, p.y, p.z), p.w);
  if (isinf(f))
    return 0.0;
  return f;
}

/* The fractal_noise functions are all exactly the same except for the input type. */
float fractal_noise(float p, float details, float roughness, float lacunarity, int use_normalize)
{
  float fscale = 1.0;
  float amp = 1.0;
  float maxamp = 0.0;
  float sum = 0.0;
  float octaves = clamp(details, 0.0, 15.0);
  int n = (int)octaves;
  for (int i = 0; i <= n; i++) {
    float t = safe_snoise(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0, 1.0);
    fscale *= lacunarity;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = safe_snoise(fscale * p);
    float sum2 = sum + t * amp;
    return use_normalize ? mix(0.5 * sum / maxamp + 0.5, 0.5 * sum2 / (maxamp + amp) + 0.5, rmd) :
                           mix(sum, sum2, rmd);
  }
  else {
    return use_normalize ? 0.5 * sum / maxamp + 0.5 : sum;
  }
}

/* The fractal_noise functions are all exactly the same except for the input type. */
float fractal_noise(vector2 p, float details, float roughness, float lacunarity, int use_normalize)
{
  float fscale = 1.0;
  float amp = 1.0;
  float maxamp = 0.0;
  float sum = 0.0;
  float octaves = clamp(details, 0.0, 15.0);
  int n = (int)octaves;
  for (int i = 0; i <= n; i++) {
    float t = safe_snoise(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0, 1.0);
    fscale *= lacunarity;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = safe_snoise(fscale * p);
    float sum2 = sum + t * amp;
    return use_normalize ? mix(0.5 * sum / maxamp + 0.5, 0.5 * sum2 / (maxamp + amp) + 0.5, rmd) :
                           mix(sum, sum2, rmd);
  }
  else {
    return use_normalize ? 0.5 * sum / maxamp + 0.5 : sum;
  }
}

/* The fractal_noise functions are all exactly the same except for the input type. */
float fractal_noise(vector3 p, float details, float roughness, float lacunarity, int use_normalize)
{
  float fscale = 1.0;
  float amp = 1.0;
  float maxamp = 0.0;
  float sum = 0.0;
  float octaves = clamp(details, 0.0, 15.0);
  int n = (int)octaves;
  for (int i = 0; i <= n; i++) {
    float t = safe_snoise(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0, 1.0);
    fscale *= lacunarity;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = safe_snoise(fscale * p);
    float sum2 = sum + t * amp;
    return use_normalize ? mix(0.5 * sum / maxamp + 0.5, 0.5 * sum2 / (maxamp + amp) + 0.5, rmd) :
                           mix(sum, sum2, rmd);
  }
  else {
    return use_normalize ? 0.5 * sum / maxamp + 0.5 : sum;
  }
}

/* The fractal_noise functions are all exactly the same except for the input type. */
float fractal_noise(vector4 p, float details, float roughness, float lacunarity, int use_normalize)
{
  float fscale = 1.0;
  float amp = 1.0;
  float maxamp = 0.0;
  float sum = 0.0;
  float octaves = clamp(details, 0.0, 15.0);
  int n = (int)octaves;
  for (int i = 0; i <= n; i++) {
    float t = safe_snoise(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0, 1.0);
    fscale *= lacunarity;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = safe_snoise(fscale * p);
    float sum2 = sum + t * amp;
    return use_normalize ? mix(0.5 * sum / maxamp + 0.5, 0.5 * sum2 / (maxamp + amp) + 0.5, rmd) :
                           mix(sum, sum2, rmd);
  }
  else {
    return use_normalize ? 0.5 * sum / maxamp + 0.5 : sum;
  }
}

#undef vector3
