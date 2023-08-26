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

#pragma BLENDER_REQUIRE(gpu_shader_common_hash.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_material_voronoi.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_material_fractal_voronoi.glsl)

#define INITIALIZE_VORONOIPARAMS(FEATURE) \
  params.feature = FEATURE; \
  params.metric = int(metric); \
  params.scale = scale; \
  params.detail = clamp(detail, 0.0, 15.0); \
  params.roughness = clamp(roughness, 0.0, 1.0); \
  params.lacunarity = lacunarity; \
  params.smoothness = clamp(smoothness / 2.0, 0.0, 0.5); \
  params.exponent = exponent; \
  params.randomness = clamp(randomness, 0.0, 1.0); \
  params.max_distance = 0.0; \
  params.normalize = bool(normalize);

/* **** 1D Voronoi **** */

void node_tex_voronoi_f1_1d(vec3 coord,
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
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F1)

  w *= scale;

  params.max_distance = 0.5 + 0.5 * params.randomness;
  VoronoiOutput Output = fractal_voronoi_x_fx(params, w);
  outDistance = Output.Distance;
  outColor.xyz = Output.Color;
  outW = Output.Position.w;
}

void node_tex_voronoi_smooth_f1_1d(vec3 coord,
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
                                   out vec4 outColor,
                                   out vec3 outPosition,
                                   out float outW,
                                   out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_SMOOTH_F1)

  w *= scale;

  params.max_distance = 0.5 + 0.5 * params.randomness;
  VoronoiOutput Output = fractal_voronoi_x_fx(params, w);
  outDistance = Output.Distance;
  outColor.xyz = Output.Color;
  outW = Output.Position.w;
}

void node_tex_voronoi_f2_1d(vec3 coord,
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
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F2)

  w *= scale;

  params.max_distance = (0.5 + 0.5 * params.randomness) * 2.0;
  VoronoiOutput Output = fractal_voronoi_x_fx(params, w);
  outDistance = Output.Distance;
  outColor.xyz = Output.Color;
  outW = Output.Position.w;
}

void node_tex_voronoi_distance_to_edge_1d(vec3 coord,
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
                                          out vec4 outColor,
                                          out vec3 outPosition,
                                          out float outW,
                                          out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_DISTANCE_TO_EDGE)

  w *= scale;

  params.max_distance = 0.5 + 0.5 * params.randomness;
  outDistance = fractal_voronoi_distance_to_edge(params, w);
}

void node_tex_voronoi_n_sphere_radius_1d(vec3 coord,
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
                                         out vec4 outColor,
                                         out vec3 outPosition,
                                         out float outW,
                                         out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_N_SPHERE_RADIUS)

  w *= scale;

  outRadius = voronoi_n_sphere_radius(params, w);
}

/* **** 2D Voronoi **** */

void node_tex_voronoi_f1_2d(vec3 coord,
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
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F1)

  coord *= scale;

  params.max_distance = voronoi_distance(vec2(0.0), vec2(0.5 + 0.5 * params.randomness), params);
  VoronoiOutput Output = fractal_voronoi_x_fx(params, coord.xy);
  outDistance = Output.Distance;
  outColor.xyz = Output.Color;
  outPosition = Output.Position.xyz;
}

void node_tex_voronoi_smooth_f1_2d(vec3 coord,
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
                                   out vec4 outColor,
                                   out vec3 outPosition,
                                   out float outW,
                                   out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_SMOOTH_F1)

  coord *= scale;

  params.max_distance = voronoi_distance(vec2(0.0), vec2(0.5 + 0.5 * params.randomness), params);
  VoronoiOutput Output = fractal_voronoi_x_fx(params, coord.xy);
  outDistance = Output.Distance;
  outColor.xyz = Output.Color;
  outPosition = Output.Position.xyz;
}

void node_tex_voronoi_f2_2d(vec3 coord,
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
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F2)

  coord *= scale;

  params.max_distance = voronoi_distance(vec2(0.0), vec2(0.5 + 0.5 * params.randomness), params) *
                        2.0;
  VoronoiOutput Output = fractal_voronoi_x_fx(params, coord.xy);
  outDistance = Output.Distance;
  outColor.xyz = Output.Color;
  outPosition = Output.Position.xyz;
}

void node_tex_voronoi_distance_to_edge_2d(vec3 coord,
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
                                          out vec4 outColor,
                                          out vec3 outPosition,
                                          out float outW,
                                          out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_DISTANCE_TO_EDGE)

  coord *= scale;

  params.max_distance = 0.5 + 0.5 * params.randomness;
  outDistance = fractal_voronoi_distance_to_edge(params, coord.xy);
}

void node_tex_voronoi_n_sphere_radius_2d(vec3 coord,
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
                                         out vec4 outColor,
                                         out vec3 outPosition,
                                         out float outW,
                                         out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_N_SPHERE_RADIUS)

  coord *= scale;

  outRadius = voronoi_n_sphere_radius(params, coord.xy);
}

