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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#ifndef __GPU_BUFFERS_H__
#define __GPU_BUFFERS_H__

#include <stddef.h>

struct BMesh;
struct CCGElem;
struct CCGKey;
struct DMFlagMat;
struct GSet;
struct MLoop;
struct MLoopTri;
struct MPoly;
struct MVert;
struct MLoopCol;
struct PBVH;

/* Buffers for drawing from PBVH grids. */
typedef struct GPU_PBVH_Buffers GPU_PBVH_Buffers;

/* build */
GPU_PBVH_Buffers *GPU_pbvh_mesh_buffers_build(const int (*face_vert_indices)[3],
                                              const struct MPoly *mpoly,
                                              const struct MLoop *mloop,
                                              const struct MLoopTri *looptri,
                                              const struct MVert *verts,
                                              const int *face_indices,
                                              const int face_indices_len);

GPU_PBVH_Buffers *GPU_pbvh_grid_buffers_build(int totgrid, unsigned int **grid_hidden);

GPU_PBVH_Buffers *GPU_pbvh_bmesh_buffers_build(bool smooth_shading);

/* update */

enum {
  GPU_PBVH_BUFFERS_SHOW_MASK = (1 << 1),
  GPU_PBVH_BUFFERS_SHOW_VCOL = (1 << 1),
};

void GPU_pbvh_mesh_buffers_update(GPU_PBVH_Buffers *buffers,
                                  const struct MVert *mvert,
                                  const int *vert_indices,
                                  int totvert,
                                  const float *vmask,
                                  const struct MLoopCol *vcol,
                                  const int (*face_vert_indices)[3],
                                  const int update_flags);

void GPU_pbvh_bmesh_buffers_update(GPU_PBVH_Buffers *buffers,
                                   struct BMesh *bm,
                                   struct GSet *bm_faces,
                                   struct GSet *bm_unique_verts,
                                   struct GSet *bm_other_verts,
                                   const int update_flags);

void GPU_pbvh_grid_buffers_update(GPU_PBVH_Buffers *buffers,
                                  struct CCGElem **grids,
                                  const struct DMFlagMat *grid_flag_mats,
                                  int *grid_indices,
                                  int totgrid,
                                  const struct CCGKey *key,
                                  const int update_flags);

/* draw */
struct GPUBatch *GPU_pbvh_buffers_batch_get(GPU_PBVH_Buffers *buffers, bool fast, bool wires);

short GPU_pbvh_buffers_material_index_get(GPU_PBVH_Buffers *buffers);

bool GPU_pbvh_buffers_has_mask(GPU_PBVH_Buffers *buffers);

void GPU_pbvh_buffers_free(GPU_PBVH_Buffers *buffers);

void GPU_pbvh_fix_linking(void);

#endif
