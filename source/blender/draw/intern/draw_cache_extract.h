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
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#ifndef __DRAW_CACHE_EXTRACT_H__
#define __DRAW_CACHE_EXTRACT_H__

struct TaskGraph;

/* Vertex Group Selection and display options */
typedef struct DRW_MeshWeightState {
  int defgroup_active;
  int defgroup_len;

  short flags;
  char alert_mode;

  /* Set of all selected bones for Multipaint. */
  bool *defgroup_sel; /* [defgroup_len] */
  int defgroup_sel_count;

  /* Set of all locked and unlocked deform bones for Lock Relative mode. */
  bool *defgroup_locked;   /* [defgroup_len] */
  bool *defgroup_unlocked; /* [defgroup_len] */
} DRW_MeshWeightState;

/* DRW_MeshWeightState.flags */
enum {
  DRW_MESH_WEIGHT_STATE_MULTIPAINT = (1 << 0),
  DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE = (1 << 1),
  DRW_MESH_WEIGHT_STATE_LOCK_RELATIVE = (1 << 2),
};

typedef struct DRW_MeshCDMask {
  uint32_t uv : 8;
  uint32_t tan : 8;
  uint32_t vcol : 8;
  uint32_t sculpt_vcol : 8;
  uint32_t orco : 1;
  uint32_t tan_orco : 1;
  /** Edit uv layer is from the base edit mesh as
   *  modifiers could remove it. (see T68857) */
  uint32_t edit_uv : 1;
} DRW_MeshCDMask;

typedef enum eMRIterType {
  MR_ITER_LOOPTRI = 1 << 0,
  MR_ITER_LOOP = 1 << 1,
  MR_ITER_LEDGE = 1 << 2,
  MR_ITER_LVERT = 1 << 3,
} eMRIterType;

typedef enum eMRDataType {
  MR_DATA_POLY_NOR = 1 << 1,
  MR_DATA_LOOP_NOR = 1 << 2,
  MR_DATA_LOOPTRI = 1 << 3,
  /** Force loop normals calculation.  */
  MR_DATA_TAN_LOOP_NOR = 1 << 4,
} eMRDataType;

typedef enum eMRExtractType {
  MR_EXTRACT_BMESH,
  MR_EXTRACT_MAPPED,
  MR_EXTRACT_MESH,
} eMRExtractType;

BLI_INLINE int mesh_render_mat_len_get(Mesh *me)
{
  return MAX2(1, me->totcol);
}

typedef struct MeshBufferCache {
  /* Every VBO below contains at least enough
   * data for every loops in the mesh (except fdots and skin roots).
   * For some VBOs, it extends to (in this exact order) :
   * loops + loose_edges*2 + loose_verts */
  struct {
    GPUVertBuf *pos_nor;  /* extend */
    GPUVertBuf *lnor;     /* extend */
    GPUVertBuf *edge_fac; /* extend */
    GPUVertBuf *weights;  /* extend */
    GPUVertBuf *uv;
    GPUVertBuf *tan;
    GPUVertBuf *vcol;
    GPUVertBuf *orco;
    /* Only for edit mode. */
    GPUVertBuf *edit_data; /* extend */
    GPUVertBuf *edituv_data;
    GPUVertBuf *stretch_area;
    GPUVertBuf *stretch_angle;
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
  } vbo;
  /* Index Buffers:
   * Only need to be updated when topology changes. */
  struct {
    /* Indices to vloops. */
    GPUIndexBuf *tris;        /* Ordered per material. */
    GPUIndexBuf *lines;       /* Loose edges last. */
    GPUIndexBuf *lines_loose; /* sub buffer of `lines` only containing the loose edges. */
    GPUIndexBuf *points;
    GPUIndexBuf *fdots;
    /* 3D overlays. */
    GPUIndexBuf *lines_paint_mask; /* no loose edges. */
    GPUIndexBuf *lines_adjacency;
    /* Uv overlays. (visibility can differ from 3D view) */
    GPUIndexBuf *edituv_tris;
    GPUIndexBuf *edituv_lines;
    GPUIndexBuf *edituv_points;
    GPUIndexBuf *edituv_fdots;
  } ibo;
} MeshBufferCache;

