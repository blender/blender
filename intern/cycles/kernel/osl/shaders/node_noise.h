/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "vector2.h"
#include "vector4.h"

#define vector3 point

float safe_noise(float co)
{
  float precision_correction = 0.5 * float(fabs(co) >= 1000000.0);
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. */
  float p = fmod(co, 100000.0) + precision_correction;

  return noise("noise", p);
}

float safe_noise(vector2 co)
{
  vector2 precision_correction = 0.5 * vector2(float(fabs(co.x) >= 1000000.0),
                                               float(fabs(co.y) >= 1000000.0));
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0, however at such scales this
   * usually shouldn't be noticeable. */
  vector2 p = fmod(co, 100000.0) + precision_correction;

  return noise("noise", p.x, p.y);
}

float safe_noise(vector3 co)
{
  vector3 precision_correction = 0.5 * vector3(float(fabs(co.x) >= 1000000.0),
                                               float(fabs(co.y) >= 1000000.0),
                                               float(fabs(co.z) >= 1000000.0));
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0, however at such scales this
   * usually shouldn't be noticeable. */
  vector3 p = fmod(co, 100000.0) + precision_correction;

  return noise("noise", p);
}

float safe_noise(vector4 co)
{
  vector4 precision_correction = 0.5 * vector4(float(fabs(co.x) >= 1000000.0),
                                               float(fabs(co.y) >= 1000000.0),
                                               float(fabs(co.z) >= 1000000.0),
                                               float(fabs(co.w) >= 1000000.0));
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0, however at such scales this
   * usually shouldn't be noticeable. */
  vector4 p = fmod(co, 100000.0) + precision_correction;

  return noise("noise", vector3(p.x, p.y, p.z), p.w);
}

float safe_snoise(float co)
{
  float precision_correction = 0.5 * float(fabs(co) >= 1000000.0);
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. */
  float p = fmod(co, 100000.0) + precision_correction;

  return noise("snoise", p);
}

float safe_snoise(vector2 co)
{
  vector2 precision_correction = 0.5 * vector2(float(fabs(co.x) >= 1000000.0),
                                               float(fabs(co.y) >= 1000000.0));
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0, however at such scales this
   * usually shouldn't be noticeable. */
  vector2 p = fmod(co, 100000.0) + precision_correction;

  return noise("snoise", p.x, p.y);
}

float safe_snoise(vector3 co)
{
  vector3 precision_correction = 0.5 * vector3(float(fabs(co.x) >= 1000000.0),
                                               float(fabs(co.y) >= 1000000.0),
                                               float(fabs(co.z) >= 1000000.0));
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0, however at such scales this
   * usually shouldn't be noticeable. */
  vector3 p = fmod(co, 100000.0) + precision_correction;

  return noise("snoise", p);
}

float safe_snoise(vector4 co)
{
  vector4 precision_correction = 0.5 * vector4(float(fabs(co.x) >= 1000000.0),
                                               float(fabs(co.y) >= 1000000.0),
                                               float(fabs(co.z) >= 1000000.0),
                                               float(fabs(co.w) >= 1000000.0));
  /* Repeat Perlin noise texture every 100000.0 on each axis to prevent floating point
   * representation issues. This causes discontinuities every 100000.0, however at such scales this
   * usually shouldn't be noticeable. */
  vector4 p = fmod(co, 100000.0) + precision_correction;

  return noise("snoise", vector3(p.x, p.y, p.z), p.w);
}

#define NOISE_FBM(T) \
  float noise_fbm(T co, float detail, float roughness, float lacunarity, int use_normalize) \
  { \
    T p = co; \
    float fscale = 1.0; \
    float amp = 1.0; \
    float maxamp = 0.0; \
    float sum = 0.0; \
\
    for (int i = 0; i <= int(detail); i++) { \
      float t = safe_snoise(fscale * p); \
      sum += t * amp; \
      maxamp += amp; \
      amp *= roughness; \
      fscale *= lacunarity; \
    } \
    float rmd = detail - floor(detail); \
    if (rmd != 0.0) { \
      float t = safe_snoise(fscale * p); \
      float sum2 = sum + t * amp; \
      return use_normalize ? \
                 mix(0.5 * sum / maxamp + 0.5, 0.5 * sum2 / (maxamp + amp) + 0.5, rmd) : \
                 mix(sum, sum2, rmd); \
    } \
    else { \
      return use_normalize ? 0.5 * sum / maxamp + 0.5 : sum; \
    } \
  }

