/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_hash.glsl"
#include "gpu_shader_material_noise.glsl"

#define NOISE_FBM(T) \
  float noise_fbm(T co, \
                  float detail, \
                  float roughness, \
                  float lacunarity, \
                  float offset, \
                  float gain, \
                  bool normalize) \
  { \
    T p = co; \
    float fscale = 1.0f; \
    float amp = 1.0f; \
    float maxamp = 0.0f; \
    float sum = 0.0f; \
\
    for (int i = 0; i <= int(detail); i++) { \
      float t = snoise(fscale * p); \
      sum += t * amp; \
      maxamp += amp; \
      amp *= roughness; \
      fscale *= lacunarity; \
    } \
    float rmd = detail - floor(detail); \
    if (rmd != 0.0f) { \
      float t = snoise(fscale * p); \
      float sum2 = sum + t * amp; \
      return normalize ? \
                 mix(0.5f * sum / maxamp + 0.5f, 0.5f * sum2 / (maxamp + amp) + 0.5f, rmd) : \
                 mix(sum, sum2, rmd); \
    } \
    else { \
      return normalize ? 0.5f * sum / maxamp + 0.5f : sum; \
    } \
  }

#define NOISE_MULTI_FRACTAL(T) \
  float noise_multi_fractal(T co, \
                            float detail, \
                            float roughness, \
                            float lacunarity, \
                            float offset, \
                            float gain, \
                            bool normalize) \
  { \
    T p = co; \
    float value = 1.0f; \
    float pwr = 1.0f; \
\
    for (int i = 0; i <= int(detail); i++) { \
      value *= (pwr * snoise(p) + 1.0f); \
      pwr *= roughness; \
      p *= lacunarity; \
    } \
\
    float rmd = detail - floor(detail); \
    if (rmd != 0.0f) { \
      value *= (rmd * pwr * snoise(p) + 1.0f); /* correct? */ \
    } \
\
    return value; \
  }

#define NOISE_HETERO_TERRAIN(T) \
  float noise_hetero_terrain(T co, \
                             float detail, \
                             float roughness, \
                             float lacunarity, \
                             float offset, \
                             float gain, \
                             bool normalize) \
  { \
    T p = co; \
    float pwr = roughness; \
\
    /* first unscaled octave of function; later octaves are scaled */ \
    float value = offset + snoise(p); \
    p *= lacunarity; \
\
    for (int i = 1; i <= int(detail); i++) { \
      float increment = (snoise(p) + offset) * pwr * value; \
      value += increment; \
      pwr *= roughness; \
      p *= lacunarity; \
    } \
\
    float rmd = detail - floor(detail); \
    if (rmd != 0.0f) { \
      float increment = (snoise(p) + offset) * pwr * value; \
      value += rmd * increment; \
    } \
\
    return value; \
  }

#define NOISE_HYBRID_MULTI_FRACTAL(T) \
  float noise_hybrid_multi_fractal(T co, \
                                   float detail, \
                                   float roughness, \
                                   float lacunarity, \
                                   float offset, \
                                   float gain, \
                                   bool normalize) \
  { \
    T p = co; \
    float pwr = 1.0f; \
    float value = 0.0f; \
    float weight = 1.0f; \
\
    for (int i = 0; (weight > 0.001f) && (i <= int(detail)); i++) { \
      if (weight > 1.0f) { \
        weight = 1.0f; \
      } \
\
      float signal = (snoise(p) + offset) * pwr; \
      pwr *= roughness; \
      value += weight * signal; \
      weight *= gain * signal; \
      p *= lacunarity; \
    } \
\
    float rmd = detail - floor(detail); \
    if ((rmd != 0.0f) && (weight > 0.001f)) { \
      if (weight > 1.0f) { \
        weight = 1.0f; \
      } \
      float signal = (snoise(p) + offset) * pwr; \
      value += rmd * weight * signal; \
    } \
\
    return value; \
  }

#define NOISE_RIDGED_MULTI_FRACTAL(T) \
  float noise_ridged_multi_fractal(T co, \
                                   float detail, \
                                   float roughness, \
                                   float lacunarity, \
                                   float offset, \
                                   float gain, \
                                   bool normalize) \
  { \
    T p = co; \
    float pwr = roughness; \
\
    float signal = offset - abs(snoise(p)); \
    signal *= signal; \
    float value = signal; \
    float weight = 1.0f; \
\
    for (int i = 1; i <= int(detail); i++) { \
      p *= lacunarity; \
      weight = clamp(signal * gain, 0.0f, 1.0f); \
      signal = offset - abs(snoise(p)); \
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
NOISE_FBM(float2)
NOISE_FBM(float3)
NOISE_FBM(float4)

/* Noise Multi-fractal. */

NOISE_MULTI_FRACTAL(float)
NOISE_MULTI_FRACTAL(float2)
NOISE_MULTI_FRACTAL(float3)
NOISE_MULTI_FRACTAL(float4)

/* Noise Hetero Terrain. */

NOISE_HETERO_TERRAIN(float)
NOISE_HETERO_TERRAIN(float2)
NOISE_HETERO_TERRAIN(float3)
NOISE_HETERO_TERRAIN(float4)

/* Noise Hybrid Multi-fractal. */

NOISE_HYBRID_MULTI_FRACTAL(float)
NOISE_HYBRID_MULTI_FRACTAL(float2)
NOISE_HYBRID_MULTI_FRACTAL(float3)
NOISE_HYBRID_MULTI_FRACTAL(float4)

/* Noise Ridged Multi-fractal. */

NOISE_RIDGED_MULTI_FRACTAL(float)
NOISE_RIDGED_MULTI_FRACTAL(float2)
NOISE_RIDGED_MULTI_FRACTAL(float3)
NOISE_RIDGED_MULTI_FRACTAL(float4)
