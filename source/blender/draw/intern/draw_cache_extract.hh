/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. */

/** \file
 * \ingroup draw
 */

#pragma once

#include <algorithm>

#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_view3d_enums.h"

#include "BKE_attribute.h"
#include "BKE_object.h"

#include "GPU_batch.h"
#include "GPU_index_buffer.h"
#include "GPU_vertex_buffer.h"

#include "draw_attributes.hh"

struct DRWSubdivCache;
struct MeshRenderData;
struct TaskGraph;

/* Vertex Group Selection and display options */
struct DRW_MeshWeightState {
  int defgroup_active;
  int defgroup_len;

  short flags;
  char alert_mode;

  /* Set of all selected bones for Multi-paint. */
  bool *defgroup_sel; /* #defgroup_len */
  int defgroup_sel_count;

  /* Set of all locked and unlocked deform bones for Lock Relative mode. */
  bool *defgroup_locked;   /* #defgroup_len */
  bool *defgroup_unlocked; /* #defgroup_len */
};

/* DRW_MeshWeightState.flags */
enum {
  DRW_MESH_WEIGHT_STATE_MULTIPAINT = (1 << 0),
  DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE = (1 << 1),
  DRW_MESH_WEIGHT_STATE_LOCK_RELATIVE = (1 << 2),
};

enum eMRIterType {
  MR_ITER_LOOPTRI = 1 << 0,
  MR_ITER_POLY = 1 << 1,
  MR_ITER_LEDGE = 1 << 2,
  MR_ITER_LVERT = 1 << 3,
};
ENUM_OPERATORS(eMRIterType, MR_ITER_LVERT)

enum eMRDataType {
  MR_DATA_NONE = 0,
  MR_DATA_POLY_NOR = 1 << 1,
  MR_DATA_LOOP_NOR = 1 << 2,
  MR_DATA_LOOPTRI = 1 << 3,
  MR_DATA_LOOSE_GEOM = 1 << 4,
  /** Force loop normals calculation. */
  MR_DATA_TAN_LOOP_NOR = 1 << 5,
  MR_DATA_POLYS_SORTED = 1 << 6,
};
ENUM_OPERATORS(eMRDataType, MR_DATA_POLYS_SORTED)

BLI_INLINE int mesh_render_mat_len_get(const Object *object, const Mesh *me)
{
  if (me->edit_mesh != NULL) {
    const Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(object);
    if (editmesh_eval_final != NULL) {
      return std::max<int>(1, editmesh_eval_final->totcol);
    }
  }
  return std::max<int>(1, me->totcol);
}

struct MeshBufferList {
  /* Every VBO below contains at least enough data for every loop in the mesh
   * (except fdots and skin roots). For some VBOs, it extends to (in this exact order) :
   * loops + loose_edges * 2 + loose_verts */
  struct {
    GPUVertBuf *pos_nor;  /* extend */
    GPUVertBuf *lnor;     /* extend */
    GPUVertBuf *edge_fac; /* extend */
    GPUVertBuf *weights;  /* extend */
    GPUVertBuf *uv;
    GPUVertBuf *tan;
    GPUVertBuf *sculpt_data;
    GPUVertBuf *orco;
    /* Only for edit mode. */
    GPUVertBuf *edit_data; /* extend */
    GPUVertBuf *edituv_data;
    GPUVertBuf *edituv_stretch_area;
    GPUVertBuf *edituv_stretch_angle;
    GPUVertBuf *mesh_analysis;
    GPUVertBuf *fdots_pos;
    GPUVertBuf *fdots_nor;
    GPUVertBuf *fdots_uv;
    // GPUVertBuf *fdots_edit_data; /* inside fdots_nor for now. */
    GPUVertBuf *fdots_edituv_data;
    GPUVertBuf *skin_roots;
    /* Selection */
    GPUVertBuf *vert_idx; /* extend */
    GPUVertBuf *edge_idx; /* extend */
    GPUVertBuf *poly_idx;
    GPUVertBuf *fdot_idx;
    GPUVertBuf *attr[GPU_MAX_ATTR];
    GPUVertBuf *attr_viewer;
  } vbo;
  /* Index Buffers:
   * Only need to be updated when topology changes. */
  struct {
    /* Indices to vloops. Ordered per material. */
    GPUIndexBuf *tris;
    /* Loose edges last. */
    GPUIndexBuf *lines;
    /* Sub buffer of `lines` only containing the loose edges. */
    GPUIndexBuf *lines_loose;
    GPUIndexBuf *points;
    GPUIndexBuf *fdots;
    /* 3D overlays. */
    /* no loose edges. */
    GPUIndexBuf *lines_paint_mask;
    GPUIndexBuf *lines_adjacency;
    /* Uv overlays. (visibility can differ from 3D view) */
    GPUIndexBuf *edituv_tris;
    GPUIndexBuf *edituv_lines;
    GPUIndexBuf *edituv_points;
    GPUIndexBuf *edituv_fdots;
  } ibo;
};

