/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"

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

/* TODO: Use #glVertexID to get loop index and use the data structure on the CPU to retrieve the
 * select element associated with this loop ID. This would remove the need for this separate
 * index VBO's. We could upload the p/e/orig_index_vert as a buffer texture and sample it inside
 * the shader to output original index. */

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
    (*(int32_t **)data)[corner] = (mr.orig_index_face) ? mr.orig_index_face[face_index] :
                                                         face_index;
  }
}

static void extract_edge_idx_iter_face_mesh(const MeshRenderData &mr,
                                            const int face_index,
                                            void *data)
{
  for (const int corner : mr.faces[face_index]) {
    const int edge = mr.corner_edges[corner];
    (*(int32_t **)data)[corner] = (mr.orig_index_edge) ? mr.orig_index_edge[edge] : edge;
  }
}

static void extract_vert_idx_iter_face_mesh(const MeshRenderData &mr,
                                            const int face_index,
                                            void *data)
{
  for (const int corner : mr.faces[face_index]) {
    const int vert = mr.corner_verts[corner];
    (*(int32_t **)data)[corner] = (mr.orig_index_vert) ? mr.orig_index_vert[vert] : vert;
  }
}

static void extract_edge_idx_iter_loose_edge_mesh(const MeshRenderData &mr,
                                                  const int2 /*edge*/,
                                                  const int loose_edge_i,
                                                  void *data)
{
  const int e_index = mr.loose_edges[loose_edge_i];
  const int e_orig = (mr.orig_index_edge) ? mr.orig_index_edge[e_index] : e_index;
  (*(int32_t **)data)[mr.corners_num + loose_edge_i * 2 + 0] = e_orig;
  (*(int32_t **)data)[mr.corners_num + loose_edge_i * 2 + 1] = e_orig;
}

static void extract_vert_idx_iter_loose_edge_mesh(const MeshRenderData &mr,
                                                  const int2 edge,
                                                  const int loose_edge_i,
                                                  void *data)
{
  int v1_orig = (mr.orig_index_vert) ? mr.orig_index_vert[edge[0]] : edge[0];
  int v2_orig = (mr.orig_index_vert) ? mr.orig_index_vert[edge[1]] : edge[1];
  (*(int32_t **)data)[mr.corners_num + loose_edge_i * 2 + 0] = v1_orig;
  (*(int32_t **)data)[mr.corners_num + loose_edge_i * 2 + 1] = v2_orig;
}

static void extract_vert_idx_iter_loose_vert_mesh(const MeshRenderData &mr,
                                                  const int loose_vert_i,
                                                  void *data)
{
  const int offset = mr.corners_num + (mr.loose_edges_num * 2);

  const int v_index = mr.loose_verts[loose_vert_i];
  const int v_orig = (mr.orig_index_vert) ? mr.orig_index_vert[v_index] : v_index;
  (*(int32_t **)data)[offset + loose_vert_i] = v_orig;
}

static void extract_vert_idx_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                         const MeshRenderData &mr,
                                         MeshBatchCache & /*cache*/,
                                         void *buf,
                                         void * /*data*/)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buf);
  /* Each element points to an element in the `ibo.points`. */
  draw_subdiv_init_origindex_buffer(vbo,
                                    (int32_t *)GPU_vertbuf_get_data(subdiv_cache.verts_orig_index),
                                    subdiv_cache.num_subdiv_loops,
                                    subdiv_full_vbo_size(mr, subdiv_cache));
  if (!mr.orig_index_vert) {
    return;
  }

  /* Remap the vertex indices to those pointed by the origin indices layer. At this point, the
   * VBO data is a copy of #verts_orig_index which contains the coarse vertices indices, so
   * the memory can both be accessed for lookup and immediately overwritten. */
  int32_t *vbo_data = static_cast<int32_t *>(GPU_vertbuf_get_data(vbo));
  for (int i = 0; i < subdiv_cache.num_subdiv_loops; i++) {
    if (vbo_data[i] == -1) {
      continue;
    }
    vbo_data[i] = mr.orig_index_vert[vbo_data[i]];
  }
}

