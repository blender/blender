/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_sys_types.h"

#include "mesh_extractors/extract_mesh.hh"

struct BMesh;
namespace blender::gpu {
class IndexBuf;
class UniformBuf;
class VertBuf;
}  // namespace blender::gpu
struct GPUVertFormat;
struct Mesh;
struct Object;
namespace blender::bke::subdiv {
struct Subdiv;
}
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
  gpu::VertBuf *patch_map_handles;
  gpu::VertBuf *patch_map_quadtree;
  int min_patch_face;
  int max_patch_face;
  int max_depth;
  bool patches_are_triangular;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRWSubdivCache
 *
 * This holds the various buffers used to evaluate and render subdivision through OpenGL.
 * \{ */

struct DRWSubdivCache {
  const Mesh *mesh;
  BMesh *bm;
  bke::subdiv::Subdiv *subdiv;
  bool optimal_display;
  bool hide_unmapped_edges;
  bool use_custom_loop_normals;

  /* Coordinates used to evaluate patches for positions and normals. */
  gpu::VertBuf *patch_coords;
  /* Coordinates used to evaluate patches for attributes. */
  gpu::VertBuf *corner_patch_coords;
  /* Coordinates used to evaluate patches for the face centers (or face dots) in edit-mode. */
  gpu::VertBuf *fdots_patch_coords;

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
  gpu::VertBuf *subdiv_vert_face_adjacency;
  /* The difference between value (i + 1) and (i) gives the number of faces adjacent to vertex (i).
   */
  gpu::VertBuf *subdiv_vert_face_adjacency_offsets;

  /* Maps subdivision loop to original coarse vertex index, only really useful for edit mode. */
  gpu::VertBuf *verts_orig_index;
  /* Maps subdivision loop to original coarse edge index, only really useful for edit mode. */
  gpu::VertBuf *edges_orig_index;
  /* Indicates if edge should be drawn in optimal display mode. */
  gpu::VertBuf *edges_draw_flag;

  /* Owned by #Subdiv. Indexed by coarse face index, difference between value (i + 1) and (i)
   * gives the number of ptex faces for coarse face (i). */
  Span<int> face_ptex_offset;
  /* Vertex buffer for face_ptex_offset. */
  gpu::VertBuf *face_ptex_offset_buffer;

  int *subdiv_face_offset;
  gpu::VertBuf *subdiv_face_offset_buffer;

  /* Contains the start loop index and the smooth flag for each coarse face. */
  gpu::VertBuf *extra_coarse_face_data;

  /* Material offsets. */
  int *mat_start;
  int *mat_end;
  gpu::VertBuf *face_mat_offset;

  DRWPatchMap gpu_patch_map;

  /**
   * Subdivided vertices of loose edges. The size of this array is the number of loose edges
   * multiplied with the resolution. For storage in the VBO the data is duplicated for each edge.
   */
  Array<float3> loose_edge_positions;

  /* UBO to store settings for the various compute shaders. */
  gpu::UniformBuf *ubo;

  /* Extra flags, passed to the UBO. */
  bool is_edit_mode;
  bool use_hide;
};

/* Only frees the data of the cache, caller is responsible to free the cache itself if necessary.
 */
void draw_subdiv_cache_free(DRWSubdivCache &cache);

/** \} */

void DRW_create_subdivision(Object &ob,
                            Mesh &mesh,
                            MeshBatchCache &batch_cache,
                            MeshBufferCache &mbc,
                            Span<IBOType> ibo_requests,
                            Span<VBOType> vbo_requests,
                            bool is_editmode,
                            bool is_paint_mode,
                            bool do_final,
                            bool do_uvedit,
                            bool do_cage,
                            const ToolSettings *ts,
                            bool use_hide);

void DRW_subdivide_loose_geom(DRWSubdivCache &subdiv_cache, const MeshBufferCache &cache);

void DRW_subdiv_cache_free(bke::subdiv::Subdiv *subdiv);

gpu::VertBufPtr draw_subdiv_init_origindex_buffer(int32_t *vert_origindex,
                                                  uint num_loops,
                                                  uint loose_len);

gpu::VertBuf *draw_subdiv_build_origindex_buffer(int *vert_origindex, uint num_loops);
gpu::VertBufPtr draw_subdiv_init_origindex_buffer(Span<int32_t> vert_origindex, uint loose_len);
gpu::VertBuf *draw_subdiv_build_origindex_buffer(Span<int> vert_origindex);

/* Compute shader functions. */

void draw_subdiv_build_sculpt_data_buffer(const DRWSubdivCache &cache,
                                          gpu::VertBuf *mask_vbo,
                                          gpu::VertBuf *face_set_vbo,
                                          gpu::VertBuf *sculpt_data);

void draw_subdiv_accumulate_normals(const DRWSubdivCache &cache,
                                    gpu::VertBuf *pos,
                                    gpu::VertBuf *face_adjacency_offsets,
                                    gpu::VertBuf *face_adjacency_lists,
                                    gpu::VertBuf *vert_loop_map,
                                    gpu::VertBuf *vert_normals);

void draw_subdiv_extract_pos(const DRWSubdivCache &cache, gpu::VertBuf *pos, gpu::VertBuf *orco);

void draw_subdiv_interp_custom_data(const DRWSubdivCache &cache,
                                    gpu::VertBuf &src_data,
                                    gpu::VertBuf &dst_data,
                                    GPUVertCompType comp_type,
                                    int dimensions,
                                    int dst_offset);

void draw_subdiv_interp_corner_normals(const DRWSubdivCache &cache,
                                       gpu::VertBuf &src_data,
                                       gpu::VertBuf &dst_data);

void draw_subdiv_extract_uvs(const DRWSubdivCache &cache,
                             gpu::VertBuf *uvs,
                             int face_varying_channel,
                             int dst_offset);

void draw_subdiv_build_edge_fac_buffer(const DRWSubdivCache &cache,
                                       gpu::VertBuf *pos,
                                       gpu::VertBuf *edge_draw_flag,
                                       gpu::VertBuf *poly_other_map,
                                       gpu::VertBuf *edge_fac);

void draw_subdiv_build_tris_buffer(const DRWSubdivCache &cache,
                                   gpu::IndexBuf *subdiv_tris,
                                   int material_count);

void draw_subdiv_build_lines_buffer(const DRWSubdivCache &cache, gpu::IndexBuf *lines_indices);

void draw_subdiv_build_lines_loose_buffer(const DRWSubdivCache &cache,
                                          gpu::IndexBuf *lines_indices,
                                          gpu::VertBuf *lines_flags,
                                          uint edge_loose_offset,
                                          uint num_loose_edges);

void draw_subdiv_build_fdots_buffers(const DRWSubdivCache &cache,
                                     gpu::VertBuf *fdots_pos,
                                     gpu::VertBuf *fdots_nor,
                                     gpu::IndexBuf *fdots_indices);

void draw_subdiv_build_lnor_buffer(const DRWSubdivCache &cache,
                                   gpu::VertBuf *pos,
                                   gpu::VertBuf *vert_normals,
                                   gpu::VertBuf *subdiv_corner_verts,
                                   gpu::VertBuf *lnor);

void draw_subdiv_build_paint_overlay_flag_buffer(const DRWSubdivCache &cache, gpu::VertBuf &flags);

void draw_subdiv_build_edituv_stretch_area_buffer(const DRWSubdivCache &cache,
                                                  gpu::VertBuf *coarse_data,
                                                  gpu::VertBuf *subdiv_data);

void draw_subdiv_build_edituv_stretch_angle_buffer(const DRWSubdivCache &cache,
                                                   gpu::VertBuf *pos,
                                                   gpu::VertBuf *uvs,
                                                   int uvs_offset,
                                                   gpu::VertBuf *stretch_angles);

/** For every coarse edge, there are `resolution - 1` subdivided edges. */
inline int subdiv_edges_per_coarse_edge(const DRWSubdivCache &cache)
{
  return cache.resolution - 1;
}

/** For every subdivided edge, there are two coarse vertices stored in vertex buffers. */
inline int subdiv_verts_per_coarse_edge(const DRWSubdivCache &cache)
{
  return subdiv_edges_per_coarse_edge(cache) * 2;
}

/** The number of subdivided edges from base mesh loose edges. */
inline int subdiv_loose_edges_num(const MeshRenderData &mr, const DRWSubdivCache &cache)
{
  return mr.loose_edges.size() * subdiv_edges_per_coarse_edge(cache);
}

/** Size of vertex buffers including all face corners, loose edges, and loose vertices. */
inline int subdiv_full_vbo_size(const MeshRenderData &mr, const DRWSubdivCache &cache)
{
  return cache.num_subdiv_loops + subdiv_loose_edges_num(mr, cache) * 2 + mr.loose_verts.size();
}

}  // namespace blender::draw
