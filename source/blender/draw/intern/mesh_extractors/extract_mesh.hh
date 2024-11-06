/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Extraction of Mesh data into VBO to feed to GPU.
 */

#pragma once

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

#include "DNA_scene_types.h"

#include "BKE_mesh.hh"

#include "bmesh.hh"

#include "GPU_vertex_buffer.hh"
#include "GPU_vertex_format.hh"

#include "draw_cache_extract.hh"

struct DRWSubdivCache;
struct BMVert;
struct BMEdge;
struct BMEditMesh;
struct BMFace;
struct BMLoop;

namespace blender::draw {

#define MIN_RANGE_LEN 1024

/* ---------------------------------------------------------------------- */
/** \name Mesh Render Data
 * \{ */

enum eMRExtractType {
  MR_EXTRACT_BMESH,
  MR_EXTRACT_MESH,
};

struct MeshRenderData {
  eMRExtractType extract_type;

  int verts_num;
  int edges_num;
  int faces_num;
  int corners_num;

  int loose_edges_num;
  int loose_verts_num;
  int loose_indices_num;

  int corner_tris_num;
  int materials_num;

  bool use_hide;
  bool use_subsurf_fdots;
  bool use_final_mesh;
  bool hide_unmapped_edges;
  bool use_simplify_normals;

  /** Use for #MeshStatVis calculation which use world-space coords. */
  float4x4 object_to_world;

  const ToolSettings *toolsettings;
  /** Edit Mesh */
  BMEditMesh *edit_bmesh;
  BMesh *bm;
  bke::EditMeshData *edit_data;

  /* For deformed edit-mesh data. */
  /* Use for #ME_WRAPPER_TYPE_BMESH. */
  Span<float3> bm_vert_coords;
  Span<float3> bm_vert_normals;
  Span<float3> bm_face_normals;
  Array<float3> bm_loop_normals;

  const int *orig_index_vert;
  const int *orig_index_edge;
  const int *orig_index_face;
  int edge_crease_ofs;
  int vert_crease_ofs;
  int bweight_ofs;
  int freestyle_edge_ofs;
  int freestyle_face_ofs;
  /** Mesh */
  const Mesh *mesh;
  Span<float3> vert_positions;
  Span<int2> edges;
  OffsetIndices<int> faces;
  Span<int> corner_verts;
  Span<int> corner_edges;

  BMVert *eve_act;
  BMEdge *eed_act;
  BMFace *efa_act;
  BMFace *efa_act_uv;
  VArraySpan<int> material_indices;

  bke::MeshNormalDomain normals_domain;
  Span<float3> face_normals;
  Span<float3> corner_normals;

  VArraySpan<bool> hide_vert;
  VArraySpan<bool> hide_edge;
  VArraySpan<bool> hide_poly;
  VArraySpan<bool> select_vert;
  VArraySpan<bool> select_edge;
  VArraySpan<bool> select_poly;
  VArraySpan<bool> sharp_faces;

  Span<int> loose_verts;
  Span<int> loose_edges;

