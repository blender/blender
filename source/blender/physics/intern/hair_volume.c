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

#include "implicit.h"

/* ================ Volumetric Hair Interaction ================
 * adapted from
 *      Volumetric Methods for Simulation and Rendering of Hair
 *      by Lena Petrovic, Mark Henne and John Anderson
 *      Pixar Technical Memo #06-08, Pixar Animation Studios
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

static int hair_grid_size(int res)
{
	return res * res * res;
}

BLI_INLINE void hair_grid_get_scale(int res, const float gmin[3], const float gmax[3], float scale[3])
{
	sub_v3_v3v3(scale, gmax, gmin);
	mul_v3_fl(scale, 1.0f / (res-1));
}

typedef struct HairGridVert {
	float velocity[3];
	float density;
	
	float velocity_smooth[3];
} HairGridVert;

typedef struct HairVertexGrid {
	HairGridVert *verts;
	int res;
	float gmin[3], gmax[3];
	float scale[3];
} HairVertexGrid;

typedef struct HairColliderGrid {
	HairGridVert *verts;
	int res;
	float gmin[3], gmax[3];
	float scale[3];
} HairColliderGrid;

#define HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, axis) ( min_ii( max_ii( (int)((vec[axis] - gmin[axis]) / scale[axis]), 0), res-2 ) )

BLI_INLINE int hair_grid_offset(const float vec[3], int res, const float gmin[3], const float scale[3])
{
	int i, j, k;
	i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
	j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
	k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
	return i + (j + k*res)*res;
}

BLI_INLINE int hair_grid_interp_weights(int res, const float gmin[3], const float scale[3], const float vec[3], float uvw[3])
{
	int i, j, k, offset;
	
	i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
	j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
	k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
	offset = i + (j + k*res)*res;
	
	uvw[0] = (vec[0] - gmin[0]) / scale[0] - (float)i;
	uvw[1] = (vec[1] - gmin[1]) / scale[1] - (float)j;
	uvw[2] = (vec[2] - gmin[2]) / scale[2] - (float)k;
	
	return offset;
}

BLI_INLINE void hair_grid_interpolate(const HairGridVert *grid, int res, const float gmin[3], const float scale[3], const float vec[3],
                                      float *density, float velocity[3], float density_gradient[3], float velocity_gradient[3][3])
{
	HairGridVert data[8];
	float uvw[3], muvw[3];
	int res2 = res * res;
	int offset;
	
	offset = hair_grid_interp_weights(res, gmin, scale, vec, uvw);
	muvw[0] = 1.0f - uvw[0];
	muvw[1] = 1.0f - uvw[1];
	muvw[2] = 1.0f - uvw[2];
	
	data[0] = grid[offset           ];
	data[1] = grid[offset         +1];
	data[2] = grid[offset     +res  ];
	data[3] = grid[offset     +res+1];
	data[4] = grid[offset+res2      ];
	data[5] = grid[offset+res2    +1];
	data[6] = grid[offset+res2+res  ];
	data[7] = grid[offset+res2+res+1];
	
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

#if 0
static void hair_velocity_collision(const HairGridVert *collgrid, const float gmin[3], const float scale[3], float collfac,
                                    lfVector *lF, lfVector *lX, lfVector *lV, unsigned int numverts)
{
	int v;
	/* calculate forces */
	for (v = 0; v < numverts; v++) {
		int offset = hair_grid_offset(lX[v], hair_grid_res, gmin, scale);
		
		if (collgrid[offset].density > 0.0f) {
			lF[v][0] += collfac * (collgrid[offset].velocity[0] - lV[v][0]);
			lF[v][1] += collfac * (collgrid[offset].velocity[1] - lV[v][1]);
			lF[v][2] += collfac * (collgrid[offset].velocity[2] - lV[v][2]);
		}
	}
}
#endif

