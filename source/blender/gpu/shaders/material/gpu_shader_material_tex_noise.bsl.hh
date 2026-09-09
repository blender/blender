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

#pragma once

#include "gpu_shader_common_hash.bsl.hh"
#include "gpu_shader_material_fractal_noise.bsl.hh"
#include "gpu_shader_material_noise.bsl.hh"

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

/* Templated random offset resolution based on coordinate type */

template<typename T> T random_offset(float /*seed*/)
{
  return T(0);
}

template<> float random_offset<float>(float seed)
{
  return random_float_offset(seed);
}
template<> float2 random_offset<float2>(float seed)
{
  return random_vec2_offset(seed);
}
template<> float3 random_offset<float3>(float seed)
{
  return random_vec3_offset(seed);
}
template<> float4 random_offset<float4>(float seed)
{
  return random_vec4_offset(seed);
}

float distort_point(float p, float distortion)
{
  if (distortion != 0.0f) {
    p += snoise(p + random_float_offset(0.0f)) * distortion;
  }
  return p;
}

float2 distort_point(float2 p, float distortion)
{
  if (distortion != 0.0f) {
    p += float2(snoise(p + random_vec2_offset(0.0f)) * distortion,
                snoise(p + random_vec2_offset(1.0f)) * distortion);
  }
  return p;
}

float3 distort_point(float3 p, float distortion)
{
  if (distortion != 0.0f) {
    p += float3(snoise(p + random_vec3_offset(0.0f)) * distortion,
                snoise(p + random_vec3_offset(1.0f)) * distortion,
                snoise(p + random_vec3_offset(2.0f)) * distortion);
  }
  return p;
}

float4 distort_point(float4 p, float distortion)
{
  if (distortion != 0.0f) {
    p += float4(snoise(p + random_vec4_offset(0.0f)) * distortion,
                snoise(p + random_vec4_offset(1.0f)) * distortion,
                snoise(p + random_vec4_offset(2.0f)) * distortion,
                snoise(p + random_vec4_offset(3.0f)) * distortion);
  }
  return p;
}

/* Used to offset the color seed to not overlap with the distort_point seeds. */

float color_seed_offset(float /*co*/)
{
  return 1.0f;
}
float color_seed_offset(float2 /*co*/)
{
  return 2.0f;
}
float color_seed_offset(float3 /*co*/)
{
  return 3.0f;
}
float color_seed_offset(float4 /*co*/)
{
  return 4.0f;
}

/* Noise functor wrappers. */

template<typename T> struct NoiseFBM {
  static float noise_fn(T co,
                        float detail,
                        float roughness,
                        float lacunarity,
                        float offset,
                        float gain,
                        bool normalize)
  {
    return noise_fbm(co, detail, roughness, lacunarity, offset, gain, normalize);
  }
};

template struct NoiseFBM<float>;
template struct NoiseFBM<float2>;
template struct NoiseFBM<float3>;
template struct NoiseFBM<float4>;

template<typename T> struct NoiseMultiFractal {
  static float noise_fn(T co,
                        float detail,
                        float roughness,
                        float lacunarity,
                        float offset,
                        float gain,
                        bool normalize)
  {
    return noise_multi_fractal(co, detail, roughness, lacunarity, offset, gain, normalize);
  }
};

template struct NoiseMultiFractal<float>;
template struct NoiseMultiFractal<float2>;
template struct NoiseMultiFractal<float3>;
template struct NoiseMultiFractal<float4>;

template<typename T> struct NoiseHeteroTerrain {
  static float noise_fn(T co,
                        float detail,
                        float roughness,
                        float lacunarity,
                        float offset,
                        float gain,
                        bool normalize)
  {
    return noise_hetero_terrain(co, detail, roughness, lacunarity, offset, gain, normalize);
  }
};

template struct NoiseHeteroTerrain<float>;
template struct NoiseHeteroTerrain<float2>;
template struct NoiseHeteroTerrain<float3>;
template struct NoiseHeteroTerrain<float4>;

