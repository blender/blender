/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */

/** \file
 * \ingroup draw
 */

#include "draw_subdivision.h"
#include "extract_mesh.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Selection Index
 * \{ */

static void extract_select_idx_init_impl(const MeshRenderData * /*mr*/,
                                         const int len,
                                         void *buf,
                                         void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "index", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, len);
  *(int32_t **)tls_data = (int32_t *)GPU_vertbuf_get_data(vbo);
}

static void extract_select_idx_init(const MeshRenderData *mr,
                                    MeshBatchCache * /*cache*/,
                                    void *buf,
                                    void *tls_data)
{
  extract_select_idx_init_impl(mr, mr->loop_len + mr->loop_loose_len, buf, tls_data);
}

/* TODO: Use #glVertexID to get loop index and use the data structure on the CPU to retrieve the
 * select element associated with this loop ID. This would remove the need for this separate
 * index VBO's. We could upload the p/e/v_origindex as a buffer texture and sample it inside the
 * shader to output original index. */

static void extract_poly_idx_iter_poly_bm(const MeshRenderData * /*mr*/,
                                          const BMFace *f,
                                          const int f_index,
                                          void *data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    (*(int32_t **)data)[l_index] = f_index;
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edge_idx_iter_poly_bm(const MeshRenderData * /*mr*/,
                                          const BMFace *f,
                                          const int /*f_index*/,
                                          void *data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    (*(int32_t **)data)[l_index] = BM_elem_index_get(l_iter->e);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_vert_idx_iter_poly_bm(const MeshRenderData * /*mr*/,
                                          const BMFace *f,
                                          const int /*f_index*/,
                                          void *data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    (*(int32_t **)data)[l_index] = BM_elem_index_get(l_iter->v);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edge_idx_iter_loose_edge_bm(const MeshRenderData *mr,
                                                const BMEdge *eed,
                                                const int ledge_index,
                                                void *data)
{
  (*(int32_t **)data)[mr->loop_len + ledge_index * 2 + 0] = BM_elem_index_get(eed);
  (*(int32_t **)data)[mr->loop_len + ledge_index * 2 + 1] = BM_elem_index_get(eed);
}

static void extract_vert_idx_iter_loose_edge_bm(const MeshRenderData *mr,
                                                const BMEdge *eed,
                                                const int ledge_index,
                                                void *data)
{
  (*(int32_t **)data)[mr->loop_len + ledge_index * 2 + 0] = BM_elem_index_get(eed->v1);
  (*(int32_t **)data)[mr->loop_len + ledge_index * 2 + 1] = BM_elem_index_get(eed->v2);
}

static void extract_vert_idx_iter_loose_vert_bm(const MeshRenderData *mr,
                                                const BMVert *eve,
                                                const int lvert_index,
                                                void *data)
{
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  (*(int32_t **)data)[offset + lvert_index] = BM_elem_index_get(eve);
}

static void extract_poly_idx_iter_poly_mesh(const MeshRenderData *mr,
                                            const int poly_index,
                                            void *data)
{
  for (const int ml_index : mr->polys[poly_index]) {
    (*(int32_t **)data)[ml_index] = (mr->p_origindex) ? mr->p_origindex[poly_index] : poly_index;
  }
}

static void extract_edge_idx_iter_poly_mesh(const MeshRenderData *mr,
                                            const int poly_index,
                                            void *data)
{
  for (const int ml_index : mr->polys[poly_index]) {
    const int edge = mr->corner_edges[ml_index];
    (*(int32_t **)data)[ml_index] = (mr->e_origindex) ? mr->e_origindex[edge] : edge;
  }
}

static void extract_vert_idx_iter_poly_mesh(const MeshRenderData *mr,
                                            const int poly_index,
                                            void *data)
{
  for (const int ml_index : mr->polys[poly_index]) {
    const int vert = mr->corner_verts[ml_index];
    (*(int32_t **)data)[ml_index] = (mr->v_origindex) ? mr->v_origindex[vert] : vert;
  }
}

static void extract_edge_idx_iter_loose_edge_mesh(const MeshRenderData *mr,
                                                  const int2 /*edge*/,
                                                  const int ledge_index,
                                                  void *data)
{
  const int e_index = mr->loose_edges[ledge_index];
  const int e_orig = (mr->e_origindex) ? mr->e_origindex[e_index] : e_index;
  (*(int32_t **)data)[mr->loop_len + ledge_index * 2 + 0] = e_orig;
  (*(int32_t **)data)[mr->loop_len + ledge_index * 2 + 1] = e_orig;
}

static void extract_vert_idx_iter_loose_edge_mesh(const MeshRenderData *mr,
                                                  const int2 edge,
                                                  const int ledge_index,
                                                  void *data)
{
  int v1_orig = (mr->v_origindex) ? mr->v_origindex[edge[0]] : edge[0];
  int v2_orig = (mr->v_origindex) ? mr->v_origindex[edge[1]] : edge[1];
  (*(int32_t **)data)[mr->loop_len + ledge_index * 2 + 0] = v1_orig;
  (*(int32_t **)data)[mr->loop_len + ledge_index * 2 + 1] = v2_orig;
}

static void extract_vert_idx_iter_loose_vert_mesh(const MeshRenderData *mr,
                                                  const int lvert_index,
                                                  void *data)
{
  const int offset = mr->loop_len + (mr->edge_loose_len * 2);

  const int v_index = mr->loose_verts[lvert_index];
  const int v_orig = (mr->v_origindex) ? mr->v_origindex[v_index] : v_index;
  (*(int32_t **)data)[offset + lvert_index] = v_orig;
}

static void extract_vert_idx_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                         const MeshRenderData *mr,
                                         MeshBatchCache * /*cache*/,
                                         void *buf,
                                         void * /*data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache->loose_geom;
  /* Each element points to an element in the `ibo.points`. */
  draw_subdiv_init_origindex_buffer(
      vbo,
      (int32_t *)GPU_vertbuf_get_data(subdiv_cache->verts_orig_index),
      subdiv_cache->num_subdiv_loops,
      loose_geom.loop_len);
  if (!mr->v_origindex) {
    return;
  }

  /* Remap the vertex indices to those pointed by the origin indices layer. At this point, the
   * VBO data is a copy of #verts_orig_index which contains the coarse vertices indices, so
   * the memory can both be accessed for lookup and immediately overwritten. */
  int32_t *vbo_data = static_cast<int32_t *>(GPU_vertbuf_get_data(vbo));
  for (int i = 0; i < subdiv_cache->num_subdiv_loops; i++) {
    if (vbo_data[i] == -1) {
      continue;
    }
    vbo_data[i] = mr->v_origindex[vbo_data[i]];
  }
}

static void extract_vert_idx_loose_geom_subdiv(const DRWSubdivCache *subdiv_cache,
                                               const MeshRenderData *mr,
                                               void *buffer,
                                               void * /*data*/)
{
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache->loose_geom;
  if (loose_geom.loop_len == 0) {
    return;
  }

  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);
  int32_t *vert_idx_data = (int32_t *)GPU_vertbuf_get_data(vbo);
  uint offset = subdiv_cache->num_subdiv_loops;

  blender::Span<DRWSubdivLooseEdge> loose_edges = draw_subdiv_cache_get_loose_edges(subdiv_cache);

  for (const DRWSubdivLooseEdge &loose_edge : loose_edges) {
    const DRWSubdivLooseVertex &v1 = loose_geom.verts[loose_edge.loose_subdiv_v1_index];
    const DRWSubdivLooseVertex &v2 = loose_geom.verts[loose_edge.loose_subdiv_v2_index];

    if (v1.coarse_vertex_index != -1u) {
      vert_idx_data[offset] = mr->v_origindex ? mr->v_origindex[v1.coarse_vertex_index] :
                                                v1.coarse_vertex_index;
    }

    if (v2.coarse_vertex_index != -1u) {
      vert_idx_data[offset + 1] = mr->v_origindex ? mr->v_origindex[v2.coarse_vertex_index] :
                                                    v2.coarse_vertex_index;
    }

    offset += 2;
  }

  blender::Span<DRWSubdivLooseVertex> loose_verts = draw_subdiv_cache_get_loose_verts(
      subdiv_cache);

  for (const DRWSubdivLooseVertex &loose_vert : loose_verts) {
    vert_idx_data[offset] = mr->v_origindex ? mr->v_origindex[loose_vert.coarse_vertex_index] :
                                              loose_vert.coarse_vertex_index;
    offset += 1;
  }
}

static void extract_edge_idx_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                         const MeshRenderData * /*mr*/,
                                         MeshBatchCache * /*cache*/,
                                         void *buf,
                                         void * /*data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache->loose_geom;
  draw_subdiv_init_origindex_buffer(
      vbo,
      static_cast<int32_t *>(GPU_vertbuf_get_data(subdiv_cache->edges_orig_index)),
      subdiv_cache->num_subdiv_loops,
      loose_geom.edge_len * 2);
}

static void extract_edge_idx_loose_geom_subdiv(const DRWSubdivCache *subdiv_cache,
                                               const MeshRenderData *mr,
                                               void *buffer,
                                               void * /*data*/)
{
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache->loose_geom;
  if (loose_geom.edge_len == 0) {
    return;
  }

  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);
  int32_t *vert_idx_data = (int32_t *)GPU_vertbuf_get_data(vbo);
  uint offset = subdiv_cache->num_subdiv_loops;

  blender::Span<DRWSubdivLooseEdge> loose_edges = draw_subdiv_cache_get_loose_edges(subdiv_cache);
  for (const DRWSubdivLooseEdge &loose_edge : loose_edges) {
    const int coarse_edge_index = mr->e_origindex ? mr->e_origindex[loose_edge.coarse_edge_index] :
                                                    loose_edge.coarse_edge_index;
    vert_idx_data[offset] = coarse_edge_index;
    vert_idx_data[offset + 1] = coarse_edge_index;
    offset += 2;
  }
}