typedef enum DRWBatchFlag {
  MBC_SURFACE = (1 << 0),
  MBC_SURFACE_WEIGHTS = (1 << 1),
  MBC_EDIT_TRIANGLES = (1 << 2),
  MBC_EDIT_VERTICES = (1 << 3),
  MBC_EDIT_EDGES = (1 << 4),
  MBC_EDIT_VNOR = (1 << 5),
  MBC_EDIT_LNOR = (1 << 6),
  MBC_EDIT_FACEDOTS = (1 << 7),
  MBC_EDIT_MESH_ANALYSIS = (1 << 8),
  MBC_EDITUV_FACES_STRETCH_AREA = (1 << 9),
  MBC_EDITUV_FACES_STRETCH_ANGLE = (1 << 10),
  MBC_EDITUV_FACES = (1 << 11),
  MBC_EDITUV_EDGES = (1 << 12),
  MBC_EDITUV_VERTS = (1 << 13),
  MBC_EDITUV_FACEDOTS = (1 << 14),
  MBC_EDIT_SELECTION_VERTS = (1 << 15),
  MBC_EDIT_SELECTION_EDGES = (1 << 16),
  MBC_EDIT_SELECTION_FACES = (1 << 17),
  MBC_EDIT_SELECTION_FACEDOTS = (1 << 18),
  MBC_ALL_VERTS = (1 << 19),
  MBC_ALL_EDGES = (1 << 20),
  MBC_LOOSE_EDGES = (1 << 21),
  MBC_EDGE_DETECTION = (1 << 22),
  MBC_WIRE_EDGES = (1 << 23),
  MBC_WIRE_LOOPS = (1 << 24),
  MBC_WIRE_LOOPS_UVS = (1 << 25),
  MBC_SURF_PER_MAT = (1 << 26),
  MBC_SKIN_ROOTS = (1 << 27),
} DRWBatchFlag;

#define MBC_EDITUV \
  (MBC_EDITUV_FACES_STRETCH_AREA | MBC_EDITUV_FACES_STRETCH_ANGLE | MBC_EDITUV_FACES | \
   MBC_EDITUV_EDGES | MBC_EDITUV_VERTS | MBC_EDITUV_FACEDOTS | MBC_WIRE_LOOPS_UVS)

#define FOREACH_MESH_BUFFER_CACHE(batch_cache, mbc) \
  for (MeshBufferCache *mbc = &batch_cache->final; \
       mbc == &batch_cache->final || mbc == &batch_cache->cage || mbc == &batch_cache->uv_cage; \
       mbc = (mbc == &batch_cache->final) ? \
                 &batch_cache->cage : \
                 ((mbc == &batch_cache->cage) ? &batch_cache->uv_cage : NULL))

typedef struct MeshBatchCache {
  MeshBufferCache final, cage, uv_cage;

  struct {
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
    GPUBatch *wire_edges;     /* Individual edges with face normals. */
    GPUBatch *wire_loops;     /* Loops around faces. no edges between selected faces */
    GPUBatch *wire_loops_uvs; /* Same as wire_loops but only has uvs. */
  } batch;

  GPUBatch **surface_per_mat;

  DRWBatchFlag batch_requested;
  DRWBatchFlag batch_ready;

  /* settings to determine if cache is invalid */
  int edge_len;
  int tri_len;
  int poly_len;
  int vert_len;
  int mat_len;
  bool is_dirty; /* Instantly invalidates cache, skipping mesh check */
  bool is_editmode;
  bool is_uvsyncsel;

  struct DRW_MeshWeightState weight_state;

  DRW_MeshCDMask cd_used, cd_needed, cd_used_over_time;

  int lastmatch;

  /* Valid only if edge_detection is up to date. */
  bool is_manifold;

  /* Total areas for drawing UV Stretching. Contains the summed area in mesh
   * space (`tot_area`) and the summed area in uv space (`tot_uvarea`).
   *
   * Only valid after `DRW_mesh_batch_cache_create_requested` has been called. */
  float tot_area, tot_uv_area;

  bool no_loose_wire;
} MeshBatchCache;

void mesh_buffer_cache_create_requested(struct TaskGraph *task_graph,
                                        MeshBatchCache *cache,
                                        MeshBufferCache mbc,
                                        Mesh *me,
                                        const bool is_editmode,
                                        const bool is_paint_mode,
                                        const float obmat[4][4],
                                        const bool do_final,
                                        const bool do_uvedit,
                                        const bool use_subsurf_fdots,
                                        const DRW_MeshCDMask *cd_layer_used,
                                        const Scene *scene,
                                        const ToolSettings *ts,
                                        const bool use_hide);

#endif /* __DRAW_CACHE_EXTRACT_H__ */
