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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

float BLI_noise_hnoise(float noisesize, float x, float y, float z);
float BLI_noise_hnoisep(float noisesize, float x, float y, float z);
float BLI_noise_turbulence(float noisesize, float x, float y, float z, int nr);
/* newnoise: generic noise & turbulence functions
 * to replace the above BLI_noise_hnoise/p & BLI_noise_turbulence/1.
 * This is done so different noise basis functions can be used */
float BLI_noise_generic_noise(
    float noisesize, float x, float y, float z, bool hard, int noisebasis);
float BLI_noise_generic_turbulence(
    float noisesize, float x, float y, float z, int oct, bool hard, int noisebasis);
/* newnoise: musgrave functions */
float BLI_noise_mg_fbm(
    float x, float y, float z, float H, float lacunarity, float octaves, int noisebasis);
float BLI_noise_mg_multi_fractal(
    float x, float y, float z, float H, float lacunarity, float octaves, int noisebasis);
float BLI_noise_mg_variable_lacunarity(
    float x, float y, float z, float distortion, int nbas1, int nbas2);
float BLI_noise_mg_hetero_terrain(float x,
                                  float y,
                                  float z,
                                  float H,
                                  float lacunarity,
                                  float octaves,
                                  float offset,
                                  int noisebasis);
float BLI_noise_mg_hybrid_multi_fractal(float x,
                                        float y,
                                        float z,
                                        float H,
                                        float lacunarity,
                                        float octaves,
                                        float offset,
                                        float gain,
                                        int noisebasis);
float BLI_noise_mg_ridged_multi_fractal(float x,
                                        float y,
                                        float z,
                                        float H,
                                        float lacunarity,
                                        float octaves,
                                        float offset,
                                        float gain,
                                        int noisebasis);
/* newnoise: voronoi */
void BLI_noise_voronoi(float x, float y, float z, float *da, float *pa, float me, int dtype);
/* newnoise: BLI_noise_cell & BLI_noise_cell_v3 (for vector/point/color) */
float BLI_noise_cell(float x, float y, float z);
void BLI_noise_cell_v3(float x, float y, float z, float r_ca[3]);

#ifdef __cplusplus
}
#endif
