/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "draw_subdivision.hh"
#include "extract_mesh.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Selection Index
 * \{ */

static void extract_select_idx_init_impl(const MeshRenderData & /*mr*/,
                                         const int len,
                                         void *buf,
                                         void *tls_data)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "index", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, len);
  *(int32_t **)tls_data = (int32_t *)GPU_vertbuf_get_data(vbo);
}

static void extract_select_idx_init(const MeshRenderData &mr,
                                    MeshBatchCache & /*cache*/,
                                    void *buf,
                                    void *tls_data)
{
  extract_select_idx_init_impl(mr, mr.corners_num + mr.loose_indices_num, buf, tls_data);
}

static void extract_face_idx_iter_face_bm(const MeshRenderData & /*mr*/,
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

static void extract_edge_idx_iter_face_bm(const MeshRenderData & /*mr*/,
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

static void extract_vert_idx_iter_face_bm(const MeshRenderData & /*mr*/,
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

static void extract_edge_idx_iter_loose_edge_bm(const MeshRenderData &mr,
                                                const BMEdge *eed,
                                                const int loose_edge_i,
                                                void *data)
{
  (*(int32_t **)data)[mr.corners_num + loose_edge_i * 2 + 0] = BM_elem_index_get(eed);
  (*(int32_t **)data)[mr.corners_num + loose_edge_i * 2 + 1] = BM_elem_index_get(eed);
}

static void extract_vert_idx_iter_loose_edge_bm(const MeshRenderData &mr,
                                                const BMEdge *eed,
                                                const int loose_edge_i,
                                                void *data)
{
  (*(int32_t **)data)[mr.corners_num + loose_edge_i * 2 + 0] = BM_elem_index_get(eed->v1);
  (*(int32_t **)data)[mr.corners_num + loose_edge_i * 2 + 1] = BM_elem_index_get(eed->v2);
}

static void extract_vert_idx_iter_loose_vert_bm(const MeshRenderData &mr,
                                                const BMVert *eve,
                                                const int loose_vert_i,
                                                void *data)
{
  const int offset = mr.corners_num + (mr.loose_edges_num * 2);

  (*(int32_t **)data)[offset + loose_vert_i] = BM_elem_index_get(eve);
}

static void extract_face_idx_iter_face_mesh(const MeshRenderData &mr,
                                            const int face_index,
                                            void *data)
{
  for (const int corner : mr.faces[face_index]) {
    (*(int32_t **)data)[corner] = face_index;
  }
}

static void extract_edge_idx_iter_face_mesh(const MeshRenderData &mr,
                                            const int face_index,
                                            void *data)
{
  for (const int corner : mr.faces[face_index]) {
    const int edge = mr.corner_edges[corner];
    (*(int32_t **)data)[corner] = edge;
  }
}

static void extract_vert_idx_iter_face_mesh(const MeshRenderData &mr,
                                            const int face_index,
                                            void *data)
{
  for (const int corner : mr.faces[face_index]) {
    const int vert = mr.corner_verts[corner];
    (*(int32_t **)data)[corner] = vert;
  }
}

static void extract_edge_idx_iter_loose_edge_mesh(const MeshRenderData &mr,
                                                  const int2 /*edge*/,
                                                  const int loose_edge_i,
                                                  void *data)
{
  const int e_index = mr.loose_edges[loose_edge_i];
  (*(int32_t **)data)[mr.corners_num + loose_edge_i * 2 + 0] = e_index;
  (*(int32_t **)data)[mr.corners_num + loose_edge_i * 2 + 1] = e_index;
}

static void extract_vert_idx_iter_loose_edge_mesh(const MeshRenderData &mr,
                                                  const int2 edge,
                                                  const int loose_edge_i,
                                                  void *data)
{
  int v1_orig = edge[0];
  int v2_orig = edge[1];
  (*(int32_t **)data)[mr.corners_num + loose_edge_i * 2 + 0] = v1_orig;
  (*(int32_t **)data)[mr.corners_num + loose_edge_i * 2 + 1] = v2_orig;
}

static void extract_vert_idx_iter_loose_vert_mesh(const MeshRenderData &mr,
                                                  const int loose_vert_i,
                                                  void *data)
{
  const int offset = mr.corners_num + (mr.loose_edges_num * 2);

  const int v_index = mr.loose_verts[loose_vert_i];
  (*(int32_t **)data)[offset + loose_vert_i] = v_index;
}

static void extract_vert_idx_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                         const MeshRenderData & /*mr*/,
                                         MeshBatchCache & /*cache*/,
                                         void *buf,
                                         void * /*data*/)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buf);
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache.loose_geom;
  /* Each element points to an element in the `ibo.points`. */
  draw_subdiv_init_origindex_buffer(vbo,
                                    (int32_t *)GPU_vertbuf_get_data(subdiv_cache.verts_orig_index),
                                    subdiv_cache.num_subdiv_loops,
                                    loose_geom.loop_len);
}

