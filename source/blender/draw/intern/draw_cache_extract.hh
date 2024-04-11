/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_math_matrix_types.hh"
#include "BLI_utildefines.h"

#include "GPU_shader.hh"

#include "draw_attributes.hh"

namespace blender::gpu {
class Batch;
class IndexBuf;
}  // namespace blender::gpu
struct TaskGraph;

namespace blender::draw {

struct MeshRenderData;
struct DRWSubdivCache;

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
  MR_ITER_CORNER_TRI = 1 << 0,
  MR_ITER_POLY = 1 << 1,
  MR_ITER_LOOSE_EDGE = 1 << 2,
  MR_ITER_LOOSE_VERT = 1 << 3,
};
ENUM_OPERATORS(eMRIterType, MR_ITER_LOOSE_VERT)

enum eMRDataType {
  MR_DATA_NONE = 0,
  MR_DATA_POLY_NOR = 1 << 1,
  MR_DATA_LOOP_NOR = 1 << 2,
  MR_DATA_CORNER_TRI = 1 << 3,
  MR_DATA_LOOSE_GEOM = 1 << 4,
  /** Force loop normals calculation. */
  MR_DATA_TAN_LOOP_NOR = 1 << 5,
  MR_DATA_POLYS_SORTED = 1 << 6,
};
ENUM_OPERATORS(eMRDataType, MR_DATA_POLYS_SORTED)

int mesh_render_mat_len_get(const Object *object, const Mesh *mesh);

struct MeshBufferList {
  /* Every VBO below contains at least enough data for every loop in the mesh
   * (except fdots and skin roots). For some VBOs, it extends to (in this exact order) :
   * loops + loose_edges * 2 + loose_verts */
  struct {
    gpu::VertBuf *pos;      /* extend */
    gpu::VertBuf *nor;      /* extend */
    gpu::VertBuf *edge_fac; /* extend */
    gpu::VertBuf *weights;  /* extend */
    gpu::VertBuf *uv;
    gpu::VertBuf *tan;
    gpu::VertBuf *sculpt_data;
    gpu::VertBuf *orco;
    /* Only for edit mode. */
    gpu::VertBuf *edit_data; /* extend */
    gpu::VertBuf *edituv_data;
    gpu::VertBuf *edituv_stretch_area;
    gpu::VertBuf *edituv_stretch_angle;
    gpu::VertBuf *mesh_analysis;
    gpu::VertBuf *fdots_pos;
    gpu::VertBuf *fdots_nor;
    gpu::VertBuf *fdots_uv;
    // gpu::VertBuf *fdots_edit_data; /* inside fdots_nor for now. */
    gpu::VertBuf *fdots_edituv_data;
    gpu::VertBuf *skin_roots;
    /* Selection */
    gpu::VertBuf *vert_idx; /* extend */
    gpu::VertBuf *edge_idx; /* extend */
    gpu::VertBuf *face_idx;
    gpu::VertBuf *fdot_idx;
    gpu::VertBuf *attr[GPU_MAX_ATTR];
    gpu::VertBuf *attr_viewer;
    gpu::VertBuf *vnor;
  } vbo;
  /* Index Buffers:
   * Only need to be updated when topology changes. */
  struct {
    /* Indices to vloops. Ordered per material. */
    gpu::IndexBuf *tris;
    /* Loose edges last. */
    gpu::IndexBuf *lines;
    /* Sub buffer of `lines` only containing the loose edges. */
    gpu::IndexBuf *lines_loose;
    gpu::IndexBuf *points;
    gpu::IndexBuf *fdots;
    /* 3D overlays. */
    /* no loose edges. */
    gpu::IndexBuf *lines_paint_mask;
    gpu::IndexBuf *lines_adjacency;
    /** UV overlays. (visibility can differ from 3D view). */
    gpu::IndexBuf *edituv_tris;
    gpu::IndexBuf *edituv_lines;
    gpu::IndexBuf *edituv_points;
    gpu::IndexBuf *edituv_fdots;
  } ibo;
};

