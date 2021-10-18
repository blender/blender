/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_float4.hh"

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

/* Fractal perlin noise in the range [0, 1]. */
float perlin_fractal(float position, float octaves, float roughness);
float perlin_fractal(float2 position, float octaves, float roughness);
float perlin_fractal(float3 position, float octaves, float roughness);
float perlin_fractal(float4 position, float octaves, float roughness);

/* Positive distorted fractal perlin noise. */
float perlin_fractal_distorted(float position, float octaves, float roughness, float distortion);
float perlin_fractal_distorted(float2 position, float octaves, float roughness, float distortion);
float perlin_fractal_distorted(float3 position, float octaves, float roughness, float distortion);
float perlin_fractal_distorted(float4 position, float octaves, float roughness, float distortion);

/* Positive distorted fractal perlin noise that outputs a float3. */
float3 perlin_float3_fractal_distorted(float position,
                                       float octaves,
                                       float roughness,
                                       float distortion);
float3 perlin_float3_fractal_distorted(float2 position,
                                       float octaves,
                                       float roughness,
                                       float distortion);
float3 perlin_float3_fractal_distorted(float3 position,
                                       float octaves,
                                       float roughness,
                                       float distortion);
float3 perlin_float3_fractal_distorted(float4 position,
                                       float octaves,
                                       float roughness,
                                       float distortion);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Musgrave Multi Fractal
 * \{ */

float musgrave_ridged_multi_fractal(const float co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves,
                                    const float offset,
                                    const float gain);
float musgrave_ridged_multi_fractal(const float2 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves,
                                    const float offset,
                                    const float gain);
float musgrave_ridged_multi_fractal(const float3 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves,
                                    const float offset,
                                    const float gain);
float musgrave_ridged_multi_fractal(const float4 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves,
                                    const float offset,
                                    const float gain);

float musgrave_hybrid_multi_fractal(const float co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves,
                                    const float offset,
                                    const float gain);
float musgrave_hybrid_multi_fractal(const float2 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves,
                                    const float offset,
                                    const float gain);
float musgrave_hybrid_multi_fractal(const float3 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves,
                                    const float offset,
                                    const float gain);
float musgrave_hybrid_multi_fractal(const float4 co,
                                    const float H,
                                    const float lacunarity,
                                    const float octaves,
                                    const float offset,
                                    const float gain);

float musgrave_fBm(const float co, const float H, const float lacunarity, const float octaves);
float musgrave_fBm(const float2 co, const float H, const float lacunarity, const float octaves);
float musgrave_fBm(const float3 co, const float H, const float lacunarity, const float octaves);
float musgrave_fBm(const float4 co, const float H, const float lacunarity, const float octaves);

float musgrave_multi_fractal(const float co,
                             const float H,
                             const float lacunarity,
                             const float octaves);
float musgrave_multi_fractal(const float2 co,
                             const float H,
                             const float lacunarity,
                             const float octaves);
float musgrave_multi_fractal(const float3 co,
                             const float H,
                             const float lacunarity,
                             const float octaves);
float musgrave_multi_fractal(const float4 co,
                             const float H,
                             const float lacunarity,
                             const float octaves);

float musgrave_hetero_terrain(const float co,
                              const float H,
                              const float lacunarity,
                              const float octaves,
                              const float offset);
float musgrave_hetero_terrain(const float2 co,
                              const float H,
                              const float lacunarity,
                              const float octaves,
                              const float offset);
float musgrave_hetero_terrain(const float3 co,
                              const float H,
                              const float lacunarity,
                              const float octaves,
                              const float offset);
float musgrave_hetero_terrain(const float4 co,
                              const float H,
                              const float lacunarity,
                              const float octaves,
                              const float offset);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Voronoi Noise
 * \{ */

void voronoi_f1(
    const float w, const float randomness, float *r_distance, float3 *r_color, float *r_w);
void voronoi_smooth_f1(const float w,
                       const float smoothness,
                       const float randomness,
                       float *r_distance,
                       float3 *r_color,
                       float *r_w);
void voronoi_f2(
    const float w, const float randomness, float *r_distance, float3 *r_color, float *r_w);
void voronoi_distance_to_edge(const float w, const float randomness, float *r_distance);
void voronoi_n_sphere_radius(const float w, const float randomness, float *r_radius);

void voronoi_f1(const float2 coord,
                const float exponent,
                const float randomness,
                const int metric,
                float *r_distance,
                float3 *r_color,
                float2 *r_position);
void voronoi_smooth_f1(const float2 coord,
                       const float smoothness,
                       const float exponent,
                       const float randomness,
                       const int metric,
                       float *r_distance,
                       float3 *r_color,
                       float2 *r_position);
void voronoi_f2(const float2 coord,
                const float exponent,
                const float randomness,
                const int metric,
                float *r_distance,
                float3 *r_color,
                float2 *r_position);
void voronoi_distance_to_edge(const float2 coord, const float randomness, float *r_distance);
void voronoi_n_sphere_radius(const float2 coord, const float randomness, float *r_radius);

void voronoi_f1(const float3 coord,
                const float exponent,
                const float randomness,
                const int metric,
                float *r_distance,
                float3 *r_color,
                float3 *r_position);
void voronoi_smooth_f1(const float3 coord,
                       const float smoothness,
                       const float exponent,
                       const float randomness,
                       const int metric,
                       float *r_distance,
                       float3 *r_color,
                       float3 *r_position);
void voronoi_f2(const float3 coord,
                const float exponent,
                const float randomness,
                const int metric,
                float *r_distance,
                float3 *r_color,
                float3 *r_position);
void voronoi_distance_to_edge(const float3 coord, const float randomness, float *r_distance);
void voronoi_n_sphere_radius(const float3 coord, const float randomness, float *r_radius);

void voronoi_f1(const float4 coord,
                const float exponent,
                const float randomness,
                const int metric,
                float *r_distance,
                float3 *r_color,
                float4 *r_position);
void voronoi_smooth_f1(const float4 coord,
                       const float smoothness,
                       const float exponent,
                       const float randomness,
                       const int metric,
                       float *r_distance,
                       float3 *r_color,
                       float4 *r_position);
void voronoi_f2(const float4 coord,
                const float exponent,
                const float randomness,
                const int metric,
                float *r_distance,
                float3 *r_color,
                float4 *r_position);
void voronoi_distance_to_edge(const float4 coord, const float randomness, float *r_distance);
void voronoi_n_sphere_radius(const float4 coord, const float randomness, float *r_radius);

/** \} */

}  // namespace blender::noise
