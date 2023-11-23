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

#pragma BLENDER_REQUIRE(gpu_shader_common_hash.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_material_noise.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_material_fractal_noise.glsl)

#define NOISE_FRACTAL_DISTORTED_1D(NOISE_TYPE) \
  if (distortion != 0.0) { \
    p += snoise(p + random_float_offset(0.0)) * distortion; \
  } \
\
  value = NOISE_TYPE(p, detail, roughness, lacunarity, offset, gain, normalize != 0.0); \
  color = vec4(value, \
               NOISE_TYPE(p + random_float_offset(1.0), \
                          detail, \
                          roughness, \
                          lacunarity, \
                          offset, \
                          gain, \
                          normalize != 0.0), \
               NOISE_TYPE(p + random_float_offset(2.0), \
                          detail, \
                          roughness, \
                          lacunarity, \
                          offset, \
                          gain, \
                          normalize != 0.0), \
               1.0);

#define NOISE_FRACTAL_DISTORTED_2D(NOISE_TYPE) \
  if (distortion != 0.0) { \
    p += vec2(snoise(p + random_vec2_offset(0.0)) * distortion, \
              snoise(p + random_vec2_offset(1.0)) * distortion); \
  } \
\
  value = NOISE_TYPE(p, detail, roughness, lacunarity, offset, gain, normalize != 0.0); \
  color = vec4(value, \
               NOISE_TYPE(p + random_vec2_offset(2.0), \
                          detail, \
                          roughness, \
                          lacunarity, \
                          offset, \
                          gain, \
                          normalize != 0.0), \
               NOISE_TYPE(p + random_vec2_offset(3.0), \
                          detail, \
                          roughness, \
                          lacunarity, \
                          offset, \
                          gain, \
                          normalize != 0.0), \
               1.0);

#define NOISE_FRACTAL_DISTORTED_3D(NOISE_TYPE) \
  if (distortion != 0.0) { \
    p += vec3(snoise(p + random_vec3_offset(0.0)) * distortion, \
              snoise(p + random_vec3_offset(1.0)) * distortion, \
              snoise(p + random_vec3_offset(2.0)) * distortion); \
  } \
\
  value = NOISE_TYPE(p, detail, roughness, lacunarity, offset, gain, normalize != 0.0); \
  color = vec4(value, \
               NOISE_TYPE(p + random_vec3_offset(3.0), \
                          detail, \
                          roughness, \
                          lacunarity, \
                          offset, \
                          gain, \
                          normalize != 0.0), \
               NOISE_TYPE(p + random_vec3_offset(4.0), \
                          detail, \
                          roughness, \
                          lacunarity, \
                          offset, \
                          gain, \
                          normalize != 0.0), \
               1.0);

#define NOISE_FRACTAL_DISTORTED_4D(NOISE_TYPE) \
  if (distortion != 0.0) { \
    p += vec4(snoise(p + random_vec4_offset(0.0)) * distortion, \
              snoise(p + random_vec4_offset(1.0)) * distortion, \
              snoise(p + random_vec4_offset(2.0)) * distortion, \
              snoise(p + random_vec4_offset(3.0)) * distortion); \
  } \
\
  value = NOISE_TYPE(p, detail, roughness, lacunarity, offset, gain, normalize != 0.0); \
  color = vec4(value, \
               NOISE_TYPE(p + random_vec4_offset(4.0), \
                          detail, \
                          roughness, \
                          lacunarity, \
                          offset, \
                          gain, \
                          normalize != 0.0), \
               NOISE_TYPE(p + random_vec4_offset(5.0), \
                          detail, \
                          roughness, \
                          lacunarity, \
                          offset, \
                          gain, \
                          normalize != 0.0), \
               1.0);

float random_float_offset(float seed)
{
  return 100.0 + hash_float_to_float(seed) * 100.0;
}

vec2 random_vec2_offset(float seed)
{
  return vec2(100.0 + hash_vec2_to_float(vec2(seed, 0.0)) * 100.0,
              100.0 + hash_vec2_to_float(vec2(seed, 1.0)) * 100.0);
}

vec3 random_vec3_offset(float seed)
{
  return vec3(100.0 + hash_vec2_to_float(vec2(seed, 0.0)) * 100.0,
              100.0 + hash_vec2_to_float(vec2(seed, 1.0)) * 100.0,
              100.0 + hash_vec2_to_float(vec2(seed, 2.0)) * 100.0);
}

vec4 random_vec4_offset(float seed)
{
  return vec4(100.0 + hash_vec2_to_float(vec2(seed, 0.0)) * 100.0,
              100.0 + hash_vec2_to_float(vec2(seed, 1.0)) * 100.0,
              100.0 + hash_vec2_to_float(vec2(seed, 2.0)) * 100.0,
              100.0 + hash_vec2_to_float(vec2(seed, 3.0)) * 100.0);
}