template<typename T> struct NoiseHybridMultiFractal {
  static float noise_fn(T co,
                        float detail,
                        float roughness,
                        float lacunarity,
                        float offset,
                        float gain,
                        bool normalize)
  {
    return noise_hybrid_multi_fractal(co, detail, roughness, lacunarity, offset, gain, normalize);
  }
};

template struct NoiseHybridMultiFractal<float>;
template struct NoiseHybridMultiFractal<float2>;
template struct NoiseHybridMultiFractal<float3>;
template struct NoiseHybridMultiFractal<float4>;

template<typename T> struct NoiseRidgedMultiFractal {
  static float noise_fn(T co,
                        float detail,
                        float roughness,
                        float lacunarity,
                        float offset,
                        float gain,
                        bool normalize)
  {
    return noise_ridged_multi_fractal(co, detail, roughness, lacunarity, offset, gain, normalize);
  }
};

template struct NoiseRidgedMultiFractal<float>;
template struct NoiseRidgedMultiFractal<float2>;
template struct NoiseRidgedMultiFractal<float3>;
template struct NoiseRidgedMultiFractal<float4>;

/* Unified Distortion Function Template. */

template<typename T, typename NoiseT>
void noise_fractal_distorted(T p,
                             float detail,
                             float roughness,
                             float lacunarity,
                             float offset,
                             float gain,
                             float distortion,
                             float normalize,
                             float compute_color,
                             float &value,
                             float4 &color)
{
  detail = clamp(detail, 0.0f, 15.0f);
  roughness = max(roughness, 0.0f);

  p = distort_point(p, distortion);

  bool norm = (normalize != 0.0f);
  value = NoiseT::noise_fn(p, detail, roughness, lacunarity, offset, gain, norm);

  if (compute_color != 0.0f) {
    float seed_base = color_seed_offset(p);
    color = float4(value,
                   NoiseT::noise_fn(p + random_offset<T>(seed_base + 0.0f),
                                    detail,
                                    roughness,
                                    lacunarity,
                                    offset,
                                    gain,
                                    norm),
                   NoiseT::noise_fn(p + random_offset<T>(seed_base + 1.0f),
                                    detail,
                                    roughness,
                                    lacunarity,
                                    offset,
                                    gain,
                                    norm),
                   1.0f);
  }
}