/* **** 3D Voronoi **** */

void node_tex_voronoi_f1_3d(vec3 coord,
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
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F1)

  coord *= scale;

  params.max_distance = voronoi_distance(vec3(0.0), vec3(0.5 + 0.5 * params.randomness), params);
  VoronoiOutput Output = fractal_voronoi_x_fx(params, coord);
  outDistance = Output.Distance;
  outColor.xyz = Output.Color;
  outPosition = Output.Position.xyz;
}

void node_tex_voronoi_smooth_f1_3d(vec3 coord,
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
                                   out vec4 outColor,
                                   out vec3 outPosition,
                                   out float outW,
                                   out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_SMOOTH_F1)

  coord *= scale;

  params.max_distance = voronoi_distance(vec3(0.0), vec3(0.5 + 0.5 * params.randomness), params);
  VoronoiOutput Output = fractal_voronoi_x_fx(params, coord);
  outDistance = Output.Distance;
  outColor.xyz = Output.Color;
  outPosition = Output.Position.xyz;
}

void node_tex_voronoi_f2_3d(vec3 coord,
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
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F2)

  coord *= scale;

  params.max_distance = voronoi_distance(vec3(0.0), vec3(0.5 + 0.5 * params.randomness), params) *
                        2.0;
  VoronoiOutput Output = fractal_voronoi_x_fx(params, coord);
  outDistance = Output.Distance;
  outColor.xyz = Output.Color;
  outPosition = Output.Position.xyz;
}

void node_tex_voronoi_distance_to_edge_3d(vec3 coord,
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
                                          out vec4 outColor,
                                          out vec3 outPosition,
                                          out float outW,
                                          out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_DISTANCE_TO_EDGE)

  coord *= scale;

  params.max_distance = 0.5 + 0.5 * params.randomness;
  outDistance = fractal_voronoi_distance_to_edge(params, coord);
}

void node_tex_voronoi_n_sphere_radius_3d(vec3 coord,
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
                                         out vec4 outColor,
                                         out vec3 outPosition,
                                         out float outW,
                                         out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_N_SPHERE_RADIUS)

  coord *= scale;

  outRadius = voronoi_n_sphere_radius(params, coord);
}

/* **** 4D Voronoi **** */

void node_tex_voronoi_f1_4d(vec3 coord,
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
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F1)

  w *= scale;
  coord *= scale;

  params.max_distance = voronoi_distance(vec4(0.0), vec4(0.5 + 0.5 * params.randomness), params);
  VoronoiOutput Output = fractal_voronoi_x_fx(params, vec4(coord, w));
  outDistance = Output.Distance;
  outColor.xyz = Output.Color;
  outPosition = Output.Position.xyz;
  outW = Output.Position.w;
}

void node_tex_voronoi_smooth_f1_4d(vec3 coord,
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
                                   out vec4 outColor,
                                   out vec3 outPosition,
                                   out float outW,
                                   out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_SMOOTH_F1)

  w *= scale;
  coord *= scale;

  params.max_distance = voronoi_distance(vec4(0.0), vec4(0.5 + 0.5 * params.randomness), params);
  VoronoiOutput Output = fractal_voronoi_x_fx(params, vec4(coord, w));
  outDistance = Output.Distance;
  outColor.xyz = Output.Color;
  outPosition = Output.Position.xyz;
  outW = Output.Position.w;
}

void node_tex_voronoi_f2_4d(vec3 coord,
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
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_F2)

  w *= scale;
  coord *= scale;

  params.max_distance = voronoi_distance(vec4(0.0), vec4(0.5 + 0.5 * params.randomness), params) *
                        2.0;
  VoronoiOutput Output = fractal_voronoi_x_fx(params, vec4(coord, w));
  outDistance = Output.Distance;
  outColor.xyz = Output.Color;
  outPosition = Output.Position.xyz;
  outW = Output.Position.w;
}

void node_tex_voronoi_distance_to_edge_4d(vec3 coord,
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
                                          out vec4 outColor,
                                          out vec3 outPosition,
                                          out float outW,
                                          out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_DISTANCE_TO_EDGE)

  w *= scale;
  coord *= scale;

  params.max_distance = 0.5 + 0.5 * params.randomness;
  outDistance = fractal_voronoi_distance_to_edge(params, vec4(coord, w));
}

void node_tex_voronoi_n_sphere_radius_4d(vec3 coord,
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
                                         out vec4 outColor,
                                         out vec3 outPosition,
                                         out float outW,
                                         out float outRadius)
{
  VoronoiParams params;

  INITIALIZE_VORONOIPARAMS(SHD_VORONOI_N_SPHERE_RADIUS)

  w *= scale;
  coord *= scale;

  outRadius = voronoi_n_sphere_radius(params, vec4(coord, w));
}