static void extract_poly_idx_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                         const MeshRenderData *mr,
                                         MeshBatchCache * /*cache*/,
                                         void *buf,
                                         void * /*data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  draw_subdiv_init_origindex_buffer(
      vbo, subdiv_cache->subdiv_loop_poly_index, subdiv_cache->num_subdiv_loops, 0);

  if (!mr->p_origindex) {
    return;
  }

  /* Remap the polygon indices to those pointed by the origin indices layer. At this point, the
   * VBO data is a copy of #subdiv_loop_poly_index which contains the coarse polygon indices, so
   * the memory can both be accessed for lookup and immediately overwritten. */
  int32_t *vbo_data = static_cast<int32_t *>(GPU_vertbuf_get_data(vbo));
  for (int i = 0; i < subdiv_cache->num_subdiv_loops; i++) {
    vbo_data[i] = mr->p_origindex[vbo_data[i]];
  }
}

constexpr MeshExtract create_extractor_poly_idx()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_select_idx_init;
  extractor.iter_poly_bm = extract_poly_idx_iter_poly_bm;
  extractor.iter_poly_mesh = extract_poly_idx_iter_poly_mesh;
  extractor.init_subdiv = extract_poly_idx_init_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(int32_t *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.poly_idx);
  return extractor;
}

constexpr MeshExtract create_extractor_edge_idx()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_select_idx_init;
  extractor.iter_poly_bm = extract_edge_idx_iter_poly_bm;
  extractor.iter_poly_mesh = extract_edge_idx_iter_poly_mesh;
  extractor.iter_loose_edge_bm = extract_edge_idx_iter_loose_edge_bm;
  extractor.iter_loose_edge_mesh = extract_edge_idx_iter_loose_edge_mesh;
  extractor.init_subdiv = extract_edge_idx_init_subdiv;
  extractor.iter_loose_geom_subdiv = extract_edge_idx_loose_geom_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(int32_t *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.edge_idx);
  return extractor;
}

