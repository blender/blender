/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

float BLI_noise_hnoise(float noisesize, float x, float y, float z);
float BLI_noise_hnoisep(float noisesize, float x, float y, float z);
/**
 * Original turbulence functions.
 */
float BLI_noise_turbulence(float noisesize, float x, float y, float z, int nr);
/**
 * newnoise: generic noise & turbulence functions
 * to replace the above BLI_noise_hnoise/p & BLI_noise_turbulence/1.
 * This is done so different noise basis functions can be used.
 */
/**
 * newnoise: generic noise function for use with different `noisebasis`.
 */
float BLI_noise_generic_noise(
    float noisesize, float x, float y, float z, bool hard, int noisebasis);
/**
 * newnoise: generic turbulence function for use with different `noisebasis`.
 */
float BLI_noise_generic_turbulence(
    float noisesize, float x, float y, float z, int oct, bool hard, int noisebasis);

/* newnoise: musgrave functions */

/**
 * Procedural `fBm` evaluated at "point"; returns value stored in "value".
 *
 * \param H: is the fractal increment parameter.
 * \param lacunarity:  is the gap between successive frequencies.
 * \param octaves: is the number of frequencies in the `fBm`.
 */
float BLI_noise_mg_fbm(
    float x, float y, float z, float H, float lacunarity, float octaves, int noisebasis);
/**
 * Procedural multi-fractal evaluated at "point";
 * returns value stored in "value".
 *
 * \param H: determines the highest fractal dimension.
 * \param lacunarity: is gap between successive frequencies.
 * \param octaves: is the number of frequencies in the `fBm`.
 *
 * \note There used to be a parameter called `offset`, old docs read:
 * is the zero offset, which determines multi-fractality.
 */
float BLI_noise_mg_multi_fractal(
    float x, float y, float z, float H, float lacunarity, float octaves, int noisebasis);
/**
 * "Variable Lacunarity Noise"
 * A distorted variety of Perlin noise.
 */
float BLI_noise_mg_variable_lacunarity(
    float x, float y, float z, float distortion, int nbas1, int nbas2);
/**
 * Heterogeneous procedural terrain function: stats by altitude method.
 * Evaluated at "point"; returns value stored in "value".
 *
 * \param H: Determines the fractal dimension of the roughest areas.
 * \param lacunarity: Is the gap between successive frequencies.
 * \param octaves: Is the number of frequencies in the `fBm`.
 * \param offset: Raises the terrain from `sea level`.
 */
float BLI_noise_mg_hetero_terrain(float x,
                                  float y,
                                  float z,
                                  float H,
                                  float lacunarity,
                                  float octaves,
                                  float offset,
                                  int noisebasis);
/**
 * Hybrid additive/multiplicative multi-fractal terrain model.
 *
 * Some good parameter values to start with:
 *
 * \param H:      0.25
 * \param offset: 0.7
 */
float BLI_noise_mg_hybrid_multi_fractal(float x,
                                        float y,
                                        float z,
                                        float H,
                                        float lacunarity,
                                        float octaves,
                                        float offset,
                                        float gain,
                                        int noisebasis);
/**
 * Ridged multi-fractal terrain model.
 *
 * Some good parameter values to start with:
 *
 * \param H:      1.0
 * \param offset: 1.0
 * \param gain:   2.0
 */
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

/**
 * Not 'pure' Worley, but the results are virtually the same.
 * Returns distances in da and point coords in `pa`.
 */
void BLI_noise_voronoi(float x, float y, float z, float *da, float *pa, float me, int dtype);
/**
 * newnoise: BLI_noise_cell & BLI_noise_cell_v3 (for vector/point/color).
 * idem, signed.
 */
float BLI_noise_cell(float x, float y, float z);
/**
 * Returns a vector/point/color in `r_ca`, using point hash-array directly.
 */
void BLI_noise_cell_v3(float x, float y, float z, float r_ca[3]);

#ifdef __cplusplus
}
#endif