struct MeshBatchList {
  /* Surfaces / Render */
  GPUBatch *surface;
  GPUBatch *surface_weights;
  /* Edit mode */
  GPUBatch *edit_triangles;
  GPUBatch *edit_vertices;
  GPUBatch *edit_edges;
  GPUBatch *edit_vnor;
  GPUBatch *edit_lnor;
  GPUBatch *edit_fdots;
  GPUBatch *edit_mesh_analysis;
  GPUBatch *edit_skin_roots;
  /* Edit UVs */
  GPUBatch *edituv_faces_stretch_area;
  GPUBatch *edituv_faces_stretch_angle;
  GPUBatch *edituv_faces;
  GPUBatch *edituv_edges;
  GPUBatch *edituv_verts;
  GPUBatch *edituv_fdots;
  /* Edit selection */
  GPUBatch *edit_selection_verts;
  GPUBatch *edit_selection_edges;
  GPUBatch *edit_selection_faces;
  GPUBatch *edit_selection_fdots;
  /* Common display / Other */
  GPUBatch *all_verts;
  GPUBatch *all_edges;
  GPUBatch *loose_edges;
  GPUBatch *edge_detection;
  /* Individual edges with face normals. */
  GPUBatch *wire_edges;
  /* Loops around faces. no edges between selected faces */
  GPUBatch *wire_loops;
  /* Same as wire_loops but only has uvs. */
  GPUBatch *wire_loops_uvs;
  GPUBatch *sculpt_overlays;
  GPUBatch *surface_viewer_attribute;
};

#define MBC_BATCH_LEN (sizeof(MeshBatchList) / sizeof(void *))
#define MBC_VBO_LEN (sizeof(MeshBufferList::vbo) / sizeof(void *))
#define MBC_IBO_LEN (sizeof(MeshBufferList::ibo) / sizeof(void *))

#define MBC_BATCH_INDEX(batch) (offsetof(MeshBatchList, batch) / sizeof(void *))

enum DRWBatchFlag {
  MBC_SURFACE = (1u << MBC_BATCH_INDEX(surface)),
  MBC_SURFACE_WEIGHTS = (1u << MBC_BATCH_INDEX(surface_weights)),
  MBC_EDIT_TRIANGLES = (1u << MBC_BATCH_INDEX(edit_triangles)),
  MBC_EDIT_VERTICES = (1u << MBC_BATCH_INDEX(edit_vertices)),
  MBC_EDIT_EDGES = (1u << MBC_BATCH_INDEX(edit_edges)),
  MBC_EDIT_VNOR = (1u << MBC_BATCH_INDEX(edit_vnor)),
  MBC_EDIT_LNOR = (1u << MBC_BATCH_INDEX(edit_lnor)),
  MBC_EDIT_FACEDOTS = (1u << MBC_BATCH_INDEX(edit_fdots)),
  MBC_EDIT_MESH_ANALYSIS = (1u << MBC_BATCH_INDEX(edit_mesh_analysis)),
  MBC_SKIN_ROOTS = (1u << MBC_BATCH_INDEX(edit_skin_roots)),
  MBC_EDITUV_FACES_STRETCH_AREA = (1u << MBC_BATCH_INDEX(edituv_faces_stretch_area)),
  MBC_EDITUV_FACES_STRETCH_ANGLE = (1u << MBC_BATCH_INDEX(edituv_faces_stretch_angle)),
  MBC_EDITUV_FACES = (1u << MBC_BATCH_INDEX(edituv_faces)),
  MBC_EDITUV_EDGES = (1u << MBC_BATCH_INDEX(edituv_edges)),
  MBC_EDITUV_VERTS = (1u << MBC_BATCH_INDEX(edituv_verts)),
  MBC_EDITUV_FACEDOTS = (1u << MBC_BATCH_INDEX(edituv_fdots)),
  MBC_EDIT_SELECTION_VERTS = (1u << MBC_BATCH_INDEX(edit_selection_verts)),
  MBC_EDIT_SELECTION_EDGES = (1u << MBC_BATCH_INDEX(edit_selection_edges)),
  MBC_EDIT_SELECTION_FACES = (1u << MBC_BATCH_INDEX(edit_selection_faces)),
  MBC_EDIT_SELECTION_FACEDOTS = (1u << MBC_BATCH_INDEX(edit_selection_fdots)),
  MBC_ALL_VERTS = (1u << MBC_BATCH_INDEX(all_verts)),
  MBC_ALL_EDGES = (1u << MBC_BATCH_INDEX(all_edges)),
  MBC_LOOSE_EDGES = (1u << MBC_BATCH_INDEX(loose_edges)),
  MBC_EDGE_DETECTION = (1u << MBC_BATCH_INDEX(edge_detection)),
  MBC_WIRE_EDGES = (1u << MBC_BATCH_INDEX(wire_edges)),
  MBC_WIRE_LOOPS = (1u << MBC_BATCH_INDEX(wire_loops)),
  MBC_WIRE_LOOPS_UVS = (1u << MBC_BATCH_INDEX(wire_loops_uvs)),
  MBC_SCULPT_OVERLAYS = (1u << MBC_BATCH_INDEX(sculpt_overlays)),
  MBC_VIEWER_ATTRIBUTE_OVERLAY = (1u << MBC_BATCH_INDEX(surface_viewer_attribute)),
  MBC_SURFACE_PER_MAT = (1u << MBC_BATCH_LEN),
};
ENUM_OPERATORS(DRWBatchFlag, MBC_SURFACE_PER_MAT);