void BPH_hair_volume_vertex_grid_forces(HairVertexGrid *grid, const float x[3], const float v[3],
                                        float smoothfac, float pressurefac, float minpressure,
                                        float f[3], float dfdx[3][3], float dfdv[3][3])
{
	float gdensity, gvelocity[3], ggrad[3], gvelgrad[3][3], gradlen;
	
	hair_grid_interpolate(grid->verts, grid->res, grid->gmin, grid->scale, x, &gdensity, gvelocity, ggrad, gvelgrad);
	
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

/* returns the grid array offset as well to avoid redundant calculation */
static int hair_grid_weights(int res, const float gmin[3], const float scale[3], const float vec[3], float weights[8])
{
	int i, j, k, offset;
	float uvw[3];
	
	i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
	j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
	k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
	offset = i + (j + k*res)*res;
	
	uvw[0] = (vec[0] - gmin[0]) / scale[0];
	uvw[1] = (vec[1] - gmin[1]) / scale[1];
	uvw[2] = (vec[2] - gmin[2]) / scale[2];
	
	weights[0] = dist_tent_v3f3(uvw, (float)i    , (float)j    , (float)k    );
	weights[1] = dist_tent_v3f3(uvw, (float)(i+1), (float)j    , (float)k    );
	weights[2] = dist_tent_v3f3(uvw, (float)i    , (float)(j+1), (float)k    );
	weights[3] = dist_tent_v3f3(uvw, (float)(i+1), (float)(j+1), (float)k    );
	weights[4] = dist_tent_v3f3(uvw, (float)i    , (float)j    , (float)(k+1));
	weights[5] = dist_tent_v3f3(uvw, (float)(i+1), (float)j    , (float)(k+1));
	weights[6] = dist_tent_v3f3(uvw, (float)i    , (float)(j+1), (float)(k+1));
	weights[7] = dist_tent_v3f3(uvw, (float)(i+1), (float)(j+1), (float)(k+1));
	
	return offset;
}

void BPH_hair_volume_add_vertex(HairVertexGrid *grid, const float x[3], const float v[3])
{
	int res = grid->res;
	float weights[8];
	int di, dj, dk;
	int offset;
	
	if (!hair_grid_point_valid(x, grid->gmin, grid->gmax))
		return;
	
	offset = hair_grid_weights(res, grid->gmin, grid->scale, x, weights);
	
	for (di = 0; di < 2; ++di) {
		for (dj = 0; dj < 2; ++dj) {
			for (dk = 0; dk < 2; ++dk) {
				int voffset = offset + di + (dj + dk*res)*res;
				int iw = di + dj*2 + dk*4;
				
				grid->verts[voffset].density += weights[iw];
				madd_v3_v3fl(grid->verts[voffset].velocity, v, weights[iw]);
			}
		}
	}
}

void BPH_hair_volume_normalize_vertex_grid(HairVertexGrid *grid)
{
	int i, size = hair_grid_size(grid->res);
	/* divide velocity with density */
	for (i = 0; i < size; i++) {
		float density = grid->verts[i].density;
		if (density > 0.0f)
			mul_v3_fl(grid->verts[i].velocity, 1.0f/density);
	}
}

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

HairVertexGrid *BPH_hair_volume_create_vertex_grid(int res, const float gmin[3], const float gmax[3])
{
	int size = hair_grid_size(res);
	HairVertexGrid *grid;
	int	            i = 0;
	
	grid = MEM_callocN(sizeof(HairVertexGrid), "hair vertex grid");
	grid->res = res;
	copy_v3_v3(grid->gmin, gmin);
	copy_v3_v3(grid->gmax, gmax);
	hair_grid_get_scale(res, gmin, gmax, grid->scale);
	grid->verts = MEM_mallocN(sizeof(HairGridVert) * size, "hair voxel data");

	/* initialize grid */
	for (i = 0; i < size; ++i) {
		zero_v3(grid->verts[i].velocity);
		grid->verts[i].density = 0.0f;
	}
	
	return grid;
}

void BPH_hair_volume_free_vertex_grid(HairVertexGrid *grid)
{
	if (grid) {
		if (grid->verts)
			MEM_freeN(grid->verts);
		MEM_freeN(grid);
	}
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

#if 0
bool implicit_hair_volume_get_texture_data(Object *UNUSED(ob), ClothModifierData *clmd, ListBase *UNUSED(effectors), VoxelData *vd)
{
	lfVector *lX, *lV;
	HairGridVert *hairgrid/*, *collgrid*/;
	int numverts;
	int totres, i;
	int depth;

	if (!clmd->clothObject || !clmd->clothObject->implicit)
		return false;

	lX = clmd->clothObject->implicit->X;
	lV = clmd->clothObject->implicit->V;
	numverts = clmd->clothObject->numverts;

	hairgrid = hair_volume_create_hair_grid(clmd, lX, lV, numverts);
//	collgrid = hair_volume_create_collision_grid(clmd, lX, numverts);

	vd->resol[0] = hair_grid_res;
	vd->resol[1] = hair_grid_res;
	vd->resol[2] = hair_grid_res;
	
	totres = hair_grid_size(hair_grid_res);
	
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
					vd->dataset[i] = hairgrid[i].density;
					break;
				
				case TEX_VD_HAIRRESTDENSITY:
					vd->dataset[i] = 0.0f; // TODO
					break;
				
				case TEX_VD_HAIRVELOCITY:
					vd->dataset[i + 0*totres] = hairgrid[i].velocity[0];
					vd->dataset[i + 1*totres] = hairgrid[i].velocity[1];
					vd->dataset[i + 2*totres] = hairgrid[i].velocity[2];
					vd->dataset[i + 3*totres] = len_v3(hairgrid[i].velocity);
					break;
				
				case TEX_VD_HAIRENERGY:
					vd->dataset[i] = 0.0f; // TODO
					break;
			}
		}
	}
	else {
		vd->dataset = NULL;
	}
	
	MEM_freeN(hairgrid);
//	MEM_freeN(collgrid);
	
	return true;
}

#endif
