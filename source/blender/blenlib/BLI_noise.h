/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * 
 */

#ifndef __BLI_NOISE_H__
#define __BLI_NOISE_H__

/** \file BLI_noise.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

/* noise.h: */
float BLI_hnoise(float noisesize, float x, float y, float z);
float BLI_hnoisep(float noisesize, float x, float y, float z);
float BLI_turbulence(float noisesize, float x, float y, float z, int nr);
float BLI_turbulence1(float noisesize, float x, float y, float z, int nr);
/* newnoise: generic noise & turbulence functions to replace the above BLI_hnoise/p & BLI_turbulence/1.
 * This is done so different noise basis functions can be used */
float BLI_gNoise(float noisesize, float x, float y, float z, int hard, int noisebasis);
float BLI_gTurbulence(float noisesize, float x, float y, float z, int oct, int hard, int noisebasis);
/* newnoise: musgrave functions */
float mg_fBm(float x, float y, float z, float H, float lacunarity, float octaves, int noisebasis);
float mg_MultiFractal(float x, float y, float z, float H, float lacunarity, float octaves, int noisebasis);
float mg_VLNoise(float x, float y, float z, float distortion, int nbas1, int nbas2);
float mg_HeteroTerrain(float x, float y, float z, float H, float lacunarity, float octaves, float offset, int noisebasis);
float mg_HybridMultiFractal(float x, float y, float z, float H, float lacunarity, float octaves, float offset, float gain, int noisebasis);
float mg_RidgedMultiFractal(float x, float y, float z, float H, float lacunarity, float octaves, float offset, float gain, int noisebasis);
/* newnoise: voronoi */
void voronoi(float x, float y, float z, float *da, float *pa, float me, int dtype);
/* newnoise: cellNoise & cellNoiseV (for vector/point/color) */
float cellNoise(float x, float y, float z);
void cellNoiseV(float x, float y, float z, float r_ca[3]);

#ifdef __cplusplus
}
#endif

#endif