struct MeshBatchList {
  /* Surfaces / Render */
  gpu::Batch *surface;
  gpu::Batch *surface_weights;
  /* Edit mode */
  gpu::Batch *edit_triangles;
  gpu::Batch *edit_vertices;
  gpu::Batch *edit_edges;
  gpu::Batch *edit_vnor;
  gpu::Batch *edit_lnor;
  gpu::Batch *edit_fdots;
  gpu::Batch *edit_mesh_analysis;
  gpu::Batch *edit_skin_roots;
  /* Edit UVs */
  gpu::Batch *edituv_faces_stretch_area;
  gpu::Batch *edituv_faces_stretch_angle;
  gpu::Batch *edituv_faces;
  gpu::Batch *edituv_edges;
  gpu::Batch *edituv_verts;
  gpu::Batch *edituv_fdots;
  /* Edit selection */
  gpu::Batch *edit_selection_verts;
  gpu::Batch *edit_selection_edges;
  gpu::Batch *edit_selection_faces;
  gpu::Batch *edit_selection_fdots;
  /* Common display / Other */
  gpu::Batch *all_verts;
  gpu::Batch *all_edges;
  gpu::Batch *loose_edges;
  gpu::Batch *edge_detection;
  /* Individual edges with face normals. */
  gpu::Batch *wire_edges;
  /* Loops around faces. no edges between selected faces */
  gpu::Batch *wire_loops;
  /* Same as wire_loops but only has uvs. */
  gpu::Batch *wire_loops_uvs;
  gpu::Batch *sculpt_overlays;
  gpu::Batch *surface_viewer_attribute;
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
  /** Indices of all vertices not used by edges in the #Mesh or #BMesh. */
  Array<int> verts;
  /** Indices of all edges not used by faces in the #Mesh or #BMesh. */
  Array<int> edges;
};

struct SortedFaceData {
  /* The total number of visible triangles (a sum of the values in #mat_tri_counts). */
  int visible_tris_num;
  /** The number of visible triangles assigned to each material. */
  Array<int> tris_num_by_material;
  /**
   * The first triangle index for each face, sorted into slices by material.
   * May be empty if the mesh only has a single material.
   */
  std::optional<Array<int>> face_tri_offsets;
};

/**
 * Data that are kept around between extractions to reduce rebuilding time.
 *
 * - Loose geometry.
 */
struct MeshBufferCache {
  MeshBufferList buff;

  MeshExtractLooseGeom loose_geom;

  SortedFaceData face_sorted;
};

#define FOREACH_MESH_BUFFER_CACHE(batch_cache, mbc) \
  for (MeshBufferCache *mbc = &batch_cache.final; \
       mbc == &batch_cache.final || mbc == &batch_cache.cage || mbc == &batch_cache.uv_cage; \
       mbc = (mbc == &batch_cache.final) ? \
                 &batch_cache.cage : \
                 ((mbc == &batch_cache.cage) ? &batch_cache.uv_cage : nullptr))

struct MeshBatchCache {
  MeshBufferCache final, cage, uv_cage;

  MeshBatchList batch;

  /* Index buffer per material. These are sub-ranges of `ibo.tris`. */
  gpu::IndexBuf **tris_per_mat;

  gpu::Batch **surface_per_mat;

  DRWSubdivCache *subdiv_cache;

  DRWBatchFlag batch_requested;
  DRWBatchFlag batch_ready;

  /* Settings to determine if cache is invalid. */
  int edge_len;
  int tri_len;
  int face_len;
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

void mesh_buffer_cache_create_requested(TaskGraph *task_graph,
                                        MeshBatchCache &cache,
                                        MeshBufferCache &mbc,
                                        Object *object,
                                        Mesh *mesh,
                                        bool is_editmode,
                                        bool is_paint_mode,
                                        bool is_mode_active,
                                        const float4x4 &object_to_world,
                                        bool do_final,
                                        bool do_uvedit,
                                        const Scene *scene,
                                        const ToolSettings *ts,
                                        bool use_hide);

void mesh_buffer_cache_create_requested_subdiv(MeshBatchCache &cache,
                                               MeshBufferCache &mbc,
                                               DRWSubdivCache &subdiv_cache,
                                               MeshRenderData &mr);

}  // namespace blender::draw
