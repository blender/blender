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

/* Create a randomized hash from the given inputs. Contrary to hash functions in `BLI_hash.hh`
 * these functions produce better randomness but are more expensive to compute. */
uint32_t hash(uint32_t kx);
uint32_t hash(uint32_t kx, uint32_t ky);
uint32_t hash(uint32_t kx, uint32_t ky, uint32_t kz);
uint32_t hash(uint32_t kx, uint32_t ky, uint32_t kz, uint32_t kw);

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

}  // namespace blender::noise
