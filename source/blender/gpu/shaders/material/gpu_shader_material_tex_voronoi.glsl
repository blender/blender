/* SPDX-FileCopyrightText: 2013 Inigo Quilez
 * SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: MIT AND GPL-2.0-or-later */

/*
 * Smooth Voronoi:
 *
 * - https://wiki.blender.org/wiki/User:OmarSquircleArt/GSoC2019/Documentation/Smooth_Voronoi
 *
 * Distance To Edge based on:
 *
 * - https://www.iquilezles.org/www/articles/voronoilines/voronoilines.htm
 * - https://www.shadertoy.com/view/ldl3W8
 *
 * With optimization to change -2..2 scan window to -1..1 for better performance,
 * as explained in https://www.shadertoy.com/view/llG3zy.
 */

#include "gpu_shader_common_hash.glsl"
#include "gpu_shader_material_fractal_voronoi.glsl"
#include "gpu_shader_material_voronoi.glsl"
#include "gpu_shader_math_base_lib.glsl"

#define INITIALIZE_VORONOIPARAMS(FEATURE) \
  params.feature = FEATURE; \
  params.metric = int(metric); \
  params.scale = scale; \
  params.detail = clamp(detail, 0.0f, 15.0f); \
  params.roughness = clamp(roughness, 0.0f, 1.0f); \
  params.lacunarity = lacunarity; \
  params.smoothness = clamp(smoothness / 2.0f, 0.0f, 0.5f); \
  params.exponent = exponent; \
  params.randomness = clamp(randomness, 0.0f, 1.0f); \
  params.max_distance = 0.0f; \
  params.normalize = bool(normalize);

/* **** 1D Voronoi **** */

void node_tex_voronoi_f1_1d(float3 coord,
                            float w,
                            float scale,
                            float detail,
                            float roughness,
                            float lacunarity,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            float normalize,
                            out float outDistance,
                            out float4 outColor,
                            out float3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F1)

  w *= scale;

  params.max_distance = 0.5f + 0.5f * params.randomness;
  VoronoiOutput Output = fractal_voronoi_x_fx(params, w);
  outDistance = Output.Distance;
  outColor = float4(Output.Color, 1.0f);
  outW = Output.Position.w;
}

void node_tex_voronoi_smooth_f1_1d(float3 coord,
                                   float w,
                                   float scale,
                                   float detail,
                                   float roughness,
                                   float lacunarity,
                                   float smoothness,
                                   float exponent,
                                   float randomness,
                                   float metric,
                                   float normalize,
                                   out float outDistance,
                                   out float4 outColor,
                                   out float3 outPosition,
                                   out float outW,
                                   out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_SMOOTH_F1)

  w *= scale;

  params.max_distance = 0.5f + 0.5f * params.randomness;
  VoronoiOutput Output = fractal_voronoi_x_fx(params, w);
  outDistance = Output.Distance;
  outColor = float4(Output.Color, 1.0f);
  outW = Output.Position.w;
}

void node_tex_voronoi_f2_1d(float3 coord,
                            float w,
                            float scale,
                            float detail,
                            float roughness,
                            float lacunarity,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            float normalize,
                            out float outDistance,
                            out float4 outColor,
                            out float3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F2)

  w *= scale;

  params.max_distance = (0.5f + 0.5f * params.randomness) * 2.0f;
  VoronoiOutput Output = fractal_voronoi_x_fx(params, w);
  outDistance = Output.Distance;
  outColor = float4(Output.Color, 1.0f);
  outW = Output.Position.w;
}

void node_tex_voronoi_distance_to_edge_1d(float3 coord,
                                          float w,
                                          float scale,
                                          float detail,
                                          float roughness,
                                          float lacunarity,
                                          float smoothness,
                                          float exponent,
                                          float randomness,
                                          float metric,
                                          float normalize,
                                          out float outDistance,
                                          out float4 outColor,
                                          out float3 outPosition,
                                          out float outW,
                                          out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_DISTANCE_TO_EDGE)

  w *= scale;

  params.max_distance = 0.5f + 0.5f * params.randomness;
  outDistance = fractal_voronoi_distance_to_edge(params, w);
}

void node_tex_voronoi_n_sphere_radius_1d(float3 coord,
                                         float w,
                                         float scale,
                                         float detail,
                                         float roughness,
                                         float lacunarity,
                                         float smoothness,
                                         float exponent,
                                         float randomness,
                                         float metric,
                                         float normalize,
                                         out float outDistance,
                                         out float4 outColor,
                                         out float3 outPosition,
                                         out float outW,
                                         out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_N_SPHERE_RADIUS)

  w *= scale;

  outRadius = voronoi_n_sphere_radius(params, w);
}

/* **** 2D Voronoi **** */

