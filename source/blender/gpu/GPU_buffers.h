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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_buffers.h
 *  \ingroup gpu
 */

#ifndef __GPU_BUFFERS_H__
#define __GPU_BUFFERS_H__

#include <stddef.h>

struct BMesh;
struct CCGElem;
struct CCGKey;
struct DMFlagMat;
struct GridCommonGPUBuffer;
struct GSet;
struct MLoop;
struct MLoopTri;
struct MPoly;
struct MVert;
struct PBVH;

/* Buffers for non-DerivedMesh drawing */
typedef struct GPU_PBVH_Buffers GPU_PBVH_Buffers;

/* build */
GPU_PBVH_Buffers *GPU_pbvh_mesh_buffers_build(
        const int (*face_vert_indices)[3],
        const struct MPoly *mpoly, const struct MLoop *mloop, const struct MLoopTri *looptri,
        const struct MVert *verts,
        const int *face_indices,
        const int  face_indices_len);

GPU_PBVH_Buffers *GPU_pbvh_grid_buffers_build(
        int *grid_indices, int totgrid, unsigned int **grid_hidden, int gridsize, const struct CCGKey *key,
        struct GridCommonGPUBuffer **grid_common_gpu_buffer);

GPU_PBVH_Buffers *GPU_pbvh_bmesh_buffers_build(bool smooth_shading);

/* update */

enum {
	GPU_PBVH_BUFFERS_SHOW_DIFFUSE_COLOR = (1 << 0),
	GPU_PBVH_BUFFERS_SHOW_MASK = (1 << 1),
};

void GPU_pbvh_mesh_buffers_update(
        GPU_PBVH_Buffers *buffers, const struct MVert *mvert,
        const int *vert_indices, int totvert, const float *vmask,
        const int (*face_vert_indices)[3],
        const int update_flags);

void GPU_pbvh_bmesh_buffers_update(
        GPU_PBVH_Buffers *buffers,
        struct BMesh *bm,
        struct GSet *bm_faces,
        struct GSet *bm_unique_verts,
        struct GSet *bm_other_verts,
        const int update_flags);

void GPU_pbvh_grid_buffers_update(
        GPU_PBVH_Buffers *buffers, struct CCGElem **grids,
        const struct DMFlagMat *grid_flag_mats,
        int *grid_indices, int totgrid, const struct CCGKey *key,
        const int update_flags);

/* draw */
struct GPUBatch *GPU_pbvh_buffers_batch_get(GPU_PBVH_Buffers *buffers, bool fast);

/* debug PBVH draw */
void GPU_pbvh_BB_draw(float min[3], float max[3], bool leaf, unsigned int pos);

bool GPU_pbvh_buffers_diffuse_changed(GPU_PBVH_Buffers *buffers, struct GSet *bm_faces, bool show_diffuse_color);
bool GPU_pbvh_buffers_mask_changed(GPU_PBVH_Buffers *buffers, bool show_mask);

void GPU_pbvh_buffers_free(GPU_PBVH_Buffers *buffers);
void GPU_pbvh_multires_buffers_free(struct GridCommonGPUBuffer **grid_common_gpu_buffer);

void GPU_pbvh_fix_linking(void);

#endif
