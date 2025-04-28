/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The following offset functions generate random offsets to be added to texture
 * coordinates to act as a seed since the noise functions don't have seed values.
 * A seed value is needed for generating distortion textures and color outputs.
 * The offset's components are in the range [100, 200], not too high to cause
 * bad precision and not too small to be noticeable. We use float seed because
 * OSL only support float hashes.
 */

#include "gpu_shader_common_hash.glsl"
#include "gpu_shader_material_fractal_noise.glsl"
#include "gpu_shader_material_noise.glsl"

#define NOISE_FRACTAL_DISTORTED_1D(NOISE_TYPE) \
  if (distortion != 0.0f) { \
    p += snoise(p + random_float_offset(0.0f)) * distortion; \
  } \
\
  value = NOISE_TYPE(p, detail, roughness, lacunarity, offset, gain, normalize != 0.0f); \
  color = float4(value, \
                 NOISE_TYPE(p + random_float_offset(1.0f), \
                            detail, \
                            roughness, \
                            lacunarity, \
                            offset, \
                            gain, \
                            normalize != 0.0f), \
                 NOISE_TYPE(p + random_float_offset(2.0f), \
                            detail, \
                            roughness, \
                            lacunarity, \
                            offset, \
                            gain, \
                            normalize != 0.0f), \
                 1.0f);

#define NOISE_FRACTAL_DISTORTED_2D(NOISE_TYPE) \
  if (distortion != 0.0f) { \
    p += float2(snoise(p + random_vec2_offset(0.0f)) * distortion, \
                snoise(p + random_vec2_offset(1.0f)) * distortion); \
  } \
\
  value = NOISE_TYPE(p, detail, roughness, lacunarity, offset, gain, normalize != 0.0f); \
  color = float4(value, \
                 NOISE_TYPE(p + random_vec2_offset(2.0f), \
                            detail, \
                            roughness, \
                            lacunarity, \
                            offset, \
                            gain, \
                            normalize != 0.0f), \
                 NOISE_TYPE(p + random_vec2_offset(3.0f), \
                            detail, \
                            roughness, \
                            lacunarity, \
                            offset, \
                            gain, \
                            normalize != 0.0f), \
                 1.0f);

#define NOISE_FRACTAL_DISTORTED_3D(NOISE_TYPE) \
  if (distortion != 0.0f) { \
    p += float3(snoise(p + random_vec3_offset(0.0f)) * distortion, \
                snoise(p + random_vec3_offset(1.0f)) * distortion, \
                snoise(p + random_vec3_offset(2.0f)) * distortion); \
  } \
\
  value = NOISE_TYPE(p, detail, roughness, lacunarity, offset, gain, normalize != 0.0f); \
  color = float4(value, \
                 NOISE_TYPE(p + random_vec3_offset(3.0f), \
                            detail, \
                            roughness, \
                            lacunarity, \
                            offset, \
                            gain, \
                            normalize != 0.0f), \
                 NOISE_TYPE(p + random_vec3_offset(4.0f), \
                            detail, \
                            roughness, \
                            lacunarity, \
                            offset, \
                            gain, \
                            normalize != 0.0f), \
                 1.0f);

#define NOISE_FRACTAL_DISTORTED_4D(NOISE_TYPE) \
  if (distortion != 0.0f) { \
    p += float4(snoise(p + random_vec4_offset(0.0f)) * distortion, \
                snoise(p + random_vec4_offset(1.0f)) * distortion, \
                snoise(p + random_vec4_offset(2.0f)) * distortion, \
                snoise(p + random_vec4_offset(3.0f)) * distortion); \
  } \
\
  value = NOISE_TYPE(p, detail, roughness, lacunarity, offset, gain, normalize != 0.0f); \
  color = float4(value, \
                 NOISE_TYPE(p + random_vec4_offset(4.0f), \
                            detail, \
                            roughness, \
                            lacunarity, \
                            offset, \
                            gain, \
                            normalize != 0.0f), \
                 NOISE_TYPE(p + random_vec4_offset(5.0f), \
                            detail, \
                            roughness, \
                            lacunarity, \
                            offset, \
                            gain, \
                            normalize != 0.0f), \
                 1.0f);

