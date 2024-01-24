/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_span.hh"
#include "BLI_sys_types.h"

struct BMesh;
struct GPUIndexBuf;
struct GPUUniformBuf;
struct GPUVertBuf;
struct GPUVertFormat;
struct Mesh;
struct Object;
struct Subdiv;
struct ToolSettings;

namespace blender::draw {

struct MeshBatchCache;
struct MeshBufferCache;
struct MeshRenderData;

/* -------------------------------------------------------------------- */
/** \name DRWPatchMap
 *
 * This is a GPU version of the OpenSubDiv PatchMap. The quad tree and the patch handles are copied
 * to GPU buffers in order to lookup the right patch for a given set of patch coordinates.
 * \{ */

struct DRWPatchMap {
  GPUVertBuf *patch_map_handles;
  GPUVertBuf *patch_map_quadtree;
  int min_patch_face;
  int max_patch_face;
  int max_depth;
  int patches_are_triangular;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivLooseEdge
 *
 * This stores information about a subdivided loose edge.
 * \{ */

struct DRWSubdivLooseEdge {
  /* The corresponding coarse edge, this is always valid. */
  int coarse_edge_index;
  /* Pointers into #DRWSubdivLooseGeom.verts. */
  int loose_subdiv_v1_index;
  int loose_subdiv_v2_index;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivLooseVertex
 *
 * This stores information about a subdivided loose vertex, that may or may not come from a loose
 * edge.
 * \{ */

struct DRWSubdivLooseVertex {
  /* The corresponding coarse vertex, or -1 if this vertex is the result
   * of subdivision. */
  unsigned int coarse_vertex_index;
  /* Position and normal of the vertex. */
  float co[3];
  float nor[3];
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivLooseGeom
 *
 * This stores the subdivided vertices and edges of loose geometry from #MeshExtractLooseGeom.
 * \{ */

struct DRWSubdivLooseGeom {
  DRWSubdivLooseEdge *edges;
  DRWSubdivLooseVertex *verts;
  int edge_len;
  int vert_len;
  int loop_len;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivCache
 *
 * This holds the various buffers used to evaluate and render subdivision through OpenGL.
 * \{ */

struct DRWSubdivCache {
  Mesh *mesh;
  BMesh *bm;
  Subdiv *subdiv;
  bool optimal_display;
  bool hide_unmapped_edges;
  bool use_custom_loop_normals;

  /* Coordinates used to evaluate patches for positions and normals. */
  GPUVertBuf *patch_coords;
  /* Coordinates used to evaluate patches for attributes. */
  GPUVertBuf *corner_patch_coords;
  /* Coordinates used to evaluate patches for the face centers (or face dots) in edit-mode. */
  GPUVertBuf *fdots_patch_coords;

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
  GPUVertBuf *subdiv_vertex_face_adjacency;
  /* The difference between value (i + 1) and (i) gives the number of faces adjacent to vertex (i).
   */
  GPUVertBuf *subdiv_vertex_face_adjacency_offsets;

  /* Maps subdivision loop to original coarse vertex index, only really useful for edit mode. */
  GPUVertBuf *verts_orig_index;
  /* Maps subdivision loop to original coarse edge index, only really useful for edit mode. */
  GPUVertBuf *edges_orig_index;
  /* Indicates if edge should be drawn in optimal display mode. */
  GPUVertBuf *edges_draw_flag;

  /* Owned by #Subdiv. Indexed by coarse face index, difference between value (i + 1) and (i)
   * gives the number of ptex faces for coarse face (i). */
  int *face_ptex_offset;
  /* Vertex buffer for face_ptex_offset. */
  GPUVertBuf *face_ptex_offset_buffer;

  int *subdiv_face_offset;
  GPUVertBuf *subdiv_face_offset_buffer;

  /* Contains the start loop index and the smooth flag for each coarse face. */
  GPUVertBuf *extra_coarse_face_data;

  /* Computed for `ibo.points`, one value per subdivided vertex,
   * mapping coarse vertices -> subdivided loop. */
  int *point_indices;

  /* Material offsets. */
  int *mat_start;
  int *mat_end;
  GPUVertBuf *face_mat_offset;

  DRWPatchMap gpu_patch_map;

  DRWSubdivLooseGeom loose_geom;

  /* UBO to store settings for the various compute shaders. */
  GPUUniformBuf *ubo;

  /* Extra flags, passed to the UBO. */
  bool is_edit_mode;
  bool use_hide;
};

/* Only frees the data of the cache, caller is responsible to free the cache itself if necessary.
 */
void draw_subdiv_cache_free(DRWSubdivCache &cache);

/** \} */

void DRW_create_subdivision(Object *ob,
                            Mesh *mesh,
                            MeshBatchCache &batch_cache,
                            MeshBufferCache *mbc,
                            bool is_editmode,
                            bool is_paint_mode,
                            bool is_mode_active,
                            const float obmat[4][4],
                            bool do_final,
                            bool do_uvedit,
                            bool do_cage,
                            const ToolSettings *ts,
                            bool use_hide);

void DRW_subdivide_loose_geom(DRWSubdivCache *subdiv_cache, MeshBufferCache *cache);

void DRW_subdiv_cache_free(Subdiv *subdiv);

void draw_subdiv_init_origindex_buffer(GPUVertBuf *buffer,
                                       int32_t *vert_origindex,
                                       uint num_loops,
                                       uint loose_len);

GPUVertBuf *draw_subdiv_build_origindex_buffer(int *vert_origindex, uint num_loops);

/* Compute shader functions. */

void draw_subdiv_build_sculpt_data_buffer(const DRWSubdivCache &cache,
                                          GPUVertBuf *mask_vbo,
                                          GPUVertBuf *face_set_vbo,
                                          GPUVertBuf *sculpt_data);

void draw_subdiv_accumulate_normals(const DRWSubdivCache &cache,
                                    GPUVertBuf *pos_nor,
                                    GPUVertBuf *face_adjacency_offsets,
                                    GPUVertBuf *face_adjacency_lists,
                                    GPUVertBuf *vertex_loop_map,
                                    GPUVertBuf *vert_normals);

void draw_subdiv_finalize_normals(const DRWSubdivCache &cache,
                                  GPUVertBuf *vert_normals,
                                  GPUVertBuf *subdiv_loop_subdiv_vert_index,
                                  GPUVertBuf *pos_nor);

void draw_subdiv_finalize_custom_normals(const DRWSubdivCache &cache,
                                         GPUVertBuf *src_custom_normals,
                                         GPUVertBuf *pos_nor);

void draw_subdiv_extract_pos_nor(const DRWSubdivCache &cache,
                                 GPUVertBuf *flags_buffer,
                                 GPUVertBuf *pos_nor,
                                 GPUVertBuf *orco);

void draw_subdiv_interp_custom_data(const DRWSubdivCache &cache,
                                    GPUVertBuf *src_data,
                                    GPUVertBuf *dst_data,
                                    int comp_type, /*GPUVertCompType*/
                                    int dimensions,
                                    int dst_offset);

void draw_subdiv_extract_uvs(const DRWSubdivCache &cache,
                             GPUVertBuf *uvs,
                             int face_varying_channel,
                             int dst_offset);

void draw_subdiv_build_edge_fac_buffer(const DRWSubdivCache &cache,
                                       GPUVertBuf *pos_nor,
                                       GPUVertBuf *edge_draw_flag,
                                       GPUVertBuf *poly_other_map,
                                       GPUVertBuf *edge_fac);

void draw_subdiv_build_tris_buffer(const DRWSubdivCache &cache,
                                   GPUIndexBuf *subdiv_tris,
                                   int material_count);

void draw_subdiv_build_lines_buffer(const DRWSubdivCache &cache, GPUIndexBuf *lines_indices);

void draw_subdiv_build_lines_loose_buffer(const DRWSubdivCache &cache,
                                          GPUIndexBuf *lines_indices,
                                          GPUVertBuf *lines_flags,
                                          uint num_loose_edges);

void draw_subdiv_build_fdots_buffers(const DRWSubdivCache &cache,
                                     GPUVertBuf *fdots_pos,
                                     GPUVertBuf *fdots_nor,
                                     GPUIndexBuf *fdots_indices);

void draw_subdiv_build_lnor_buffer(const DRWSubdivCache &cache,
                                   GPUVertBuf *pos_nor,
                                   GPUVertBuf *lnor);

void draw_subdiv_build_edituv_stretch_area_buffer(const DRWSubdivCache &cache,
                                                  GPUVertBuf *coarse_data,
                                                  GPUVertBuf *subdiv_data);

void draw_subdiv_build_edituv_stretch_angle_buffer(const DRWSubdivCache &cache,
                                                   GPUVertBuf *pos_nor,
                                                   GPUVertBuf *uvs,
                                                   int uvs_offset,
                                                   GPUVertBuf *stretch_angles);

/** Return the format used for the positions and normals VBO. */
GPUVertFormat *draw_subdiv_get_pos_nor_format();

/* Helper to access the loose edges. */
Span<DRWSubdivLooseEdge> draw_subdiv_cache_get_loose_edges(const DRWSubdivCache &cache);

/* Helper to access only the loose vertices, i.e. not the ones attached to loose edges. To access
 * loose vertices of loose edges #draw_subdiv_cache_get_loose_edges should be used. */
Span<DRWSubdivLooseVertex> draw_subdiv_cache_get_loose_verts(const DRWSubdivCache &cache);

}  // namespace blender::draw
