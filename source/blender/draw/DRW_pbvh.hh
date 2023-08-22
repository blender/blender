/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

/* Needed for BKE_ccg.h. */
#include "BLI_assert.h"
#include "BLI_bitmap.h"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"

#include "BKE_ccg.h"
#include "BKE_dyntopo_set.hh"

#ifdef __cplusplus
extern "C" {
#endif

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
struct SubdivCCG;
struct BMesh;
struct BMVert;
struct BMFace;

struct PBVH_GPU_Args {
  int pbvh_type;

  BMesh *bm;
  const Mesh *me;
  blender::MutableSpan<blender::float3> vert_positions;
  blender::OffsetIndices<int> faces;
  blender::Span<int> corner_verts;
  blender::Span<int> corner_edges;
  int mesh_verts_num, mesh_faces_num, mesh_grids_num;
  const CustomData *vert_data;
  const CustomData *loop_data;
  const CustomData *face_data;
  blender::Span<blender::float3> vert_normals;
  blender::Span<blender::float3> face_normals;

  const char *active_color;
  const char *render_color;

  int face_sets_color_seed, face_sets_color_default;
  const int *face_sets; /* for PBVH_FACES and PBVH_GRIDS */

  SubdivCCG *subdiv_ccg;
  const DMFlagMat *grid_flag_mats;
  blender::Span<int> grid_indices;
  CCGKey ccg_key;
  CCGElem **grids;
  BLI_bitmap **grid_hidden;

  blender::Span<int> prim_indices;

  const bool *hide_poly;

  blender::Span<MLoopTri> mlooptri;
  blender::Span<int> looptri_faces;
  PBVHNode *node;

  /* Debug mode to show original coordinates instead of vertex positions. */
  bool show_orig;

  int cd_mask_layer;
  struct PBVHTriBuf *tribuf, *tri_buffers;
  int tot_tri_buffers, updategen;
  blender::Span<blender::float3> origco, origno;

  blender::bke::dyntopo::DyntopoSet<BMFace> *bm_faces;
  blender::bke::dyntopo::DyntopoSet<BMVert> *bm_unique_verts;
  blender::bke::dyntopo::DyntopoSet<BMVert> *bm_other_verts;
};

void DRW_pbvh_node_update(PBVHBatches *batches, const PBVH_GPU_Args &args);
void DRW_pbvh_update_pre(PBVHBatches *batches, const PBVH_GPU_Args &args);

void DRW_pbvh_node_gpu_flush(PBVHBatches *batches);
PBVHBatches *DRW_pbvh_node_create(const PBVH_GPU_Args &args);
void DRW_pbvh_node_free(PBVHBatches *batches);
GPUBatch *DRW_pbvh_tris_get(PBVHBatches *batches,
                            PBVHAttrReq *attrs,
                            int attrs_num,
                            const PBVH_GPU_Args &args,
                            int *r_prim_count,
                            bool do_coarse_grids);
GPUBatch *DRW_pbvh_lines_get(PBVHBatches *batches,
                             PBVHAttrReq *attrs,
                             int attrs_num,
                             const PBVH_GPU_Args &args,
                             int *r_prim_count,
                             bool do_coarse_grids);

#ifdef __cplusplus
}
#endif