#define NOISE_MULTI_FRACTAL(T) \
  float noise_multi_fractal(T co, float detail, float roughness, float lacunarity) \
  { \
    T p = co; \
    float value = 1.0; \
    float pwr = 1.0; \
\
    for (int i = 0; i <= (int)detail; i++) { \
      value *= (pwr * safe_snoise(p) + 1.0); \
      pwr *= roughness; \
      p *= lacunarity; \
    } \
\
    float rmd = detail - floor(detail); \
    if (rmd != 0.0) { \
      value *= (rmd * pwr * safe_snoise(p) + 1.0); /* correct? */ \
    } \
\
    return value; \
  }

#define NOISE_HETERO_TERRAIN(T) \
  float noise_hetero_terrain(T co, float detail, float roughness, float lacunarity, float offset) \
  { \
    T p = co; \
    float pwr = roughness; \
\
    /* first unscaled octave of function; later octaves are scaled */ \
    float value = offset + safe_snoise(p); \
    p *= lacunarity; \
\
    for (int i = 1; i <= (int)detail; i++) { \
      float increment = (safe_snoise(p) + offset) * pwr * value; \
      value += increment; \
      pwr *= roughness; \
      p *= lacunarity; \
    } \
\
    float rmd = detail - floor(detail); \
    if (rmd != 0.0) { \
      float increment = (safe_snoise(p) + offset) * pwr * value; \
      value += rmd * increment; \
    } \
\
    return value; \
  }

#define NOISE_HYBRID_MULTI_FRACTAL(T) \
  float noise_hybrid_multi_fractal( \
      T co, float detail, float roughness, float lacunarity, float offset, float gain) \
  { \
    T p = co; \
    float pwr = 1.0; \
    float value = 0.0; \
    float weight = 1.0; \
\
    for (int i = 0; (weight > 0.001) && (i <= (int)detail); i++) { \
      if (weight > 1.0) { \
        weight = 1.0; \
      } \
\
      float signal = (safe_snoise(p) + offset) * pwr; \
      pwr *= roughness; \
      value += weight * signal; \
      weight *= gain * signal; \
      p *= lacunarity; \
    } \
\
    float rmd = detail - floor(detail); \
    if ((rmd != 0.0) && (weight > 0.001)) { \
      if (weight > 1.0) { \
        weight = 1.0; \
      } \
      float signal = (safe_snoise(p) + offset) * pwr; \
      value += rmd * weight * signal; \
    } \
\
    return value; \
  }

#define NOISE_RIDGED_MULTI_FRACTAL(T) \
  float noise_ridged_multi_fractal( \
      T co, float detail, float roughness, float lacunarity, float offset, float gain) \
  { \
    T p = co; \
    float pwr = roughness; \
\
    float signal = offset - fabs(safe_snoise(p)); \
    signal *= signal; \
    float value = signal; \
    float weight = 1.0; \
\
    for (int i = 1; i <= (int)detail; i++) { \
      p *= lacunarity; \
      weight = clamp(signal * gain, 0.0, 1.0); \
      signal = offset - fabs(safe_snoise(p)); \
      signal *= signal; \
      signal *= weight; \
      value += signal * pwr; \
      pwr *= roughness; \
    } \
\
    return value; \
  }

/* Noise fBM. */

NOISE_FBM(float)
NOISE_FBM(vector2)
NOISE_FBM(vector3)
NOISE_FBM(vector4)

/* Noise Multi-fractal. */

NOISE_MULTI_FRACTAL(float)
NOISE_MULTI_FRACTAL(vector2)
NOISE_MULTI_FRACTAL(vector3)
NOISE_MULTI_FRACTAL(vector4)

/* Noise Hetero Terrain. */

NOISE_HETERO_TERRAIN(float)
NOISE_HETERO_TERRAIN(vector2)
NOISE_HETERO_TERRAIN(vector3)
NOISE_HETERO_TERRAIN(vector4)

/* Noise Hybrid Multi-fractal. */

NOISE_HYBRID_MULTI_FRACTAL(float)
NOISE_HYBRID_MULTI_FRACTAL(vector2)
NOISE_HYBRID_MULTI_FRACTAL(vector3)
NOISE_HYBRID_MULTI_FRACTAL(vector4)

/* Noise Ridged Multi-fractal. */

NOISE_RIDGED_MULTI_FRACTAL(float)
NOISE_RIDGED_MULTI_FRACTAL(vector2)
NOISE_RIDGED_MULTI_FRACTAL(vector3)
NOISE_RIDGED_MULTI_FRACTAL(vector4)

#undef vector3
