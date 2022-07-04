/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include <stddef.h>

#include "BKE_attribute.h"
#include "BKE_pbvh.h"

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
struct MSculptVert;
struct MPoly;
struct MPropCol;
struct MVert;
struct Mesh;
struct PBVH;
struct SubdivCCG;
struct CustomData;

typedef struct PBVHGPUFormat PBVHGPUFormat;

/**
 * Buffers for drawing from PBVH grids.
 */
typedef struct GPU_PBVH_Buffers GPU_PBVH_Buffers;

typedef struct PBVHGPUBuildArgs {
  GPU_PBVH_Buffers *buffers;
  struct BMesh *bm;

  struct TableGSet *bm_faces;
  struct TableGSet *bm_unique_verts;
  struct TableGSet *bm_other_verts;

  struct PBVHTriBuf *tribuf;

  const int update_flags;
  const int cd_vert_node_offset;
  int face_sets_color_seed;
  int face_sets_color_default;
  bool flat_vcol;
  short mat_nr, active_vcol_type, active_vcol_domain;
  struct CustomDataLayer *active_vcol_layer;
  struct CustomDataLayer *render_vcol_layer;
} PBVHGPUBuildArgs;

/**
 * Build must be called once before using the other functions,
 * used every time mesh topology changes.
 *
 * Threaded: do not call any functions that use OpenGL calls!
 */
GPU_PBVH_Buffers *GPU_pbvh_mesh_buffers_build(PBVHGPUFormat *vbo_id,
                                              const struct MPoly *mpoly,
                                              const struct MLoop *mloop,
                                              const struct MLoopTri *looptri,
                                              const struct MVert *mvert,
                                              const int *face_indices,
                                              const int *sculpt_face_sets,
                                              int face_indices_len,
                                              const struct Mesh *mesh);

/**
 * Threaded: do not call any functions that use OpenGL calls!
 */
GPU_PBVH_Buffers *GPU_pbvh_grid_buffers_build(int totgrid,
                                              unsigned int **grid_hidden,
                                              bool smooth);

/**
 * Threaded: do not call any functions that use OpenGL calls!
 */
GPU_PBVH_Buffers *GPU_pbvh_bmesh_buffers_build(PBVHGPUFormat *vbo_id, bool smooth_shading);

/**
 * Free part of data for update. Not thread safe, must run in OpenGL main thread.
 */
void GPU_pbvh_bmesh_buffers_update_free(PBVHGPUFormat *vbo_id, GPU_PBVH_Buffers *buffers);
void GPU_pbvh_grid_buffers_update_free(PBVHGPUFormat *vbo_id,
                                       GPU_PBVH_Buffers *buffers,
                                       const struct DMFlagMat *grid_flag_mats,
                                       const int *grid_indices);

/**
 * Update mesh buffers without topology changes. Threaded.
 */
enum {
  GPU_PBVH_BUFFERS_SHOW_MASK = (1 << 1),
  GPU_PBVH_BUFFERS_SHOW_VCOL = (1 << 2),
  GPU_PBVH_BUFFERS_SHOW_SCULPT_FACE_SETS = (1 << 3),
};

/**
 * Creates a vertex buffer (coordinate, normal, color) and,
 * if smooth shading, an element index buffer.
 * Threaded: do not call any functions that use OpenGL calls!
 */
void GPU_pbvh_mesh_buffers_update(PBVHGPUFormat *vbo_id,
                                  GPU_PBVH_Buffers *buffers,
                                  const struct MVert *mvert,
                                  const CustomData *vdata,
                                  const CustomData *ldata,
                                  const float *vmask,
                                  const int *sculpt_face_sets,
                                  const int face_sets_color_seed,
                                  const int face_sets_color_default,
                                  const int update_flags,
                                  const float (*vert_normals)[3]);

bool GPU_pbvh_attribute_names_update(PBVHType pbvh_type,
                                     PBVHGPUFormat *vbo_id,
                                     const struct CustomData *vdata,
                                     const struct CustomData *ldata,
                                     bool active_attrs_only);

/**
 * Creates a vertex buffer (coordinate, normal, color) and,
 * if smooth shading, an element index buffer.
 * Threaded: do not call any functions that use OpenGL calls!
 */
void GPU_pbvh_bmesh_buffers_update(PBVHGPUFormat *vbo_id, PBVHGPUBuildArgs *args);

/**
 * Threaded: do not call any functions that use OpenGL calls!
 */
void GPU_pbvh_grid_buffers_update(PBVHGPUFormat *vbo_id,
                                  GPU_PBVH_Buffers *buffers,
                                  struct SubdivCCG *subdiv_ccg,
                                  struct CCGElem **grids,
                                  const struct DMFlagMat *grid_flag_mats,
                                  int *grid_indices,
                                  int totgrid,
                                  const int *sculpt_face_sets,
                                  int face_sets_color_seed,
                                  int face_sets_color_default,
                                  const struct CCGKey *key,
                                  int update_flags);

/**
 * Finish update. Not thread safe, must run in OpenGL main
 * thread.
 */
void GPU_pbvh_buffers_update_flush(GPU_PBVH_Buffers *buffers);

/**
 * Free buffers. Not thread safe, must run in OpenGL main thread.
 */
void GPU_pbvh_buffers_free(GPU_PBVH_Buffers *buffers);

/** Draw. */
struct GPUBatch *GPU_pbvh_buffers_batch_get(GPU_PBVH_Buffers *buffers, bool fast, bool wires);

short GPU_pbvh_buffers_material_index_get(GPU_PBVH_Buffers *buffers);
bool GPU_pbvh_buffers_has_overlays(GPU_PBVH_Buffers *buffers);
float *GPU_pbvh_get_extra_matrix(GPU_PBVH_Buffers *buffers);

/** if need_full_render is false, only the active (not render!)
   vcol layer will be uploaded to GPU*/

void GPU_pbvh_need_full_render_set(PBVHGPUFormat *vbo_id, bool state);
bool GPU_pbvh_need_full_render_get(PBVHGPUFormat *vbo_id);

PBVHGPUFormat *GPU_pbvh_make_format(void);
void GPU_pbvh_free_format(PBVHGPUFormat *vbo_id);

#ifdef __cplusplus
}
#endif