void node_tex_voronoi_f1_2d(float3 coord,
                            float w,
                            float scale,
                            float detail,
                            float roughness,
                            float lacunarity,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            float normalize,
                            out float outDistance,
                            out float4 outColor,
                            out float3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F1)

  coord *= scale;

  params.max_distance = voronoi_distance(
      float2(0.0f), float2(0.5f + 0.5f * params.randomness), params);
  VoronoiOutput Output = fractal_voronoi_x_fx(params, coord.xy);
  outDistance = Output.Distance;
  outColor = float4(Output.Color, 1.0f);
  outPosition = Output.Position.xyz;
}

void node_tex_voronoi_smooth_f1_2d(float3 coord,
                                   float w,
                                   float scale,
                                   float detail,
                                   float roughness,
                                   float lacunarity,
                                   float smoothness,
                                   float exponent,
                                   float randomness,
                                   float metric,
                                   float normalize,
                                   out float outDistance,
                                   out float4 outColor,
                                   out float3 outPosition,
                                   out float outW,
                                   out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_SMOOTH_F1)

  coord *= scale;

  params.max_distance = voronoi_distance(
      float2(0.0f), float2(0.5f + 0.5f * params.randomness), params);
  VoronoiOutput Output = fractal_voronoi_x_fx(params, coord.xy);
  outDistance = Output.Distance;
  outColor = float4(Output.Color, 1.0f);
  outPosition = Output.Position.xyz;
}

void node_tex_voronoi_f2_2d(float3 coord,
                            float w,
                            float scale,
                            float detail,
                            float roughness,
                            float lacunarity,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            float normalize,
                            out float outDistance,
                            out float4 outColor,
                            out float3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F2)

  coord *= scale;

  params.max_distance = voronoi_distance(
                            float2(0.0f), float2(0.5f + 0.5f * params.randomness), params) *
                        2.0f;
  VoronoiOutput Output = fractal_voronoi_x_fx(params, coord.xy);
  outDistance = Output.Distance;
  outColor = float4(Output.Color, 1.0f);
  outPosition = Output.Position.xyz;
}

void node_tex_voronoi_distance_to_edge_2d(float3 coord,
                                          float w,
                                          float scale,
                                          float detail,
                                          float roughness,
                                          float lacunarity,
                                          float smoothness,
                                          float exponent,
                                          float randomness,
                                          float metric,
                                          float normalize,
                                          out float outDistance,
                                          out float4 outColor,
                                          out float3 outPosition,
                                          out float outW,
                                          out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_DISTANCE_TO_EDGE)

  coord *= scale;

  params.max_distance = 0.5f + 0.5f * params.randomness;
  outDistance = fractal_voronoi_distance_to_edge(params, coord.xy);
}

void node_tex_voronoi_n_sphere_radius_2d(float3 coord,
                                         float w,
                                         float scale,
                                         float detail,
                                         float roughness,
                                         float lacunarity,
                                         float smoothness,
                                         float exponent,
                                         float randomness,
                                         float metric,
                                         float normalize,
                                         out float outDistance,
                                         out float4 outColor,
                                         out float3 outPosition,
                                         out float outW,
                                         out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_N_SPHERE_RADIUS)

  coord *= scale;

  outRadius = voronoi_n_sphere_radius(params, coord.xy);
}

/* **** 3D Voronoi **** */

void node_tex_voronoi_f1_3d(float3 coord,
                            float w,
                            float scale,
                            float detail,
                            float roughness,
                            float lacunarity,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            float normalize,
                            out float outDistance,
                            out float4 outColor,
                            out float3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F1)

  coord *= scale;

  params.max_distance = voronoi_distance(
      float3(0.0f), float3(0.5f + 0.5f * params.randomness), params);
  VoronoiOutput Output = fractal_voronoi_x_fx(params, coord);
  outDistance = Output.Distance;
  outColor = float4(Output.Color, 1.0f);
  outPosition = Output.Position.xyz;
}

void node_tex_voronoi_smooth_f1_3d(float3 coord,
                                   float w,
                                   float scale,
                                   float detail,
                                   float roughness,
                                   float lacunarity,
                                   float smoothness,
                                   float exponent,
                                   float randomness,
                                   float metric,
                                   float normalize,
                                   out float outDistance,
                                   out float4 outColor,
                                   out float3 outPosition,
                                   out float outW,
                                   out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_SMOOTH_F1)

  coord *= scale;

  params.max_distance = voronoi_distance(
      float3(0.0f), float3(0.5f + 0.5f * params.randomness), params);
  VoronoiOutput Output = fractal_voronoi_x_fx(params, coord);
  outDistance = Output.Distance;
  outColor = float4(Output.Color, 1.0f);
  outPosition = Output.Position.xyz;
}

void node_tex_voronoi_f2_3d(float3 coord,
                            float w,
                            float scale,
                            float detail,
                            float roughness,
                            float lacunarity,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            float normalize,
                            out float outDistance,
                            out float4 outColor,
                            out float3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F2)

  coord *= scale;

  params.max_distance = voronoi_distance(
                            float3(0.0f), float3(0.5f + 0.5f * params.randomness), params) *
                        2.0f;
  VoronoiOutput Output = fractal_voronoi_x_fx(params, coord);
  outDistance = Output.Distance;
  outColor = float4(Output.Color, 1.0f);
  outPosition = Output.Position.xyz;
}

