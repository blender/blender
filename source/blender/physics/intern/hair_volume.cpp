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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Janne Karhu, Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/hair_volume.c
 *  \ingroup bph
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_texture_types.h"

#include "BKE_effect.h"

#include "implicit.h"

/* ================ Volumetric Hair Interaction ================
 * adapted from
 * 
 * Volumetric Methods for Simulation and Rendering of Hair
 *     (Petrovic, Henne, Anderson, Pixar Technical Memo #06-08, Pixar Animation Studios)
 * 
 * as well as
 * 
 * "Detail Preserving Continuum Simulation of Straight Hair"
 *     (McAdams, Selle 2009)
 */

/* Note about array indexing:
 * Generally the arrays here are one-dimensional.
 * The relation between 3D indices and the array offset is
 *   offset = x + res_x * y + res_y * z
 */

/* TODO: This is an initial implementation and should be made much better in due time.
 * What should at least be implemented is a grid size parameter and a smoothing kernel
 * for bigger grids.
 */


static float I[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

BLI_INLINE int hair_grid_size(const int res[3])
{
	return res[0] * res[1] * res[2];
}

typedef struct HairGridVert {
	float velocity[3];
	float density;
	
	float velocity_smooth[3];
} HairGridVert;

typedef struct HairGrid {
	HairGridVert *verts;
	int res[3];
	float gmin[3], gmax[3];
	float cellsize, inv_cellsize;
	
	struct SimDebugData *debug_data;
} HairGrid;

#define HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, axis) ( min_ii( max_ii( (int)((vec[axis] - gmin[axis]) * scale), 0), res[axis]-2 ) )

BLI_INLINE int hair_grid_offset(const float vec[3], const int res[3], const float gmin[3], float scale)
{
	int i, j, k;
	i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
	j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
	k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
	return i + (j + k*res[1])*res[0];
}

BLI_INLINE int hair_grid_interp_weights(const int res[3], const float gmin[3], float scale, const float vec[3], float uvw[3])
{
	int i, j, k, offset;
	
	i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
	j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
	k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
	offset = i + (j + k*res[1])*res[0];
	
	uvw[0] = (vec[0] - gmin[0]) * scale - (float)i;
	uvw[1] = (vec[1] - gmin[1]) * scale - (float)j;
	uvw[2] = (vec[2] - gmin[2]) * scale - (float)k;
	
//	BLI_assert(0.0f <= uvw[0] && uvw[0] <= 1.0001f);
//	BLI_assert(0.0f <= uvw[1] && uvw[1] <= 1.0001f);
//	BLI_assert(0.0f <= uvw[2] && uvw[2] <= 1.0001f);
	
	return offset;
}

BLI_INLINE void hair_grid_interpolate(const HairGridVert *grid, const int res[3], const float gmin[3], float scale, const float vec[3],
                                      float *density, float velocity[3], float density_gradient[3], float velocity_gradient[3][3])
{
	HairGridVert data[8];
	float uvw[3], muvw[3];
	int res2 = res[1] * res[0];
	int offset;
	
	offset = hair_grid_interp_weights(res, gmin, scale, vec, uvw);
	muvw[0] = 1.0f - uvw[0];
	muvw[1] = 1.0f - uvw[1];
	muvw[2] = 1.0f - uvw[2];
	
	data[0] = grid[offset              ];
	data[1] = grid[offset            +1];
	data[2] = grid[offset     +res[0]  ];
	data[3] = grid[offset     +res[0]+1];
	data[4] = grid[offset+res2         ];
	data[5] = grid[offset+res2       +1];
	data[6] = grid[offset+res2+res[0]  ];
	data[7] = grid[offset+res2+res[0]+1];
	
	if (density) {
		*density = muvw[2]*( muvw[1]*( muvw[0]*data[0].density + uvw[0]*data[1].density )   +
		                      uvw[1]*( muvw[0]*data[2].density + uvw[0]*data[3].density ) ) +
		            uvw[2]*( muvw[1]*( muvw[0]*data[4].density + uvw[0]*data[5].density )   +
		                      uvw[1]*( muvw[0]*data[6].density + uvw[0]*data[7].density ) );
	}
	
	if (velocity) {
		int k;
		for (k = 0; k < 3; ++k) {
			velocity[k] = muvw[2]*( muvw[1]*( muvw[0]*data[0].velocity[k] + uvw[0]*data[1].velocity[k] )   +
			                         uvw[1]*( muvw[0]*data[2].velocity[k] + uvw[0]*data[3].velocity[k] ) ) +
			               uvw[2]*( muvw[1]*( muvw[0]*data[4].velocity[k] + uvw[0]*data[5].velocity[k] )   +
			                         uvw[1]*( muvw[0]*data[6].velocity[k] + uvw[0]*data[7].velocity[k] ) );
		}
	}
	
	if (density_gradient) {
		density_gradient[0] = muvw[1] * muvw[2] * ( data[0].density - data[1].density ) +
		                       uvw[1] * muvw[2] * ( data[2].density - data[3].density ) +
		                      muvw[1] *  uvw[2] * ( data[4].density - data[5].density ) +
		                       uvw[1] *  uvw[2] * ( data[6].density - data[7].density );
		
		density_gradient[1] = muvw[2] * muvw[0] * ( data[0].density - data[2].density ) +
		                       uvw[2] * muvw[0] * ( data[4].density - data[6].density ) +
		                      muvw[2] *  uvw[0] * ( data[1].density - data[3].density ) +
		                       uvw[2] *  uvw[0] * ( data[5].density - data[7].density );
		
		density_gradient[2] = muvw[2] * muvw[0] * ( data[0].density - data[4].density ) +
		                       uvw[2] * muvw[0] * ( data[1].density - data[5].density ) +
		                      muvw[2] *  uvw[0] * ( data[2].density - data[6].density ) +
		                       uvw[2] *  uvw[0] * ( data[3].density - data[7].density );
	}
	
	if (velocity_gradient) {
		/* XXX TODO */
		zero_m3(velocity_gradient);
	}
}

void BPH_hair_volume_vertex_grid_forces(HairGrid *grid, const float x[3], const float v[3],
                                        float smoothfac, float pressurefac, float minpressure,
                                        float f[3], float dfdx[3][3], float dfdv[3][3])
{
	float gdensity, gvelocity[3], ggrad[3], gvelgrad[3][3], gradlen;
	
	hair_grid_interpolate(grid->verts, grid->res, grid->gmin, grid->inv_cellsize, x, &gdensity, gvelocity, ggrad, gvelgrad);
	
	zero_v3(f);
	sub_v3_v3(gvelocity, v);
	mul_v3_v3fl(f, gvelocity, smoothfac);
	
	gradlen = normalize_v3(ggrad) - minpressure;
	if (gradlen > 0.0f) {
		mul_v3_fl(ggrad, gradlen);
		madd_v3_v3fl(f, ggrad, pressurefac);
	}
	
	zero_m3(dfdx);
	
	sub_m3_m3m3(dfdv, gvelgrad, I);
	mul_m3_fl(dfdv, smoothfac);
}

void BPH_hair_volume_grid_interpolate(HairGrid *grid, const float x[3],
                                      float *density, float velocity[3], float density_gradient[3], float velocity_gradient[3][3])
{
	hair_grid_interpolate(grid->verts, grid->res, grid->gmin, grid->inv_cellsize, x, density, velocity, density_gradient, velocity_gradient);
}

void BPH_hair_volume_grid_velocity(HairGrid *grid, const float x[3], const float v[3],
                                   float fluid_factor,
                                   float r_v[3])
{
	float gdensity, gvelocity[3], ggrad[3], gvelgrad[3][3];
	
	hair_grid_interpolate(grid->verts, grid->res, grid->gmin, grid->inv_cellsize, x, &gdensity, gvelocity, ggrad, gvelgrad);
	
	/* XXX TODO implement FLIP method and use fluid_factor to blend between FLIP and PIC */
	copy_v3_v3(r_v, gvelocity);
}

BLI_INLINE bool hair_grid_point_valid(const float vec[3], float gmin[3], float gmax[3])
{
	return !(vec[0] < gmin[0] || vec[1] < gmin[1] || vec[2] < gmin[2] ||
	         vec[0] > gmax[0] || vec[1] > gmax[1] || vec[2] > gmax[2]);
}

BLI_INLINE float dist_tent_v3f3(const float a[3], float x, float y, float z)
{
	float w = (1.0f - fabsf(a[0] - x)) * (1.0f - fabsf(a[1] - y)) * (1.0f - fabsf(a[2] - z));
	return w;
}

BLI_INLINE float weights_sum(const float weights[8])
{
	float totweight = 0.0f;
	int i;
	for (i = 0; i < 8; ++i)
		totweight += weights[i];
	return totweight;
}

/* returns the grid array offset as well to avoid redundant calculation */
BLI_INLINE int hair_grid_weights(const int res[3], const float gmin[3], float scale, const float vec[3], float weights[8])
{
	int i, j, k, offset;
	float uvw[3];
	
	i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
	j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
	k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
	offset = i + (j + k*res[1])*res[0];
	
	uvw[0] = (vec[0] - gmin[0]) * scale;
	uvw[1] = (vec[1] - gmin[1]) * scale;
	uvw[2] = (vec[2] - gmin[2]) * scale;
	
	weights[0] = dist_tent_v3f3(uvw, (float)i    , (float)j    , (float)k    );
	weights[1] = dist_tent_v3f3(uvw, (float)(i+1), (float)j    , (float)k    );
	weights[2] = dist_tent_v3f3(uvw, (float)i    , (float)(j+1), (float)k    );
	weights[3] = dist_tent_v3f3(uvw, (float)(i+1), (float)(j+1), (float)k    );
	weights[4] = dist_tent_v3f3(uvw, (float)i    , (float)j    , (float)(k+1));
	weights[5] = dist_tent_v3f3(uvw, (float)(i+1), (float)j    , (float)(k+1));
	weights[6] = dist_tent_v3f3(uvw, (float)i    , (float)(j+1), (float)(k+1));
	weights[7] = dist_tent_v3f3(uvw, (float)(i+1), (float)(j+1), (float)(k+1));
	
//	BLI_assert(fabsf(weights_sum(weights) - 1.0f) < 0.0001f);
	
	return offset;
}

void BPH_hair_volume_add_vertex(HairGrid *grid, const float x[3], const float v[3])
{
	const int res[3] = { grid->res[0], grid->res[1], grid->res[2] };
	float weights[8];
	int di, dj, dk;
	int offset;
	
	if (!hair_grid_point_valid(x, grid->gmin, grid->gmax))
		return;
	
	offset = hair_grid_weights(res, grid->gmin, grid->inv_cellsize, x, weights);
	
	for (di = 0; di < 2; ++di) {
		for (dj = 0; dj < 2; ++dj) {
			for (dk = 0; dk < 2; ++dk) {
				int voffset = offset + di + (dj + dk*res[1])*res[0];
				int iw = di + dj*2 + dk*4;
				
				grid->verts[voffset].density += weights[iw];
				madd_v3_v3fl(grid->verts[voffset].velocity, v, weights[iw]);
			}
		}
	}
}

BLI_INLINE void hair_volume_eval_grid_vertex(HairGridVert *vert, const float loc[3], float radius, float dist_scale,
                                             const float x2[3], const float v2[3], const float x3[3], const float v3[3])
{
	float closest[3], lambda, dist, weight;
	
	lambda = closest_to_line_v3(closest, loc, x2, x3);
	dist = len_v3v3(closest, loc);
	
	weight = (radius - dist) * dist_scale;
	
	if (weight > 0.0f) {
		float vel[3];
		
		interp_v3_v3v3(vel, v2, v3, lambda);
		madd_v3_v3fl(vert->velocity, vel, weight);
		vert->density += weight;
	}
}

BLI_INLINE int major_axis_v3(const float v[3])
{
	return v[0] > v[1] ? (v[0] > v[2] ? 0 : 2) : (v[1] > v[2] ? 1 : 2);
}

BLI_INLINE void grid_to_world(HairGrid *grid, float vecw[3], const float vec[3])
{
	copy_v3_v3(vecw, vec);
	mul_v3_fl(vecw, grid->cellsize);
	add_v3_v3(vecw, grid->gmin);
}

/* Uses a variation of Bresenham's algorithm for rasterizing a 3D grid with a line segment.
 * 
 * The radius of influence around a segment is assumed to be at most 2*cellsize,
 * i.e. only cells containing the segment and their direct neighbors are examined.
 * 
 * 
 */
void BPH_hair_volume_add_segment(HairGrid *grid,
                                 const float UNUSED(x1[3]), const float UNUSED(v1[3]), const float x2[3], const float v2[3],
                                 const float x3[3], const float v3[3], const float UNUSED(x4[3]), const float UNUSED(v4[3]),
                                 const float UNUSED(dir1[3]), const float dir2[3], const float UNUSED(dir3[3]))
{
	SimDebugData *debug_data = grid->debug_data;
	
	const int res[3] = { grid->res[0], grid->res[1], grid->res[2] };
	
	/* find the primary direction from the major axis of the direction vector */
	const int axis0 = major_axis_v3(dir2);
	const int axis1 = (axis0 + 1) % 3;
	const int axis2 = (axis0 + 2) % 3;
	
	/* range along primary direction */
	const float h2 = x2[axis0], h3 = x3[axis0];
	const float hmin = min_ff(h2, h3);
	const float hmax = max_ff(h2, h3);
	const int imin = max_ii((int)hmin, 0);
	const int imax = min_ii((int)hmax + 1, res[axis0]);
	
	const float inc[2] = { dir2[axis1], dir2[axis2] };          /* increment of secondary directions per step in the primary direction */
	const int grid_start1 = (int)x2[axis1];                     /* offset of cells on minor axes */
	const int grid_start2 = (int)x2[axis2];                     /* offset of cells on minor axes */
	
	const float cellsize = grid->cellsize;
	float shift[2] = { x2[axis1] - floorf(x2[axis1]),           /* fraction of a full cell shift [0.0, 1.0) */
	                   x2[axis2] - floorf(x2[axis2]) };
	
	/* vertex buffer offset factors along cardinal axes */
	const int strides[3] = { 1, res[0], res[0] * res[1] };
	/* change in offset when incrementing one of the axes */
	const int stride0 = strides[axis0];
	const int stride1 = strides[axis1];
	const int stride2 = strides[axis2];
	
	const float radius = 1.5f;
	/* XXX cell size should be fixed and uniform! */
	const float dist_scale = grid->inv_cellsize;
	
	HairGridVert *vert0;
	float loc0[3];
	int j0, k0;
	int i;
	
	(void)debug_data;
	
	j0 = grid_start1 - 1;
	k0 = grid_start2 - 1;
	vert0 = grid->verts + stride0 * imin + stride1 * j0 + stride2 * k0;
	loc0[axis0] = (float)imin;
	loc0[axis1] = (float)j0;
	loc0[axis2] = (float)k0;
	
	/* loop over all planes crossed along the primary direction */
	for (i = imin; i < imax; ++i, vert0 += stride0, loc0[axis0] += cellsize) {
		const int jmin = max_ii(j0, 0);
		const int jmax = min_ii(j0 + 5, res[axis1]);
		const int kmin = max_ii(k0, 0);
		const int kmax = min_ii(k0 + 5, res[axis2]);
		
		/* XXX problem: this can be offset beyond range of this plane when jmin/kmin gets clamped,
		 * for now simply calculate in outer loop with multiplication once
		 */
//		HairGridVert *vert1 = vert0;
//		float loc1[3] = { loc0[0], loc0[1], loc0[2] };
		HairGridVert *vert1 = grid->verts + stride0 * i + stride1 * jmin + stride2 * kmin;
		float loc1[3];
		int j, k;
		
		/* note: loc is in grid cell units,
		 * distances are be scaled by cell size for weighting
		 */
		loc1[axis0] = (float)i;
		loc1[axis1] = (float)jmin;
		loc1[axis2] = (float)kmin;
		
		/* 2x2 cells can be hit directly by the segment between two planes,
		 * margin is 1 cell, i.e. 4x4 cells are influenced at most,
		 * -> evaluate 5x5 grid vertices on cell borders
		 */
		
		for (j = jmin; j < jmax; ++j, vert1 += stride1, loc1[axis1] += 1.0f) {
			HairGridVert *vert2 = vert1;
			float loc2[3] = { loc1[0], loc1[1], loc1[2] };
			
			for (k = kmin; k < kmax; ++k, vert2 += stride2, loc2[axis2] += 1.0f) {
				hair_volume_eval_grid_vertex(vert2, loc2, radius, dist_scale, x2, v2, x3, v3);
			}
		}
		
		/* increment */
		add_v2_v2(shift, inc);
		if (shift[0] > 1.0f) {
			shift[0] -= 1.0f;
			
			j0 += 1;
			vert0 += stride1;
			loc0[axis1] += 1.0f;
		}
		else if (shift[0] < -1.0f) {
			shift[0] += 1.0f;
			
			j0 -= 1;
			vert0 -= stride1;
			loc0[axis1] -= 1.0f;
		}
		if (shift[1] > 1.0f) {
			shift[1] -= 1.0f;
			
			k0 += 1;
			vert0 += stride2;
			loc0[axis2] += 1.0f;
		}
		else if (shift[1] < -1.0f) {
			shift[1] += 1.0f;
			
			k0 -= 1;
			vert0 -= stride2;
			loc0[axis2] -= 1.0f;
		}
	}
}

void BPH_hair_volume_normalize_vertex_grid(HairGrid *grid)
{
	int i, size = hair_grid_size(grid->res);
	/* divide velocity with density */
	for (i = 0; i < size; i++) {
		float density = grid->verts[i].density;
		if (density > 0.0f)
			mul_v3_fl(grid->verts[i].velocity, 1.0f/density);
	}
}

#if 0 /* XXX weighting is incorrect, disabled for now */
/* Velocity filter kernel
 * See http://en.wikipedia.org/wiki/Filter_%28large_eddy_simulation%29
 */

BLI_INLINE void hair_volume_filter_box_convolute(HairVertexGrid *grid, float invD, const int kernel_size[3], int i, int j, int k)
{
	int res = grid->res;
	int p, q, r;
	int minp = max_ii(i - kernel_size[0], 0), maxp = min_ii(i + kernel_size[0], res-1);
	int minq = max_ii(j - kernel_size[1], 0), maxq = min_ii(j + kernel_size[1], res-1);
	int minr = max_ii(k - kernel_size[2], 0), maxr = min_ii(k + kernel_size[2], res-1);
	int offset, kernel_offset, kernel_dq, kernel_dr;
	HairGridVert *verts;
	float *vel_smooth;
	
	offset = i + (j + k*res)*res;
	verts = grid->verts;
	vel_smooth = verts[offset].velocity_smooth;
	
	kernel_offset = minp + (minq + minr*res)*res;
	kernel_dq = res;
	kernel_dr = res * res;
	for (r = minr; r <= maxr; ++r) {
		for (q = minq; q <= maxq; ++q) {
			for (p = minp; p <= maxp; ++p) {
				
				madd_v3_v3fl(vel_smooth, verts[kernel_offset].velocity, invD);
				
				kernel_offset += 1;
			}
			kernel_offset += kernel_dq;
		}
		kernel_offset += kernel_dr;
	}
}

void BPH_hair_volume_vertex_grid_filter_box(HairVertexGrid *grid, int kernel_size)
{
	int size = hair_grid_size(grid->res);
	int kernel_sizev[3] = {kernel_size, kernel_size, kernel_size};
	int tot;
	float invD;
	int i, j, k;
	
	if (kernel_size <= 0)
		return;
	
	tot = kernel_size * 2 + 1;
	invD = 1.0f / (float)(tot*tot*tot);
	
	/* clear values for convolution */
	for (i = 0; i < size; ++i) {
		zero_v3(grid->verts[i].velocity_smooth);
	}
	
	for (i = 0; i < grid->res; ++i) {
		for (j = 0; j < grid->res; ++j) {
			for (k = 0; k < grid->res; ++k) {
				hair_volume_filter_box_convolute(grid, invD, kernel_sizev, i, j, k);
			}
		}
	}
	
	/* apply as new velocity */
	for (i = 0; i < size; ++i) {
		copy_v3_v3(grid->verts[i].velocity, grid->verts[i].velocity_smooth);
	}
}
#endif

HairGrid *BPH_hair_volume_create_vertex_grid(float cellsize, const float gmin[3], const float gmax[3])
{
	float scale;
	float extent[3];
	int resmin[3], resmax[3], res[3];
	float gmin_margin[3], gmax_margin[3];
	int size;
	HairGrid *grid;
	int i;
	
	/* sanity check */
	if (cellsize <= 0.0f)
		cellsize = 1.0f;
	scale = 1.0f / cellsize;
	
	sub_v3_v3v3(extent, gmax, gmin);
	for (i = 0; i < 3; ++i) {
		resmin[i] = (int)(gmin[i] * scale);
		resmax[i] = (int)(gmax[i] * scale) + 1;
		
		/* add margin of 1 cell */
		resmin[i] -= 1;
		resmax[i] += 1;
		
		res[i] = resmax[i] - resmin[i];
		/* sanity check: avoid null-sized grid */
		if (res[i] < 3) {
			res[i] = 3;
			resmax[i] = resmin[i] + 3;
		}
		/* sanity check: avoid too large grid size */
		if (res[i] > MAX_HAIR_GRID_RES) {
			res[i] = MAX_HAIR_GRID_RES;
			resmax[i] = resmin[i] + MAX_HAIR_GRID_RES;
		}
		
		gmin_margin[i] = (float)resmin[i] * cellsize;
		gmax_margin[i] = (float)resmax[i] * cellsize;
	}
	size = hair_grid_size(res);
	
	grid = (HairGrid *)MEM_callocN(sizeof(HairGrid), "hair grid");
	grid->res[0] = res[0];
	grid->res[1] = res[1];
	grid->res[2] = res[2];
	copy_v3_v3(grid->gmin, gmin_margin);
	copy_v3_v3(grid->gmax, gmax_margin);
	grid->cellsize = cellsize;
	grid->inv_cellsize = scale;
	grid->verts = (HairGridVert *)MEM_mallocN(sizeof(HairGridVert) * size, "hair voxel data");

	/* initialize grid */
	for (i = 0; i < size; ++i) {
		zero_v3(grid->verts[i].velocity);
		grid->verts[i].density = 0.0f;
	}
	
	return grid;
}

void BPH_hair_volume_free_vertex_grid(HairGrid *grid)
{
	if (grid) {
		if (grid->verts)
			MEM_freeN(grid->verts);
		MEM_freeN(grid);
	}
}

void BPH_hair_volume_set_debug_data(HairGrid *grid, SimDebugData *debug_data)
{
	grid->debug_data = debug_data;
}

void BPH_hair_volume_grid_geometry(HairGrid *grid, float *cellsize, int res[3], float gmin[3], float gmax[3])
{
	if (cellsize) *cellsize = grid->cellsize;
	if (res) copy_v3_v3_int(res, grid->res);
	if (gmin) copy_v3_v3(gmin, grid->gmin);
	if (gmax) copy_v3_v3(gmax, grid->gmax);
}

#if 0
static HairGridVert *hair_volume_create_collision_grid(ClothModifierData *clmd, lfVector *lX, unsigned int numverts)
{
	int res = hair_grid_res;
	int size = hair_grid_size(res);
	HairGridVert *collgrid;
	ListBase *colliders;
	ColliderCache *col = NULL;
	float gmin[3], gmax[3], scale[3];
	/* 2.0f is an experimental value that seems to give good results */
	float collfac = 2.0f * clmd->sim_parms->collider_friction;
	unsigned int	v = 0;
	int	            i = 0;

	hair_volume_get_boundbox(lX, numverts, gmin, gmax);
	hair_grid_get_scale(res, gmin, gmax, scale);

	collgrid = MEM_mallocN(sizeof(HairGridVert) * size, "hair collider voxel data");

	/* initialize grid */
	for (i = 0; i < size; ++i) {
		zero_v3(collgrid[i].velocity);
		collgrid[i].density = 0.0f;
	}

	/* gather colliders */
	colliders = get_collider_cache(clmd->scene, NULL, NULL);
	if (colliders && collfac > 0.0f) {
		for (col = colliders->first; col; col = col->next) {
			MVert *loc0 = col->collmd->x;
			MVert *loc1 = col->collmd->xnew;
			float vel[3];
			float weights[8];
			int di, dj, dk;
			
			for (v=0; v < col->collmd->numverts; v++, loc0++, loc1++) {
				int offset;
				
				if (!hair_grid_point_valid(loc1->co, gmin, gmax))
					continue;
				
				offset = hair_grid_weights(res, gmin, scale, lX[v], weights);
				
				sub_v3_v3v3(vel, loc1->co, loc0->co);
				
				for (di = 0; di < 2; ++di) {
					for (dj = 0; dj < 2; ++dj) {
						for (dk = 0; dk < 2; ++dk) {
							int voffset = offset + di + (dj + dk*res)*res;
							int iw = di + dj*2 + dk*4;
							
							collgrid[voffset].density += weights[iw];
							madd_v3_v3fl(collgrid[voffset].velocity, vel, weights[iw]);
						}
					}
				}
			}
		}
	}
	free_collider_cache(&colliders);

	/* divide velocity with density */
	for (i = 0; i < size; i++) {
		float density = collgrid[i].density;
		if (density > 0.0f)
			mul_v3_fl(collgrid[i].velocity, 1.0f/density);
	}
	
	return collgrid;
}
#endif

bool BPH_hair_volume_get_texture_data(HairGrid *grid, VoxelData *vd)
{
	int totres, i;
	int depth;

	vd->resol[0] = grid->res[0];
	vd->resol[1] = grid->res[1];
	vd->resol[2] = grid->res[2];
	
	totres = hair_grid_size(grid->res);
	
	if (vd->hair_type == TEX_VD_HAIRVELOCITY) {
		depth = 4;
		vd->data_type = TEX_VD_RGBA_PREMUL;
	}
	else {
		depth = 1;
		vd->data_type = TEX_VD_INTENSITY;
	}
	
	if (totres > 0) {
		vd->dataset = (float *)MEM_mapallocN(sizeof(float) * depth * (totres), "hair volume texture data");
		
		for (i = 0; i < totres; ++i) {
			switch (vd->hair_type) {
				case TEX_VD_HAIRDENSITY:
					vd->dataset[i] = grid->verts[i].density;
					break;
				
				case TEX_VD_HAIRRESTDENSITY:
					vd->dataset[i] = 0.0f; // TODO
					break;
				
				case TEX_VD_HAIRVELOCITY: {
					vd->dataset[i + 0*totres] = grid->verts[i].velocity[0];
					vd->dataset[i + 1*totres] = grid->verts[i].velocity[1];
					vd->dataset[i + 2*totres] = grid->verts[i].velocity[2];
					vd->dataset[i + 3*totres] = len_v3(grid->verts[i].velocity);
					break;
				}
				case TEX_VD_HAIRENERGY:
					vd->dataset[i] = 0.0f; // TODO
					break;
			}
		}
	}
	else {
		vd->dataset = NULL;
	}
	
	return true;
}