  const char *active_color_name;
  const char *default_color_name;
};

const Mesh &editmesh_final_or_this(const Object &object, const Mesh &mesh);
const CustomData &mesh_cd_vdata_get_from_mesh(const Mesh &mesh);
const CustomData &mesh_cd_edata_get_from_mesh(const Mesh &mesh);
const CustomData &mesh_cd_pdata_get_from_mesh(const Mesh &mesh);
const CustomData &mesh_cd_ldata_get_from_mesh(const Mesh &mesh);

BLI_INLINE BMFace *bm_original_face_get(const MeshRenderData &mr, int idx)
{
  return ((mr.orig_index_face != nullptr) && (mr.orig_index_face[idx] != ORIGINDEX_NONE) &&
          mr.bm) ?
             BM_face_at_index(mr.bm, mr.orig_index_face[idx]) :
             nullptr;
}

BLI_INLINE BMEdge *bm_original_edge_get(const MeshRenderData &mr, int idx)
{
  return ((mr.orig_index_edge != nullptr) && (mr.orig_index_edge[idx] != ORIGINDEX_NONE) &&
          mr.bm) ?
             BM_edge_at_index(mr.bm, mr.orig_index_edge[idx]) :
             nullptr;
}

BLI_INLINE BMVert *bm_original_vert_get(const MeshRenderData &mr, int idx)
{
  return ((mr.orig_index_vert != nullptr) && (mr.orig_index_vert[idx] != ORIGINDEX_NONE) &&
          mr.bm) ?
             BM_vert_at_index(mr.bm, mr.orig_index_vert[idx]) :
             nullptr;
}

BLI_INLINE const float *bm_vert_co_get(const MeshRenderData &mr, const BMVert *eve)
{
  if (!mr.bm_vert_coords.is_empty()) {
    return mr.bm_vert_coords[BM_elem_index_get(eve)];
  }
  return eve->co;
}

BLI_INLINE const float *bm_vert_no_get(const MeshRenderData &mr, const BMVert *eve)
{
  if (!mr.bm_vert_normals.is_empty()) {
    return mr.bm_vert_normals[BM_elem_index_get(eve)];
  }
  return eve->no;
}

BLI_INLINE const float *bm_face_no_get(const MeshRenderData &mr, const BMFace *efa)
{
  if (!mr.bm_face_normals.is_empty()) {
    return mr.bm_face_normals[BM_elem_index_get(efa)];
  }
  return efa->no;
}

/** \} */

/* `draw_cache_extract_mesh_render_data.cc` */

/**
 * \param edit_mode_active: When true, use the modifiers from the edit-data,
 * otherwise don't use modifiers as they are not from this object.
 */
std::unique_ptr<MeshRenderData> mesh_render_data_create(Object &object,
                                                        Mesh &mesh,
                                                        bool is_editmode,
                                                        bool is_paint_mode,
                                                        const float4x4 &object_to_world,
                                                        bool do_final,
                                                        bool do_uvedit,
                                                        bool use_hide,
                                                        const ToolSettings *ts);
void mesh_render_data_update_corner_normals(MeshRenderData &mr);
void mesh_render_data_update_face_normals(MeshRenderData &mr);
void mesh_render_data_update_loose_geom(MeshRenderData &mr, MeshBufferCache &cache);
const SortedFaceData &mesh_render_data_faces_sorted_ensure(const MeshRenderData &mr,
                                                           MeshBufferCache &cache);

/* draw_cache_extract_mesh_extractors.c */

struct EditLoopData {
  uchar v_flag;
  uchar e_flag;
  /* This is used for both vertex and edge creases. The edge crease value is stored in the bottom 4
   * bits, while the vertex crease is stored in the upper 4 bits. */
  uchar crease;
  uchar bweight;
};

void mesh_render_data_face_flag(const MeshRenderData &mr,
                                const BMFace *efa,
                                BMUVOffsets offsets,
                                EditLoopData &eattr);
void mesh_render_data_loop_flag(const MeshRenderData &mr,
                                const BMLoop *l,
                                BMUVOffsets offsets,
                                EditLoopData &eattr);
void mesh_render_data_loop_edge_flag(const MeshRenderData &mr,
                                     const BMLoop *l,
                                     BMUVOffsets offsets,
                                     EditLoopData &eattr);

template<typename GPUType> inline GPUType convert_normal(const float3 &src);

template<> inline GPUPackedNormal convert_normal(const float3 &src)
{
  return GPU_normal_convert_i10_v3(src);
}

template<> inline short4 convert_normal(const float3 &src)
{
  short4 dst;
  normal_float_to_short_v3(dst, src);
  return dst;
}

template<typename GPUType> void convert_normals(Span<float3> src, MutableSpan<GPUType> dst);

template<typename T>
void extract_mesh_loose_edge_data(const Span<T> vert_data,
                                  const Span<int2> edges,
                                  const Span<int> loose_edges,
                                  MutableSpan<T> gpu_data)
{
  threading::parallel_for(loose_edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const int2 edge = edges[loose_edges[i]];
      gpu_data[i * 2 + 0] = vert_data[edge[0]];
      gpu_data[i * 2 + 1] = vert_data[edge[1]];
    }
  });
}