static void extract_vert_idx_loose_geom_subdiv(const DRWSubdivCache &subdiv_cache,
                                               const MeshRenderData & /*mr*/,
                                               void *buffer,
                                               void * /*data*/)
{
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache.loose_geom;
  if (loose_geom.loop_len == 0) {
    return;
  }

  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buffer);
  int32_t *vert_idx_data = (int32_t *)GPU_vertbuf_get_data(vbo);
  uint offset = subdiv_cache.num_subdiv_loops;

  Span<DRWSubdivLooseEdge> loose_edges = draw_subdiv_cache_get_loose_edges(subdiv_cache);

  for (const DRWSubdivLooseEdge &loose_edge : loose_edges) {
    const DRWSubdivLooseVertex &v1 = loose_geom.verts[loose_edge.loose_subdiv_v1_index];
    const DRWSubdivLooseVertex &v2 = loose_geom.verts[loose_edge.loose_subdiv_v2_index];

    if (v1.coarse_vertex_index != -1u) {
      vert_idx_data[offset] = v1.coarse_vertex_index;
    }

    if (v2.coarse_vertex_index != -1u) {
      vert_idx_data[offset + 1] = v2.coarse_vertex_index;
    }

    offset += 2;
  }

  Span<DRWSubdivLooseVertex> loose_verts = draw_subdiv_cache_get_loose_verts(subdiv_cache);

  for (const DRWSubdivLooseVertex &loose_vert : loose_verts) {
    vert_idx_data[offset] = loose_vert.coarse_vertex_index;
    offset += 1;
  }
}

static void extract_edge_idx_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                         const MeshRenderData & /*mr*/,
                                         MeshBatchCache & /*cache*/,
                                         void *buf,
                                         void * /*data*/)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buf);
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache.loose_geom;
  draw_subdiv_init_origindex_buffer(
      vbo,
      static_cast<int32_t *>(GPU_vertbuf_get_data(subdiv_cache.edges_orig_index)),
      subdiv_cache.num_subdiv_loops,
      loose_geom.edge_len * 2);
}

static void extract_edge_idx_loose_geom_subdiv(const DRWSubdivCache &subdiv_cache,
                                               const MeshRenderData & /*mr*/,
                                               void *buffer,
                                               void * /*data*/)
{
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache.loose_geom;
  if (loose_geom.edge_len == 0) {
    return;
  }

  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buffer);
  int32_t *vert_idx_data = (int32_t *)GPU_vertbuf_get_data(vbo);
  uint offset = subdiv_cache.num_subdiv_loops;

  Span<DRWSubdivLooseEdge> loose_edges = draw_subdiv_cache_get_loose_edges(subdiv_cache);
  for (const DRWSubdivLooseEdge &loose_edge : loose_edges) {
    const int coarse_edge_index = loose_edge.coarse_edge_index;
    vert_idx_data[offset] = coarse_edge_index;
    vert_idx_data[offset + 1] = coarse_edge_index;
    offset += 2;
  }
}

static void extract_face_idx_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                         const MeshRenderData & /*mr*/,
                                         MeshBatchCache & /*cache*/,
                                         void *buf,
                                         void * /*data*/)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buf);
  draw_subdiv_init_origindex_buffer(
      vbo, subdiv_cache.subdiv_loop_face_index, subdiv_cache.num_subdiv_loops, 0);
}

constexpr MeshExtract create_extractor_face_idx()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_select_idx_init;
  extractor.iter_face_bm = extract_face_idx_iter_face_bm;
  extractor.iter_face_mesh = extract_face_idx_iter_face_mesh;
  extractor.init_subdiv = extract_face_idx_init_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(int32_t *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.face_idx);
  return extractor;
}

constexpr MeshExtract create_extractor_edge_idx()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_select_idx_init;
  extractor.iter_face_bm = extract_edge_idx_iter_face_bm;
  extractor.iter_face_mesh = extract_edge_idx_iter_face_mesh;
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
  extractor.iter_face_bm = extract_vert_idx_iter_face_bm;
  extractor.iter_face_mesh = extract_vert_idx_iter_face_mesh;
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

static void extract_fdot_idx_init(const MeshRenderData &mr,
                                  MeshBatchCache & /*cache*/,
                                  void *buf,
                                  void *tls_data)
{
  extract_select_idx_init_impl(mr, mr.faces_num, buf, tls_data);
}

static void extract_fdot_idx_iter_face_bm(const MeshRenderData & /*mr*/,
                                          const BMFace * /*f*/,
                                          const int f_index,
                                          void *data)
{
  (*(int32_t **)data)[f_index] = f_index;
}

static void extract_fdot_idx_iter_face_mesh(const MeshRenderData & /*mr*/,
                                            const int face_index,
                                            void *data)
{
  (*(int32_t **)data)[face_index] = face_index;
}

constexpr MeshExtract create_extractor_fdot_idx()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_fdot_idx_init;
  extractor.iter_face_bm = extract_fdot_idx_iter_face_bm;
  extractor.iter_face_mesh = extract_fdot_idx_iter_face_mesh;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(int32_t *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.fdot_idx);
  return extractor;
}

/** \} */

const MeshExtract extract_face_idx = create_extractor_face_idx();
const MeshExtract extract_edge_idx = create_extractor_edge_idx();
const MeshExtract extract_vert_idx = create_extractor_vert_idx();
const MeshExtract extract_fdot_idx = create_extractor_fdot_idx();

}  // namespace blender::draw