float random_float_offset(float seed)
{
  return 100.0f + hash_float_to_float(seed) * 100.0f;
}

float2 random_vec2_offset(float seed)
{
  return float2(100.0f + hash_vec2_to_float(float2(seed, 0.0f)) * 100.0f,
                100.0f + hash_vec2_to_float(float2(seed, 1.0f)) * 100.0f);
}

float3 random_vec3_offset(float seed)
{
  return float3(100.0f + hash_vec2_to_float(float2(seed, 0.0f)) * 100.0f,
                100.0f + hash_vec2_to_float(float2(seed, 1.0f)) * 100.0f,
                100.0f + hash_vec2_to_float(float2(seed, 2.0f)) * 100.0f);
}

float4 random_vec4_offset(float seed)
{
  return float4(100.0f + hash_vec2_to_float(float2(seed, 0.0f)) * 100.0f,
                100.0f + hash_vec2_to_float(float2(seed, 1.0f)) * 100.0f,
                100.0f + hash_vec2_to_float(float2(seed, 2.0f)) * 100.0f,
                100.0f + hash_vec2_to_float(float2(seed, 3.0f)) * 100.0f);
}

/* Noise fBM */

void node_noise_tex_fbm_1d(float3 co,
                           float w,
                           float scale,
                           float detail,
                           float roughness,
                           float lacunarity,
                           float offset,
                           float gain,
                           float distortion,
                           float normalize,
                           out float value,
                           out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float p = w * scale;

  NOISE_FRACTAL_DISTORTED_1D(noise_fbm)
}

void node_noise_tex_fbm_2d(float3 co,
                           float w,
                           float scale,
                           float detail,
                           float roughness,
                           float lacunarity,
                           float offset,
                           float gain,
                           float distortion,
                           float normalize,
                           out float value,
                           out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float2 p = co.xy * scale;

  NOISE_FRACTAL_DISTORTED_2D(noise_fbm)
}

void node_noise_tex_fbm_3d(float3 co,
                           float w,
                           float scale,
                           float detail,
                           float roughness,
                           float lacunarity,
                           float offset,
                           float gain,
                           float distortion,
                           float normalize,
                           out float value,
                           out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float3 p = co * scale;

  NOISE_FRACTAL_DISTORTED_3D(noise_fbm)
}

void node_noise_tex_fbm_4d(float3 co,
                           float w,
                           float scale,
                           float detail,
                           float roughness,
                           float lacunarity,
                           float offset,
                           float gain,
                           float distortion,
                           float normalize,
                           out float value,
                           out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float4 p = float4(co, w) * scale;

  NOISE_FRACTAL_DISTORTED_4D(noise_fbm)
}

/* Noise Multi-fractal. */

void node_noise_tex_multi_fractal_1d(float3 co,
                                     float w,
                                     float scale,
                                     float detail,
                                     float roughness,
                                     float lacunarity,
                                     float offset,
                                     float gain,
                                     float distortion,
                                     float normalize,
                                     out float value,
                                     out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float p = w * scale;

  NOISE_FRACTAL_DISTORTED_1D(noise_multi_fractal)
}

void node_noise_tex_multi_fractal_2d(float3 co,
                                     float w,
                                     float scale,
                                     float detail,
                                     float roughness,
                                     float lacunarity,
                                     float offset,
                                     float gain,
                                     float distortion,
                                     float normalize,
                                     out float value,
                                     out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float2 p = co.xy * scale;

  NOISE_FRACTAL_DISTORTED_2D(noise_multi_fractal)
}

void node_noise_tex_multi_fractal_3d(float3 co,
                                     float w,
                                     float scale,
                                     float detail,
                                     float roughness,
                                     float lacunarity,
                                     float offset,
                                     float gain,
                                     float distortion,
                                     float normalize,
                                     out float value,
                                     out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float3 p = co * scale;

  NOISE_FRACTAL_DISTORTED_3D(noise_multi_fractal)
}