void node_tex_voronoi_distance_to_edge_3d(float3 coord,
                                          float w,
                                          float scale,
                                          float detail,
                                          float roughness,
                                          float lacunarity,
                                          float smoothness,
                                          float exponent,
                                          float randomness,
                                          float metric,
                                          float normalize,
                                          out float outDistance,
                                          out float4 outColor,
                                          out float3 outPosition,
                                          out float outW,
                                          out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_DISTANCE_TO_EDGE)

  coord *= scale;

  params.max_distance = 0.5f + 0.5f * params.randomness;
  outDistance = fractal_voronoi_distance_to_edge(params, coord);
}

void node_tex_voronoi_n_sphere_radius_3d(float3 coord,
                                         float w,
                                         float scale,
                                         float detail,
                                         float roughness,
                                         float lacunarity,
                                         float smoothness,
                                         float exponent,
                                         float randomness,
                                         float metric,
                                         float normalize,
                                         out float outDistance,
                                         out float4 outColor,
                                         out float3 outPosition,
                                         out float outW,
                                         out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_N_SPHERE_RADIUS)

  coord *= scale;

  outRadius = voronoi_n_sphere_radius(params, coord);
}

/* **** 4D Voronoi **** */

void node_tex_voronoi_f1_4d(float3 coord,
                            float w,
                            float scale,
                            float detail,
                            float roughness,
                            float lacunarity,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            float normalize,
                            out float outDistance,
                            out float4 outColor,
                            out float3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F1)

  w *= scale;
  coord *= scale;

  params.max_distance = voronoi_distance(
      float4(0.0f), float4(0.5f + 0.5f * params.randomness), params);
  VoronoiOutput Output = fractal_voronoi_x_fx(params, float4(coord, w));
  outDistance = Output.Distance;
  outColor = float4(Output.Color, 1.0f);
  outPosition = Output.Position.xyz;
  outW = Output.Position.w;
}

void node_tex_voronoi_smooth_f1_4d(float3 coord,
                                   float w,
                                   float scale,
                                   float detail,
                                   float roughness,
                                   float lacunarity,
                                   float smoothness,
                                   float exponent,
                                   float randomness,
                                   float metric,
                                   float normalize,
                                   out float outDistance,
                                   out float4 outColor,
                                   out float3 outPosition,
                                   out float outW,
                                   out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_SMOOTH_F1)

  w *= scale;
  coord *= scale;

  params.max_distance = voronoi_distance(
      float4(0.0f), float4(0.5f + 0.5f * params.randomness), params);
  VoronoiOutput Output = fractal_voronoi_x_fx(params, float4(coord, w));
  outDistance = Output.Distance;
  outColor = float4(Output.Color, 1.0f);
  outPosition = Output.Position.xyz;
  outW = Output.Position.w;
}

void node_tex_voronoi_f2_4d(float3 coord,
                            float w,
                            float scale,
                            float detail,
                            float roughness,
                            float lacunarity,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            float normalize,
                            out float outDistance,
                            out float4 outColor,
                            out float3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F2)

  w *= scale;
  coord *= scale;

  params.max_distance = voronoi_distance(
                            float4(0.0f), float4(0.5f + 0.5f * params.randomness), params) *
                        2.0f;
  VoronoiOutput Output = fractal_voronoi_x_fx(params, float4(coord, w));
  outDistance = Output.Distance;
  outColor = float4(Output.Color, 1.0f);
  outPosition = Output.Position.xyz;
  outW = Output.Position.w;
}

void node_tex_voronoi_distance_to_edge_4d(float3 coord,
                                          float w,
                                          float scale,
                                          float detail,
                                          float roughness,
                                          float lacunarity,
                                          float smoothness,
                                          float exponent,
                                          float randomness,
                                          float metric,
                                          float normalize,
                                          out float outDistance,
                                          out float4 outColor,
                                          out float3 outPosition,
                                          out float outW,
                                          out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_DISTANCE_TO_EDGE)

  w *= scale;
  coord *= scale;

  params.max_distance = 0.5f + 0.5f * params.randomness;
  outDistance = fractal_voronoi_distance_to_edge(params, float4(coord, w));
}

void node_tex_voronoi_n_sphere_radius_4d(float3 coord,
                                         float w,
                                         float scale,
                                         float detail,
                                         float roughness,
                                         float lacunarity,
                                         float smoothness,
                                         float exponent,
                                         float randomness,
                                         float metric,
                                         float normalize,
                                         out float outDistance,
                                         out float4 outColor,
                                         out float3 outPosition,
                                         out float outW,
                                         out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_N_SPHERE_RADIUS)

  w *= scale;
  coord *= scale;

  outRadius = voronoi_n_sphere_radius(params, float4(coord, w));
}
