/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

namespace blender::noise {

/* -------------------------------------------------------------------- */
/** \name Hash Functions
 *
 * Create a randomized hash from the given inputs. Contrary to hash functions in `BLI_hash.hh`
 * these functions produce better randomness but are more expensive to compute.
 * \{ */

/* Hash integers to `uint32_t`. */

uint32_t hash(uint32_t kx);
uint32_t hash(uint32_t kx, uint32_t ky);
uint32_t hash(uint32_t kx, uint32_t ky, uint32_t kz);
uint32_t hash(uint32_t kx, uint32_t ky, uint32_t kz, uint32_t kw);

/* Hash floats to `uint32_t`. */

uint32_t hash_float(float kx);
uint32_t hash_float(float2 k);
uint32_t hash_float(float3 k);
uint32_t hash_float(float4 k);
uint32_t hash_float(const float4x4 &k);

/* Hash integers to `float` between 0 and 1. */

float hash_to_float(uint32_t kx);
float hash_to_float(uint32_t kx, uint32_t ky);
float hash_to_float(uint32_t kx, uint32_t ky, uint32_t kz);
float hash_to_float(uint32_t kx, uint32_t ky, uint32_t kz, uint32_t kw);

/* Hash floats to `float` between 0 and 1. */

float hash_float_to_float(float k);
float hash_float_to_float(float2 k);
float hash_float_to_float(float3 k);
float hash_float_to_float(float4 k);

float2 hash_float_to_float2(float2 k);
float2 hash_float_to_float2(float3 k);
float2 hash_float_to_float2(float4 k);

float3 hash_float_to_float3(float k);
float3 hash_float_to_float3(float2 k);
float3 hash_float_to_float3(float3 k);
float3 hash_float_to_float3(float4 k);

float4 hash_float_to_float4(float4 k);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Perlin Noise
 * \{ */

/* Perlin noise in the range [-1, 1]. */

float perlin_signed(float position);
float perlin_signed(float2 position);
float perlin_signed(float3 position);
float perlin_signed(float4 position);

/* Perlin noise in the range [0, 1]. */

float perlin(float position);
float perlin(float2 position);
float perlin(float3 position);
float perlin(float4 position);

/* Perlin fractal Brownian motion. */

template<typename T>
float perlin_fbm(T p, float detail, float roughness, float lacunarity, bool normalize);

/* Distorted fractal perlin noise. */

template<typename T>
float perlin_fractal_distorted(T position,
                               float detail,
                               float roughness,
                               float lacunarity,
                               float offset,
                               float gain,
                               float distortion,
                               int type,
                               bool normalize);

/* Distorted fractal perlin noise that outputs a float3. */

float3 perlin_float3_fractal_distorted(float position,
                                       float detail,
                                       float roughness,
                                       float lacunarity,
                                       float offset,
                                       float gain,
                                       float distortion,
                                       int type,
                                       bool normalize);
float3 perlin_float3_fractal_distorted(float2 position,
                                       float detail,
                                       float roughness,
                                       float lacunarity,
                                       float offset,
                                       float gain,
                                       float distortion,
                                       int type,
                                       bool normalize);
float3 perlin_float3_fractal_distorted(float3 position,
                                       float detail,
                                       float roughness,
                                       float lacunarity,
                                       float offset,
                                       float gain,
                                       float distortion,
                                       int type,
                                       bool normalize);
float3 perlin_float3_fractal_distorted(float4 position,
                                       float detail,
                                       float roughness,
                                       float lacunarity,
                                       float offset,
                                       float gain,
                                       float distortion,
                                       int type,
                                       bool normalize);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Voronoi Noise
 * \{ */

struct VoronoiParams {
  float scale;
  float detail;
  float roughness;
  float lacunarity;
  float smoothness;
  float exponent;
  float randomness;
  float max_distance;
  bool normalize;
  int feature;
  int metric;
};

struct VoronoiOutput {
  float distance = 0.0f;
  float3 color{0.0f, 0.0f, 0.0f};
  float4 position{0.0f, 0.0f, 0.0f, 0.0f};
};

/* ***** Distances ***** */

float voronoi_distance(const float a, const float b);
float voronoi_distance(const float2 a, const float2 b, const VoronoiParams &params);
float voronoi_distance(const float3 a, const float3 b, const VoronoiParams &params);
float voronoi_distance(const float4 a, const float4 b, const VoronoiParams &params);

/* **** 1D Voronoi **** */

float4 voronoi_position(const float coord);
VoronoiOutput voronoi_f1(const VoronoiParams &params, const float coord);
VoronoiOutput voronoi_smooth_f1(const VoronoiParams &params,
                                const float coord,
                                const bool calc_color);
VoronoiOutput voronoi_f2(const VoronoiParams &params, const float coord);
float voronoi_distance_to_edge(const VoronoiParams &params, const float coord);
float voronoi_n_sphere_radius(const VoronoiParams &params, const float coord);

/* **** 2D Voronoi **** */

float4 voronoi_position(const float2 coord);
VoronoiOutput voronoi_f1(const VoronoiParams &params, const float2 coord);
VoronoiOutput voronoi_smooth_f1(const VoronoiParams &params,
                                const float2 coord,
                                const bool calc_color);
VoronoiOutput voronoi_f2(const VoronoiParams &params, const float2 coord);
float voronoi_distance_to_edge(const VoronoiParams &params, const float2 coord);
float voronoi_n_sphere_radius(const VoronoiParams &params, const float2 coord);

/* **** 3D Voronoi **** */

float4 voronoi_position(const float3 coord);
VoronoiOutput voronoi_f1(const VoronoiParams &params, const float3 coord);
VoronoiOutput voronoi_smooth_f1(const VoronoiParams &params,
                                const float3 coord,
                                const bool calc_color);
VoronoiOutput voronoi_f2(const VoronoiParams &params, const float3 coord);
float voronoi_distance_to_edge(const VoronoiParams &params, const float3 coord);
float voronoi_n_sphere_radius(const VoronoiParams &params, const float3 coord);

/* **** 4D Voronoi **** */

float4 voronoi_position(const float4 coord);
VoronoiOutput voronoi_f1(const VoronoiParams &params, const float4 coord);
VoronoiOutput voronoi_smooth_f1(const VoronoiParams &params,
                                const float4 coord,
                                const bool calc_color);
VoronoiOutput voronoi_f2(const VoronoiParams &params, const float4 coord);
float voronoi_distance_to_edge(const VoronoiParams &params, const float4 coord);
float voronoi_n_sphere_radius(const VoronoiParams &params, const float4 coord);

/* Fractal Voronoi Noise */

template<typename T>
VoronoiOutput fractal_voronoi_x_fx(const VoronoiParams &params,
                                   const T coord,
                                   const bool calc_color);
template<typename T>
float fractal_voronoi_distance_to_edge(const VoronoiParams &params, const T coord);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gabor Noise
 * \{ */

void gabor(const float2 coordinates,
           const float scale,
           const float frequency,
           const float anisotropy,
           const float orientation,
           float *r_value,
           float *r_phase,
           float *r_intensity);

void gabor(const float3 coordinates,
           const float scale,
           const float frequency,
           const float anisotropy,
           const float3 orientation,
           float *r_value,
           float *r_phase,
           float *r_intensity);

/** \} */

}  // namespace blender::noise