template void noise_fractal_distorted<float, NoiseFBM<float>>(
    float, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float2, NoiseFBM<float2>>(
    float2, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float3, NoiseFBM<float3>>(
    float3, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float4, NoiseFBM<float4>>(
    float4, float, float, float, float, float, float, float, float, float &, float4 &);

template void noise_fractal_distorted<float, NoiseMultiFractal<float>>(
    float, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float2, NoiseMultiFractal<float2>>(
    float2, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float3, NoiseMultiFractal<float3>>(
    float3, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float4, NoiseMultiFractal<float4>>(
    float4, float, float, float, float, float, float, float, float, float &, float4 &);

template void noise_fractal_distorted<float, NoiseHeteroTerrain<float>>(
    float, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float2, NoiseHeteroTerrain<float2>>(
    float2, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float3, NoiseHeteroTerrain<float3>>(
    float3, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float4, NoiseHeteroTerrain<float4>>(
    float4, float, float, float, float, float, float, float, float, float &, float4 &);

template void noise_fractal_distorted<float, NoiseHybridMultiFractal<float>>(
    float, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float2, NoiseHybridMultiFractal<float2>>(
    float2, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float3, NoiseHybridMultiFractal<float3>>(
    float3, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float4, NoiseHybridMultiFractal<float4>>(
    float4, float, float, float, float, float, float, float, float, float &, float4 &);

template void noise_fractal_distorted<float, NoiseRidgedMultiFractal<float>>(
    float, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float2, NoiseRidgedMultiFractal<float2>>(
    float2, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float3, NoiseRidgedMultiFractal<float3>>(
    float3, float, float, float, float, float, float, float, float, float &, float4 &);
template void noise_fractal_distorted<float4, NoiseRidgedMultiFractal<float4>>(
    float4, float, float, float, float, float, float, float, float, float &, float4 &);

/* Noise fBM */

[[node]]
void node_noise_tex_fbm_1d(float3 /*co*/,
                           float w,
                           float scale,
                           float detail,
                           float roughness,
                           float lacunarity,
                           float offset,
                           float gain,
                           float distortion,
                           float normalize,
                           float compute_color,
                           float &value,
                           float4 &color)
{
  noise_fractal_distorted<float, NoiseFBM<float>>(w * scale,
                                                  detail,
                                                  roughness,
                                                  lacunarity,
                                                  offset,
                                                  gain,
                                                  distortion,
                                                  normalize,
                                                  compute_color,
                                                  value,
                                                  color);
}

[[node]]
void node_noise_tex_fbm_2d(float3 co,
                           float /*w*/,
                           float scale,
                           float detail,
                           float roughness,
                           float lacunarity,
                           float offset,
                           float gain,
                           float distortion,
                           float normalize,
                           float compute_color,
                           float &value,
                           float4 &color)
{
  noise_fractal_distorted<float2, NoiseFBM<float2>>(co.xy * scale,
                                                    detail,
                                                    roughness,
                                                    lacunarity,
                                                    offset,
                                                    gain,
                                                    distortion,
                                                    normalize,
                                                    compute_color,
                                                    value,
                                                    color);
}

[[node]]
void node_noise_tex_fbm_3d(float3 co,
                           float /*w*/,
                           float scale,
                           float detail,
                           float roughness,
                           float lacunarity,
                           float offset,
                           float gain,
                           float distortion,
                           float normalize,
                           float compute_color,
                           float &value,
                           float4 &color)
{
  noise_fractal_distorted<float3, NoiseFBM<float3>>(co * scale,
                                                    detail,
                                                    roughness,
                                                    lacunarity,
                                                    offset,
                                                    gain,
                                                    distortion,
                                                    normalize,
                                                    compute_color,
                                                    value,
                                                    color);
}

[[node]]
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
                           float compute_color,
                           float &value,
                           float4 &color)
{
  noise_fractal_distorted<float4, NoiseFBM<float4>>(float4(co, w) * scale,
                                                    detail,
                                                    roughness,
                                                    lacunarity,
                                                    offset,
                                                    gain,
                                                    distortion,
                                                    normalize,
                                                    compute_color,
                                                    value,
                                                    color);
}

/* Noise Multi-fractal. */

[[node]]
void node_noise_tex_multi_fractal_1d(float3 /*co*/,
                                     float w,
                                     float scale,
                                     float detail,
                                     float roughness,
                                     float lacunarity,
                                     float offset,
                                     float gain,
                                     float distortion,
                                     float normalize,
                                     float compute_color,
                                     float &value,
                                     float4 &color)
{
  noise_fractal_distorted<float, NoiseMultiFractal<float>>(w * scale,
                                                           detail,
                                                           roughness,
                                                           lacunarity,
                                                           offset,
                                                           gain,
                                                           distortion,
                                                           normalize,
                                                           compute_color,
                                                           value,
                                                           color);
}

[[node]]
void node_noise_tex_multi_fractal_2d(float3 co,
                                     float /*w*/,
                                     float scale,
                                     float detail,
                                     float roughness,
                                     float lacunarity,
                                     float offset,
                                     float gain,
                                     float distortion,
                                     float normalize,
                                     float compute_color,
                                     float &value,
                                     float4 &color)
{
  noise_fractal_distorted<float2, NoiseMultiFractal<float2>>(co.xy * scale,
                                                             detail,
                                                             roughness,
                                                             lacunarity,
                                                             offset,
                                                             gain,
                                                             distortion,
                                                             normalize,
                                                             compute_color,
                                                             value,
                                                             color);
}

[[node]]
void node_noise_tex_multi_fractal_3d(float3 co,
                                     float /*w*/,
                                     float scale,
                                     float detail,
                                     float roughness,
                                     float lacunarity,
                                     float offset,
                                     float gain,
                                     float distortion,
                                     float normalize,
                                     float compute_color,
                                     float &value,
                                     float4 &color)
{
  noise_fractal_distorted<float3, NoiseMultiFractal<float3>>(co * scale,
                                                             detail,
                                                             roughness,
                                                             lacunarity,
                                                             offset,
                                                             gain,
                                                             distortion,
                                                             normalize,
                                                             compute_color,
                                                             value,
                                                             color);
}

[[node]]
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
                                     float compute_color,
                                     float &value,
                                     float4 &color)
{
  noise_fractal_distorted<float4, NoiseMultiFractal<float4>>(float4(co, w) * scale,
                                                             detail,
                                                             roughness,
                                                             lacunarity,
                                                             offset,
                                                             gain,
                                                             distortion,
                                                             normalize,
                                                             compute_color,
                                                             value,
                                                             color);
}

/* Noise Hetero Terrain */

[[node]]
void node_noise_tex_hetero_terrain_1d(float3 /*co*/,
                                      float w,
                                      float scale,
                                      float detail,
                                      float roughness,
                                      float lacunarity,
                                      float offset,
                                      float gain,
                                      float distortion,
                                      float normalize,
                                      float compute_color,
                                      float &value,
                                      float4 &color)
{
  noise_fractal_distorted<float, NoiseHeteroTerrain<float>>(w * scale,
                                                            detail,
                                                            roughness,
                                                            lacunarity,
                                                            offset,
                                                            gain,
                                                            distortion,
                                                            normalize,
                                                            compute_color,
                                                            value,
                                                            color);
}

[[node]]
void node_noise_tex_hetero_terrain_2d(float3 co,
                                      float /*w*/,
                                      float scale,
                                      float detail,
                                      float roughness,
                                      float lacunarity,
                                      float offset,
                                      float gain,
                                      float distortion,
                                      float normalize,
                                      float compute_color,
                                      float &value,
                                      float4 &color)
{
  noise_fractal_distorted<float2, NoiseHeteroTerrain<float2>>(co.xy * scale,
                                                              detail,
                                                              roughness,
                                                              lacunarity,
                                                              offset,
                                                              gain,
                                                              distortion,
                                                              normalize,
                                                              compute_color,
                                                              value,
                                                              color);
}

[[node]]
void node_noise_tex_hetero_terrain_3d(float3 co,
                                      float /*w*/,
                                      float scale,
                                      float detail,
                                      float roughness,
                                      float lacunarity,
                                      float offset,
                                      float gain,
                                      float distortion,
                                      float normalize,
                                      float compute_color,
                                      float &value,
                                      float4 &color)
{
  noise_fractal_distorted<float3, NoiseHeteroTerrain<float3>>(co * scale,
                                                              detail,
                                                              roughness,
                                                              lacunarity,
                                                              offset,
                                                              gain,
                                                              distortion,
                                                              normalize,
                                                              compute_color,
                                                              value,
                                                              color);
}

[[node]]
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
                                      float compute_color,
                                      float &value,
                                      float4 &color)
{
  noise_fractal_distorted<float4, NoiseHeteroTerrain<float4>>(float4(co, w) * scale,
                                                              detail,
                                                              roughness,
                                                              lacunarity,
                                                              offset,
                                                              gain,
                                                              distortion,
                                                              normalize,
                                                              compute_color,
                                                              value,
                                                              color);
}

/* Noise Hybrid Multi-fractal. */

[[node]]
void node_noise_tex_hybrid_multi_fractal_1d(float3 /*co*/,
                                            float w,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            float compute_color,
                                            float &value,
                                            float4 &color)
{
  noise_fractal_distorted<float, NoiseHybridMultiFractal<float>>(w * scale,
                                                                 detail,
                                                                 roughness,
                                                                 lacunarity,
                                                                 offset,
                                                                 gain,
                                                                 distortion,
                                                                 normalize,
                                                                 compute_color,
                                                                 value,
                                                                 color);
}

[[node]]
void node_noise_tex_hybrid_multi_fractal_2d(float3 co,
                                            float /*w*/,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            float compute_color,
                                            float &value,
                                            float4 &color)
{
  noise_fractal_distorted<float2, NoiseHybridMultiFractal<float2>>(co.xy * scale,
                                                                   detail,
                                                                   roughness,
                                                                   lacunarity,
                                                                   offset,
                                                                   gain,
                                                                   distortion,
                                                                   normalize,
                                                                   compute_color,
                                                                   value,
                                                                   color);
}

[[node]]
void node_noise_tex_hybrid_multi_fractal_3d(float3 co,
                                            float /*w*/,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            float compute_color,
                                            float &value,
                                            float4 &color)
{
  noise_fractal_distorted<float3, NoiseHybridMultiFractal<float3>>(co * scale,
                                                                   detail,
                                                                   roughness,
                                                                   lacunarity,
                                                                   offset,
                                                                   gain,
                                                                   distortion,
                                                                   normalize,
                                                                   compute_color,
                                                                   value,
                                                                   color);
}

[[node]]
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
                                            float compute_color,
                                            float &value,
                                            float4 &color)
{
  noise_fractal_distorted<float4, NoiseHybridMultiFractal<float4>>(float4(co, w) * scale,
                                                                   detail,
                                                                   roughness,
                                                                   lacunarity,
                                                                   offset,
                                                                   gain,
                                                                   distortion,
                                                                   normalize,
                                                                   compute_color,
                                                                   value,
                                                                   color);
}

/* Noise Ridged Multi-fractal. */

[[node]]
void node_noise_tex_ridged_multi_fractal_1d(float3 /*co*/,
                                            float w,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            float compute_color,
                                            float &value,
                                            float4 &color)
{
  noise_fractal_distorted<float, NoiseRidgedMultiFractal<float>>(w * scale,
                                                                 detail,
                                                                 roughness,
                                                                 lacunarity,
                                                                 offset,
                                                                 gain,
                                                                 distortion,
                                                                 normalize,
                                                                 compute_color,
                                                                 value,
                                                                 color);
}

[[node]]
void node_noise_tex_ridged_multi_fractal_2d(float3 co,
                                            float /*w*/,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            float compute_color,
                                            float &value,
                                            float4 &color)
{
  noise_fractal_distorted<float2, NoiseRidgedMultiFractal<float2>>(co.xy * scale,
                                                                   detail,
                                                                   roughness,
                                                                   lacunarity,
                                                                   offset,
                                                                   gain,
                                                                   distortion,
                                                                   normalize,
                                                                   compute_color,
                                                                   value,
                                                                   color);
}

[[node]]
void node_noise_tex_ridged_multi_fractal_3d(float3 co,
                                            float /*w*/,
                                            float scale,
                                            float detail,
                                            float roughness,
                                            float lacunarity,
                                            float offset,
                                            float gain,
                                            float distortion,
                                            float normalize,
                                            float compute_color,
                                            float &value,
                                            float4 &color)
{
  noise_fractal_distorted<float3, NoiseRidgedMultiFractal<float3>>(co * scale,
                                                                   detail,
                                                                   roughness,
                                                                   lacunarity,
                                                                   offset,
                                                                   gain,
                                                                   distortion,
                                                                   normalize,
                                                                   compute_color,
                                                                   value,
                                                                   color);
}

[[node]]
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
                                            float compute_color,
                                            float &value,
                                            float4 &color)
{
  noise_fractal_distorted<float4, NoiseRidgedMultiFractal<float4>>(float4(co, w) * scale,
                                                                   detail,
                                                                   roughness,
                                                                   lacunarity,
                                                                   offset,
                                                                   gain,
                                                                   distortion,
                                                                   normalize,
                                                                   compute_color,
                                                                   value,
                                                                   color);
}