BLI_STATIC_ASSERT(MBC_BATCH_LEN < 32, "Number of batches exceeded the limit of bit fields");

struct MeshExtractLooseGeom {
  int edge_len;
  int vert_len;
  int *verts;
  int *edges;
};

/**
 * Data that are kept around between extractions to reduce rebuilding time.
 *
 * - Loose geometry.
 */
struct MeshBufferCache {
  MeshBufferList buff;

  MeshExtractLooseGeom loose_geom;

  struct {
    int *tri_first_index;
    int *mat_tri_len;
    int visible_tri_len;
  } poly_sorted;
};

#define FOREACH_MESH_BUFFER_CACHE(batch_cache, mbc) \
  for (MeshBufferCache *mbc = &batch_cache->final; \
       mbc == &batch_cache->final || mbc == &batch_cache->cage || mbc == &batch_cache->uv_cage; \
       mbc = (mbc == &batch_cache->final) ? \
                 &batch_cache->cage : \
                 ((mbc == &batch_cache->cage) ? &batch_cache->uv_cage : NULL))

struct MeshBatchCache {
  MeshBufferCache final, cage, uv_cage;

  MeshBatchList batch;

  /* Index buffer per material. These are subranges of `ibo.tris` */
  GPUIndexBuf **tris_per_mat;

  GPUBatch **surface_per_mat;

  DRWSubdivCache *subdiv_cache;

  DRWBatchFlag batch_requested;
  DRWBatchFlag batch_ready;

  /* Settings to determine if cache is invalid. */
  int edge_len;
  int tri_len;
  int poly_len;
  int vert_len;
  int mat_len;
  /* Instantly invalidates cache, skipping mesh check */
  bool is_dirty;
  bool is_editmode;
  bool is_uvsyncsel;

  DRW_MeshWeightState weight_state;

  DRW_MeshCDMask cd_used, cd_needed, cd_used_over_time;

  DRW_Attributes attr_used, attr_needed, attr_used_over_time;

  int lastmatch;

  /* Valid only if edge_detection is up to date. */
  bool is_manifold;

  /* Total areas for drawing UV Stretching. Contains the summed area in mesh
   * space (`tot_area`) and the summed area in uv space (`tot_uvarea`).
   *
   * Only valid after `DRW_mesh_batch_cache_create_requested` has been called. */
  float tot_area, tot_uv_area;

  bool no_loose_wire;

  eV3DShadingColorType color_type;
  bool pbvh_is_drawing;
};

#define MBC_EDITUV \
  (MBC_EDITUV_FACES_STRETCH_AREA | MBC_EDITUV_FACES_STRETCH_ANGLE | MBC_EDITUV_FACES | \
   MBC_EDITUV_EDGES | MBC_EDITUV_VERTS | MBC_EDITUV_FACEDOTS | MBC_WIRE_LOOPS_UVS)

namespace blender::draw {

void mesh_buffer_cache_create_requested(TaskGraph *task_graph,
                                        MeshBatchCache *cache,
                                        MeshBufferCache *mbc,
                                        Object *object,
                                        Mesh *me,
                                        bool is_editmode,
                                        bool is_paint_mode,
                                        bool is_mode_active,
                                        const float obmat[4][4],
                                        bool do_final,
                                        bool do_uvedit,
                                        const Scene *scene,
                                        const ToolSettings *ts,
                                        bool use_hide);

void mesh_buffer_cache_create_requested_subdiv(MeshBatchCache *cache,
                                               MeshBufferCache *mbc,
                                               DRWSubdivCache *subdiv_cache,
                                               MeshRenderData *mr);

}  // namespace blender::draw