/* Noise fBM */

void node_noise_tex_fbm_1d(vec3 co,
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
                           out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  float p = w * scale;

  NOISE_FRACTAL_DISTORTED_1D(noise_fbm)
}

void node_noise_tex_fbm_2d(vec3 co,
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
                           out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec2 p = co.xy * scale;

  NOISE_FRACTAL_DISTORTED_2D(noise_fbm)
}

void node_noise_tex_fbm_3d(vec3 co,
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
                           out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec3 p = co * scale;

  NOISE_FRACTAL_DISTORTED_3D(noise_fbm)
}

void node_noise_tex_fbm_4d(vec3 co,
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
                           out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec4 p = vec4(co, w) * scale;

  NOISE_FRACTAL_DISTORTED_4D(noise_fbm)
}

/* Noise Multifractal */

void node_noise_tex_multi_fractal_1d(vec3 co,
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
                                     out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  float p = w * scale;

  NOISE_FRACTAL_DISTORTED_1D(noise_multi_fractal)
}

void node_noise_tex_multi_fractal_2d(vec3 co,
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
                                     out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec2 p = co.xy * scale;

  NOISE_FRACTAL_DISTORTED_2D(noise_multi_fractal)
}

void node_noise_tex_multi_fractal_3d(vec3 co,
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
                                     out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec3 p = co * scale;

  NOISE_FRACTAL_DISTORTED_3D(noise_multi_fractal)
}

void node_noise_tex_multi_fractal_4d(vec3 co,
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
                                     out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec4 p = vec4(co, w) * scale;

  NOISE_FRACTAL_DISTORTED_4D(noise_multi_fractal)
}

/* Noise Hetero Terrain */

void node_noise_tex_hetero_terrain_1d(vec3 co,
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
                                      out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  float p = w * scale;

  NOISE_FRACTAL_DISTORTED_1D(noise_hetero_terrain)
}

void node_noise_tex_hetero_terrain_2d(vec3 co,
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
                                      out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec2 p = co.xy * scale;

  NOISE_FRACTAL_DISTORTED_2D(noise_hetero_terrain)
}

void node_noise_tex_hetero_terrain_3d(vec3 co,
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
                                      out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec3 p = co * scale;

  NOISE_FRACTAL_DISTORTED_3D(noise_hetero_terrain)
}

void node_noise_tex_hetero_terrain_4d(vec3 co,
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
                                      out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec4 p = vec4(co, w) * scale;

  NOISE_FRACTAL_DISTORTED_4D(noise_hetero_terrain)
}

/* Noise Hybrid Multifractal */

void node_noise_tex_hybrid_multi_fractal_1d(vec3 co,
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
                                            out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  float p = w * scale;

  NOISE_FRACTAL_DISTORTED_1D(noise_hybrid_multi_fractal)
}

void node_noise_tex_hybrid_multi_fractal_2d(vec3 co,
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
                                            out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec2 p = co.xy * scale;

  NOISE_FRACTAL_DISTORTED_2D(noise_hybrid_multi_fractal)
}

void node_noise_tex_hybrid_multi_fractal_3d(vec3 co,
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
                                            out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec3 p = co * scale;

  NOISE_FRACTAL_DISTORTED_3D(noise_hybrid_multi_fractal)
}

void node_noise_tex_hybrid_multi_fractal_4d(vec3 co,
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
                                            out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec4 p = vec4(co, w) * scale;

  NOISE_FRACTAL_DISTORTED_4D(noise_hybrid_multi_fractal)
}

/* Noise Ridged Multifractal */

void node_noise_tex_ridged_multi_fractal_1d(vec3 co,
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
                                            out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  float p = w * scale;

  NOISE_FRACTAL_DISTORTED_1D(noise_ridged_multi_fractal)
}

void node_noise_tex_ridged_multi_fractal_2d(vec3 co,
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
                                            out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec2 p = co.xy * scale;

  NOISE_FRACTAL_DISTORTED_2D(noise_ridged_multi_fractal)
}

void node_noise_tex_ridged_multi_fractal_3d(vec3 co,
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
                                            out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec3 p = co * scale;

  NOISE_FRACTAL_DISTORTED_3D(noise_ridged_multi_fractal)
}

void node_noise_tex_ridged_multi_fractal_4d(vec3 co,
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
                                            out vec4 color)
{
  detail = clamp(detail, 0.0, 15.0);
  roughness = max(roughness, 0.0);

  vec4 p = vec4(co, w) * scale;

  NOISE_FRACTAL_DISTORTED_4D(noise_ridged_multi_fractal)
}