static void extract_vert_idx_loose_geom_subdiv(const DRWSubdivCache &subdiv_cache,
                                               const MeshRenderData &mr,
                                               void *buffer,
                                               void * /*data*/)
{
  const Span<int> loose_verts = mr.loose_verts;
  const Span<int> loose_edges = mr.loose_edges;
  if (loose_edges.is_empty() && loose_verts.is_empty()) {
    return;
  }

  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buffer);
  MutableSpan<int32_t> vbo_data(static_cast<int32_t *>(GPU_vertbuf_get_data(vbo)),
                                subdiv_full_vbo_size(mr, subdiv_cache));

  const Span<int2> coarse_edges = mr.edges;
  const int verts_per_edge = subdiv_verts_per_coarse_edge(subdiv_cache);
  MutableSpan<int32_t> edge_data = vbo_data.slice(subdiv_cache.num_subdiv_loops,
                                                  loose_edges.size() * verts_per_edge);
  for (const int i : loose_edges.index_range()) {
    const int2 edge = coarse_edges[loose_edges[i]];
    MutableSpan data = edge_data.slice(i * verts_per_edge, verts_per_edge);
    data.first() = mr.orig_index_vert ? mr.orig_index_vert[edge[0]] : edge[0];
    data.last() = mr.orig_index_vert ? mr.orig_index_vert[edge[1]] : edge[1];
  }

  MutableSpan<int32_t> loose_vert_data = vbo_data.take_back(loose_verts.size());
  if (mr.orig_index_vert) {
    array_utils::gather(Span(mr.orig_index_vert, mr.verts_num), loose_verts, loose_vert_data);
  }
  else {
    array_utils::copy(loose_verts, loose_vert_data);
  }
}

static void extract_edge_idx_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                         const MeshRenderData &mr,
                                         MeshBatchCache & /*cache*/,
                                         void *buf,
                                         void * /*data*/)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buf);
  draw_subdiv_init_origindex_buffer(
      vbo,
      static_cast<int32_t *>(GPU_vertbuf_get_data(subdiv_cache.edges_orig_index)),
      subdiv_cache.num_subdiv_loops,
      subdiv_loose_edges_num(mr, subdiv_cache) * 2);
}

static void extract_edge_idx_loose_geom_subdiv(const DRWSubdivCache &subdiv_cache,
                                               const MeshRenderData &mr,
                                               void *buffer,
                                               void * /*data*/)
{
  const Span<int> loose_edges = mr.loose_edges;
  if (loose_edges.is_empty()) {
    return;
  }

  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buffer);
  MutableSpan<int32_t> vbo_data(static_cast<int32_t *>(GPU_vertbuf_get_data(vbo)),
                                subdiv_full_vbo_size(mr, subdiv_cache));

  const int verts_per_edge = subdiv_verts_per_coarse_edge(subdiv_cache);
  MutableSpan data = vbo_data.slice(subdiv_cache.num_subdiv_loops,
                                    loose_edges.size() * verts_per_edge);
  for (const int i : loose_edges.index_range()) {
    const int edge = loose_edges[i];
    const int index = mr.orig_index_edge ? mr.orig_index_edge[edge] : edge;
    data.slice(i * verts_per_edge, verts_per_edge).fill(index);
  }
}

static void extract_face_idx_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                         const MeshRenderData &mr,
                                         MeshBatchCache & /*cache*/,
                                         void *buf,
                                         void * /*data*/)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buf);
  draw_subdiv_init_origindex_buffer(
      vbo, subdiv_cache.subdiv_loop_face_index, subdiv_cache.num_subdiv_loops, 0);

  if (!mr.orig_index_face) {
    return;
  }

  /* Remap the face indices to those pointed by the origin indices layer. At this point, the
   * VBO data is a copy of #subdiv_loop_face_index which contains the coarse face indices, so
   * the memory can both be accessed for lookup and immediately overwritten. */
  int32_t *vbo_data = static_cast<int32_t *>(GPU_vertbuf_get_data(vbo));
  for (int i = 0; i < subdiv_cache.num_subdiv_loops; i++) {
    vbo_data[i] = mr.orig_index_face[vbo_data[i]];
  }
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

static void extract_fdot_idx_iter_face_mesh(const MeshRenderData &mr,
                                            const int face_index,
                                            void *data)
{
  if (mr.orig_index_face != nullptr) {
    (*(int32_t **)data)[face_index] = mr.orig_index_face[face_index];
  }
  else {
    (*(int32_t **)data)[face_index] = face_index;
  }
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