void extract_positions(const MeshRenderData &mr, gpu::VertBuf &vbo);
void extract_positions_subdiv(const DRWSubdivCache &subdiv_cache,
                              const MeshRenderData &mr,
                              gpu::VertBuf &vbo,
                              gpu::VertBuf *orco_vbo);

void extract_face_dots_position(const MeshRenderData &mr, gpu::VertBuf &vbo);
void extract_face_dots_subdiv(const DRWSubdivCache &subdiv_cache,
                              gpu::VertBuf &fdots_pos,
                              gpu::VertBuf *fdots_nor,
                              gpu::IndexBuf &fdots);

void extract_normals(const MeshRenderData &mr, bool use_hq, gpu::VertBuf &vbo);
void extract_normals_subdiv(const MeshRenderData &mr,
                            const DRWSubdivCache &subdiv_cache,
                            gpu::VertBuf &pos_nor,
                            gpu::VertBuf &lnor);
void extract_vert_normals(const MeshRenderData &mr, gpu::VertBuf &vbo);
void extract_face_dot_normals(const MeshRenderData &mr, const bool use_hq, gpu::VertBuf &vbo);
void extract_edge_factor(const MeshRenderData &mr, gpu::VertBuf &vbo);
void extract_edge_factor_subdiv(const DRWSubdivCache &subdiv_cache,
                                const MeshRenderData &mr,
                                gpu::VertBuf &pos_nor,
                                gpu::VertBuf &vbo);

void extract_tris(const MeshRenderData &mr,
                  const SortedFaceData &face_sorted,
                  MeshBatchCache &cache,
                  gpu::IndexBuf &ibo);
void extract_tris_subdiv(const DRWSubdivCache &subdiv_cache,
                         MeshBatchCache &cache,
                         gpu::IndexBuf &ibo);

void extract_lines(const MeshRenderData &mr,
                   gpu::IndexBuf *lines,
                   gpu::IndexBuf *lines_loose,
                   bool &no_loose_wire);
void extract_lines_subdiv(const DRWSubdivCache &subdiv_cache,
                          const MeshRenderData &mr,
                          gpu::IndexBuf *lines,
                          gpu::IndexBuf *lines_loose,
                          bool &no_loose_wire);

void extract_points(const MeshRenderData &mr, gpu::IndexBuf &points);
void extract_points_subdiv(const MeshRenderData &mr,
                           const DRWSubdivCache &subdiv_cache,
                           gpu::IndexBuf &points);

void extract_edit_data(const MeshRenderData &mr, gpu::VertBuf &vbo);
void extract_edit_data_subdiv(const MeshRenderData &mr,
                              const DRWSubdivCache &subdiv_cache,
                              gpu::VertBuf &vbo);

void extract_tangents(const MeshRenderData &mr,
                      const MeshBatchCache &cache,
                      const bool use_hq,
                      gpu::VertBuf &vbo);
void extract_tangents_subdiv(const MeshRenderData &mr,
                             const DRWSubdivCache &subdiv_cache,
                             const MeshBatchCache &cache,
                             gpu::VertBuf &vbo);

void extract_vert_index(const MeshRenderData &mr, gpu::VertBuf &vbo);
void extract_edge_index(const MeshRenderData &mr, gpu::VertBuf &vbo);
void extract_face_index(const MeshRenderData &mr, gpu::VertBuf &vbo);
void extract_face_dot_index(const MeshRenderData &mr, gpu::VertBuf &vbo);

void extract_vert_index_subdiv(const DRWSubdivCache &subdiv_cache,
                               const MeshRenderData &mr,
                               gpu::VertBuf &vbo);
void extract_edge_index_subdiv(const DRWSubdivCache &subdiv_cache,
                               const MeshRenderData &mr,
                               gpu::VertBuf &vbo);
void extract_face_index_subdiv(const DRWSubdivCache &subdiv_cache,
                               const MeshRenderData &mr,
                               gpu::VertBuf &vbo);

void extract_weights(const MeshRenderData &mr, const MeshBatchCache &cache, gpu::VertBuf &vbo);
void extract_weights_subdiv(const MeshRenderData &mr,
                            const DRWSubdivCache &subdiv_cache,
                            const MeshBatchCache &cache,
                            gpu::VertBuf &vbo);

