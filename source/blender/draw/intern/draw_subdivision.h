/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_sys_types.h"

struct BMesh;
struct GPUIndexBuf;
struct GPUUniformBuf;
struct GPUVertBuf;
struct GPUVertFormat;
struct Mesh;
struct MeshBatchCache;
struct MeshBufferCache;
struct MeshRenderData;
struct Object;
struct Subdiv;
struct ToolSettings;

/* -------------------------------------------------------------------- */
/** \name DRWPatchMap
 *
 * This is a GPU version of the OpenSubDiv PatchMap. The quad tree and the patch handles are copied
 * to GPU buffers in order to lookup the right patch for a given set of patch coordinates.
 * \{ */

typedef struct DRWPatchMap {
  struct GPUVertBuf *patch_map_handles;
  struct GPUVertBuf *patch_map_quadtree;
  int min_patch_face;
  int max_patch_face;
  int max_depth;
  int patches_are_triangular;
} DRWPatchMap;

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivLooseEdge
 *
 * This stores information about a subdivided loose edge.
 * \{ */

typedef struct DRWSubdivLooseEdge {
  /* The corresponding coarse edge, this is always valid. */
  int coarse_edge_index;
  /* Pointers into #DRWSubdivLooseGeom.verts. */
  int loose_subdiv_v1_index;
  int loose_subdiv_v2_index;
} DRWSubdivLooseEdge;

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivLooseVertex
 *
 * This stores information about a subdivided loose vertex, that may or may not come from a loose
 * edge.
 * \{ */

typedef struct DRWSubdivLooseVertex {
  /* The corresponding coarse vertex, or -1 if this vertex is the result
   * of subdivision. */
  unsigned int coarse_vertex_index;
  /* Position and normal of the vertex. */
  float co[3];
  float nor[3];
} DRWSubdivLooseVertex;

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivLooseGeom
 *
 * This stores the subdivided vertices and edges of loose geometry from #MeshExtractLooseGeom.
 * \{ */

typedef struct DRWSubdivLooseGeom {
  DRWSubdivLooseEdge *edges;
  DRWSubdivLooseVertex *verts;
  int edge_len;
  int vert_len;
  int loop_len;
} DRWSubdivLooseGeom;

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivCache
 *
 * This holds the various buffers used to evaluate and render subdivision through OpenGL.
 * \{ */

typedef struct DRWSubdivCache {
  struct Mesh *mesh;
  struct BMesh *bm;
  struct Subdiv *subdiv;
  bool optimal_display;
  bool hide_unmapped_edges;
  bool use_custom_loop_normals;

  /* Coordinates used to evaluate patches for positions and normals. */
  struct GPUVertBuf *patch_coords;
  /* Coordinates used to evaluate patches for attributes. */
  struct GPUVertBuf *corner_patch_coords;
  /* Coordinates used to evaluate patches for the face centers (or face dots) in edit-mode. */
  struct GPUVertBuf *fdots_patch_coords;

  /* Resolution used to generate the patch coordinates. */
  int resolution;

  /* Number of subdivided loops, also the number of patch coordinates since we have one coordinate
   * but quad corner/vertex. */
  uint num_subdiv_loops;
  uint num_subdiv_edges;
  uint num_subdiv_triangles;
  uint num_subdiv_verts;
  uint num_subdiv_quads;

  /* We only do the subdivision traversal for full faces, however we may have geometries that only
   * have loose edges (e.g. a custom bone shape). This flag is used to detect those cases, as the
   * counters above will all be set to zero if we do not have subdivision loops. */
  bool may_have_loose_geom;

  /* Number of faces in the coarse mesh, notably used to compute a coarse face index given a
   * subdivision loop index. */
  int num_coarse_faces;

  /* Maps subdivision loop to subdivided vertex index. */
  int *subdiv_loop_subdiv_vert_index;
  /* Maps subdivision loop to subdivided edge index. */
  int *subdiv_loop_subdiv_edge_index;
  /* Maps subdivision loop to original coarse face index. */
  int *subdiv_loop_face_index;

  /* Indices of faces adjacent to the vertices, ordered by vertex index, with no particular
   * winding. */
  struct GPUVertBuf *subdiv_vertex_face_adjacency;
  /* The difference between value (i + 1) and (i) gives the number of faces adjacent to vertex (i).
   */
  struct GPUVertBuf *subdiv_vertex_face_adjacency_offsets;

  /* Maps subdivision loop to original coarse vertex index, only really useful for edit mode. */
  struct GPUVertBuf *verts_orig_index;
  /* Maps subdivision loop to original coarse edge index, only really useful for edit mode. */
  struct GPUVertBuf *edges_orig_index;
  /* Indicates if edge should be drawn in optimal display mode. */
  struct GPUVertBuf *edges_draw_flag;

  /* Owned by #Subdiv. Indexed by coarse face index, difference between value (i + 1) and (i)
   * gives the number of ptex faces for coarse face (i). */
  int *face_ptex_offset;
  /* Vertex buffer for face_ptex_offset. */
  struct GPUVertBuf *face_ptex_offset_buffer;

  int *subdiv_face_offset;
  struct GPUVertBuf *subdiv_face_offset_buffer;

  /* Contains the start loop index and the smooth flag for each coarse face. */
  struct GPUVertBuf *extra_coarse_face_data;

  /* Computed for `ibo.points`, one value per subdivided vertex,
   * mapping coarse vertices -> subdivided loop. */
  int *point_indices;

  /* Material offsets. */
  int *mat_start;
  int *mat_end;
  struct GPUVertBuf *face_mat_offset;

  DRWPatchMap gpu_patch_map;

  DRWSubdivLooseGeom loose_geom;

  /* UBO to store settings for the various compute shaders. */
  struct GPUUniformBuf *ubo;

  /* Extra flags, passed to the UBO. */
  bool is_edit_mode;
  bool use_hide;
} DRWSubdivCache;

/* Only frees the data of the cache, caller is responsible to free the cache itself if necessary.
 */
void draw_subdiv_cache_free(DRWSubdivCache &cache);

/** \} */

void DRW_create_subdivision(struct Object *ob,
                            struct Mesh *mesh,
                            struct MeshBatchCache &batch_cache,
                            struct MeshBufferCache *mbc,
                            const bool is_editmode,
                            const bool is_paint_mode,
                            const bool is_mode_active,
                            const float obmat[4][4],
                            const bool do_final,
                            const bool do_uvedit,
                            const bool do_cage,
                            const ToolSettings *ts,
                            const bool use_hide);

void DRW_subdivide_loose_geom(DRWSubdivCache *subdiv_cache, struct MeshBufferCache *cache);

void DRW_subdiv_cache_free(struct Subdiv *subdiv);

void draw_subdiv_init_origindex_buffer(struct GPUVertBuf *buffer,
                                       int32_t *vert_origindex,
                                       uint num_loops,
                                       uint loose_len);

struct GPUVertBuf *draw_subdiv_build_origindex_buffer(int *vert_origindex, uint num_loops);

/* Compute shader functions. */

void draw_subdiv_build_sculpt_data_buffer(const DRWSubdivCache &cache,
                                          struct GPUVertBuf *mask_vbo,
                                          struct GPUVertBuf *face_set_vbo,
                                          struct GPUVertBuf *sculpt_data);

void draw_subdiv_accumulate_normals(const DRWSubdivCache &cache,
                                    struct GPUVertBuf *pos_nor,
                                    struct GPUVertBuf *face_adjacency_offsets,
                                    struct GPUVertBuf *face_adjacency_lists,
                                    struct GPUVertBuf *vertex_loop_map,
                                    struct GPUVertBuf *vert_normals);

void draw_subdiv_finalize_normals(const DRWSubdivCache &cache,
                                  struct GPUVertBuf *vert_normals,
                                  struct GPUVertBuf *subdiv_loop_subdiv_vert_index,
                                  struct GPUVertBuf *pos_nor);

void draw_subdiv_finalize_custom_normals(const DRWSubdivCache &cache,
                                         GPUVertBuf *src_custom_normals,
                                         GPUVertBuf *pos_nor);

void draw_subdiv_extract_pos_nor(const DRWSubdivCache &cache,
                                 GPUVertBuf *flags_buffer,
                                 struct GPUVertBuf *pos_nor,
                                 struct GPUVertBuf *orco);

void draw_subdiv_interp_custom_data(const DRWSubdivCache &cache,
                                    struct GPUVertBuf *src_data,
                                    struct GPUVertBuf *dst_data,
                                    int comp_type, /*GPUVertCompType*/
                                    int dimensions,
                                    int dst_offset);

void draw_subdiv_extract_uvs(const DRWSubdivCache &cache,
                             struct GPUVertBuf *uvs,
                             int face_varying_channel,
                             int dst_offset);

void draw_subdiv_build_edge_fac_buffer(const DRWSubdivCache &cache,
                                       struct GPUVertBuf *pos_nor,
                                       struct GPUVertBuf *edge_draw_flag,
                                       struct GPUVertBuf *poly_other_map,
                                       struct GPUVertBuf *edge_fac);

void draw_subdiv_build_tris_buffer(const DRWSubdivCache &cache,
                                   struct GPUIndexBuf *subdiv_tris,
                                   int material_count);

void draw_subdiv_build_lines_buffer(const DRWSubdivCache &cache,
                                    struct GPUIndexBuf *lines_indices);

void draw_subdiv_build_lines_loose_buffer(const DRWSubdivCache &cache,
                                          struct GPUIndexBuf *lines_indices,
                                          GPUVertBuf *lines_flags,
                                          uint num_loose_edges);

void draw_subdiv_build_fdots_buffers(const DRWSubdivCache &cache,
                                     struct GPUVertBuf *fdots_pos,
                                     struct GPUVertBuf *fdots_nor,
                                     struct GPUIndexBuf *fdots_indices);

void draw_subdiv_build_lnor_buffer(const DRWSubdivCache &cache,
                                   struct GPUVertBuf *pos_nor,
                                   struct GPUVertBuf *lnor);

void draw_subdiv_build_edituv_stretch_area_buffer(const DRWSubdivCache &cache,
                                                  struct GPUVertBuf *coarse_data,
                                                  struct GPUVertBuf *subdiv_data);

void draw_subdiv_build_edituv_stretch_angle_buffer(const DRWSubdivCache &cache,
                                                   struct GPUVertBuf *pos_nor,
                                                   struct GPUVertBuf *uvs,
                                                   int uvs_offset,
                                                   struct GPUVertBuf *stretch_angles);

/** Return the format used for the positions and normals VBO. */
struct GPUVertFormat *draw_subdiv_get_pos_nor_format(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#  include "BLI_span.hh"

/* Helper to access the loose edges. */
blender::Span<DRWSubdivLooseEdge> draw_subdiv_cache_get_loose_edges(const DRWSubdivCache &cache);

/* Helper to access only the loose vertices, i.e. not the ones attached to loose edges. To access
 * loose vertices of loose edges #draw_subdiv_cache_get_loose_edges should be used. */
blender::Span<DRWSubdivLooseVertex> draw_subdiv_cache_get_loose_verts(const DRWSubdivCache &cache);

#endif
