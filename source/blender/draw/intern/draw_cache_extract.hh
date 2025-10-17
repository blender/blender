/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_enum_flags.hh"
#include "BLI_map.hh"
#include "BLI_math_matrix_types.hh"

#include "DNA_view3d_enums.h"

#include "GPU_index_buffer.hh"
#include "GPU_shader.hh"
#include "GPU_vertex_buffer.hh"

#include "draw_attributes.hh"

namespace blender::gpu {
class Batch;
class IndexBuf;
}  // namespace blender::gpu
struct Mesh;
struct Object;
struct Scene;
struct TaskGraph;
struct ToolSettings;

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

/**
 * Vertex buffer types that can be use by batches in the mesh batch cache.
 *
 * \todo It would be good to change this to something like #draw::pbvh::AttributeRequest to
 * separate the generic attribute requests. While there is a limit on the number of vertex buffers
 * used by a single shader/batch, there is no need for that limit here; there are potentially many
 * shaders requiring attributes for a particular mesh. OTOH, it may be good to use flags for the
 * builtin buffer types, so that bitwise operations can be used.
 */
enum class VBOType : int8_t {
  Position,
  CornerNormal,
  EdgeFactor,
  VertexGroupWeight,
  UVs,
  Tangents,
  SculptData,
  Orco,
  EditData,
  EditUVData,
  EditUVStretchArea,
  EditUVStretchAngle,
  MeshAnalysis,
  FaceDotPosition,
  FaceDotNormal,
  FaceDotUV,
  FaceDotEditUVData,
  SkinRoots,
  IndexVert,
  IndexEdge,
  IndexFace,
  IndexFaceDot,
  Attr0,
  Attr1,
  Attr2,
  Attr3,
  Attr5,
  Attr6,
  Attr7,
  Attr8,
  Attr9,
  Attr10,
  Attr11,
  Attr12,
  Attr13,
  Attr14,
  Attr15,
  AttrViewer,
  VertexNormal,
  PaintOverlayFlag,
};

/**
 * All index buffers used for mesh batches.
 *
 * \note "Tris per material" (#MeshBatchCache::tris_per_mat) is an exception. Since there are
 * an arbitrary numbers of materials, those are handled separately (as slices of the overall
 * triangles buffer).
 */
enum class IBOType : int8_t {
  Tris,
  Lines,
  LinesLoose,
  Points,
  FaceDots,
  LinesPaintMask,
  LinesAdjacency,
  UVTris,
  AllUVLines,
  UVLines,
  EditUVTris,
  EditUVLines,
  EditUVPoints,
  EditUVFaceDots,
};

struct MeshBufferList {
  /* Though using maps here may add some overhead compared to just indexed arrays, it's a bit more
   * convenient currently, because the "buffer exists" test is very clear, it's just whether the
   * map contains it (e.g. compared to "buffer is allocated but not filled with data"). The
   * sparseness *may* be useful for reducing memory usage when only few buffers are used. */

  Map<VBOType, std::unique_ptr<gpu::VertBuf, gpu::VertBufDeleter>> vbos;
  Map<IBOType, std::unique_ptr<gpu::IndexBuf, gpu::IndexBufDeleter>> ibos;
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
  gpu::Batch *uv_faces;
  gpu::Batch *all_verts;
  gpu::Batch *all_edges;
  gpu::Batch *loose_edges;
  gpu::Batch *edge_detection;
  /* Individual edges with face normals. */
  gpu::Batch *wire_edges;
  /* Loops around faces. no edges between selected faces */
  gpu::Batch *paint_overlay_wire_loops;
  gpu::Batch *wire_loops_all_uvs;
  gpu::Batch *wire_loops_uvs;
  gpu::Batch *wire_loops_edituvs;
  gpu::Batch *sculpt_overlays;
  gpu::Batch *surface_viewer_attribute;
  gpu::Batch *paint_overlay_verts;
  gpu::Batch *paint_overlay_surface;
};

#define MBC_BATCH_LEN (sizeof(MeshBatchList) / sizeof(void *))

#define MBC_BATCH_INDEX(batch) (offsetof(MeshBatchList, batch) / sizeof(void *))

enum DRWBatchFlag : uint64_t {
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
  MBC_UV_FACES = (1u << MBC_BATCH_INDEX(uv_faces)),
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
  MBC_PAINT_OVERLAY_WIRE_LOOPS = (1u << MBC_BATCH_INDEX(paint_overlay_wire_loops)),
  MBC_WIRE_LOOPS_ALL_UVS = (1u << MBC_BATCH_INDEX(wire_loops_all_uvs)),
  MBC_WIRE_LOOPS_UVS = (1u << MBC_BATCH_INDEX(wire_loops_uvs)),
  MBC_WIRE_LOOPS_EDITUVS = (1u << MBC_BATCH_INDEX(wire_loops_edituvs)),
  MBC_SCULPT_OVERLAYS = (1u << MBC_BATCH_INDEX(sculpt_overlays)),
  MBC_VIEWER_ATTRIBUTE_OVERLAY = (1u << MBC_BATCH_INDEX(surface_viewer_attribute)),
  MBC_PAINT_OVERLAY_VERTS = (uint64_t(1u) << MBC_BATCH_INDEX(paint_overlay_verts)),
  MBC_PAINT_OVERLAY_SURFACE = (uint64_t(1u) << MBC_BATCH_INDEX(paint_overlay_surface)),
  MBC_SURFACE_PER_MAT = (uint64_t(1u) << MBC_BATCH_LEN),
};
ENUM_OPERATORS(DRWBatchFlag);

BLI_STATIC_ASSERT(MBC_BATCH_LEN < 64, "Number of batches exceeded the limit of bit fields");

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
  Array<gpu::IndexBufPtr> tris_per_mat;
  Array<gpu::Batch *> surface_per_mat;

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

  VectorSet<std::string> attr_used, attr_needed, attr_used_over_time;

  int lastmatch;

  /* Valid only if edge_detection is up to date. */
  bool is_manifold;

  bool no_loose_wire;

  /* Total areas for drawing UV Stretching. Contains the summed area in mesh
   * space (`tot_area`) and the summed area in uv space (`tot_uvarea`).
   *
   * Only valid after `DRW_mesh_batch_cache_create_requested` has been called. */
  float tot_area, tot_uv_area;
};

#define MBC_EDITUV \
  (MBC_EDITUV_FACES_STRETCH_AREA | MBC_EDITUV_FACES_STRETCH_ANGLE | MBC_EDITUV_FACES | \
   MBC_EDITUV_EDGES | MBC_EDITUV_VERTS | MBC_EDITUV_FACEDOTS | MBC_UV_FACES | \
   MBC_WIRE_LOOPS_ALL_UVS | MBC_WIRE_LOOPS_UVS | MBC_WIRE_LOOPS_EDITUVS)

void mesh_buffer_cache_create_requested(TaskGraph &task_graph,
                                        const Scene &scene,
                                        MeshBatchCache &cache,
                                        MeshBufferCache &mbc,
                                        Span<IBOType> ibo_requests,
                                        Span<VBOType> vbo_requests,
                                        Object &object,
                                        Mesh &mesh,
                                        bool is_editmode,
                                        bool is_paint_mode,
                                        bool do_final,
                                        bool do_uvedit,
                                        bool use_hide);

void mesh_buffer_cache_create_requested_subdiv(MeshBatchCache &cache,
                                               MeshBufferCache &mbc,
                                               Span<IBOType> ibo_requests,
                                               Span<VBOType> vbo_requests,
                                               DRWSubdivCache &subdiv_cache,
                                               MeshRenderData &mr);

}  // namespace blender::draw
