/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup draw
 */

#pragma once

/* Needed for BKE_ccg.h. */
#include "BLI_assert.h"
#include "BLI_bitmap.h"

#include "BKE_ccg.h"

#ifdef __cplusplus
extern "C" {
#endif

struct GPUViewport;
struct PBVHAttrReq;
struct GPUBatch;
struct PBVHNode;
struct GSet;
struct DMFlagMat;
struct Object;
struct Mesh;
struct MLoopTri;
struct CustomData;
struct MVert;
struct MEdge;
struct MLoop;
struct MPoly;
struct SubdivCCG;
struct BMesh;

typedef struct PBVHBatches PBVHBatches;

typedef struct PBVH_GPU_Args {
  int pbvh_type;

  struct BMesh *bm;
  const struct Mesh *me;
  const struct MVert *mvert;
  const struct MLoop *mloop;
  const struct MPoly *mpoly;
  int mesh_verts_num, mesh_faces_num, mesh_grids_num;
  struct CustomData *vdata, *ldata, *pdata;
  const float (*vert_normals)[3];

  const char *active_color;
  const char *render_color;

  int face_sets_color_seed, face_sets_color_default;
  int *face_sets; /* for PBVH_FACES and PBVH_GRIDS */

  struct SubdivCCG *subdiv_ccg;
  const struct DMFlagMat *grid_flag_mats;
  const int *grid_indices;
  CCGKey ccg_key;
  CCGElem **grids;
  void **gridfaces;
  BLI_bitmap **grid_hidden;

  int *prim_indices;
  int totprim;

  bool *hide_poly;

  int node_verts_num;

  const struct MLoopTri *mlooptri;
  struct PBVHNode *node;

  /* BMesh. */
  struct GSet *bm_unique_vert, *bm_other_verts, *bm_faces;
  int cd_mask_layer;
} PBVH_GPU_Args;

typedef struct PBVHGPUFormat PBVHGPUFormat;

void DRW_pbvh_node_update(PBVHBatches *batches, PBVH_GPU_Args *args);
void DRW_pbvh_update_pre(PBVHBatches *batches, PBVH_GPU_Args *args);

void DRW_pbvh_node_gpu_flush(PBVHBatches *batches);
struct PBVHBatches *DRW_pbvh_node_create(PBVH_GPU_Args *args);
void DRW_pbvh_node_free(PBVHBatches *batches);
struct GPUBatch *DRW_pbvh_tris_get(PBVHBatches *batches,
                                   struct PBVHAttrReq *attrs,
                                   int attrs_num,
                                   PBVH_GPU_Args *args,
                                   int *r_prim_count,
                                   bool do_coarse_grids);
struct GPUBatch *DRW_pbvh_lines_get(struct PBVHBatches *batches,
                                    struct PBVHAttrReq *attrs,
                                    int attrs_num,
                                    PBVH_GPU_Args *args,
                                    int *r_prim_count,
                                    bool do_coarse_grids);

#ifdef __cplusplus
}
#endif
