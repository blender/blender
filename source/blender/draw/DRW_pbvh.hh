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
#include "BLI_set.hh"
#include "BLI_span.hh"

#include "BKE_ccg.h"

class PBVHAttrReq;
struct GPUBatch;
struct PBVHNode;
struct DMFlagMat;
struct Mesh;
struct MLoopTri;
struct CustomData;
struct SubdivCCG;
struct BMesh;
struct BMFace;

namespace blender::draw::pbvh {

struct PBVHBatches;

struct PBVH_GPU_Args {
  int pbvh_type;

  BMesh *bm;
  const Mesh *me;
  MutableSpan<float3> vert_positions;
  Span<int> corner_verts;
  Span<int> corner_edges;
  const CustomData *vert_data;
  const CustomData *loop_data;
  const CustomData *face_data;
  Span<float3> vert_normals;
  Span<float3> face_normals;

  const char *active_color;
  const char *render_color;

  int face_sets_color_seed;
  int face_sets_color_default;

  SubdivCCG *subdiv_ccg;
  Span<DMFlagMat> grid_flag_mats;
  Span<int> grid_indices;
  CCGKey ccg_key;
  Span<CCGElem *> grids;
  Span<const BLI_bitmap *> grid_hidden;

  Span<int> prim_indices;

  const bool *hide_poly;

  Span<MLoopTri> mlooptri;
  Span<int> looptri_faces;

  /* BMesh. */
  const Set<BMFace *, 0> *bm_faces;
  int cd_mask_layer;
};

void node_update(PBVHBatches *batches, const PBVH_GPU_Args &args);
void update_pre(PBVHBatches *batches, const PBVH_GPU_Args &args);

void node_gpu_flush(PBVHBatches *batches);
PBVHBatches *node_create(const PBVH_GPU_Args &args);
void node_free(PBVHBatches *batches);
GPUBatch *tris_get(PBVHBatches *batches,
                   Span<PBVHAttrReq> attrs,
                   const PBVH_GPU_Args &args,
                   bool do_coarse_grids);
GPUBatch *lines_get(PBVHBatches *batches,
                    Span<PBVHAttrReq> attrs,
                    const PBVH_GPU_Args &args,
                    bool do_coarse_grids);

}  // namespace blender::draw::pbvh
