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

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct BMesh;
struct CCGElem;
struct CCGKey;
struct DMFlagMat;
struct GSet;
struct TableGSet;
struct MLoop;
struct MLoopCol;
struct MLoopTri;
struct MPoly;
struct MPropCol;
struct MVert;
struct Mesh;
struct PBVH;
struct SubdivCCG;
struct CustomData;

/* Buffers for drawing from PBVH grids. */
typedef struct GPU_PBVH_Buffers GPU_PBVH_Buffers;

/* Build must be called once before using the other functions, used every time
 * mesh topology changes. Threaded. */
GPU_PBVH_Buffers *GPU_pbvh_mesh_buffers_build(const struct MPoly *mpoly,
                                              const struct MLoop *mloop,
                                              const struct MLoopTri *looptri,
                                              const struct MVert *mvert,
                                              const int *face_indices,
                                              const int *sculpt_face_sets,
                                              const int face_indices_len,
                                              const struct Mesh *mesh);

GPU_PBVH_Buffers *GPU_pbvh_grid_buffers_build(int totgrid, unsigned int **grid_hidden);

GPU_PBVH_Buffers *GPU_pbvh_bmesh_buffers_build(bool smooth_shading);

/* Free part of data for update. Not thread safe, must run in OpenGL main thread. */
void GPU_pbvh_bmesh_buffers_update_free(GPU_PBVH_Buffers *buffers);
void GPU_pbvh_grid_buffers_update_free(GPU_PBVH_Buffers *buffers,
                                       const struct DMFlagMat *grid_flag_mats,
                                       const int *grid_indices);

/* Update mesh buffers without topology changes. Threaded. */
enum {
  GPU_PBVH_BUFFERS_SHOW_MASK = (1 << 1),
  GPU_PBVH_BUFFERS_SHOW_VCOL = (1 << 2),
  GPU_PBVH_BUFFERS_SHOW_SCULPT_FACE_SETS = (1 << 3),
};

void GPU_pbvh_mesh_buffers_update(GPU_PBVH_Buffers *buffers,
                                  const struct MVert *mvert,
                                  const float *vmask,
                                  const struct MLoopCol *vcol,
                                  const int *sculpt_face_sets,
                                  const int face_sets_color_seed,
                                  const int face_sets_color_default,
                                  const struct MPropCol *vtcol,
                                  const int update_flags);

void GPU_pbvh_update_attribute_names(struct CustomData *vdata,
                                     struct CustomData *ldata,
                                     bool need_full_render);

void GPU_pbvh_bmesh_buffers_update(GPU_PBVH_Buffers *buffers,
                                   struct BMesh *bm,
                                   struct TableGSet *bm_faces,
                                   struct TableGSet *bm_unique_verts,
                                   struct TableGSet *bm_other_verts,
                                   struct PBVHTriBuf *tribuf,
                                   const int update_flags,
                                   const int cd_vert_node_offset,
                                   int face_sets_color_seed,
                                   int face_sets_color_default,
                                   bool flat_vcol,
                                   short mat_nr);

void GPU_pbvh_grid_buffers_update(GPU_PBVH_Buffers *buffers,
                                  struct SubdivCCG *subdiv_ccg,
                                  struct CCGElem **grids,
                                  const struct DMFlagMat *grid_flag_mats,
                                  int *grid_indices,
                                  int totgrid,
                                  const int *sculpt_face_sets,
                                  const int face_sets_color_seed,
                                  const int face_sets_color_default,
                                  const struct CCGKey *key,
                                  const int update_flags);

/* Finish update. Not thread safe, must run in OpenGL main thread. */
void GPU_pbvh_buffers_update_flush(GPU_PBVH_Buffers *buffers);

/* Free buffers.  Not thread safe, must run in OpenGL main thread. */
void GPU_pbvh_buffers_free(GPU_PBVH_Buffers *buffers);

/* draw */
struct GPUBatch *GPU_pbvh_buffers_batch_get(GPU_PBVH_Buffers *buffers, bool fast, bool wires);

short GPU_pbvh_buffers_material_index_get(GPU_PBVH_Buffers *buffers);
bool GPU_pbvh_buffers_has_overlays(GPU_PBVH_Buffers *buffers);
float *GPU_pbvh_get_extra_matrix(GPU_PBVH_Buffers *buffers);

/** if need_full_render is false, only the active (not render!) vcol layer will
    be uploaded to GPU*/

void GPU_pbvh_need_full_render_set(bool state);
bool GPU_pbvh_need_full_render_get(void);

#ifdef __cplusplus
}
#endif