void extract_face_dots(const MeshRenderData &mr, gpu::IndexBuf &face_dots);

void extract_face_dots_uv(const MeshRenderData &mr, gpu::VertBuf &vbo);
void extract_face_dots_edituv_data(const MeshRenderData &mr, gpu::VertBuf &vbo);

void extract_lines_paint_mask(const MeshRenderData &mr, gpu::IndexBuf &lines);
void extract_lines_paint_mask_subdiv(const MeshRenderData &mr,
                                     const DRWSubdivCache &subdiv_cache,
                                     gpu::IndexBuf &lines);

void extract_lines_adjacency(const MeshRenderData &mr, gpu::IndexBuf &ibo, bool &r_is_manifold);
void extract_lines_adjacency_subdiv(const DRWSubdivCache &subdiv_cache,
                                    gpu::IndexBuf &ibo,
                                    bool &r_is_manifold);

void extract_uv_maps(const MeshRenderData &mr, const MeshBatchCache &cache, gpu::VertBuf &vbo);
void extract_uv_maps_subdiv(const DRWSubdivCache &subdiv_cache,
                            const MeshBatchCache &cache,
                            gpu::VertBuf &vbo);
void extract_edituv_stretch_area(const MeshRenderData &mr,
                                 gpu::VertBuf &vbo,
                                 float &tot_area,
                                 float &tot_uv_area);
void extract_edituv_stretch_area_subdiv(const MeshRenderData &mr,
                                        const DRWSubdivCache &subdiv_cache,
                                        gpu::VertBuf &vbo,
                                        float &tot_area,
                                        float &tot_uv_area);
void extract_edituv_stretch_angle(const MeshRenderData &mr, gpu::VertBuf &vbo);
void extract_edituv_stretch_angle_subdiv(const MeshRenderData &mr,
                                         const DRWSubdivCache &subdiv_cache,
                                         const MeshBatchCache &cache,
                                         gpu::VertBuf &vbo);
void extract_edituv_data(const MeshRenderData &mr, gpu::VertBuf &vbo);
void extract_edituv_data_subdiv(const MeshRenderData &mr,
                                const DRWSubdivCache &subdiv_cache,
                                gpu::VertBuf &vbo);
void extract_edituv_tris(const MeshRenderData &mr, gpu::IndexBuf &ibo);
void extract_edituv_tris_subdiv(const MeshRenderData &mr,
                                const DRWSubdivCache &subdiv_cache,
                                gpu::IndexBuf &ibo);
void extract_edituv_lines(const MeshRenderData &mr, gpu::IndexBuf &ibo);
void extract_edituv_lines_subdiv(const MeshRenderData &mr,
                                 const DRWSubdivCache &subdiv_cache,
                                 gpu::IndexBuf &ibo);
void extract_edituv_points(const MeshRenderData &mr, gpu::IndexBuf &ibo);
void extract_edituv_points_subdiv(const MeshRenderData &mr,
                                  const DRWSubdivCache &subdiv_cache,
                                  gpu::IndexBuf &ibo);
void extract_edituv_face_dots(const MeshRenderData &mr, gpu::IndexBuf &ibo);

void extract_skin_roots(const MeshRenderData &mr, gpu::VertBuf &vbo);

void extract_sculpt_data(const MeshRenderData &mr, gpu::VertBuf &vbo);
void extract_sculpt_data_subdiv(const MeshRenderData &mr,
                                const DRWSubdivCache &subdiv_cache,
                                gpu::VertBuf &vbo);

void extract_orco(const MeshRenderData &mr, gpu::VertBuf &vbo);

void extract_mesh_analysis(const MeshRenderData &mr, gpu::VertBuf &vbo);

void extract_attributes(const MeshRenderData &mr,
                        const Span<DRW_AttributeRequest> requests,
                        const Span<gpu::VertBuf *> vbos);
void extract_attributes_subdiv(const MeshRenderData &mr,
                               const DRWSubdivCache &subdiv_cache,
                               const Span<DRW_AttributeRequest> requests,
                               const Span<gpu::VertBuf *> vbos);
void extract_attr_viewer(const MeshRenderData &mr, gpu::VertBuf &vbo);

}  // namespace blender::draw