constexpr MeshExtract create_extractor_vert_idx()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_select_idx_init;
  extractor.iter_poly_bm = extract_vert_idx_iter_poly_bm;
  extractor.iter_poly_mesh = extract_vert_idx_iter_poly_mesh;
  extractor.iter_loose_edge_bm = extract_vert_idx_iter_loose_edge_bm;
  extractor.iter_loose_edge_mesh = extract_vert_idx_iter_loose_edge_mesh;
  extractor.iter_loose_vert_bm = extract_vert_idx_iter_loose_vert_bm;
  extractor.iter_loose_vert_mesh = extract_vert_idx_iter_loose_vert_mesh;
  extractor.init_subdiv = extract_vert_idx_init_subdiv;
  extractor.iter_loose_geom_subdiv = extract_vert_idx_loose_geom_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(int32_t *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.vert_idx);
  return extractor;
}

static void extract_fdot_idx_init(const MeshRenderData *mr,
                                  MeshBatchCache * /*cache*/,
                                  void *buf,
                                  void *tls_data)
{
  extract_select_idx_init_impl(mr, mr->poly_len, buf, tls_data);
}

static void extract_fdot_idx_iter_poly_bm(const MeshRenderData * /*mr*/,
                                          const BMFace * /*f*/,
                                          const int f_index,
                                          void *data)
{
  (*(int32_t **)data)[f_index] = f_index;
}

static void extract_fdot_idx_iter_poly_mesh(const MeshRenderData *mr,
                                            const int poly_index,
                                            void *data)
{
  if (mr->p_origindex != nullptr) {
    (*(int32_t **)data)[poly_index] = mr->p_origindex[poly_index];
  }
  else {
    (*(int32_t **)data)[poly_index] = poly_index;
  }
}

constexpr MeshExtract create_extractor_fdot_idx()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_fdot_idx_init;
  extractor.iter_poly_bm = extract_fdot_idx_iter_poly_bm;
  extractor.iter_poly_mesh = extract_fdot_idx_iter_poly_mesh;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(int32_t *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.fdot_idx);
  return extractor;
}

/** \} */

}  // namespace blender::draw

const MeshExtract extract_poly_idx = blender::draw::create_extractor_poly_idx();
const MeshExtract extract_edge_idx = blender::draw::create_extractor_edge_idx();
const MeshExtract extract_vert_idx = blender::draw::create_extractor_vert_idx();
const MeshExtract extract_fdot_idx = blender::draw::create_extractor_fdot_idx();
