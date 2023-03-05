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

struct PBVHAttrReq;
struct GPUBatch;
struct PBVHNode;
struct PBVHBatches;
struct PBVHGPUFormat;
struct GSet;
struct DMFlagMat;
struct Mesh;
struct MLoopTri;
struct CustomData;
struct MLoop;
struct MPoly;
struct SubdivCCG;
struct BMesh;

struct PBVH_GPU_Args {
  int pbvh_type;

  BMesh *bm;
  const Mesh *me;
  const float (*vert_positions)[3];
  const MLoop *mloop;
  const MPoly *mpoly;
  int mesh_verts_num, mesh_faces_num, mesh_grids_num;
  CustomData *vdata, *ldata, *pdata;
  const float (*vert_normals)[3];

  const char *active_color;
  const char *render_color;

  int face_sets_color_seed, face_sets_color_default;
  int *face_sets; /* for PBVH_FACES and PBVH_GRIDS */

  SubdivCCG *subdiv_ccg;
  const DMFlagMat *grid_flag_mats;
  const int *grid_indices;
  CCGKey ccg_key;
  CCGElem **grids;
  void **gridfaces;
  BLI_bitmap **grid_hidden;

  int *prim_indices;
  int totprim;

  const bool *hide_poly;

  int node_verts_num;

  const MLoopTri *mlooptri;
  PBVHNode *node;

  /* BMesh. */
  GSet *bm_unique_vert, *bm_other_verts, *bm_faces;
  int cd_mask_layer;
};

void DRW_pbvh_node_update(PBVHBatches *batches, PBVH_GPU_Args *args);
void DRW_pbvh_update_pre(PBVHBatches *batches, PBVH_GPU_Args *args);

void DRW_pbvh_node_gpu_flush(PBVHBatches *batches);
PBVHBatches *DRW_pbvh_node_create(PBVH_GPU_Args *args);
void DRW_pbvh_node_free(PBVHBatches *batches);
GPUBatch *DRW_pbvh_tris_get(PBVHBatches *batches,
                            PBVHAttrReq *attrs,
                            int attrs_num,
                            PBVH_GPU_Args *args,
                            int *r_prim_count,
                            bool do_coarse_grids);
GPUBatch *DRW_pbvh_lines_get(PBVHBatches *batches,
                             PBVHAttrReq *attrs,
                             int attrs_num,
                             PBVH_GPU_Args *args,
                             int *r_prim_count,
                             bool do_coarse_grids);