void node_noise_tex_multi_fractal_4d(float3 co,
                                     float w,
                                     float scale,
                                     float detail,
                                     float roughness,
                                     float lacunarity,
                                     float offset,
                                     float gain,
                                     float distortion,
                                     float normalize,
                                     out float value,
                                     out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float4 p = float4(co, w) * scale;

  NOISE_FRACTAL_DISTORTED_4D(noise_multi_fractal)
}

/* Noise Hetero Terrain */

void node_noise_tex_hetero_terrain_1d(float3 co,
                                      float w,
                                      float scale,
                                      float detail,
                                      float roughness,
                                      float lacunarity,
                                      float offset,
                                      float gain,
                                      float distortion,
                                      float normalize,
                                      out float value,
                                      out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float p = w * scale;

  NOISE_FRACTAL_DISTORTED_1D(noise_hetero_terrain)
}

void node_noise_tex_hetero_terrain_2d(float3 co,
                                      float w,
                                      float scale,
                                      float detail,
                                      float roughness,
                                      float lacunarity,
                                      float offset,
                                      float gain,
                                      float distortion,
                                      float normalize,
                                      out float value,
                                      out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float2 p = co.xy * scale;

  NOISE_FRACTAL_DISTORTED_2D(noise_hetero_terrain)
}

void node_noise_tex_hetero_terrain_3d(float3 co,
                                      float w,
                                      float scale,
                                      float detail,
                                      float roughness,
                                      float lacunarity,
                                      float offset,
                                      float gain,
                                      float distortion,
                                      float normalize,
                                      out float value,
                                      out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float3 p = co * scale;

  NOISE_FRACTAL_DISTORTED_3D(noise_hetero_terrain)
}

void node_noise_tex_hetero_terrain_4d(float3 co,
                                      float w,
                                      float scale,
                                      float detail,
                                      float roughness,
                                      float lacunarity,
                                      float offset,
                                      float gain,
                                      float distortion,
                                      float normalize,
                                      out float value,
                                      out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float4 p = float4(co, w) * scale;

  NOISE_FRACTAL_DISTORTED_4D(noise_hetero_terrain)
}

/* Noise Hybrid Multi-fractal. */

void node_noise_tex_hybrid_multi_fractal_1d(float3 co,
                                            float w,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            out float value,
                                            out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float p = w * scale;

  NOISE_FRACTAL_DISTORTED_1D(noise_hybrid_multi_fractal)
}

void node_noise_tex_hybrid_multi_fractal_2d(float3 co,
                                            float w,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            out float value,
                                            out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float2 p = co.xy * scale;

  NOISE_FRACTAL_DISTORTED_2D(noise_hybrid_multi_fractal)
}

void node_noise_tex_hybrid_multi_fractal_3d(float3 co,
                                            float w,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            out float value,
                                            out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float3 p = co * scale;

  NOISE_FRACTAL_DISTORTED_3D(noise_hybrid_multi_fractal)
}

void node_noise_tex_hybrid_multi_fractal_4d(float3 co,
                                            float w,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            out float value,
                                            out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float4 p = float4(co, w) * scale;

  NOISE_FRACTAL_DISTORTED_4D(noise_hybrid_multi_fractal)
}

/* Noise Ridged Multi-fractal. */

void node_noise_tex_ridged_multi_fractal_1d(float3 co,
                                            float w,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            out float value,
                                            out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float p = w * scale;

  NOISE_FRACTAL_DISTORTED_1D(noise_ridged_multi_fractal)
}

void node_noise_tex_ridged_multi_fractal_2d(float3 co,
                                            float w,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            out float value,
                                            out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float2 p = co.xy * scale;

  NOISE_FRACTAL_DISTORTED_2D(noise_ridged_multi_fractal)
}

void node_noise_tex_ridged_multi_fractal_3d(float3 co,
                                            float w,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            out float value,
                                            out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float3 p = co * scale;

  NOISE_FRACTAL_DISTORTED_3D(noise_ridged_multi_fractal)
}

void node_noise_tex_ridged_multi_fractal_4d(float3 co,
                                            float w,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            out float value,
                                            out float4 color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  float4 p = float4(co, w) * scale;

  NOISE_FRACTAL_DISTORTED_4D(noise_ridged_multi_fractal)
}
